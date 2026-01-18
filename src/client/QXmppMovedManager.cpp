// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMovedManager.h"

#include "QXmppConstants_p.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMovedItem_p.h"
#include "QXmppPubSubManager.h"
#include "QXmppRosterManager.h"
#include "QXmppTask.h"
#include "QXmppUri.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Async.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QUrl>

using namespace QXmpp;
using namespace QXmpp::Private;

QXmppMovedItem::QXmppMovedItem(const QString &newJid)
    : m_newJid(newJid)
{
    setId(QXmppPubSubManager::standardItemIdToString(QXmppPubSubManager::Current));
}

// Returns true if the given DOM element is a valid \xep{0283, Moved} item.
bool QXmppMovedItem::isItem(const QDomElement &itemElement)
{
    return QXmppPubSubBaseItem::isItem(itemElement, [](const QDomElement &payload) {
        if (payload.tagName() != u"moved" || payload.namespaceURI() != ns_moved) {
            return false;
        }
        return payload.firstChildElement().tagName() == u"new-jid";
    });
}

void QXmppMovedItem::parsePayload(const QDomElement &payloadElement)
{
    m_newJid = payloadElement.firstChildElement(u"new-jid"_s).text();
}

void QXmppMovedItem::serializePayload(QXmlStreamWriter *writer) const
{
    if (m_newJid.isEmpty()) {
        return;
    }

    XmlWriter(writer).write(Element {
        { u"moved", ns_moved },
        TextElement { u"new-jid", m_newJid },
    });
}

// Ensures that both JIDs match.
//
// \param newBareJid JID of the contact that sent the subscription request
// \param pepBareJid JID of the new account as fetched from the old account statement
static QXmppMovedManager::Result movedJidsMatch(const QString &newBareJid, const QString &pepBareJid)
{
    if (newBareJid == pepBareJid) {
        return Success();
    }

    return QXmppError { u"The JID does not match the user's statement."_s, {} };
}

class QXmppMovedManagerPrivate
{
public:
    bool supportedByServer = false;
};

///
/// \class QXmppMovedManager
///
/// This class manages user account moving as specified in \xep{0283, Moved}
///
/// In order to use this manager, make sure to add all managers needed by this manager:
/// \code
/// client->addNewExtension<QXmppDiscoveryManager>();
/// client->addNewExtension<QXmppPubSubManager>();
/// \endcode
///
/// Afterwards, you need to add this manager to the client:
/// \code
/// auto *manager = client->addNewExtension<QXmppMovedManager>();
/// \endcode
///
/// If you want to publish a moved statement use the publishStatement call with the old account:
/// \code
/// manager->publishStatement("new@example.org");
/// \endcode
///
/// Once you published your statement, you then need to subscribe to your old contacts with the new account:
/// \code
/// manager->notifyContact("contact@xmpp.example", "old@example.org", "Hey, I moved my account, please accept me.");
/// \endcode
///
/// When a contact receive a subscription request from a moved user he needs to verify the authenticity of the request.
/// The QXmppRosterManager handle it on its own if the client has the QXmppMovedManager extension available.
/// The request will be ignored entirely if the old jid incoming subscription is not part of the roster with a 'from' or 'both' type.
/// In case of the authenticity can't be established the moved element is ignored entirely. Alternatively, if the client
/// does not has QXmppMovedManager support the request message will be changed to introduce a warning message before emitting
/// the subscription{Request}Received signal.
///
/// \ingroup Managers
///
/// \since QXmpp 1.9
///

///
/// \typedef QXmppMovedManager::Result
///
/// Contains QXmpp::Success or an error.
///

///
/// Constructs a \xep{0283, Moved} manager.
///
QXmppMovedManager::QXmppMovedManager()
    : d(new QXmppMovedManagerPrivate())
{
}

QXmppMovedManager::~QXmppMovedManager() = default;

QStringList QXmppMovedManager::discoveryFeatures() const
{
    return { ns_moved.toString() };
}

///
/// \property QXmppMovedManager::supportedByServer
///
/// \see QXmppMovedManager::supportedByServer()
///

///
/// Returns whether the own server supports \xep{0283, Moved} feature.
///
/// \return whether \xep{0283, Moved} feature is supported
///
bool QXmppMovedManager::supportedByServer() const
{
    return d->supportedByServer;
}

///
/// \fn QXmppMovedManager::supportedByServerChanged()
///
/// Emitted when the server enabled or disabled support for \xep{0283, Moved}.
///

///
/// Publish a moved statement.
///
/// \param newBareJid JID of the new account
///
/// \return the result of the action
///
QXmppTask<QXmppClient::EmptyResult> QXmppMovedManager::publishStatement(const QString &newBareJid)
{
    return chainSuccess(client()->findExtension<QXmppPubSubManager>()->publishOwnPepItem(ns_moved.toString(), QXmppMovedItem { newBareJid }), this);
}

///
/// Verify a user moved statement.
///
/// \param oldBareJid JID of the old account to check statement
/// \param newBareJid JID of the new account that send the subscription request
///
/// \return the result of the action
///
QXmppTask<QXmppMovedManager::Result> QXmppMovedManager::verifyStatement(const QString &oldBareJid, const QString &newBareJid)
{
    return chain<QXmppClient::EmptyResult>(
        client()->findExtension<QXmppPubSubManager>()->requestItem<QXmppMovedItem>(oldBareJid, ns_moved.toString(), u"current"_s),
        this,
        [=, this](QXmppPubSubManager::ItemResult<QXmppMovedItem> &&result) {
            return std::visit(
                overloaded {
                    [newBareJid](QXmppMovedItem item) -> Result {
                        return movedJidsMatch(newBareJid, item.newJid());
                    },
                    [newBareJid](QXmppError err) -> Result {
                        // As a special case, if the attempt to retrieve the moved statement results in an error with the <gone/> condition
                        // as defined in RFC 6120, and that <gone/> element contains a valid XMPP URI (e.g. xmpp:user@example.com), then the
                        // error response MUST be handled equivalent to a <moved/> statement containing a <new-jid/> element with the JID
                        // provided in the URI (e.g. user@example.com).
                        if (auto e = err.value<QXmppStanza::Error>()) {
                            const auto newJid = [&e]() -> QString {
                                if (e->condition() != QXmppStanza::Error::Gone) {
                                    return {};
                                }

                                const auto result = QXmppUri::fromString(e->redirectionUri());

                                if (hasValue(result)) {
                                    return getValue(result).jid();
                                }

                                return {};
                            }();

                            if (!newJid.isEmpty()) {
                                return movedJidsMatch(newBareJid, newJid);
                            }
                        }

                        return err;
                    },
                },
                std::move(result));
        });
}

///
/// Notifies a contact that the user has moved to another account.
///
/// \param contactBareJid JID of the contact to send the subscription request
/// \param oldBareJid JID of the old account we moved from
/// \param sensitive If true the notification is sent sensitively
/// \param reason The reason of the move
///
/// \return the result of the action
///
QXmppTask<QXmpp::SendResult> QXmppMovedManager::notifyContact(const QString &contactBareJid, const QString &oldBareJid, bool sensitive, const QString &reason)
{
    QXmppPresence packet;
    packet.setTo(QXmppUtils::jidToBareJid(contactBareJid));
    packet.setType(QXmppPresence::Subscribe);
    packet.setStatusText(reason);
    packet.setOldJid(oldBareJid);
    return sensitive ? client()->sendSensitive(std::move(packet)) : client()->send(std::move(packet));
}

/// \cond
void QXmppMovedManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, [this]() {
        if (this->client()->streamManagementState() == QXmppClient::NewStream) {
            resetCachedData();

            if (auto *disco = this->client()->findExtension<QXmppDiscoveryManager>()) {
                disco->info(this->client()->configuration().domain()).then(this, [this](auto &&result) {
                    if (hasValue(result)) {
                        setSupportedByServer(getValue(result).features().contains(ns_moved));
                    } else {
                        warning(u"MovedManager: Could not fetch server features: %1"_s.arg(getError(result).description));
                    }
                });
            } else {
                warning(u"MovedManager: Missing recommended QXmppDiscoveryManager"_s);
            }
        }
    });

    Q_ASSERT_X(client->findExtension<QXmppPubSubManager>(), "QXmppMovedManager", "QXmppPubSubManager is missing");
}

void QXmppMovedManager::onUnregistered(QXmppClient *client)
{
    resetCachedData();
    disconnect(client, &QXmppClient::connected, this, nullptr);
}
/// \endcond

///
/// Verifies an old JID in a received presence subscription request and removes it if it is invalid.
///
/// This requires the QXmppRosterManager to be registered with the client.
///
/// \param presence presence subscription request to be processed containing a non-empty old JID.
///
/// \returns the processed presence subscription request
///
QXmppTask<QXmppPresence> QXmppMovedManager::processSubscriptionRequest(QXmppPresence presence)
{
    Q_ASSERT(!presence.oldJid().isEmpty());

    auto *rosterManager = client()->findExtension<QXmppRosterManager>();
    Q_ASSERT(rosterManager);

    const auto entry = rosterManager->getRosterEntry(presence.oldJid());

    switch (entry.subscriptionType()) {
    case QXmppRosterIq::Item::From:
    case QXmppRosterIq::Item::Both:
        return chain<QXmppPresence>(verifyStatement(presence.oldJid(), QXmppUtils::jidToBareJid(presence.from())), this, [this, presence = presence](Result &&result) mutable {
            if (hasError(result)) {
                warning(presence.from() + u" sent a presence subscription request with the invalid old JID "_s + presence.oldJid());
                presence.setOldJid({});
            }

            return presence;
        });
    default:
        presence.setOldJid({});
        return makeReadyTask(std::move(presence));
    }
}

///
/// Sets whether the own server supports \xep{0283, Moved}.
///
/// \param supportedByServer whether \xep{0283, Moved} is supported by the server
///
void QXmppMovedManager::setSupportedByServer(bool supportedByServer)
{
    if (d->supportedByServer != supportedByServer) {
        d->supportedByServer = supportedByServer;
        Q_EMIT supportedByServerChanged();
    }
}

///
/// Resets the cached data.
///
void QXmppMovedManager::resetCachedData()
{
    setSupportedByServer(false);
}

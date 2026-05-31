// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2020 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppRosterManager.h"

#include "QXmppAccountMigrationManager.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppMovedManager.h"
#include "QXmppPresence.h"
#include "QXmppRosterIq.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace QXmpp::Private {

struct RosterData {
    using Items = QList<QXmppRosterIq::Item>;

    Items items;

    static std::variant<RosterData, QXmppError> fromDom(const QDomElement &el)
    {
        if (el.tagName() != u"roster" && el.namespaceURI() != ns_qxmpp_export) {
            return QXmppError { u"Invalid element."_s, {} };
        }

        return RosterData {
            parseChildElements<QList<QXmppRosterIq::Item>>(el),
        };
    }

    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"roster",
            [&] {
                for (const auto &item : items) {
                    item.toXml(w, true);
                }
            },
        });
    }
};

static void serializeRosterData(const RosterData &d, QXmlStreamWriter &writer)
{
    XmlWriter(&writer).write(d);
}

}  // namespace QXmpp::Private

/*!
    \fn QXmppRosterManager::subscriptionRequestReceived

    This signal is emitted when a JID asks to subscribe to the user's presence.

    The user can either accept the request by calling acceptSubscription() or refuse it
    by calling refuseSubscription().

    Since *QXmpp 1.10.2* only verified \xep{0283}{Moved} old JIDs are passed in \a presence. If
    verification fails or the given old JID is not valid, the attribute is cleared in the
    QXmppPresence. See \l rostermanager_moved "above" for more details.

    \note If QXmppConfiguration::autoAcceptSubscriptions() is set to true or the subscription
    request is automatically accepted by the QXmppMovedManager, this signal will not be emitted.

    \a subscriberBareJid is the bare JID that wants to subscribe to the user's presence.
    \a presence is the presence stanza, e.g. containing the message (presence.statusText()),
    \xep{0283}{Moved} old JID or other information.

    \since QXmpp 1.5
*/

class QXmppRosterManagerPrivate
{
public:
    QXmppRosterManagerPrivate();

    void clear();

    // map of bareJid and its rosterEntry
    QMap<QString, QXmppRosterIq::Item> entries;

    // map of resources of the jid and map of resources and presences
    QMap<QString, QMap<QString, QXmppPresence>> presences;

    // flag to store that the roster has been populated
    bool isRosterReceived;
};

QXmppRosterManagerPrivate::QXmppRosterManagerPrivate()
    : isRosterReceived(false)
{
}

void QXmppRosterManagerPrivate::clear()
{
    entries.clear();
    presences.clear();
    isRosterReceived = false;
}

/*!
    Constructs a roster manager.

    \a client.
*/
QXmppRosterManager::QXmppRosterManager(QXmppClient *client)
    : d(std::make_unique<QXmppRosterManagerPrivate>())
{
    QXmppExportData::registerExtension<RosterData, RosterData::fromDom, serializeRosterData>(u"roster", ns_qxmpp_export);

    connect(client, &QXmppClient::connected,
            this, &QXmppRosterManager::_q_connected);

    connect(client, &QXmppClient::disconnected,
            this, &QXmppRosterManager::_q_disconnected);

    connect(client, &QXmppClient::presenceReceived,
            this, &QXmppRosterManager::_q_presenceReceived);
}

QXmppRosterManager::~QXmppRosterManager() = default;

/*!
    Accepts an existing subscription request or pre-approves future subscription
    requests. Returns true on success.

    You can call this method in reply to the subscriptionRequest() signal or to
    create a pre-approved subscription.

    \note Pre-approving subscription requests is only allowed, if the server
    supports RFC6121 and advertises the 'urn:xmpp:features:pre-approval' stream
    feature.

    \sa QXmppStreamFeatures::preApprovedSubscriptionsSupported()

    \a reason and \a bareJid.
*/
bool QXmppRosterManager::acceptSubscription(const QString &bareJid, const QString &reason)
{
    QXmppPresence presence;
    presence.setTo(bareJid);
    presence.setType(QXmppPresence::Subscribed);
    presence.setStatusText(reason);
    return client()->sendLegacy(presence);
}

/*! Upon XMPP connection, request the roster. */
void QXmppRosterManager::_q_connected()
{
    // clear cache if stream has not been resumed
    if (client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->clear();
    }

    if (!d->isRosterReceived && client()->isAuthenticated()) {
        requestRoster().then(this, [this](auto &&result) {
            if (hasValue(result)) {
                // reset entries
                d->entries.clear();
                const auto items = getValue(result).items();
                for (const auto &item : items) {
                    d->entries.insert(item.bareJid(), item);
                }

                // notify
                d->isRosterReceived = true;
                Q_EMIT rosterReceived();
            }
        });
    }
}

void QXmppRosterManager::_q_disconnected()
{
    // clear cache if stream cannot be resumed
    if (client()->streamManagementState() == QXmppClient::NoStreamManagement) {
        d->clear();
    }
}

bool QXmppRosterManager::handleStanza(const QDomElement &element)
{
    if (!isIqElement<QXmppRosterIq>(element)) {
        return false;
    }

    // Security check: only server should send this iq
    // from() should be either empty or bareJid of the user
    const auto fromJid = element.attribute(u"from"_s);
    if (!fromJid.isEmpty() && QXmppUtils::jidToBareJid(fromJid) != client()->configuration().jidBare()) {
        return false;
    }

    QXmppRosterIq rosterIq;
    rosterIq.parse(element);

    switch (rosterIq.type()) {
    case QXmppIq::Set: {
        // send result iq
        QXmppIq returnIq(QXmppIq::Result);
        returnIq.setId(rosterIq.id());
        client()->send(std::move(returnIq));

        // store updated entries and notify changes
        const auto items = rosterIq.items();
        for (const auto &item : items) {
            const QString bareJid = item.bareJid();
            if (item.subscriptionType() == QXmppRosterIq::Item::Remove) {
                if (d->entries.remove(bareJid)) {
                    // notify the user that the item was removed
                    Q_EMIT itemRemoved(bareJid);
                }
            } else {
                const bool added = !d->entries.contains(bareJid);
                d->entries.insert(bareJid, item);
                if (added) {
                    // notify the user that the item was added
                    Q_EMIT itemAdded(bareJid);
                } else {
                    // notify the user that the item changed
                    Q_EMIT itemChanged(bareJid);
                }
            }
        }
        break;
    }
    default:
        break;
    }

    return true;
}

void QXmppRosterManager::_q_presenceReceived(const QXmppPresence &presence)
{
    const auto jid = presence.from();
    const auto bareJid = QXmppUtils::jidToBareJid(jid);
    const auto resource = QXmppUtils::jidToResource(jid);

    if (bareJid.isEmpty()) {
        return;
    }

    switch (presence.type()) {
    case QXmppPresence::Available:
        d->presences[bareJid][resource] = presence;
        Q_EMIT presenceChanged(bareJid, resource);
        break;
    case QXmppPresence::Unavailable:
        d->presences[bareJid].remove(resource);
        Q_EMIT presenceChanged(bareJid, resource);
        break;
    case QXmppPresence::Subscribe: {
        handleSubscriptionRequest(bareJid, presence);
        break;
    }
    default:
        break;
    }
}

void QXmppRosterManager::handleSubscriptionRequest(const QString &bareJid, const QXmppPresence &presence)
{
    auto notifyOnSubscriptionRequest = [this, bareJid](const QXmppPresence &presence) {
        Q_EMIT subscriptionReceived(bareJid);
        Q_EMIT subscriptionRequestReceived(bareJid, presence);
    };

    // Automatically accept all incoming subscription requests if enabled.
    if (client()->configuration().autoAcceptSubscriptions()) {
        acceptSubscription(bareJid);
        subscribe(bareJid);
        return;
    }

    // check for XEP-0283: Moved subscription requests and verify them
    if (auto *movedManager = client()->findExtension<QXmppMovedManager>(); movedManager && !presence.oldJid().isEmpty()) {
        movedManager->processSubscriptionRequest(presence).then(this, [this, notifyOnSubscriptionRequest](QXmppPresence &&presence) mutable {
            notifyOnSubscriptionRequest(presence);
        });
    } else {
        auto safePresence = presence;
        safePresence.setOldJid({});

        notifyOnSubscriptionRequest(safePresence);
    }
}

QXmppTask<QXmppRosterManager::RosterResult> QXmppRosterManager::requestRoster()
{
    QXmppRosterIq iq;
    iq.setType(QXmppIq::Get);
    iq.setFrom(client()->configuration().jid());

    // TODO: Request MIX annotations only when the server supports MIX-PAM.
    iq.setMixAnnotate(true);

    co_return parseIq<QXmppRosterIq>(co_await client()->sendIq(std::move(iq)));
}

/*!
    Adds a new item with bare JID \a bareJid to the roster without sending any subscription
    requests. \a name is an optional name for the item and \a groups specifies optional groups
    for the item.

    As a result, the server will initiate a roster push, causing the
    itemAdded() or itemChanged() signal to be emitted.

    \since QXmpp 1.5 Returns true on success.
*/
QXmppTask<QXmppRosterManager::Result> QXmppRosterManager::addRosterItem(const QString &bareJid, const QString &name, const QSet<QString> &groups)
{
    QXmppRosterIq::Item item;
    item.setBareJid(bareJid);
    item.setName(name);
    item.setGroups(groups);
    item.setSubscriptionType(QXmppRosterIq::Item::NotSet);

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendGenericIq(std::move(iq));
}

/*!
    Removes the roster item with bare JID \a bareJid and cancels subscriptions to and from the
    contact.

    As a result, the server will initiate a roster push, causing the
    itemRemoved() signal to be emitted.

    \since QXmpp 1.5 Returns true on success.
*/
QXmppTask<QXmppRosterManager::Result> QXmppRosterManager::removeRosterItem(const QString &bareJid)
{
    QXmppRosterIq::Item item;
    item.setBareJid(bareJid);
    item.setSubscriptionType(QXmppRosterIq::Item::Remove);

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendGenericIq(std::move(iq));
}

/*!
    Renames the roster item with bare JID \a bareJid to \a name.

    As a result, the server will initiate a roster push, causing the
    itemChanged() signal to be emitted.

    \since QXmpp 1.5
*/
QXmppTask<QXmppRosterManager::Result> QXmppRosterManager::renameRosterItem(const QString &bareJid, const QString &name)
{
    if (!d->entries.contains(bareJid)) {
        return makeReadyTask<Result>(
            QXmppError { u"The roster doesn't contain this user."_s, {} });
    }

    auto item = d->entries.value(bareJid);
    item.setName(name);

    // If there is a pending subscription, do not include the corresponding attribute in the stanza.
    if (!item.subscriptionStatus().isEmpty()) {
        item.setSubscriptionStatus({});
    }

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendGenericIq(std::move(iq));
}

/*!
    Requests a subscription to the given contact.

    As a result, the server will initiate a roster push, causing the
    itemAdded() or itemChanged() signal to be emitted.

    \since QXmpp 1.5

    \a reason and \a bareJid.
*/
QXmppTask<QXmpp::SendResult> QXmppRosterManager::subscribeTo(const QString &bareJid, const QString &reason)
{
    QXmppPresence packet;
    packet.setTo(QXmppUtils::jidToBareJid(bareJid));
    packet.setType(QXmppPresence::Subscribe);
    packet.setStatusText(reason);
    return client()->sendSensitive(std::move(packet));
}

/*!
    Removes a subscription to the given contact.

    As a result, the server will initiate a roster push, causing the
    itemChanged() signal to be emitted.

    \since QXmpp 1.5 Returns true on success.

    \a reason and \a bareJid.
*/
QXmppTask<QXmpp::SendResult> QXmppRosterManager::unsubscribeFrom(const QString &bareJid, const QString &reason)
{
    QXmppPresence packet;
    packet.setTo(QXmppUtils::jidToBareJid(bareJid));
    packet.setType(QXmppPresence::Unsubscribe);
    packet.setStatusText(reason);
    return client()->sendSensitive(std::move(packet));
}

/*!
    Refuses a subscription request. Returns true on success.

    You can call this method in reply to the subscriptionRequest() signal.

    \a reason and \a bareJid.
*/
bool QXmppRosterManager::refuseSubscription(const QString &bareJid, const QString &reason)
{
    QXmppPresence presence;
    presence.setTo(bareJid);
    presence.setType(QXmppPresence::Unsubscribed);
    presence.setStatusText(reason);
    return client()->sendLegacy(presence);
}

/*!
    Adds a new item with bare JID \a bareJid to the roster without sending any subscription
    requests. \a name is an optional name for the item and \a groups specifies optional groups
    for the item.

    As a result, the server will initiate a roster push, causing the
    itemAdded() or itemChanged() signal to be emitted. Returns true on success.
*/
bool QXmppRosterManager::addItem(const QString &bareJid, const QString &name, const QSet<QString> &groups)
{
    QXmppRosterIq::Item item;
    item.setBareJid(bareJid);
    item.setName(name);
    item.setGroups(groups);
    item.setSubscriptionType(QXmppRosterIq::Item::NotSet);

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendLegacy(iq);
}

/*!
    Removes the roster item with bare JID \a bareJid and cancels subscriptions to and from the
    contact.

    As a result, the server will initiate a roster push, causing the
    itemRemoved() signal to be emitted. Returns true on success.
*/
bool QXmppRosterManager::removeItem(const QString &bareJid)
{
    QXmppRosterIq::Item item;
    item.setBareJid(bareJid);
    item.setSubscriptionType(QXmppRosterIq::Item::Remove);

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendLegacy(iq);
}

/*!
    Renames the roster item with bare JID \a bareJid to \a name; returns
    true on success.

    As a result, the server will initiate a roster push, causing the
    itemChanged() signal to be emitted.
*/
bool QXmppRosterManager::renameItem(const QString &bareJid, const QString &name)
{
    if (!d->entries.contains(bareJid)) {
        return false;
    }

    auto item = d->entries.value(bareJid);
    item.setName(name);

    // If there is a pending subscription, do not include the corresponding attribute in the stanza.
    if (!item.subscriptionStatus().isEmpty()) {
        item.setSubscriptionStatus({});
    }

    QXmppRosterIq iq;
    iq.setType(QXmppIq::Set);
    iq.addItem(item);
    return client()->sendLegacy(iq);
}

/*!
    Requests a subscription to the given contact; returns true on success.

    As a result, the server will initiate a roster push, causing the
    itemAdded() or itemChanged() signal to be emitted.

    \a reason and \a bareJid.
*/
bool QXmppRosterManager::subscribe(const QString &bareJid, const QString &reason)
{
    QXmppPresence packet;
    packet.setTo(QXmppUtils::jidToBareJid(bareJid));
    packet.setType(QXmppPresence::Subscribe);
    packet.setStatusText(reason);
    return client()->sendLegacy(packet);
}

/*!
    Removes a subscription to the given contact.

    As a result, the server will initiate a roster push, causing the
    itemChanged() signal to be emitted. Returns true on success.

    \a reason and \a bareJid.
*/
bool QXmppRosterManager::unsubscribe(const QString &bareJid, const QString &reason)
{
    QXmppPresence packet;
    packet.setTo(QXmppUtils::jidToBareJid(bareJid));
    packet.setType(QXmppPresence::Unsubscribe);
    packet.setStatusText(reason);
    return client()->sendLegacy(packet);
}

void QXmppRosterManager::onRegistered(QXmppClient *client)
{
    // data import/export
    if (auto manager = client->findExtension<QXmppAccountMigrationManager>()) {
        using ImportResult = std::variant<Success, QXmppError>;
        auto importData = [this, client, manager](const RosterData &data) -> QXmppTask<ImportResult> {
            auto tasks = transform<std::vector<QXmppTask<QXmppClient::EmptyResult>>>(data.items, [client](const auto &item) {
                Q_ASSERT(!item.isMixChannel());
                QXmppRosterIq iq;
                iq.addItem(item);
                iq.setType(QXmppIq::Set);
                return client->sendGenericIq(std::move(iq));
            });

            for (auto &task : tasks) {
                auto result = co_await task;

                if (hasError(result)) {
                    co_return getError(std::move(result));
                }
            }
            co_return Success();
        };
        auto exportData = [this]() -> QXmppTask<std::variant<RosterData, QXmppError>> {
            co_return mapSuccess(co_await requestRoster(), [](const QXmppRosterIq &iq) -> RosterData {
                return {
                    transformFilter<RosterData::Items>(iq.items(), [](const auto &item) -> std::optional<QXmppRosterIq::Item> {
                        if (item.isMixChannel()) {
                            return {};
                        }

                        auto fixed = item;

                        // We don't want this to be sent while importing.
                        // See https://datatracker.ietf.org/doc/html/rfc6121#section-2.1.2.2
                        fixed.setSubscriptionStatus({});

                        return fixed;
                    })
                };
            });
        };

        manager->registerExportData<RosterData>(importData, exportData);
    }
}

void QXmppRosterManager::onUnregistered(QXmppClient *client)
{
    if (auto manager = client->findExtension<QXmppAccountMigrationManager>()) {
        manager->unregisterExportData<RosterData>();
    }
}

/*!
    Returns a QStringList list of all the bareJids present in the roster.
*/
QStringList QXmppRosterManager::getRosterBareJids() const
{
    return d->entries.keys();
}

/*!
    Returns the roster entry of the given \a bareJid. If \a bareJid is not in the
    database and empty QXmppRosterIq::Item will be returned.
*/
QXmppRosterIq::Item QXmppRosterManager::getRosterEntry(
    const QString &bareJid) const
{
    // will return blank entry if bareJid doesn't exist
    if (d->entries.contains(bareJid)) {
        return d->entries.value(bareJid);
    }
    return {};
}

/*!
    Returns the list of associated resources for the given \a bareJid as a QStringList.
*/
QStringList QXmppRosterManager::getResources(const QString &bareJid) const
{
    if (d->presences.contains(bareJid)) {
        return d->presences[bareJid].keys();
    }
    return {};
}

/*!
    Returns a QMap<QString, QXmppPresence> of resource and its respective presence for all the
    resources of the given \a bareJid. A bareJid can have multiple resources and each resource
    will have a presence associated with it.
*/
QMap<QString, QXmppPresence> QXmppRosterManager::getAllPresencesForBareJid(
    const QString &bareJid) const
{
    if (d->presences.contains(bareJid)) {
        return d->presences.value(bareJid);
    }
    return {};
}

/*!
    Returns the QXmppPresence of the given \a resource of the given \a bareJid.
*/
QXmppPresence QXmppRosterManager::getPresence(const QString &bareJid,
                                              const QString &resource) const
{
    if (d->presences.contains(bareJid) && d->presences[bareJid].contains(resource)) {
        return d->presences[bareJid][resource];
    }

    QXmppPresence presence;
    presence.setType(QXmppPresence::Unavailable);
    return presence;
}

/*!
    Returns true if the roster has been received, otherwise false.

    On disconnecting this is reset to false if no stream management is used by
    the client and so the stream cannot be resumed later.
*/
bool QXmppRosterManager::isRosterReceived() const
{
    return d->isRosterReceived;
}

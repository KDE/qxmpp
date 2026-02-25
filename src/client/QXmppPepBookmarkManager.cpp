// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPepBookmarkManager.h"

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppPepBookmarkManager_p.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils_p.h"

#include "Global.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

using namespace QXmpp;
using namespace QXmpp::Private;

//
// Serialization
//

namespace QXmpp::Private {

struct Bookmarks2Conference {
    bool autojoin = false;
    QString name;
    QString nick;
    QString password;
    // extensions (none supported)
    // TODO: impl generic extensions for passthrough for editing
};

struct Bookmarks2ConferenceItem : public QXmppPubSubBaseItem {
    Bookmarks2Conference payload;

    void parsePayload(const QDomElement &el) override
    {
        payload.autojoin = parseBoolean(el.attribute(u"autojoin"_s)).value_or(false);
        payload.name = el.attribute(u"name"_s);
        payload.nick = firstChildElement(el, u"nick", ns_bookmarks2).text();
        payload.password = firstChildElement(el, u"password", ns_bookmarks2).text();
    }
    void serializePayload(QXmlStreamWriter *writer) const override
    {
        XmlWriter(writer).write(Element {
            { u"conference", ns_bookmarks2 },
            OptionalAttribute { u"autojoin", DefaultedBool { payload.autojoin, false } },
            OptionalAttribute { u"name", payload.name },
            OptionalTextElement { u"nick", payload.nick },
            OptionalTextElement { u"password", payload.password },
        });
    }
};

}  // namespace QXmpp::Private

class QXmppMucBookmarkPrivate : public QSharedData
{
public:
    QString jid;
    Bookmarks2Conference payload;
};

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMucBookmark)

/// Empty constructor.
QXmppMucBookmark::QXmppMucBookmark()
    : d(new QXmppMucBookmarkPrivate)
{
}

/// Constructs with values.
QXmppMucBookmark::QXmppMucBookmark(const QString &jid, const QString &name, bool autojoin, const QString &nick, const QString &password)
    : d(new QXmppMucBookmarkPrivate { {}, jid, Bookmarks2Conference { autojoin, name, nick, password } })
{
}

/// Constructs with values.
QXmppMucBookmark::QXmppMucBookmark(const QString &jid, QXmpp::Private::Bookmarks2Conference conference)
    : d(new QXmppMucBookmarkPrivate { {}, jid, std::move(conference) })
{
}

/// Returns the (bare) JID of the MUC.
const QString &QXmppMucBookmark::jid() const { return d->jid; }
/// Sets the (bare) JID of the MUC.
void QXmppMucBookmark::setJid(const QString &jid) { d->jid = jid; }

/// Returns the user-defined display name of the MUC.
const QString &QXmppMucBookmark::name() const { return d->payload.name; }
/// Sets the user-defined display name of the MUC.
void QXmppMucBookmark::setName(const QString &name) { d->payload.name = name; }

/// Returns the user's preferred nick for this MUC.
const QString &QXmppMucBookmark::nick() const { return d->payload.nick; }
/// Sets the user's preferred nick for this MUC.
void QXmppMucBookmark::setNick(const QString &nick) { d->payload.nick = nick; }

/// Returns the required password for the MUC.
const QString &QXmppMucBookmark::password() const { return d->payload.password; }
/// Sets the required password for the MUC.
void QXmppMucBookmark::setPassword(const QString &password) { d->payload.password = password; }

/// Returns whether to automatically join this MUC on connection.
bool QXmppMucBookmark::autojoin() const { return d->payload.autojoin; }
/// Sets whether to automatically join this MUC on connection.
void QXmppMucBookmark::setAutojoin(bool autojoin) { d->payload.autojoin = autojoin; }

//
// Manager
//

using EmptyResult = std::variant<Success, QXmppError>;

///
/// \fn QXmppPepBookmarkManager::bookmarksReset()
///
/// Emitted when the total set of bookmarks is reset, e.g. when receiving the initial bookmarks
/// items query.
///

///
/// \fn QXmppPepBookmarkManager::bookmarksAdded()
///
/// Emitted when bookmarks have been added. This is triggered by PubSub event notifications.
///

///
/// \fn QXmppPepBookmarkManager::bookmarksChanged()
///
/// Emitted when bookmarks have been changed.
///

///
/// \fn QXmppPepBookmarkManager::bookmarksRemoved()
///
/// Emitted when bookmarks are retracted.
///

/// Default constructor.
QXmppPepBookmarkManager::QXmppPepBookmarkManager()
    : d(std::make_unique<QXmppPepBookmarkManagerPrivate>(this))
{
}

QXmppPepBookmarkManager::~QXmppPepBookmarkManager() = default;

/// Supported service discovery features.
QStringList QXmppPepBookmarkManager::discoveryFeatures() const
{
    return { ns_bookmarks2 + u"+notify" };
}

///
/// Returns the currently cached list of bookmarks, or \c std::nullopt if they haven't been
/// fetched yet. Connect to bookmarksReset() to be notified once the initial fetch completes.
///
const std::optional<QList<QXmppMucBookmark>> &QXmppPepBookmarkManager::bookmarks() const { return d->bookmarks; }

///
/// Publishes or updates a bookmark via \xep{0402, PEP Native Bookmarks}.
///
/// If a bookmark for the same JID already exists it is replaced. The change is
/// propagated to all connected clients via PubSub event notification, which will
/// trigger bookmarksChanged() on this manager.
///
/// \param bookmark Bookmark to publish. The JID must not be empty.
///
QXmppTask<Result<>> QXmppPepBookmarkManager::setBookmark(QXmppMucBookmark &&bookmark)
{
    return d->setBookmark(std::move(bookmark));
}

///
/// Retracts the bookmark for the room at \a jid via \xep{0402, PEP Native Bookmarks}.
///
/// Does nothing if no bookmark for \a jid exists. The retraction is propagated to all connected
/// clients, which will trigger bookmarksRemoved() on this manager.
///
QXmppTask<Result<>> QXmppPepBookmarkManager::removeBookmark(const QString &jid)
{
    return d->removeBookmark(jid);
}

/// Handles incoming PubSub events.
bool QXmppPepBookmarkManager::handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName)
{
    if (d->bookmarks && pubSubService == client()->configuration().jidBare() && nodeName == ns_bookmarks2) {
        auto &bookmarks = *d->bookmarks;

        QXmppPubSubEvent<Bookmarks2ConferenceItem> event;
        event.parse(element);

        using enum QXmppPubSubEventBase::EventType;
        switch (event.eventType()) {
        case Purge:
        case Delete:
            bookmarks = QList<QXmppMucBookmark>();
            Q_EMIT bookmarksReset();
            break;
        case Items: {
            const auto items = event.items();

            // No reserve because in practise in 95% of the use-case only one change/addition is
            // done and only one allocation is needed then.
            QList<BookmarkChange> changes;
            QList<QXmppMucBookmark> addedBookmarks;

            for (const auto &item : items) {
                auto payload = item.payload;
                auto newBookmark = QXmppMucBookmark { item.id(), std::move(payload) };

                if (auto it = std::ranges::find(bookmarks, item.id(), &QXmppMucBookmark::jid); it != bookmarks.end()) {
                    changes.push_back(BookmarkChange { std::move(*it), newBookmark });
                    *it = std::move(newBookmark);
                } else {
                    addedBookmarks.push_back(newBookmark);
                    bookmarks.push_back(std::move(newBookmark));
                }
            }

            if (!changes.isEmpty()) {
                Q_EMIT bookmarksChanged(changes);
            }
            if (!addedBookmarks.isEmpty()) {
                Q_EMIT bookmarksAdded(addedBookmarks);
            }
            break;
        }
        case Retract: {
            const auto jids = event.retractIds();

            QList<QString> removedJids;
            removedJids.reserve(jids.size());

            for (const auto &jid : jids) {
                if (auto it = std::ranges::find(bookmarks, jid, &QXmppMucBookmark::jid);
                    it != bookmarks.end()) {
                    bookmarks.erase(it);
                    removedJids.push_back(jid);
                }
            }

            Q_EMIT bookmarksRemoved(removedJids);
            break;
        }
        case Configuration:
        case Subscription:
            break;
        }
        return true;
    }
    return false;
}

void QXmppPepBookmarkManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, &QXmppPepBookmarkManager::onConnected);
}

void QXmppPepBookmarkManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
}

void QXmppPepBookmarkManager::onConnected()
{
    if (client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->pubsub()->requestItems<Bookmarks2ConferenceItem>({}, ns_bookmarks2.toString()).then(this, [this](auto &&result) {
            if (hasError(result)) {
                warning(u"Could not fetch bookmarks: " + getError(result).description);
                d->resetBookmarks();
                return;
            }

            auto items = getValue(std::move(result));
            d->setBookmarks(std::move(items.items));
        });
    }
}

//
// Manager private
//

auto QXmppPepBookmarkManagerPrivate::setBookmark(QXmppMucBookmark &&bookmark) -> QXmppTask<EmptyResult>
{
    auto opts = QXmppPubSubPublishOptions();
    opts.setPersistItems(true);
    opts.setMaxItems(QXmppPubSubNodeConfig::Max());
    opts.setSendLastItem(QXmppPubSubNodeConfig::SendLastItemType::Never);
    opts.setAccessModel(QXmppPubSubNodeConfig::AccessModel::Allowlist);
    auto item = Bookmarks2ConferenceItem();
    item.setId(bookmark.jid());
    item.payload = bookmark.d->payload;

    return chain<EmptyResult>(
        pubsub()->publishItem({}, ns_bookmarks2.toString(), item, opts),
        q,
        [this, bookmark](auto &&result) mutable -> EmptyResult {
            if (hasError(result)) {
                return getError(std::move(result));
            }
            if (bookmarks) {
                if (auto it = std::ranges::find(*bookmarks, bookmark.jid(), &QXmppMucBookmark::jid);
                    it != bookmarks->end()) {
                    *it = std::move(bookmark);
                } else {
                    bookmarks->append(std::move(bookmark));
                }
            }
            return Success();
        });
}

QXmppTask<EmptyResult> QXmppPepBookmarkManagerPrivate::removeBookmark(const QString &jid)
{
    return chain<EmptyResult>(pubsub()->retractOwnPepItem(ns_bookmarks2.toString(), jid, true), q, [this, jid](auto &&result) -> EmptyResult {
        if (hasError(result)) {
            return getError(std::move(result));
        }
        if (bookmarks) {
            if (auto it = std::ranges::find(*bookmarks, jid, &QXmppMucBookmark::jid);
                it != bookmarks->end()) {
                bookmarks->erase(it);
            }
        }
        return Success();
    });
}

QXmppPubSubManager *QXmppPepBookmarkManagerPrivate::pubsub()
{
    if (!q->client()) {
        qFatal("PepBookmarkManager: Not registered.");
    }
    if (auto manager = q->client()->findExtension<QXmppPubSubManager>()) {
        return manager;
    }
    qFatal("PepBookmarkManager: Missing required PubSubManager.");
    return nullptr;
}

void QXmppPepBookmarkManagerPrivate::setBookmarks(QVector<Bookmarks2ConferenceItem> &&items)
{
    bookmarks = transform<QList<QXmppMucBookmark>>(items, [](auto &&item) {
        return QXmppMucBookmark { item.id(), std::move(item.payload) };
    });
    Q_EMIT q->bookmarksReset();
}

void QXmppPepBookmarkManagerPrivate::resetBookmarks()
{
    if (bookmarks) {
        bookmarks.reset();
        Q_EMIT q->bookmarksReset();
    }
}

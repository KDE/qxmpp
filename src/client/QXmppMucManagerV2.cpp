// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucManagerV2.h"

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMucForms.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"

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

///
/// \class QXmppMucBookmark
///
/// Bookmark data for a MUC room, retrieved via \xep{0402, PEP Native Bookmarks}.
///
/// \since QXmpp 1.13
///

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

struct QXmppMucManagerV2Private {
    QXmppMucManagerV2 *q = nullptr;
    std::optional<QList<QXmppMucBookmark>> bookmarks;

    QXmppPubSubManager *pubsub();
    QXmppDiscoveryManager *disco();
    void setBookmarks(QVector<Bookmarks2ConferenceItem> &&items);
    void resetBookmarks();
    QXmppTask<EmptyResult> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<EmptyResult> removeBookmark(const QString &jid);
};

///
/// \class QXmppMucManagerV2
///
/// \brief \xep{0045, Multi-User Chat} Manager with support for \xep{0402, PEP Native Bookmarks}.
///
/// The manager automatically fetches bookmarks on session establishment and afterwards emits
/// bookmarksReset(), bookmarksAdded(), bookmarksRemoved() and bookmarksChanged().
///
/// The QXmppPubSubManager must be registered with the client.
///
/// \note This manager is work-in-progress and joining MUCs is not yet implemented.
///
/// \ingroup Managers
/// \since QXmpp 1.13
///

///
/// \fn QXmppMucManagerV2::bookmarksReset()
///
/// Emitted when the total set of bookmarks is reset, e.g. when receiving the initial bookmarks
/// items query.
///

///
/// \fn QXmppMucManagerV2::bookmarksAdded()
///
/// Emitted when bookmarks have been added. This is triggered by PubSub event notifications.
///

///
/// \fn QXmppMucManagerV2::bookmarksChanged()
///
/// Emitted when bookmarks have been changed.
///

///
/// \fn QXmppMucManagerV2::bookmarksRemoved()
///
/// Emitted when bookmarks are retracted.
///

/// Default constructor.
QXmppMucManagerV2::QXmppMucManagerV2()
    : d(std::make_unique<QXmppMucManagerV2Private>(this))
{
}

QXmppMucManagerV2::~QXmppMucManagerV2() = default;

/// Supported service discovery features.
QStringList QXmppMucManagerV2::discoveryFeatures() const { return { ns_bookmarks2 + u"+notify" }; }

/// Retrieved bookmarks.
const std::optional<QList<QXmppMucBookmark>> &QXmppMucManagerV2::bookmarks() const { return d->bookmarks; }

///
/// Sets the avatar of a MUC room.
///
/// Requires the MUC service to support "vcard-temp" and to be "an owner or some other priviledged
/// entity" of the MUC.
///
/// \param jid JID of the MUC room
/// \param avatar Avatar to be set
///
QXmppTask<QXmpp::Result<>> QXmppMucManagerV2::setRoomAvatar(const QString &jid, const Avatar &avatar)
{
    QXmppVCardIq vCardIq;
    vCardIq.setTo(jid);
    vCardIq.setFrom({});
    vCardIq.setType(QXmppIq::Set);
    vCardIq.setPhotoType(avatar.contentType);
    vCardIq.setPhoto(avatar.data);
    return client()->sendGenericIq(std::move(vCardIq));
}

///
/// Removes the avatar of a MUC room.
///
/// Requires the MUC service to support "vcard-temp" and to be "an owner or some other priviledged
/// entity" of the MUC.
///
/// \param jid JID of the MUC room
///
QXmppTask<QXmpp::Result<>> QXmppMucManagerV2::removeRoomAvatar(const QString &jid)
{
    QXmppVCardIq vCardIq;
    vCardIq.setTo(jid);
    vCardIq.setFrom({});
    vCardIq.setType(QXmppIq::Set);
    return client()->sendGenericIq(std::move(vCardIq));
}

///
/// Fetches the Avatar of a MUC room
///
/// First fetches the avatar hashes via the `muc#roominfo` form from service discovery information
/// and then fetches the avatar itself via vcard-temp.
///
/// \note This currently does not do any caching and does not listen for avatar changes.
/// \param jid JID of the MUC room
/// \return nullopt if VCards are not supported in this MUC or no avatar has been published, otherwise the published avatar
///
QXmppTask<Result<std::optional<QXmppMucManagerV2::Avatar>>> QXmppMucManagerV2::fetchRoomAvatar(const QString &jid)
{
    QXmppPromise<Result<std::optional<Avatar>>> p;
    auto t = p.task();

    d->disco()->info(jid).then(this, [this, jid, p = std::move(p)](auto result) mutable {
        if (!hasValue(result)) {
            p.finish(getError(std::move(result)));
            return;
        }

        auto &info = getValue(result);
        if (!contains(info.features(), ns_vcard)) {
            p.finish(std::nullopt);
            return;
        }

        auto roomInfo = info.template dataForm<QXmppMucRoomInfo>();
        if (!roomInfo.has_value()) {
            p.finish(std::nullopt);
            return;
        }
        auto hashes = roomInfo->avatarHashes();
        if (hashes.isEmpty()) {
            p.finish(std::nullopt);
            return;
        }

        client()->sendIq(QXmppVCardIq(jid)).then(this, [this, hashes, p = std::move(p)](auto &&result) mutable {
            auto iqResponse = parseIq<QXmppVCardIq, Result<QXmppVCardIq>>(std::move(result));
            if (hasError(iqResponse)) {
                p.finish(getError(std::move(iqResponse)));
                return;
            }

            const auto &vcard = getValue(iqResponse);

            auto hexHash = QString::fromUtf8(QCryptographicHash::hash(vcard.photo(), QCryptographicHash::Sha1).toHex());
            if (!contains(hashes, hexHash)) {
                p.finish(QXmppError { u"Avatar hash mismatch"_s });
            } else {
                p.finish(Avatar { vcard.photoType(), vcard.photo() });
            }
        });
    });

    return t;
}

/// Handles incoming PubSub events.
bool QXmppMucManagerV2::handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName)
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

/// Adds or updates the bookmark for a MUC.
QXmppTask<Result<>> QXmppMucManagerV2::setBookmark(QXmppMucBookmark &&bookmark)
{
    return d->setBookmark(std::move(bookmark));
}

/// Removes a bookmark.
QXmppTask<Result<>> QXmppMucManagerV2::removeBookmark(const QString &jid)
{
    return d->removeBookmark(jid);
}

void QXmppMucManagerV2::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, &QXmppMucManagerV2::onConnected);
}

void QXmppMucManagerV2::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
}

void QXmppMucManagerV2::onConnected()
{
    using PubSub = QXmppPubSubManager;

    if (client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->pubsub()->requestItems<Bookmarks2ConferenceItem>({}, ns_bookmarks2.toString()).then(this, [this](auto &&result) {
            if (hasError(result)) {
                warning(u"Could not fetch MUC Bookmarks: " + getError(result).description);
                d->resetBookmarks();
                return;
            }

            auto items = getValue(std::move(result));
            d->setBookmarks(std::move(items.items));
        });
    }
}

auto QXmppMucManagerV2Private::setBookmark(QXmppMucBookmark &&bookmark) -> QXmppTask<EmptyResult>
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
                bookmarks->append(std::move(bookmark));
            }
            return Success();
        });
}

QXmppTask<EmptyResult> QXmppMucManagerV2Private::removeBookmark(const QString &jid)
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

QXmppPubSubManager *QXmppMucManagerV2Private::pubsub()
{
    if (!q->client()) {
        qFatal("MucManagerV2: Not registered.");
    }
    if (auto manager = q->client()->findExtension<QXmppPubSubManager>()) {
        return manager;
    }
    qFatal("MucManagerV2: Missing required PubSubManager.");
    return nullptr;
}

QXmppDiscoveryManager *QXmppMucManagerV2Private::disco()
{
    if (!q->client()) {
        qFatal("MucManagerV2: Not registered.");
    }
    if (auto manager = q->client()->findExtension<QXmppDiscoveryManager>()) {
        return manager;
    }
    qFatal("MucManagerV2: Missing required DiscoveryManager.");
    return nullptr;
}

void QXmppMucManagerV2Private::setBookmarks(QVector<Bookmarks2ConferenceItem> &&items)
{
    bookmarks = transform<QList<QXmppMucBookmark>>(items, [](auto &&item) {
        return QXmppMucBookmark { item.id(), std::move(item.payload) };
    });
    Q_EMIT q->bookmarksReset();
}

void QXmppMucManagerV2Private::resetBookmarks()
{
    if (bookmarks) {
        bookmarks.reset();
        Q_EMIT q->bookmarksReset();
    }
}

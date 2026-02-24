// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucManagerV2.h"

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMucForms.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"
#include <QXmppUtils.h>

#include "Global.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <unordered_map>

#include <QProperty>
#include <QTimer>

using namespace QXmpp;
using namespace QXmpp::Private;

static std::optional<QXmpp::Muc::Affiliation> affiliationFromLegacy(QXmppMucItem::Affiliation affiliation)
{
    switch (affiliation) {
    case QXmppMucItem::UnspecifiedAffiliation:
        return {};
    case QXmppMucItem::OutcastAffiliation:
        return QXmpp::Muc::Affiliation::Outcast;
    case QXmppMucItem::NoAffiliation:
        return QXmpp::Muc::Affiliation::None;
    case QXmppMucItem::MemberAffiliation:
        return QXmpp::Muc::Affiliation::Member;
    case QXmppMucItem::AdminAffiliation:
        return QXmpp::Muc::Affiliation::Admin;
    case QXmppMucItem::OwnerAffiliation:
        return QXmpp::Muc::Affiliation::Owner;
    }
    return {};
}

static std::optional<Muc::Role> roleFromLegacy(QXmppMucItem::Role role)
{
    switch (role) {
    case QXmppMucItem::UnspecifiedRole:
        return {};
    case QXmppMucItem::NoRole:
        return Muc::Role::None;
    case QXmppMucItem::VisitorRole:
        return Muc::Role::Visitor;
    case QXmppMucItem::ParticipantRole:
        return Muc::Role::Participant;
    case QXmppMucItem::ModeratorRole:
        return Muc::Role::Moderator;
    }
    return {};
}

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

namespace QXmpp::Private {

struct MucParticipantData {
    QProperty<QString> nickname;
    QProperty<QString> jid;
    QString occupantId;
    QProperty<Muc::Role> role;
    QProperty<Muc::Affiliation> affiliation;
    QProperty<QXmppPresence> presence;

    MucParticipantData(const QXmppPresence &presence)
        : occupantId(presence.mucOccupantId())
    {
        setBindings();
        setPresence(presence);
    }

    void setBindings()
    {
        nickname.setBinding([&] {
            return QXmppUtils::jidToResource(presence.value().from());
        });
        jid.setBinding([&] {
            return presence.value().mucItem().jid();
        });
        role.setBinding([&] {
            return roleFromLegacy(presence.value().mucItem().role()).value_or(Muc::Role::None);
        });
        affiliation.setBinding([&] {
            return affiliationFromLegacy(presence.value().mucItem().affiliation()).value_or(Muc::Affiliation::None);
        });
    }
    void setPresence(const QXmppPresence &newPresence) { presence = newPresence; }
};

enum class MucRoomState {
    NotJoined,
    JoiningOccupantPresences,
    JoiningRoomHistory,
    Joined,
};

struct DeleteLaterDeleter {
    void operator()(QObject *obj) const { obj->deleteLater(); }
};

struct PendingMessage {
    QXmppPromise<Result<>> promise;
    std::unique_ptr<QTimer, DeleteLaterDeleter> timer;
};

struct MucRoomData {
    MucRoomState state = MucRoomState::NotJoined;
    QProperty<QString> subject;
    QProperty<QString> nickname;
    QProperty<bool> joined = QProperty<bool> { false };
    std::optional<uint32_t> participantId;
    std::unordered_map<uint32_t, MucParticipantData> participants;
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> joinPromise;
    QList<QXmppMessage> historyMessages;
    std::unique_ptr<QTimer, DeleteLaterDeleter> joinTimer;
    std::unordered_map<QString, PendingMessage> pendingMessages;
};

}  // namespace QXmpp::Private

#include "QXmppMucManagerV2_p.h"

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

QXmppTask<Result<QXmppMucRoomV2>> QXmppMucManagerV2::joinRoom(const QString &jid, const QString &nickname)
{
    return joinRoom(jid, nickname, std::nullopt);
}

QXmppTask<Result<QXmppMucRoomV2>> QXmppMucManagerV2::joinRoom(const QString &jid, const QString &nickname,
                                                              std::optional<QXmpp::Muc::HistoryOptions> history)
{
    // nickname empty check
    if (auto itr = d->rooms.find(jid); itr != d->rooms.end()) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(room(jid));
    }

    // request MUC features?

    // create MUC room state
    auto &roomData = d->rooms[jid];
    roomData.state = MucRoomState::JoiningOccupantPresences;
    roomData.nickname = nickname;

    QXmppPromise<Result<QXmppMucRoomV2>> promise;
    auto task = promise.task();
    roomData.joinPromise = std::move(promise);

    QXmppPresence p;
    p.setTo(jid + u'/' + nickname);
    p.setMucSupported(true);
    p.setMucHistory(history);
    client()->send(std::move(p));

    // start timeout timer
    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [this, roomJid = jid]() {
        d->handleJoinTimeout(roomJid);
    });
    roomData.joinTimer.reset(timer);
    timer->start(d->joinTimeout);

    return task;
}

bool QXmppMucManagerV2::handleMessage(const QXmppMessage &message)
{
    if (message.type() != QXmppMessage::GroupChat && message.type() != QXmppMessage::Error) {
        return false;
    }

    auto bareFrom = QXmppUtils::jidToBareJid(message.from());
    auto itr = d->rooms.find(bareFrom);
    if (itr == d->rooms.end()) {
        return false;
    }

    auto &data = itr->second;

    // Handle error responses to sent messages
    if (message.type() == QXmppMessage::Error) {
        if (auto originId = message.originId(); !originId.isEmpty()) {
            if (auto pendingItr = data.pendingMessages.find(originId); pendingItr != data.pendingMessages.end()) {
                pendingItr->second.promise.finish(QXmppError { message.error().text(), message.error() });
                data.pendingMessages.erase(pendingItr);
                return true;
            }
        }
        return false;
    }

    switch (data.state) {
    case MucRoomState::JoiningRoomHistory:
        if (!message.body().isEmpty()) {
            // Has body: history message — cache for delivery after join
            data.historyMessages.append(message);
        } else if (message.hasSubject()) {
            // Has <subject/> but no body: subject message, always the last stanza during joining (XEP-0045 §7.2.7)
            data.subject = message.subject();
            data.state = MucRoomState::Joined;
            data.joined = true;
            data.joinTimer.reset();

            auto history = std::move(data.historyMessages);
            auto promise = std::move(*data.joinPromise);
            data.joinPromise.reset();

            if (!history.isEmpty()) {
                Q_EMIT roomHistoryReceived(bareFrom, history);
            }
            promise.finish(room(bareFrom));
        }
        return true;
    case MucRoomState::Joined:
        // Check for reflected message (match by origin-id)
        if (auto originId = message.originId(); !originId.isEmpty()) {
            if (auto pendingItr = data.pendingMessages.find(originId); pendingItr != data.pendingMessages.end()) {
                pendingItr->second.promise.finish(Success());
                data.pendingMessages.erase(pendingItr);
            }
        }
        if (message.hasSubject() && message.body().isEmpty()) {
            data.subject = message.subject();
        }
        Q_EMIT messageReceived(bareFrom, message);
        return true;
    default:
        return false;
    }
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
    connect(client, &QXmppClient::presenceReceived, this, [this](const auto &p) { d->handlePresence(p); });
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

const MucRoomData *QXmppMucManagerV2::roomData(const QString &jid) const
{
    if (auto itr = d->rooms.find(jid); itr != d->rooms.end()) {
        return &itr->second;
    }
    return nullptr;
}

//
// Manager private: Bookmarks 2
//

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

//
// Manager private: MUC Core
//

void QXmppMucManagerV2Private::handlePresence(const QXmppPresence &p)
{
    auto bareFrom = QXmppUtils::jidToBareJid(p.from());
    if (auto itr = rooms.find(bareFrom); itr != rooms.end()) {
        handleRoomPresence(bareFrom, itr->second, p);
    }
}

void QXmppMucManagerV2Private::handleRoomPresence(const QString &roomJid, QXmpp::Private::MucRoomData &data, const QXmppPresence &presence)
{
    using enum MucRoomState;

    auto nickname = QXmppUtils::jidToResource(presence.from());

    // TODO: clear occupant ID in presence at this point if not supported by MUC to prevent occupant ID injection

    switch (data.state) {
    case NotJoined:
        // did not request to join; ignore
        break;
    case JoiningOccupantPresences:
        if (presence.type() == QXmppPresence::Available) {
            for (const auto &[pId, participant] : data.participants) {
                if (participant.nickname == nickname) {
                    // room sent two presences for the same nickname
                    throwRoomError(roomJid, QXmppError { u"MUC reported two presences for the same nickname"_s });
                    return;
                } else if (participant.occupantId == presence.mucOccupantId()) {
                    // sent two presences with the same occupant ID
                    throwRoomError(roomJid, QXmppError { u"MUC reported two presences for the same occupant ID"_s });
                    return;
                }
            }

            // store new presence
            auto [itr, inserted] = data.participants.emplace(generateParticipantId(), presence);
            QX_ALWAYS_ASSERT(inserted);

            // this is our presence (must be last)
            if (presence.mucStatusCodes().contains(110)) {
                data.state = JoiningRoomHistory;
                if (nickname != data.nickname) {
                    if (presence.mucStatusCodes().contains(210)) {
                        // service modified nickname
                        data.nickname = nickname;
                    } else {
                        throwRoomError(roomJid, QXmppError { u"MUC modified nickname without sending status 210."_s });
                        return;
                    }
                }
            }
        } else if (presence.type() == QXmppPresence::Error) {
            auto error = presence.error();
            throwRoomError(roomJid, QXmppError { u"Cannot join MUC: "_s + error.text(), std::move(error) });
            return;
        }
        break;
    case JoiningRoomHistory:
    case Joined:
        break;
    }
}

void QXmppMucManagerV2Private::throwRoomError(const QString &roomJid, QXmppError &&error)
{
    auto itr = rooms.find(roomJid);
    if (itr == rooms.end()) {
        return;
    }

    // Move promise out before erasing so we can finish it cleanly
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> promise;
    if (itr->second.joinPromise) {
        promise = std::move(itr->second.joinPromise);
    }
    itr->second.joinTimer.reset();
    rooms.erase(itr);

    if (promise) {
        promise->finish(std::move(error));
    }
}

void QXmppMucManagerV2Private::handleJoinTimeout(const QString &roomJid)
{
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(joinTimeout).count();
    throwRoomError(roomJid, QXmppError { u"Joining room timed out after %1 seconds."_s.arg(secs) });
}

void QXmppMucManagerV2Private::handleMessageTimeout(const QString &roomJid, const QString &originId)
{
    auto itr = rooms.find(roomJid);
    if (itr == rooms.end()) {
        return;
    }

    auto pendingItr = itr->second.pendingMessages.find(originId);
    if (pendingItr == itr->second.pendingMessages.end()) {
        return;
    }

    pendingItr->second.promise.finish(QXmppError { u"Sending message timed out."_s });
    itr->second.pendingMessages.erase(pendingItr);
}

//
// MucRoom
//

/// Returns whether the room handle refers to a valid, active room.
bool QXmppMucRoomV2::isValid() const
{
    return m_manager->roomData(m_jid) != nullptr;
}

/// Returns the room subject as a bindable property.
QBindable<QString> QXmppMucRoomV2::subject() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->subject) };
    }
    return {};
}

/// Returns the user's nickname in the room as a bindable property.
QBindable<QString> QXmppMucRoomV2::nickname() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->nickname) };
    }
    return {};
}

/// Returns whether the room is currently joined as a bindable property.
QBindable<bool> QXmppMucRoomV2::joined() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->joined) };
    }
    return {};
}

///
/// Sends a groupchat message to the room.
///
/// The message's \c to and \c type fields are set automatically. An \c origin-id
/// (\xep{0359, Unique and Stable Stanza IDs}) is generated if not already set,
/// and is used to match the server's reflected message.
///
/// The returned task completes with success when the message is reflected back
/// by the MUC service, or with an error if the service responds with an error
/// or the request times out.
///
QXmppTask<Result<>> QXmppMucRoomV2::sendMessage(QXmppMessage message)
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() || itr->second.state != MucRoomState::Joined) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    auto &data = itr->second;

    message.setTo(m_jid);
    message.setType(QXmppMessage::GroupChat);
    if (message.originId().isEmpty()) {
        message.setOriginId(QXmppUtils::generateStanzaUuid());
    }
    auto originId = message.originId();

    QXmppPromise<Result<>> promise;
    auto task = promise.task();

    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, m_manager, [d = m_manager->d.get(), roomJid = m_jid, originId]() {
        d->handleMessageTimeout(roomJid, originId);
    });

    data.pendingMessages.emplace(originId, PendingMessage { std::move(promise), std::unique_ptr<QTimer, DeleteLaterDeleter>(timer) });
    timer->start(m_manager->d->joinTimeout);

    m_manager->client()->send(std::move(message));
    return task;
}

///
/// Sends a private message to a room occupant.
///
/// The message is addressed to the occupant's room JID (room\@service/nick).
/// The returned task completes when the message has been sent to the server.
///
QXmppTask<SendResult> QXmppMucRoomV2::sendPrivateMessage(const QString &occupantNick, QXmppMessage message)
{
    if (!isValid()) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    message.setTo(m_jid + u'/' + occupantNick);
    message.setType(QXmppMessage::Chat);

    return m_manager->client()->send(std::move(message));
}

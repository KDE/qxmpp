// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucManagerV2.h"

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMucForms.h"
#include "QXmppMucIq.h"
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

static QXmppMucItem::Role roleToLegacy(Muc::Role role)
{
    switch (role) {
    case Muc::Role::None:
        return QXmppMucItem::NoRole;
    case Muc::Role::Visitor:
        return QXmppMucItem::VisitorRole;
    case Muc::Role::Participant:
        return QXmppMucItem::ParticipantRole;
    case Muc::Role::Moderator:
        return QXmppMucItem::ModeratorRole;
    }
    return QXmppMucItem::NoRole;
}

static QXmppMucItem::Affiliation affiliationToLegacy(Muc::Affiliation affiliation)
{
    switch (affiliation) {
    case Muc::Affiliation::None:
        return QXmppMucItem::NoAffiliation;
    case Muc::Affiliation::Outcast:
        return QXmppMucItem::OutcastAffiliation;
    case Muc::Affiliation::Member:
        return QXmppMucItem::MemberAffiliation;
    case Muc::Affiliation::Admin:
        return QXmppMucItem::AdminAffiliation;
    case Muc::Affiliation::Owner:
        return QXmppMucItem::OwnerAffiliation;
    }
    return QXmppMucItem::NoAffiliation;
}

static Muc::LeaveReason leaveReasonFromPresence(const QXmppPresence &presence)
{
    if (presence.mucDestroy()) {
        return Muc::LeaveReason::RoomDestroyed;
    }
    const auto &codes = presence.mucStatusCodes();
    if (codes.contains(301)) {
        return Muc::LeaveReason::Banned;
    }
    if (codes.contains(307)) {
        return Muc::LeaveReason::Kicked;
    }
    if (codes.contains(321)) {
        return Muc::LeaveReason::AffiliationChanged;
    }
    if (codes.contains(332)) {
        return Muc::LeaveReason::MembersOnly;
    }
    return Muc::LeaveReason::Left;
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
    Creating,
    Joined,
};

struct DeleteLaterDeleter {
    void operator()(QObject *obj) const
    {
        if (obj) {
            obj->deleteLater();
        }
    }
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
    std::optional<uint32_t> selfParticipantId;
    std::unordered_map<uint32_t, MucParticipantData> participants;
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> joinPromise;
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> createPromise;
    QList<QXmppMessage> historyMessages;
    std::unique_ptr<QTimer, DeleteLaterDeleter> joinTimer;
    std::unordered_map<QString, PendingMessage> pendingMessages;
    std::optional<QXmppPromise<Result<>>> nickChangePromise;
    std::optional<QXmppPromise<Result<>>> leavePromise;
    std::unique_ptr<QTimer, DeleteLaterDeleter> leaveTimer;
    // Room feature flags populated from disco#info after joining (re-fetched on status code 104).
    // isNonAnonymous is additionally updated on status codes 172/173.
    QProperty<bool> isNonAnonymous;
    QProperty<bool> isPublic = QProperty<bool> { true };
    QProperty<bool> isMembersOnly;
    QProperty<bool> isModerated;
    QProperty<bool> isPersistent;
    QProperty<bool> isPasswordProtected;
    // Room info fields populated from muc#roominfo (re-fetched on status code 104)
    QProperty<std::optional<QXmppMucRoomInfo>> roomInfo;
    // Room config — populated when subscribeToRoomConfig(true) is called, re-fetched on status 104
    QProperty<std::optional<QXmppMucRoomConfig>> roomConfig;
    bool watchingRoomConfig = false;
    std::vector<QXmppPromise<Result<>>> roomConfigWaiters;
    // Convenience bindings derived from roomInfo
    QProperty<bool> subjectChangeable;
    QProperty<QString> description;
    QProperty<QString> language;
    QProperty<QStringList> contactJids;
    // Permission properties — bindings set up in setupPermissionBindings()
    // after selfParticipantId is known. Declared after `participants` so that
    // `participants` outlives these bindings during destruction.
    QProperty<bool> canSendMessages;
    QProperty<bool> canChangeSubject;
    QProperty<bool> canSetRoles;
    QProperty<bool> canSetAffiliations;
    QProperty<bool> canConfigureRoom;

    MucRoomData()
    {
        subjectChangeable.setBinding([this]() -> bool {
            const auto &info = roomInfo.value();
            return info && info->subjectChangeable().value_or(false);
        });
        description.setBinding([this]() -> QString {
            const auto &info = roomInfo.value();
            return info ? info->description() : QString {};
        });
        language.setBinding([this]() -> QString {
            const auto &info = roomInfo.value();
            return info ? info->language() : QString {};
        });
        contactJids.setBinding([this]() -> QStringList {
            const auto &info = roomInfo.value();
            return info ? info->contactJids() : QStringList {};
        });
    }

    void setupPermissionBindings()
    {
        Q_ASSERT(selfParticipantId.has_value());
        auto &pData = participants.at(*selfParticipantId);

        canSendMessages.setBinding([&pData]() {
            auto role = pData.role.value();
            return role == Muc::Role::Participant || role == Muc::Role::Moderator;
        });
        canChangeSubject.setBinding([this, &pData]() {
            auto role = pData.role.value();
            return role == Muc::Role::Moderator ||
                (role == Muc::Role::Participant && subjectChangeable.value());
        });
        canSetRoles.setBinding([&pData]() {
            return pData.role.value() == Muc::Role::Moderator;
        });
        canSetAffiliations.setBinding([&pData]() {
            auto affil = pData.affiliation.value();
            return affil == Muc::Affiliation::Admin || affil == Muc::Affiliation::Owner;
        });
        canConfigureRoom.setBinding([&pData]() {
            return pData.affiliation.value() == Muc::Affiliation::Owner;
        });
    }
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
QStringList QXmppMucManagerV2::discoveryFeatures() const
{
    return { ns_muc.toString(), ns_bookmarks2 + u"+notify" };
}

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
                                                              std::optional<QXmpp::Muc::HistoryOptions> history,
                                                              const QString &password)
{
    // nickname empty check
    if (nickname.isEmpty()) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Nickname must not be empty."_s });
    }
    if (auto itr = d->rooms.find(jid); itr != d->rooms.end()) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(room(jid));
    }

    // create MUC room state
    auto &roomData = d->rooms[jid];
    roomData.state = MucRoomState::JoiningOccupantPresences;
    roomData.nickname = nickname;

    // Fetch room features in parallel; updates roominfo properties when it arrives.
    d->fetchRoomInfo(jid);

    QXmppPromise<Result<QXmppMucRoomV2>> promise;
    auto task = promise.task();
    roomData.joinPromise = std::move(promise);

    QXmppPresence p;
    p.setTo(jid + u'/' + nickname);
    p.setMucSupported(true);
    p.setMucHistory(history);
    p.setMucPassword(password);
    client()->send(std::move(p));

    // start timeout timer
    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [this, roomJid = jid]() {
        d->handleJoinTimeout(roomJid);
    });
    roomData.joinTimer.reset(timer);
    timer->start(d->timeout);

    return task;
}

///
/// Creates a new reserved MUC room at \a jid with the given \a nickname.
///
/// The room is created in a locked state; no other users can join until the
/// owner submits the configuration form via QXmppMucRoomV2::setRoomConfig().
///
/// The returned task resolves once the server has confirmed room creation
/// (XEP-0045 status code 201) and the configuration form has been fetched.
/// If the room already exists the join will succeed normally and the task
/// will fail with an error. If the local state machine detects that the room
/// already exists (already tracked) the task fails immediately.
///
/// \since QXmpp 1.15
///
QXmppTask<Result<QXmppMucRoomV2>> QXmppMucManagerV2::createRoom(const QString &jid, const QString &nickname)
{
    if (nickname.isEmpty()) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Nickname must not be empty."_s });
    }
    if (d->rooms.count(jid)) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Room is already tracked (join or create already in progress)."_s });
    }

    auto &roomData = d->rooms[jid];
    roomData.state = MucRoomState::JoiningOccupantPresences;
    roomData.nickname = nickname;

    QXmppPromise<Result<QXmppMucRoomV2>> promise;
    auto task = promise.task();
    roomData.createPromise = std::move(promise);

    QXmppPresence p;
    p.setTo(jid + u'/' + nickname);
    p.setMucSupported(true);
    client()->send(std::move(p));

    // start timeout timer (reuse join timeout)
    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, this, [this, roomJid = jid]() {
        d->handleJoinTimeout(roomJid);
    });
    roomData.joinTimer.reset(timer);
    timer->start(d->timeout);

    return task;
}

bool QXmppMucManagerV2::handleMessage(const QXmppMessage &message)
{
    const auto type = message.type();
    if (type != QXmppMessage::GroupChat && type != QXmppMessage::Error && type != QXmppMessage::Normal) {
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

    // Handle Normal-type messages from the room (e.g., voice request approval forms)
    if (message.type() == QXmppMessage::Normal) {
        if (data.state == MucRoomState::Joined) {
            if (auto voiceRequest = message.mucVoiceRequest()) {
                Q_EMIT voiceRequestReceived(bareFrom, *voiceRequest);
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
        // Status 104: room configuration changed — re-fetch roominfo and config (if subscribed)
        if (message.mucStatusCodes().contains(104)) {
            d->fetchRoomInfo(bareFrom);
            if (data.watchingRoomConfig) {
                d->fetchRoomConfigSubscribed(bareFrom);
            }
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
    connect(client, &QXmppClient::disconnected, this, &QXmppMucManagerV2::onDisconnected);
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
        d->clearAllRooms();
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

void QXmppMucManagerV2::onDisconnected()
{
    if (client()->streamManagementState() == QXmppClient::NoStreamManagement) {
        d->clearAllRooms();
    }
}

const MucRoomData *QXmppMucManagerV2::roomData(const QString &jid) const
{
    if (auto itr = d->rooms.find(jid); itr != d->rooms.end()) {
        return &itr->second;
    }
    return nullptr;
}

const MucParticipantData *QXmppMucManagerV2::participantData(const QString &roomJid, uint32_t participantId) const
{
    if (auto roomItr = d->rooms.find(roomJid); roomItr != d->rooms.end()) {
        if (auto pItr = roomItr->second.participants.find(participantId); pItr != roomItr->second.participants.end()) {
            return &pItr->second;
        }
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
                if (participant.nickname.value() == nickname) {
                    // room sent two presences for the same nickname
                    throwRoomError(roomJid, QXmppError { u"MUC reported two presences for the same nickname"_s });
                    return;
                } else if (!participant.occupantId.isEmpty() && participant.occupantId == presence.mucOccupantId()) {
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
                data.selfParticipantId = itr->first;
                data.setupPermissionBindings();
                if (nickname != data.nickname) {
                    if (presence.mucStatusCodes().contains(210)) {
                        // service modified nickname
                        data.nickname = nickname;
                    } else {
                        throwRoomError(roomJid, QXmppError { u"MUC modified nickname without sending status 210."_s });
                        return;
                    }
                }

                if (presence.mucStatusCodes().contains(201)) {
                    // New room was created and is locked (XEP-0045 §10.1).
                    if (data.createPromise) {
                        // createRoom() flow: transition to Creating state.
                        // Fetch the config form; the promise is resolved when the form arrives.
                        data.state = Creating;
                        data.joinTimer.reset();
                        fetchConfigForm(roomJid);
                    } else {
                        // joinRoom() flow: we accidentally created a new room.
                        // Send cancel IQ to destroy the locked room and fail the join.
                        QXmppMucOwnerIq cancelIq;
                        cancelIq.setTo(roomJid);
                        cancelIq.setType(QXmppIq::Set);
                        QXmppDataForm cancelForm;
                        cancelForm.setType(QXmppDataForm::Cancel);
                        cancelIq.setForm(cancelForm);
                        q->client()->sendIq(std::move(cancelIq));
                        throwRoomError(roomJid, QXmppError { u"Room does not exist."_s });
                    }
                } else {
                    if (data.createPromise) {
                        // createRoom() flow: the room already existed — fail.
                        throwRoomError(roomJid, QXmppError { u"Room already exists."_s });
                    } else {
                        data.state = JoiningRoomHistory;
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
    case Creating:
    case Joined:
        if (presence.type() == QXmppPresence::Unavailable && presence.mucStatusCodes().contains(303)) {
            // Nickname change (XEP-0045 §7.6): unavailable with 303 + new nick in item
            auto newNick = presence.mucItem().nick();
            auto isSelf = presence.mucStatusCodes().contains(110);

            if (isSelf && !newNick.isEmpty()) {
                data.nickname = newNick;
                if (data.nickChangePromise) {
                    auto promise = std::move(*data.nickChangePromise);
                    data.nickChangePromise.reset();
                    promise.finish(Success());
                }
            }

            // Update participant's presence with new nickname so the
            // following available presence matches by nickname.
            if (!newNick.isEmpty()) {
                for (auto &[pId, pData] : data.participants) {
                    if (pData.nickname.value() == nickname) {
                        auto updated = presence;
                        updated.setFrom(roomJid + u'/' + newNick);
                        pData.setPresence(updated);
                        break;
                    }
                }
            }
        } else if (presence.type() == QXmppPresence::Unavailable && presence.mucStatusCodes().contains(110)) {
            // Self-unavailable without 303: we left the room
            auto reason = leaveReasonFromPresence(presence);
            std::optional<QXmppPromise<Result<>>> promise;
            if (data.leavePromise) {
                promise = std::move(data.leavePromise);
            }

            if (reason != Muc::LeaveReason::Left) {
                Q_EMIT q->removedFromRoom(roomJid, reason, presence.mucDestroy());
            }

            rooms.erase(roomJid);

            if (promise) {
                promise->finish(Success());
            }
        } else if (presence.type() == QXmppPresence::Unavailable && !presence.mucStatusCodes().contains(110)) {
            // Another participant left the room
            auto reason = leaveReasonFromPresence(presence);
            for (auto pItr = data.participants.begin(); pItr != data.participants.end(); ++pItr) {
                if (pItr->second.nickname.value() == nickname) {
                    auto participantId = pItr->first;
                    Q_EMIT q->participantLeft(roomJid, QXmppMucParticipant(q, roomJid, participantId), reason);
                    data.participants.erase(pItr);
                    break;
                }
            }
        } else if (presence.type() == QXmppPresence::Available) {
            // Check if participant already exists
            bool found = false;
            for (auto &[pId, pData] : data.participants) {
                if (pData.nickname.value() == nickname) {
                    pData.setPresence(presence);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // New participant joined
                auto id = generateParticipantId();
                data.participants.emplace(id, presence);
                Q_EMIT q->participantJoined(roomJid, QXmppMucParticipant(q, roomJid, id));
            }
        } else if (presence.type() == QXmppPresence::Error) {
            if (data.leavePromise) {
                auto error = presence.error();
                auto promise = std::move(*data.leavePromise);
                data.leavePromise.reset();
                data.leaveTimer.reset();
                promise.finish(QXmppError { error.text(), std::move(error) });
            } else if (data.nickChangePromise) {
                auto error = presence.error();
                auto promise = std::move(*data.nickChangePromise);
                data.nickChangePromise.reset();
                promise.finish(QXmppError { error.text(), std::move(error) });
            }
        }
        // Status 172/173: privacy-related anonymity change (XEP-0045 §10.2)
        const auto &codes = presence.mucStatusCodes();
        if (codes.contains(172)) {
            data.isNonAnonymous = true;
        } else if (codes.contains(173)) {
            data.isNonAnonymous = false;
        }
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
    } else if (itr->second.createPromise) {
        promise = std::move(itr->second.createPromise);
    }
    itr->second.joinTimer.reset();
    rooms.erase(itr);

    if (promise) {
        promise->finish(std::move(error));
    }
}

void QXmppMucManagerV2Private::clearAllRooms()
{
    // Collect pending promises before clearing so their callbacks see a clean (empty) room state.
    std::vector<QXmppPromise<Result<QXmppMucRoomV2>>> joinPromises;
    std::vector<QXmppPromise<Result<>>> otherPromises;

    for (auto &[jid, data] : rooms) {
        // Notify QBindable observers while the room data is still valid
        data.joined = false;

        if (data.joinPromise) {
            joinPromises.push_back(std::move(*data.joinPromise));
        }
        if (data.createPromise) {
            joinPromises.push_back(std::move(*data.createPromise));
        }
        if (data.leavePromise) {
            otherPromises.push_back(std::move(*data.leavePromise));
        }
        if (data.nickChangePromise) {
            otherPromises.push_back(std::move(*data.nickChangePromise));
        }
        for (auto &[msgId, pending] : data.pendingMessages) {
            otherPromises.push_back(std::move(pending.promise));
        }
    }
    rooms.clear();

    const auto error = QXmppError { u"Disconnected from server."_s };
    for (auto &promise : joinPromises) {
        promise.finish(error);
    }
    for (auto &promise : otherPromises) {
        promise.finish(error);
    }
}

void QXmppMucManagerV2Private::fetchRoomInfo(const QString &roomJid)
{
    auto *disco = q->client()->findExtension<QXmppDiscoveryManager>();
    if (!disco) {
        return;
    }
    disco->info(roomJid, {}, QXmppDiscoveryManager::CachePolicy::Strict).then(q, [this, roomJid](auto &&result) {
        auto itr = rooms.find(roomJid);
        if (itr == rooms.end() || hasError(result)) {
            return;
        }
        auto &info = getValue(result);
        auto &data = itr->second;
        data.roomInfo = info.template dataForm<QXmppMucRoomInfo>();
        const auto &features = info.features();
        data.isNonAnonymous = features.contains(muc_feat_nonanonymous);
        data.isPublic = features.contains(muc_feat_public);
        data.isMembersOnly = features.contains(muc_feat_membersonly);
        data.isModerated = features.contains(muc_feat_moderated);
        data.isPersistent = features.contains(muc_feat_persistent);
        data.isPasswordProtected = features.contains(muc_feat_passwordprotected);
    });
}

void QXmppMucManagerV2Private::fetchConfigForm(const QString &roomJid)
{
    QXmppMucOwnerIq iq;
    iq.setTo(roomJid);
    iq.setType(QXmppIq::Get);
    q->client()->sendIq(std::move(iq)).then(q, [this, roomJid](QXmppClient::IqResult &&result) {
        auto itr = rooms.find(roomJid);
        if (itr == rooms.end() || itr->second.state != MucRoomState::Creating) {
            return;
        }
        auto &data = itr->second;

        if (auto *error = std::get_if<QXmppError>(&result)) {
            throwRoomError(roomJid, std::move(*error));
            return;
        }

        // Parse the owner IQ response DOM element
        auto iqResult = QXmppMucOwnerIq();
        iqResult.parse(std::get<QDomElement>(result));

        // Resolve the createPromise — room is locked, owner can now configure it
        auto promise = std::move(*data.createPromise);
        data.createPromise.reset();
        promise.finish(q->room(roomJid));
    });
}

void QXmppMucManagerV2Private::fetchRoomConfigSubscribed(const QString &roomJid)
{
    QXmppMucOwnerIq iq;
    iq.setTo(roomJid);
    iq.setType(QXmppIq::Get);
    q->client()->sendIq(std::move(iq)).then(q, [this, roomJid](QXmppClient::IqResult &&result) {
        auto itr = rooms.find(roomJid);
        if (itr == rooms.end()) {
            return;
        }
        auto &data = itr->second;
        auto waiters = std::move(data.roomConfigWaiters);

        if (auto *error = std::get_if<QXmppError>(&result)) {
            for (auto &p : waiters) {
                p.finish(QXmppError { error->description });
            }
            return;
        }

        QXmppMucOwnerIq iqResult;
        iqResult.parse(std::get<QDomElement>(result));
        auto config = QXmppMucRoomConfig::fromDataForm(iqResult.form());
        if (!config) {
            for (auto &p : waiters) {
                p.finish(QXmppError { u"Server returned an invalid or missing muc#roomconfig form."_s });
            }
            return;
        }
        data.roomConfig = std::move(config);
        for (auto &p : waiters) {
            p.finish(QXmpp::Success {});
        }
    });
}

void QXmppMucManagerV2Private::handleJoinTimeout(const QString &roomJid)
{
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(timeout).count();
    throwRoomError(roomJid, QXmppError { u"Joining room timed out after %1 seconds."_s.arg(secs) });
}

void QXmppMucManagerV2Private::handleLeaveTimeout(const QString &roomJid)
{
    auto itr = rooms.find(roomJid);
    if (itr == rooms.end()) {
        return;
    }

    std::optional<QXmppPromise<Result<>>> promise;
    if (itr->second.leavePromise) {
        promise = std::move(itr->second.leavePromise);
    }
    rooms.erase(itr);

    if (promise) {
        promise->finish(QXmppError { u"Leaving room timed out."_s });
    }
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
/// Returns a list of all participants currently in the room.
///
/// The returned handles are lightweight and do not own any data.
///
QList<QXmppMucParticipant> QXmppMucRoomV2::participants() const
{
    QList<QXmppMucParticipant> result;
    if (auto *data = m_manager->roomData(m_jid)) {
        result.reserve(data->participants.size());
        for (const auto &[id, _] : data->participants) {
            result.append(QXmppMucParticipant(m_manager, m_jid, id));
        }
    }
    return result;
}

///
/// Returns a handle to the local user's own participant entry, if joined.
///
/// Returns \c std::nullopt before joinRoom() has completed or after leaving the room.
/// Use the returned handle's role() and affiliation() QBindables to reactively
/// observe your own permissions — they update automatically when the MUC service
/// grants or revokes permissions.
///
/// \note The participant handle is only valid while the room is joined.
/// Do not use it after leaving the room or after a non-resumed reconnect.
///
std::optional<QXmppMucParticipant> QXmppMucRoomV2::selfParticipant() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        if (data->selfParticipantId.has_value()) {
            return QXmppMucParticipant(m_manager, m_jid, *data->selfParticipantId);
        }
    }
    return std::nullopt;
}

/// Returns whether the local user can send groupchat messages.
///
/// True when the user's role is Participant or Moderator.
/// In unmoderated rooms the server assigns Participant by default.
QBindable<bool> QXmppMucRoomV2::canSendMessages() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->canSendMessages) };
    }
    return {};
}

/// Returns whether the local user can change the room subject.
///
/// True for Moderators, or for Participants when the room allows it
/// (\c muc#roominfo_subjectmod). Defaults to false until disco\#info arrives.
QBindable<bool> QXmppMucRoomV2::canChangeSubject() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->canChangeSubject) };
    }
    return {};
}

/// Returns whether the local user can change other participants' roles (XEP-0045 §8.4–8.6).
///
/// True when the user's role is Moderator. This covers role changes including kicking
/// (setting role to None) and granting/revoking voice.
QBindable<bool> QXmppMucRoomV2::canSetRoles() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->canSetRoles) };
    }
    return {};
}

/// Returns whether the local user can change affiliations (XEP-0045 §9).
///
/// True when the user's affiliation is Admin or Owner. This covers banning,
/// granting and revoking membership, and querying affiliation lists.
QBindable<bool> QXmppMucRoomV2::canSetAffiliations() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->canSetAffiliations) };
    }
    return {};
}

/// Returns whether the local user can configure the room (XEP-0045 §10).
///
/// True when the user's affiliation is Owner.
QBindable<bool> QXmppMucRoomV2::canConfigureRoom() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->canConfigureRoom) };
    }
    return {};
}

///
/// Returns whether the room is non-anonymous (XEP-0045 §4.2).
///
/// In a non-anonymous room, all occupants can see each other's real JIDs.
/// In a semi-anonymous room (the default), only moderators can see real JIDs.
///
/// Populated from the \c muc_nonanonymous disco feature on join and re-fetched
/// on status code 104. Also updated immediately on status codes 172 (now non-anonymous)
/// and 173 (now semi-anonymous), without a round-trip.
///
/// Defaults to \c false (semi-anonymous) until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isNonAnonymous() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isNonAnonymous) };
    }
    return {};
}

///
/// Returns whether the room is publicly listed (XEP-0045 §4.2).
///
/// Public rooms (\c muc_public) appear in service discovery results.
/// Hidden rooms (\c muc_hidden) do not.
///
/// Populated from the \c muc_public disco feature on join and re-fetched
/// on status code 104.
///
/// Defaults to \c true until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isPublic() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isPublic) };
    }
    return {};
}

///
/// Returns whether the room is members-only (XEP-0045 §4.2).
///
/// In a members-only room (\c muc_membersonly), users must be on the member
/// list to enter. Open rooms (\c muc_open) allow any non-banned user to join.
///
/// Populated from the \c muc_membersonly disco feature on join and re-fetched
/// on status code 104.
///
/// Defaults to \c false until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isMembersOnly() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isMembersOnly) };
    }
    return {};
}

///
/// Returns whether the room is moderated (XEP-0045 §4.2).
///
/// In a moderated room (\c muc_moderated), only occupants with voice
/// (role Participant or Moderator) can send messages. Visitors cannot.
///
/// Populated from the \c muc_moderated disco feature on join and re-fetched
/// on status code 104.
///
/// Defaults to \c false until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isModerated() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isModerated) };
    }
    return {};
}

///
/// Returns whether the room is persistent (XEP-0045 §4.2).
///
/// A persistent room (\c muc_persistent) is not destroyed when the last
/// occupant exits. A temporary room (\c muc_temporary) is destroyed automatically.
///
/// Populated from the \c muc_persistent disco feature on join and re-fetched
/// on status code 104.
///
/// Defaults to \c false until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isPersistent() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isPersistent) };
    }
    return {};
}

///
/// Returns whether the room requires a password to enter (XEP-0045 §4.2).
///
/// Password-protected rooms (\c muc_passwordprotected) require the correct
/// password when joining (XEP-0045 §7.2.5).
///
/// Populated from the \c muc_passwordprotected disco feature on join and
/// re-fetched on status code 104.
///
/// Defaults to \c false until disco\#info arrives.
///
/// \since QXmpp 1.15
///
QBindable<bool> QXmppMucRoomV2::isPasswordProtected() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->isPasswordProtected) };
    }
    return {};
}

/// Returns the full \c muc#roominfo data form from \xep{0045, Multi-User Chat}.
///
/// The form is fetched via \c disco\#info automatically on join and re-fetched whenever
/// a status code 104 (room configuration changed) is received.
/// It gives access to all available room info fields in one object, including fields
/// not individually exposed (e.g. \c muc#roominfo_subject, \c muc#roominfo_occupants,
/// \c muc#roominfo_avatarhash).
///
/// Returns \c std::nullopt until the first \c disco\#info response has arrived or if
/// the room's disco\#info does not include a \c muc#roominfo form.
///
/// \sa description(), language(), contactJids()
/// \since QXmpp 1.15
///
QBindable<std::optional<QXmppMucRoomInfo>> QXmppMucRoomV2::roomInfo() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->roomInfo) };
    }
    return {};
}

///
/// Returns the current room configuration form from \xep{0045, Multi-User Chat}.
///
/// The form is only populated after subscribeToRoomConfig(true) has been called
/// and the initial fetch has completed. It is automatically re-fetched whenever a
/// status code 104 (room configuration changed) is received.
///
/// Returns \c std::nullopt until the first response has arrived or if
/// subscribeToRoomConfig() has not been called.
///
/// \sa subscribeToRoomConfig()
/// \since QXmpp 1.15
///
QBindable<std::optional<QXmppMucRoomConfig>> QXmppMucRoomV2::roomConfig() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->roomConfig) };
    }
    return {};
}

///
/// Enables or disables automatic room configuration updates.
///
/// When set to \c true, status code 104 messages (room configuration changed)
/// will automatically trigger a re-fetch to keep roomConfig() current. If the
/// configuration has not been fetched yet, a fetch is triggered immediately
/// (fire-and-forget; use requestRoomConfig() if you need the result).
///
/// When set to \c false, disables auto-refresh. The last fetched configuration
/// remains available in roomConfig() but is no longer updated automatically.
///
/// \sa roomConfig(), requestRoomConfig()
/// \since QXmpp 1.15
///
void QXmppMucRoomV2::setWatchRoomConfig(bool watch)
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end()) {
        return;
    }
    auto &data = itr->second;
    const bool needsFetch = watch && !data.watchingRoomConfig && !data.roomConfig.value().has_value();
    data.watchingRoomConfig = watch;
    if (needsFetch) {
        m_manager->d->fetchRoomConfigSubscribed(m_jid);
    }
}

///
/// Returns whether automatic room configuration updates are enabled.
///
/// \sa setWatchRoomConfig()
/// \since QXmpp 1.15
///
bool QXmppMucRoomV2::isWatchingRoomConfig() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return data->watchingRoomConfig;
    }
    return false;
}

///
/// Returns the room description from \xep{0045, Multi-User Chat} \c muc#roominfo.
///
/// The description is populated from the \c muc#roominfo_description field of the
/// \c disco\#info response. It is fetched automatically on join and re-fetched whenever
/// a status code 104 (room configuration changed) is received.
///
/// Returns an empty string until the \c disco\#info response has arrived.
///
/// \since QXmpp 1.15
///
QBindable<QString> QXmppMucRoomV2::description() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->description) };
    }
    return {};
}

///
/// Returns the language of the room discussion from \xep{0045, Multi-User Chat} \c muc#roominfo.
///
/// The language tag is populated from the \c muc#roominfo_lang field of the
/// \c disco\#info response, following BCP 47 (e.g. \c "en" or \c "de"). It is fetched
/// automatically on join and re-fetched whenever a status code 104 is received.
///
/// Returns an empty string until the \c disco\#info response has arrived.
///
/// \since QXmpp 1.15
///
QBindable<QString> QXmppMucRoomV2::language() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->language) };
    }
    return {};
}

///
/// Returns the list of admin contact JIDs for the room from \xep{0045, Multi-User Chat} \c muc#roominfo.
///
/// The JIDs are populated from the \c muc#roominfo_contactjid field of the
/// \c disco\#info response. They represent real JIDs of people responsible for the
/// room, which may differ from the room's current affiliations list. The list is fetched
/// automatically on join and re-fetched whenever a status code 104 is received.
///
/// Returns an empty list until the \c disco\#info response has arrived or if the
/// room has no contact JIDs configured.
///
/// \since QXmpp 1.15
///
QBindable<QStringList> QXmppMucRoomV2::contactJids() const
{
    if (auto *data = m_manager->roomData(m_jid)) {
        return { &(data->contactJids) };
    }
    return {};
}

///
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
    timer->start(m_manager->d->timeout);

    m_manager->client()->send(std::move(message));
    return task;
}

///
/// Sends a private message to a room occupant.
///
/// The message is addressed to the occupant's current room JID (room\@service/nick).
/// The returned task completes when the message has been sent to the server.
///
QXmppTask<SendResult> QXmppMucRoomV2::sendPrivateMessage(const QXmppMucParticipant &participant, QXmppMessage message)
{
    if (!isValid()) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    auto *pData = m_manager->participantData(m_jid, participant.m_participantId);
    if (!pData) {
        return makeReadyTask<SendResult>(QXmppError { u"Participant is no longer in the room."_s });
    }

    message.setTo(m_jid + u'/' + pData->nickname.value());
    message.setType(QXmppMessage::Chat);

    return m_manager->client()->send(std::move(message));
}

///
/// Changes the room subject.
///
/// Sends a subject-only groupchat message. The returned task completes when
/// the MUC service reflects the subject change, or with an error if the
/// service rejects it or the request times out.
///
/// \sa subject()
///
QXmppTask<Result<>> QXmppMucRoomV2::setSubject(const QString &subject)
{
    QXmppMessage message;
    message.setSubject(subject);
    return sendMessage(std::move(message));
}

///
/// Changes the user's nickname in the room.
///
/// Sends a presence to the new occupant JID. The returned task completes
/// when the MUC service confirms the change (status code 303), or with an
/// error if it is rejected.
///
/// \sa nickname()
///
QXmppTask<Result<>> QXmppMucRoomV2::setNickname(const QString &newNick)
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() || itr->second.state != MucRoomState::Joined) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    auto &data = itr->second;

    // Cancel any pending nickname change
    if (data.nickChangePromise) {
        auto oldPromise = std::move(*data.nickChangePromise);
        data.nickChangePromise.reset();
        oldPromise.finish(QXmppError { u"Superseded by a new nickname change request."_s });
    }

    QXmppPromise<Result<>> promise;
    auto task = promise.task();
    data.nickChangePromise = std::move(promise);

    QXmppPresence p;
    p.setTo(m_jid + u'/' + newNick);
    m_manager->client()->send(std::move(p));

    return task;
}

///
/// Changes the user's presence in the room.
///
/// The presence is addressed to the user's current occupant JID. The returned
/// task completes when the presence has been sent to the server.
///
QXmppTask<SendResult> QXmppMucRoomV2::setPresence(QXmppPresence presence)
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end()) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    presence.setTo(m_jid + u'/' + itr->second.nickname.value());
    return m_manager->client()->send(std::move(presence));
}

///
/// Leaves the room by sending an unavailable presence (XEP-0045 §7.14).
///
/// The returned task completes when the server confirms the leave with a
/// self-unavailable presence containing status code 110.
///
QXmppTask<Result<>> QXmppMucRoomV2::leave()
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end()) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    auto &data = itr->second;
    if (data.leavePromise) {
        return makeReadyTask<Result<>>(QXmppError { u"Already leaving the room."_s });
    }

    QXmppPresence p;
    p.setTo(m_jid + u'/' + data.nickname.value());
    p.setType(QXmppPresence::Unavailable);

    auto sendResult = m_manager->client()->send(std::move(p));
    // Check if send failed immediately
    if (sendResult.isFinished()) {
        if (std::holds_alternative<QXmppError>(sendResult.result())) {
            return makeReadyTask<Result<>>(std::get<QXmppError>(std::move(sendResult.result())));
        }
    }

    QXmppPromise<Result<>> promise;
    auto task = promise.task();
    data.leavePromise = std::move(promise);

    // Start timeout timer
    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, m_manager, [d = m_manager->d.get(), roomJid = m_jid]() {
        d->handleLeaveTimeout(roomJid);
    });
    data.leaveTimer.reset(timer);
    timer->start(m_manager->d->timeout);

    return task;
}

///
/// Changes the role of a room participant (XEP-0045 §8.4–8.6).
///
/// Role changes are transient and apply only for the current session.
/// The \a participant must still be in the room. Roles are identified by
/// the participant's current nickname.
///
QXmppTask<Result<>> QXmppMucRoomV2::setRole(const QXmppMucParticipant &participant, QXmpp::Muc::Role role, const QString &reason)
{
    auto *pData = m_manager->participantData(m_jid, participant.m_participantId);
    if (!pData) {
        return makeReadyTask<Result<>>(QXmppError { u"Participant is no longer in the room."_s });
    }

    QXmppMucItem item;
    item.setNick(pData->nickname.value());
    item.setRole(roleToLegacy(role));
    item.setReason(reason);

    QXmppMucAdminIq iq;
    iq.setType(QXmppIq::Set);
    iq.setTo(m_jid);
    iq.setItems({ item });

    return m_manager->client()->sendGenericIq(std::move(iq));
}

///
/// Changes the affiliation of a user by bare JID (XEP-0045 §9).
///
/// Affiliation changes are persistent. The \a jid must be a bare JID.
/// The user does not need to be currently in the room.
///
QXmppTask<Result<>> QXmppMucRoomV2::setAffiliation(const QString &jid, QXmpp::Muc::Affiliation affiliation, const QString &reason)
{
    QXmppMucItem item;
    item.setJid(jid);
    item.setAffiliation(affiliationToLegacy(affiliation));
    item.setReason(reason);

    QXmppMucAdminIq iq;
    iq.setType(QXmppIq::Set);
    iq.setTo(m_jid);
    iq.setItems({ item });

    return m_manager->client()->sendGenericIq(std::move(iq));
}

///
/// Requests the list of all users with a given \a affiliation (XEP-0045 §9.5–9.8).
///
/// Returns a snapshot of the affiliation list. There are no live updates;
/// call this method again after making changes if you need a fresh list.
///
/// Only admins and owners are allowed to retrieve these lists.
///
QXmppTask<Result<QList<QXmppMucItem>>> QXmppMucRoomV2::requestAffiliationList(QXmpp::Muc::Affiliation affiliation)
{
    QXmppMucItem item;
    item.setAffiliation(affiliationToLegacy(affiliation));

    QXmppMucAdminIq iq;
    iq.setType(QXmppIq::Get);
    iq.setTo(m_jid);
    iq.setItems({ item });

    return chainIq(m_manager->client()->sendIq(std::move(iq)), m_manager,
                   [](QXmppMucAdminIq &&iq) -> Result<QList<QXmppMucItem>> {
                       return iq.items();
                   });
}

///
/// Requests voice in a moderated room as a visitor.
///
/// Sends a \c muc#request data form to the room. The MUC service forwards an
/// approval form to all moderators, who can then call answerVoiceRequest()
/// to grant or deny voice.
///
/// The returned task reflects the stream-management acknowledgement:
/// QXmpp::SendSuccess::acknowledged is \c true once the server has confirmed
/// receipt. Returns an error task immediately if the room is not currently joined.
///
/// \since QXmpp 1.15
///
QXmppTask<SendResult> QXmppMucRoomV2::requestVoice()
{
    if (m_manager->d->rooms.find(m_jid) == m_manager->d->rooms.end() ||
        m_manager->d->rooms.at(m_jid).state != MucRoomState::Joined) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    QXmppMessage message;
    message.setTo(m_jid);
    message.setType(QXmppMessage::Normal);
    message.setMucVoiceRequest(QXmppMucVoiceRequest {});
    return m_manager->client()->send(std::move(message));
}

///
/// Approves or denies a voice request as a moderator.
///
/// Sends the \a request form back to the room with \c muc#request_allow set to
/// \a allow. If \a allow is \c true the MUC service promotes the visitor to
/// participant; if \c false the request is denied and the visitor remains a
/// visitor.
///
/// The returned task reflects the stream-management acknowledgement:
/// QXmpp::SendSuccess::acknowledged is \c true once the server has confirmed
/// receipt. Returns an error task immediately if the room is not currently joined.
///
/// \since QXmpp 1.15
///
QXmppTask<SendResult> QXmppMucRoomV2::answerVoiceRequest(const QXmppMucVoiceRequest &request, bool allow)
{
    if (m_manager->d->rooms.find(m_jid) == m_manager->d->rooms.end() ||
        m_manager->d->rooms.at(m_jid).state != MucRoomState::Joined) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    auto response = request;
    response.setRequestAllow(allow);

    QXmppMessage message;
    message.setTo(m_jid);
    message.setType(QXmppMessage::Normal);
    message.setMucVoiceRequest(std::move(response));
    return m_manager->client()->send(std::move(message));
}

///
/// Requests the current room configuration form from the server.
///
/// Valid in both the \c Creating and \c Joined states. Returns a
/// QXmppMucRoomConfig that can be edited and submitted via setRoomConfig().
/// Also updates the roomConfig() bindable on success.
///
/// If a configuration has already been fetched (cached in roomConfig()), it
/// is returned immediately without sending a network request.
///
/// If \a watch is \c true, enables automatic re-fetching on status code 104
/// (room configuration changed). Equivalent to calling setWatchRoomConfig(true)
/// before this call.
///
/// \sa setRoomConfig(), roomConfig(), setWatchRoomConfig()
/// \since QXmpp 1.15
///
QXmppTask<Result<QXmppMucRoomConfig>> QXmppMucRoomV2::requestRoomConfig(bool watch)
{
    using enum MucRoomState;
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() ||
        (itr->second.state != Creating && itr->second.state != Joined)) {
        return makeReadyTask<Result<QXmppMucRoomConfig>>(QXmppError { u"Room is not in Creating or Joined state."_s });
    }

    if (watch) {
        itr->second.watchingRoomConfig = true;
    }

    // Return cached value if available
    if (const auto &cached = itr->second.roomConfig.value()) {
        return makeReadyTask<Result<QXmppMucRoomConfig>>(*cached);
    }

    QXmppMucOwnerIq iq;
    iq.setTo(m_jid);
    iq.setType(QXmppIq::Get);
    return chainIq(m_manager->client()->sendIq(std::move(iq)), m_manager,
                   [this, jid = m_jid](QXmppMucOwnerIq &&iq) -> Result<QXmppMucRoomConfig> {
                       auto config = QXmppMucRoomConfig::fromDataForm(iq.form());
                       if (!config) {
                           return QXmppError { u"Server returned an invalid or missing muc#roomconfig form."_s };
                       }
                       if (auto itr2 = m_manager->d->rooms.find(jid); itr2 != m_manager->d->rooms.end()) {
                           itr2->second.roomConfig = config;
                       }
                       return std::move(*config);
                   });
}

///
/// Submits the room configuration to the server.
///
/// In the \c Creating state (after createRoom()) this unlocks the room and
/// transitions it to the \c Joined state. In the \c Joined state this
/// updates an existing room's configuration.
///
/// \since QXmpp 1.15
///
QXmppTask<Result<>> QXmppMucRoomV2::setRoomConfig(const QXmppMucRoomConfig &config)
{
    using enum MucRoomState;
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() ||
        (itr->second.state != Creating && itr->second.state != Joined)) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not in Creating or Joined state."_s });
    }

    auto form = config.toDataForm();
    form.setType(QXmppDataForm::Submit);

    QXmppMucOwnerIq iq;
    iq.setTo(m_jid);
    iq.setType(QXmppIq::Set);
    iq.setForm(std::move(form));

    const bool wasCreating = (itr->second.state == Creating);
    return chainIq(m_manager->client()->sendIq(std::move(iq)), m_manager,
                   [this, wasCreating](QXmppMucOwnerIq &&) -> Result<> {
                       auto itr2 = m_manager->d->rooms.find(m_jid);
                       if (itr2 != m_manager->d->rooms.end() && wasCreating) {
                           // Unlock the room: transition to Joined state
                           itr2->second.state = MucRoomState::Joined;
                           itr2->second.joined = true;
                           // Fetch room info now that the room is configured
                           m_manager->d->fetchRoomInfo(m_jid);
                       }
                       return Success();
                   });
}

///
/// Cancels room creation and destroys the locked room on the server.
///
/// Only valid in the \c Creating state (i.e. after createRoom() has resolved
/// but before setRoomConfig() has been called). Sending the cancel form
/// instructs the server to destroy the still-locked room.
///
/// \since QXmpp 1.15
///
QXmppTask<Result<>> QXmppMucRoomV2::cancelRoomCreation()
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() ||
        itr->second.state != MucRoomState::Creating) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not in Creating state."_s });
    }

    QXmppDataForm cancelForm;
    cancelForm.setType(QXmppDataForm::Cancel);

    QXmppMucOwnerIq iq;
    iq.setTo(m_jid);
    iq.setType(QXmppIq::Set);
    iq.setForm(std::move(cancelForm));

    return chainIq(m_manager->client()->sendIq(std::move(iq)), m_manager,
                   [this](QXmppMucOwnerIq &&) -> Result<> {
                       m_manager->d->rooms.erase(m_jid);
                       return Success();
                   });
}

///
/// Destroys the MUC room on the server.
///
/// Only valid in the \c Joined state. Sends a destroy IQ to the MUC service.
/// An optional \a reason and an \a alternateJid can be provided; the alternate
/// JID is sent to occupants so they may join an alternative room.
///
/// \since QXmpp 1.15
///
QXmppTask<Result<>> QXmppMucRoomV2::destroyRoom(const QString &reason, const QString &alternateJid)
{
    auto itr = m_manager->d->rooms.find(m_jid);
    if (itr == m_manager->d->rooms.end() ||
        itr->second.state != MucRoomState::Joined) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    QXmppMucOwnerIq iq;
    iq.setTo(m_jid);
    iq.setType(QXmppIq::Set);
    iq.setDestroyJid(alternateJid);
    iq.setDestroyReason(reason);

    return chainIq(m_manager->client()->sendIq(std::move(iq)), m_manager,
                   [this](QXmppMucOwnerIq &&) -> Result<> {
                       m_manager->d->rooms.erase(m_jid);
                       return Success();
                   });
}

bool QXmppMucParticipant::isValid() const
{
    return m_manager->participantData(m_roomJid, m_participantId) != nullptr;
}

QBindable<QString> QXmppMucParticipant::nickname() const
{
    if (auto *data = m_manager->participantData(m_roomJid, m_participantId)) {
        return { &(data->nickname) };
    }
    return {};
}

QBindable<QString> QXmppMucParticipant::jid() const
{
    if (auto *data = m_manager->participantData(m_roomJid, m_participantId)) {
        return { &(data->jid) };
    }
    return {};
}

QBindable<Muc::Role> QXmppMucParticipant::role() const
{
    if (auto *data = m_manager->participantData(m_roomJid, m_participantId)) {
        return { &(data->role) };
    }
    return {};
}

QBindable<Muc::Affiliation> QXmppMucParticipant::affiliation() const
{
    if (auto *data = m_manager->participantData(m_roomJid, m_participantId)) {
        return { &(data->affiliation) };
    }
    return {};
}

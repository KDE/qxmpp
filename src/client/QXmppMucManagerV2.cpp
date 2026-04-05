// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucManagerV2.h"

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMucForms.h"
#include "QXmppPubSubManager.h"
#include "QXmppStanza.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"
#include <QXmppUtils.h>

#include "Global.h"
#include "IqSending.h"
#include "Ping.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <unordered_map>

#include <QProperty>
#include <QTimer>
#include <QUuid>

using namespace QXmpp;
using namespace QXmpp::Private;

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
    if (codes.contains(322)) {
        return Muc::LeaveReason::MembersOnly;
    }
    if (codes.contains(332)) {
        return Muc::LeaveReason::ServiceShutdown;
    }
    if (codes.contains(333)) {
        return Muc::LeaveReason::Error;
    }
    return Muc::LeaveReason::Left;
}

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
            return presence.value().mucParticipantItem().jid();
        });
        role.setBinding([&] {
            return presence.value().mucParticipantItem().role().value_or(Muc::Role::None);
        });
        affiliation.setBinding([&] {
            return presence.value().mucParticipantItem().affiliation().value_or(Muc::Affiliation::None);
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
    QXmppMucManagerV2 *manager = nullptr;
    QString roomJid;
    MucRoomState state = MucRoomState::NotJoined;
    QProperty<QString> subject;
    QProperty<QString> nickname;
    QProperty<bool> joined = QProperty<bool> { false };
    QProperty<std::shared_ptr<MucParticipantData>> selfParticipant;
    std::unordered_map<QString, std::shared_ptr<MucParticipantData>> participants;
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> joinPromise;
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> createPromise;
    QList<QXmppMessage> historyMessages;
    std::unique_ptr<QTimer, DeleteLaterDeleter> joinTimer;
    std::unordered_map<QString, PendingMessage> pendingMessages;
    std::optional<QXmppPromise<Result<>>> nickChangePromise;
    std::unique_ptr<QTimer, DeleteLaterDeleter> nickChangeTimer;
    std::optional<QXmppPromise<Result<>>> leavePromise;
    std::unique_ptr<QTimer, DeleteLaterDeleter> leaveTimer;
    // XEP-0410: MUC Self-Ping
    std::chrono::steady_clock::time_point lastActivity;
    bool selfPingInFlight = false;
    int selfPingRetryCount = 0;
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
    bool fetchingRoomConfig = false;
    std::vector<QXmppPromise<Result<QXmppMucRoomConfig>>> roomConfigWaiters;
    // Avatar — populated when setWatchAvatar(true) is called, re-fetched on status 104
    QProperty<QStringList> avatarHashes;
    QProperty<std::optional<QXmpp::Muc::Avatar>> avatar;
    bool supportsOccupantIds = false;
    bool supportsVCard = false;
    bool watchingAvatar = false;
    bool fetchingAvatar = false;
    bool avatarOutdated = true;
    // Convenience bindings derived from roomInfo
    QProperty<bool> subjectChangeable;
    QProperty<QString> description;
    QProperty<QString> language;
    QProperty<QStringList> contactJids;
    // Permission properties — bindings installed once in the ctor, reactive to selfParticipant changes.
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
        avatarHashes.setBinding([this]() -> QStringList {
            const auto &info = roomInfo.value();
            return info ? info->avatarHashes() : QStringList {};
        });

        auto selfRole = [this]() { auto sp = selfParticipant.value(); return sp ? sp->role.value() : Muc::Role::None; };
        auto selfAffil = [this]() { auto sp = selfParticipant.value(); return sp ? sp->affiliation.value() : Muc::Affiliation::None; };

        canSendMessages.setBinding([selfRole]() {
            auto r = selfRole();
            return r == Muc::Role::Participant || r == Muc::Role::Moderator;
        });
        canChangeSubject.setBinding([this, selfRole]() {
            auto r = selfRole();
            return r == Muc::Role::Moderator ||
                (r == Muc::Role::Participant && subjectChangeable.value());
        });
        canSetRoles.setBinding([selfRole]() {
            return selfRole() == Muc::Role::Moderator;
        });
        canSetAffiliations.setBinding([selfAffil]() {
            auto a = selfAffil();
            return a == Muc::Affiliation::Admin || a == Muc::Affiliation::Owner;
        });
        canConfigureRoom.setBinding([selfAffil]() {
            return selfAffil() == Muc::Affiliation::Owner;
        });
    }

    // Reset per-session state on leave. The participants map is intentionally kept
    // as a frozen snapshot for caller handles; selfParticipant (the "who am I"
    // pointer) is reset to null so permission bindings evaluate to false.
    void resetSessionState()
    {
        state = MucRoomState::NotJoined;
        joined = false;
        selfParticipant = nullptr;
        historyMessages.clear();
        joinPromise.reset();
        createPromise.reset();
        leavePromise.reset();
        nickChangePromise.reset();
        joinTimer.reset();
        leaveTimer.reset();
        nickChangeTimer.reset();
        pendingMessages.clear();
        selfPingInFlight = false;
        selfPingRetryCount = 0;
        lastActivity = {};
    }
};

}  // namespace QXmpp::Private

#include "QXmppMucManagerV2_p.h"

///
/// \class QXmppMucManagerV2
///
/// \brief \xep{0045, Multi-User Chat} manager.
///
/// \section setup Setup
///
/// QXmppMucManagerV2 requires QXmppDiscoManager to be registered with the client:
///
/// \code
/// auto *muc   = new QXmppMucManagerV2;
/// auto *disco  = new QXmppDiscoManager;
/// client.addExtension(muc);
/// client.addExtension(disco);
/// \endcode
///
/// For bookmark management, see QXmppPepBookmarkManager.
///
/// \section joining Joining a room
///
/// Call joinRoom() to join a room. The returned task resolves once all initial occupant presences
/// have been received, so the participant list is already populated when the task finishes.
///
/// \code
/// muc->joinRoom(u"room@conference.example.org"_s, u"alice"_s).then(this, [](auto result) {
///     if (auto *room = std::get_if<QXmppMucRoomV2>(&result)) {
///         qDebug() << "Joined as" << room->nickname().value();
///     }
/// });
/// \endcode
///
/// After joining, retrieve the room handle at any time via findRoom():
///
/// \code
/// if (auto r = muc->findRoom(u"room@conference.example.org"_s)) {
///     /* use *r */
/// }
/// \endcode
///
/// \section sending Sending messages
///
/// \code
/// auto r = muc->findRoom(u"room@conference.example.org"_s).value();
/// QXmppMessage msg;
/// msg.setBody(u"Hello, room!"_s);
/// r.sendMessage(std::move(msg)).then(this, [](auto result) { /* ... */ });
/// \endcode
///
/// \section creating Creating a room
///
/// createRoom() creates a new reserved (locked) room. The task resolves once the server confirms
/// room creation and the configuration form has been fetched. Configure the room via
/// setRoomConfig() to unlock it, or cancel with cancelRoomCreation().
///
/// \code
/// muc->createRoom(u"conference.example.org"_s, u"alice"_s, u"newroom"_s).then(this, [](auto result) {
///     if (auto *room = std::get_if<QXmppMucRoomV2>(&result)) {
///         auto config = room->roomConfig().value().value_or(QXmppMucRoomConfig{});
///         config.setName(u"My New Room"_s);
///         room->setRoomConfig(config);
///     }
/// });
/// \endcode
///
/// \section moderation Moderation and affiliation management
///
/// Use setRole() to change a participant's role (e.g. kick or grant/revoke voice) and
/// setAffiliation() to change a user's persistent affiliation (e.g. ban or grant membership).
/// Use requestAffiliationList() to retrieve the full list of users with a given affiliation.
///
/// \code
/// // Kick a participant
/// r.setRole(participant, QXmpp::Muc::Role::None, u"Misbehaving"_s);
///
/// // Ban a user
/// r.setAffiliation(u"user@example.org"_s, QXmpp::Muc::Affiliation::Outcast);
///
/// // Fetch the member list
/// r.requestAffiliationList(QXmpp::Muc::Affiliation::Member).then(this, [](auto result) {
///     if (auto *list = std::get_if<QList<QXmpp::Muc::Item>>(&result)) {
///         for (const auto &item : *list) {
///             qDebug() << item.jid() << item.affiliation();
///         }
///     }
/// });
/// \endcode
///
/// \section config Room configuration
///
/// Call requestRoomConfig() to retrieve a typed configuration form. Edit the returned
/// QXmppMucRoomConfig and submit it with setRoomConfig(). Pass \c watch = \c true (or call
/// setWatchRoomConfig()) to be notified of configuration changes via the roomConfig() bindable.
///
/// \code
/// r.requestRoomConfig(true).then(this, [r](auto result) mutable {
///     if (auto *config = std::get_if<QXmppMucRoomConfig>(&result)) {
///         config->setMaxUsers(50);
///         r.setRoomConfig(*config);
///     }
/// });
/// \endcode
///
/// \section participants Participants and permissions
///
/// participants() returns lightweight handles to all current occupants. selfParticipant() returns
/// your own participant entry, which exposes your current role and affiliation as QBindables.
///
/// The capability QBindables (canSendMessages(), canSetRoles(), canConfigureRoom(), …) update
/// automatically whenever the MUC service changes your permissions.
///
/// \ingroup Managers
/// \since QXmpp 1.13
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
    return { ns_muc.toString() };
}

///
/// Returns the JIDs of discovered MUC services on the server.
///
/// The list is populated automatically after connecting and updated reactively.
///
/// \since QXmpp 1.16
///
QBindable<QStringList> QXmppMucManagerV2::mucServices() const
{
    return &d->mucServices;
}

///
/// Returns whether MUC service discovery has completed.
///
/// \since QXmpp 1.16
///
QBindable<bool> QXmppMucManagerV2::mucServicesLoaded() const
{
    return &d->mucServicesLoaded;
}

///
/// Returns the current XEP-0410 self-ping silence threshold.
///
/// After a room has been silent for this duration, the manager sends a self-ping to verify
/// that the client is still joined. A zero value disables self-pinging entirely.
///
/// \since QXmpp 1.16
///
std::chrono::milliseconds QXmppMucManagerV2::selfPingSilenceThreshold() const
{
    return d->selfPingSilenceThreshold;
}

///
/// Sets the XEP-0410 self-ping silence threshold.
///
/// After \a threshold of silence on a joined room, the manager sends an XEP-0199 ping to
/// the user's own occupant JID and interprets the response to detect silent server-side
/// eviction ("ghosting"). Defaults to 5 minutes. Pass a zero duration to disable
/// self-pinging entirely.
///
/// Ghosted rooms are surfaced via removedFromRoom() with \c LeaveReason::ConnectionLost.
///
/// \since QXmpp 1.16
///
void QXmppMucManagerV2::setSelfPingSilenceThreshold(std::chrono::milliseconds threshold)
{
    d->selfPingSilenceThreshold = threshold;
    d->rescheduleSelfPing();
}

///
/// Requests a unique room localpart from the MUC \a serviceJid (XEP-0307).
///
/// Returns the full room JID (\c localpart@serviceJid) which can be passed to createRoom().
/// Fails if the service does not support XEP-0307 or returns an empty name. Use createRoom()
/// without an explicit \c roomName if you want an automatic client-side fallback.
///
/// \since QXmpp 1.16
///
QXmppTask<Result<QString>> QXmppMucManagerV2::requestUniqueRoomName(QString serviceJid)
{
    auto result = co_await get<MucUniqueQuery>(client(), serviceJid, MucUniqueQuery {}).withContext(this);
    if (auto *q = std::get_if<MucUniqueQuery>(&result)) {
        if (q->name.isEmpty()) {
            co_return QXmppError { u"MUC service did not return a unique room name."_s };
        }
        co_return q->name + u'@' + serviceJid;
    }
    co_return std::get<QXmppError>(std::move(result));
}

///
///
/// Joins the MUC room at \a jid with the given \a nickname.
///
/// Sends an initial presence to the room and waits for all occupant presences that the MUC service
/// sends back. The returned task resolves once the server sends the self-presence with status code
/// 110, meaning the participant list is already fully populated when the task finishes.
///
/// If a join for the same room is already in progress the task fails immediately. If the room is
/// already joined the existing room handle is returned as a success.
///
/// \param jid Bare JID of the room (e.g. \c room\@conference.example.org).
/// \param nickname Desired nickname. Must not be empty.
///
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
    if (auto itr = d->activeRooms.find(jid); itr != d->activeRooms.end()) {
        if (itr->second->state == MucRoomState::Joined) {
            return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppMucRoomV2(itr->second));
        }
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Room join already in progress."_s });
    }

    // Create or revive the room (cached server state carries over a leave/rejoin cycle).
    auto roomData = d->createOrReviveRoom(jid);
    roomData->state = MucRoomState::JoiningOccupantPresences;
    roomData->nickname = nickname;

    // Fetch room features in parallel; updates roominfo properties when it arrives.
    d->fetchRoomInfo(jid);

    QXmppPromise<Result<QXmppMucRoomV2>> promise;
    auto task = promise.task();
    roomData->joinPromise = std::move(promise);

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
    roomData->joinTimer.reset(timer);
    timer->start(d->timeout);

    return task;
}

///
/// Creates a new reserved (locked) MUC room on \a serviceJid with the given \a nickname.
///
/// If \a roomName is set, the room is created at \c roomName@serviceJid. If \a roomName is
/// unset, createRoom() asks the service for a unique localpart via XEP-0307 (muc\#unique);
/// if that query fails for any reason (service does not support XEP-0307, IQ error, …), it
/// falls back to a client-generated UUID localpart so creation still succeeds against any
/// compliant MUC service.
///
/// The room is created in a locked state; no other users can join until the owner submits
/// the configuration form via QXmppMucRoomV2::setRoomConfig(). The task resolves once the
/// server has confirmed room creation (XEP-0045 status code 201) and the configuration form
/// has been fetched. If the local state machine detects that the room is already tracked,
/// the task fails immediately.
///
/// \since QXmpp 1.15
///
QXmppTask<Result<QXmppMucRoomV2>> QXmppMucManagerV2::createRoom(QString serviceJid,
                                                                QString nickname,
                                                                std::optional<QString> roomName)
{
    QString roomJid;
    if (roomName) {
        roomJid = *roomName + u'@' + serviceJid;
    } else {
        auto unique = co_await requestUniqueRoomName(serviceJid).withContext(this);
        if (auto *jid = std::get_if<QString>(&unique)) {
            roomJid = *jid;
        } else {
            // XEP-0307 not supported / IQ failed — fall back to a locally generated name.
            roomJid = QUuid::createUuid().toString(QUuid::WithoutBraces) + u'@' + serviceJid;
        }
    }
    co_return co_await createRoomAtJid(roomJid, nickname).withContext(this);
}

QXmppTask<Result<QXmppMucRoomV2>> QXmppMucManagerV2::createRoomAtJid(const QString &jid, const QString &nickname)
{
    if (nickname.isEmpty()) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Nickname must not be empty."_s });
    }
    if (d->activeRooms.count(jid)) {
        return makeReadyTask<Result<QXmppMucRoomV2>>(QXmppError { u"Room is already tracked (join or create already in progress)."_s });
    }

    auto roomData = d->createOrReviveRoom(jid);
    roomData->state = MucRoomState::JoiningOccupantPresences;
    roomData->nickname = nickname;

    QXmppPromise<Result<QXmppMucRoomV2>> promise;
    auto task = promise.task();
    roomData->createPromise = std::move(promise);

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
    roomData->joinTimer.reset(timer);
    timer->start(d->timeout);

    return task;
}

bool QXmppMucManagerV2::handleMessage(const QXmppMessage &uncheckedMessage)
{
    const auto type = uncheckedMessage.type();
    if (type != QXmppMessage::GroupChat && type != QXmppMessage::Error && type != QXmppMessage::Normal) {
        return false;
    }

    auto bareFrom = QXmppUtils::jidToBareJid(uncheckedMessage.from());

    // Handle invitations from users we haven't joined a room with.
    if (type == QXmppMessage::Normal) {
        // XEP-0249: Direct MUC Invitations
        if (auto inv = uncheckedMessage.mucDirectInvitation(); inv && !inv->jid().isEmpty()) {
            Q_EMIT directInvitationReceived(*inv, uncheckedMessage.from(), uncheckedMessage);
            return true;
        }
        // XEP-0045 §7.8.2: Mediated invitations
        if (const auto &ue = uncheckedMessage.mucUserQuery(); ue && ue->invite() && !ue->invite()->from().isEmpty()) {
            Q_EMIT invitationReceived(bareFrom, *ue->invite(), ue->password());
            return true;
        }
    }

    auto itr = d->activeRooms.find(bareFrom);
    if (itr == d->activeRooms.end()) {
        return false;
    }

    auto &data = *itr->second;

    // Strip occupant ID if the room does not support XEP-0421 to prevent occupant ID injection.
    auto message = [&]() {
        if (!data.supportsOccupantIds && !uncheckedMessage.mucOccupantId().isEmpty()) {
            auto msg = uncheckedMessage;
            msg.setMucOccupantId({});
            return msg;
        }
        return uncheckedMessage;
    }();

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

    // Handle Normal-type messages from the room (e.g., voice request approval forms, declines)
    if (message.type() == QXmppMessage::Normal) {
        if (data.state == MucRoomState::Joined) {
            if (auto voiceRequest = message.mucVoiceRequest()) {
                Q_EMIT voiceRequestReceived(bareFrom, *voiceRequest);
                return true;
            }
            if (const auto &ue = message.mucUserQuery(); ue && ue->decline() && !ue->decline()->from().isEmpty()) {
                Q_EMIT invitationDeclined(bareFrom, *ue->decline());
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
            // XEP-0410: start self-ping silence tracking.
            data.lastActivity = std::chrono::steady_clock::now();
            d->rescheduleSelfPing();

            auto history = std::move(data.historyMessages);
            auto promise = std::move(*data.joinPromise);
            data.joinPromise.reset();

            if (!history.isEmpty()) {
                Q_EMIT roomHistoryReceived(bareFrom, history);
            }
            promise.finish(QXmppMucRoomV2(d->activeRooms[bareFrom]));
        }
        return true;
    case MucRoomState::Joined:
        // XEP-0410: any incoming stanza on a joined room counts as activity.
        data.lastActivity = std::chrono::steady_clock::now();
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
        if (message.mucUserQuery() && message.mucUserQuery()->statusCodes().contains(104)) {
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

///
/// Declines a mediated invitation by sending a \c decline message to \a roomJid.
///
/// Can be called before joining the room. Set \c decline.to() to the inviter's JID.
///
/// \since QXmpp 1.15
///
QXmppTask<SendResult> QXmppMucManagerV2::declineInvitation(const QString &roomJid, QXmpp::Muc::Decline decline)
{
    QXmppMessage message;
    message.setTo(roomJid);
    message.setType(QXmppMessage::Normal);
    QXmpp::Muc::UserQuery ue;
    ue.setDecline(std::move(decline));
    message.setMucUserQuery(std::move(ue));
    return client()->send(std::move(message));
}

///
/// Sends a direct invitation (XEP-0249) to \a to.
///
/// Direct invitations are sent peer-to-peer and are not routed through the room.
/// The optional \a message parameter can be used to set custom extensions on the
/// message stanza; the \c to, \c type, and invitation fields will be overwritten.
///
/// \since QXmpp 1.15
///
QXmppTask<SendResult> QXmppMucManagerV2::sendDirectInvitation(const QString &to, Muc::DirectInvitation invitation, QXmppMessage message)
{
    message.setTo(to);
    message.setType(QXmppMessage::Normal);
    message.setMucDirectInvitation(std::move(invitation));
    return client()->send(std::move(message));
}

void QXmppMucManagerV2::onRegistered(QXmppClient *client)
{
    auto *disco = client->findExtension<QXmppDiscoveryManager>();
    QX_ALWAYS_ASSERT(disco);

    d->servicesWatch = disco->discoverServices(
        QXmpp::Disco::Category::Conference,
        QXmpp::Disco::Type::Text,
        { ns_muc.toString() });

    d->mucServices.setBinding([this]() -> QStringList {
        return transform<QStringList>(d->servicesWatch->services().value(), &QXmppDiscoService::jid);
    });
    d->mucServicesLoaded.setBinding([this]() {
        return d->servicesWatch->loaded().value();
    });

    connect(client, &QXmppClient::connected, this, &QXmppMucManagerV2::onConnected);
    connect(client, &QXmppClient::disconnected, this, &QXmppMucManagerV2::onDisconnected);
    connect(client, &QXmppClient::presenceReceived, this, [this](const auto &p) { d->handlePresence(p); });
}

void QXmppMucManagerV2::onUnregistered(QXmppClient *client)
{
    d->mucServices = QStringList();
    d->mucServicesLoaded = false;
    d->servicesWatch = {};
    disconnect(client, nullptr, this, nullptr);
}

void QXmppMucManagerV2::onConnected()
{
    if (client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->deactivateAllRooms();
    }
}

void QXmppMucManagerV2::onDisconnected()
{
    if (client()->streamManagementState() == QXmppClient::NoStreamManagement) {
        d->deactivateAllRooms();
    }
}

std::optional<QXmppMucRoomV2> QXmppMucManagerV2::findRoom(const QString &jid)
{
    if (auto data = d->lookupRoom(jid)) {
        return QXmppMucRoomV2(std::move(data));
    }
    return std::nullopt;
}

QXmppMucManagerV2Private::QXmppMucManagerV2Private(QXmppMucManagerV2 *manager)
    : q(manager)
{
    selfPingTimer.setSingleShot(true);
    QObject::connect(&selfPingTimer, &QTimer::timeout, q, [this]() { onSelfPingTick(); });
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

//
// Manager private: MUC Core
//

void QXmppMucManagerV2Private::handlePresence(const QXmppPresence &p)
{
    auto bareFrom = QXmppUtils::jidToBareJid(p.from());
    if (auto itr = activeRooms.find(bareFrom); itr != activeRooms.end()) {
        handleRoomPresence(bareFrom, *itr->second, p);
    }
}

void QXmppMucManagerV2Private::handleRoomPresence(const QString &roomJid, QXmpp::Private::MucRoomData &data, QXmppPresence presence)
{
    using enum MucRoomState;

    auto nickname = QXmppUtils::jidToResource(presence.from());

    // Strip occupant ID if the room does not support XEP-0421 to prevent occupant ID injection.
    if (!data.supportsOccupantIds) {
        presence.setMucOccupantId({});
    }

    // XEP-0410: any presence on a joined room counts as activity.
    if (data.state == Joined) {
        data.lastActivity = std::chrono::steady_clock::now();
    }

    switch (data.state) {
    case NotJoined:
        // did not request to join; ignore
        break;
    case JoiningOccupantPresences:
        if (presence.type() == QXmppPresence::Available) {
            for (const auto &[pId, participant] : data.participants) {
                if (participant->nickname.value() == nickname) {
                    // room sent two presences for the same nickname
                    throwRoomError(roomJid, QXmppError { u"MUC reported two presences for the same nickname"_s });
                    return;
                } else if (!participant->occupantId.isEmpty() && participant->occupantId == presence.mucOccupantId()) {
                    // sent two presences with the same occupant ID
                    throwRoomError(roomJid, QXmppError { u"MUC reported two presences for the same occupant ID"_s });
                    return;
                }
            }

            // store new presence (keyed by nickname, unique per XEP-0045)
            auto newParticipant = std::make_shared<MucParticipantData>(presence);
            auto [itr, inserted] = data.participants.emplace(nickname, newParticipant);
            QX_ALWAYS_ASSERT(inserted);

            // this is our presence (must be last)
            if (presence.mucStatusCodes().contains(110)) {
                data.selfParticipant = newParticipant;
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
                        QXmppDataForm cancelForm;
                        cancelForm.setType(QXmppDataForm::Cancel);
                        set(q->client(), roomJid, MucOwnerQuery { .form = std::move(cancelForm) });
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
            auto newNick = presence.mucParticipantItem().nick();
            auto isSelf = presence.mucStatusCodes().contains(110);

            if (isSelf && !newNick.isEmpty()) {
                data.nickname = newNick;
                if (data.nickChangePromise) {
                    auto promise = std::move(*data.nickChangePromise);
                    data.nickChangePromise.reset();
                    data.nickChangeTimer.reset();
                    // XEP-0410 §4.1: nick change complete — the room can be self-pinged again.
                    rescheduleSelfPing();
                    promise.finish(Success());
                }
            }

            // Re-key the participant under the new nickname.
            if (!newNick.isEmpty()) {
                if (auto node = data.participants.extract(nickname); !node.empty()) {
                    auto updated = presence;
                    updated.setFrom(roomJid + u'/' + newNick);
                    node.mapped()->setPresence(updated);
                    node.key() = newNick;
                    data.participants.insert(std::move(node));
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

            deactivateRoom(roomJid);
            rescheduleSelfPing();

            if (promise) {
                promise->finish(Success());
            }
        } else if (presence.type() == QXmppPresence::Unavailable && !presence.mucStatusCodes().contains(110)) {
            // Another participant left the room
            auto reason = leaveReasonFromPresence(presence);
            if (auto pItr = data.participants.find(nickname); pItr != data.participants.end()) {
                Q_EMIT q->participantLeft(roomJid, QXmppMucParticipant(pItr->second), reason);
                data.participants.erase(pItr);
            }
        } else if (presence.type() == QXmppPresence::Available) {
            if (auto pItr = data.participants.find(nickname); pItr != data.participants.end()) {
                // Existing participant — update presence
                pItr->second->setPresence(presence);
            } else {
                // New participant joined
                auto newParticipant = std::make_shared<MucParticipantData>(presence);
                data.participants.emplace(nickname, newParticipant);
                Q_EMIT q->participantJoined(roomJid, QXmppMucParticipant(newParticipant));
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
                data.nickChangeTimer.reset();
                // XEP-0410 §4.1: nick change ended (with error) — room can be self-pinged again.
                rescheduleSelfPing();
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

//
// XEP-0410: MUC Self-Ping
//

void QXmppMucManagerV2Private::rescheduleSelfPing()
{
    using namespace std::chrono;
    if (selfPingSilenceThreshold == milliseconds::zero()) {
        selfPingTimer.stop();
        return;
    }
    auto now = steady_clock::now();
    auto earliest = steady_clock::time_point::max();
    for (const auto &[jid, data] : activeRooms) {
        if (data->state != MucRoomState::Joined) {
            continue;
        }
        if (data->selfPingInFlight || data->nickChangePromise) {
            continue;
        }
        auto deadline = data->lastActivity + selfPingSilenceThreshold;
        if (deadline < earliest) {
            earliest = deadline;
        }
    }
    if (earliest == steady_clock::time_point::max()) {
        selfPingTimer.stop();
        return;
    }
    auto delta = duration_cast<milliseconds>(earliest - now);
    if (delta < milliseconds::zero()) {
        delta = milliseconds::zero();
    }
    selfPingTimer.start(delta);
}

void QXmppMucManagerV2Private::onSelfPingTick()
{
    using namespace std::chrono;
    if (selfPingSilenceThreshold == milliseconds::zero()) {
        return;
    }
    auto now = steady_clock::now();
    // Collect room JIDs first — runSelfPing() may mutate rooms via async callbacks,
    // but for the synchronous iteration we only trigger the send, not the response path.
    std::vector<QString> due;
    for (const auto &[jid, data] : activeRooms) {
        if (data->state != MucRoomState::Joined) {
            continue;
        }
        if (data->selfPingInFlight || data->nickChangePromise) {
            continue;
        }
        if (now - data->lastActivity >= selfPingSilenceThreshold) {
            due.push_back(jid);
        }
    }
    for (const auto &jid : due) {
        runSelfPing(jid);
    }
    rescheduleSelfPing();
}

void QXmppMucManagerV2Private::runSelfPing(const QString &roomJid)
{
    enum class SelfPingOutcome {
        Joined,
        NotJoined,
        Inconclusive,
    };

    auto it = activeRooms.find(roomJid);
    if (it == activeRooms.end() || it->second->state != MucRoomState::Joined) {
        return;
    }
    auto &data = *it->second;
    if (data.selfPingInFlight || data.nickChangePromise) {
        return;
    }
    data.selfPingInFlight = true;
    const QString occupantJid = roomJid + u'/' + data.nickname.value();

    getEmpty(q->client(), occupantJid, Ping {}).then(q, [this, roomJid](Result<> &&result) {
        auto it = activeRooms.find(roomJid);
        if (it == activeRooms.end()) {
            return;
        }
        auto &data = *it->second;
        data.selfPingInFlight = false;

        using Cond = QXmppStanza::Error::Condition;
        auto outcome = SelfPingOutcome::Inconclusive;
        if (std::holds_alternative<Success>(result)) {
            outcome = SelfPingOutcome::Joined;
        } else {
            auto &err = std::get<QXmppError>(result);
            if (auto stanzaErr = err.value<QXmppStanza::Error>()) {
                switch (stanzaErr->condition()) {
                case Cond::ServiceUnavailable:
                case Cond::FeatureNotImplemented:
                case Cond::ItemNotFound:
                    outcome = SelfPingOutcome::Joined;
                    break;
                case Cond::NotAcceptable:
                case Cond::NotAllowed:
                case Cond::BadRequest:
                    outcome = SelfPingOutcome::NotJoined;
                    break;
                case Cond::RemoteServerNotFound:
                case Cond::RemoteServerTimeout:
                    outcome = SelfPingOutcome::Inconclusive;
                    break;
                default:
                    outcome = SelfPingOutcome::NotJoined;
                    break;
                }
            }
            // else: transport-level error / task timeout → inconclusive (already default)
        }

        using namespace std::chrono;
        switch (outcome) {
        case SelfPingOutcome::Joined:
            data.selfPingRetryCount = 0;
            data.lastActivity = steady_clock::now();
            rescheduleSelfPing();
            break;
        case SelfPingOutcome::Inconclusive:
            if (++data.selfPingRetryCount < MucSelfPingMaxRetries) {
                // Backoff: 30s, 1m, 2m, 4m. Encode as an earlier lastActivity so
                // the scheduler naturally fires at the right time — keeps the
                // single-timer design free of retry-specific paths.
                auto backoff = seconds(30) * (1u << (data.selfPingRetryCount - 1));
                data.lastActivity = steady_clock::now() - selfPingSilenceThreshold + backoff;
            } else {
                data.selfPingRetryCount = 0;
                data.lastActivity = steady_clock::now();
            }
            rescheduleSelfPing();
            break;
        case SelfPingOutcome::NotJoined:
            // Synthesize a forcible removal — reuses the standard removedFromRoom cleanup path.
            Q_EMIT q->removedFromRoom(roomJid, Muc::LeaveReason::ConnectionLost, std::nullopt);
            deactivateRoom(roomJid);
            rescheduleSelfPing();
            break;
        }
    });
}

bool QXmppMucManagerV2Private::testSelfPingInFlight(const QString &roomJid) const
{
    auto it = activeRooms.find(roomJid);
    return it != activeRooms.end() && it->second->selfPingInFlight;
}

int QXmppMucManagerV2Private::testSelfPingRetryCount(const QString &roomJid) const
{
    auto it = activeRooms.find(roomJid);
    return it != activeRooms.end() ? it->second->selfPingRetryCount : 0;
}

bool QXmppMucManagerV2Private::testHasRoom(const QString &roomJid) const
{
    return activeRooms.contains(roomJid);
}

void QXmppMucManagerV2Private::testForceDueForSelfPing(const QString &roomJid)
{
    auto it = activeRooms.find(roomJid);
    if (it != activeRooms.end()) {
        it->second->lastActivity = std::chrono::steady_clock::now() - std::chrono::hours(1);
    }
}

std::chrono::steady_clock::time_point QXmppMucManagerV2Private::testLastActivity(const QString &roomJid) const
{
    auto it = activeRooms.find(roomJid);
    return it != activeRooms.end() ? it->second->lastActivity : std::chrono::steady_clock::time_point {};
}

void QXmppMucManagerV2Private::throwRoomError(const QString &roomJid, QXmppError &&error)
{
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }

    // Move promise out before deactivating so we can finish it cleanly after the
    // session state (including the promise slots) is reset.
    std::optional<QXmppPromise<Result<QXmppMucRoomV2>>> promise;
    if (itr->second->joinPromise) {
        promise = std::move(itr->second->joinPromise);
    } else if (itr->second->createPromise) {
        promise = std::move(itr->second->createPromise);
    }
    deactivateRoom(roomJid);
    rescheduleSelfPing();

    if (promise) {
        promise->finish(std::move(error));
    }
}

//
// Room lifetime management
//
// activeRooms holds strong shared_ptr references while the user is joined or joining.
// On leave/kick/disconnect, rooms are moved to inactiveRooms as weak_ptrs. If any
// caller still holds a QXmppMucRoomV2 handle (which also owns a shared_ptr), the
// MucRoomData lives on as a frozen snapshot. Rejoining via createOrReviveRoom()
// upgrades the weak_ptr back to strong ownership, reusing the cached state.
//

std::shared_ptr<MucRoomData> QXmppMucManagerV2Private::lookupRoom(const QString &jid)
{
    if (auto itr = activeRooms.find(jid); itr != activeRooms.end()) {
        return itr->second;
    }
    if (auto itr = inactiveRooms.find(jid); itr != inactiveRooms.end()) {
        if (auto locked = itr->second.lock()) {
            return locked;
        }
        inactiveRooms.erase(itr);  // expired — prune inline
    }
    return nullptr;
}

std::shared_ptr<MucRoomData> QXmppMucManagerV2Private::createOrReviveRoom(const QString &jid)
{
    if (auto itr = activeRooms.find(jid); itr != activeRooms.end()) {
        return itr->second;  // caller is responsible for detecting duplicate join
    }
    // Try to revive from inactive so cached state (roomInfo, roomConfig, avatar, feature
    // flags, last known subject/nickname) carries over a leave/rejoin cycle.
    if (auto itr = inactiveRooms.find(jid); itr != inactiveRooms.end()) {
        if (auto locked = itr->second.lock()) {
            inactiveRooms.erase(itr);
            // Clear stale participants from the previous session. Permission bindings
            // react to selfParticipant being null — no manual unwiring needed.
            locked->participants.clear();
            locked->selfParticipant = nullptr;
            activeRooms[jid] = locked;
            return locked;
        }
        inactiveRooms.erase(itr);
    }
    auto data = std::make_shared<MucRoomData>();
    data->manager = q;
    data->roomJid = jid;
    activeRooms[jid] = data;
    return data;
}

void QXmppMucManagerV2Private::deactivateRoom(const QString &jid)
{
    auto itr = activeRooms.find(jid);
    if (itr == activeRooms.end()) {
        return;  // already inactive (or never tracked) — idempotent
    }
    auto data = std::move(itr->second);
    activeRooms.erase(itr);

    // Reset per-session state (promises, timers, state flags). Participants and
    // permission bindings are intentionally preserved — they serve as a frozen
    // snapshot for caller handles and are cleared on the next join instead.
    data->resetSessionState();

    // Register a weak_ptr so subsequent joinRoom()/findRoom() calls can revive the
    // same MucRoomData instance as long as any caller handle is still holding it.
    inactiveRooms[jid] = data;

    // data shared_ptr goes out of scope here. If no caller holds a handle, the data
    // is freed and the weak_ptr expires (pruned lazily on next lookup or threshold sweep).

    if (inactiveRooms.size() > 32) {
        pruneInactiveRooms();
    }
}

void QXmppMucManagerV2Private::deactivateAllRooms()
{
    // Collect pending promises before resetting so their error callbacks observe a
    // clean (empty) room state.
    std::vector<QXmppPromise<Result<QXmppMucRoomV2>>> joinPromises;
    std::vector<QXmppPromise<Result<>>> otherPromises;

    for (auto &[jid, data] : activeRooms) {
        if (data->joinPromise) {
            joinPromises.push_back(std::move(*data->joinPromise));
        }
        if (data->createPromise) {
            joinPromises.push_back(std::move(*data->createPromise));
        }
        if (data->leavePromise) {
            otherPromises.push_back(std::move(*data->leavePromise));
        }
        if (data->nickChangePromise) {
            otherPromises.push_back(std::move(*data->nickChangePromise));
        }
        for (auto &[msgId, pending] : data->pendingMessages) {
            otherPromises.push_back(std::move(pending.promise));
        }
    }

    // Deactivate every room (demote to inactive, reset session state, keep cache).
    // Copy jids first since deactivateRoom() mutates activeRooms.
    std::vector<QString> jids;
    jids.reserve(activeRooms.size());
    for (const auto &[jid, _] : activeRooms) {
        jids.push_back(jid);
    }
    for (const auto &jid : jids) {
        deactivateRoom(jid);
    }

    selfPingTimer.stop();

    const auto error = QXmppError { u"Disconnected from server."_s };
    for (auto &promise : joinPromises) {
        promise.finish(error);
    }
    for (auto &promise : otherPromises) {
        promise.finish(error);
    }
}

void QXmppMucManagerV2Private::pruneInactiveRooms()
{
    for (auto itr = inactiveRooms.begin(); itr != inactiveRooms.end();) {
        if (itr->second.expired()) {
            itr = inactiveRooms.erase(itr);
        } else {
            ++itr;
        }
    }
}

void QXmppMucManagerV2Private::fetchRoomInfo(const QString &roomJid)
{
    auto *disco = q->client()->findExtension<QXmppDiscoveryManager>();
    if (!disco) {
        return;
    }
    disco->info(roomJid, {}, QXmppDiscoveryManager::CachePolicy::Strict).then(q, [this, roomJid](auto &&result) {
        auto itr = activeRooms.find(roomJid);
        if (itr == activeRooms.end() || hasError(result)) {
            return;
        }
        auto &info = getValue(result);
        auto &data = *itr->second;
        const auto oldHashes = data.avatarHashes.value();
        data.roomInfo = info.template dataForm<QXmppMucRoomInfo>();
        const auto &features = info.features();
        data.supportsOccupantIds = features.contains(ns_muc_occupant_id);
        data.supportsVCard = features.contains(ns_vcard);
        data.isNonAnonymous = features.contains(muc_feat_nonanonymous);
        data.isPublic = features.contains(muc_feat_public);
        data.isMembersOnly = features.contains(muc_feat_membersonly);
        data.isModerated = features.contains(muc_feat_moderated);
        data.isPersistent = features.contains(muc_feat_persistent);
        data.isPasswordProtected = features.contains(muc_feat_passwordprotected);
        if (data.avatarHashes.value() != oldHashes) {
            data.avatarOutdated = true;
        }
        if (data.watchingAvatar && !data.fetchingAvatar && data.avatarOutdated) {
            fetchAvatar(roomJid);
        }
    });
}

void QXmppMucManagerV2Private::fetchAvatar(const QString &roomJid)
{
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }
    auto &data = *itr->second;
    const auto hashes = data.avatarHashes.value();
    data.avatarOutdated = false;

    if (!data.supportsVCard || hashes.isEmpty()) {
        data.avatar = std::nullopt;
        return;
    }

    data.fetchingAvatar = true;
    q->client()->sendIq(QXmppVCardIq(roomJid)).then(q, [this, roomJid, hashes](auto &&result) mutable {
        auto itr = activeRooms.find(roomJid);
        if (itr == activeRooms.end()) {
            return;
        }
        auto &data = *itr->second;
        data.fetchingAvatar = false;

        auto iqResponse = parseIq<QXmppVCardIq>(std::move(result));
        if (hasError(iqResponse)) {
            return;
        }

        const auto &vcard = getValue(iqResponse);
        const auto hexHash = QString::fromUtf8(QCryptographicHash::hash(vcard.photo(), QCryptographicHash::Sha1).toHex());
        if (!contains(hashes, hexHash)) {
            return;
        }
        if (vcard.photo().isEmpty()) {
            data.avatar = std::nullopt;
        } else {
            data.avatar = QXmpp::Muc::Avatar { vcard.photoType(), vcard.photo() };
        }
    });
}

QXmppTask<Result<MucOwnerQuery>> QXmppMucManagerV2Private::sendOwnerFormRequest(const QString &roomJid)
{
    return get<MucOwnerQuery>(q->client(), roomJid, MucOwnerQuery {});
}

void QXmppMucManagerV2Private::fetchConfigForm(const QString &roomJid)
{
    sendOwnerFormRequest(roomJid).then(q, [this, roomJid](Result<MucOwnerQuery> &&result) {
        auto itr = activeRooms.find(roomJid);
        if (itr == activeRooms.end() || itr->second->state != MucRoomState::Creating) {
            return;
        }
        auto &data = *itr->second;

        if (auto *error = std::get_if<QXmppError>(&result)) {
            throwRoomError(roomJid, std::move(*error));
            return;
        }

        // Resolve the createPromise — room is locked, owner can now configure it
        auto promise = std::move(*data.createPromise);
        data.createPromise.reset();
        promise.finish(QXmppMucRoomV2(activeRooms[roomJid]));
    });
}

void QXmppMucManagerV2Private::fetchRoomConfigSubscribed(const QString &roomJid)
{
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }
    itr->second->fetchingRoomConfig = true;

    sendOwnerFormRequest(roomJid).then(q, [this, roomJid](Result<MucOwnerQuery> &&result) {
        auto itr = activeRooms.find(roomJid);
        if (itr == activeRooms.end()) {
            return;
        }
        auto &data = *itr->second;
        data.fetchingRoomConfig = false;
        auto waiters = std::move(data.roomConfigWaiters);

        if (auto *error = std::get_if<QXmppError>(&result)) {
            for (auto &p : waiters) {
                p.finish(QXmppError { error->description });
            }
            return;
        }

        auto &oq = std::get<MucOwnerQuery>(result);
        auto config = oq.form ? QXmppMucRoomConfig::fromDataForm(*oq.form) : std::nullopt;
        if (!config) {
            for (auto &p : waiters) {
                p.finish(QXmppError { u"Server returned an invalid or missing muc#roomconfig form."_s });
            }
            return;
        }
        data.roomConfig = *config;
        for (auto &p : waiters) {
            p.finish(*config);
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
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }

    std::optional<QXmppPromise<Result<>>> promise;
    if (itr->second->leavePromise) {
        promise = std::move(itr->second->leavePromise);
    }
    deactivateRoom(roomJid);
    rescheduleSelfPing();

    if (promise) {
        promise->finish(QXmppError { u"Leaving room timed out."_s });
    }
}

void QXmppMucManagerV2Private::handleNickChangeTimeout(const QString &roomJid)
{
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }

    auto &data = *itr->second;
    if (!data.nickChangePromise) {
        return;
    }

    auto promise = std::move(*data.nickChangePromise);
    data.nickChangePromise.reset();
    data.nickChangeTimer.reset();
    // XEP-0410 §4.1: nick change ended (with timeout) — room can be self-pinged again.
    rescheduleSelfPing();
    promise.finish(QXmppError { u"Changing nickname timed out."_s });
}

void QXmppMucManagerV2Private::handleMessageTimeout(const QString &roomJid, const QString &originId)
{
    auto itr = activeRooms.find(roomJid);
    if (itr == activeRooms.end()) {
        return;
    }

    auto pendingItr = itr->second->pendingMessages.find(originId);
    if (pendingItr == itr->second->pendingMessages.end()) {
        return;
    }

    pendingItr->second.promise.finish(QXmppError { u"Sending message timed out."_s });
    itr->second->pendingMessages.erase(pendingItr);
}

//
// MucRoom
//

static bool isRoomJoined(QXmppMucManagerV2Private *d, const QString &jid)
{
    auto itr = d->activeRooms.find(jid);
    return itr != d->activeRooms.end() && itr->second->state == MucRoomState::Joined;
}

/// Returns the room JID this handle refers to.
QString QXmppMucRoomV2::jid() const
{
    return m_data->roomJid;
}

/// Returns the manager that owns this room's state.
QXmppMucManagerV2 *QXmppMucRoomV2::manager() const
{
    return m_data->manager;
}

/// Returns the room subject as a bindable property.
QBindable<QString> QXmppMucRoomV2::subject() const
{
    return &m_data->subject;
}

/// Returns the user's nickname in the room as a bindable property.
QBindable<QString> QXmppMucRoomV2::nickname() const
{
    return &m_data->nickname;
}

/// Returns whether the room is currently joined as a bindable property.
QBindable<bool> QXmppMucRoomV2::joined() const
{
    return &m_data->joined;
}

///
/// Returns a list of all participants currently in the room.
///
/// The returned handles are lightweight and do not own any data.
///
QList<QXmppMucParticipant> QXmppMucRoomV2::participants() const
{
    QList<QXmppMucParticipant> result;
    result.reserve(m_data->participants.size());
    for (const auto &[id, pData] : m_data->participants) {
        result.append(QXmppMucParticipant(pData));
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
    if (auto sp = m_data->selfParticipant.value()) {
        return QXmppMucParticipant(std::move(sp));
    }
    return std::nullopt;
}

/// Returns whether the local user can send groupchat messages.
///
/// True when the user's role is Participant or Moderator.
/// In unmoderated rooms the server assigns Participant by default.
QBindable<bool> QXmppMucRoomV2::canSendMessages() const
{
    return &m_data->canSendMessages;
}

/// Returns whether the local user can change the room subject.
///
/// True for Moderators, or for Participants when the room allows it
/// (\c muc#roominfo_subjectmod). Defaults to false until disco\#info arrives.
QBindable<bool> QXmppMucRoomV2::canChangeSubject() const
{
    return &m_data->canChangeSubject;
}

/// Returns whether the local user can change other participants' roles (XEP-0045 §8.4–8.6).
///
/// True when the user's role is Moderator. This covers role changes including kicking
/// (setting role to None) and granting/revoking voice.
QBindable<bool> QXmppMucRoomV2::canSetRoles() const
{
    return &m_data->canSetRoles;
}

/// Returns whether the local user can change affiliations (XEP-0045 §9).
///
/// True when the user's affiliation is Admin or Owner. This covers banning,
/// granting and revoking membership, and querying affiliation lists.
QBindable<bool> QXmppMucRoomV2::canSetAffiliations() const
{
    return &m_data->canSetAffiliations;
}

/// Returns whether the local user can configure the room (XEP-0045 §10).
///
/// True when the user's affiliation is Owner.
QBindable<bool> QXmppMucRoomV2::canConfigureRoom() const
{
    return &m_data->canConfigureRoom;
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
    return &m_data->isNonAnonymous;
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
    return &m_data->isPublic;
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
    return &m_data->isMembersOnly;
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
    return &m_data->isModerated;
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
    return &m_data->isPersistent;
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
    return &m_data->isPasswordProtected;
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
    return &m_data->roomInfo;
}

///
/// Returns the current room configuration form from \xep{0045, Multi-User Chat}.
///
/// The form is populated after requestRoomConfig() or setWatchRoomConfig(true) has been called
/// and the initial fetch has completed. While watching is active, the form is automatically
/// re-fetched whenever a status code 104 (room configuration changed) is received.
///
/// Returns \c std::nullopt until the first response has arrived.
///
/// \sa requestRoomConfig(), setWatchRoomConfig()
/// \since QXmpp 1.15
///
QBindable<std::optional<QXmppMucRoomConfig>> QXmppMucRoomV2::roomConfig() const
{
    return &m_data->roomConfig;
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
    const bool needsFetch = watch && !m_data->watchingRoomConfig;
    m_data->watchingRoomConfig = watch;
    if (needsFetch) {
        m_data->manager->d->fetchRoomConfigSubscribed(m_data->roomJid);
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
    return m_data->watchingRoomConfig;
}

///
/// Returns the avatar hashes for the room from \xep{0045, Multi-User Chat} \c muc#roominfo.
///
/// The hashes are derived from the \c muc#roominfo_avatarhash field of the \c disco\#info
/// response. They are automatically updated on join and whenever a status code 104
/// (room configuration changed) is received.
///
/// Returns an empty list until the first \c disco\#info response has arrived or if no avatar
/// has been published.
///
/// \sa avatar(), setWatchAvatar()
/// \since QXmpp 1.15
///
QBindable<QStringList> QXmppMucRoomV2::avatarHashes() const
{
    return &m_data->avatarHashes;
}

///
/// Returns the cached room avatar.
///
/// Populated after setWatchAvatar(true) has been called and the vcard-temp fetch has completed.
/// Automatically re-fetched whenever a status code 104 (room configuration changed) causes the
/// avatar hashes to change.
///
/// Returns \c std::nullopt until the first fetch completes, or if the room has no avatar or the
/// MUC service does not support vcard-temp.
///
/// \sa avatarHashes(), setWatchAvatar()
/// \since QXmpp 1.15
///
QBindable<std::optional<QXmpp::Muc::Avatar>> QXmppMucRoomV2::avatar() const
{
    return &m_data->avatar;
}

///
/// Enables or disables automatic avatar updates.
///
/// When set to \c true, the avatar is fetched immediately if the room info is already available
/// and the hashes have not been fetched yet. Whenever a status code 104 (room configuration
/// changed) causes the avatar hashes to change, the avatar is re-fetched automatically.
///
/// When set to \c false, auto-refresh is disabled. The last fetched avatar remains available in
/// avatar() but is no longer updated automatically.
///
/// \sa avatar(), avatarHashes()
/// \since QXmpp 1.15
///
void QXmppMucRoomV2::setWatchAvatar(bool watch)
{
    const bool needsFetch = watch && !m_data->watchingAvatar && m_data->roomInfo.value().has_value() && !m_data->fetchingAvatar && m_data->avatarOutdated;
    m_data->watchingAvatar = watch;
    if (needsFetch) {
        m_data->manager->d->fetchAvatar(m_data->roomJid);
    }
}

///
/// Returns whether automatic avatar updates are enabled.
///
/// \sa setWatchAvatar()
/// \since QXmpp 1.15
///
bool QXmppMucRoomV2::isWatchingAvatar() const
{
    return m_data->watchingAvatar;
}

///
/// Sets or removes the avatar of the room via vcard-temp.
///
/// Pass an \c Avatar to publish a new avatar, or \c std::nullopt to remove the current one.
///
/// Requires the MUC service to support "vcard-temp" and the local user to be an owner or other
/// privileged entity of the room.
///
/// \since QXmpp 1.15
///
QXmppTask<QXmpp::Result<>> QXmppMucRoomV2::setAvatar(std::optional<QXmpp::Muc::Avatar> newAvatar)
{
    QXmppVCardIq vCardIq;
    vCardIq.setTo(m_data->roomJid);
    vCardIq.setFrom({});
    vCardIq.setType(QXmppIq::Set);
    if (newAvatar) {
        vCardIq.setPhotoType(newAvatar->contentType);
        vCardIq.setPhoto(newAvatar->data);
    }
    return m_data->manager->client()->sendGenericIq(std::move(vCardIq));
}

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
    return &m_data->description;
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
    return &m_data->language;
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
    return &m_data->contactJids;
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
    if (m_data->state != MucRoomState::Joined) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    message.setTo(m_data->roomJid);
    message.setType(QXmppMessage::GroupChat);
    if (message.originId().isEmpty()) {
        message.setOriginId(QXmppUtils::generateStanzaUuid());
    }
    auto originId = message.originId();

    QXmppPromise<Result<>> promise;
    auto task = promise.task();

    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, m_data->manager, [d = m_data->manager->d.get(), roomJid = m_data->roomJid, originId]() {
        d->handleMessageTimeout(roomJid, originId);
    });

    m_data->pendingMessages.emplace(originId, PendingMessage { std::move(promise), std::unique_ptr<QTimer, DeleteLaterDeleter>(timer) });
    timer->start(m_data->manager->d->timeout);

    m_data->manager->client()->send(std::move(message));
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
    if (m_data->state != MucRoomState::Joined) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    message.setTo(m_data->roomJid + u'/' + participant.m_data->nickname.value());
    message.setType(QXmppMessage::Chat);

    return m_data->manager->client()->send(std::move(message));
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
    if (m_data->state != MucRoomState::Joined) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    auto &data = *m_data;

    // Cancel any pending nickname change
    if (data.nickChangePromise) {
        auto oldPromise = std::move(*data.nickChangePromise);
        data.nickChangePromise.reset();
        data.nickChangeTimer.reset();
        oldPromise.finish(QXmppError { u"Superseded by a new nickname change request."_s });
    }

    QXmppPromise<Result<>> promise;
    auto task = promise.task();
    data.nickChangePromise = std::move(promise);

    QXmppPresence p;
    p.setTo(m_data->roomJid + u'/' + newNick);
    m_data->manager->client()->send(std::move(p));

    auto *timer = new QTimer();
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, m_data->manager, [d = m_data->manager->d.get(), roomJid = m_data->roomJid]() {
        d->handleNickChangeTimeout(roomJid);
    });
    data.nickChangeTimer.reset(timer);
    timer->start(m_data->manager->d->timeout);

    // XEP-0410 §4.1: suppress self-ping on this room until the nick change completes.
    m_data->manager->d->rescheduleSelfPing();

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
    if (m_data->state != MucRoomState::Joined) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    presence.setTo(m_data->roomJid + u'/' + m_data->nickname.value());
    return m_data->manager->client()->send(std::move(presence));
}

///
/// Leaves the room by sending an unavailable presence (XEP-0045 §7.14).
///
/// The returned task completes when the server confirms the leave with a
/// self-unavailable presence containing status code 110.
///
QXmppTask<Result<>> QXmppMucRoomV2::leave()
{
    auto itr = m_data->manager->d->activeRooms.find(m_data->roomJid);
    if (itr == m_data->manager->d->activeRooms.end()) {
        return makeReadyTask<Result<>>(QXmppError { u"Room is not joined."_s });
    }

    auto &data = *m_data;
    if (data.leavePromise) {
        return makeReadyTask<Result<>>(QXmppError { u"Already leaving the room."_s });
    }

    QXmppPresence p;
    p.setTo(m_data->roomJid + u'/' + data.nickname.value());
    p.setType(QXmppPresence::Unavailable);

    auto sendResult = m_data->manager->client()->send(std::move(p));
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
    QObject::connect(timer, &QTimer::timeout, m_data->manager, [d = m_data->manager->d.get(), roomJid = m_data->roomJid]() {
        d->handleLeaveTimeout(roomJid);
    });
    data.leaveTimer.reset(timer);
    timer->start(m_data->manager->d->timeout);

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
    Muc::Item item;
    item.setNick(participant.m_data->nickname.value());
    item.setRole(role);
    item.setReason(reason);

    return set(m_data->manager->client(), m_data->roomJid, MucAdminQuery { .items = { item } });
}

///
/// Changes the roles of multiple room participants in a single request (XEP-0045 §8.5).
///
/// Each item must have its nick and role set. Role changes are transient
/// and apply only for the current session.
///
QXmppTask<Result<>> QXmppMucRoomV2::setRoles(const QList<Muc::Item> &items)
{
    return set(m_data->manager->client(), m_data->roomJid, MucAdminQuery { .items = items });
}

///
/// Changes the affiliation of a user by bare JID (XEP-0045 §9).
///
/// Affiliation changes are persistent. The \a jid must be a bare JID.
/// The user does not need to be currently in the room.
///
QXmppTask<Result<>> QXmppMucRoomV2::setAffiliation(const QString &jid, QXmpp::Muc::Affiliation affiliation, const QString &reason)
{
    Muc::Item item;
    item.setJid(jid);
    item.setAffiliation(affiliation);
    item.setReason(reason);

    return set(m_data->manager->client(), m_data->roomJid, MucAdminQuery { .items = { item } });
}

///
/// Changes the affiliations of multiple users in a single request (XEP-0045 §9).
///
/// Each item must have its jid and affiliation set. Affiliation changes are
/// persistent and the users do not need to be currently in the room.
///
QXmppTask<Result<>> QXmppMucRoomV2::setAffiliations(const QList<Muc::Item> &items)
{
    return set(m_data->manager->client(), m_data->roomJid, MucAdminQuery { .items = items });
}

///
/// Requests the list of all users with a given \a affiliation (XEP-0045 §9.5–9.8).
///
/// Returns a snapshot of the affiliation list. There are no live updates;
/// call this method again after making changes if you need a fresh list.
///
/// Only admins and owners are allowed to retrieve these lists.
///
QXmppTask<Result<QList<Muc::Item>>> QXmppMucRoomV2::requestAffiliationList(QXmpp::Muc::Affiliation affiliation)
{
    Muc::Item item;
    item.setAffiliation(affiliation);

    auto result = co_await get<MucAdminQuery>(m_data->manager->client(), m_data->roomJid, MucAdminQuery { .items = { item } }).withContext(m_data->manager);
    if (auto *q = std::get_if<MucAdminQuery>(&result)) {
        co_return std::move(q->items);
    }
    co_return std::get<QXmppError>(std::move(result));
}

///
/// Queries the server for the user's reserved nickname in this room (XEP-0045 §7.12).
///
/// Returns the reserved nickname, or an empty string if no nickname is reserved.
///
QXmppTask<Result<QString>> QXmppMucRoomV2::requestReservedNickname()
{
    auto result = co_await m_data->manager->d->disco()->info(m_data->roomJid, u"x-roomuser-item"_s, QXmppDiscoveryManager::CachePolicy::Strict).withContext(m_data->manager);
    if (auto *info = std::get_if<QXmppDiscoInfo>(&result)) {
        for (const auto &identity : info->identities()) {
            if (identity.category() == u"conference") {
                co_return identity.name();
            }
        }
        co_return QString();
    }
    co_return std::get<QXmppError>(std::move(result));
}

///
/// Requests the registration form for this room (XEP-0045 §7.10).
///
/// Returns the data form that needs to be filled in and submitted via submitRegistration().
///
QXmppTask<Result<QXmppDataForm>> QXmppMucRoomV2::requestRegistrationForm()
{
    auto result = co_await get<MucRegisterQuery>(m_data->manager->client(), m_data->roomJid, MucRegisterQuery {}).withContext(m_data->manager);
    if (auto *q = std::get_if<MucRegisterQuery>(&result)) {
        if (q->form) {
            co_return std::move(*q->form);
        }
        co_return QXmppError { u"Server did not return a registration form."_s };
    }
    co_return std::get<QXmppError>(std::move(result));
}

///
/// Submits a filled-in registration form to the room (XEP-0045 §7.10).
///
/// The \a form should be the form returned by requestRegistrationForm(), filled in by the user,
/// with its type set to \c QXmppDataForm::Submit.
///
QXmppTask<Result<>> QXmppMucRoomV2::submitRegistration(const QXmppDataForm &form)
{
    return set(m_data->manager->client(), m_data->roomJid, MucRegisterQuery { .form = form });
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
    if (!isRoomJoined(m_data->manager->d.get(), m_data->roomJid)) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    QXmppMessage message;
    message.setTo(m_data->roomJid);
    message.setType(QXmppMessage::Normal);
    message.setMucVoiceRequest(QXmppMucVoiceRequest {});
    return m_data->manager->client()->send(std::move(message));
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
    if (!isRoomJoined(m_data->manager->d.get(), m_data->roomJid)) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    auto response = request;
    response.setRequestAllow(allow);

    QXmppMessage message;
    message.setTo(m_data->roomJid);
    message.setType(QXmppMessage::Normal);
    message.setMucVoiceRequest(std::move(response));
    return m_data->manager->client()->send(std::move(message));
}

///
/// Sends a mediated invitation to \a invite.to() via this room.
///
/// The room must be currently joined. The room service will forward the invitation to the
/// specified invitee and add the room password automatically for password-protected rooms.
///
/// \since QXmpp 1.15
///
QXmppTask<SendResult> QXmppMucRoomV2::inviteUser(QXmpp::Muc::Invite invite)
{
    if (!isRoomJoined(m_data->manager->d.get(), m_data->roomJid)) {
        return makeReadyTask<SendResult>(QXmppError { u"Room is not joined."_s });
    }

    QXmppMessage message;
    message.setTo(m_data->roomJid);
    message.setType(QXmppMessage::Normal);
    QXmpp::Muc::UserQuery ue;
    ue.setInvite(std::move(invite));
    message.setMucUserQuery(std::move(ue));
    return m_data->manager->client()->send(std::move(message));
}

///
/// Requests the current room configuration form from the server.
///
/// Valid in both the \c Creating and \c Joined states. Returns a
/// QXmppMucRoomConfig that can be edited and submitted via setRoomConfig().
/// Also updates the roomConfig() bindable on success.
///
/// If watching is active (setWatchRoomConfig(true) or \a watch = \c true) and
/// the configuration has already been fetched, the cached value is returned
/// immediately without a network request. When watching is disabled the cached
/// value may be stale, so a fresh fetch is always performed in that case.
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
    if (m_data->state != Creating && m_data->state != Joined) {
        co_return QXmppError { u"Room is not in Creating or Joined state."_s };
    }

    // Capture wasWatching before potentially enabling watch, so we only use the cache
    // when watching was already active (status 104 was keeping it current).
    // If we're just (re-)enabling watch now, the cache may be stale — always re-fetch.
    const bool wasWatching = m_data->watchingRoomConfig;
    if (watch) {
        m_data->watchingRoomConfig = true;
    }

    if (wasWatching) {
        // If a status-104 re-fetch is in progress, join it — its result will be fresher
        // than the current cache. Otherwise return the cache directly.
        if (m_data->fetchingRoomConfig) {
            QXmppPromise<Result<QXmppMucRoomConfig>> p;
            auto task = p.task();
            m_data->roomConfigWaiters.push_back(std::move(p));
            co_return co_await std::move(task);
        }
        if (const auto &cached = m_data->roomConfig.value()) {
            co_return *cached;
        }
    }

    auto result = co_await get<MucOwnerQuery>(m_data->manager->client(), m_data->roomJid, MucOwnerQuery {}).withContext(m_data->manager);
    if (auto *e = std::get_if<QXmppError>(&result)) {
        co_return std::move(*e);
    }
    auto &oq = std::get<MucOwnerQuery>(result);
    auto config = oq.form ? QXmppMucRoomConfig::fromDataForm(*oq.form) : std::nullopt;
    if (!config) {
        co_return QXmppError { u"Server returned an invalid or missing muc#roomconfig form."_s };
    }
    m_data->roomConfig = config;
    co_return std::move(*config);
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
    if (m_data->state != Creating && m_data->state != Joined) {
        co_return QXmppError { u"Room is not in Creating or Joined state."_s };
    }

    auto form = config.toDataForm();
    form.setType(QXmppDataForm::Submit);

    const bool wasCreating = (m_data->state == Creating);
    auto result = co_await set(m_data->manager->client(), m_data->roomJid, MucOwnerQuery { .form = std::move(form) }).withContext(m_data->manager);
    if (std::holds_alternative<Success>(result) && wasCreating) {
        // Unlock the room: transition to Joined state
        m_data->state = MucRoomState::Joined;
        m_data->joined = true;
        // XEP-0410: start self-ping silence tracking.
        m_data->lastActivity = std::chrono::steady_clock::now();
        m_data->manager->d->rescheduleSelfPing();
        // Fetch room info now that the room is configured
        m_data->manager->d->fetchRoomInfo(m_data->roomJid);
    }
    co_return result;
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
    if (m_data->state != MucRoomState::Creating) {
        co_return QXmppError { u"Room is not in Creating state."_s };
    }

    QXmppDataForm cancelForm;
    cancelForm.setType(QXmppDataForm::Cancel);

    auto result = co_await set(m_data->manager->client(), m_data->roomJid, MucOwnerQuery { .form = std::move(cancelForm) }).withContext(m_data->manager);
    if (std::holds_alternative<Success>(result)) {
        m_data->manager->d->deactivateRoom(m_data->roomJid);
        m_data->manager->d->rescheduleSelfPing();
    }
    co_return result;
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
    if (m_data->state != MucRoomState::Joined) {
        co_return QXmppError { u"Room is not joined."_s };
    }

    auto result = co_await set(m_data->manager->client(), m_data->roomJid, MucOwnerQuery { .destroyAlternateJid = alternateJid, .destroyReason = reason }).withContext(m_data->manager);
    if (std::holds_alternative<Success>(result)) {
        m_data->manager->d->deactivateRoom(m_data->roomJid);
        m_data->manager->d->rescheduleSelfPing();
    }
    co_return result;
}

QBindable<QString> QXmppMucParticipant::nickname() const
{
    return &m_data->nickname;
}

QBindable<QString> QXmppMucParticipant::jid() const
{
    return &m_data->jid;
}

QBindable<Muc::Role> QXmppMucParticipant::role() const
{
    return &m_data->role;
}

QBindable<Muc::Affiliation> QXmppMucParticipant::affiliation() const
{
    return &m_data->affiliation;
}

QString QXmppMucParticipant::occupantId() const
{
    return m_data->occupantId;
}

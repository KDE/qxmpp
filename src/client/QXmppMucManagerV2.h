// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_H
#define QXMPPMUCMANAGERV2_H

#include "QXmppClientExtension.h"
#include "QXmppMessageHandler.h"
#include "QXmppMucData.h"
#include "QXmppMucForms.h"
#include "QXmppSendResult.h"
#include "QXmppTask.h"

#include <chrono>
#include <memory>
#include <optional>
#include <variant>

class QXmppDataForm;
class QXmppPresence;
namespace QXmpp::Private {
struct MucRoomData;
struct MucParticipantData;
}  // namespace QXmpp::Private

class QXmppError;

class QXmppMucRoomV2;
class QXmppMucParticipant;
struct QXmppMucManagerV2Private;

class QXMPP_EXPORT QXmppMucManagerV2 : public QXmppClientExtension, public QXmppMessageHandler
{
    Q_OBJECT

public:
    QXmppMucManagerV2();
    ~QXmppMucManagerV2();

    QStringList discoveryFeatures() const override;

    QBindable<QStringList> mucServices() const;
    QBindable<bool> mucServicesLoaded() const;

    std::chrono::milliseconds selfPingSilenceThreshold() const;
    void setSelfPingSilenceThreshold(std::chrono::milliseconds threshold);

    /// Returns a handle for a tracked room, or std::nullopt if the manager has no state
    /// for this jid. Inactive rooms (left but still held by a caller handle) are revived
    /// transparently so that leave → findRoom observes the same underlying state.
    std::optional<QXmppMucRoomV2> findRoom(const QString &jid);

    /// Joins the MUC room at \a jid with the given \a nickname.
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> joinRoom(const QString &jid, const QString &nickname);
    /// \overload
    /// Joins the room with optional \a history request parameters and an optional \a password
    /// for password-protected rooms (XEP-0045 §7.2.5).
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> joinRoom(const QString &jid, const QString &nickname,
                                                      std::optional<QXmpp::Muc::HistoryOptions> history,
                                                      const QString &password = {});
    QXmppTask<QXmpp::Result<QString>> requestUniqueRoomName(QString serviceJid);
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> createRoom(QString serviceJid,
                                                        QString nickname,
                                                        std::optional<QString> roomName = std::nullopt);
    /// Emitted when a participant joins a room.
    Q_SIGNAL void participantJoined(const QString &roomJid, const QXmppMucParticipant &participant);
    /// Emitted when a participant leaves a room.
    Q_SIGNAL void participantLeft(const QString &roomJid, const QXmppMucParticipant &participant, QXmpp::Muc::LeaveReason reason);
    /// Emitted when we are forcibly removed from a room (kicked, banned, etc.).
    /// The room state is still accessible during this signal's emission.
    /// After all handlers return, the room state is cleaned up.
    /// The \a destroy parameter contains room destruction info if the reason is \c RoomDestroyed.
    Q_SIGNAL void removedFromRoom(const QString &roomJid, QXmpp::Muc::LeaveReason reason, const std::optional<QXmpp::Muc::Destroy> &destroy);
    /// Emitted when room history messages are received during joining.
    Q_SIGNAL void roomHistoryReceived(const QString &roomJid, const QList<QXmppMessage> &messages);
    /// Emitted when a groupchat message is received in a joined room.
    Q_SIGNAL void messageReceived(const QString &roomJid, const QXmppMessage &message);
    /// Emitted when a voice request is received in a joined moderated room.
    ///
    /// This signal is only emitted for moderators. The moderator can approve or
    /// deny the request by calling QXmppMucRoomV2::answerVoiceRequest().
    ///
    /// \since QXmpp 1.16
    Q_SIGNAL void voiceRequestReceived(const QString &roomJid, const QXmppMucVoiceRequest &request);
    /// Emitted when a mediated invitation is received from a room.
    ///
    /// The user is not yet in the room at this point. Call QXmppMucManagerV2::joinRoom() to accept
    /// or QXmppMucManagerV2::declineInvitation() to decline.
    ///
    /// \since QXmpp 1.16
    Q_SIGNAL void invitationReceived(const QString &roomJid, const QXmpp::Muc::Invite &invite, const QString &password);
    /// Emitted when a room reports that an invitee declined an invitation.
    ///
    /// \since QXmpp 1.16
    Q_SIGNAL void invitationDeclined(const QString &roomJid, const QXmpp::Muc::Decline &decline);

    /// Emitted when a direct invitation (XEP-0249) is received.
    ///
    /// The \a from is the full JID of the user who sent the invitation.
    ///
    /// \since QXmpp 1.16
    Q_SIGNAL void directInvitationReceived(const QXmpp::Muc::DirectInvitation &invitation, const QString &from, const QXmppMessage &message);

    bool handleMessage(const QXmppMessage &) override;

    /// Sends a decline for a mediated invitation to \a roomJid.
    ///
    /// Can be called before joining the room.
    ///
    /// \since QXmpp 1.16
    QXmppTask<QXmpp::SendResult> declineInvitation(const QString &roomJid, QXmpp::Muc::Decline decline);

    /// Sends a direct invitation (XEP-0249) to \a to.
    ///
    /// The optional \a message parameter can be used to set custom extensions on the
    /// message stanza; the \c to, \c type, and invitation fields will be overwritten.
    ///
    /// \since QXmpp 1.16
    QXmppTask<QXmpp::SendResult> sendDirectInvitation(const QString &to, QXmpp::Muc::DirectInvitation invitation, QXmppMessage message = {});

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onConnected();
    void onDisconnected();

    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> createRoomAtJid(const QString &roomJid, const QString &nickname);

    friend class QXmppMucManagerV2Private;
    friend class QXmppMucRoomV2;
    friend class QXmppMucParticipant;
    friend class tst_QXmppMuc;
    const std::unique_ptr<QXmppMucManagerV2Private> d;
};

///
/// \class QXmppMucRoomV2
///
/// \brief Handle to a MUC room. Copyable value type backed by a shared_ptr.
///
/// A handle is obtained from QXmppMucManagerV2::joinRoom(), QXmppMucManagerV2::createRoom(), or
/// QXmppMucManagerV2::findRoom(). It owns a strong reference to the underlying room state so its
/// QBindable getters are always safe to read — the state lives as long as any handle does.
///
/// \par Lifetime
/// When you leave a room or are kicked, the manager drops its own reference but keeps a weak_ptr
/// so that caller handles can continue to observe the final state as a frozen snapshot. Rejoining
/// the same room while a handle is still held revives the existing state in place, preserving
/// cached roomInfo, roomConfig, avatars, etc. When the last handle is dropped, memory is freed.
///
/// \par Bindable properties
/// Most observable state (subject(), nickname(), joined(), canSendMessages(), …) is exposed as
/// QBindable<T>. You can connect them to QProperty observers or read them directly:
/// \code
/// auto canSend = room.canSendMessages();
/// canSend.addNotifier([canSend]() {
///     qDebug() << "canSendMessages changed:" << canSend.value();
/// });
/// \endcode
///
/// \par Participants
/// participants() returns a snapshot list of QXmppMucParticipant handles. Each participant handle
/// likewise owns a strong reference to its participant data, so it is safe to hold across
/// participants coming and going. Connect to QXmppMucManagerV2::participantJoined() and
/// participantLeft() (or the convenience helpers onParticipantJoined() / onParticipantLeft()) to
/// be notified of changes.
///
/// \note The QXmppMucManagerV2 itself must remain alive while handles are in use — the handles
/// own the data but the manager owns the signal infrastructure. Accessing a handle after the
/// manager is destroyed is undefined behavior.
///
/// \since QXmpp 1.16
///
class QXMPP_EXPORT QXmppMucRoomV2
{
public:
    QXmppMucRoomV2() = delete;
    QXmppMucRoomV2(const QXmppMucRoomV2 &) = default;
    QXmppMucRoomV2(QXmppMucRoomV2 &&) = default;
    QXmppMucRoomV2 &operator=(const QXmppMucRoomV2 &) = default;
    QXmppMucRoomV2 &operator=(QXmppMucRoomV2 &&) = default;

    QBindable<QString> subject() const;
    QBindable<QString> nickname() const;
    QBindable<bool> joined() const;
    QList<QXmppMucParticipant> participants() const;
    std::optional<QXmppMucParticipant> selfParticipant() const;

    QBindable<bool> canSendMessages() const;
    QBindable<bool> canChangeSubject() const;
    QBindable<bool> canSetRoles() const;
    QBindable<bool> canSetAffiliations() const;
    QBindable<bool> canConfigureRoom() const;
    QBindable<bool> isNonAnonymous() const;
    QBindable<bool> isPublic() const;
    QBindable<bool> isMembersOnly() const;
    QBindable<bool> isModerated() const;
    QBindable<bool> isPersistent() const;
    QBindable<bool> isPasswordProtected() const;
    QBindable<QString> description() const;
    QBindable<QString> language() const;
    QBindable<QStringList> contactJids() const;
    QBindable<std::optional<QXmppMucRoomInfo>> roomInfo() const;
    QBindable<std::optional<QXmppMucRoomConfig>> roomConfig() const;
    void setWatchRoomConfig(bool watch);
    bool isWatchingRoomConfig() const;

    QBindable<QStringList> avatarHashes() const;
    QBindable<std::optional<QXmpp::Muc::Avatar>> avatar() const;
    void setWatchAvatar(bool watch);
    bool isWatchingAvatar() const;
    QXmppTask<QXmpp::Result<>> setAvatar(std::optional<QXmpp::Muc::Avatar> avatar);

    QXmppTask<QXmpp::Result<>> sendMessage(QXmppMessage message);
    QXmppTask<QXmpp::SendResult> sendPrivateMessage(const QXmppMucParticipant &participant, QXmppMessage message);
    QXmppTask<QXmpp::Result<>> setSubject(const QString &subject);
    QXmppTask<QXmpp::Result<>> setNickname(const QString &newNick);
    QXmppTask<QXmpp::SendResult> setPresence(QXmppPresence presence);
    QXmppTask<QXmpp::Result<>> leave();

    QXmppTask<QXmpp::Result<>> setRole(const QXmppMucParticipant &participant, QXmpp::Muc::Role role, const QString &reason = {});
    QXmppTask<QXmpp::Result<>> setRoles(const QList<QXmpp::Muc::Item> &items);
    QXmppTask<QXmpp::Result<>> setAffiliation(const QString &jid, QXmpp::Muc::Affiliation affiliation, const QString &reason = {});
    QXmppTask<QXmpp::Result<>> setAffiliations(const QList<QXmpp::Muc::Item> &items);
    QXmppTask<QXmpp::Result<QList<QXmpp::Muc::Item>>> requestAffiliationList(QXmpp::Muc::Affiliation affiliation);
    QXmppTask<QXmpp::Result<QString>> requestReservedNickname();
    QXmppTask<QXmpp::Result<QXmppDataForm>> requestRegistrationForm();
    QXmppTask<QXmpp::Result<>> submitRegistration(const QXmppDataForm &form);

    QXmppTask<QXmpp::SendResult> requestVoice();
    QXmppTask<QXmpp::SendResult> answerVoiceRequest(const QXmppMucVoiceRequest &request, bool allow);
    QXmppTask<QXmpp::SendResult> inviteUser(QXmpp::Muc::Invite invite);

    QXmppTask<QXmpp::Result<QXmppMucRoomConfig>> requestRoomConfig(bool watch = false);
    QXmppTask<QXmpp::Result<>> setRoomConfig(const QXmppMucRoomConfig &config);
    QXmppTask<QXmpp::Result<>> cancelRoomCreation();
    QXmppTask<QXmpp::Result<>> destroyRoom(const QString &reason = {}, const QString &alternateJid = {});

    /// Connects to the participantJoined signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantJoined(QObject *context, Func &&f) const
    {
        auto *manager = this->manager();
        return QObject::connect(manager, &QXmppMucManagerV2::participantJoined, context, [jid = jid(), f = std::forward<Func>(f)](const QString &roomJid, const QXmppMucParticipant &participant) {
            if (roomJid == jid) {
                f(participant);
            }
        });
    }

    /// Connects to the participantLeft signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantLeft(QObject *context, Func &&f) const
    {
        auto *manager = this->manager();
        return QObject::connect(manager, &QXmppMucManagerV2::participantLeft, context, [jid = jid(), f = std::forward<Func>(f)](const QString &roomJid, const QXmppMucParticipant &participant, QXmpp::Muc::LeaveReason reason) {
            if (roomJid == jid) {
                f(participant, reason);
            }
        });
    }

    QString jid() const;
    QXmppMucManagerV2 *manager() const;

private:
    explicit QXmppMucRoomV2(std::shared_ptr<QXmpp::Private::MucRoomData> data)
        : m_data(std::move(data))
    {
    }

    friend class QXmppMucManagerV2;
    friend struct QXmppMucManagerV2Private;
    friend class tst_QXmppMuc;
    std::shared_ptr<QXmpp::Private::MucRoomData> m_data;
};

///
/// \class QXmppMucParticipant
///
/// Lightweight handle to a participant in a MUC room, backed by data in QXmppMucManagerV2.
///
/// \note Lifetime note: This is a lightweight handle and does not own any data.
/// The manager must remain alive for the lifetime of any participant handle.
///
/// \since QXmpp 1.16
///
class QXMPP_EXPORT QXmppMucParticipant
{
public:
    QXmppMucParticipant() = delete;
    QXmppMucParticipant(const QXmppMucParticipant &) = default;
    QXmppMucParticipant(QXmppMucParticipant &&) = default;
    QXmppMucParticipant &operator=(const QXmppMucParticipant &) = default;
    QXmppMucParticipant &operator=(QXmppMucParticipant &&) = default;

    /// Returns the participant's nickname in the room.
    QBindable<QString> nickname() const;
    /// Returns the participant's real JID if known.
    QBindable<QString> jid() const;
    /// Returns the participant's role in the room.
    QBindable<QXmpp::Muc::Role> role() const;
    /// Returns the participant's affiliation with the room.
    QBindable<QXmpp::Muc::Affiliation> affiliation() const;
    /// Returns the participant's occupant ID (XEP-0421) if the room supports it.
    QString occupantId() const;

private:
    explicit QXmppMucParticipant(std::shared_ptr<QXmpp::Private::MucParticipantData> data)
        : m_data(std::move(data))
    {
    }

    friend class QXmppMucManagerV2;
    friend struct QXmppMucManagerV2Private;
    friend class QXmppMucRoomV2;
    friend class tst_QXmppMuc;
    std::shared_ptr<QXmpp::Private::MucParticipantData> m_data;
};

#endif  // QXMPPMUCMANAGERV2_H

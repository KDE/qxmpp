// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_H
#define QXMPPMUCMANAGERV2_H

#include "QXmppClientExtension.h"
#include "QXmppMessageHandler.h"
#include "QXmppMucData.h"
#include "QXmppMucForms.h"
#include "QXmppMucIq.h"
#include "QXmppPubSubEventHandler.h"
#include "QXmppSendResult.h"
#include "QXmppTask.h"

#include <optional>
#include <variant>

class QXmppPresence;
namespace QXmpp::Private {
struct Bookmarks2Conference;
struct Bookmarks2ConferenceItem;
struct MucRoomData;
struct MucParticipantData;
}  // namespace QXmpp::Private

class QXmppError;
class QXmppMucBookmarkPrivate;

class QXMPP_EXPORT QXmppMucBookmark
{
public:
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucBookmark)

    QXmppMucBookmark();
    QXmppMucBookmark(const QString &jid, const QString &name, bool autojoin, const QString &nick, const QString &password);
    QXmppMucBookmark(const QString &jid, QXmpp::Private::Bookmarks2Conference conference);

    const QString &jid() const;
    void setJid(const QString &jid);
    const QString &name() const;
    void setName(const QString &name);
    const QString &nick() const;
    void setNick(const QString &nick);
    const QString &password() const;
    void setPassword(const QString &password);
    bool autojoin() const;
    void setAutojoin(bool autojoin);

private:
    friend class QXmppMucManagerV2Private;
    QSharedDataPointer<QXmppMucBookmarkPrivate> d;
};

Q_DECLARE_METATYPE(QXmppMucBookmark);

class QXmppMucRoomV2;
class QXmppMucParticipant;
struct QXmppMucManagerV2Private;

class QXMPP_EXPORT QXmppMucManagerV2 : public QXmppClientExtension, public QXmppPubSubEventHandler, public QXmppMessageHandler
{
    Q_OBJECT

public:
    struct BookmarkChange {
        QXmppMucBookmark oldBookmark;
        QXmppMucBookmark newBookmark;
    };

    struct Avatar {
        QString contentType;
        QByteArray data;
    };

    QXmppMucManagerV2();
    ~QXmppMucManagerV2();

    QStringList discoveryFeatures() const override;

    const std::optional<QList<QXmppMucBookmark>> &bookmarks() const;
    Q_SIGNAL void bookmarksReset();
    Q_SIGNAL void bookmarksAdded(const QList<QXmppMucBookmark> &newBookmarks);
    Q_SIGNAL void bookmarksChanged(const QList<QXmppMucManagerV2::BookmarkChange> &bookmarkUpdates);
    Q_SIGNAL void bookmarksRemoved(const QList<QString> &removedBookmarkJids);

    QXmppTask<QXmpp::Result<>> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<QXmpp::Result<>> removeBookmark(const QString &jid);

    QXmppTask<QXmpp::Result<>> setRoomAvatar(const QString &jid, const Avatar &avatar);
    QXmppTask<QXmpp::Result<>> removeRoomAvatar(const QString &jid);
    QXmppTask<QXmpp::Result<std::optional<Avatar>>> fetchRoomAvatar(const QString &jid);

    /// Returns a lightweight handle for the room with the given \a jid.
    QXmppMucRoomV2 room(const QString &jid);

    /// Joins the MUC room at \a jid with the given \a nickname.
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> joinRoom(const QString &jid, const QString &nickname);
    /// \overload
    /// Joins the room with optional \a history request parameters and an optional \a password
    /// for password-protected rooms (XEP-0045 §7.2.5).
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> joinRoom(const QString &jid, const QString &nickname,
                                                      std::optional<QXmpp::Muc::HistoryOptions> history,
                                                      const QString &password = {});
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> createRoom(const QString &jid, const QString &nickname);
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
    /// \since QXmpp 1.15
    Q_SIGNAL void voiceRequestReceived(const QString &roomJid, const QXmppMucVoiceRequest &request);

    bool handleMessage(const QXmppMessage &) override;
    bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) override;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onConnected();
    void onDisconnected();

    const QXmpp::Private::MucRoomData *roomData(const QString &jid) const;
    const QXmpp::Private::MucParticipantData *participantData(const QString &roomJid, uint32_t participantId) const;

    friend class QXmppMucManagerV2Private;
    friend class QXmppMucRoomV2;
    friend class QXmppMucParticipant;
    friend class tst_QXmppMuc;
    const std::unique_ptr<QXmppMucManagerV2Private> d;
};

Q_DECLARE_METATYPE(QXmppMucManagerV2::BookmarkChange);

///
/// \class QXmppMucRoomV2
///
/// \note Lifetime note: QXmppMucRoomV2 and QXmppMucParticipant are lightweight handles and do not
/// own any data.
/// They access state stored in the QXmppMucManager. The manager must remain alive for the lifetime
/// of any room or participant handle.
/// Accessing a handle after the manager is destroyed is undefined behavior.
///
/// \note Always call isValid() before accessing properties, especially if the room might have
/// been left or participants removed.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT QXmppMucRoomV2
{
public:
    bool isValid() const;
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
    QXmppTask<QXmpp::Result<>> subscribeToRoomConfig(bool enabled);

    QXmppTask<QXmpp::Result<>> sendMessage(QXmppMessage message);
    QXmppTask<QXmpp::SendResult> sendPrivateMessage(const QXmppMucParticipant &participant, QXmppMessage message);
    QXmppTask<QXmpp::Result<>> setSubject(const QString &subject);
    QXmppTask<QXmpp::Result<>> setNickname(const QString &newNick);
    QXmppTask<QXmpp::SendResult> setPresence(QXmppPresence presence);
    QXmppTask<QXmpp::Result<>> leave();

    QXmppTask<QXmpp::Result<>> setRole(const QXmppMucParticipant &participant, QXmpp::Muc::Role role, const QString &reason = {});
    QXmppTask<QXmpp::Result<>> setAffiliation(const QString &jid, QXmpp::Muc::Affiliation affiliation, const QString &reason = {});
    QXmppTask<QXmpp::Result<QList<QXmppMucItem>>> requestAffiliationList(QXmpp::Muc::Affiliation affiliation);

    QXmppTask<QXmpp::SendResult> requestVoice();
    QXmppTask<QXmpp::SendResult> answerVoiceRequest(const QXmppMucVoiceRequest &request, bool allow);

    QXmppTask<QXmpp::Result<QXmppMucRoomConfig>> requestRoomConfig();
    QXmppTask<QXmpp::Result<>> submitRoomConfig(const QXmppMucRoomConfig &config);
    QXmppTask<QXmpp::Result<>> cancelRoomCreation();
    QXmppTask<QXmpp::Result<>> destroyRoom(const QString &reason = {}, const QString &alternateJid = {});

    /// Connects to the participantJoined signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantJoined(QObject *context, Func &&f) const
    {
        return QObject::connect(m_manager, &QXmppMucManagerV2::participantJoined, context, [jid = m_jid, f = std::forward<Func>(f)](const QString &roomJid, const QXmppMucParticipant &participant) {
            if (roomJid == jid) {
                f(participant);
            }
        });
    }

    /// Connects to the participantLeft signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantLeft(QObject *context, Func &&f) const
    {
        return QObject::connect(m_manager, &QXmppMucManagerV2::participantLeft, context, [jid = m_jid, f = std::forward<Func>(f)](const QString &roomJid, const QXmppMucParticipant &participant, QXmpp::Muc::LeaveReason reason) {
            if (roomJid == jid) {
                f(participant, reason);
            }
        });
    }

private:
    QXmppMucRoomV2(QXmppMucManagerV2 *manager, const QString &jid)
        : m_manager(manager), m_jid(jid)
    {
    }

    friend class QXmppMucManagerV2;
    QXmppMucManagerV2 *m_manager;
    QString m_jid;
};

///
/// \class QXmppMucParticipant
///
/// Lightweight handle to a participant in a MUC room, backed by data in QXmppMucManagerV2.
///
/// \note Lifetime note: This is a lightweight handle and does not own any data.
/// The manager must remain alive for the lifetime of any participant handle.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT QXmppMucParticipant
{
public:
    /// Returns whether the participant handle refers to a valid participant.
    bool isValid() const;
    /// Returns the participant's nickname in the room.
    QBindable<QString> nickname() const;
    /// Returns the participant's real JID if known.
    QBindable<QString> jid() const;
    /// Returns the participant's role in the room.
    QBindable<QXmpp::Muc::Role> role() const;
    /// Returns the participant's affiliation with the room.
    QBindable<QXmpp::Muc::Affiliation> affiliation() const;

private:
    QXmppMucParticipant(QXmppMucManagerV2 *manager, const QString &roomJid, uint32_t participantId)
        : m_manager(manager), m_roomJid(roomJid), m_participantId(participantId)
    {
    }

    friend class QXmppMucManagerV2;
    friend class QXmppMucManagerV2Private;
    friend class QXmppMucRoomV2;
    friend class tst_QXmppMuc;
    QXmppMucManagerV2 *m_manager;
    QString m_roomJid;
    uint32_t m_participantId;
};

inline QXmppMucRoomV2 QXmppMucManagerV2::room(const QString &jid)
{
    return QXmppMucRoomV2(this, jid);
}

#endif  // QXMPPMUCMANAGERV2_H

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_H
#define QXMPPMUCMANAGERV2_H

#include "QXmppClientExtension.h"
#include "QXmppMessageHandler.h"
#include "QXmppMucData.h"
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
    /// Joins the room with optional \a history request parameters.
    QXmppTask<QXmpp::Result<QXmppMucRoomV2>> joinRoom(const QString &jid, const QString &nickname,
                                                      std::optional<QXmpp::Muc::HistoryOptions> history);
    /// Emitted when a participant joins a room.
    Q_SIGNAL void participantJoined(const QString &roomJid, const QString &participant);
    /// Emitted when a participant leaves a room.
    Q_SIGNAL void participantLeft(const QString &roomJid, const QString &participant);
    /// Emitted when room history messages are received during joining.
    Q_SIGNAL void roomHistoryReceived(const QString &roomJid, const QList<QXmppMessage> &messages);
    /// Emitted when a groupchat message is received in a joined room.
    Q_SIGNAL void messageReceived(const QString &roomJid, const QXmppMessage &message);

    bool handleMessage(const QXmppMessage &) override;
    bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) override;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onConnected();

    const QXmpp::Private::MucRoomData *roomData(const QString &jid) const;

    friend class QXmppMucManagerV2Private;
    friend class QXmppMucRoomV2;
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

    QXmppTask<QXmpp::Result<>> sendMessage(QXmppMessage message);
    QXmppTask<QXmpp::SendResult> sendPrivateMessage(const QString &occupantNick, QXmppMessage message);
    QXmppTask<QXmpp::Result<>> setSubject(const QString &subject);

    /// Connects to the participantJoined signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantJoined(QObject *context, Func &&f) const
    {
        return QObject::connect(m_manager, &QXmppMucManagerV2::participantJoined, context, [jid = m_jid, f = std::forward<Func>(f)](const QString &roomJid, const QString &id) {
            if (roomJid == jid) {
                f(id);
            }
        });
    }

    /// Connects to the participantLeft signal, filtered for this room.
    template<typename Func>
    QMetaObject::Connection onParticipantLeft(QObject *context, Func &&f) const
    {
        return QObject::connect(m_manager, &QXmppMucManagerV2::participantLeft, context, [jid = m_jid, f = std::forward<Func>(f)](const QString &roomJid, const QString &id) {
            if (roomJid == jid) {
                f(id);
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

inline QXmppMucRoomV2 QXmppMucManagerV2::room(const QString &jid)
{
    return QXmppMucRoomV2(this, jid);
}

#endif  // QXMPPMUCMANAGERV2_H

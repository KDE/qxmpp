// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGER_H
#define QXMPPMUCMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppMucIq.h"
#include "QXmppPresence.h"

class QXmppDataForm;
class QXmppDiscoveryIq;
class QXmppMessage;
class QXmppMucManagerPrivate;
class QXmppMucRoom;
class QXmppMucRoomPrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppMucManager class makes it possible to interact with
    multi-user chat rooms as defined by \xep{0045}{Multi-User Chat}.

    To make use of this manager, you need to instantiate it and load it into
    the QXmppClient instance as follows:

    \code
    QXmppMucManager *manager = new QXmppMucManager;
    client->addExtension(manager);
    \endcode

    You can then join a room as follows:

    \code
    QXmppMucRoom *room = manager->addRoom("room@conference.example.com");
    room->setNickName("mynick");
    room->join();
    \endcode

    \note This manager is going to be replaced with the QXmppMucManagerV2 in the future.

    \ingroup Managers
*/
class QXMPP_EXPORT QXmppMucManager : public QXmppClientExtension
{
    Q_OBJECT
    /*!
        \property QXmppMucManager::rooms

        List of joined MUC rooms
    */
    Q_PROPERTY(QList<QXmppMucRoom *> rooms READ rooms NOTIFY roomAdded)

public:
    QXmppMucManager();
    ~QXmppMucManager() override;

    QXmppMucRoom *addRoom(const QString &roomJid);

    QList<QXmppMucRoom *> rooms() const;

    QStringList discoveryFeatures() const override;
    bool handleStanza(const QDomElement &element) override;

    /*!
        This signal is emitted when an invitation to a chat room is received.

        \a roomJid, \a inviter, and \a reason.
    */
    Q_SIGNAL void invitationReceived(const QString &roomJid, const QString &inviter, const QString &reason);

    /*! This signal is emitted when a new \a room is managed. */
    Q_SIGNAL void roomAdded(QXmppMucRoom *room);

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    Q_SLOT void _q_messageReceived(const QXmppMessage &message);
    Q_SLOT void _q_roomDestroyed(QObject *object);

    const std::unique_ptr<QXmppMucManagerPrivate> d;
};

/*!
    \inmodule QXmpp

    \brief The QXmppMucRoom class represents a multi-user chat room
    as defined by \xep{0045}{Multi-User Chat}.

    \sa QXmppMucManager
*/
class QXMPP_EXPORT QXmppMucRoom : public QObject
{
    Q_OBJECT
    Q_FLAGS(Action Actions)

    /*!
        \property QXmppMucRoom::allowedActions

        The actions you are allowed to perform on the room
    */
    Q_PROPERTY(QXmppMucRoom::Actions allowedActions READ allowedActions NOTIFY allowedActionsChanged)
    /*!
        \property QXmppMucRoom::isJoined

        Whether you are currently in the room
    */
    Q_PROPERTY(bool isJoined READ isJoined NOTIFY isJoinedChanged)
    /*!
        \property QXmppMucRoom::jid

        The chat room's bare JID
    */
    Q_PROPERTY(QString jid READ jid CONSTANT)
    /*!
        \property QXmppMucRoom::name

        The chat room's human-readable name
    */
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    /*!
        \property QXmppMucRoom::nickName

        Your own nickname
    */
    Q_PROPERTY(QString nickName READ nickName WRITE setNickName NOTIFY nickNameChanged)
    /*!
        \property QXmppMucRoom::participants

        The list of participant JIDs
    */
    Q_PROPERTY(QStringList participants READ participants NOTIFY participantsChanged)
    /*!
        \property QXmppMucRoom::password

        The chat room password
    */
    Q_PROPERTY(QString password READ password WRITE setPassword)
    /*!
        \property QXmppMucRoom::subject

        The room's subject
    */
    Q_PROPERTY(QString subject READ subject WRITE setSubject NOTIFY subjectChanged)

public:
    /*!
        This enum is used to describe chat room actions.

        \value NoAction no action.
        \value SubjectAction change the room's subject.
        \value ConfigurationAction change the room's configuration.
        \value PermissionsAction change the room's permissions.
        \value KickAction kick users from the room.
    */
    enum Action {
        NoAction = 0,
        SubjectAction = 1,
        ConfigurationAction = 2,
        PermissionsAction = 4,
        KickAction = 8
    };
    Q_DECLARE_FLAGS(Actions, Action)

    ~QXmppMucRoom() override;

    Actions allowedActions() const;
    bool isJoined() const;
    QString jid() const;
    QString name() const;
    QString nickName() const;
    void setNickName(const QString &nickName);

    Q_INVOKABLE QString participantFullJid(const QString &jid) const;
    QXmppPresence participantPresence(const QString &jid) const;

    QStringList participants() const;
    QString password() const;
    void setPassword(const QString &password);

    QString subject() const;
    void setSubject(const QString &subject);

    /*! This signal is emitted when the allowed \a actions change. */
    Q_SIGNAL void allowedActionsChanged(QXmppMucRoom::Actions actions);

    /*! This signal is emitted when the \a configuration form for the room is received. */
    Q_SIGNAL void configurationReceived(const QXmppDataForm &configuration);

    /*! This signal is emitted when an \a error is encountered. */
    Q_SIGNAL void error(const QXmppStanza::Error &error);

    /*! This signal is emitted once you have joined the room. */
    Q_SIGNAL void joined();

    /*!
        This signal is emitted if you get kicked from the room.

        \a jid and \a reason.
    */
    Q_SIGNAL void kicked(const QString &jid, const QString &reason);

    Q_SIGNAL void isJoinedChanged();

    /*! This signal is emitted once you have left the room. */
    Q_SIGNAL void left();

    /*! This signal is emitted when a \a message is received. */
    Q_SIGNAL void messageReceived(const QXmppMessage &message);

    /*! This signal is emitted when the room's human-readable \a name changes. */
    Q_SIGNAL void nameChanged(const QString &name);

    /*!
        This signal is emitted when your own nick name changes.

        \a nickName.
    */
    Q_SIGNAL void nickNameChanged(const QString &nickName);

    /*!
        This signal is emitted when a participant joins the room.

        \a jid.
    */
    Q_SIGNAL void participantAdded(const QString &jid);

    /*!
        This signal is emitted when a participant changes.

        \a jid.
    */
    Q_SIGNAL void participantChanged(const QString &jid);

    /*!
        This signal is emitted when a participant leaves the room.

        \a jid.
    */
    Q_SIGNAL void participantRemoved(const QString &jid);

    Q_SIGNAL void participantsChanged();

    /*! This signal is emitted when the room's \a permissions are received. */
    Q_SIGNAL void permissionsReceived(const QList<QXmppMucItem> &permissions);

    /*! This signal is emitted when the room's \a subject changes. */
    Q_SIGNAL void subjectChanged(const QString &subject);

    Q_SLOT bool ban(const QString &jid, const QString &reason);
    Q_SLOT bool join();
    Q_SLOT bool kick(const QString &jid, const QString &reason);
    Q_SLOT bool leave(const QString &message = QString());
    Q_SLOT bool requestConfiguration();
    Q_SLOT bool requestPermissions();
    Q_SLOT bool setConfiguration(const QXmppDataForm &form);
    Q_SLOT bool setPermissions(const QList<QXmppMucItem> &permissions);
    Q_SLOT bool sendInvitation(const QString &jid, const QString &reason);
    Q_SLOT bool sendMessage(const QString &text);

private:
    QXmppMucRoom(QXmppClient *client, const QString &jid, QObject *parent);

    Q_SLOT void _q_disconnected();
    Q_SLOT void _q_messageReceived(const QXmppMessage &message);
    Q_SLOT void _q_presenceReceived(const QXmppPresence &presence);

    const std::unique_ptr<QXmppMucRoomPrivate> d;
    friend class QXmppMucManager;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QXmppMucRoom::Actions)

#endif

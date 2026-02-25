// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

///
/// \example muc_bot
///
/// This example demonstrates how to use QXmppMucManagerV2 to build a simple
/// MUC bot. It covers joining a room, sending and receiving messages, tracking
/// participants, managing bookmarks, and observing room state via QBindable.
///

#include "QXmppClient.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppMucManagerV2.h"
#include "QXmppPepBookmarkManager.h"
#include "QXmppPubSubManager.h"

#include <QCoreApplication>
#include <QProperty>
#include <QTimer>

using namespace QXmpp;

class MucBot : public QObject
{
    Q_OBJECT
public:
    MucBot(const QString &jid,
           const QString &password,
           const QString &roomJid,
           const QString &nick,
           QObject *parent = nullptr)
        : QObject(parent), m_roomJid(roomJid), m_nick(nick)
    {
        m_client.addNewExtension<QXmppPubSubManager>();
        m_muc = m_client.addNewExtension<QXmppMucManagerV2>();

        connect(&m_client, &QXmppClient::connected, this, &MucBot::onConnected);
        connect(&m_client, &QXmppClient::disconnected, this, []() { QCoreApplication::quit(); });

        connect(m_muc, &QXmppMucManagerV2::messageReceived, this, &MucBot::onMessage);
        connect(m_muc, &QXmppMucManagerV2::participantJoined, this, &MucBot::onParticipantJoined);
        connect(m_muc, &QXmppMucManagerV2::participantLeft, this, &MucBot::onParticipantLeft);
        connect(m_muc, &QXmppMucManagerV2::removedFromRoom,
                this, [](const QString &roomJid, Muc::LeaveReason reason, const std::optional<Muc::Destroy> &) {
                    qWarning() << "Removed from room" << roomJid << "reason:" << int(reason);
                });

        m_client.connectToServer(jid, password);
    }

private:
    void onConnected()
    {
        qDebug() << "Connected. Joining room" << m_roomJid;

        m_muc->joinRoom(m_roomJid, m_nick).then(this, [this](Result<QXmppMucRoomV2> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Failed to join room:" << error->description;
                return;
            }
            auto room = std::get<QXmppMucRoomV2>(std::move(result));
            qDebug() << "Joined room" << m_roomJid << "as" << room.nickname().value();

            // List participants already present
            qDebug() << "Current participants:";
            for (const auto &p : room.participants()) {
                qDebug() << " -" << p.nickname().value();
            }

            // Observe subject changes reactively via QBindable
            m_subjectNotifier = room.subject().addNotifier([this]() {
                qDebug() << "Subject:" << m_muc->room(m_roomJid).subject().value();
            });

            // Observe joined state – e.g. to detect kicks or bans
            m_joinedNotifier = room.joined().addNotifier([this]() {
                qDebug() << "Joined state changed:" << m_muc->room(m_roomJid).joined().value();
            });

            // Save a bookmark with autojoin enabled
            auto *bm = m_client.addNewExtension<QXmppPepBookmarkManager>();
            QXmppMucBookmark bookmark(m_roomJid, QStringLiteral("My Room"), true, m_nick, {});
            bm->setBookmark(std::move(bookmark)).then(this, [bm](Result<> result) {
                if (const auto *error = std::get_if<QXmppError>(&result)) {
                    qWarning() << "Failed to set bookmark:" << error->description;
                    return;
                }
                if (const auto &bookmarks = bm->bookmarks()) {
                    qDebug() << "Bookmarks count:" << bookmarks->size();
                }
            });

            // Set the room subject
            room.setSubject(QStringLiteral("Hello from MucBot!")).then(this, [](Result<> result) {
                if (const auto *error = std::get_if<QXmppError>(&result)) {
                    qWarning() << "Failed to set subject:" << error->description;
                }
            });

            // Leave after 60 seconds
            QTimer::singleShot(60000, this, [this]() {
                m_muc->room(m_roomJid).leave().then(this, [this](Result<> result) {
                    if (const auto *error = std::get_if<QXmppError>(&result)) {
                        qWarning() << "Failed to leave room:" << error->description;
                        return;
                    }
                    qDebug() << "Left room. Disconnecting.";
                    m_client.disconnectFromServer();
                });
            });
        });
    }

    void onMessage(const QString &roomJid, const QXmppMessage &message)
    {
        const auto senderNick = message.from().section(u'/', 1);

        // Ignore own messages
        if (senderNick == m_nick) {
            return;
        }

        qDebug() << "[" << roomJid << "]" << senderNick << ":" << message.body();

        // Echo the message back to the room
        QXmppMessage reply;
        reply.setBody(QStringLiteral("Echo: ") + message.body());
        m_muc->room(roomJid).sendMessage(std::move(reply)).then(this, [](Result<> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Failed to send message:" << error->description;
            }
        });
    }

    void onParticipantJoined(const QString &roomJid, const QXmppMucParticipant &participant)
    {
        qDebug() << participant.nickname().value() << "joined" << roomJid;
    }

    void onParticipantLeft(const QString &roomJid, const QXmppMucParticipant &participant, Muc::LeaveReason reason)
    {
        Q_UNUSED(reason)
        qDebug() << participant.nickname().value() << "left" << roomJid;
    }

private:
    QXmppClient m_client;
    QXmppMucManagerV2 *m_muc;
    QString m_roomJid;
    QString m_nick;
    QPropertyNotifier m_subjectNotifier;
    QPropertyNotifier m_joinedNotifier;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QXmppLogger::getLogger()->setLoggingType(QXmppLogger::StdoutLogging);

    if (argc < 5) {
        qWarning() << "Usage:" << argv[0] << "<jid> <password> <room-jid> <nick>";
        return 1;
    }

    MucBot bot(argv[1], argv[2], argv[3], argv[4]);

    return app.exec();
}

#include "muc_bot.moc"

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

///
/// \example muc_create
///
/// This example demonstrates the discovery and creation side of QXmppMucManagerV2:
/// finding the MUC services offered by a server, probing a JID to see whether it
/// hosts a MUC room, and creating and configuring a new room.
///
/// It also shows how to observe QBindable state correctly. Every observer handle
/// is stored as a member — a discarded QPropertyNotifier unsubscribes immediately
/// and the callback never runs. The "Reactive Properties" page in the QXmpp
/// documentation covers this in full.
///

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppLogger.h"
#include "QXmppMucManagerV2.h"

#include <optional>

#include <QCoreApplication>
#include <QProperty>

using namespace QXmpp;

class MucCreator : public QObject
{
    Q_OBJECT
public:
    MucCreator(const QString &jid, const QString &password, const QString &probeJid, QObject *parent = nullptr)
        : QObject(parent), m_probeJid(probeJid)
    {
        m_client.logger()->setLoggingType(QXmppLogger::StdoutLogging);
        m_client.logger()->enablePrettyXml();

        m_client.addNewExtension<QXmppDiscoveryManager>();
        m_muc = m_client.addNewExtension<QXmppMucManagerV2>();

        connect(&m_client, &QXmppClient::disconnected, this, []() { QCoreApplication::quit(); });

        // Without this the example would sit in QXmppClient's reconnection loop forever when
        // the server is unreachable or the credentials are wrong.
        connect(&m_client, &QXmppClient::errorOccurred, this, [](const QXmppError &error) {
            qWarning() << "Client error:" << error.description;
            QCoreApplication::exit(1);
        });

        // Service discovery runs after connecting, so mucServices() is empty until then.
        // The notifier handle has to be a member or it unsubscribes right away.
        m_servicesLoadedNotifier = m_muc->mucServicesLoaded().addNotifier([this]() {
            if (m_muc->mucServicesLoaded().value()) {
                onServicesLoaded();
            }
        });

        m_client.connectToServer(jid, password);
    }

private:
    void onServicesLoaded()
    {
        // mucServices() gives the plain JIDs, mucServiceInfos() the full disco#info.
        const auto services = m_muc->mucServiceInfos().value();
        if (services.isEmpty()) {
            qWarning() << "This server offers no MUC service — cannot create group chats.";
            m_client.disconnectFromServer();
            return;
        }

        qDebug() << "MUC services:";
        for (const auto &service : services) {
            qDebug() << " -" << service.jid;
            for (const auto &identity : service.info.identities()) {
                qDebug() << "   identity:" << identity.category() << identity.type() << identity.name();
            }
            qDebug() << "   supports unique room names:"
                     << service.info.features().contains(QStringLiteral("http://jabber.org/protocol/muc#unique"));
        }

        m_serviceJid = services.first().jid;

        if (!m_probeJid.isEmpty()) {
            probeJid();
        } else {
            createRoom();
        }
    }

    // false covers everything that is not a MUC room: a MIX channel, a MUC service, an
    // ordinary account, or a room that does not exist.
    void probeJid()
    {
        m_muc->isMucRoom(m_probeJid).then(this, [this](Result<bool> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Could not probe" << m_probeJid << ":" << error->description;
            } else {
                qDebug() << m_probeJid << (std::get<bool>(result) ? "is a MUC room." : "is not a MUC room.");
            }
            createRoom();
        });
    }

    void createRoom()
    {
        qDebug() << "Creating a room on" << m_serviceJid;

        // Without a room name, createRoom() asks the service for a unique localpart
        // (XEP-0307) and falls back to a generated UUID if that is unsupported.
        m_muc->createRoom(m_serviceJid, QStringLiteral("owner")).then(this, [this](Result<QXmppMucRoomV2> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Failed to create room:" << error->description;
                m_client.disconnectFromServer();
                return;
            }

            // The handle must be kept alive: a QBindable taken from it does not own the room.
            m_room = std::get<QXmppMucRoomV2>(std::move(result));
            qDebug() << "Created locked room" << m_room->jid();

            // addNotifier() only reports future changes, and joined() is already true by the
            // time a joinRoom() task resolves, so read the current value once as well.
            reportJoined();
            m_joinedNotifier = m_room->joined().addNotifier([this]() { reportJoined(); });

            configureRoom();
        });
    }

    void reportJoined()
    {
        qDebug() << "joined:" << m_room->joined().value();
    }

    void configureRoom()
    {
        // The room is created in a locked state. createRoom() already fetched the owner
        // configuration form, so roomConfig() is populated when the task resolves.
        const auto &config = m_room->roomConfig().value();
        if (!config) {
            qWarning() << "Room was created without a usable configuration form.";
            m_client.disconnectFromServer();
            return;
        }

        qDebug() << "Default room name from the service:" << config->name();

        auto newConfig = *config;
        newConfig.setName(QStringLiteral("Example Room"));
        newConfig.setDescription(QStringLiteral("Created by the QXmpp muc_create example"));
        newConfig.setPersistent(true);
        newConfig.setPublic(true);

        // Submitting the configuration unlocks the room and joins it.
        m_room->setRoomConfig(newConfig).then(this, [this](Result<> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Failed to configure room:" << error->description;
                m_client.disconnectFromServer();
                return;
            }
            qDebug() << "Room unlocked:" << m_room->jid();
            destroyRoom();
        });
    }

    void destroyRoom()
    {
        // Drop this call to keep the room.
        m_room->destroyRoom(QStringLiteral("Example finished")).then(this, [this](Result<> result) {
            if (const auto *error = std::get_if<QXmppError>(&result)) {
                qWarning() << "Failed to destroy room:" << error->description;
            } else {
                qDebug() << "Room destroyed.";
            }
            m_client.disconnectFromServer();
        });
    }

private:
    QXmppClient m_client;
    QXmppMucManagerV2 *m_muc;
    QString m_probeJid;
    QString m_serviceJid;

    // QXmppMucRoomV2 has no default constructor, so the handle is held in an optional.
    std::optional<QXmppMucRoomV2> m_room;

    // Notifier handles unsubscribe in their destructor and must outlive the observation.
    QPropertyNotifier m_servicesLoadedNotifier;
    QPropertyNotifier m_joinedNotifier;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 3) {
        qWarning() << "Usage:" << argv[0] << "<jid> <password> [jid-to-probe]";
        return 1;
    }

    MucCreator creator(argv[1], argv[2], argc > 3 ? argv[3] : QString());

    return app.exec();
}

#include "muc_create.moc"

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_P_H
#define QXMPPMUCMANAGERV2_P_H

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMucData.h"
#include "QXmppMucData_p.h"
#include "QXmppMucManagerV2.h"
#include "QXmppTask.h"

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>

#include <QTimer>

namespace QXmpp::Private {
struct MucRoomData;

constexpr auto MucJoinTimeout = std::chrono::seconds(30);
constexpr auto MucSelfPingDefaultThreshold = std::chrono::minutes(5);
constexpr int MucSelfPingMaxRetries = 4;
}  // namespace QXmpp::Private

struct QXmppMucManagerV2Private {
    QXmppMucManagerV2 *q = nullptr;
    std::unordered_map<QString, std::shared_ptr<QXmpp::Private::MucRoomData>> activeRooms;
    std::unordered_map<QString, std::weak_ptr<QXmpp::Private::MucRoomData>> inactiveRooms;
    std::chrono::milliseconds timeout { QXmpp::Private::MucJoinTimeout };

    // MUC service discovery
    std::optional<QXmppDiscoServicesWatch> servicesWatch;
    QProperty<QStringList> mucServices;
    QProperty<bool> mucServicesLoaded { false };

    // XEP-0410: MUC Self-Ping — manager-wide scheduler state
    QTimer selfPingTimer;
    std::chrono::milliseconds selfPingSilenceThreshold { QXmpp::Private::MucSelfPingDefaultThreshold };

    QXmppMucManagerV2Private(QXmppMucManagerV2 *manager);

    QXmppDiscoveryManager *disco();

    // Room lifetime — see the big header comment on activeRooms/inactiveRooms above.
    std::shared_ptr<QXmpp::Private::MucRoomData> lookupRoom(const QString &jid);
    std::shared_ptr<QXmpp::Private::MucRoomData> createOrReviveRoom(const QString &jid);
    void deactivateRoom(const QString &jid);
    void deactivateAllRooms();
    void pruneInactiveRooms();

    // MUC Core
    void fetchRoomInfo(const QString &roomJid);
    void fetchAvatar(const QString &roomJid);
    QXmppTask<QXmpp::Result<QXmpp::Private::MucOwnerQuery>> sendOwnerFormRequest(const QString &roomJid);
    void fetchConfigForm(const QString &roomJid);
    void fetchRoomConfigSubscribed(const QString &roomJid);
    void handlePresence(const QXmppPresence &);
    void handleRoomPresence(const QString &roomJid, QXmpp::Private::MucRoomData &data, QXmppPresence);
    void throwRoomError(const QString &roomJid, QXmppError &&error);
    void handleJoinTimeout(const QString &roomJid);
    void handleLeaveTimeout(const QString &roomJid);
    void handleNickChangeTimeout(const QString &roomJid);
    void handleMessageTimeout(const QString &roomJid, const QString &originId);

    // XEP-0410: MUC Self-Ping
    void rescheduleSelfPing();
    void onSelfPingTick();
    void runSelfPing(const QString &roomJid);

    // XEP-0410 test accessors — MucRoomData is defined in the .cpp, so tests can't
    // reach it directly. These helpers let tst_QXmppMuc inspect/manipulate per-room
    // self-ping state without exposing the full struct.
    bool testSelfPingInFlight(const QString &roomJid) const;
    int testSelfPingRetryCount(const QString &roomJid) const;
    bool testHasRoom(const QString &roomJid) const;
    void testForceDueForSelfPing(const QString &roomJid);
    std::chrono::steady_clock::time_point testLastActivity(const QString &roomJid) const;
};

#endif  // QXMPPMUCMANAGERV2_P_H

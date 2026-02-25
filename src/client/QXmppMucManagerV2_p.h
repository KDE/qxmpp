// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_P_H
#define QXMPPMUCMANAGERV2_P_H

#include "QXmppClient.h"
#include "QXmppMucManagerV2.h"
#include "QXmppTask.h"

#include <chrono>
#include <optional>
#include <unordered_map>

class QXmppDiscoveryManager;

namespace QXmpp::Private {
struct MucRoomData;

constexpr auto MucJoinTimeout = std::chrono::seconds(30);
}  // namespace QXmpp::Private

struct QXmppMucManagerV2Private {
    QXmppMucManagerV2 *q = nullptr;
    std::unordered_map<QString, QXmpp::Private::MucRoomData> rooms;
    uint32_t participantIdCounter = 0;
    std::chrono::milliseconds timeout { QXmpp::Private::MucJoinTimeout };

    QXmppDiscoveryManager *disco();

    // MUC Core
    void clearAllRooms();
    void fetchRoomInfo(const QString &roomJid);
    void fetchAvatar(const QString &roomJid);
    QXmppTask<QXmppClient::IqResult> sendOwnerFormRequest(const QString &roomJid);
    void fetchConfigForm(const QString &roomJid);
    void fetchRoomConfigSubscribed(const QString &roomJid);
    void handlePresence(const QXmppPresence &);
    void handleRoomPresence(const QString &roomJid, QXmpp::Private::MucRoomData &data, const QXmppPresence &);
    void throwRoomError(const QString &roomJid, QXmppError &&error);
    void handleJoinTimeout(const QString &roomJid);
    void handleLeaveTimeout(const QString &roomJid);
    void handleNickChangeTimeout(const QString &roomJid);
    void handleMessageTimeout(const QString &roomJid, const QString &originId);
    uint32_t generateParticipantId() { return participantIdCounter++; }
};

#endif  // QXMPPMUCMANAGERV2_P_H

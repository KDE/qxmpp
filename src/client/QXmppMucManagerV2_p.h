// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_P_H
#define QXMPPMUCMANAGERV2_P_H

#include "QXmppMucManagerV2.h"
#include "QXmppTask.h"

#include <chrono>
#include <optional>
#include <unordered_map>

class QXmppPubSubManager;
class QXmppDiscoveryManager;

namespace QXmpp::Private {
struct MucRoomData;
struct Bookmarks2ConferenceItem;

constexpr auto MucJoinTimeout = std::chrono::seconds(30);
}  // namespace QXmpp::Private

struct QXmppMucManagerV2Private {
    QXmppMucManagerV2 *q = nullptr;
    std::optional<QList<QXmppMucBookmark>> bookmarks;
    std::unordered_map<QString, QXmpp::Private::MucRoomData> rooms;
    uint32_t participantIdCounter = 0;
    std::chrono::milliseconds timeout { QXmpp::Private::MucJoinTimeout };

    QXmppPubSubManager *pubsub();
    QXmppDiscoveryManager *disco();

    // Bookmarks 2
    void setBookmarks(QVector<QXmpp::Private::Bookmarks2ConferenceItem> &&items);
    void resetBookmarks();
    QXmppTask<QXmpp::Result<>> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<QXmpp::Result<>> removeBookmark(const QString &jid);

    // MUC Core
    void clearAllRooms();
    void fetchRoomInfo(const QString &roomJid);
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

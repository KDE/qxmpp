// SPDX-FileCopyrightText: 2019 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALLMANAGER_P_H
#define QXMPPCALLMANAGER_P_H

#include "QXmppCall.h"
#include "QXmppExternalService.h"
#include "QXmppStunServer.h"
#include "QXmppTask.h"
#include "QXmppTurnServer.h"

#include <QDateTime>

class QXmppCallManager;
class QXmppJingleReason;

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

namespace QXmpp::Private {

struct StunServerConfig {
    StunServer server;
    std::optional<QDateTime> expires;
};

struct TurnServerConfig {
    TurnServer server;
    std::optional<QDateTime> expires;
};

struct StunTurnConfig {
    QList<StunServerConfig> stun;
    std::optional<TurnServerConfig> turn;
};

using ServiceResult = std::variant<QXmppExternalService, QXmppError>;
using ServicesResult = std::variant<QVector<QXmppExternalService>, QXmppError>;
QXmppTask<ServicesResult> requestExternalServices(QXmppClient *client, const QString &jid);
QXmppTask<ServiceResult> requestCredentials(QXmppClient *client, const QString &jid, const QString &type, const QString &host);

using StunTurnResult = std::variant<StunTurnConfig, QXmppError>;
QXmppTask<StunTurnResult> requestStunTurnConfig(QXmppClient *client, QXmppLoggable *context);

}  // namespace QXmpp::Private

class QXmppCallManagerPrivate
{
public:
    explicit QXmppCallManagerPrivate(QXmppCallManager *qq);

    void addCall(QXmppCall *call);
    QList<QXmpp::StunServer> stunServers() const;
    std::optional<QXmpp::TurnServer> turnServer() const;

    QList<QXmppCall *> calls;

    // STUN/TURN config
    std::optional<QXmpp::Private::StunTurnConfig> stunTurnServers;
    QList<QXmpp::StunServer> fallbackStunServers;
    std::optional<QXmpp::TurnServer> fallbackTurnServer;

    bool dtlsRequired = false;
    bool supportsDtls = false;

private:
    QXmppCallManager *q;
};

#endif

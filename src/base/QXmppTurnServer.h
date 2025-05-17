// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTURNSERVER_H
#define QXMPPTURNSERVER_H

#include "QXmppGlobal.h"

#include <QHostAddress>

namespace QXmpp {

///
/// \brief TURN server address
///
/// \since QXmpp 1.11
///
struct TurnServer {
    /// host address of the TURN server
    QHostAddress host;
    /// port of the TURN server (default: 3478)
    quint16 port;
    /// username for authentication
    QString username;
    /// password for authentication
    QString password;
};

}  // namespace QXmpp

#endif  // QXMPPTURNSERVER_H

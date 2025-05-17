// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTUNSERVER_H
#define QXMPPSTUNSERVER_H

#include "QXmppGlobal.h"

#include <QHostAddress>

namespace QXmpp {

///
/// \brief STUN server address
///
/// \since QXmpp 1.11
///
struct StunServer {
    /// host address of the STUN server
    QHostAddress host;
    /// port of the STUN server (default: 3478)
    quint16 port;
};

}  // namespace QXmpp

#endif

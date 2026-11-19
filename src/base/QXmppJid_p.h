// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPJID_P_H
#define QXMPPJID_P_H

#include "QXmppGlobal.h"

#include <QString>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.
//
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

namespace QXmpp::Private {

// Compare JID strings as defined in RFC 7622. Identical strings are always equal, otherwise both
// need to be valid JIDs.
QXMPP_PRIVATE_EXPORT bool jidsEqual(QStringView a, QStringView b);
QXMPP_PRIVATE_EXPORT bool bareJidsEqual(QStringView a, QStringView b);

// Parts of a JID string. Valid JIDs are returned in canonical form (see QXmpp::Jid), strings that
// are no valid JIDs are only split (RFC 7622, section 3.2), so that they can still be addressed.
QXMPP_PRIVATE_EXPORT QString jidBare(QStringView jid);
QXMPP_PRIVATE_EXPORT QString jidLocalpart(QStringView jid);
QXMPP_PRIVATE_EXPORT QString jidDomain(QStringView jid);
QXMPP_PRIVATE_EXPORT QString jidResource(QStringView jid);

}  // namespace QXmpp::Private

#endif  // QXMPPJID_P_H

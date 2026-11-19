// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPJID_P_H
#define QXMPPJID_P_H

#include "QXmppError.h"
#include "QXmppGlobal.h"
#include "QXmppJid.h"

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

// The parsed JID, null if it is invalid.
inline Jid jidOrNull(QStringView jid) { return Jid::fromString(jid).value_or(Jid()); }

// Parses a JID passed as QString to the API, empty strings result in a null JID. Invalid JIDs
// result in a jid-malformed error, like the server would reply with.
QXMPP_PRIVATE_EXPORT std::variant<Jid, QXmppError> parseJidArgument(QStringView jid);
// Parses JIDs passed as QString to the API and returns their bare JIDs.
QXMPP_PRIVATE_EXPORT std::variant<QList<Jid>, QXmppError> parseBareJidArguments(const QList<QString> &jids);

}  // namespace QXmpp::Private

#endif  // QXMPPJID_P_H

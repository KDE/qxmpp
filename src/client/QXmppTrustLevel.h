// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTRUSTLEVEL_H
#define QXMPPTRUSTLEVEL_H

#include <QFlags>
#include <QHashFunctions>

namespace QXmpp {

/*!
    Trust level of public long-term keys used by end-to-end encryption
    protocols

    \since QXmpp 1.5

    \value Undecided The key's trust is not decided.
    \value AutomaticallyDistrusted The key is automatically distrusted (e.g., by the security policy TOAKAFA). \sa SecurityPolicy.
    \value ManuallyDistrusted The key is manually distrusted (e.g., by clicking a button or \xep{0450}{Automatic Trust Management (ATM)}).
    \value AutomaticallyTrusted The key is automatically trusted (e.g., by the client for all keys of a bare JID until one of it is authenticated).
    \value ManuallyTrusted The key is manually trusted (e.g., by clicking a button).
    \value Authenticated The key is authenticated (e.g., by QR code scanning or \xep{0450}{Automatic Trust Management (ATM)}).
*/
enum class TrustLevel {
    Undecided = 1,
    AutomaticallyDistrusted = 2,
    ManuallyDistrusted = 4,
    AutomaticallyTrusted = 8,
    ManuallyTrusted = 16,
    Authenticated = 32,
};

Q_DECLARE_FLAGS(TrustLevels, TrustLevel)
Q_DECLARE_OPERATORS_FOR_FLAGS(TrustLevels)

// Scoped enums (enum class) are not implicitly converted to int
inline auto qHash(QXmpp::TrustLevel key, uint seed) noexcept { return ::qHash(std::underlying_type_t<QXmpp::TrustLevel>(key), seed); }

}  // namespace QXmpp

#endif  // QXMPPTRUSTLEVEL_H

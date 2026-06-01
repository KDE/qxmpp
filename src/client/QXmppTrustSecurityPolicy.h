// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTRUSTSECURITYPOLICY_H
#define QXMPPTRUSTSECURITYPOLICY_H

#include <QMetaType>

namespace QXmpp {

/*!
    Security policy to decide which public long-term keys are used for encryption because they are
    trusted

    \since QXmpp 1.5

    \value NoSecurityPolicy New keys must be trusted manually.
    \value Toakafa New keys are trusted automatically until the first authentication but automatically distrusted afterwards (\xep{0450}{Automatic Trust Management (ATM)}).
*/
enum TrustSecurityPolicy {
    NoSecurityPolicy,
    Toakafa,
};

}  // namespace QXmpp

Q_DECLARE_METATYPE(QXmpp::TrustSecurityPolicy)

#endif  // QXMPPTRUSTSECURITYPOLICY_H

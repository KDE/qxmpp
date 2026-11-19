// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPRECIS_P_H
#define QXMPPPRECIS_P_H

#include <variant>

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

namespace QXmpp::Private::Precis {

struct Error {
    QString description;
};

using Result = std::variant<QString, Error>;

// RFC 8265, section 3.3
Result enforceUsernameCaseMapped(QStringView input);
// RFC 8265, section 4.2
Result enforceOpaqueString(QStringView input);

}  // namespace QXmpp::Private::Precis

#endif  // QXMPPPRECIS_P_H

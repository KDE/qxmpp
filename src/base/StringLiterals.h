// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef STRINGLITERALS_H
#define STRINGLITERALS_H

#include <QString>
#include <QStringView>

using namespace Qt::Literals::StringLiterals;

namespace QXmpp::Private {

/// Creates a QString from a QStringView with static storage duration without copying.
///
/// Constructs a QString with a null QArrayData header, same as QStringLiteral. This means
/// no ref-counting, no heap allocation, and no deallocation. Only safe for QStringViews
/// backed by static storage (e.g. inline constexpr QStringView literals).
inline QString staticString(QStringView sv)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return QString(QStringPrivate(nullptr, const_cast<char16_t *>(sv.utf16()), sv.size()));
}

}  // namespace QXmpp::Private

#endif  // STRINGLITERALS_H

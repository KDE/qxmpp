// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLFORMATTER_P_H
#define QXMPPXMLFORMATTER_P_H

#include "QXmppXmlFormatter.h"

#include <optional>

namespace QXmpp::Private {

struct XmlFormatOptions {
    bool indent = true;
    int indentWidth = 2;
    bool colorize = false;
    // maximum length of an XML text node; longer text is elided in the middle
    std::optional<qsizetype> elideTextAbove;
};

QString formatXmlForDebug(QStringView raw, const XmlFormatOptions &options);

// Cuts the middle out of text longer than maxLength, leaving head and tail visible.
// If ownLine is true, the elision marker is placed on a line of its own.
QString elideMiddle(QStringView text, qsizetype maxLength, bool colorize = false, bool ownLine = false);

}  // namespace QXmpp::Private

#endif  // QXMPPXMLFORMATTER_P_H

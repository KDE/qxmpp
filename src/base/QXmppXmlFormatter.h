// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@jahnson.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLFORMATTER_H
#define QXMPPXMLFORMATTER_H

#include "QXmppGlobal.h"

#include <QString>

namespace QXmpp {

///
/// Pretty-prints an XML fragment for debug display.
///
/// Reparses \a raw with QXmlStreamReader and re-emits it with line breaks and
/// indentation. If \a colorize is true, ANSI 16-color escape sequences are
/// inserted for terminal syntax highlighting (tags, attributes, text).
///
/// Tolerates XMPP stream fragments that are not standalone documents (such as
/// inner child stanzas without their enclosing `<stream:stream>`). If the
/// input cannot be parsed as XML at all, the original string is returned
/// unchanged, so non-XML payloads (e.g. STUN packet dumps) pass through.
///
/// \param raw          The raw XML string.
/// \param indent       Whether to add line breaks and indentation.
/// \param indentWidth  Number of spaces per indentation level.
/// \param colorize     Whether to emit ANSI 16-color escapes.
///
/// \since QXmpp 1.16
///
QXMPP_EXPORT QString formatXmlForDebug(QStringView raw,
                                       bool indent = true,
                                       int indentWidth = 2,
                                       bool colorize = false);

}  // namespace QXmpp

#endif  // QXMPPXMLFORMATTER_H

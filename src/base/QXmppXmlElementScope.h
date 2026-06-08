// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLELEMENTSCOPE_H
#define QXMPPXMLELEMENTSCOPE_H

#include <QFlags>

namespace QXmpp::Xml {

/*!
    \inmodule QXmpp

    \brief Identifies the type of stanza a registered XML extension applies to.

    Scopes are combined via QFlags. Scope::Generic is a catch-all that matches in
    every stanza scope, intended for application-defined extensions that do not
    belong to a specific stanza type.

    \value Generic Matches in every scope (catch-all for application extensions).
    \value Message Matches in Message stanzas.
    \value Presence Matches in Presence stanzas.

    \since QXmpp 1.16
*/
enum class Scope : quint32 {
    Generic = 1U << 0,
    Message = 1U << 1,
    Presence = 1U << 2,
};
Q_DECLARE_FLAGS(Scopes, Scope)

}  // namespace QXmpp::Xml

Q_DECLARE_OPERATORS_FOR_FLAGS(QXmpp::Xml::Scopes)

#endif  // QXMPPXMLELEMENTSCOPE_H

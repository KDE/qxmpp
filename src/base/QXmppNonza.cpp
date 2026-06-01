// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppNonza.h"

/*!
    \class QXmppNonza
    \inmodule QXmpp

    Abstract class for content that can be parsed from DOM and serialized to
    XML.

    If you want to implement a XMPP stanza (IQ, message or presence) then you
    should use QXmppStanza. Directly inheriting from this class is useful for
    other elements like stream management elements in the XML stream.

    \since QXmpp 1.5
*/

/*!
    \fn bool QXmppNonza::isXmppStanza() const

    Returns whether the QXmppStanza is a stanza in the XMPP sense (i. e. a message,
    iq or presence).

    \since QXmpp 1.0 (moved from QXmppStanza in 1.5)
*/

/*!
    \fn void QXmppNonza::parse(const QDomElement &element)

    Parses the object from a DOM element \a element.
*/

/*!
    \fn void QXmppNonza::toXml(QXmlStreamWriter *writer) const

    Serializes the object to XML using the QXmlStreamWriter \a writer.
*/

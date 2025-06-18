// SPDX-FileCopyrightText: 2011 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBindIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"
#include "packets/Bind.h"

#include <QDomElement>
#include <QTextStream>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

/// Creates a Bind IQ of type set with the specified resource.
QXmppBindIq QXmppBindIq::bindAddressIq(const QString &resource)
{
    QXmppBindIq iq;
    iq.setType(QXmppIq::Set);
    iq.setResource(resource);
    return iq;
}

/// Returns the bound JID.
QString QXmppBindIq::jid() const { return m_data.jid; }

/// Sets the bound JID.
void QXmppBindIq::setJid(const QString &jid) { m_data.jid = jid; }

/// Returns the requested resource.
QString QXmppBindIq::resource() const { return m_data.resource; }

/// Sets the requested resource.
void QXmppBindIq::setResource(const QString &resource) { m_data.resource = resource; }

/// \cond
void QXmppBindIq::parseElementFromChild(const QDomElement &element)
{
    try {
        m_data = XmlSpecParser::parse<Bind>(element.firstChildElement());
    } catch (ParsingError) {
    }
}

void QXmppBindIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlSpecSerializer::serialize(XmlWriter(writer), m_data);
}
/// \endcond

// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppVersionIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

/// Returns the name of the software.
QString QXmppVersionIq::name() const
{
    return m_name;
}

/// Sets the name of the software.
void QXmppVersionIq::setName(const QString &name)
{
    m_name = name;
}

/// Returns the operating system.
QString QXmppVersionIq::os() const
{
    return m_os;
}

/// Sets the operating system.
void QXmppVersionIq::setOs(const QString &os)
{
    m_os = os;
}

/// Returns the software version.
QString QXmppVersionIq::version() const
{
    return m_version;
}

/// Sets the software version.
void QXmppVersionIq::setVersion(const QString &version)
{
    m_version = version;
}

/// \cond
bool QXmppVersionIq::isVersionIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_version);
}

bool QXmppVersionIq::checkIqType(const QString &tagName, const QString &xmlNamespace)
{
    return tagName == u"query" && xmlNamespace == ns_version;
}

void QXmppVersionIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    m_name = queryElement.firstChildElement(u"name"_s).text();
    m_os = queryElement.firstChildElement(u"os"_s).text();
    m_version = queryElement.firstChildElement(u"version"_s).text();
}

void QXmppVersionIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        OptionalTextElement { u"name", m_name },
        OptionalTextElement { u"os", m_os },
        OptionalTextElement { u"version", m_version },
    });
}
/// \endcond

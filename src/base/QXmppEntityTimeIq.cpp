// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppEntityTimeIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

struct TimezoneOffset {
    int seconds;
};

template<>
struct QXmpp::Private::StringSerializer<TimezoneOffset> {
    static QString serialize(const TimezoneOffset &tzo)
    {
        return QXmppUtils::timezoneOffsetToString(tzo.seconds);
    }
};

///
/// Returns the timezone offset in seconds.
///
int QXmppEntityTimeIq::tzo() const
{
    return m_tzo;
}

///
/// Sets the timezone offset in seconds.
///
/// \param tzo
///
void QXmppEntityTimeIq::setTzo(int tzo)
{
    m_tzo = tzo;
}

///
/// Returns the date/time in Coordinated Universal Time (UTC).
///
QDateTime QXmppEntityTimeIq::utc() const
{
    return m_utc;
}

///
/// Sets the date/time in Coordinated Universal Time (UTC).
///
/// \param utc
///
void QXmppEntityTimeIq::setUtc(const QDateTime &utc)
{
    m_utc = utc;
}

/// \cond
void QXmppEntityTimeIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement timeElement = firstChildElement(element, u"time");
    m_tzo = QXmppUtils::timezoneOffsetFromString(firstChildElement(timeElement, u"tzo").text());
    m_utc = QXmppUtils::datetimeFromString(firstChildElement(timeElement, u"utc").text());
}

void QXmppEntityTimeIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        { u"time", ns_entity_time },
        OptionalContent {
            m_utc.isValid(),
            TextElement { u"tzo", TimezoneOffset { m_tzo } },
            TextElement { u"utc", m_utc },
        },
    });
}
/// \endcond

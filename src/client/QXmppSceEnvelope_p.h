// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

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

#ifndef QXMPPSCEENVELOPE_P_H
#define QXMPPSCEENVELOPE_P_H

#include "QXmppUtils.h"

#include <chrono>
#include <cstdlib>

#include <QDateTime>
#include <QDomElement>

// Maximum difference between the time affix and the time a stanza has been sent (XEP-0420). Larger
// differences are caused by replays of old stanzas or by misconfigured clocks.
constexpr std::chrono::hours SCE_TIME_TOLERANCE { 24 };

// Whether the time affix matches the time the stanza has been sent. A missing affix matches.
inline bool isSceTimeValid(const QDateTime &sceTime, const QDateTime &sendTime)
{
    return !sceTime.isValid() || std::abs(sceTime.secsTo(sendTime)) <= std::chrono::seconds(SCE_TIME_TOLERANCE).count();
}

class QXmppSceEnvelopeReader
{
public:
    QXmppSceEnvelopeReader(QDomElement &&element)
        : element(std::move(element))
    {
    }

    inline QDomElement contentElement()
    {
        return element.firstChildElement(QStringLiteral("content"));
    }
    inline QString from()
    {
        return element.firstChildElement(QStringLiteral("from")).attribute(QStringLiteral("jid"));
    }
    inline QString to()
    {
        return element.firstChildElement(QStringLiteral("to")).attribute(QStringLiteral("jid"));
    }
    inline QDateTime timestamp()
    {
        return QXmppUtils::datetimeFromString(
            element.firstChildElement(QStringLiteral("time")).attribute(QStringLiteral("stamp")));
    }

    // rpad is usually not needed (but can be parsed manually if really needed)

private:
    QDomElement element;
};

class QXmppSceEnvelopeWriter
{
public:
    QXmppSceEnvelopeWriter(QXmlStreamWriter &writer)
        : writer(writer)
    {
    }

    inline void start()
    {
        writer.writeStartElement(QStringLiteral("envelope"));
        writer.writeDefaultNamespace(QStringLiteral("urn:xmpp:sce:1"));
    }
    inline void end()
    {
        writer.writeEndElement();
    }
    template<typename Functor>
    void writeContent(Functor writeContent)
    {
        writer.writeStartElement(QStringLiteral("content"));
        writeContent();
        writer.writeEndElement();
    }
    inline void writeFrom(const QString &jid)
    {
        writer.writeStartElement(QStringLiteral("from"));
        writer.writeAttribute(QStringLiteral("jid"), jid);
        writer.writeEndElement();
    }
    inline void writeTo(const QString &jid)
    {
        writer.writeStartElement(QStringLiteral("to"));
        writer.writeAttribute(QStringLiteral("jid"), jid);
        writer.writeEndElement();
    }
    inline void writeTimestamp(const QDateTime &timestamp)
    {
        writer.writeStartElement(QStringLiteral("time"));
        writer.writeAttribute(QStringLiteral("stamp"), QXmppUtils::datetimeToString(timestamp));
        writer.writeEndElement();
    }
    inline void writeRpad(const QString &value)
    {
        writer.writeStartElement(QStringLiteral("rpad"));
        writer.writeCharacters(value);
        writer.writeEndElement();
    }

private:
    QXmlStreamWriter &writer;
};

#endif  // QXMPPSCEENVELOPE_P_H

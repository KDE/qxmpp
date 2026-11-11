// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppIqStanza.h"

#include "QXmppConstants_p.h"

#include "Enums.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QXmlStreamWriter>

/*!
    \namespace QXmpp::Iq
    \inmodule QXmpp

    Typed IQ stanzas and the handling of incoming IQ requests.

    \since QXmpp 1.18
*/

/*!
    \typealias QXmpp::Iq::Get
    \relates QXmpp::Iq::Stanza

    IQ request of type get with the payload \c Payload.
*/

/*!
    \typealias QXmpp::Iq::Set
    \relates QXmpp::Iq::Stanza

    IQ request of type set with the payload \c Payload.
*/

/*!
    \typealias QXmpp::Iq::Result
    \relates QXmpp::Iq::Stanza

    IQ response of type result with the payload \c Payload, by default without payload.
*/

/*!
    \typealias QXmpp::Iq::Error
    \relates QXmpp::Iq::Stanza

    IQ response of type error, optionally with the payload \c Payload of the request.
*/

namespace QXmpp::Private {

template<>
struct Enums::Data<Iq::Type> {
    using enum Iq::Type;
    static constexpr auto Values = makeValues<Iq::Type>({
        { Get, u"get" },
        { Set, u"set" },
        { Result, u"result" },
        { Error, u"error" },
    });
};

void writeIqStart(QXmlStreamWriter *writer, Iq::Type type, const QString &id, const QString &from, const QString &to, const QString &lang)
{
    Q_ASSERT_X(!id.isEmpty(), "QXmpp::Iq::Stanza::toXml()", "IQs require an id");

    writer->writeStartElement(u"iq"_s);
    writer->writeAttribute(u"id"_s, id);
    if (!from.isEmpty()) {
        writer->writeAttribute(u"from"_s, from);
    }
    if (!to.isEmpty()) {
        writer->writeAttribute(u"to"_s, to);
    }
    writer->writeAttribute(u"type"_s, Enums::toString(type));
    if (!lang.isEmpty()) {
        writer->writeAttribute(u"xml:lang"_s, lang);
    }
}

void writeIqEnd(QXmlStreamWriter *writer)
{
    writer->writeEndElement();
}

void writeXmlWriterPayload(QXmlStreamWriter *writer, const void *payload, void (*write)(XmlWriter &, const void *))
{
    XmlWriter w(writer);
    write(w, payload);
}

// Checks the type, the id and the child elements according to RFC 6120, section 8.2.3.
bool isIq(const QDomElement &element, Iq::Type type)
{
    if (element.tagName() != u"iq" || element.attribute(u"id"_s).isEmpty() || element.attribute(u"type"_s) != Enums::toStringView(type)) {
        return false;
    }

    auto children = 0;
    for (auto child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        children++;
    }

    switch (type) {
    case Iq::Type::Get:
    case Iq::Type::Set:
        // exactly one payload
        return children == 1;
    case Iq::Type::Result:
        // zero or one payload
        return children <= 1;
    case Iq::Type::Error:
        // optionally the payload of the request and the error
        return children <= 2 && element.lastChildElement().tagName() == u"error";
    }
    Q_UNREACHABLE();
}

}  // namespace QXmpp::Private

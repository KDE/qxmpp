// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppIq.h"

#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppIq::Type> {
    using enum QXmppIq::Type;
    static constexpr auto Values = makeValues<QXmppIq::Type>({
        { Error, u"error" },
        { Get, u"get" },
        { Set, u"set" },
        { Result, u"result" },
    });
};

class QXmppIqPrivate : public QSharedData
{
public:
    QXmppIq::Type type;
};

///
/// Constructs a QXmppIq with the specified \a type.
///
/// \param type
///
QXmppIq::QXmppIq(QXmppIq::Type type)
    : QXmppStanza(), d(new QXmppIqPrivate)
{
    d->type = type;
    generateAndSetNextId();
}

/// Constructs a copy of \a other.
QXmppIq::QXmppIq(const QXmppIq &other) = default;
/// Default move-constructor.
QXmppIq::QXmppIq(QXmppIq &&) = default;
QXmppIq::~QXmppIq() = default;

/// Assigns \a other to this IQ.
QXmppIq &QXmppIq::operator=(const QXmppIq &other) = default;
/// Move-assignment operator.
QXmppIq &QXmppIq::operator=(QXmppIq &&) = default;

///
/// Returns the IQ's type.
///
QXmppIq::Type QXmppIq::type() const
{
    return d->type;
}

///
/// Sets the IQ's type.
///
/// \param type
///
void QXmppIq::setType(QXmppIq::Type type)
{
    d->type = type;
}

///
/// Indicates if the QXmppStanza is a stanza in the XMPP sense (i. e. a message,
/// iq or presence)
///
/// \since QXmpp 1.0
///
bool QXmppIq::isXmppStanza() const
{
    return true;
}

/// \cond
void QXmppIq::parse(const QDomElement &element)
{
    QXmppStanza::parse(element);

    d->type = Enums::fromString<Type>(element.attribute(u"type"_s)).value_or(Get);

    parseElementFromChild(element);
}

void QXmppIq::parseElementFromChild(const QDomElement &element)
{
    QXmppElementList extensions;

    for (const auto &itemElement : iterChildElements(element)) {
        extensions.append(QXmppElement(itemElement));
    }
    setExtensions(extensions);
}

void QXmppIq::toXml(QXmlStreamWriter *w) const
{
    XmlWriter(w).write(Element {
        u"iq",
        Attribute { u"id", id() },
        OptionalAttribute { u"to", to() },
        OptionalAttribute { u"from", from() },
        Attribute { u"type", d->type },
        [&] { toXmlElementFromChild(w); },
        errorOptional(),
    });
}

void QXmppIq::toXmlElementFromChild(QXmlStreamWriter *w) const
{
    XmlWriter(w).write(extensions());
}
/// \endcond

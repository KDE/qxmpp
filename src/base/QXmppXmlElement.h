// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLELEMENT_H
#define QXMPPXMLELEMENT_H

#include "QXmppGlobal.h"

#include <optional>
#include <vector>

#include <QString>
#include <QStringView>

class QDomElement;

namespace QXmpp::Private {
class XmlWriter;
}

namespace QXmpp::Xml {

/*!
    \namespace QXmpp::Xml
    \inmodule QXmpp

    Contains types for generic and dynamically-registered XML extensions.

    \since QXmpp 1.16
*/

/*!
    \inmodule QXmpp

    \brief A single XML attribute (name-value pair).

    \since QXmpp 1.16
*/
struct QXMPP_EXPORT Attribute {
    QString name;
    QString value;

    bool operator==(const Attribute &) const = default;
};

/*!
    \inmodule QXmpp

    \brief A generic, recursive representation of an XML element.

    Element holds a tag name, an optional XML namespace, a list of attributes, a
    list of child elements and a simple text content. It is a value type meant
    to eventually replace the older QXmppElement.

    Mixed content (text interleaved with child elements) is not supported; only a
    single text value is kept.

    \since QXmpp 1.16
*/
class QXMPP_EXPORT Element
{
public:
    Element() = default;
    explicit Element(QString tag, QString xmlns = {});

    const QString &tag() const;
    void setTag(QString tag);

    const QString &xmlns() const;
    void setXmlns(QString xmlns);

    const QString &text() const;
    void setText(QString text);

    const std::vector<Attribute> &attributes() const;
    void setAttributes(std::vector<Attribute> attributes);
    std::optional<QString> attribute(QStringView name) const;
    void setAttribute(const QString &name, const QString &value);
    void removeAttribute(QStringView name);

    const std::vector<Element> &children() const;
    void setChildren(std::vector<Element> children);
    void addChild(Element child);

    static Element fromDom(const QDomElement &el);
    void toXml(QXmpp::Private::XmlWriter &writer) const;

    bool operator==(const Element &) const = default;

private:
    QString m_tag;
    QString m_xmlns;
    QString m_text;
    std::vector<Attribute> m_attributes;
    std::vector<Element> m_children;
};

}  // namespace QXmpp::Xml

#endif  // QXMPPXMLELEMENT_H

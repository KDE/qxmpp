// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppXmlElement.h"

#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

namespace QXmpp::Xml {

/*! Constructs an element with the given tag name \a tag and (optional) namespace \a xmlns. */
Element::Element(QString tag, QString xmlns)
    : m_tag(std::move(tag)), m_xmlns(std::move(xmlns))
{
}

/*! Returns the tag name of the element. */
const QString &Element::tag() const
{
    return m_tag;
}

/*! Sets the tag name of the element to \a tag. */
void Element::setTag(QString tag)
{
    m_tag = std::move(tag);
}

/*! Returns the XML namespace of the element (may be empty). */
const QString &Element::xmlns() const
{
    return m_xmlns;
}

/*! Sets the XML namespace of the element to \a xmlns. */
void Element::setXmlns(QString xmlns)
{
    m_xmlns = std::move(xmlns);
}

/*! Returns the text content of the element. */
const QString &Element::text() const
{
    return m_text;
}

/*! Sets the text content of the element to \a text. */
void Element::setText(QString text)
{
    m_text = std::move(text);
}

/*! Returns the list of attributes. */
const std::vector<Attribute> &Element::attributes() const
{
    return m_attributes;
}

/*! Replaces all attributes with \a attributes. */
void Element::setAttributes(std::vector<Attribute> attributes)
{
    m_attributes = std::move(attributes);
}

/*! Returns the value of the attribute named \a name, if it exists. */
std::optional<QString> Element::attribute(QStringView name) const
{
    for (const auto &attribute : m_attributes) {
        if (attribute.name == name) {
            return attribute.value;
        }
    }
    return {};
}

/*! Sets the attribute \a name to \a value, replacing an existing one with the same name. */
void Element::setAttribute(const QString &name, const QString &value)
{
    for (auto &attribute : m_attributes) {
        if (attribute.name == name) {
            attribute.value = value;
            return;
        }
    }
    m_attributes.push_back({ name, value });
}

/*! Removes the attribute named \a name, if it exists. */
void Element::removeAttribute(QStringView name)
{
    std::erase_if(m_attributes, [name](const Attribute &attribute) {
        return attribute.name == name;
    });
}

/*! Returns the list of child elements. */
const std::vector<Element> &Element::children() const
{
    return m_children;
}

/*! Replaces all child elements with \a children. */
void Element::setChildren(std::vector<Element> children)
{
    m_children = std::move(children);
}

/*! Appends the child element \a child. */
void Element::addChild(Element child)
{
    m_children.push_back(std::move(child));
}

/*!
    Recursively builds an Element from the QDomElement \a el.

    The DOM element is expected to be parsed with namespace processing enabled.
*/
Element Element::fromDom(const QDomElement &el)
{
    Element result;
    if (el.isNull()) {
        return result;
    }

    result.m_tag = el.tagName();
    result.m_xmlns = el.namespaceURI();

    const auto attributes = el.attributes();
    for (int i = 0; i < attributes.size(); ++i) {
        const auto attr = attributes.item(i).toAttr();
        result.m_attributes.push_back({ attr.name(), attr.value() });
    }

    for (auto node = el.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.isElement()) {
            result.m_children.push_back(fromDom(node.toElement()));
        } else if (node.isText()) {
            result.m_text += node.toText().data();
        }
    }

    return result;
}

/*! Recursively serializes the element to \a writer. */
void Element::toXml(XmlWriter &writer) const
{
    writer.write(QXmpp::Private::Element {
        std::tuple { m_tag, m_xmlns },
        [&] {
            for (const auto &attribute : m_attributes) {
                writer.write(QXmpp::Private::Attribute { attribute.name, attribute.value });
            }
        },
        OptionalCharacters { m_text },
        m_children,
    });
}

}  // namespace QXmpp::Xml

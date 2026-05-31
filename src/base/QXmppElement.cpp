// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppElement.h"

#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QTextStream>

using namespace QXmpp::Private;

class QXmppElementPrivate
{
public:
    QXmppElementPrivate() = default;
    QXmppElementPrivate(const QDomElement &element);
    ~QXmppElementPrivate();

    QAtomicInt counter = 1;

    QXmppElementPrivate *parent = nullptr;
    QMap<QString, QString> attributes;
    QList<QXmppElementPrivate *> children;
    QString name;
    QString value;

    QByteArray serializedSource;
};

QXmppElementPrivate::QXmppElementPrivate(const QDomElement &element)
{
    if (element.isNull()) {
        return;
    }

    name = element.tagName();
    QString xmlns = element.namespaceURI();
    QString parentns = element.parentNode().namespaceURI();
    if (!xmlns.isEmpty() && xmlns != parentns) {
        attributes.insert(u"xmlns"_s, xmlns);
    }
    QDomNamedNodeMap attrs = element.attributes();
    for (int i = 0; i < attrs.size(); i++) {
        QDomAttr attr = attrs.item(i).toAttr();
        attributes.insert(attr.name(), attr.value());
    }

    for (auto childNode = element.firstChild();
         !childNode.isNull();
         childNode = childNode.nextSibling()) {
        if (childNode.isElement()) {
            QXmppElementPrivate *child = new QXmppElementPrivate(childNode.toElement());
            child->parent = this;
            children.append(child);
        } else if (childNode.isText()) {
            value += childNode.toText().data();
        }
    }

    QTextStream stream(&serializedSource);
    element.save(stream, 0);
}

QXmppElementPrivate::~QXmppElementPrivate()
{
    for (auto *child : std::as_const(children)) {
        if (!child->counter.deref()) {
            delete child;
        }
    }
}

/*!
    \class QXmppElement
    \inmodule QXmpp

    QXmppElement represents a raw XML element with possible children.
*/

/*! Default constructor */
QXmppElement::QXmppElement()
{
    d = new QXmppElementPrivate();
}

/*! Copy constructor */
QXmppElement::QXmppElement(const QXmppElement &other)
{
    other.d->counter.ref();
    d = other.d;
}

QXmppElement::QXmppElement(QXmppElementPrivate *other)
{
    other->counter.ref();
    d = other;
}

/*! Copy-construct DOM element contents */
QXmppElement::QXmppElement(const QDomElement &element)
{
    d = new QXmppElementPrivate(element);
}

QXmppElement::~QXmppElement()
{
    if (!d->counter.deref()) {
        delete d;
    }
}

/*! Assignment operator */
QXmppElement &QXmppElement::operator=(const QXmppElement &other)
{
    // self-assignment check
    if (this != &other) {
        other.d->counter.ref();
        if (!d->counter.deref()) {
            delete d;
        }
        d = other.d;
    }
    return *this;
}

/*!
    Creates a DOM element from the source element

    The source DOM element is saved as XML and needs to be parsed again in this
    step.
*/
QDomElement QXmppElement::sourceDomElement() const
{
    if (d->serializedSource.isEmpty()) {
        return QDomElement();
    }

    QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (!doc.setContent(d->serializedSource, QDomDocument::ParseOption::UseNamespaceProcessing)) {
#else
    if (!doc.setContent(d->serializedSource, true)) {
#endif
        qWarning("[QXmpp] QXmppElement::sourceDomElement(): cannot parse source element");
        return QDomElement();
    }

    return doc.documentElement();
}

/*! Returns the list of attributes */
QStringList QXmppElement::attributeNames() const
{
    return d->attributes.keys();
}

/*! Returns an attribute by \a name */
QString QXmppElement::attribute(const QString &name) const
{
    return d->attributes.value(name);
}

/*!
    Sets an attribute

    \a name and \a value.
*/
void QXmppElement::setAttribute(const QString &name, const QString &value)
{
    d->attributes.insert(name, value);
}

/*! Adds a \a child element */
void QXmppElement::appendChild(const QXmppElement &child)
{
    if (child.d->parent == d) {
        return;
    }

    if (child.d->parent) {
        child.d->parent->children.removeAll(child.d);
    } else {
        child.d->counter.ref();
    }
    child.d->parent = d;
    d->children.append(child.d);
}

/*!
    Returns the first child element with the given \a name or the first child
    element if the given \a name is empty.
*/
QXmppElement QXmppElement::firstChildElement(const QString &name) const
{
    for (auto *child_d : std::as_const(d->children)) {
        if (name.isEmpty() || child_d->name == name) {
            return QXmppElement(child_d);
        }
    }
    return QXmppElement();
}

/*!
    Returns the next sibling element with the given name or the next sibling
    element if the given name is empty.

    \a name.
*/
QXmppElement QXmppElement::nextSiblingElement(const QString &name) const
{
    if (!d->parent) {
        return QXmppElement();
    }
    const QList<QXmppElementPrivate *> &siblings_d = d->parent->children;
    for (qsizetype i = siblings_d.indexOf(d) + 1; i < siblings_d.size(); i++) {
        if (name.isEmpty() || siblings_d[i]->name == name) {
            return QXmppElement(siblings_d[i]);
        }
    }
    return QXmppElement();
}

/*! Returns true if the element is null */
bool QXmppElement::isNull() const
{
    return d->name.isEmpty();
}

/*! Removes a \a child element */
void QXmppElement::removeChild(const QXmppElement &child)
{
    if (child.d->parent != d) {
        return;
    }

    d->children.removeAll(child.d);
    child.d->counter.deref();
    child.d->parent = nullptr;
}

/*! Returns the tag name of the element */
QString QXmppElement::tagName() const
{
    return d->name;
}

/*!
    Sets the tag name of the element

    \a tagName.
*/
void QXmppElement::setTagName(const QString &tagName)
{
    d->name = tagName;
}

/*! Returns the text content of the element */
QString QXmppElement::value() const
{
    return d->value;
}

/*!
    Sets the text content of the element

    \a value.
*/
void QXmppElement::setValue(const QString &value)
{
    d->value = value;
}

/*!
    Serializes the element to XML

    \a writer.
*/
void QXmppElement::toXml(QXmlStreamWriter *writer) const
{
    using std::ranges::transform_view;
    if (isNull()) {
        return;
    }

    XmlWriter w(writer);
    w.write(Element {
        d->name,
        [&] {
            std::for_each(d->attributes.keyValueBegin(), d->attributes.keyValueEnd(), [&](auto pair) {
                w.write(Attribute { pair.first, pair.second });
            });
        },
        OptionalCharacters { d->value },
        transform_view(d->children, [](auto d) { return QXmppElement(d); }),
    });
}

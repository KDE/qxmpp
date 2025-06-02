// SPDX-FileCopyrightText: 2009 Ian Reinhart Geiser <geiseri@kde.org>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppRpcIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <ranges>

#include <QDateTime>
#include <QDomElement>
#include <QMap>
#include <QStringList>
#include <QVariant>

using namespace QXmpp::Private;
using std::ranges::transform_view;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using MetaType = QMetaType::Type;
#else
using MetaType = QVariant::Type;
#endif

struct RpcValue {
    const QVariant &value;

    static RpcValue fromVariant(const QVariant &v) { return RpcValue { v }; }
    void toXml(QXmlStreamWriter *writer) const
    {
        QXmppRpcMarshaller::marshall(writer, value);
    }
};

struct RpcList {
    const QVariantList &list;

    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"array",
            Element {
                u"data",
                transform_view(list, &RpcValue::fromVariant),
            },
        });
    }
};

struct RpcMap {
    const QMap<QString, QVariant> &map;

    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"struct",
            [&] {
                for (auto it = map.begin(); it != map.end(); ++it) {
                    w.write(Element {
                        u"member",
                        TextElement { u"name", it.key() },
                        RpcValue { it.value() },
                    });
                }
            },
        });
    }
};

struct RpcParams {
    const QVariantList &params;

    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"params",
            [&] {
                for (const auto &value : params) {
                    w.write(Element { u"param", RpcValue { value } });
                }
            },
        });
    }
};

void QXmppRpcMarshaller::marshall(QXmlStreamWriter *writer, const QVariant &value)
{
    XmlWriter w(writer);

    writer->writeStartElement(u"value"_s);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    switch (value.typeId()) {
#else
    switch (value.type()) {
#endif
    case MetaType::Int:
    case MetaType::UInt:
    case MetaType::LongLong:
    case MetaType::ULongLong:
        w.write(TextElement { u"i4", value.toString() });
        break;
    case MetaType::Double:
        w.write(TextElement { u"double", value.toString() });
        break;
    case MetaType::Bool:
        w.write(TextElement { u"boolean", value.toBool() });
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QDate:
#else
    case QVariant::Date:
#endif
        w.write(TextElement { u"dateTime.iso8601", value.toDate().toString(Qt::ISODate) });
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QDateTime:
#else
    case QVariant::DateTime:
#endif
        w.write(TextElement { u"dateTime.iso8601", value.toDateTime() });
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QTime:
#else
    case QVariant::Time:
#endif
        w.write(TextElement { u"dateTime.iso8601", value.toTime().toString(Qt::ISODate) });
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QStringList:
    case QMetaType::Type::QVariantList: {
#else
    case QVariant::StringList:
    case QVariant::List: {
#endif
        w.write(RpcList { value.toList() });
        break;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QVariantMap: {
#else
    case QVariant::Map: {
#endif
        w.write(RpcMap { value.toMap() });
        break;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QMetaType::Type::QByteArray: {
#else
    case QVariant::ByteArray: {
#endif
        w.write(TextElement { u"base64", Base64 { value.toByteArray() } });
        break;
    }
    default: {
        if (value.isNull()) {
            w.write(Element { u"nil" });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        } else if (value.canConvert(QMetaType(QMetaType::Type::QString))) {
#else
        } else if (value.canConvert(QVariant::String)) {
#endif
            w.write(Element {
                u"string",
                [&] {
                    const auto string = value.toString();
                    if (std::ranges::all_of(string, [](auto c) { return c.isSpace(); })) {
                        // strings with only whitespace get stripped by some parsers
                        writer->writeCDATA(string);
                    } else {
                        writer->writeCharacters(string);
                    }
                },
            });
        }
        break;
    }
    }
    writer->writeEndElement();
}

QVariant QXmppRpcMarshaller::demarshall(const QDomElement &elem, QStringList &errors)
{
    if (elem.tagName().toLower() != u"value") {
        errors << u"Bad param value"_s;
        return QVariant();
    }

    if (!elem.firstChild().isElement()) {
        return QVariant(elem.text());
    }

    const QDomElement typeData = elem.firstChild().toElement();
    const QString typeName = typeData.tagName().toLower();

    if (typeName == u"nil") {
        return QVariant();
    }
    if (typeName == u"string") {
        return QVariant(typeData.text());
    } else if (typeName == u"int" || typeName == u"i4") {
        bool ok = false;
        QVariant val(typeData.text().toInt(&ok));
        if (ok) {
            return val;
        }
        errors << u"I was looking for an integer but data was courupt"_s;
        return QVariant();
    } else if (typeName == u"double") {
        bool ok = false;
        QVariant val(typeData.text().toDouble(&ok));
        if (ok) {
            return val;
        }
        errors << u"I was looking for an double but data was corrupt"_s;
    } else if (typeName == u"boolean") {
        return QVariant(typeData.text() == u"1" || typeData.text().toLower() == u"true");
    } else if (typeName == u"datetime" || typeName == u"datetime.iso8601") {
        return QVariant(QDateTime::fromString(typeData.text(), Qt::ISODate));
    } else if (typeName == u"array") {
        QVariantList arr;
        QDomElement valueNode = firstChildElement(typeData, u"data").firstChildElement();
        while (!valueNode.isNull() && errors.isEmpty()) {
            arr.append(demarshall(valueNode, errors));
            valueNode = valueNode.nextSiblingElement();
        }
        return QVariant(arr);
    } else if (typeName == u"struct") {
        QMap<QString, QVariant> stct;
        QDomNode valueNode = typeData.firstChild();
        while (!valueNode.isNull() && errors.isEmpty()) {
            const QDomElement memberNode = valueNode.toElement().elementsByTagName(u"name"_s).item(0).toElement();
            const QDomElement dataNode = valueNode.toElement().elementsByTagName(u"value"_s).item(0).toElement();
            stct[memberNode.text()] = demarshall(dataNode, errors);
            valueNode = valueNode.nextSibling();
        }
        return QVariant(stct);
    } else if (typeName == u"base64") {
        QByteArray src = typeData.text().toLatin1();
        return QVariant(QByteArray::fromBase64(src));
    }

    errors << u"Cannot handle type %1"_s.arg(typeName);
    return QVariant();
}

QXmppRpcErrorIq::QXmppRpcErrorIq() : QXmppIq(QXmppIq::Error)
{
}

QXmppRpcInvokeIq QXmppRpcErrorIq::query() const
{
    return m_query;
}

void QXmppRpcErrorIq::setQuery(const QXmppRpcInvokeIq &query)
{
    m_query = query;
}

/// \cond
bool QXmppRpcErrorIq::isRpcErrorIq(const QDomElement &element)
{
    QString type = element.attribute(u"type"_s);
    QDomElement errorElement = element.firstChildElement(u"error"_s);
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    return type == u"error" &&
        !errorElement.isNull() &&
        queryElement.namespaceURI() == ns_rpc;
}

void QXmppRpcErrorIq::parseElementFromChild(const QDomElement &element)
{
    m_query.parseElementFromChild(element);
}

void QXmppRpcErrorIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    m_query.toXmlElementFromChild(writer);
}
/// \endcond

QXmppRpcResponseIq::QXmppRpcResponseIq()
    : QXmppIq(QXmppIq::Result),
      m_faultCode(0)
{
}

/// Returns the fault code.
int QXmppRpcResponseIq::faultCode() const
{
    return m_faultCode;
}

/// Sets the fault code.
void QXmppRpcResponseIq::setFaultCode(int faultCode)
{
    m_faultCode = faultCode;
}

/// Returns the fault string.
QString QXmppRpcResponseIq::faultString() const
{
    return m_faultString;
}

/// Sets the fault string.
void QXmppRpcResponseIq::setFaultString(const QString &faultString)
{
    m_faultString = faultString;
}

/// Returns the response values.
QVariantList QXmppRpcResponseIq::values() const
{
    return m_values;
}

/// Sets the response values.
void QXmppRpcResponseIq::setValues(const QVariantList &values)
{
    m_values = values;
}

/// \cond
bool QXmppRpcResponseIq::isRpcResponseIq(const QDomElement &element)
{
    QString type = element.attribute(u"type"_s);
    QDomElement dataElement = element.firstChildElement(u"query"_s);
    return dataElement.namespaceURI() == ns_rpc &&
        type == u"result";
}

void QXmppRpcResponseIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    QDomElement methodElement = queryElement.firstChildElement(u"methodResponse"_s);

    const QDomElement contents = methodElement.firstChildElement();
    if (contents.tagName().toLower() == u"params") {
        for (const auto &param : iterChildElements(contents, u"param")) {
            QStringList errors;
            const QVariant value = QXmppRpcMarshaller::demarshall(param.firstChildElement(u"value"_s), errors);
            if (!errors.isEmpty()) {
                break;
            }
            m_values << value;
        }
    } else if (contents.tagName().toLower() == u"fault") {
        QStringList errors;
        const QDomElement errElement = contents.firstChildElement(u"value"_s);
        const QVariant error = QXmppRpcMarshaller::demarshall(errElement, errors);
        if (!errors.isEmpty()) {
            return;
        }
        m_faultCode = error.toMap()[u"faultCode"_s].toInt();
        m_faultString = error.toMap()[u"faultString"_s].toString();
    }
}

void QXmppRpcResponseIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"query", ns_rpc },
        Element {
            u"methodResponse",
            [&] {
                if (m_faultCode) {
                    QVariantMap fault;
                    fault[u"faultCode"_s] = m_faultCode;
                    fault[u"faultString"_s] = m_faultString;

                    w.write(Element { u"fault", Element { u"value", RpcMap { fault } } });
                } else if (!m_values.isEmpty()) {
                    w.write(RpcParams { m_values });
                }
            },
        },
    });
}
/// \endcond

QXmppRpcInvokeIq::QXmppRpcInvokeIq()
    : QXmppIq(QXmppIq::Set)
{
}

/// Returns the method arguments.
QVariantList QXmppRpcInvokeIq::arguments() const
{
    return m_arguments;
}

/// Sets the method arguments.
void QXmppRpcInvokeIq::setArguments(const QVariantList &arguments)
{
    m_arguments = arguments;
}

/// Returns the method name.
///

QString QXmppRpcInvokeIq::method() const
{
    return m_method;
}

/// Sets the method name.
void QXmppRpcInvokeIq::setMethod(const QString &method)
{
    m_method = method;
}

/// \cond
bool QXmppRpcInvokeIq::isRpcInvokeIq(const QDomElement &element)
{
    QString type = element.attribute(u"type"_s);
    QDomElement dataElement = element.firstChildElement(u"query"_s);
    return dataElement.namespaceURI() == ns_rpc &&
        type == u"set";
}

void QXmppRpcInvokeIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    QDomElement methodElement = queryElement.firstChildElement(u"methodCall"_s);

    m_method = methodElement.firstChildElement(u"methodName"_s).text();

    const QDomElement methodParams = methodElement.firstChildElement(u"params"_s);
    m_arguments.clear();
    if (!methodParams.isNull()) {
        QDomNode param = methodParams.firstChildElement(u"param"_s);
        while (!param.isNull()) {
            QStringList errors;
            QVariant arg = QXmppRpcMarshaller::demarshall(param.firstChildElement(u"value"_s), errors);
            if (!errors.isEmpty()) {
                break;
            }
            m_arguments << arg;
            param = param.nextSiblingElement(u"param"_s);
        }
    }
}

void QXmppRpcInvokeIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        { u"query", ns_rpc },
        Element {
            u"methodCall",
            TextElement { u"methodName", m_method },
            OptionalContent { !m_arguments.isEmpty(), RpcParams { m_arguments } },
        },
    });
}
/// \endcond

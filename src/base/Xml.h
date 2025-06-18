// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XML_H
#define XML_H

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "XmlWriter.h"

namespace QXmpp::Private {

//
// General parsing errors
//

struct ParsingError : public std::runtime_error {
    ParsingError(const std::string &e) : std::runtime_error(e) { }
};

struct MissingAttributeError : public ParsingError {
    MissingAttributeError(QStringView attributeName, const QDomElement &el)
        : ParsingError(std::format("Missing required attribute '{}' in <{}/> ({})",
                                   attributeName.toUtf8().toStdString(),
                                   el.tagName().toStdString(),
                                   el.namespaceURI().toStdString()))
    {
    }
};

struct MissingElementError : ParsingError {
    MissingElementError(const QDomElement &el, QStringView name, QStringView xmlns = {})
        : ParsingError(std::format("Missing required element <{}/> ({}) in parent <{}/> ({})",
                                   name.toUtf8().toStdString(),
                                   xmlns.toUtf8().toStdString(),
                                   el.tagName().toStdString(),
                                   el.namespaceURI().toStdString())) { }

    template<IsXmlTag Tag>
    MissingElementError(const QDomElement &el, Tag tag)
        : MissingElementError(
              el, [&] { auto &[name, _] = tag; return name; }(),
              [&] { auto &[_, xmlns] = tag; return xmlns; }()) { }
};

struct UnknownParsingError : ParsingError {
    UnknownParsingError(QStringView elementName, std::string_view typeName)
        : ParsingError(std::format("Could not parse <{}/> into {}.",
                                   elementName.toUtf8().toStdString(), typeName))
    {
    }
};

struct InvalidValueError : ParsingError {
    InvalidValueError(std::string_view typeName, QStringView value)
        : ParsingError(std::format("Invalid value of '{}' encountered for type '{}'",
                                   value.toUtf8().toStdString(), typeName))
    {
    }
    InvalidValueError(std::type_info type, QStringView value)
        : InvalidValueError(type.name(), value)
    {
    }
};

//
// String serialization/deserialization
//

template<typename T>
struct StringSerDe;

template<typename T>
struct StringSerDe<std::optional<T>> {
    auto parse(const QString &s) const { return std::optional<T>(StringSerDe<T>().parse(s)); }
    auto serialize(const std::optional<T> &o) const { return StringSerDe<T>().serialize(o.value()); }
    bool hasValue(const std::optional<T> &o) const { return o.has_value(); }
    auto defaultValue() const { return std::optional<T>(); }
};

template<>
struct StringSerDe<QString> {
    QString parse(QString &&s) const { return std::move(s); }
    const QString &serialize(const QString &s) const { return s; }
    bool hasValue(const QString &s) const { return !s.isEmpty(); }
    QString defaultValue() const { return {}; }
};

template<std::integral Int>
struct StringSerDe<Int> {
    Int parse(const QString &s) const
    {
        if (auto value = parseInt<Int>(s)) {
            return *value;
        }
        throw InvalidValueError(typeid(Int).name(), s);
    }
    QString serialize(Int value) const
    {
        return QString::number(value);
    }
};

struct PositiveIntSerializer {
    uint64_t parse(QStringView s) const
    {
        if (auto i = parseInt<uint64_t>(s)) {
            if (i != 0) {
                return *i;
            }
        }
        throw InvalidValueError("positiveInteger<uint64>", s);
    }
    QString serialize(uint64_t i) const { return QString::number(i); }
    bool hasValue(uint64_t i) const { return i != 0; }
    uint64_t defaultValue() const { return 0; }
};

template<typename Enum>
    requires Enums::SerializableEnum<Enum>
struct StringSerDe<Enum> {
    Enum parse(const QString &s) const
    {
        auto e = Enums::fromString<Enum>(s);
        if (!e) {
            throw InvalidValueError(typeid(Enum).name(), s);
        }
        return *e;
    }
    auto serialize(Enum value) const { return Enums::toString(value); }
    bool hasValue(Enum value) const
        requires Enums::NullableEnum<Enum>
    {
        return value != Enums::Data<Enum>::NullValue;
    }
    Enum defaultValue() const
        requires Enums::NullableEnum<Enum>
    {
        return Enums::Data<Enum>::NullValue;
    }
};

template<>
struct StringSerDe<bool> {
    bool parse(const QString &s)
    {
        if (s == u"true" || s == u"1") {
            return true;
        }
        if (s == u"false" || s == u"0") {
            return false;
        }
        throw InvalidValueError("bool", s);
    }
    auto serialize(bool b) { return b ? u"true"_s : u"false"_s; }
};

template<>
struct StringSerDe<QDateTime> {
    QDateTime parse(QStringView s) const
    {
        return QDateTime::fromString(toString60(s), Qt::ISODate).toUTC();
    }
    QString serialize(const QDateTime &datetime) const
    {
        if (datetime.time().msec() != 0) {
            return datetime.toString(Qt::ISODateWithMs);
        }
        return datetime.toString(Qt::ISODate);
    }
    bool hasValue(const QDateTime &datetime) const { return datetime.isValid(); }
    auto defaultValue() const { return QDateTime(); }
};

template<>
struct StringSerDe<QUrl> {
    QUrl parse(const QString &s) const { return QUrl(s); }
    QString serialize(const QUrl &url) const { return url.toString(); }
    bool hasValue(const QUrl &url) const { return url.isValid(); }
    auto defaultValue() const { return QUrl(); }
};

template<>
struct StringSerDe<QUuid> {
    QUuid parse(QStringView s) const
    {
        auto uuid = QUuid::fromString(s);
        if (uuid.isNull()) {
            throw InvalidValueError("QUuid", s);
        }
        return uuid;
    }
    QString serialize(QUuid uuid) const
    {
        return uuid.toString(QUuid::WithoutBraces);
    }
};

struct Base64Serializer {
    QByteArray parse(const QString &s) const
    {
        auto result = QByteArray::fromBase64Encoding(s.toUtf8());
        if (!result) {
            throw InvalidValueError("Base64<QByteArray>", s);
        }
        return std::move(*result);
    }
    auto serialize(const QByteArray &value) const { return value.toBase64(); }
    bool hasValue(const QByteArray &value) const { return !value.isEmpty(); }
    QByteArray defaultValue() const { return {}; }
};

struct OptionalBase64Serializer {
    std::optional<QByteArray> parse(const QString &s) const
    {
        if (s.isEmpty()) {
            return {};
        }
        auto result = QByteArray::fromBase64Encoding(s.toUtf8());
        if (!result) {
            throw InvalidValueError("Base64<QByteArray>", s);
        }
        return std::move(*result);
    }
    auto serialize(const std::optional<QByteArray> &value) const { return value ? value->toBase64() : QByteArray(); }
    bool hasValue(const std::optional<QByteArray> &value) const { return value.has_value(); }
    std::optional<QByteArray> defaultValue() const { return {}; }
};

struct BoolDefaultSerializer {
    bool defaultValue_;

    bool parse(const QString &s) const
    {
        if (s.isEmpty()) {
            return defaultValue_;
        }
        return StringSerDe<bool>().parse(s);
    }
    auto serialize(bool b) const { return StringSerDe<bool>().serialize(b); }
    bool hasValue(bool b) const { return b != defaultValue_; }
    bool defaultValue() const { return defaultValue_; }
};

//
// XML Spec Ser/Deser
//

struct Required { };
struct Optional { };

struct XmlSpecParser {
    template<typename Struct>
    static Struct parse(const QDomElement &el)
    {
        // 1. for each spec field call parse() (returns tuple)
        // 2. concat those using std::tuple_cat and then
        // 3. use the flattened result to construct Struct
        return std::apply(
            [&](auto &&...fields) {
                return std::apply(
                    [](auto &&...values) {
                        return Struct { std::forward<decltype(values)>(values)... };
                    },
                    std::tuple_cat(fields.parse(el)...));
            },
            XmlSpec<Struct>::Spec);
    }

    template<typename Struct>
    static Struct parseFallback(const QDomElement &el)
    {
        if constexpr (HasXmlSpec<Struct>) {
            return parse<Struct>(el);
        } else {
            if (auto parsed = parseElement<Struct>(el)) {
                return std::move(*parsed);
            } else {
                auto &[name, _] = Struct::XmlTag;
                throw UnknownParsingError(name, typeid(Struct).name());
            }
        }
    }

    template<typename Struct>
    static std::optional<Struct> fromDomImpl(const QDomElement &el)
    {
        if (!isElementType<Struct>(el)) {
            return {};
        }

        try {
            return parse<Struct>(el);
        } catch (ParsingError e) {
            return {};
        }
    }
};

struct XmlSpecSerializer {
    template<typename Struct>
    static void serialize(QXmlStreamWriter *w, const Struct &input, QStringView xmlns = {})
    {
        auto &[tagName, tagXmlns] = Struct::XmlTag;
        w->writeStartElement(toString65(tagName));
        if (tagXmlns != xmlns) {
            w->writeDefaultNamespace(toString65(tagXmlns));
        }

        std::apply(
            [&](auto &&...fields) {
                (fields.serialize(w, input, tagXmlns), ...);
            },
            XmlSpec<Struct>::Spec);

        w->writeEndElement();
    }

    template<typename Struct>
    static void serializeFallback(QXmlStreamWriter *w, const Struct &input, QStringView xmlns)
    {
        if constexpr (HasXmlSpec<Struct>) {
            serialize(w, input, xmlns);
        } else {
            XmlWriter(w).write(input);
        }
    }
};

template<typename Struct, typename Value>
struct XmlReference;

template<typename Struct, HasXmlTag Element>
struct XmlReference<Struct, Element> {
    Element Struct::*member;

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement<Element>(parent);
        if (child.isNull()) {
            throw MissingElementError(parent, Element::XmlTag);
        }
        return std::tuple { XmlSpecParser::parseFallback<Element>(child) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        XmlSpecSerializer::serializeFallback(w, data.*member, currentXmlns);
    }
};

template<typename Struct, HasXmlTag Element>
struct XmlReference<Struct, std::optional<Element>> {
    std::optional<Element> Struct::*member;

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement<Element>(parent);
        if (child.isNull()) {
            return std::tuple { std::optional<Element>() };
        }
        return std::tuple { std::make_optional(XmlSpecParser::parseFallback<Element>(child)) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (data.*member) {
            XmlSpecSerializer::serializeFallback(w, (data.*member).value(), currentXmlns);
        }
    }
    bool hasValue(const Struct &data) const { return (data.*member).has_value(); }
};

template<std::ranges::range Range, typename Struct>
    requires(HasXmlTag<std::ranges::range_value_t<Range>>)
struct XmlReference<Struct, Range> {
    using Element = std::ranges::range_value_t<Range>;

    Range Struct::*member;

    auto parse(const QDomElement &parent) const
    {
        return std::tuple {
            transform<Range>(iterChildElements<Element>(parent), &XmlSpecParser::parseFallback<Element>),
        };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        for (const auto &value : data.*member) {
            XmlSpecSerializer::serializeFallback(w, value, currentXmlns);
        }
    }
    bool hasValue(const Struct &data) const { return !std::ranges::empty(data.*member); }
};

template<typename Struct>
struct XmlReference<Struct, bool> {
    bool Struct::*member;
    std::tuple<QStringView, QStringView> tag;

    auto parse(const QDomElement &parent) const
    {
        auto &[name, xmlns] = tag;
        return std::tuple { hasChild(parent, name, xmlns) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (data.*member) {
            auto &[name, xmlns] = tag;
            w->writeStartElement(toString65(name));
            if (xmlns != currentXmlns) {
                w->writeDefaultNamespace(toString65(xmlns));
            }
            w->writeEndElement();
        }
    }
    bool hasValue(const Struct &data) const { return data.*member; }
};

template<typename Struct>
XmlReference(bool Struct::*, std::tuple<QStringView, QStringView>) -> XmlReference<Struct, bool>;

template<HasXmlTag Element, typename Struct>
XmlReference(std::optional<Element> Struct::*) -> XmlReference<Struct, std::optional<Element>>;

template<HasXmlTag Element, typename Struct>
XmlReference(Element Struct::*) -> XmlReference<Struct, std::optional<Element>>;

template<std::ranges::range Range, typename Struct>
    requires(HasXmlTag<std::ranges::range_value_t<Range>>)
XmlReference(Range Struct::*) -> XmlReference<Struct, Range>;

template<typename Tag, typename Requirement, typename... Contents>
struct XmlElement {
    Tag tag;
    Requirement r;
    std::tuple<Contents...> contents;

    template<typename... C>
    constexpr XmlElement(Tag tag, Requirement &&r, C &&...c)
        : tag(std::forward<Tag>(tag)), r(r), contents(std::forward<Contents>(c)...) { }

    auto parse(const QDomElement &parent) const
    {
        QDomElement child;
        if constexpr (IsXmlTag<Tag>) {
            auto &[name, xmlns] = tag;
            child = firstChildElement(parent, name, xmlns);
        } else {
            child = firstChildElement(parent, tag, parent.namespaceURI());
        }
        if constexpr (std::is_same_v<Requirement, Required>) {
            if (child.isNull()) {
                throw MissingElementError(parent, tag);
            }
        }
        return std::apply(
            [&](auto &&...fields) {
                return std::tuple_cat(fields.parse(child)...);
            },
            contents);
    }
    template<typename Struct>
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if constexpr (std::is_same_v<Requirement, Optional>) {
            auto contentsHasValue = std::apply(
                [&](auto &&...fields) {
                    return (fields.hasValue(data) || ...);
                },
                contents);

            if (!contentsHasValue) {
                return;
            }
        }

        QStringView childXmlns = currentXmlns;
        if constexpr (IsXmlTag<Tag>) {
            auto &[name, xmlns] = tag;
            w->writeStartElement(toString65(name));
            if (xmlns != currentXmlns) {
                w->writeDefaultNamespace(toString65(xmlns));
            }
            childXmlns = xmlns;
        } else {
            w->writeStartElement(toString65(tag));
        }
        std::apply(
            [&](auto &&...fields) {
                (fields.serialize(w, data, childXmlns), ...);
            },
            contents);
        w->writeEndElement();
    }
};

template<typename Tag = Tag<QStringView, QStringView>, typename... Contents>
XmlElement(Tag &&, Required, Contents &&...) -> XmlElement<Tag, Required, Contents...>;

template<typename Tag = Tag<QStringView, QStringView>, typename... Contents>
XmlElement(Tag &&, Optional, Contents &&...) -> XmlElement<Tag, Optional, Contents...>;

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlAttribute {
    Value Struct::*member;
    QStringView name;
    StringSerializer serde = {};

    auto parse(QDomElement el) const
    {
        auto attributeNode = el.attributeNode(name.toString());
        if (attributeNode.isNull()) {
            throw MissingAttributeError(name, el);
        }
        return std::tuple { serde.parse(attributeNode.value()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        w->writeAttribute(toString65(name), toString65(serde.serialize(data.*member)));
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlOptionalAttribute {
    Value Struct::*member;
    QStringView name;
    StringSerializer serde = {};

    auto parse(QDomElement el) const
    {
        auto attributeNode = el.attributeNode(name.toString());
        if (attributeNode.isNull()) {
            return std::tuple { serde.defaultValue() };
        }
        return std::tuple { serde.parse(attributeNode.value()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w->writeAttribute(toString65(name), toString65(serde.serialize(data.*member)));
        }
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlText {
    Value Struct::*member;
    StringSerializer serde = {};

    auto parse(const QDomElement &el) const
    {
        return std::tuple { serde.parse(el.text()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        w->writeCharacters(toString65(serde.serialize(data.*member)));
    }
    bool hasValue(const Struct &data) const
    {
        return serde.hasValue(data.*member);
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlOptionalText {
    Value Struct::*member;
    StringSerializer serde = {};

    auto parse(const QDomElement &el) const
    {
        auto text = el.text();
        if (!text.isEmpty()) {
            return std::tuple { serde.parse(el.text()) };
        }
        return std::tuple { serde.defaultValue() };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w->writeCharacters(toString65(serde.serialize(data.*member)));
        }
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlTextElement {
    Value Struct::*member;
    QStringView name;
    StringSerializer serde = {};

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement(parent, name, parent.namespaceURI());
        if (child.isNull()) {
            throw MissingElementError(parent, name, parent.namespaceURI());
        }
        return std::tuple { serde.parse(child.text()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        XmlWriter(w).write(TextElement { name, serde.serialize(data.*member) });
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlOptionalTextElement {
    Value Struct::*member;
    QStringView name;
    StringSerializer serde = {};

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement(parent, name, parent.namespaceURI());
        if (child.isNull()) {
            return std::tuple { serde.defaultValue() };
        }
        return std::tuple { serde.parse(child.text()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            XmlWriter(w).write(TextElement { name, serde.serialize(data.*member) });
        }
    }
    bool hasValue(const Struct &data) { return serde.hasValue(data.*member); }
};

template<typename Struct, std::ranges::range Range>
struct XmlTextElements {
    Range Struct::*member;
    QStringView name;

    auto parse(const QDomElement &el) const
    {
        QString attributeName = name.toString();
        return std::tuple {
            transform<Range>(iterChildElements(el, name, el.namespaceURI()), &QDomElement::text),
        };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        for (const auto &value : data.*member) {
            XmlWriter(w).write(Element { name, Characters { value } });
        }
    }
    bool hasValue(const Struct &data) const { return !std::ranges::empty(data.*member); }
};

template<typename Struct, typename Range, typename Tag, typename StringSerializer = StringSerDe<std::ranges::range_value_t<Range>>>
struct XmlSingleAttributeElements {
    Range Struct::*member;
    Tag tag;
    QStringView attribute;
    StringSerializer serde = {};

    auto parse(const QDomElement &el) const
    {
        auto &[name, xmlns] = tag;
        // TODO: error on invalid element (without attribute)
        return std::tuple { parseSingleAttributeElements<Range>(el, name, xmlns, attribute.toString()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        for (const auto &value : data.*member) {
            using ValueType = std::ranges::range_value_t<Range>;
            auto &[name, xmlns] = tag;
            w->writeStartElement(toString65(name));
            if (currentXmlns != xmlns) {
                w->writeDefaultNamespace(toString65(xmlns));
            }
            w->writeAttribute(toString65(attribute), toString65(serde.serialize(value)));
            w->writeEndElement();
        }
    }
    bool hasValue(const Struct &data) const
    {
        return !std::ranges::empty(data.*member);
    }
};

template<typename Struct, typename Enum, typename StringSerializer = StringSerDe<Enum>>
struct XmlEnumElement {
    Enum Struct::*member;
    QStringView xmlns;
    StringSerializer serde = {};

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement(parent, {}, xmlns);
        if (child.isNull()) {
            throw MissingElementError(parent, Tag<QStringView, QStringView> { {}, xmlns });
        }
        return std::tuple { serde.parse(child.tagName()) };
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        w->writeStartElement(toString65(serde.serialize(data.*member)));
        if (xmlns != currentXmlns) {
            w->writeDefaultNamespace(toString65(xmlns));
        }
        w->writeEndElement();
    }
};

template<typename Struct, typename Value, typename StringSerializer = StringSerDe<Value>>
struct XmlOptionalEnumElement {
    Value Struct::*member;
    QStringView xmlns;
    StringSerializer serde = {};

    auto parse(const QDomElement &parent) const
    {
        auto child = firstChildElement(parent, {}, xmlns);
        if (child.isNull()) {
            return std::tuple { serde.defaultValue() };
        }
        // other elements with same namespace are no parsing error
        try {
            return std::tuple { serde.parse(child.tagName()) };
        } catch (ParsingError) {
            return std::tuple { serde.defaultValue() };
        }
    }
    void serialize(QXmlStreamWriter *w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w->writeStartElement(toString65(serde.serialize(data.*member)));
            if (xmlns != currentXmlns) {
                w->writeDefaultNamespace(toString65(xmlns));
            }
            w->writeEndElement();
        }
    }
};

template<typename T>
inline QByteArray serializeXml(const T &packet)
    requires(HasXmlSpec<T> && !QXmlStreamSerializeable<T> && !XmlWriterSerializeable<T>)
{
    return serializeXml(std::function<void(QXmlStreamWriter *)> {
        [&](QXmlStreamWriter *w) {
            XmlSpecSerializer::serialize(w, packet);
        } });
}

// TODO: Merge into parseElement() in Utils_p.h
template<HasXmlSpec T>
auto elementFromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<T>(el); }

// Impl for XmlWriter
template<HasXmlSpec T>
inline void XmlWriter::write(const T &t) { XmlSpecSerializer::serialize(*this, t); }

}  // namespace QXmpp::Private

#endif  // XML_H

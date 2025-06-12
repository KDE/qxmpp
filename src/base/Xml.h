// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XML_H
#define XML_H

#include "QXmppUtils.h"

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
    auto parse(const QString &s) const
    {
        return std::optional<T>(StringSerDe<T>().parse(s));
    }
    auto serialize(const std::optional<T> &o) const
    {
        if (o.has_value()) {
            return StringSerDe<T>().serialize(*o);
        }
        return decltype(StringSerDe<T>().serialize(*o)) {};
    }
    bool hasValue(const std::optional<T> &o) const { return o.has_value(); }
};

template<>
struct StringSerDe<QString> {
    static QString parse(QString &&s) { return std::move(s); }
    static const QString &serialize(const QString &s) { return s; }
    static bool hasValue(const QString &s) { return !s.isEmpty(); }
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

template<typename Enum>
    requires Enums::SerializableEnum<Enum>
struct StringSerDe<Enum> {
    static Enum parse(const QString &s)
    {
        auto e = Enums::fromString<Enum>(s);
        if (!e) {
            throw InvalidValueError(typeid(Enum).name(), s);
        }
        return *e;
    }
    static auto serialize(Enum value) { return Enums::toString(value); }
    static bool hasValue(Enum value)
        requires Enums::NullableEnum<Enum>
    {
        return value != Enums::Data<Enum>::NullValue;
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
    static QByteArray parse(const QString &s)
    {
        auto result = QByteArray::fromBase64Encoding(s.toUtf8());
        if (!result) {
            throw InvalidValueError("Base64<QByteArray>", s);
        }
        return std::move(*result);
    }
    static auto serialize(const QByteArray &value) { return value.toBase64(); }
    static bool hasValue(const QByteArray &value) { return !value.isEmpty(); }

    static auto serialize(const std::optional<QByteArray> &value) { return value ? value->toBase64() : QByteArray(); }
    static bool hasValue(const std::optional<QByteArray> &value) { return value.has_value(); }
};

struct BoolDefaultSerializer {
    bool defaultValue;

    bool parse(const QString &s) const
    {
        if (s.isEmpty()) {
            return defaultValue;
        }
        return StringSerDe<bool>().parse(s);
    }
    auto serialize(bool b) const { return StringSerDe<bool>().serialize(b); }
    bool hasValue(bool b) const { return b != defaultValue; }
};

//
// XML Spec Ser/Deser
//

template<typename T>
struct MemberPointerValueTypeImpl;

// Partial specialization for data member pointer
template<typename Struct, typename MemberType>
struct MemberPointerValueTypeImpl<MemberType Struct::*> {
    using Type = MemberType;
};

template<>
struct MemberPointerValueTypeImpl<void> {
    using Type = void;
};

template<typename T>
using MemberPointerValueType = MemberPointerValueTypeImpl<T>::Type;

struct Required { };
struct Optional { };

template<typename Struct>
struct XmlSpec;

template<typename T>
concept HasXmlSpec = requires {
    typename XmlSpec<T>;
    { XmlSpec<T>::Spec };
};

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
    static void serialize(XmlWriter &w, const Struct &input, QStringView xmlns = {})
    {
        auto &[tagName, tagXmlns] = Struct::XmlTag;
        w.writer()->writeStartElement(toString65(tagName));
        if (tagXmlns != xmlns) {
            w.writer()->writeDefaultNamespace(toString65(tagXmlns));
        }

        std::apply(
            [&](auto &&...fields) {
                (fields.serialize(w, input, tagXmlns), ...);
            },
            XmlSpec<Struct>::Spec);

        w.writer()->writeEndElement();
    }

    template<typename Struct>
    static void serializeFallback(XmlWriter &w, const Struct &input, QStringView xmlns)
    {
        if constexpr (HasXmlSpec<Struct>) {
            serialize(w, input, xmlns);
        } else {
            w.write(input);
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        if (data.*member) {
            auto &[name, xmlns] = tag;
            if (xmlns != currentXmlns) {
                w.write(Element { tag });
            } else {
                w.write(Element { name });
            }
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
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
            w.writer()->writeStartElement(toString65(name));
            if (xmlns != currentXmlns) {
                w.writer()->writeDefaultNamespace(toString65(xmlns));
            }
            childXmlns = xmlns;
        } else {
            w.writer()->writeStartElement(toString65(tag));
        }
        std::apply(
            [&](auto &&...fields) {
                (fields.serialize(w, data, childXmlns), ...);
            },
            contents);
        w.writer()->writeEndElement();
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        w.writer()->writeAttribute(toString65(name), toString65(serde.serialize(data.*member)));
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
            return std::tuple { Value {} };
        }
        return std::tuple { serde.parse(attributeNode.value()) };
    }
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w.writer()->writeAttribute(toString65(name), toString65(serde.serialize(data.*member)));
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        w.writer()->writeCharacters(toString65(serde.serialize(data.*member)));
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
        return std::tuple { Value {} };
    }
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w.writer()->writeCharacters(toString65(serde.serialize(data.*member)));
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        w.write(TextElement { name, serde.serialize(data.*member) });
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
            return std::tuple { decltype(serde.parse(QString())) {} };
        }
        return std::tuple { serde.parse(child.text()) };
    }
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        if (serde.hasValue(data.*member)) {
            w.write(TextElement { name, serde.serialize(data.*member) });
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        for (const auto &value : data.*member) {
            w.write(Element { name, Characters { value } });
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
    void serialize(XmlWriter &w, const Struct &data, QStringView currentXmlns) const
    {
        for (const auto &value : data.*member) {
            using ValueType = std::ranges::range_value_t<Range>;
            auto &[name, xmlns] = tag;
            w.writer()->writeStartElement(toString65(name));
            if (currentXmlns != xmlns) {
                w.writer()->writeDefaultNamespace(toString65(xmlns));
            }
            w.writer()->writeAttribute(toString65(attribute), toString65(serde.serialize(value)));
            w.writer()->writeEndElement();
        }
    }
    bool hasValue(const Struct &data) const
    {
        return !std::ranges::empty(data.*member);
    }
};

}  // namespace QXmpp::Private

#endif  // XML_H

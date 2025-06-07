// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XML_H
#define XML_H

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

struct InvalidValueError : public ParsingError {
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
    static auto parse(const QDomElement &el)
    {
        return std::optional<T>(StringSerDe<T>::parse(el));
    }
};

template<>
struct StringSerDe<QString> {
    static QString parse(QString &&s) { return std::move(s); }
    static const QString &serialize(const QString &s) { return s; }
};

template<std::integral Int>
struct StringSerDe<Int> {
    static Int parse(const QString &s)
    {
        if (auto value = parseInt<Int>(s)) {
            return *value;
        }
        throw InvalidValueError(typeid(Int).name(), s);
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
};

//
// XML Spec Ser/Deser
//

enum ElementResult {
    Exists,
};

struct Required { };
struct Optional { };

template<typename Tag, typename Requirement, typename... Contents>
struct XmlElement {
    Tag tag;
    Requirement r;
    std::tuple<Contents...> contents;

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
                return std::tuple { fields.parse(child)... };
            },
            contents);
    }
    template<typename Struct>
    void serialize(XmlWriter &w, const Struct &data) const
    {
        w.write(Element {
            tag,
            [&] {
                std::apply(
                    [&](auto &&...fields) {
                        (fields.serialize(w, data), ...);
                    },
                    contents);
            },
        });
    }
};

template<typename Tag = Tag<QStringView, QStringView>, typename... Contents>
XmlElement(Tag &&, Required, Contents &&...) -> XmlElement<Tag, Required, Contents...>;

template<typename Tag = Tag<QStringView, QStringView>, typename... Contents>
XmlElement(Tag &&, Optional, Contents &&...) -> XmlElement<Tag, Optional, Contents...>;

template<typename Tag = Tag<QStringView, QStringView>, typename... Contents>
XmlElement(Tag &&, Contents &&...) -> XmlElement<Tag, Contents...>;

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
    void serialize(XmlWriter &w, const Struct &data) const
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
            return Value {};
        }
        return std::tuple { serde.parse(attributeNode.value()) };
    }
    void serialize(XmlWriter &w, const Struct &data) const
    {
        w.writer()->writeAttribute(toString65(name), toString65(serde.serialize(data.*member)));
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
    void serialize(XmlWriter &w, const Struct &data) const
    {
        w.writer()->writeCharacters(toString65(serde.serialize(data.*member)));
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
    void serialize(XmlWriter &w, const Struct &data) const
    {
        if (serde.hasValue(data.*member)) {
            w.writer()->writeCharacters(toString65(serde.serialize(data.*member)));
        }
    }
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
    void serialize(XmlWriter &w, const Struct &data) const
    {
        for (const auto &value : data.*member) {
            using ValueType = std::ranges::range_value_t<Range>;
            auto &[name, _] = tag;
            w.writer()->writeStartElement(toString65(name));
            w.writer()->writeAttribute(toString65(attribute), toString65(serde.serialize(value)));
            w.writer()->writeEndElement();
        }
    }
};

template<typename Struct>
struct XmlSpec;

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
    static void serialize(XmlWriter &w, const Struct &input)
    {
        w.write(Element {
            Struct::XmlTag,
            [&] {
                std::apply(
                    [&](auto &&...fields) {
                        (fields.serialize(w, input), ...);
                    },
                    XmlSpec<Struct>::Spec);
            },
        });
    }
};

}  // namespace QXmpp::Private

#endif  // XML_H

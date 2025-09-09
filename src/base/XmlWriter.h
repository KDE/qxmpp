// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XMLWRITER_H
#define XMLWRITER_H

#include "QXmppUtils_p.h"

#include "Enums.h"
#include "StringLiterals.h"

#include <optional>

#include <QDateTime>
#include <QMimeType>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamWriter>

namespace QXmpp::Private {

class XmlWriter;

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
inline auto toString65(QStringView s) { return s.toString(); }
inline auto toString65(const QByteArray &s) { return QString::fromUtf8(s); }
inline const QString &toString65(const QString &s) { return s; }
inline QString toString65(QString &&s) { return std::move(s); }
#else
#define toString65(x) x
#endif

template<typename T>
struct StringSerializer {
    static decltype(auto) serialize(auto &&s) { return std::forward<decltype(s)>(s); }
    static bool hasValue(auto &&s) { return !s.isEmpty(); }
};

template<typename T>
concept IsOptionalStringSerializable = requires(const T &value) {
    { StringSerializer<T>::hasValue(value) } -> std::convertible_to<bool>;
};

template<typename U>
struct StringSerializer<std::optional<U>> {
    static auto serialize(auto &&value)
    {
        if (value) {
            return StringSerializer<U>::serialize(*value);
        }
        return std::remove_cvref_t<decltype(StringSerializer<U>::serialize(*value))> {};
    }
    static bool hasValue(const auto &value) { return value.has_value(); }
};

template<>
struct StringSerializer<bool> {
    static auto serialize(bool value) { return value ? u"true"_s : u"false"_s; }
    static bool hasValue(bool) { return true; }
};

template<typename Number>
    requires std::is_integral_v<Number> || std::is_floating_point_v<Number>
struct StringSerializer<Number> {
    static auto serialize(Number value) { return QString::number(value); }
};

template<typename Enum>
    requires std::is_enum_v<Enum>
struct StringSerializer<Enum> {
    static auto serialize(Enum value) { return Enums::toString(value); }
    static bool hasValue(Enum value)
        requires Enums::NullableEnum<Enum>
    {
        return value != Enums::Data<Enum>::NullValue;
    }
};

template<>
struct StringSerializer<QDateTime> {
    static QString serialize(const QDateTime &datetime);
    static bool hasValue(const QDateTime &datetime) { return datetime.isValid(); }
};

template<>
struct StringSerializer<QUuid> {
    static auto serialize(QUuid uuid) { return uuid.toString(QUuid::WithoutBraces); }
    static bool hasValue(QUuid uuid) { return !uuid.isNull(); }
};

template<>
struct StringSerializer<QUrl> {
    static auto serialize(const QUrl &url) { return url.toString(); }
    static bool hasValue(const QUrl &url) { return !url.isEmpty(); }
};

template<>
struct StringSerializer<QMimeType> {
    static auto serialize(const QMimeType &mimeType) { return mimeType.name(); }
    static bool hasValue(const QMimeType &mimeType) { return mimeType.isValid(); }
};

struct Base64 {
    const QByteArray &data;

    static Base64 fromByteArray(const QByteArray &d) { return { d }; }
};

template<>
struct StringSerializer<Base64> {
    static auto serialize(Base64 value) { return value.data.toBase64(); }
    static bool hasValue(Base64 value) { return !value.data.isEmpty(); }
};

struct DefaultedBool {
    bool value;
    bool defaultValue;
};

template<>
struct StringSerializer<DefaultedBool> {
    static auto serialize(auto &&v) { return v.value ? u"true"_s : u"false"_s; }
    static bool hasValue(auto &&v) { return v.value != v.defaultValue; }
};

// Serializes value to string and converts to type writeable to QXmlStreamWriter
template<typename T>
decltype(auto) xmlS(T &&value)
{
    return toString65(StringSerializer<std::decay_t<T>>::serialize(std::forward<T>(value)));
}

template<typename T>
concept XmlSerializeable = XmlWriterSerializeable<T> || QXmlStreamSerializeable<T>;

template<typename T>
concept XmlSerializeableRange =
    std::ranges::range<T> && XmlSerializeable<std::ranges::range_value_t<T>>;

class XmlWriter
{
public:
    explicit XmlWriter(QXmlStreamWriter *writer) noexcept : w(writer) { }
    operator QXmlStreamWriter *() const noexcept { return w; }
    QXmlStreamWriter *writer() const noexcept { return w; }

    template<XmlSerializeable T>
    void write(T &&value)
    {
        value.toXml(*this);
    }
    template<IsStdOptional T>
    void write(T &&opt)
    {
        if (opt) {
            write(*opt);
        }
    }
    template<typename Container>
        requires(XmlSerializeableRange<Container> && !XmlSerializeable<Container>)
    void write(Container &&container)
    {
        for (const auto &value : container) {
            write(value);
        }
    }
    template<std::invocable Function>
    void write(Function &&f)
    {
        f();
    }

private:
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    using String = QAnyStringView;
#else
    using String = const QString &;
#endif

    void writeStartElement(String name) { w->writeStartElement(name); }
    QXMPP_PRIVATE_EXPORT void writeStartElement(String name, String xmlns);
    void writeEndElement() { w->writeEndElement(); }
    void writeEmptyElement(String name) { w->writeEmptyElement(name); }
    QXMPP_PRIVATE_EXPORT void writeEmptyElement(String name, String xmlns);
    QXMPP_PRIVATE_EXPORT void writeTextOrEmptyElement(String name, String value);
    QXMPP_PRIVATE_EXPORT void writeTextOrEmptyElement(String name, String xmlns, String value);
    QXMPP_PRIVATE_EXPORT void writeSingleAttributeElement(String name, String attribute, String value);

    template<typename Tag, typename... Values>
    friend struct Element;

    template<IsOptionalStringSerializable Enum>
    friend struct OptionalEnumElement;

    template<typename Tag, typename Value>
    friend struct TextElement;

    template<typename Tag, IsOptionalStringSerializable Value>
    friend struct OptionalTextElement;

    template<std::ranges::range Container>
    friend struct SingleAttributeElements;

    QXmlStreamWriter *w;
};

template<typename Tag, typename... Values>
struct Element {
    Tag tag;
    std::tuple<Values...> values;

    template<typename... V>
    Element(Tag tag, V &&...values)
        : tag(std::forward<Tag>(tag)), values(std::forward<V>(values)...)
    {
    }

    void toXml(XmlWriter &w)
    {
        if constexpr (sizeof...(Values) == 0) {
            if constexpr (IsXmlTag<Tag>) {
                auto &[name, xmlns] = tag;
                w.writeEmptyElement(xmlS(name), xmlS(xmlns));
            } else {
                w.writeEmptyElement(xmlS(tag));
            }
        } else {
            if constexpr (IsXmlTag<Tag>) {
                auto &[name, xmlns] = tag;
                w.writeStartElement(xmlS(name), xmlS(xmlns));
            } else {
                w.writeStartElement(xmlS(tag));
            }
            std::apply([&w](auto &&...value) { (w.write(value), ...); }, values);
            w.writeEndElement();
        }
    }
};

template<typename Name, typename... Values>
    requires(!IsXmlTag<Name>)
Element(Name &&, Values &&...) -> Element<Name, Values...>;

template<IsXmlTag Tag = Tag<QStringView, QStringView>, typename... Values>
Element(Tag &&, Values &&...) -> Element<Tag, Values...>;

template<typename Value>
struct Attribute {
    QStringView name;
    Value value;

    void toXml(XmlWriter &w) const
    {
        w.writer()->writeAttribute(xmlS(name), xmlS(value));
    }
};

template<typename Value>
Attribute(QStringView, Value &&) -> Attribute<Value>;

template<IsOptionalStringSerializable Value>
struct OptionalAttribute {
    QStringView name;
    Value value;

    void toXml(QXmlStreamWriter *w) const
    {
        if (StringSerializer<std::decay_t<Value>>::hasValue(value)) {
            w->writeAttribute(xmlS(name), xmlS(value));
        }
    }
};

template<typename Value>
OptionalAttribute(QStringView, Value &&) -> OptionalAttribute<Value>;

template<typename Value>
struct Characters {
    Value value;

    template<typename V>
    Characters(V &&value) : value(std::forward<V>(value)) { }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeCharacters(xmlS(value));
    }
};

template<typename Value>
Characters(Value &&) -> Characters<Value>;

template<IsOptionalStringSerializable Value>
struct OptionalCharacters {
    Value value;

    template<typename V>
    OptionalCharacters(V &&value) : value(std::forward<V>(value)) { }

    void toXml(QXmlStreamWriter *w) const
    {
        if (StringSerializer<std::decay_t<Value>>::hasValue(value)) {
            w->writeCharacters(xmlS(value));
        }
    }
};

template<typename T>
OptionalCharacters(T &&) -> OptionalCharacters<T>;

struct DefaultNamespace {
    QStringView xmlns;

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeDefaultNamespace(xmlS(xmlns));
    }
};

struct Namespace {
    QStringView prefix;
    QStringView xmlns;

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeNamespace(xmlS(xmlns), xmlS(prefix));
    }
};

template<IsOptionalStringSerializable Enum>
struct OptionalEnumElement {
    Enum enumeration;
    QStringView xmlns = {};

    void toXml(XmlWriter &w) const
    {
        if (StringSerializer<Enum>::hasValue(enumeration)) {
            if (xmlns.isNull()) {
                w.writeEmptyElement(xmlS(enumeration));
            } else {
                w.writeEmptyElement(xmlS(enumeration), xmlS(xmlns));
            }
        }
    }
};

template<IsOptionalStringSerializable Enum>
OptionalEnumElement(Enum) -> OptionalEnumElement<Enum>;
template<IsOptionalStringSerializable Enum, std::convertible_to<QStringView> StringView>
OptionalEnumElement(Enum, StringView) -> OptionalEnumElement<Enum>;

template<typename Tag, typename Value>
struct TextElement {
    Tag tag;
    Value value;

    template<typename V>
    TextElement(Tag tag, V &&value) : tag(tag), value(std::forward<V>(value)) { }

    void toXml(XmlWriter &w)
    {
        if constexpr (IsXmlTag<Tag>) {
            auto &[name, xmlns] = tag;
            w.writeTextOrEmptyElement(xmlS(name), xmlS(xmlns), xmlS(value));
        } else {
            w.writeTextOrEmptyElement(xmlS(tag), xmlS(value));
        }
    }
};

template<typename Name, typename Value>
    requires(!IsXmlTag<Name>)
TextElement(Name &&, Value &&) -> TextElement<Name, Value>;

template<IsXmlTag Tag = Tag<QStringView, QStringView>, typename Value>
TextElement(Tag &&, Value &&) -> TextElement<Tag, Value>;

template<typename Tag, std::ranges::range Range>
struct TextElements {
    Tag tag;
    Range values;

    template<typename R>
    TextElements(Tag tag, R &&values) : tag(tag), values(std::forward<R>(values)) { }

    void toXml(XmlWriter &w)
    {
        for (const auto &value : values) {
            w.write(TextElement { tag, value });
        }
    }
};

template<typename Name, typename Range>
    requires(!IsXmlTag<Name>)
TextElements(Name &&, Range &&) -> TextElements<Name, Range>;

template<IsXmlTag Tag = Tag<QStringView, QStringView>, typename Range>
TextElements(Tag &&, Range &&) -> TextElements<Tag, Range>;

template<typename Tag, IsOptionalStringSerializable Value>
struct OptionalTextElement {
    Tag tag;
    Value value;

    template<typename V>
    OptionalTextElement(Tag tag, V &&value) : tag(tag), value(std::forward<V>(value)) { }

    void toXml(XmlWriter &w)
    {
        if (StringSerializer<std::decay_t<Value>>::hasValue(value)) {
            if constexpr (IsXmlTag<Tag>) {
                auto &[name, xmlns] = tag;
                w.writeTextOrEmptyElement(xmlS(name), xmlS(xmlns), xmlS(value));
            } else {
                w.writeTextOrEmptyElement(xmlS(tag), xmlS(value));
            }
        }
    }
};

template<typename Name, typename Value>
    requires(!IsXmlTag<Name>)
OptionalTextElement(Name &&, Value &&) -> OptionalTextElement<Name, Value>;

template<IsXmlTag Tag = Tag<QStringView, QStringView>, typename Value>
OptionalTextElement(Tag, Value &&) -> OptionalTextElement<Tag, Value>;

template<std::ranges::range Range>
struct SingleAttributeElements {
    QStringView name;
    QStringView attribute;
    Range values;

    template<typename R>
    SingleAttributeElements(QStringView name, QStringView attribute, R &&values)
        : name(name), attribute(attribute), values(std::forward<R>(values)) { }

    void toXml(XmlWriter &w)
    {
        for (const auto &value : values) {
            w.writeSingleAttributeElement(xmlS(name), xmlS(attribute), xmlS(value));
        }
    }
};

template<typename Range>
SingleAttributeElements(QStringView, QStringView, Range &&) -> SingleAttributeElements<Range>;

template<typename... Values>
struct OptionalContent {
    bool condition;
    std::tuple<Values...> values;

    template<std::convertible_to<bool> Condition, typename... V>
    OptionalContent(Condition condition, V &&...values)
        : condition(condition), values(std::forward<V>(values)...) { }

    void toXml(XmlWriter &w)
    {
        if (condition) {
            std::apply([&w](auto &&...value) { (w.write(value), ...); }, values);
        }
    }
};

template<std::convertible_to<bool> Condition, typename... Values>
OptionalContent(Condition, Values &&...) -> OptionalContent<Values...>;

}  // namespace QXmpp::Private

#endif  // XMLWRITER_H

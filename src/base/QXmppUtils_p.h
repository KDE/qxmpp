// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPUTILS_P_H
#define QXMPPUTILS_P_H

#include "QXmppGlobal.h"
#include "QXmppXmlTags_p.h"

#include "Algorithms.h"

#include <functional>
#include <optional>
#include <stdint.h>

#include <QByteArray>
#include <QDomElement>

class QDomElement;
class QXmlStreamWriter;
class QXmppNonza;

namespace QXmpp::Private {

class XmlWriter;

// Helper for Q(Any)StringView overloads that were added later
inline auto toString60(QStringView s)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return s;
#else
    return s.toString();
#endif
}

template<typename T>
concept IsStdOptional = requires {
    typename std::remove_cvref_t<T>::value_type;
    requires std::same_as<std::remove_cvref_t<T>, std::optional<typename std::remove_cvref_t<T>::value_type>>;
};

// Base64
std::optional<QByteArray> parseBase64(const QString &);
inline QString serializeBase64(const QByteArray &data) { return QString::fromUtf8(data.toBase64()); }

// Integer parsing
template<typename Int = int>
std::optional<Int> parseInt(QStringView str);
template<typename Int>
inline QString serializeInt(Int value) { return QString::number(value); }

// Double parsing
std::optional<double> parseDouble(QStringView str);
std::optional<float> parseFloat(QStringView str);

// Booleans
std::optional<bool> parseBoolean(const QString &str);
QString serializeBoolean(bool);

//
// DOM
//

QXMPP_EXPORT QDomElement firstChildElement(const QDomElement &, QStringView tagName = {}, QStringView xmlNs = {});
QXMPP_EXPORT QDomElement nextSiblingElement(const QDomElement &, QStringView tagName = {}, QStringView xmlNs = {});

template<typename T>
inline auto firstChildElement(const QDomElement &el)
    requires HasXmlTag<T>
{
    auto [tag, ns] = T::XmlTag;
    return firstChildElement(el, tag, ns);
}

template<typename T>
inline auto nextSiblingElement(const QDomElement &el)
    requires HasXmlTag<T>
{
    auto [tag, ns] = T::XmlTag;
    return nextSiblingElement(el, tag, ns);
}

inline auto hasChild(const QDomElement &el, QStringView tagName = {}, QStringView xmlns = {})
{
    return !firstChildElement(el, tagName, xmlns).isNull();
}

template<typename T>
inline auto hasChild(const QDomElement &el)
    requires HasXmlTag<T>
{
    return !firstChildElement<T>(el).isNull();
}

struct DomChildElements {
    QDomElement parent;
    QStringView tagName;
    QStringView namespaceUri;

    struct EndIterator { };
    struct Iterator {
        Iterator operator++()
        {
            el = nextSiblingElement(el, tagName, namespaceUri);
            return *this;
        }
        bool operator!=(EndIterator) const { return !el.isNull(); }
        const QDomElement &operator*() const { return el; }

        QDomElement el;
        QStringView tagName;
        QStringView namespaceUri;
    };

    Iterator begin() const { return { firstChildElement(parent, tagName, namespaceUri), tagName, namespaceUri }; }
    EndIterator end() const { return {}; }
};

inline DomChildElements iterChildElements(const QDomElement &el, QStringView tagName = {}, QStringView namespaceUri = {}) { return DomChildElements { el, tagName, namespaceUri }; }

template<typename T>
inline auto iterChildElements(const QDomElement &el)
    requires HasXmlTag<T>
{
    auto [tag, ns] = T::XmlTag;
    return iterChildElements(el, tag, ns);
}

template<typename T>
concept DomParsableV1 = requires(T t) {
    { t.parse(QDomElement()) } -> std::same_as<void>;
};

template<typename T>
concept DomParsableV2 = requires(T t) {
    { t.parse(QDomElement()) } -> std::same_as<bool>;
};

template<typename T>
concept DomParsableV3 = requires {
    { T::fromDom(QDomElement()) } -> std::same_as<std::optional<T>>;
};

template<typename T>
concept DomParsable = DomParsableV1<T> || DomParsableV2<T> || DomParsableV3<T>;

// Parse T from a QDomElement.
template<typename T>
auto parseElement(const QDomElement &el) -> std::optional<T>
    requires DomParsable<T>
{
    if constexpr (DomParsableV3<T>) {
        return T::fromDom(el);
    } else if constexpr (DomParsableV2<T>) {
        T element;
        if (!element.parse(el)) {
            return {};
        }
        return element;
    } else if constexpr (DomParsableV1<T>) {
        T element;
        element.parse(el);
        return element;
    }
}

// Parse T with T::parse() if DOM element is not null (no namespace check).
template<typename T>
auto parseOptionalElement(const QDomElement &domEl) -> std::optional<T>
{
    if (domEl.isNull()) {
        return {};
    }
    return parseElement<T>(domEl);
}

// Parse T with T::parse() with first child element with the correct tag name and namespace.
template<typename T>
auto parseOptionalChildElement(const QDomElement &parentEl, QStringView tagName, QStringView xmlns)
    requires(!HasXmlTag<T>)
{
    return parseOptionalElement<T>(firstChildElement(parentEl, tagName, xmlns));
}

template<typename T>
auto parseOptionalChildElement(const QDomElement &parentEl)
    requires HasXmlTag<T>
{
    auto [tag, ns] = T::XmlTag;
    return parseOptionalElement<T>(firstChildElement(parentEl, tag, ns));
}

// Parse all child elements matching the tag name and namespace into a container.
template<typename Container>
auto parseChildElements(const QDomElement &parentEl, QStringView tagName, QStringView xmlns)
    -> Container
    requires(!HasXmlTag<typename Container::value_type>)
{
    using T = typename Container::value_type;

    Container elements;
    for (const auto &childEl : iterChildElements(parentEl, tagName, xmlns)) {
        if (auto parsedEl = parseElement<T>(childEl)) {
            elements.push_back(std::move(*parsedEl));
        }
    }
    return elements;
}

template<typename Container>
auto parseChildElements(const QDomElement &parentEl) -> Container
    requires HasXmlTag<typename Container::value_type>
{
    using T = typename Container::value_type;
    auto [tag, ns] = T::XmlTag;

    Container elements;
    for (const auto &childEl : iterChildElements(parentEl, tag, ns)) {
        if (auto parsedEl = parseElement<T>(childEl)) {
            elements.push_back(std::move(*parsedEl));
        }
    }
    return elements;
}

template<typename Container = QList<QString>>
auto parseTextElements(const QDomElement &parent, QStringView tagName, QStringView xmlns)
    -> Container
{
    return transform<Container>(iterChildElements(parent, tagName, xmlns), &QDomElement::text);
}

template<typename Container = QList<QString>>
auto parseSingleAttributeElements(const QDomElement &parent, QStringView tagName, QStringView xmlns, const QString &attribute)
{
    return transform<Container>(iterChildElements(parent, tagName, xmlns), [=](const QDomElement &el) {
        return el.attribute(attribute);
    });
}

QByteArray serializeXml(std::function<void(XmlWriter &)>);
QByteArray serializeXml(std::function<void(QXmlStreamWriter *)>);

template<typename T>
concept XmlWriterSerializeable = requires(T packet, XmlWriter &writer) {
    { packet.toXml(writer) } -> std::same_as<void>;
};
template<typename T>
concept QXmlStreamSerializeable = requires(T packet, QXmlStreamWriter *writer) {
    { packet.toXml(writer) } -> std::same_as<void>;
};

template<typename T>
inline QByteArray serializeXml(const T &packet)
    requires XmlWriterSerializeable<T> || QXmlStreamSerializeable<T>
{
    if constexpr (XmlWriterSerializeable<T>) {
        return serializeXml([&](XmlWriter &w) {
            packet.toXml(w);
        });
    } else if constexpr (QXmlStreamSerializeable<T>) {
        return serializeXml([&](QXmlStreamWriter *w) {
            packet.toXml(w);
        });
    }
}

QXMPP_EXPORT QByteArray generateRandomBytes(size_t minimumByteCount, size_t maximumByteCount);
QXMPP_EXPORT void generateRandomBytes(uint8_t *bytes, size_t byteCount);
float calculateProgress(qint64 transferred, qint64 total);

QXMPP_EXPORT std::pair<QString, int> parseHostAddress(const QString &address);

}  // namespace QXmpp::Private

#endif  // QXMPPUTILS_P_H

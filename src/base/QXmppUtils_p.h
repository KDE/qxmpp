// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPUTILS_P_H
#define QXMPPUTILS_P_H

#include "QXmppGlobal.h"

#include <array>
#include <functional>
#include <optional>
#include <stdint.h>

#include <QByteArray>
#include <QDomElement>

class QDomElement;
class QXmlStreamWriter;
class QXmppNonza;

namespace QXmpp::Private {

// std::array helper
namespace detail {
template<class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
to_array_impl(T (&&a)[N], std::index_sequence<I...>)
{
    return { { std::move(a[I])... } };
}
}  // namespace detail

template<class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N])
{
    return detail::to_array_impl(std::move(a), std::make_index_sequence<N> {});
}

// Helper for Q(Any)StringView overloads that were added later
inline auto toString60(QStringView s)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return s;
#else
    return s.toString();
#endif
}
inline auto toString65(QStringView s)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return s;
#else
    return s.toString();
#endif
}

// QStringLiteral for Qt < 6.5, otherwise uses string view
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#define QSL65(text) u"" text
#else
#define QSL65(text) QStringLiteral(text)
#endif

// Enum parsing
template<typename Enum, std::size_t N>
std::optional<Enum> enumFromString(const std::array<QStringView, N> &values, QStringView str)
{
    auto itr = std::find(values.begin(), values.end(), str);
    if (itr != values.end()) {
        return Enum(std::distance(values.begin(), itr));
    }
    return {};
}

// XML streams
void writeOptionalXmlAttribute(QXmlStreamWriter *stream, QStringView name, QStringView value);
void writeXmlTextElement(QXmlStreamWriter *stream, QStringView name, QStringView value);
void writeXmlTextElement(QXmlStreamWriter *writer, QStringView name, QStringView xmlns, QStringView value);
void writeOptionalXmlTextElement(QXmlStreamWriter *writer, QStringView name, QStringView value);
void writeEmptyElement(QXmlStreamWriter *writer, QStringView name, QStringView xmlns);
template<typename Container>
void writeTextElements(QXmlStreamWriter *writer, QStringView name, const Container &values)
{
    for (const auto &value : values) {
        writeXmlTextElement(writer, name, value);
    }
}
template<typename T>
inline void writeOptional(QXmlStreamWriter *writer, const std::optional<T> &value)
{
    if (value) {
        value->toXml(writer);
    }
}

// Base64
std::optional<QByteArray> parseBase64(const QString &);
inline QString serializeBase64(const QByteArray &data) { return QString::fromUtf8(data.toBase64()); }

// Integer parsing
template<typename Int>
std::optional<Int> parseInt(QStringView str);
template<typename Int>
inline QString serializeInt(Int value) { return QString::number(value); }

// Booleans
std::optional<bool> parseBoolean(const QString &str);
QString serializeBoolean(bool);

//
// DOM
//

QXMPP_EXPORT bool isIqType(const QDomElement &, QStringView tagName, QStringView xmlns);
QXMPP_EXPORT QDomElement firstChildElement(const QDomElement &, QStringView tagName = {}, QStringView xmlNs = {});
QXMPP_EXPORT QDomElement nextSiblingElement(const QDomElement &, QStringView tagName = {}, QStringView xmlNs = {});

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
{
    return parseOptionalElement<T>(firstChildElement(parentEl, tagName, xmlns));
}

template<typename Container = QList<QString>>
auto parseTextElements(const QDomElement &parent, QStringView tagName, QStringView xmlns)
    -> Container
{
    return transform<Container>(iterChildElements(parent, tagName, xmlns), &QDomElement::text);
}

QByteArray serializeXml(const void *packet, void (*toXml)(const void *, QXmlStreamWriter *));
template<typename T>
inline QByteArray serializeXml(const T &packet)
{
    return serializeXml(&packet, [](const void *packet, QXmlStreamWriter *w) {
        std::invoke(&T::toXml, reinterpret_cast<const T *>(packet), w);
    });
}

QXMPP_EXPORT QByteArray generateRandomBytes(size_t minimumByteCount, size_t maximumByteCount);
QXMPP_EXPORT void generateRandomBytes(uint8_t *bytes, size_t byteCount);
float calculateProgress(qint64 transferred, qint64 total);

QXMPP_EXPORT std::pair<QString, int> parseHostAddress(const QString &address);

}  // namespace QXmpp::Private

#endif  // QXMPPUTILS_P_H

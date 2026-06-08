// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppXmlRegistry.h"

#include "QXmppXmlElement.h"

#include "XmlWriter.h"

#include <unordered_map>

#include <QDomElement>
#include <QLoggingCategory>
#include <QReadWriteLock>

using namespace QXmpp::Xml;

using ErasedParser = std::function<std::optional<std::any>(const QDomElement &)>;
using ErasedSerializer = std::function<void(const std::any &, QXmlStreamWriter &)>;

namespace {
Q_LOGGING_CATEGORY(QXMPP_REGISTRY, "qxmpp.xml.registry")
}  // namespace

// Per-entry data stored in the parser map.
struct ParserEntry {
    Scopes scopes;
    ErasedParser parse;
    std::type_index type;
};

// Storage key: (localName, namespaceURI).
struct XmlElementId {
    QString tag;
    QString xmlns;

    bool operator==(const XmlElementId &) const = default;
};

#ifndef QXMPP_DOC
template<>
struct std::hash<XmlElementId> {
    size_t operator()(const XmlElementId &id) const noexcept
    {
        return qHash(id.tag, size_t(0)) ^ (qHash(id.xmlns, size_t(0)) << 1);
    }
};
#endif

// Reverse map: type_index → XmlElementId (for unregister).
struct RegistryState {
    std::unordered_map<XmlElementId, ParserEntry> parsers;
    std::unordered_map<std::type_index, ErasedSerializer> serializers;
    std::unordered_map<std::type_index, XmlElementId> typeToId;
};

static QReadWriteLock s_lock;

static RegistryState &state()
{
    static RegistryState s;
    return s;
}

void Registry::registerInternal(std::type_index type, Scopes scopes,
                                QStringView tag, QStringView xmlns,
                                ErasedParser parse, ErasedSerializer serialize)
{
    XmlElementId id { tag.toString(), xmlns.toString() };

    QWriteLocker lock(&s_lock);
    auto &st = state();

    // Warn on duplicate XmlTag registration (different type, same tag/ns).
    if (auto it = st.parsers.find(id); it != st.parsers.end() && it->second.type != type) {
        qCWarning(QXMPP_REGISTRY) << "Replacing existing registration for"
                                  << tag << "in" << xmlns;
        // Clean up old type's entries.
        st.serializers.erase(it->second.type);
        st.typeToId.erase(it->second.type);
    }

    if (auto it = st.typeToId.find(type); it != st.typeToId.end()) {
        // Same type re-registered: remove old id entry first.
        st.parsers.erase(it->second);
    }

    st.parsers.insert_or_assign(id, ParserEntry { scopes, std::move(parse), type });
    st.serializers.insert_or_assign(type, std::move(serialize));
    st.typeToId.insert_or_assign(type, id);
}

void Registry::unregisterInternal(std::type_index type)
{
    QWriteLocker lock(&s_lock);
    auto &st = state();

    auto idIt = st.typeToId.find(type);
    if (idIt == st.typeToId.end()) {
        return;
    }

    st.parsers.erase(idIt->second);
    st.serializers.erase(type);
    st.typeToId.erase(idIt);
}

std::optional<std::any> Registry::tryParse(Scope scope, const QDomElement &el)
{
    XmlElementId id { el.tagName(), el.namespaceURI() };

    // Copy the parser function under the read lock; call it outside.
    ErasedParser parse;
    {
        QReadLocker lock(&s_lock);
        auto &st = state();
        auto it = st.parsers.find(id);
        if (it == st.parsers.end()) {
            return {};
        }
        // Match: scope must intersect with (requested scope | Generic).
        const Scopes matchMask = Scopes(scope) | Scope::Generic;
        if (!(it->second.scopes & matchMask)) {
            return {};
        }
        parse = it->second.parse;
    }

    return parse(el);
}

void Registry::serializeAll(QXmlStreamWriter &w, const std::vector<std::any> &items)
{
    for (const auto &item : items) {
        // Fallback type: serialize directly without a registry lookup.
        if (item.type() == typeid(QXmpp::Xml::Element)) {
            QXmpp::Private::XmlWriter writer(&w);
            std::any_cast<const QXmpp::Xml::Element &>(item).toXml(writer);
            continue;
        }

        ErasedSerializer fn;
        {
            QReadLocker lock(&s_lock);
            auto &st = state();
            auto it = st.serializers.find(std::type_index(item.type()));
            if (it != st.serializers.end()) {
                fn = it->second;
            }
        }

        if (fn) {
            fn(item, w);
        } else {
            qCWarning(QXMPP_REGISTRY) << "Cannot serialize extension of unregistered type"
                                      << item.type().name();
        }
    }
}

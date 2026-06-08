// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLREGISTRY_H
#define QXMPPXMLREGISTRY_H

#include "QXmppGlobal.h"
#include "QXmppUtils_p.h"
#include "QXmppXmlElementScope.h"

#include <any>
#include <functional>
#include <optional>
#include <typeindex>
#include <vector>

#include <QStringView>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp::Xml {

/*!
    \inmodule QXmpp

    \brief A global registry for type-driven parsing and serialization of XML extension elements.

    Registered types are matched by their \c XmlTag static member. When a stanza's
    \c parseExtension hook encounters an unknown element, Registry::tryParse is
    consulted. The first matching entry returns a typed \c std::any value which is
    stored in the stanza's QXmpp::Xml::Extensions container.

    The registry is a process-wide singleton protected by a read-write lock.
    Lookup takes roughly 30 ns and is negligible compared to XML parse costs.

    \section1 Registration

    Register a type once at application startup:
    \code
    QXmpp::Xml::Registry::registerElement<MyFlag>(QXmpp::Xml::Scope::Message);
    // Generic scope matches in every stanza context:
    QXmpp::Xml::Registry::registerElement<MyDebugTag>(QXmpp::Xml::Scope::Generic);
    \endcode

    \section1 Constraints on T

    T must satisfy:
    \list
    \li \c T::XmlTag — a tag/namespace pair (see \c QXmpp::Private::HasXmlTag).
    \li One of the three parse styles (\c QXmpp::Private::DomParsable):
        \list
        \li \c static std::optional<T> T::fromDom(const QDomElement &)
        \li \c bool T::parse(const QDomElement &)
        \li \c void T::parse(const QDomElement &)
        \endlist
    \li \c void T::toXml(QXmlStreamWriter *) const (serialization).
    \endlist

    \since QXmpp 1.16
*/
class QXMPP_EXPORT Registry
{
public:
    /*!
        Registers \c T for the given \a scopes.

        If a type with the same \c XmlTag is already registered, the previous
        entry is replaced and a warning is emitted.
    */
    template<typename T>
        requires QXmpp::Private::HasXmlTag<T> &&
        QXmpp::Private::DomParsable<T> &&
        QXmpp::Private::QXmlStreamSerializeable<T>
    static void registerElement(Scopes scopes)
    {
        ErasedParser parse = [](const QDomElement &el) -> std::optional<std::any> {
            auto result = QXmpp::Private::parseElement<T>(el);
            if (!result) {
                return {};
            }
            return std::any(std::move(*result));
        };
        ErasedSerializer serialize = [](const std::any &val, QXmlStreamWriter &w) {
            std::any_cast<const T &>(val).toXml(&w);
        };
        auto [xmlTag, xmlNs] = T::XmlTag;
        registerInternal(std::type_index(typeid(T)), scopes,
                         xmlTag, xmlNs,
                         std::move(parse), std::move(serialize));
    }

    /*!
        Removes the registration for \c T.

        After this call, unknown elements with T's tag/namespace are stored as a
        QXmpp::Xml::Element fallback instead.
    */
    template<typename T>
    static void unregisterElement()
    {
        unregisterInternal(std::type_index(typeid(T)));
    }

    /*!
        Attempts to parse \a el as a registered extension type for \a scope.

        Returns a typed \c std::any on success, or \c std::nullopt if no
        matching registration is found.
    */
    static std::optional<std::any> tryParse(Scope scope, const QDomElement &el);

    /*!
        Serializes all \a items to \a w by dispatching to each item's registered
        serializer. QXmpp::Xml::Element fallback items are handled directly.
        Items with no registered serializer emit a warning and are skipped.
    */
    static void serializeAll(QXmlStreamWriter &w, const std::vector<std::any> &items);

private:
    using ErasedParser = std::function<std::optional<std::any>(const QDomElement &)>;
    using ErasedSerializer = std::function<void(const std::any &, QXmlStreamWriter &)>;

    static void registerInternal(std::type_index type, Scopes scopes,
                                 QStringView tag, QStringView xmlns,
                                 ErasedParser parse, ErasedSerializer serialize);
    static void unregisterInternal(std::type_index type);
};

}  // namespace QXmpp::Xml

#endif  // QXMPPXMLREGISTRY_H

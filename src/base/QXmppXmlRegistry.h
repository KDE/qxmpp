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

#include <QList>
#include <QStringView>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp::Xml {

/*!
    \enum QXmpp::Xml::Sensitivity

    Whether an element is encrypted with \xep{0420}{Stanza Content Encryption} or sent in the
    public part of an encrypted stanza.

    \value Sensitive Encrypted. Unknown elements are handled as sensitive.
    \value Public Sent unencrypted, e.g. elements needed for routing or the encrypted element
    itself.

    \since QXmpp 1.18
*/
enum class Sensitivity : uint8_t {
    Sensitive,
    Public,
};

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

        \a sensitivity decides whether the element is encrypted in end-to-end encrypted stanzas
        (since QXmpp 1.18).

        If a type with the same \c XmlTag is already registered, the previous
        entry is replaced and a warning is emitted.
    */
    template<typename T>
        requires QXmpp::Private::HasXmlTag<T> &&
        QXmpp::Private::DomParsable<T> &&
        QXmpp::Private::QXmlStreamSerializeable<T>
    static void registerElement(Scopes scopes, Sensitivity sensitivity = Sensitivity::Sensitive)
    {
        registerElementInternal<T>(scopes, sensitivity, false);
    }

    /*!
        Registers \c T as the element containing the encrypted content of an end-to-end
        encryption.

        The encryption is identified by the namespace of \c T, see QXmpp::E2ee. The element is
        public. Stanzas containing it are handled as encrypted. \a scopes are the stanzas the
        element can occur in.

        \since QXmpp 1.18
    */
    template<typename T>
        requires QXmpp::Private::HasXmlTag<T> &&
        QXmpp::Private::DomParsable<T> &&
        QXmpp::Private::QXmlStreamSerializeable<T>
    static void registerEncryptedContainer(Scopes scopes)
    {
        registerElementInternal<T>(scopes, Sensitivity::Public, true);
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

    static Sensitivity sensitivity(const std::any &value);
    static QStringView encryption(const QDomElement &stanza);
    static QList<QStringView> encryptions(const QDomElement &stanza);
    static QStringView encryption(const std::any &value);

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

    template<typename T>
    static void registerElementInternal(Scopes scopes, Sensitivity sensitivity, bool encryptedContainer)
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
                         std::move(parse), std::move(serialize), sensitivity, encryptedContainer);
    }

    static void registerInternal(std::type_index type, Scopes scopes,
                                 QStringView tag, QStringView xmlns,
                                 ErasedParser parse, ErasedSerializer serialize,
                                 Sensitivity sensitivity, bool encryptedContainer);
    static void unregisterInternal(std::type_index type);
};

}  // namespace QXmpp::Xml

#endif  // QXMPPXMLREGISTRY_H

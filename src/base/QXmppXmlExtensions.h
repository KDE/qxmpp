// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLEXTENSIONS_H
#define QXMPPXMLEXTENSIONS_H

#include "QXmppGlobal.h"
#include "QXmppXmlElementScope.h"

#include <any>
#include <optional>
#include <typeindex>
#include <vector>

#include <QSharedDataPointer>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp::Private {
struct ExtensionsPrivate;
}

namespace QXmpp::Xml {

/*!
    \inmodule QXmpp

    \brief A type-safe, ordered container for XML extension elements.

    Extensions stores zero or more XML extension values as type-erased \c std::any
    entries. Values are added either by the stanza's \c parseExtension hook via
    QXmpp::Xml::Registry (typed entries) or as a QXmpp::Xml::Element fallback for
    unknown elements. Values can be accessed, iterated, and mutated by the
    application without knowledge of the concrete type used internally.

    The container follows value semantics with implicit sharing (copy-on-write).

    \since QXmpp 1.16
*/
class QXMPP_EXPORT Extensions
{
public:
    Extensions();
    Extensions(const Extensions &);
    Extensions(Extensions &&) noexcept;
    ~Extensions();
    Extensions &operator=(const Extensions &);
    Extensions &operator=(Extensions &&) noexcept;

    /*!
        Returns the first value of type \c T, or \c std::nullopt if none is present.
    */
    template<typename T>
    std::optional<T> get() const
    {
        for (const auto &item : items()) {
            if (const auto *val = std::any_cast<T>(&item)) {
                return *val;
            }
        }
        return {};
    }

    /*!
        Returns all values of type \c T.
    */
    template<typename T>
    std::vector<T> getAll() const
    {
        std::vector<T> result;
        for (const auto &item : items()) {
            if (const auto *val = std::any_cast<T>(&item)) {
                result.push_back(*val);
            }
        }
        return result;
    }

    /*!
        Appends \a value.

        For \a value to be serialized when the stanza is sent, its type must be
        registered via QXmpp::Xml::Registry (the QXmpp::Xml::Element fallback type
        is the exception and is always serializable). Unregistered types are kept
        in the container but skipped during serialization with a warning.
    */
    template<typename T>
    void add(T value)
    {
        push(std::any(std::move(value)));
    }

    /*!
        Removes the first value of type \c T and returns \c true if one was found.
    */
    template<typename T>
    bool remove()
    {
        return removeFirst(std::type_index(typeid(T)));
    }

    /*!
        Removes all values of type \c T and returns how many were removed.
    */
    template<typename T>
    int removeAll()
    {
        return removeAllOf(std::type_index(typeid(T)));
    }

    /*!
        Returns \c true if at least one value of type \c T is present.
    */
    template<typename T>
    bool contains() const
    {
        for (const auto &item : items()) {
            if (item.type() == typeid(T)) {
                return true;
            }
        }
        return false;
    }

    using const_iterator = std::vector<std::any>::const_iterator;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    int size() const noexcept;
    bool isEmpty() const noexcept;
    void clear();

    // Internal: called from QXmppMessage::parseExtension / QXmppPresence::parseExtension.
    bool tryParseAndAdd(Scope scope, const QDomElement &el);
    void toXml(QXmlStreamWriter *w) const;

private:
    const std::vector<std::any> &items() const;
    void push(std::any value);
    bool removeFirst(std::type_index type);
    int removeAllOf(std::type_index type);

    QSharedDataPointer<QXmpp::Private::ExtensionsPrivate> d;
};

}  // namespace QXmpp::Xml

#endif  // QXMPPXMLEXTENSIONS_H

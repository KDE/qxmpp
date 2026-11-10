// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppXmlExtensions.h"

#include "QXmppXmlElement.h"
#include "QXmppXmlExtensions_p.h"
#include "QXmppXmlRegistry.h"

#include <algorithm>

namespace QXmpp::Xml {

Extensions::Extensions()
    : d(new QXmpp::Private::ExtensionsPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(Extensions)

const std::vector<std::any> &Extensions::items() const
{
    return d->items;
}

void Extensions::push(std::any value)
{
    d->items.push_back(std::move(value));
}

bool Extensions::removeFirst(std::type_index type)
{
    auto &items = d->items;
    auto it = std::find_if(items.begin(), items.end(), [type](const std::any &a) {
        return std::type_index(a.type()) == type;
    });
    if (it == items.end()) {
        return false;
    }
    items.erase(it);
    return true;
}

int Extensions::removeAllOf(std::type_index type)
{
    auto &items = d->items;
    int count = 0;
    items.erase(std::remove_if(items.begin(), items.end(), [type, &count](const std::any &a) {
                    if (std::type_index(a.type()) == type) {
                        ++count;
                        return true;
                    }
                    return false;
                }),
                items.end());
    return count;
}

Extensions::const_iterator Extensions::begin() const noexcept
{
    return d->items.begin();
}

Extensions::const_iterator Extensions::end() const noexcept
{
    return d->items.end();
}

int Extensions::size() const noexcept
{
    return int(d->items.size());
}

bool Extensions::isEmpty() const noexcept
{
    return d->items.empty();
}

void Extensions::clear()
{
    d->items.clear();
}

/*!
    Appends all values of \a other.

    \since QXmpp 1.18
*/
void Extensions::append(const Extensions &other)
{
    if (&other == this) {
        auto copy = other.d->items;
        d->items.insert(d->items.end(), copy.begin(), copy.end());
        return;
    }
    d->items.insert(d->items.end(), other.d->items.begin(), other.d->items.end());
}

/*!
    Parses \a el as a registered extension type and adds it to the container.

    If the registry has no matching registration for \a scope, the element is
    stored as a QXmpp::Xml::Element fallback so that round-trip serialization
    is preserved.

    \return \c true always (the element is always consumed).
*/
bool Extensions::tryParseAndAdd(Scope scope, const QDomElement &el)
{
    if (auto result = Registry::tryParse(scope, el)) {
        push(std::move(*result));
    } else {
        push(std::any(Element::fromDom(el)));
    }
    return true;
}

/*!
    Like tryParseAndAdd(), but for parsing a part of an end-to-end encrypted stanza.

    With QXmpp::ScePublic (the unencrypted part of an encrypted stanza), sensitive elements are
    added to \a publicExtensions instead, so they can't be mistaken for encrypted content.
*/
bool Extensions::tryParseAndAdd(Scope scope, const QDomElement &el, SceMode sceMode, Extensions &publicExtensions)
{
    auto parsed = Registry::tryParse(scope, el);
    auto value = parsed ? std::move(*parsed) : std::any(Element::fromDom(el));
    if (sceMode == ScePublic && Registry::sensitivity(value) == Sensitivity::Sensitive) {
        publicExtensions.push(std::move(value));
    } else {
        push(std::move(value));
    }
    return true;
}

/*!
    Serializes all stored extensions to \a w.

    Delegates to QXmpp::Xml::Registry::serializeAll for dispatch.
*/
void Extensions::toXml(QXmlStreamWriter *w) const
{
    Registry::serializeAll(*w, d->items);
}

/*!
    Removes all values for which \a predicate returns true and returns how many were removed.
*/
int Extensions::removeIf(const std::function<bool(const std::any &)> &predicate)
{
    return int(std::erase_if(d->items, predicate));
}

/*!
    Serializes the stored extensions of the part \a sceMode of an encrypted stanza to \a w.
*/
void Extensions::toXml(QXmlStreamWriter *w, SceMode sceMode) const
{
    if (sceMode == SceAll) {
        Registry::serializeAll(*w, d->items);
        return;
    }
    const auto wanted = sceMode == ScePublic ? Sensitivity::Public : Sensitivity::Sensitive;
    std::vector<std::any> items;
    std::copy_if(d->items.cbegin(), d->items.cend(), std::back_inserter(items), [&](const std::any &item) {
        return Registry::sensitivity(item) == wanted;
    });
    Registry::serializeAll(*w, items);
}

}  // namespace QXmpp::Xml

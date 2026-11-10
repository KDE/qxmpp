// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppHooks.h"

#include "QXmppHooks_p.h"

#include "StringLiterals.h"

#include <algorithm>
#include <cstring>

#include <QLoggingCategory>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace {
Q_LOGGING_CATEGORY(QXMPP_HOOKS, "org.qxmpp.hooks")

bool sameSignature(const char *a, const char *b)
{
    return a == b || std::strcmp(a, b) == 0;
}
}  // namespace

/*!
    \class QXmpp::Hooks
    \inmodule QXmpp

    \brief Registers handlers on hooks and runs hooks.

    Hooks connect extensions without requiring a common base class or virtual functions in
    public headers. Any extension (or the application) can define a hook by declaring a type that
    satisfies QXmpp::HookDefinition. Other extensions add handlers to it using the hook's type.
    Nothing needs to be declared in QXmpp itself and handlers can be added before the extension
    defining the hook is added to the client.

    A Hooks object is a lightweight handle to the hooks of a client together with an owner.
    Handlers added through it are removed when the owner is destroyed. Extensions usually add
    their handlers in their constructor using QXmppClientExtension::hooks():

    \code
    MyManager::MyManager()
        : d(std::make_unique<MyManagerPrivate>())
    {
        hooks().add<QXmpp::Muc::AffiliationChanged>(d.get(), &MyManagerPrivate::onAffiliationChanged);
    }
    \endcode

    The extension defining a hook runs it:

    \code
    hooks().run<QXmpp::Muc::AffiliationChanged>(roomJid, item);
    \endcode

    To collect values from multiple handlers (a fold), pass a mutable reference as argument.

    Hook names should be prefixed to avoid collisions, QXmpp uses "org.qxmpp.". Handlers with a
    signature differing from the one of the existing handlers of the same hook are rejected.

    \since QXmpp 1.18
*/

/*!
    Constructs a handle to the hooks in \a registry with \a owner as the owner of added handlers.

    Prefer QXmppClient::hooks() and QXmppClientExtension::hooks() over this constructor.
*/
Hooks::Hooks(HookRegistry *registry, QObject *owner)
    : m_registry(registry), m_owner(owner)
{
}

/*!
    \internal

    Constructs a handle to the hooks in \a registry with \a owner as the owner of added handlers.

    Added handlers are also stored in \a ownerRegistry, which keeps them independently of
    \a registry. Extensions use this to keep their handlers when they are added to a client later.
*/
Hooks::Hooks(HookRegistry *registry, HookRegistry *ownerRegistry, QObject *owner)
    : m_registry(registry), m_ownerRegistry(ownerRegistry), m_owner(owner)
{
}

/*!
    Removes all handlers that have been added with the owner of this object.
*/
void Hooks::removeAll() const
{
    if (!m_owner) {
        return;
    }
    if (m_registry) {
        m_registry->removeAll(m_owner);
    }
    if (m_ownerRegistry) {
        m_ownerRegistry->removeAll(m_owner);
    }
}

void Hooks::addInternal(QStringView name, const std::type_info &signature, int sequence, std::shared_ptr<void> function) const
{
    if (!m_registry && !m_ownerRegistry) {
        qCWarning(QXMPP_HOOKS) << "Cannot add handler for hook" << name << "without a client.";
        return;
    }
    if (!m_owner) {
        qCWarning(QXMPP_HOOKS) << "Cannot add handler for hook" << name << "without an owner.";
        return;
    }
    if (m_ownerRegistry) {
        m_ownerRegistry->add(name, signature, sequence, m_owner, function);
    }
    if (m_registry) {
        m_registry->add(name, signature, sequence, m_owner, std::move(function));
    }
}

bool Hooks::runInternal(QStringView name, const std::type_info &signature, const std::function<bool(void *)> &invoke) const
{
    return m_registry && m_registry->run(name, signature, invoke);
}

bool Hooks::hasHandlersInternal(QStringView name) const
{
    return m_registry && m_registry->hasHandlers(name);
}

void HookRegistry::add(QStringView name, const std::type_info &signature, int sequence, QObject *owner, std::shared_ptr<void> function)
{
    add(name, signature.name(), sequence, owner, std::move(function));
}

// Adds all handlers of \a other, keeping their owners and order.
void HookRegistry::addAll(const HookRegistry &other)
{
    for (auto itr = other.m_hooks.cbegin(); itr != other.m_hooks.cend(); ++itr) {
        for (const auto &entry : *itr.value()) {
            if (!entry->removed) {
                add(itr.key(), entry->signature, entry->sequence, entry->owner, entry->function);
            }
        }
    }
}

void HookRegistry::add(QStringView name, const char *signature, int sequence, QObject *owner, std::shared_ptr<void> function)
{
    // the names of hooks have static storage (Hook::Name), so the key needs no copy
    const auto key = staticString(name);
    const auto current = m_hooks.value(key);

    if (current) {
        for (const auto &entry : *current) {
            if (!sameSignature(entry->signature, signature)) {
                qCWarning(QXMPP_HOOKS) << "Rejected handler for hook" << name << "with a different signature than the existing handlers.";
                return;
            }
        }
    }

    auto entries = current ? EntryList(*current) : EntryList();
    // insert after all entries with a lower or equal sequence to keep the order of addition
    auto position = std::ranges::upper_bound(entries, sequence, {}, [](const auto &entry) { return entry->sequence; });
    entries.insert(position, std::make_shared<Entry>(Entry { sequence, owner, signature, std::move(function) }));
    m_hooks.insert(key, std::make_shared<const EntryList>(std::move(entries)));

    if (!m_owners.contains(owner)) {
        auto connection = connect(owner, &QObject::destroyed, this, [this, owner] {
            removeAll(owner);
        });
        m_owners.insert(owner, connection);
    }
}

bool HookRegistry::run(QStringView name, const std::type_info &signature, const std::function<bool(void *)> &invoke) const
{
    // keep a reference to the current list: handlers may modify the registry
    const auto entries = m_hooks.value(staticString(name));
    if (!entries) {
        return false;
    }

    for (const auto &entry : *entries) {
        if (entry->removed) {
            continue;
        }
        if (!sameSignature(entry->signature, signature.name())) {
            qCWarning(QXMPP_HOOKS) << "Hook" << name << "was run with a different signature than its handlers.";
            return false;
        }
        // keep the function alive in case the handler removes itself
        const auto function = entry->function;
        if (invoke(function.get())) {
            return true;
        }
    }
    return false;
}

bool HookRegistry::hasHandlers(QStringView name) const
{
    const auto entries = m_hooks.value(staticString(name));
    return entries && std::ranges::any_of(*entries, [](const auto &entry) { return !entry->removed; });
}

void HookRegistry::removeAll(QObject *owner)
{
    if (auto connection = m_owners.take(owner)) {
        disconnect(connection);
    }

    for (auto itr = m_hooks.begin(); itr != m_hooks.end();) {
        const auto &entries = *itr.value();
        if (std::ranges::none_of(entries, [=](const auto &entry) { return entry->owner == owner; })) {
            ++itr;
            continue;
        }

        EntryList remaining;
        for (const auto &entry : entries) {
            if (entry->owner == owner) {
                // running hooks still reference the entry
                entry->removed = true;
            } else {
                remaining.push_back(entry);
            }
        }

        if (remaining.empty()) {
            itr = m_hooks.erase(itr);
        } else {
            itr.value() = std::make_shared<const EntryList>(std::move(remaining));
            ++itr;
        }
    }
}

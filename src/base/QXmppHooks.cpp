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

// Result of a handler of an asynchronous hook. It is finished with "continue" if the owner of the
// handler is destroyed or the task of the handler is dropped, so the remaining handlers still run.
struct GuardedResult {
    QXmppPromise<bool> promise;
    QMetaObject::Connection ownerDestroyed;
    bool finished = false;

    void finish(bool stop)
    {
        if (!finished) {
            finished = true;
            QObject::disconnect(ownerDestroyed);
            promise.finish(stop);
        }
    }
};

// Finishes the result when the continuation is destroyed without having been called.
struct GuardedResultFinisher {
    std::shared_ptr<GuardedResult> result;

    explicit GuardedResultFinisher(std::shared_ptr<GuardedResult> result) : result(std::move(result)) { }
    GuardedResultFinisher(GuardedResultFinisher &&) = default;
    GuardedResultFinisher(const GuardedResultFinisher &) = delete;
    ~GuardedResultFinisher()
    {
        if (result) {
            result->finish(false);
        }
    }
};

QXmppTask<bool> guardHandler(QXmppTask<bool> task, QObject *owner)
{
    if (task.isFinished()) {
        return task;
    }

    auto result = std::make_shared<GuardedResult>();
    auto resultTask = result->promise.task();
    result->ownerDestroyed = QObject::connect(owner, &QObject::destroyed, [weak = std::weak_ptr(result)] {
        if (auto result = weak.lock()) {
            result->finish(false);
        }
    });
    task.then(owner, [finisher = GuardedResultFinisher(result)](bool stop) {
        finisher.result->finish(stop);
    });
    return resultTask;
}

QXmppTask<bool> runHandlers(std::shared_ptr<const HookRegistry::EntryList> entries, std::function<QXmppTask<bool>(void *)> invoke, bool singleResponder)
{
    for (const auto &entry : *entries) {
        // also set if the owner has been destroyed while an earlier handler was running
        if (entry->removed) {
            continue;
        }
        // keep the function alive in case the handler removes itself
        const auto function = entry->function;
        const bool stop = co_await guardHandler(invoke(function.get()), entry->owner);
        // a single responder is not replaced by the next handler if its result is lost
        if (stop || singleResponder) {
            co_return stop;
        }
    }
    co_return false;
}
}  // namespace

/*!
    \class QXmpp::Hooks
    \inmodule QXmpp

    \brief Registers handlers on hooks and runs hooks.

    Hooks connect extensions without requiring a common base class or virtual functions in
    public headers. Any extension (or the application) can define a hook by declaring a type.
    Other extensions add handlers to it using the hook's type. Nothing needs to be declared in
    QXmpp itself and handlers can be added before the extension defining the hook is added to the
    client.

    A hook is defined by a type with a static \c Name and a \c Signature. The name identifies the
    hook, the signature is the type of the handlers. The return type of the signature decides how
    the handlers are run:
    \list
    \li \c void: all handlers are run.
    \li QXmpp::HookResult: the handlers are run until one returns QXmpp::HookResult::Stop.
    \li Any other type: the hook has a single responder. Only the first handler is run and run()
        returns its result as \c std::optional, \c std::nullopt if there is no handler. Adding
        another handler logs a warning, it is still added and the sequence decides which one is
        used, e.g. an application can replace a handler of QXmpp with HookSequence::Early.
    \endlist

    Hooks whose signature returns a QXmppTask are asynchronous: a handler can wait for a result
    (e.g. a database query) before the next handler is run. Handlers of asynchronous hooks may
    also return the value directly. If the owner of the single responder of an asynchronous hook
    is destroyed or its task is dropped, the result is \c std::nullopt; the next handler is not
    run instead.

    Names use a reverse domain prefix, so hooks of different projects don't collide. Hooks of
    QXmpp start with \c org.qxmpp.

    \code
    struct AffiliationChanged {
        static constexpr QStringView Name = u"org.qxmpp.muc.affiliation-changed";
        using Signature = void(const QString &roomJid, const QXmpp::Muc::Item &item);
    };
    \endcode

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

    To collect values from multiple handlers (a fold), pass a mutable reference as argument, like
    QXmpp::Disco::CollectInfo does.

    \section1 Keys

    A hook can have a \c Key type. Handlers are then added for a key and run() only runs the
    handlers of the given key. Keys can be strings, enums of QXmpp with a string representation
    and tuples of them, e.g. the tag name and namespace of an element.

    \code
    struct EventReceived {
        static constexpr QStringView Name = u"org.qxmpp.pubsub.event-received";
        using Key = QStringView;
        using Signature = QXmpp::HookResult(const QDomElement &element, const QString &service, const QString &node);
    };

    hooks().add<EventReceived>(u"urn:xmpp:mix:nodes:info", this, &MyManager::handleEvent);
    hooks().run<EventReceived>(node, element, service, node);
    \endcode

    \section1 Adapted hooks

    A type with a \c Target hook and a static \c adapt() function adds typed handlers to another
    hook, e.g. a hook working on serialized data. \c adapt() turns the handler into a handler of
    the target. For a keyed target, the static \c key() function returns the key. Handlers are
    added with add() like to other hooks, the target is run with run():

    \code
    struct ConfigChanged {
        static constexpr QStringView Name = u"org.example.config-changed";
        using Key = QString;
        using Signature = void(const QString &json);
    };

    template<typename T>
    struct Changed {
        using Target = ConfigChanged;
        static QString key() { return T::Id; }

        template<typename Handler>
            requires std::invocable<Handler &, T>
        static auto adapt(Handler &&handler)
        {
            return [handler = std::forward<Handler>(handler)](const QString &json) mutable {
                handler(T::fromJson(json));
            };
        }
    };

    hooks().add<Changed<ProxySettings>>([](ProxySettings &&settings) { ... });
    \endcode

    Hook names should be prefixed to avoid collisions, QXmpp uses "org.qxmpp.". Handlers with a
    signature or key type differing from the one of the existing handlers of the same hook are
    rejected.

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

void Hooks::addInternal(QStringView name, const QString &key, const std::type_info &signature, int sequence, std::shared_ptr<void> function, bool singleResponder) const
{
    if (!m_registry && !m_ownerRegistry) {
        qCWarning(QXMPP_HOOKS) << "Cannot add handler for hook" << name << "without a client.";
        return;
    }
    if (!m_owner) {
        qCWarning(QXMPP_HOOKS) << "Cannot add handler for hook" << name << "without an owner.";
        return;
    }
    // duplicates are reported once, by the client's registry if there is one
    if (m_ownerRegistry) {
        m_ownerRegistry->add(name, key, signature, sequence, m_owner, function, singleResponder, !m_registry);
    }
    if (m_registry) {
        m_registry->add(name, key, signature, sequence, m_owner, std::move(function), singleResponder, true);
    }
}

bool Hooks::runInternal(QStringView name, const QString &key, const std::type_info &signature, const std::function<bool(void *)> &invoke) const
{
    return m_registry && m_registry->run(name, key, signature, invoke);
}

QXmppTask<bool> Hooks::runAsyncInternal(QStringView name, const QString &key, const std::type_info &signature, std::function<QXmppTask<bool>(void *)> invoke, bool singleResponder) const
{
    if (!m_registry) {
        return makeReadyHookTask<bool>(false);
    }
    return m_registry->runAsync(name, key, signature, std::move(invoke), singleResponder);
}

bool Hooks::hasHandlersInternal(QStringView name, const std::optional<QString> &key) const
{
    return m_registry && m_registry->hasHandlers(name, key);
}

void HookRegistry::add(QStringView name, const QString &key, const std::type_info &signature, int sequence, QObject *owner, std::shared_ptr<void> function, bool singleResponder, bool warnDuplicate)
{
    add(name, key, signature.name(), sequence, owner, std::move(function), singleResponder, warnDuplicate);
}

// Adds all handlers of \a other, keeping their owners and order.
void HookRegistry::addAll(const HookRegistry &other)
{
    for (auto hook = other.m_hooks.cbegin(); hook != other.m_hooks.cend(); ++hook) {
        for (auto list = hook->handlers.cbegin(); list != hook->handlers.cend(); ++list) {
            for (const auto &entry : *list.value()) {
                if (!entry->removed) {
                    add(hook.key(), list.key(), entry->signature, entry->sequence, entry->owner, entry->function, entry->singleResponder, true);
                }
            }
        }
    }
}

void HookRegistry::add(QStringView name, const QString &key, const char *signature, int sequence, QObject *owner, std::shared_ptr<void> function, bool singleResponder, bool warnDuplicate)
{
    // the names of hooks have static storage (Hook::Name), so the key needs no copy
    auto &hook = m_hooks[staticString(name)];
    if (hook.signature && !sameSignature(hook.signature, signature)) {
        qCWarning(QXMPP_HOOKS) << "Rejected handler for hook" << name << "with a different signature than the existing handlers.";
        if (hook.handlers.isEmpty()) {
            m_hooks.remove(staticString(name));
        }
        return;
    }
    hook.signature = signature;

    const auto current = hook.handlers.value(key);
    if (singleResponder && warnDuplicate && current && !current->empty()) {
        const auto hook = key.isNull() ? name.toString() : name + u" (key " + key + u')';
        qCWarning(QXMPP_HOOKS).noquote() << u"Hook " + hook + u" already has a handler, only the first one by sequence is used.";
    }

    auto entries = current ? EntryList(*current) : EntryList();
    // insert after all entries with a lower or equal sequence to keep the order of addition
    auto position = std::ranges::upper_bound(entries, sequence, {}, [](const auto &entry) { return entry->sequence; });
    entries.insert(position, std::make_shared<Entry>(Entry { sequence, owner, signature, std::move(function), singleResponder }));
    hook.handlers.insert(key, std::make_shared<const EntryList>(std::move(entries)));

    if (!m_owners.contains(owner)) {
        auto connection = connect(owner, &QObject::destroyed, this, [this, owner] {
            removeAll(owner);
        });
        m_owners.insert(owner, connection);
    }
}

// The handlers of the hook for the key, null if there are none or the signature does not match.
std::shared_ptr<const HookRegistry::EntryList> HookRegistry::entries(QStringView name, const QString &key, const char *signature) const
{
    const auto hook = m_hooks.constFind(staticString(name));
    if (hook == m_hooks.cend()) {
        return {};
    }
    if (!sameSignature(hook->signature, signature)) {
        qCWarning(QXMPP_HOOKS) << "Hook" << name << "was run with a different signature than its handlers.";
        return {};
    }
    return hook->handlers.value(key);
}

bool HookRegistry::run(QStringView name, const QString &key, const std::type_info &signature, const std::function<bool(void *)> &invoke) const
{
    // keep a reference to the current list: handlers may modify the registry
    const auto list = entries(name, key, signature.name());
    if (!list) {
        return false;
    }

    for (const auto &entry : *list) {
        if (entry->removed) {
            continue;
        }
        // keep the function alive in case the handler removes itself
        const auto function = entry->function;
        if (invoke(function.get())) {
            return true;
        }
    }
    return false;
}

QXmppTask<bool> HookRegistry::runAsync(QStringView name, const QString &key, const std::type_info &signature, std::function<QXmppTask<bool>(void *)> invoke, bool singleResponder) const
{
    // the handlers run on a snapshot that stays valid if the registry is destroyed
    auto list = entries(name, key, signature.name());
    if (!list) {
        return makeReadyHookTask<bool>(false);
    }
    return runHandlers(std::move(list), std::move(invoke), singleResponder);
}

bool HookRegistry::hasHandlers(QStringView name, const std::optional<QString> &key) const
{
    const auto hook = m_hooks.constFind(staticString(name));
    if (hook == m_hooks.cend()) {
        return false;
    }
    auto hasEntries = [](const std::shared_ptr<const EntryList> &list) {
        return list && std::ranges::any_of(*list, [](const auto &entry) { return !entry->removed; });
    };
    if (key) {
        return hasEntries(hook->handlers.value(*key));
    }
    return std::ranges::any_of(hook->handlers, hasEntries);
}

void HookRegistry::removeAll(QObject *owner)
{
    if (auto connection = m_owners.take(owner)) {
        disconnect(connection);
    }

    for (auto hook = m_hooks.begin(); hook != m_hooks.end();) {
        auto &handlers = hook->handlers;
        for (auto list = handlers.begin(); list != handlers.end();) {
            const auto &entries = *list.value();
            if (std::ranges::none_of(entries, [=](const auto &entry) { return entry->owner == owner; })) {
                ++list;
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
                list = handlers.erase(list);
            } else {
                list.value() = std::make_shared<const EntryList>(std::move(remaining));
                ++list;
            }
        }

        // without handlers, the hook can be added again with another signature
        if (handlers.isEmpty()) {
            hook = m_hooks.erase(hook);
        } else {
            ++hook;
        }
    }
}

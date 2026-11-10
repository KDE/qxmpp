// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHOOKS_H
#define QXMPPHOOKS_H

#include "QXmppGlobal.h"

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeinfo>

#include <QStringView>

class QObject;

namespace QXmpp {

/*!
    \enum QXmpp::HookResult
    \inmodule QXmpp

    Result of a handler of a hook whose signature returns HookResult.

    \value Continue The remaining handlers are run.
    \value Stop No further handlers are run.

    \since QXmpp 1.18
*/
enum class HookResult : uint8_t {
    Continue,
    Stop,
};

/*!
    \namespace QXmpp::HookSequence
    \inmodule QXmpp

    Common sequence numbers for hook handlers. Handlers with a lower sequence number run first,
    handlers with the same sequence number run in the order they were added.

    \since QXmpp 1.18
*/
namespace HookSequence {
constexpr int First = 0;
constexpr int Early = 25;
constexpr int Default = 50;
constexpr int Late = 75;
constexpr int Last = 100;
}  // namespace HookSequence

namespace Private {

class HookRegistry;

template<typename Signature>
struct HookSignature;

template<typename R, typename... Args>
struct HookSignature<R(Args...)> {
    using Result = R;
    using Function = std::function<R(Args...)>;
};

template<typename Hook>
using HookResultOf = typename HookSignature<typename Hook::Signature>::Result;

}  // namespace Private

/*!
    \concept QXmpp::HookDefinition
    \inmodule QXmpp

    A hook is defined by a type with a static \c Name and a \c Signature. The name identifies the
    hook, the signature is the type of the handlers. Handlers return either \c void (all handlers
    are run) or QXmpp::HookResult (handlers can stop the processing).

    Names use a reverse domain prefix, so hooks of different projects don't collide. Hooks of
    QXmpp start with \c org.qxmpp.

    \code
    struct AffiliationChanged {
        static constexpr QStringView Name = u"org.qxmpp.muc.affiliation-changed";
        using Signature = void(const QString &roomJid, const QXmpp::Muc::Item &item);
    };
    \endcode

    \since QXmpp 1.18
*/
template<typename T>
concept HookDefinition = requires {
    { T::Name } -> std::convertible_to<QStringView>;
    typename Private::HookResultOf<T>;
} && (std::is_void_v<Private::HookResultOf<T>> || std::same_as<Private::HookResultOf<T>, HookResult>);

class QXMPP_EXPORT Hooks
{
public:
    Hooks(Private::HookRegistry *registry, QObject *owner);
    Hooks(Private::HookRegistry *registry, Private::HookRegistry *ownerRegistry, QObject *owner);

    /*!
        Adds \a handler to the hook \c Hook.

        The handler is removed when the owner of this Hooks object is destroyed or, for
        QXmppClientExtension, when the extension is removed from the client.
        \a sequence decides the position of the handler, see QXmpp::HookSequence.
    */
    template<HookDefinition Hook, typename Handler>
        requires std::constructible_from<typename Private::HookSignature<typename Hook::Signature>::Function, Handler>
    void add(Handler &&handler, int sequence = HookSequence::Default) const
    {
        using Function = typename Private::HookSignature<typename Hook::Signature>::Function;
        addInternal(Hook::Name, typeid(typename Hook::Signature), sequence,
                    std::make_shared<Function>(std::forward<Handler>(handler)));
    }

    /*!
        Adds the member function \a method of \a object to the hook \c Hook.

        \a object is not required to be a QObject, it can also be the private class of a manager.
        It must outlive the owner of this Hooks object.
    */
    template<HookDefinition Hook, typename Object, typename Method>
        requires std::is_member_function_pointer_v<Method>
    void add(Object *object, Method method, int sequence = HookSequence::Default) const
    {
        add<Hook>([object, method](auto &&...args) {
            return std::invoke(method, object, std::forward<decltype(args)>(args)...);
        },
                  sequence);
    }

    /*!
        Runs all handlers of the hook \c Hook with \a args.

        For hooks returning QXmpp::HookResult, the handlers are run until one returns
        HookResult::Stop, which is then returned. Otherwise all handlers are run.
    */
    template<HookDefinition Hook, typename... Args>
    auto run(Args &&...args) const
    {
        using Signature = Private::HookSignature<typename Hook::Signature>;
        using Function = typename Signature::Function;

        if constexpr (std::is_void_v<typename Signature::Result>) {
            runInternal(Hook::Name, typeid(typename Hook::Signature), [&](void *function) {
                (*static_cast<Function *>(function))(args...);
                return false;
            });
        } else {
            const auto stopped = runInternal(Hook::Name, typeid(typename Hook::Signature), [&](void *function) {
                return (*static_cast<Function *>(function))(args...) == HookResult::Stop;
            });
            return stopped ? HookResult::Stop : HookResult::Continue;
        }
    }

    /*! Returns whether the hook \c Hook has any handlers. */
    template<HookDefinition Hook>
    bool hasHandlers() const
    {
        return hasHandlersInternal(Hook::Name);
    }

    void removeAll() const;

private:
    void addInternal(QStringView name, const std::type_info &signature, int sequence, std::shared_ptr<void> function) const;
    bool runInternal(QStringView name, const std::type_info &signature, const std::function<bool(void *)> &invoke) const;
    bool hasHandlersInternal(QStringView name) const;

    Private::HookRegistry *m_registry;
    // handlers of the owner kept independently of a client, e.g. of an extension
    Private::HookRegistry *m_ownerRegistry = nullptr;
    QObject *m_owner;
};

}  // namespace QXmpp

#endif  // QXMPPHOOKS_H

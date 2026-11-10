// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHOOKS_H
#define QXMPPHOOKS_H

#include "QXmppGlobal.h"
#include "QXmppPromise.h"
#include "QXmppTask.h"

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <typeinfo>

#include <QString>
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

template<typename T>
struct HookTaskValue {
    static constexpr bool IsTask = false;
    using Type = T;
};

template<typename T>
struct HookTaskValue<QXmppTask<T>> {
    static constexpr bool IsTask = true;
    using Type = T;
};

template<typename T>
QXmppTask<T> makeReadyHookTask(auto &&...value)
{
    QXmppPromise<T> promise;
    promise.finish(std::forward<decltype(value)>(value)...);
    return promise.task();
}

template<typename Signature>
struct HookSignature;

template<typename R, typename... Args>
struct HookSignature<R(Args...)> {
    // QXmppTask<T> for asynchronous hooks
    using Result = R;
    // void or HookResult, or the result of the single responder
    using Value = typename HookTaskValue<R>::Type;
    using Function = std::function<R(Args...)>;
    using Arguments = std::tuple<Args...>;
    static constexpr bool IsAsync = HookTaskValue<R>::IsTask;
    // only the first handler runs and its result is returned
    static constexpr bool IsSingleResponder = !std::is_void_v<Value> && !std::is_same_v<Value, HookResult>;

    // Handlers of asynchronous hooks may also return Value directly.
    template<typename Handler>
    static constexpr bool isHandler()
    {
        if constexpr (!IsAsync) {
            return std::is_constructible_v<Function, Handler>;
        } else if constexpr (!std::is_invocable_v<Handler &, Args...>) {
            return false;
        } else {
            using HandlerResult = std::invoke_result_t<Handler &, Args...>;
            return std::is_same_v<HandlerResult, R> || std::is_convertible_v<HandlerResult, Value>;
        }
    }

    template<typename Handler>
    static Function makeFunction(Handler &&handler)
    {
        using HandlerResult = std::invoke_result_t<std::decay_t<Handler> &, Args...>;
        if constexpr (IsAsync && !std::is_same_v<HandlerResult, R>) {
            return [handler = std::forward<Handler>(handler)](Args... args) mutable -> R {
                if constexpr (std::is_void_v<Value>) {
                    std::invoke(handler, std::forward<Args>(args)...);
                    return makeReadyHookTask<void>();
                } else {
                    return makeReadyHookTask<Value>(std::invoke(handler, std::forward<Args>(args)...));
                }
            };
        } else {
            return Function(std::forward<Handler>(handler));
        }
    }
};

template<typename Hook>
using HookResultOf = typename HookSignature<typename Hook::Signature>::Value;

// Maps keys of hooks to unique strings. Specializations provide
// static QString toString(const Key &key).
template<typename Key>
struct HookKeyTraits;

template<>
struct HookKeyTraits<QString> {
    static QString toString(const QString &key) { return key; }
};

template<>
struct HookKeyTraits<QStringView> {
    static QString toString(QStringView key) { return key.toString(); }
};

namespace Enums {
template<typename Enum>
struct Data;
}

// enums with a string mapping (Enums::Data)
template<typename Enum>
    requires std::is_enum_v<Enum> && requires { Enums::Data<Enum>::Values; }
struct HookKeyTraits<Enum> {
    static QString toString(Enum key)
    {
        for (const auto &[value, string] : Enums::Data<Enum>::Values) {
            if (value == key) {
                return string.toString();
            }
        }
        return QString::number(qlonglong(key));
    }
};

template<typename Key>
concept HookKey = requires(const Key &key) {
    { HookKeyTraits<Key>::toString(key) } -> std::convertible_to<QString>;
};

// The elements are prefixed with their length, so different tuples never map to the same string.
template<HookKey... Keys>
struct HookKeyTraits<std::tuple<Keys...>> {
    static QString toString(const std::tuple<Keys...> &key)
    {
        QString result;
        std::apply([&](const auto &...elements) {
            ((result += [](const QString &element) {
                 return QString::number(element.size()) + u':' + element;
             }(HookKeyTraits<std::decay_t<decltype(elements)>>::toString(elements))),
             ...);
        },
                   key);
        return result;
    }
};

template<HookKey Key>
QString hookKeyString(const Key &key)
{
    return HookKeyTraits<Key>::toString(key);
}

// Identifies the signature of a keyed hook, so the same name with another key type is a mismatch.
template<typename Key, typename Signature>
struct KeyedHookSignature { };

// Stores the result of the handler of an asynchronous single responder hook.
template<typename T>
QXmppTask<bool> hookStoreTask(QXmppTask<T> task, std::shared_ptr<std::optional<T>> result)
{
    *result = co_await task;
    co_return true;
}

template<typename T>
QXmppTask<std::optional<T>> hookSingleResultTask(QXmppTask<bool> task, std::shared_ptr<std::optional<T>> result)
{
    co_await task;
    co_return std::move(*result);
}

// Converts the result of a handler of an asynchronous hook, true means stop.
inline QXmppTask<bool> hookStopTask(QXmppTask<void> task)
{
    co_await task;
    co_return false;
}

inline QXmppTask<bool> hookStopTask(QXmppTask<HookResult> task)
{
    co_return (co_await task) == HookResult::Stop;
}

inline QXmppTask<void> hookResultTask(QXmppTask<bool> task, std::type_identity<void>)
{
    co_await task;
}

inline QXmppTask<HookResult> hookResultTask(QXmppTask<bool> task, std::type_identity<HookResult>)
{
    co_return (co_await task) ? HookResult::Stop : HookResult::Continue;
}

}  // namespace Private

// A hook definition, see the documentation of QXmpp::Hooks.
template<typename T>
concept HookDefinition = requires {
    { T::Name } -> std::convertible_to<QStringView>;
    typename Private::HookResultOf<T>;
};

// A hook whose handlers are added and run per key.
template<typename T>
concept KeyedHookDefinition = HookDefinition<T> && requires {
    typename T::Key;
    requires Private::HookKey<typename T::Key>;
};

template<typename T>
concept UnkeyedHookDefinition = HookDefinition<T> && !requires { typename T::Key; };

// A typed definition adding handlers to another hook (Target), see QXmpp::Hooks.
template<typename T>
concept AdaptedHookDefinition = requires {
    typename T::Target;
    requires HookDefinition<typename T::Target>;
} && (!KeyedHookDefinition<typename T::Target> || requires {
                                    { T::key() } -> std::convertible_to<typename T::Target::Key>;
                                });

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

        \c Hook can also be an adapted hook definition: \c {Hook::adapt()} then turns \a handler
        into a handler of \c {Hook::Target}, for keyed targets with the key \c {Hook::key()}.
    */
    template<UnkeyedHookDefinition Hook, typename Handler>
        requires(Private::HookSignature<typename Hook::Signature>::template isHandler<std::decay_t<Handler>>())
    void add(Handler &&handler, int sequence = HookSequence::Default) const
    {
        addFunction<Hook>(QString(), std::forward<Handler>(handler), sequence);
    }

    /*!
        Adds the member function \a method of \a object to the hook \c Hook.

        \a object is not required to be a QObject, it can also be the private class of a manager.
        It must outlive the owner of this Hooks object. \a sequence decides the position of the
        handler, see QXmpp::HookSequence. \c Hook can also be an adapted hook definition.
    */
    template<UnkeyedHookDefinition Hook, typename Object, typename Method>
        requires std::is_member_function_pointer_v<Method>
    void add(Object *object, Method method, int sequence = HookSequence::Default) const
    {
        add<Hook>(memberHandler(object, method), sequence);
    }

    /*!
        Adds \a handler for \a key to the keyed hook \c Hook. It is only run for this key.
        \a sequence decides the position of the handler, see QXmpp::HookSequence.
    */
    template<KeyedHookDefinition Hook, typename Handler>
        requires(Private::HookSignature<typename Hook::Signature>::template isHandler<std::decay_t<Handler>>())
    void add(const typename Hook::Key &key, Handler &&handler, int sequence = HookSequence::Default) const
    {
        addFunction<Hook>(Private::hookKeyString(key), std::forward<Handler>(handler), sequence);
    }

    /*!
        Adds the member function \a method of \a object for \a key to the keyed hook \c Hook.
        \a sequence decides the position of the handler, see QXmpp::HookSequence.
    */
    template<KeyedHookDefinition Hook, typename Object, typename Method>
        requires std::is_member_function_pointer_v<Method>
    void add(const typename Hook::Key &key, Object *object, Method method, int sequence = HookSequence::Default) const
    {
        add<Hook>(key, memberHandler(object, method), sequence);
    }

    // Adds the handler via the adapted hook definition (documented with the other overload).
    template<AdaptedHookDefinition Adapted, typename Handler>
        requires requires(Handler &&handler) { Adapted::adapt(std::forward<Handler>(handler)); }
    void add(Handler &&handler, int sequence = HookSequence::Default) const
    {
        using Target = typename Adapted::Target;
        if constexpr (KeyedHookDefinition<Target>) {
            add<Target>(Adapted::key(), Adapted::adapt(std::forward<Handler>(handler)), sequence);
        } else {
            add<Target>(Adapted::adapt(std::forward<Handler>(handler)), sequence);
        }
    }

    // Adds the member function via the adapted hook definition (documented with the other overload).
    template<AdaptedHookDefinition Adapted, typename Object, typename Method>
        requires std::is_member_function_pointer_v<Method>
    void add(Object *object, Method method, int sequence = HookSequence::Default) const
    {
        add<Adapted>(memberHandler(object, method), sequence);
    }

    /*!
        Runs the handlers of the hook \c Hook with \a args.

        For hooks returning QXmpp::HookResult, the handlers are run until one returns
        HookResult::Stop, which is then returned. For hooks returning \c void, all handlers are
        run. For hooks returning another type, only the first handler is run and its result is
        returned, \c std::nullopt if there is no handler.

        Asynchronous hooks return a QXmppTask that finishes after the last handler. The handlers
        are run one after another. Arguments passed by reference must stay valid until the task
        has finished, arguments passed by value are copied. A handler is skipped when its owner
        is destroyed or its task is dropped before it has finished; the result of a single
        responder is then \c std::nullopt. The task finishes immediately if no handler waits for
        anything.
    */
    template<UnkeyedHookDefinition Hook, typename... Args>
    auto run(Args &&...args) const
    {
        return runFunctions<Hook>(QString(), std::forward<Args>(args)...);
    }

    /*!
        Runs the handlers of the keyed hook \c Hook for \a key with \a args, see run().
    */
    template<KeyedHookDefinition Hook, typename... Args>
    auto run(const typename Hook::Key &key, Args &&...args) const
    {
        return runFunctions<Hook>(Private::hookKeyString(key), std::forward<Args>(args)...);
    }

    /*! Returns whether the hook \c Hook has any handlers, for keyed hooks for any key. */
    template<typename Hook>
        requires HookDefinition<Hook> || AdaptedHookDefinition<Hook>
    bool hasHandlers() const
    {
        if constexpr (AdaptedHookDefinition<Hook>) {
            if constexpr (KeyedHookDefinition<typename Hook::Target>) {
                return hasHandlers<typename Hook::Target>(Hook::key());
            } else {
                return hasHandlers<typename Hook::Target>();
            }
        } else {
            return hasHandlersInternal(Hook::Name, std::nullopt);
        }
    }

    /*! Returns whether the keyed hook \c Hook has handlers for \a key. */
    template<KeyedHookDefinition Hook>
    bool hasHandlers(const typename Hook::Key &key) const
    {
        return hasHandlersInternal(Hook::Name, Private::hookKeyString(key));
    }

    void removeAll() const;

private:
    template<typename Object, typename Method>
    static auto memberHandler(Object *object, Method method)
    {
        // the explicit return type keeps it SFINAE friendly, e.g. for checks of adapted hooks
        return [object, method](auto &&...args) -> decltype(std::invoke(method, object, std::forward<decltype(args)>(args)...)) {
            return std::invoke(method, object, std::forward<decltype(args)>(args)...);
        };
    }

    template<HookDefinition Hook>
    static const std::type_info &signatureType()
    {
        if constexpr (KeyedHookDefinition<Hook>) {
            return typeid(Private::KeyedHookSignature<typename Hook::Key, typename Hook::Signature>);
        } else {
            return typeid(typename Hook::Signature);
        }
    }

    template<HookDefinition Hook, typename Handler>
    void addFunction(const QString &key, Handler &&handler, int sequence) const
    {
        using Signature = Private::HookSignature<typename Hook::Signature>;
        using Function = typename Signature::Function;
        addInternal(Hook::Name, key, signatureType<Hook>(), sequence,
                    std::make_shared<Function>(Signature::makeFunction(std::forward<Handler>(handler))),
                    Signature::IsSingleResponder);
    }

    template<HookDefinition Hook, typename... Args>
    auto runFunctions(const QString &key, Args &&...args) const
    {
        using Signature = Private::HookSignature<typename Hook::Signature>;
        using Function = typename Signature::Function;
        using Value = typename Signature::Value;
        const auto &signature = signatureType<Hook>();

        if constexpr (Signature::IsAsync && Signature::IsSingleResponder) {
            auto result = std::make_shared<std::optional<Value>>();
            auto invoke = [arguments = typename Signature::Arguments(std::forward<Args>(args)...), result](void *function) mutable {
                return Private::hookStoreTask(std::apply(*static_cast<Function *>(function), arguments), result);
            };
            return Private::hookSingleResultTask(runAsyncInternal(Hook::Name, key, signature, std::move(invoke), true), std::move(result));
        } else if constexpr (Signature::IsAsync) {
            auto invoke = [arguments = typename Signature::Arguments(std::forward<Args>(args)...)](void *function) mutable {
                return Private::hookStopTask(std::apply(*static_cast<Function *>(function), arguments));
            };
            return Private::hookResultTask(runAsyncInternal(Hook::Name, key, signature, std::move(invoke), false),
                                           std::type_identity<Value>());
        } else if constexpr (Signature::IsSingleResponder) {
            std::optional<Value> result;
            runInternal(Hook::Name, key, signature, [&](void *function) {
                result = (*static_cast<Function *>(function))(args...);
                // only the first handler
                return true;
            });
            return result;
        } else if constexpr (std::is_void_v<typename Signature::Result>) {
            runInternal(Hook::Name, key, signature, [&](void *function) {
                (*static_cast<Function *>(function))(args...);
                return false;
            });
        } else {
            const auto stopped = runInternal(Hook::Name, key, signature, [&](void *function) {
                return (*static_cast<Function *>(function))(args...) == HookResult::Stop;
            });
            return stopped ? HookResult::Stop : HookResult::Continue;
        }
    }

    void addInternal(QStringView name, const QString &key, const std::type_info &signature, int sequence, std::shared_ptr<void> function, bool singleResponder) const;
    bool runInternal(QStringView name, const QString &key, const std::type_info &signature, const std::function<bool(void *)> &invoke) const;
    QXmppTask<bool> runAsyncInternal(QStringView name, const QString &key, const std::type_info &signature, std::function<QXmppTask<bool>(void *)> invoke, bool singleResponder) const;
    bool hasHandlersInternal(QStringView name, const std::optional<QString> &key) const;

    Private::HookRegistry *m_registry;
    // handlers of the owner kept independently of a client, e.g. of an extension
    Private::HookRegistry *m_ownerRegistry = nullptr;
    QObject *m_owner;
};

}  // namespace QXmpp

#endif  // QXMPPHOOKS_H

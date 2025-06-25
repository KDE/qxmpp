// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTASK_H
#define QXMPPTASK_H

#include "QXmppGlobal.h"

#include <coroutine>
#include <memory>
#include <optional>

#include <QFuture>
#include <QPointer>

#if QXMPP_DEPRECATED_SINCE(1, 11)
#include <functional>
#endif

namespace QXmpp::Private {

template<typename T>
struct TaskData {
    std::conditional_t<std::is_void_v<T>, std::monostate, std::optional<T>> result;
    std::coroutine_handle<> handle;
    QPointer<const QObject> context;
    bool finished = false;
    bool cancelled = false;
    bool hasContext = false;
    uint8_t promiseCount = 1;

    ~TaskData()
    {
        Q_ASSERT(promiseCount == 0);
    }
};

template<typename T>
struct ConstRefOrVoidHelper {
    using Type = const T &;
};
template<>
struct ConstRefOrVoidHelper<void> {
    using Type = void;
};

template<typename T>
using ConstRefOrVoid = ConstRefOrVoidHelper<T>::Type;

template<typename Continuation, typename T>
struct InvokeContinuationResultHelper {
    using Type = std::invoke_result_t<Continuation, T &&>;
};
template<typename Continuation>
struct InvokeContinuationResultHelper<Continuation, void> {
    using Type = std::invoke_result_t<Continuation>;
};

template<typename Continuation, typename T>
using InvokeContinuationResult = InvokeContinuationResultHelper<Continuation, T>::Type;

}  // namespace QXmpp::Private

template<typename T>
class QXmppTask;

///
/// \brief Create and update QXmppTask objects to communicate results of asynchronous operations.
///
/// Unlike QFuture, this is not thread-safe. This avoids the need to do mutex locking at every
/// access though.
///
/// \ingroup Core
///
/// \since QXmpp 1.5
///
template<typename T>
class QXmppPromise
{
    using Task = QXmppTask<T>;
    using SharedData = QXmpp::Private::TaskData<T>;
    using SharedDataPtr = std::shared_ptr<SharedData>;

    struct InlineData {
        Task *task = nullptr;
        QPointer<const QObject> context;
        std::coroutine_handle<> handle;
        bool cancelled = false;
        bool hasContext = false;
    };

public:
    QXmppPromise() : data(InlineData()) { }
    // [[deprecated]]
    QXmppPromise(const QXmppPromise<T> &p)
    {
        p.detachData();
        data = p.data;
        sharedData().promiseCount += 1;
    }
    QXmppPromise(QXmppPromise<T> &&p)
    {
        std::swap(data, p.data);
        if (!shared()) {
            if (auto *task = inlineData().task) {
                task->setPromise(this);
            }
        }
    }
    ~QXmppPromise()
    {
        if (shared()) {
            sharedData().promiseCount -= 1;

            // cancel coroutine if any
            if (sharedData().promiseCount == 0) {
                if (auto handle = sharedData().handle) {
                    sharedData().handle = nullptr;
                    handle.destroy();
                }
            }
        } else {
            if (auto *task = inlineData().task) {
                task->setPromise(nullptr);
            }
            // cancel coroutine if any
            if (inlineData().handle) {
                inlineData().handle.destroy();
            }
        }
    }

    // [[deprecated]]
    QXmppPromise<T> &operator=(const QXmppPromise<T> &p)
    {
        if (shared()) {
            sharedData().promiseCount -= 1;
        }
        p.detachData();
        data = p.data;
        if (shared()) {
            sharedData().promiseCount += 1;
        }
        return *this;
    }
    QXmppPromise<T> &operator=(QXmppPromise<T> &&p)
    {
        std::swap(data, p.data);
        if (!shared()) {
            if (auto *task = inlineData().task) {
                task->setPromise(this);
            }
        }
        return *this;
    }

    ///
    /// Obtain a handle to this promise that allows to obtain the value that will be produced
    /// asynchronously.
    ///
    QXmppTask<T> task()
    {
        if (!shared()) {
            if (inlineData().task == nullptr) {
                return Task { this };
            } else {
                detachData();
            }
        }
        return Task { std::get<SharedDataPtr>(data) };
    }

    ///
    /// Finishes task.
    ///
    /// Must be called only once.
    ///
    void finish()
        requires(std::is_void_v<T>)
    {
        if (shared()) {
            sharedData().finished = true;
        } else {
            if (auto *task = inlineData().task) {
                task->inlineData().finished = true;
            } else {
                // finish called without generating task
                detachData();
                sharedData().finished = true;
            }
        }
        invokeHandle();
    }

    ///
    /// Finishes task with result.
    ///
    /// Must be called only once.
    ///
    template<typename U>
    void finish(U &&value)
        requires(!std::is_void_v<T>)
    {
        if (shared()) {
            sharedData().finished = true;
            sharedData().result = std::forward<U>(value);
        } else {
            if (auto *task = inlineData().task) {
                inlineData().task->inlineData().finished = true;
                inlineData().task->inlineData().result = std::forward<U>(value);
            } else {
                // finish called without generating task
                detachData();
                sharedData().finished = true;
                sharedData().result = std::forward<U>(value);
            }
        }
        invokeHandle();
    }

    ///
    /// Returns whether the task has been cancelled.
    ///
    /// If a task is cancelled, no call to `finish()` is needed and no continuation is resumed.
    ///
    /// \since QXmpp 1.11
    ///
    bool cancelled() const
    {
        return shared() ? sharedData().cancelled : inlineData().cancelled;
    }

private:
    friend class QXmppTask<T>;

    bool shared() const { return std::holds_alternative<SharedDataPtr>(data); }
    InlineData &inlineData()
    {
        Q_ASSERT(!shared());
        return std::get<InlineData>(data);
    }
    const InlineData &inlineData() const
    {
        Q_ASSERT(!shared());
        return std::get<InlineData>(data);
    }
    SharedData &sharedData()
    {
        Q_ASSERT(shared());
        return *std::get<SharedDataPtr>(data);
    }
    const SharedData &sharedData() const
    {
        Q_ASSERT(shared());
        return *std::get<SharedDataPtr>(data);
    }

    bool contextAlive() const
    {
        if (shared()) {
            return sharedData().context != nullptr || !sharedData().hasContext;
        } else {
            return inlineData().context != nullptr || !inlineData().hasContext;
        }
    }

    void invokeHandle()
    {
        auto &handleRef = shared() ? sharedData().handle : inlineData().handle;
        if (auto handle = handleRef) {
            handleRef = nullptr;
            if (contextAlive()) {
                handle.resume();
            } else {
                handle.destroy();
            }
        }
    }

    // Moves data from QXmppTask object to shared_ptr that can be accessed by multiple tasks and
    // multiple promises.
    // Historically required because task and promise must be copyable.
    void detachData() const
    {
        if (shared()) {
            return;
        }

        if (inlineData().task != nullptr) {
            auto &taskData = inlineData().task->inlineData();

            auto sharedData = std::make_shared<SharedData>(
                std::move(taskData.result),
                inlineData().handle,
                inlineData().context,
                taskData.finished,
                inlineData().cancelled,
                inlineData().hasContext,
                1);
            inlineData().task->data = sharedData;
            data = std::move(sharedData);
        } else {
            data = std::make_shared<SharedData>();
        }
    }

    mutable std::variant<InlineData, SharedDataPtr> data;
};

///
/// Handle for an ongoing operation that finishes in the future.
///
/// Tasks are generated by QXmppPromise and can be handled using QXmppTask::then().
///
/// Unlike QFuture, this is *not* thread-safe!! This avoids the need to do mutex locking at every
/// access though.
///
/// \ingroup Core
///
/// \since QXmpp 1.5
///
template<typename T>
class QXmppTask
{
    using Task = QXmppTask<T>;
    using SharedData = QXmpp::Private::TaskData<T>;
    using SharedDataPtr = std::shared_ptr<SharedData>;

    struct InlineData {
        QXmppPromise<T> *promise = nullptr;
        std::conditional_t<std::is_void_v<T>, std::monostate, std::optional<T>> result;
        bool finished = false;
    };

public:
    QXmppTask(QXmppTask &&t) : data(InlineData {})
    {
        std::swap(data, t.data);
        if (!shared()) {
            if (auto *p = inlineData().promise) {
                p->inlineData().task = this;
            }
        }
    }
    QXmppTask(const QXmppTask &) = delete;
    ~QXmppTask()
    {
        if (!shared()) {
            if (auto *p = inlineData().promise) {
                p->inlineData().task = nullptr;
            }
        }
    }

    QXmppTask &operator=(QXmppTask &&t) noexcept
    {
        // unregister with old promise
        if (!shared()) {
            if (auto *p = inlineData().promise) {
                p->inlineData().task = nullptr;
                // to swap nullptr into `t` in next step
                inlineData().promise = nullptr;
            }
        }
        std::swap(data, t.data);
        // register with new promise
        if (!shared()) {
            if (auto *p = inlineData().promise) {
                p->inlineData().task = this;
            }
        }
        return *this;
    }
    QXmppTask &operator=(const QXmppTask &) = delete;

    bool await_ready() const noexcept { return isFinished(); }
    void await_suspend(std::coroutine_handle<> handle)
    {
        auto replace = [](auto &var, auto newValue) {
            if (var) {
                var.destroy();
            }
            var = newValue;
        };

        if (shared()) {
            if (sharedData().promiseCount > 0 && !sharedData().cancelled) {
                replace(sharedData().handle, handle);
            } else {
                handle.destroy();
            }
        } else {
            if (auto *p = inlineData().promise; p && !p->cancelled()) {
                replace(p->inlineData().handle, handle);
            } else {
                handle.destroy();
            }
        }
    }
    auto await_resume()
    {
        if constexpr (!std::is_void_v<T>) {
            return takeResult();
        }
    }

    ///
    /// Registers a function that will be called with the result as parameter when the asynchronous
    /// operation finishes.
    ///
    /// If the task is already finished when calling this (and still has a result), the function
    /// will be called immediately.
    ///
    /// `.then()` can only be called once.
    ///
    /// Example usage:
    /// ```
    /// QXmppTask<QString> generateSomething();
    ///
    /// void Manager::generate()
    /// {
    ///     generateSomething().then(this, [](QString &&result) {
    ///         // runs as soon as the result is finished
    ///         qDebug() << "Generating finished:" << result;
    ///     });
    ///
    ///     // The generation could still be in progress here and the lambda might not
    ///     // have been executed yet.
    /// }
    ///
    /// // Manager is derived from QObject.
    /// ```
    ///
    /// \param context QObject used for unregistering the handler function when the object is
    /// deleted. This way your lambda will never be executed after your object has been deleted.
    /// \param continuation A function accepting a result in the form of `T &&`.
    ///
    template<typename Continuation>
    auto then(const QObject *context, Continuation continuation)
        -> QXmppTask<QXmpp::Private::InvokeContinuationResult<Continuation, T>>
    {
        QXmppTask<T> task = std::move(*this);
        if constexpr (std::is_void_v<T>) {
            co_await task.withContext(context);
            continuation();
        } else {
            continuation(co_await task.withContext(context));
        }
    }

    ///
    /// Sets a context object for co_await.
    ///
    /// If this task is co_await'ed, the coroutine will only be resumed if the context object is
    /// still alive. This is very helpful for usage in member functions to assure that `this` still
    /// exists after an co_await statement.
    ///
    /// \returns reference to this task
    ///
    /// \since QXmpp 1.11
    ///
    QXmppTask<T> &withContext(const QObject *c)
    {
        if (shared()) {
            if (!sharedData().finished) {
                sharedData().context = c;
                sharedData().hasContext = true;
            }
        } else {
            if (auto *p = inlineData().promise) {
                p->inlineData().context = c;
                p->inlineData().hasContext = true;
            }
        }
        return *this;
    }

    ///
    /// Cancels the task
    ///
    /// If there is a waiting coroutine, it is cancelled immediately. Any continuation set in the
    /// future also won't be executed.
    ///
    /// \since QXmpp 1.11
    ///
    void cancel()
    {
        if (shared()) {
            sharedData().cancelled = true;
            if (auto handle = sharedData().handle) {
                sharedData().handle = nullptr;
                handle.destroy();
            }
        } else {
            if (auto *p = inlineData().promise) {
                p->inlineData().cancelled = true;
                if (auto handle = p->inlineData().handle) {
                    p->inlineData().handle = nullptr;
                    handle.destroy();
                }
            }
        }
    }

    ///
    /// Whether the asynchronous operation is already finished.
    ///
    /// This does not mean that the result is still stored, it might have been taken using
    /// takeResult() or handled using then().
    ///
    [[nodiscard]]
    bool isFinished() const
    {
        return shared() ? sharedData().finished : inlineData().finished;
    }

    ///
    /// Returns whether the task is finished and the value has not been taken yet.
    ///
    [[nodiscard]]
    bool hasResult() const
        requires(!std::is_void_v<T>)
    {
        return shared() ? sharedData().result.has_value() : inlineData().result.has_value();
    }

    ///
    /// The result of the operation.
    ///
    /// \warning This can only be used once the operation is finished.
    ///
    [[nodiscard]]
    QXmpp::Private::ConstRefOrVoid<T> result() const
        requires(!std::is_void_v<T>)
    {
        Q_ASSERT(isFinished());
        Q_ASSERT(hasResult());
        return shared() ? sharedData().result.value() : inlineData().result.value();
    }

    ///
    /// Moves the result of the operation out of the task.
    ///
    /// \warning This can only be used once and only after the operation has finished.
    ///
    [[nodiscard]]
    T takeResult()
        requires(!std::is_void_v<T>)
    {
        Q_ASSERT(isFinished());
        Q_ASSERT(hasResult());
        auto &result = shared() ? sharedData().result : inlineData().result;

        auto value = std::move(*result);
        result.reset();
        return value;
    }

    ///
    /// Converts the Task into a QFuture. Afterwards the QXmppTask object is invalid.
    ///
    [[nodiscard]]
    QFuture<T> toFuture(const QObject *context)
    {
        QFutureInterface<T> interface;

        if constexpr (std::is_same_v<T, void>) {
            then(context, [interface]() mutable {
                interface.reportFinished();
            });
        } else {
            then(context, [interface](T &&val) mutable {
                interface.reportResult(val);
                interface.reportFinished();
            });
        }

        return interface.future();
    }

private:
    friend class QXmppPromise<T>;

    explicit QXmppTask(QXmppPromise<T> *p) : data(InlineData {})
    {
        inlineData().promise = p;

        Q_ASSERT(p->inlineData().task == nullptr);
        p->inlineData().task = this;
    }
    explicit QXmppTask(SharedDataPtr data) : data(std::move(data)) { }

    bool shared() const { return std::holds_alternative<SharedDataPtr>(data); }
    InlineData &inlineData()
    {
        Q_ASSERT(!shared());
        return std::get<InlineData>(data);
    }
    const InlineData &inlineData() const
    {
        Q_ASSERT(!shared());
        return std::get<InlineData>(data);
    }
    SharedData &sharedData()
    {
        Q_ASSERT(shared());
        return *std::get<SharedDataPtr>(data);
    }
    const SharedData &sharedData() const
    {
        Q_ASSERT(shared());
        return *std::get<SharedDataPtr>(data);
    }

    void setPromise(QXmppPromise<T> *p)
    {
        inlineData().promise = p;
    }

    std::variant<InlineData, SharedDataPtr> data;
};

namespace std {

template<typename T, typename... Args>
struct coroutine_traits<QXmppTask<T>, Args...> {
    struct promise_type {
        QXmppPromise<T> p;

        QXmppTask<T> get_return_object() { return p.task(); }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception()
        {
            // exception handling currently not supported
            throw std::current_exception();
        }

        void return_value(T value) { p.finish(std::move(value)); }
    };
};

template<typename... Args>
struct coroutine_traits<QXmppTask<void>, Args...> {
    struct promise_type {
        QXmppPromise<void> p;

        QXmppTask<void> get_return_object() { return p.task(); }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception()
        {
            // exception handling currently not supported
            throw std::current_exception();
        }

        void return_void() { p.finish(); }
    };
};

}  // namespace std

namespace QXmpp {

namespace Private {

template<typename T>
struct IsTaskHelper {
    constexpr static bool Value = false;
};
template<typename T>
struct IsTaskHelper<QXmppTask<T>> {
    using Type = T;
    constexpr static bool Value = true;
};

}  // namespace Private

///
/// Returns whether `T` is a `QXmppTask<U>`
///
/// \since QXmpp 1.14
///
template<typename T>
concept IsTask = Private::IsTaskHelper<T>::Value;

///
/// Returns the type `T` of a `QXmppTask<T>`
///
/// \since QXmpp 1.14
///
template<IsTask T>
using TaskValueType = typename Private::IsTaskHelper<T>::Type;

}  // namespace QXmpp

#endif  // QXMPPTASK_H

// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2025 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef ASYNC_H
#define ASYNC_H

#include "QXmppAsync_p.h"
#include "QXmppError.h"
#include "QXmppGlobal.h"

#include "Algorithms.h"

#include <memory>
#include <variant>
#include <vector>

#include <QFutureWatcher>

namespace QXmpp::Private {

// For QObjects that may be deleted while one of their signals is emitted, e.g. timers.
struct DeleteLaterDeleter {
    void operator()(QObject *obj) const
    {
        if (obj) {
            obj->deleteLater();
        }
    }
};

template<typename Function>
auto later(QObject *context, Function function)
{
    QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}

// QtFuture::makeReadyFuture() is deprecated since Qt 6.6 in favor of
// makeReadyValueFuture() (available since Qt 6.6). Wrap it to stay
// warning-free while keeping Qt 6.4 compatibility.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
template<typename T>
auto makeReadyFuture(T &&value)
{
    return QtFuture::makeReadyValueFuture(std::forward<T>(value));
}
#else
using QtFuture::makeReadyFuture;
#endif

template<typename T, typename Handler>
void await(const QFuture<T> &future, QObject *context, Handler handler)
{
    auto *watcher = new QFutureWatcher<T>(context);
    QObject::connect(watcher, &QFutureWatcherBase::finished,
                     context, [watcher, handler = std::move(handler)]() mutable {
                         handler(watcher->result());
                         watcher->deleteLater();
                     });
    watcher->setFuture(future);
}

template<typename Handler>
void await(const QFuture<void> &future, QObject *context, Handler handler)
{
    auto *watcher = new QFutureWatcher<void>(context);
    QObject::connect(watcher, &QFutureWatcherBase::finished,
                     context, [watcher, handler = std::move(handler)]() mutable {
                         handler();
                         watcher->deleteLater();
                     });
    watcher->setFuture(future);
}

template<typename T, typename Err, typename Function>
auto mapSuccess(const std::variant<T, Err> &var, Function lambda)
{
    using MappedVariant = std::variant<std::invoke_result_t<Function, const T &>, Err>;
    return map<MappedVariant>(lambda, var);
}

template<typename T, typename Err, typename Function>
auto mapSuccess(std::variant<T, Err> &&var, Function lambda)
{
    using MappedVariant = std::variant<std::invoke_result_t<Function, T &&>, Err>;
    return map<MappedVariant>(lambda, std::move(var));
}

template<typename T, typename Err>
auto mapToSuccess(std::variant<T, Err> &&var)
{
    return mapSuccess(std::move(var), [](T) {
        return Success();
    });
}

// Awaits running tasks, e.g. requests sent at once. Returns their results in the order of the tasks,
// or nothing for tasks without a return value.
template<typename T>
auto joinTasks(QObject *context, std::vector<QXmppTask<T>> tasks) -> QXmppTask<std::conditional_t<std::is_void_v<T>, void, std::vector<T>>>
{
    if constexpr (std::is_void_v<T>) {
        for (auto &task : tasks) {
            co_await task.withContext(context);
        }
    } else {
        std::vector<T> results;
        results.reserve(tasks.size());
        for (auto &task : tasks) {
            results.push_back(co_await task.withContext(context));
        }
        co_return results;
    }
}

// Finishes the promises with the response, copied for all but the last one.
template<typename Promises, typename Response>
void finishAll(Promises &promises, Response &&response)
{
    for (auto it = promises.begin(); it != promises.end(); ++it) {
        it->finish(std::next(it) == promises.end() ? std::move(response) : response);
    }
}

// Requests shared by the callers requesting the same data while they are running.
template<typename Params, typename Response>
class AttachableRequests
{
public:
    // Starts the request unless one with the same parameters is running, and returns its response.
    template<typename RequestFunction>
    QXmppTask<Response> produce(Params key, RequestFunction requestFunction, QObject *context)
    {
        QXmppPromise<Response> promise;
        auto task = promise.task();
        if (auto itr = std::ranges::find(m_requests, key, &Request::params); itr != m_requests.end()) {
            itr->promises.push_back(std::move(promise));
            return task;
        }

        m_requests.push_back(Request { key, {} });
        m_requests.back().promises.push_back(std::move(promise));
        requestFunction(key).then(context, [this, key](auto &&response) {
            finish(key, std::move(response));
        });
        return task;
    }

private:
    struct Request {
        Params params;
        std::vector<QXmppPromise<Response>> promises;
    };

    void finish(const Params &key, Response &&response)
    {
        auto itr = std::ranges::find(m_requests, key, &Request::params);
        Q_ASSERT(itr != m_requests.end());
        if (itr == m_requests.end()) {
            return;
        }

        // removed first, so the callers can start the same request again
        auto promises = std::move(itr->promises);
        m_requests.erase(itr);
        finishAll(promises, std::move(response));
    }

    std::vector<Request> m_requests;
};

// Promises of the callers waiting for the same result.
template<typename T>
class MultiPromise
{
public:
    bool isEmpty() const { return m_promises.empty(); }

    QXmppTask<T> generateTask()
    {
        m_promises.push_back(QXmppPromise<T>());
        return m_promises.back().task();
    }

    // The promises are removed first, so the callers can wait again in their continuations.
    void finish(T &&response)
    {
        auto promises = std::exchange(m_promises, {});
        finishAll(promises, std::move(response));
    }

private:
    std::vector<QXmppPromise<T>> m_promises;
};

template<>
class MultiPromise<void>
{
public:
    bool isEmpty() const { return m_promises.empty(); }

    QXmppTask<void> generateTask()
    {
        m_promises.push_back(QXmppPromise<void>());
        return m_promises.back().task();
    }

    // The promises are removed first, so the callers can wait again in their continuations.
    void finish()
    {
        auto promises = std::exchange(m_promises, {});
        for (auto &promise : promises) {
            promise.finish();
        }
    }

private:
    std::vector<QXmppPromise<void>> m_promises;
};

}  // namespace QXmpp::Private

#endif  // ASYNC_H

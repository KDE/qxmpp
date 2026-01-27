// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2025 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef ASYNC_H
#define ASYNC_H

#include "QXmppAsync_p.h"
#include "QXmppGlobal.h"

#include <memory>
#include <variant>

#include <QFutureWatcher>

namespace QXmpp::Private {

template<typename Function>
auto later(QObject *context, Function function)
{
    QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
template<typename T>
QFuture<T> makeReadyFuture(T &&value) { return QtFuture::makeReadyValueFuture(std::move(value)); }
#elif QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
using QtFuture::makeReadyFuture;
#else
template<typename T>
QFuture<T> makeReadyFuture(T &&value)
{
    QFutureInterface<T> interface(QFutureInterfaceBase::Started);
    interface.reportResult(std::move(value));
    interface.reportFinished();
    return interface.future();
}

inline QFuture<void> makeReadyFuture()
{
    using State = QFutureInterfaceBase::State;
    return QFutureInterface<void>(State(State::Started | State::Finished)).future();
}
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
auto mapSuccess(std::variant<T, Err> var, Function lambda)
{
    using MapResult = std::decay_t<decltype(lambda({}))>;
    using MappedVariant = std::variant<MapResult, Err>;
    return std::visit(overloaded {
                          [lambda = std::move(lambda)](T val) -> MappedVariant {
                              return lambda(std::move(val));
                          },
                          [](Err err) -> MappedVariant {
                              return err;
                          } },
                      std::move(var));
}

template<typename T, typename Err>
auto mapToSuccess(std::variant<T, Err> var)
{
    return mapSuccess(std::move(var), [](T) {
        return Success();
    });
}

template<typename T, typename Err>
auto chainSuccess(QXmppTask<std::variant<T, Err>> &&source, QObject *context) -> QXmppTask<std::variant<QXmpp::Success, QXmppError>>
{
    return chain<std::variant<QXmpp::Success, QXmppError>>(std::move(source), context, mapToSuccess<T, Err>);
}

template<typename Input, typename Converter>
auto chainMapSuccess(QXmppTask<Input> &&source, QObject *context, Converter convert)
{
    return chain<std::variant<decltype(convert({})), QXmppError>>(std::move(source), context, [convert](Input &&input) {
        return mapSuccess(std::move(input), convert);
    });
}

// Creates a task for multiple tasks without a return value.
template<typename T>
QXmppTask<void> joinVoidTasks(QObject *context, QList<QXmppTask<T>> &&tasks)
{
    int taskCount = tasks.size();
    auto finishedTaskCount = std::make_shared<int>();

    QXmppPromise<void> promise;

    for (auto &task : tasks) {
        task.then(context, [=]() mutable {
            if (++(*finishedTaskCount) == taskCount) {
                promise.finish();
            }
        });
    }

    return promise.task();
}

template<typename Params, typename Response>
struct AttachableRequests {
    struct Request {
        Params params;
        std::vector<QXmppPromise<Response>> promises;
    };

    std::vector<Request> requests;

    /// Find existing request and attach if found.
    std::optional<QXmppTask<Response>> attach(const Params &key)
    {
        auto itr = std::ranges::find(requests, key, &Request::params);
        if (itr != requests.end()) {
            QXmppPromise<Response> p;
            auto task = p.task();
            itr->promises.push_back(std::move(p));
            return task;
        }

        return std::nullopt;
    }

    QXmppTask<Response> makeNew(Params key)
    {
        Q_ASSERT(!contains(requests, key, &Request::params));

        QXmppPromise<Response> p;
        auto task = p.task();
        requests.push_back(Request { key, { std::move(p) } });
        return task;
    }

    void finish(const Params &key, Response &&response)
    {
        auto itr = std::ranges::find(requests, key, &Request::params);
        Q_ASSERT(itr != requests.end());
        if (itr == requests.end()) {
            return;
        }

        auto promises = std::move(itr->promises);
        requests.erase(itr);

        for (auto it = promises.begin(); it != promises.end(); ++it) {
            // copy unless this is the last iteration (then do move)
            it->finish(std::next(it) == promises.end() ? std::move(response) : response);
        }
    }

    QXmppTask<Response> produce(Params key, std::function<QXmppTask<Response>(Params)> requestFunction, QObject *context)
    {
        if (auto task = attach(key)) {
            return std::move(*task);
        }
        auto task = makeNew(key);
        requestFunction(key).then(context, [this, key](auto &&response) {
            finish(key, std::move(response));
        });
        return task;
    }
};

}  // namespace QXmpp::Private

#endif  // ASYNC_H

// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
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

}  // namespace QXmpp::Private

#endif  // ASYNC_H

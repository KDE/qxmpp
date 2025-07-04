// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPASYNC_P_H
#define QXMPPASYNC_P_H

#include "QXmppPromise.h"
#include "QXmppVisitHelper_p.h"

class QDomElement;
class QXmppError;

namespace QXmpp::Private {

// first argument of function
template<typename F, typename Ret, typename A, typename... Rest>
A lambda_helper(Ret (F::*)(A, Rest...));

template<typename F, typename Ret, typename A, typename... Rest>
A lambda_helper(Ret (F::*)(A, Rest...) const);

template<typename F>
struct first_argument {
    using type = decltype(lambda_helper(&F::operator()));
};

template<typename F>
using first_argument_t = typename first_argument<F>::type;

// creates a task in finished state with value
template<typename T>
QXmppTask<T> makeReadyTask(T &&value)
{
    QXmppPromise<T> promise;
    promise.finish(std::move(value));
    return promise.task();
}

inline QXmppTask<void> makeReadyTask()
{
    QXmppPromise<void> promise;
    promise.finish();
    return promise.task();
}

// Attaches to existing promise
template<typename Result, typename Input, typename Converter>
auto chain(QXmppTask<Input> &&source, QObject *context, QXmppPromise<Result> &&p, Converter convert)
{
    if constexpr (std::is_void_v<Input>) {
        source.then(context, [p = std::move(p), convert = std::move(convert)]() mutable {
            if constexpr (std::is_void_v<Result>) {
                convert();
                p.finish();
            } else {
                p.finish(convert());
            }
        });
    } else {
        source.then(context, [p = std::move(p), convert = std::move(convert)](Input &&input) mutable {
            if constexpr (std::is_void_v<Result>) {
                convert(std::move(input));
                p.finish();
            } else {
                p.finish(convert(std::move(input)));
            }
        });
    }
}

// creates new task which converts the result of the first
template<typename Result, typename Input, typename Converter>
auto chain(QXmppTask<Input> &&source, QObject *context, Converter convert) -> QXmppTask<Result>
{
    QXmppPromise<Result> p;
    auto task = p.task();
    if constexpr (std::is_void_v<Input>) {
        source.then(context, [p = std::move(p), convert = std::move(convert)]() mutable {
            if constexpr (std::is_void_v<Result>) {
                convert();
                p.finish();
            } else {
                p.finish(convert());
            }
        });
    } else {
        source.then(context, [p = std::move(p), convert = std::move(convert)](Input &&input) mutable {
            if constexpr (std::is_void_v<Result>) {
                convert(std::move(input));
                p.finish();
            } else {
                p.finish(convert(std::move(input)));
            }
        });
    }
    return task;
}

// parse Iq type from QDomElement or pass error
template<typename IqType, typename Input, typename Converter>
auto parseIq(Input &&sendResult, Converter convert) -> decltype(convert({}))
{
    using Result = decltype(convert({}));
    return std::visit(overloaded {
                          [convert = std::move(convert)](const QDomElement &element) -> Result {
                              IqType iq;
                              iq.parse(element);
                              return convert(std::move(iq));
                          },
                          [](QXmppError &&error) -> Result {
                              return error;
                          },
                      },
                      std::move(sendResult));
}

template<typename IqType, typename Result, typename Input>
auto parseIq(Input &&sendResult) -> Result
{
    return parseIq<IqType>(std::move(sendResult), [](IqType &&iq) -> Result {
        // no conversion
        return iq;
    });
}

// chain sendIq() task and parse DOM element to IQ type of first parameter of convert function
template<typename Input, typename Converter>
auto chainIq(QXmppTask<Input> &&input, QObject *context, Converter convert) -> QXmppTask<decltype(convert({}))>
{
    using Result = decltype(convert({}));
    using IqType = std::decay_t<first_argument_t<Converter>>;
    return chain<Result>(std::move(input), context, [convert = std::move(convert)](Input &&input) -> Result {
        return parseIq<IqType>(std::move(input), convert);
    });
}

// chain sendIq() task and parse DOM element to first type of Result variant
template<typename Result, typename Input>
auto chainIq(QXmppTask<Input> &&input, QObject *context) -> QXmppTask<Result>
{
    // IQ type is first std::variant parameter
    using IqType = std::decay_t<decltype(std::get<0>(Result {}))>;
    return chain<Result>(std::move(input), context, [](Input &&sendResult) mutable {
        return parseIq<IqType, Result>(sendResult);
    });
}

}  // namespace QXmpp::Private

#endif  // QXMPPASYNC_P_H

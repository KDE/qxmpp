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
QXmppTask<T> makeReadyTask(T &&value) { co_return value; }
inline QXmppTask<void> makeReadyTask() { co_return; }

// parse Iq type from QDomElement or pass error
template<typename IqType, typename Input, typename Converter>
auto parseIq(const Input &sendResult, Converter convert) -> decltype(convert({}))
{
    using Result = decltype(convert({}));
    return std::visit(
        overloaded {
            [convert = std::move(convert)](const QDomElement &element) -> Result {
                IqType iq;
                iq.parse(element);
                return convert(std::move(iq));
            },
            [](const QXmppError &error) -> Result {
                return error;
            },
        },
        sendResult);
}

template<typename IqType, typename Result, typename Input>
auto parseIq(const Input &sendResult) -> Result
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
    using IqType = std::decay_t<first_argument_t<Converter>>;
    co_return parseIq<IqType>(co_await input.withContext(context), convert);
}

// chain sendIq() task and parse DOM element to first type of Result variant
template<typename Result, typename Input>
auto chainIq(QXmppTask<Input> &&input, QObject *context) -> QXmppTask<Result>
{
    // IQ type is first std::variant parameter
    using IqType = std::decay_t<decltype(std::get<0>(Result {}))>;
    co_return parseIq<IqType, Result>(co_await input.withContext(context));
}

}  // namespace QXmpp::Private

#endif  // QXMPPASYNC_P_H

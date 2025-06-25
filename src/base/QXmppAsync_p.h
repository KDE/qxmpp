// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPASYNC_P_H
#define QXMPPASYNC_P_H

#include "QXmppTask.h"
#include "QXmppVisitHelper_p.h"

class QDomElement;
class QXmppError;

namespace QXmpp::Private {

// creates a task in finished state with value
template<typename T>
QXmppTask<T> makeReadyTask(T &&value) { co_return value; }
inline QXmppTask<void> makeReadyTask() { co_return; }

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
template<typename IqType, typename Converter>
auto parseIq(std::variant<QDomElement, QXmppError> &&sendResult, Converter convert)
{
    using Result = std::invoke_result_t<Converter, IqType &&>;
    return std::visit(
        overloaded {
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

template<typename IqType>
auto parseIq(std::variant<QDomElement, QXmppError> &&sendResult) -> std::variant<IqType, QXmppError>
{
    return parseIq<IqType>(std::move(sendResult), [](IqType &&iq) {
        return std::variant<IqType, QXmppError> { iq };
    });
}

}  // namespace QXmpp::Private

#endif  // QXMPPASYNC_P_H

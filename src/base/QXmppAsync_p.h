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

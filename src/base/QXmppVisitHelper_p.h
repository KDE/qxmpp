// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPVISITHELPER_P_H
#define QXMPPVISITHELPER_P_H

#include <variant>

namespace QXmpp::Private {

// helper for std::visit
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// Variation of std::visit allowing to forward unhandled types
template<typename ReturnType, typename T, typename Visitor>
auto visitForward(T variant, Visitor visitor)
{
    return std::visit([&](auto &&value) -> ReturnType {
        using ValueType = std::decay_t<decltype(value)>;
        if constexpr (std::is_invocable_v<Visitor, ValueType>) {
            return visitor(std::move(value));
        } else {
            return value;
        }
    },
                      std::forward<T>(variant));
}

}  // namespace QXmpp::Private

#endif  // QXMPPVISITHELPER_P_H

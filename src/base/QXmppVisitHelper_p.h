// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPVISITHELPER_P_H
#define QXMPPVISITHELPER_P_H

namespace QXmpp::Private {

// helper for std::visit
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace QXmpp::Private

#endif  // QXMPPVISITHELPER_P_H

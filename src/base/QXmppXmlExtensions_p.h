// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPXMLEXTENSIONS_P_H
#define QXMPPXMLEXTENSIONS_P_H

#include <any>
#include <vector>

#include <QSharedData>

namespace QXmpp::Private {

struct ExtensionsPrivate : QSharedData {
    std::vector<std::any> items;
};

}  // namespace QXmpp::Private

#endif  // QXMPPXMLEXTENSIONS_P_H

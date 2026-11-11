// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGEHANDLER_H
#define QXMPPMESSAGEHANDLER_H

#include "QXmppExtension.h"
#include "QXmppMessage.h"

#include <variant>

#include <QFuture>

#if QXMPP_DEPRECATED_SINCE(1, 18)
class [[deprecated("Use the QXmpp::Messages::Received hook")]]
QXMPP_EXPORT QXmppMessageHandler : public QXmppExtension
{
public:
    virtual bool handleMessage(const QXmppMessage &) = 0;
};
#endif

#endif  // QXMPPMESSAGEHANDLER_H

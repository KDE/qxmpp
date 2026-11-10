// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPUBSUBEVENTHANDLER_H
#define QXMPPPUBSUBEVENTHANDLER_H

#include "QXmppExtension.h"

class QDomElement;
class QXmppPubSubManager;

#if QXMPP_DEPRECATED_SINCE(1, 18)
// Documented in QXmppPubSubManager.cpp.
class [[deprecated("Use the QXmpp::PubSub::EventReceived hook")]]
QXMPP_EXPORT QXmppPubSubEventHandler : public QXmppExtension
{
public:
    virtual bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) = 0;
};
#endif

#endif  // QXMPPPUBSUBEVENTHANDLER_H

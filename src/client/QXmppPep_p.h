// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <QXmppPubSubEvent.h>
#include <QXmppPubSubManager.h>

#include "StringLiterals.h"

namespace QXmpp::Private::Pep {

template<typename T>
using GetResult = std::variant<T, QXmppError>;
using PublishResult = std::variant<QString, QXmppError>;

template<typename ItemT>
inline QXmppTask<GetResult<ItemT>> request(QXmppPubSubManager *pubSub, const QString &jid, const QString &nodeName, QObject *parent)
{
    auto result = co_await pubSub->requestItems<ItemT>(jid, nodeName);

    if (hasValue(result)) {
        if (!getValue(result).items.isEmpty()) {
            co_return getValue(result).items.constFirst();
        }
        co_return QXmppError { u"User has no published items."_s, {} };
    } else {
        co_return getError(std::move(result));
    }
}

// NodeName is a template parameter, so the right qstring comparison overload is used
// (if we used 'const QString &' as type, a 'const char *' string would be converted)
template<typename ItemT, typename NodeName, typename Manager, typename ReceivedSignal>
inline bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &eventNode, NodeName nodeName, Manager *manager, ReceivedSignal itemReceived)
{
    if (eventNode == nodeName && QXmppPubSubEvent<ItemT>::isPubSubEvent(element)) {
        QXmppPubSubEvent<ItemT> event;
        event.parse(element);

        if (event.eventType() == QXmppPubSubEventBase::Items) {
            if (!event.items().isEmpty()) {
                (manager->*itemReceived)(pubSubService, event.items().constFirst());
            } else {
                (manager->*itemReceived)(pubSubService, {});
            }
            return true;
        } else if (event.eventType() == QXmppPubSubEventBase::Retract) {
            (manager->*itemReceived)(pubSubService, {});
            return true;
        }
    }
    return false;
}

}  // namespace QXmpp::Private::Pep

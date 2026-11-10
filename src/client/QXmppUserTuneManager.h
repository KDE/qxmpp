// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPUSERTUNEMANAGER_H
#define QXMPPUSERTUNEMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppError.h"

#include <variant>

class QXmppTuneItem;
template<typename T>
class QXmppTask;

class QXMPP_EXPORT QXmppUserTuneManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    using Item = QXmppTuneItem;
    using GetResult = std::variant<Item, QXmppError>;
    using PublishResult = std::variant<QString, QXmppError>;

    QXmppUserTuneManager();

    auto request(const QString &jid) -> QXmppTask<GetResult>;
    auto publish(const QXmppTuneItem &item) -> QXmppTask<PublishResult>;

    Q_SIGNAL void itemReceived(const QString &jid, const QXmppTuneItem &item);

private:
    QXmpp::HookResult handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName);
};

#endif  // QXMPPUSERTUNEMANAGER_H

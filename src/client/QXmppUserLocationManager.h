// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Cochise César <cochisecesar@zoho.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPUSERLOCATIONMANAGER_H
#define QXMPPUSERLOCATIONMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppError.h"
#include "QXmppPubSubEventHandler.h"

#include <variant>

template<typename T>
class QXmppTask;
class QXmppGeolocItem;

class QXMPP_EXPORT QXmppUserLocationManager : public QXmppClientExtension, public QXmppPubSubEventHandler
{
    Q_OBJECT

public:
    using Item = QXmppGeolocItem;
    using GetResult = std::variant<Item, QXmppError>;
    using PublishResult = std::variant<QString, QXmppError>;

    QXmppUserLocationManager();

    QStringList discoveryFeatures() const override;

    auto request(const QString &jid) -> QXmppTask<GetResult>;
    /*!
        Publishes the User Location \a item on the user's account.
    */
    auto publish(const Item &item) -> QXmppTask<PublishResult>;

    Q_SIGNAL void itemReceived(const QString &jid, const QXmppGeolocItem &);

protected:
    bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) override;
};

#endif  // QXMPPUSERLOCATIONMANAGER_H

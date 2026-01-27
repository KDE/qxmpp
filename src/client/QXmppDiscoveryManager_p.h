// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_P_H
#define QXMPPDISCOVERYMANAGER_P_H

#include "QXmppDiscoveryManager.h"
#include "QXmppPromise.h"

#include "Async.h"
#include "Iq.h"

#include <QCache>

using namespace QXmpp::Private;

class QXmppDiscoveryManagerPrivate
{
public:
    using StanzaError = QXmppStanza::Error;

    QXmppDiscoveryManager *q = nullptr;
    QString clientCapabilitiesNode;
    QList<QXmppDiscoIdentity> identities;
    QList<QXmppDataForm> dataForms;

    // cached data
    QCache<std::tuple<QString, QString>, QXmppDiscoInfo> infoCache;
    QCache<std::tuple<QString, QString>, QList<QXmppDiscoItem>> itemsCache;

    // outgoing requests
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QXmppDiscoInfo>> infoRequests;
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QList<QXmppDiscoItem>>> itemsRequests;

    explicit QXmppDiscoveryManagerPrivate(QXmppDiscoveryManager *q) : q(q) { }

    static QString defaultApplicationName();
    static QXmppDiscoIdentity defaultIdentity();

    std::variant<CompatIq<QXmppDiscoInfo>, StanzaError> handleIq(GetIq<QXmppDiscoInfo> &&iq);
    std::variant<CompatIq<QXmppDiscoItems>, StanzaError> handleIq(GetIq<QXmppDiscoItems> &&iq);
};

#endif  // QXMPPDISCOVERYMANAGER_P_H

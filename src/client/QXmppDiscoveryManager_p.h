// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_P_H
#define QXMPPDISCOVERYMANAGER_P_H

#include "QXmppDiscoveryManager.h"

class QXmppDiscoveryManagerPrivate
{
public:
    QString clientCapabilitiesNode;
    QList<QXmppDiscoIdentity> identities;
    QList<QXmppDataForm> dataForms;

    static QString defaultApplicationName();
    static QXmppDiscoIdentity defaultIdentity();
};

#endif  // QXMPPDISCOVERYMANAGER_P_H

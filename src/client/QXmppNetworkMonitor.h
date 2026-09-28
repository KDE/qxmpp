// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPNETWORKMONITOR_H
#define QXMPPNETWORKMONITOR_H

#include "QXmppClientExtension.h"

#include <memory>

#include <QNetworkInformation>

class QXmppNetworkMonitorPrivate;

class QXMPP_EXPORT QXmppNetworkMonitor : public QXmppClientExtension
{
    Q_OBJECT
public:
    QXmppNetworkMonitor();
    ~QXmppNetworkMonitor() override;

    bool isActive() const;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void setReachability(QNetworkInformation::Reachability reachability);
    void onTransportMediumChanged();
    void setBehindCaptivePortal(bool behindCaptivePortal);

    const std::unique_ptr<QXmppNetworkMonitorPrivate> d;

    friend class tst_QXmppNetworkMonitor;
};

#endif  // QXMPPNETWORKMONITOR_H

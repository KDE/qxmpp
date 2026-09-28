// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppNetworkMonitor.h"

#include "QXmppClient.h"
#include "QXmppClient_p.h"

#include "StringLiterals.h"

#include <utility>

#include <QCoreApplication>
#include <QThread>

using Reachability = QNetworkInformation::Reachability;
using Feature = QNetworkInformation::Feature;

class QXmppNetworkMonitorPrivate
{
public:
    bool active = false;
    Reachability reachability = Reachability::Unknown;
    bool behindCaptivePortal = false;
    // the monitor told the client that the network is unavailable
    bool networkUnavailableSet = false;
};

/*!
    \class QXmppNetworkMonitor
    \inmodule QXmpp

    \brief The QXmppNetworkMonitor class reconnects the client based on network changes reported
    by QNetworkInformation.

    Without it, the client only notices a lost connection when a ping times out and only
    reconnects when its reconnection timer fires. With it:

    \list
    \li When the network becomes unavailable, the connection is closed immediately (without
    ending the stream, so it can be resumed) and no reconnection attempts are made until it is
    available again (see QXmppClient::setNetworkAvailable()).
    \li When the network becomes available again, is fully online again or a captive portal has
    been passed, the client reconnects immediately if it is waiting to reconnect (see
    QXmppClient::reconnectNow()).
    \li When the internet connection is lost while other interfaces keep the network available
    (reachability drops to QNetworkInformation::Reachability::Local, e.g. Wi-Fi turned off with a
    VPN interface still up), the connection is closed immediately (without ending the stream)
    and the client reconnects right away, in case the server is still reachable.
    \li When only the connectivity check fails (reachability drops from \c Online to \c Site),
    the connection is checked with a ping, as it may still work.
    \li When the transport medium changes (e.g. from Wi-Fi to mobile data), the connection is
    checked with a ping, as it may have broken without the socket noticing (see
    QXmppClient::checkConnection()).
    \endlist

    Only QNetworkInformation::Reachability::Disconnected is treated as an unavailable network.
    Other states do not guarantee that the server can (or cannot) be reached, e.g. the server may
    be in the local network, so they never prevent reconnection attempts.

    The monitor is enabled by adding it to the client and disabled by removing it:
    \code
    QXmppClient client;
    client.addNewExtension<QXmppNetworkMonitor>();
    \endcode

    If no QNetworkInformation backend has been loaded yet, the monitor loads one that supports
    reachability when it is added. The backend is process-wide and must be loaded in the thread of
    the QCoreApplication. If the client lives in another thread, load the backend in the
    application's thread with QNetworkInformation::loadBackendByFeatures() before adding the
    monitor. isActive() tells whether a usable backend was found.

    Applications that already monitor the network in a different way (e.g. using NetworkManager
    directly) can call QXmppClient::setNetworkAvailable() and the related functions themselves
    instead. The two should not be combined.

    \ingroup Core

    \since QXmpp 1.17
*/

QXmppNetworkMonitor::QXmppNetworkMonitor()
    : d(std::make_unique<QXmppNetworkMonitorPrivate>())
{
}

QXmppNetworkMonitor::~QXmppNetworkMonitor() = default;

/*!
    Returns whether network changes are monitored, i.e. whether the monitor has been added to a
    client and a QNetworkInformation backend supporting reachability is available.
*/
bool QXmppNetworkMonitor::isActive() const
{
    return d->active;
}

void QXmppNetworkMonitor::onRegistered(QXmppClient *client)
{
    auto *networkInfo = QNetworkInformation::instance();
    if (!networkInfo) {
        auto *app = QCoreApplication::instance();
        if (!app || QThread::currentThread() != app->thread()) {
            warning(u"Network monitoring disabled: The QNetworkInformation backend must be loaded in the thread of the QCoreApplication."_s);
            return;
        }
        if (!QNetworkInformation::loadBackendByFeatures(Feature::Reachability)) {
            info(u"Network monitoring disabled: No QNetworkInformation backend supporting reachability available."_s);
            return;
        }
        networkInfo = QNetworkInformation::instance();
    }
    if (!networkInfo->supports(Feature::Reachability)) {
        info(u"Network monitoring disabled: QNetworkInformation backend '%1' does not support reachability."_s.arg(networkInfo->backendName()));
        return;
    }

    connect(networkInfo, &QNetworkInformation::reachabilityChanged, this, &QXmppNetworkMonitor::setReachability);
    if (networkInfo->supports(Feature::TransportMedium)) {
        connect(networkInfo, &QNetworkInformation::transportMediumChanged, this, &QXmppNetworkMonitor::onTransportMediumChanged);
    }
    if (networkInfo->supports(Feature::CaptivePortal)) {
        d->behindCaptivePortal = networkInfo->isBehindCaptivePortal();
        connect(networkInfo, &QNetworkInformation::isBehindCaptivePortalChanged, this, &QXmppNetworkMonitor::setBehindCaptivePortal);
    }

    d->active = true;
    d->reachability = networkInfo->reachability();
    if (d->reachability == Reachability::Disconnected) {
        d->networkUnavailableSet = true;
        client->setNetworkAvailable(false);
    }
}

void QXmppNetworkMonitor::onUnregistered(QXmppClient *client)
{
    if (auto *networkInfo = QNetworkInformation::instance()) {
        disconnect(networkInfo, nullptr, this, nullptr);
    }

    // do not leave the client waiting for a network that nobody reports anymore
    if (d->networkUnavailableSet) {
        client->setNetworkAvailable(true);
    }
    *d = {};
}

void QXmppNetworkMonitor::setReachability(Reachability reachability)
{
    const auto previous = std::exchange(d->reachability, reachability);

    if (reachability == Reachability::Disconnected) {
        if (!d->networkUnavailableSet) {
            d->networkUnavailableSet = true;
            client()->setNetworkAvailable(false);
        }
        return;
    }

    if (d->networkUnavailableSet) {
        d->networkUnavailableSet = false;
        // reconnects immediately
        client()->setNetworkAvailable(true);
    } else if (reachability == Reachability::Online && previous != Reachability::Online) {
        client()->reconnectNow();
    } else if (reachability == Reachability::Local && (previous == Reachability::Online || previous == Reachability::Site)) {
        // There is no route beyond the local network anymore, but other interfaces are still up
        // (e.g. a VPN), so the network is not reported as disconnected. The connection most likely
        // broke, but the server may still be reachable (in the local network), so reconnect right
        // away instead of waiting for a ping to time out.
        info(u"Lost internet connection, reconnecting"_s);
        client()->d->reconnectForResumption();
    } else if (reachability == Reachability::Site && previous == Reachability::Online) {
        // Only the connectivity check failed, which may also happen while the connection works
        // (e.g. the check's server is blocked), so only check the connection.
        client()->checkConnection();
    }
}

void QXmppNetworkMonitor::onTransportMediumChanged()
{
    if (!d->networkUnavailableSet) {
        client()->checkConnection();
    }
}

void QXmppNetworkMonitor::setBehindCaptivePortal(bool behindCaptivePortal)
{
    if (std::exchange(d->behindCaptivePortal, behindCaptivePortal) && !behindCaptivePortal) {
        client()->reconnectNow();
    }
}

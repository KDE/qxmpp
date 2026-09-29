// SPDX-FileCopyrightText: 2023 Tibor Csötönyi <work@taibsu.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppExternalServiceDiscoveryManager.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppExternalServiceDiscoveryIq.h"
#include "QXmppIqHandling.h"

#include "Async.h"
#include "StringLiterals.h"

using namespace QXmpp::Private;

/*!
    \brief The QXmppExternalServiceDiscoveryManager class makes it possible to
    discover information about external services from providers
    as defined by \xep{0215}{External Service Discovery}.

    To make use of this manager, you need to instantiate it and load it into
    the QXmppClient instance as follows:

    \code
    auto *manager = client->addNewExtension<QXmppExternalServiceDiscoveryManager>();
    \endcode

    Checking whether the server supports external service discovery using watchServerSupport()
    requires the QXmppDiscoveryManager to be registered with the client.

    \ingroup Managers

    \since QXmpp 1.6
*/
QXmppExternalServiceDiscoveryManager::QXmppExternalServiceDiscoveryManager()
{
}

QXmppExternalServiceDiscoveryManager::~QXmppExternalServiceDiscoveryManager() = default;

/*!
    Requests external services from the specified XMPP entity. \a jid is the target entity's
    JID. \a node is the target node (optional).

    \since QXmpp 1.6
*/
QXmppTask<QXmppExternalServiceDiscoveryManager::ServicesResult> QXmppExternalServiceDiscoveryManager::requestServices(const QString &jid, const QString &node)
{
    QXmppExternalServiceDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);

    co_return parseIq<QXmppExternalServiceDiscoveryIq>(co_await client()->sendIq(std::move(request)).withContext(this), [](QXmppExternalServiceDiscoveryIq &&iq) -> ServicesResult {
        return iq.externalServices();
    });
}

/*!
    Returns a watch on whether the own server supports \xep{0215}{External Service Discovery}.

    The information is requested as long as a copy of the watch exists and shared with all other
    watches on the server information. This requires the QXmppDiscoveryManager to be registered
    with the client. If the manager is not registered with a client, a watch that never loads is
    returned.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppExternalServiceDiscoveryManager::watchServerSupport() const
{
    return watchServerFeature(client(), QXmpp::Namespace::ExternalServiceDiscovery2);
}

QStringList QXmppExternalServiceDiscoveryManager::discoveryFeatures() const
{
    return { staticString(ns_external_service_discovery) };
}

// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppIqHandling.h"
#include "QXmppUtils.h"

#include "Async.h"
#include "StringLiterals.h"

#include <QCoreApplication>

using namespace QXmpp;
using namespace QXmpp::Private;

///
/// \typedef QXmppDiscoveryManager::InfoResult
///
/// Contains the discovery information result in the form of an QXmppDiscoveryIq
/// or (in case the request did not succeed) a QXmppStanza::Error.
///
/// \since QXmpp 1.5
///

///
/// \typedef QXmppDiscoveryManager::ItemsResult
///
/// Contains a list of service discovery items or (in case the request did not
/// succeed) a QXmppStanza::Error.
///
/// \since QXmpp 1.5
///

QXmppDiscoveryManager::QXmppDiscoveryManager()
    : d(new QXmppDiscoveryManagerPrivate)
{
    d->clientCapabilitiesNode = u"https://github.com/qxmpp-project/qxmpp"_s;
    d->identities = { d->defaultIdentity() };
}

QXmppDiscoveryManager::~QXmppDiscoveryManager() = default;

///
/// Requests information from the specified XMPP entity.
///
/// \param jid  The target entity's JID.
/// \param node The target node (optional).
///
/// \since QXmpp 1.5
///
QXmppTask<QXmppDiscoveryManager::InfoResult> QXmppDiscoveryManager::requestDiscoInfo(const QString &jid, const QString &node)
{
    QXmppDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setQueryType(QXmppDiscoveryIq::InfoQuery);
    request.setTo(jid);
    if (!node.isEmpty()) {
        request.setQueryNode(node);
    }

    return chainIq<InfoResult>(client()->sendIq(std::move(request)), this);
}

///
/// Requests items from the specified XMPP entity.
///
/// \param jid  The target entity's JID.
/// \param node The target node (optional).
///
/// \since QXmpp 1.5
///
QXmppTask<QXmppDiscoveryManager::ItemsResult> QXmppDiscoveryManager::requestDiscoItems(const QString &jid, const QString &node)
{
    QXmppDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setQueryType(QXmppDiscoveryIq::ItemsQuery);
    request.setTo(jid);
    if (!node.isEmpty()) {
        request.setQueryNode(node);
    }

    return chainIq(client()->sendIq(std::move(request)), this, [](QXmppDiscoveryIq &&iq) -> ItemsResult {
        return iq.items();
    });
}

///
/// Returns the client's full capabilities.
///
QXmppDiscoveryIq QXmppDiscoveryManager::capabilities()
{
    QXmppDiscoveryIq iq;
    iq.setType(QXmppIq::Result);
    iq.setQueryType(QXmppDiscoveryIq::InfoQuery);

    // features
    QStringList features;
    // add base features of the client
    features << QXmppClientPrivate::discoveryFeatures();

    // add features of all registered extensions
    const auto extensions = client()->extensions();
    for (auto *extension : extensions) {
        if (extension) {
            features << extension->discoveryFeatures();
        }
    }
    iq.setFeatures(features);

    // identities
    auto identities = d->identities;
    for (auto *extension : client()->extensions()) {
        if (extension) {
            identities << extension->discoveryIdentities();
        }
    }
    iq.setIdentities(identities);

    // extra data forms
    iq.setDataForms(d->dataForms);

    return iq;
}

///
/// Returns the base identities of this client.
///
/// The identities are added to the service discovery information other entities can request.
///
/// \note Additionally also all identities reported via QXmppClientExtension::discoveryIdentities() are added.
///
/// \note The default identity is type=client, category=pc/phone (OS dependent) and name="{application name} {application version}".
///
/// \since QXmpp 1.12
///
const QList<QXmppDiscoIdentity> &QXmppDiscoveryManager::identities() const
{
    return d->identities;
}

///
/// Sets the base identities of this client.
///
/// The identities are added to the service discovery information other entities can request.
///
/// \note Additionally also all identities reported via QXmppClientExtension::discoveryIdentities() are added.
///
/// \note The default identity is type=client, category=pc/phone (OS dependent) and name="{application name} {application version}".
///
/// \since QXmpp 1.12
///
void QXmppDiscoveryManager::setIdentities(const QList<QXmppDiscoIdentity> &identities)
{
    d->identities = identities;
}

///
/// Returns the data forms for this client as defined in \xep{0128, Service Discovery Extensions}.
///
/// The data forms are added to the service discovery information other entities can request.
///
/// \since QXmpp 1.12
///
const QList<QXmppDataForm> &QXmppDiscoveryManager::infoForms() const
{
    return d->dataForms;
}

///
/// Sets the data forms for this client as defined in \xep{0128, Service Discovery Extensions}.
///
/// The data forms are added to the service discovery information other entities can request.
///
/// \since QXmpp 1.12
///
void QXmppDiscoveryManager::setInfoForms(const QList<QXmppDataForm> &dataForms)
{
    d->dataForms = dataForms;
}

///
/// Returns the capabilities node of the local XMPP client.
///
/// By default this is "https://github.com/qxmpp-project/qxmpp".
///
QString QXmppDiscoveryManager::clientCapabilitiesNode() const
{
    return d->clientCapabilitiesNode;
}

///
/// Sets the capabilities node of the local XMPP client.
///
void QXmppDiscoveryManager::setClientCapabilitiesNode(const QString &node)
{
    d->clientCapabilitiesNode = node;
}

/// \cond
QStringList QXmppDiscoveryManager::discoveryFeatures() const
{
    return { ns_disco_info.toString() };
}

bool QXmppDiscoveryManager::handleStanza(const QDomElement &element)
{
    if (handleIqRequests<QXmppDiscoveryIq>(element, client(), this)) {
        return true;
    }

    if (isIqElement<QXmppDiscoveryIq>(element)) {
        QXmppDiscoveryIq receivedIq;
        receivedIq.parse(element);

        switch (receivedIq.type()) {
        case QXmppIq::Get:
            break;
        case QXmppIq::Result:
        case QXmppIq::Error:
            // handle all replies
            if (receivedIq.queryType() == QXmppDiscoveryIq::InfoQuery) {
                Q_EMIT infoReceived(receivedIq);
            } else if (receivedIq.queryType() == QXmppDiscoveryIq::ItemsQuery) {
                Q_EMIT itemsReceived(receivedIq);
            }
            return true;

        case QXmppIq::Set:
            // let other manager handle "set" IQs
            return false;
        }
    }
    return false;
}

std::variant<QXmppDiscoveryIq, QXmppStanza::Error> QXmppDiscoveryManager::handleIq(QXmppDiscoveryIq &&iq)
{
    using Error = QXmppStanza::Error;

    if (!iq.queryNode().isEmpty() && !iq.queryNode().startsWith(d->clientCapabilitiesNode)) {
        return Error(Error::Cancel, Error::ItemNotFound, u"Unknown node."_s);
    }

    switch (iq.queryType()) {
    case QXmppDiscoveryIq::InfoQuery: {
        // respond to info queries for the client itself
        QXmppDiscoveryIq features = capabilities();
        features.setQueryNode(iq.queryNode());
        return features;
    }
    case QXmppDiscoveryIq::ItemsQuery: {
        QXmppDiscoveryIq reply;
        reply.setQueryType(QXmppDiscoveryIq::ItemsQuery);
        return reply;
    }
    }
    Q_UNREACHABLE();
}
/// \endcond

QString QXmppDiscoveryManagerPrivate::defaultApplicationName()
{
    if (!qApp->applicationName().isEmpty()) {
        if (!qApp->applicationVersion().isEmpty()) {
            return qApp->applicationName() + u' ' + qApp->applicationVersion();
        } else {
            return qApp->applicationName();
        }
    } else {
        return u"QXmpp " + QXmppVersion();
    }
}

QXmppDiscoIdentity QXmppDiscoveryManagerPrivate::defaultIdentity()
{
    return QXmppDiscoIdentity {
        u"client"_s,
#if defined Q_OS_ANDROID || defined Q_OS_BLACKBERRY || defined Q_OS_IOS || defined Q_OS_WP
        u"phone"_s,
#else
        u"pc"_s,
#endif
        defaultApplicationName(),
    };
}

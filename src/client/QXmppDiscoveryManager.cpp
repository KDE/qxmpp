// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppDiscoveryManager.h"

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppIqHandling.h"
#include "QXmppUtils.h"

#include "Async.h"
#include "StringLiterals.h"

#include <QCoreApplication>
#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

class QXmppDiscoveryManagerPrivate
{
public:
    QString clientCapabilitiesNode;
    QList<QXmppDiscoIdentity> identities;
    QList<QXmppDataForm> dataForms;
};

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

static QString clientApplicationName()
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

static auto clientDefaultIdentity()
{
    return QXmppDiscoIdentity {
        u"client"_s,
#if defined Q_OS_ANDROID || defined Q_OS_BLACKBERRY || defined Q_OS_IOS || defined Q_OS_WP
        u"phone"_s,
#else
        u"pc"_s,
#endif
        clientApplicationName(),
    };
}

QXmppDiscoveryManager::QXmppDiscoveryManager()
    : d(new QXmppDiscoveryManagerPrivate)
{
    d->clientCapabilitiesNode = u"https://github.com/qxmpp-project/qxmpp"_s;
    d->identities = { clientDefaultIdentity() };
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
const QList<QXmppDiscoIdentity> &QXmppDiscoveryManager::ownIdentities() const
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
void QXmppDiscoveryManager::setOwnIdentities(const QList<QXmppDiscoIdentity> &identities)
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
const QList<QXmppDataForm> &QXmppDiscoveryManager::ownDataForms() const
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
void QXmppDiscoveryManager::setOwnDataForms(const QList<QXmppDataForm> &dataForms)
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

///
/// Sets the category of the local XMPP client.
///
/// You can find a list of valid categories at:
/// http://xmpp.org/registrar/disco-categories.html
///
/// \deprecated Use setOwnIdentities(), this function will remove other identities if set.
///
void QXmppDiscoveryManager::setClientCategory(const QString &category)
{
    if (d->identities.size() != 1) {
        d->identities = { clientDefaultIdentity() };
    }
    d->identities.first().setCategory(category);
}

///
/// Sets the type of the local XMPP client.
///
/// You can find a list of valid types at:
/// http://xmpp.org/registrar/disco-categories.html
///
/// \deprecated Use setOwnIdentities(), this function will remove other identities if set.
///
void QXmppDiscoveryManager::setClientType(const QString &type)
{
    if (d->identities.size() != 1) {
        d->identities = { clientDefaultIdentity() };
    }
    d->identities.first().setType(type);
}

///
/// Sets the name of the local XMPP client.
///
/// \deprecated Use setOwnIdentities(), this function will remove other identities if set.
///
void QXmppDiscoveryManager::setClientName(const QString &name)
{
    if (d->identities.size() != 1) {
        d->identities = { clientDefaultIdentity() };
    }
    d->identities.first().setName(name);
}

///
/// Returns the category of the local XMPP client.
///
/// By default this is "client".
///
/// \deprecated Use ownIdentities()
///
QString QXmppDiscoveryManager::clientCategory() const
{
    if (d->identities.isEmpty()) {
        return {};
    }
    return d->identities.constFirst().category();
}

///
/// Returns the type of the local XMPP client.
///
/// With Qt builds for Android, Blackberry, iOS or Windows Phone this is set to
/// "phone", otherwise it defaults to "pc".
///
/// \deprecated Use ownIdentities()
///
QString QXmppDiscoveryManager::clientType() const
{
    if (d->identities.isEmpty()) {
        return {};
    }
    return d->identities.constFirst().type();
}

///
/// Returns the name of the local XMPP client.
///
/// By default this is "QXmpp x.y.z".
///
/// \deprecated Use setOwnIdentities()
///
QString QXmppDiscoveryManager::clientApplicationName() const
{
    if (d->identities.isEmpty()) {
        return {};
    }
    return d->identities.constFirst().name();
}

///
/// Returns the client's extended information form, as defined by \xep{0128, Service Discovery Extensions}.
///
/// \deprecated Use ownDataForms()
///
QXmppDataForm QXmppDiscoveryManager::clientInfoForm() const
{
    return d->dataForms.isEmpty() ? QXmppDataForm() : d->dataForms.constFirst();
}

///
/// Sets the client's extended information form, as defined by \xep{0128, Service Discovery Extensions}.
///
/// \deprecated Use setOwnDataForms()
///
void QXmppDiscoveryManager::setClientInfoForm(const QXmppDataForm &form)
{
    d->dataForms = { form };
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

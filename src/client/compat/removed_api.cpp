// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2009 Ian Reinhart Geiser <geiseri@kde.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppMessage.h"
#include "QXmppPacket_p.h"
#include "QXmppRosterManager.h"
#include "QXmppRpcManager.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppVCardManager.h"
#include "QXmppVersionManager.h"

#include "StringLiterals.h"

#include <QEventLoop>
#include <QTimer>

using namespace QXmpp::Private;

// Client

///
/// \brief You need to implement this method to process incoming XMPP
/// stanzas.
///
/// You should return true if the stanza was handled and no further
/// processing should occur, or false to let other extensions process
/// the stanza.
///
/// End-to-end encrypted stanzas are not passed to this overload, for that
/// purpose use the new overload instead.
///
/// \deprecated This is deprecated since QXmpp 1.5. Please use
/// QXmppClientExtension::handleStanza(const QDomElement &stanza,
/// const std::optional<QXmppE2eeMetadata> &e2eeMetadata).
/// Currently both methods are called by the client, so only implement one!
///
bool QXmppClientExtension::handleStanza(const QDomElement &)
{
    return false;
}

///
/// Returns the reference to QXmppRosterManager object of the client.
///
/// \return Reference to the roster object of the connected client. Use this to
/// get the list of friends in the roster and their presence information.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppClient::findExtension<QXmppRosterManager>() instead.
///
QXmppRosterManager &QXmppClient::rosterManager()
{
    return *findExtension<QXmppRosterManager>();
}

///
/// Returns the reference to QXmppVCardManager, implementation of \xep{0054}.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppClient::findExtension<QXmppVCardManager>() instead.
///
QXmppVCardManager &QXmppClient::vCardManager()
{
    return *findExtension<QXmppVCardManager>();
}

///
/// Returns the reference to QXmppVersionManager, implementation of \xep{0092}.
///
/// \deprecated This method is deprecated since QXmpp 1.1. Use
/// \c QXmppClient::findExtension<QXmppVersionManager>() instead.
///
QXmppVersionManager &QXmppClient::versionManager()
{
    return *findExtension<QXmppVersionManager>();
}

///
/// After successfully connecting to the server use this function to send
/// stanzas to the server. This function can solely be used to send various kind
/// of stanzas to the server. QXmppStanza is a parent class of all the stanzas
/// QXmppMessage, QXmppPresence, QXmppIq, QXmppBind, QXmppRosterIq, QXmppSession
/// and QXmppVCard.
///
/// This function does not end-to-end encrypt the packets.
///
/// \return Returns true if the packet was sent, false otherwise.
///
/// Following code snippet illustrates how to send a message using this function:
/// \code
/// QXmppMessage message(from, to, message);
/// client.sendPacket(message);
/// \endcode
///
/// \param packet A valid XMPP stanza. It can be an iq, a message or a presence stanza.
///
/// \deprecated
///
bool QXmppClient::sendPacket(const QXmppNonza &packet)
{
    return d->stream->streamAckManager().sendPacketCompat(packet);
}

///
/// Utility function to send message to all the resources associated with the
/// specified bareJid. If there are no resources available, that is the contact
/// is offline or not present in the roster, it will still send a message to
/// the bareJid.
///
/// \note Usage of this method is discouraged because most modern clients use
/// carbon messages (\xep{0280, Message Carbons}) and MAM (\xep{0313, Message
/// Archive Management}) and so could possibly receive messages multiple times
/// or not receive them at all.
/// \c QXmppClient::sendPacket() should be used instead with a \c QXmppMessage.
///
/// \param bareJid bareJid of the receiving entity
/// \param message Message string to be sent.
///
/// \deprecated
///
void QXmppClient::sendMessage(const QString &bareJid, const QString &message)
{
    QXmppRosterManager *rosterManager = findExtension<QXmppRosterManager>();

    const QStringList resources = rosterManager
        ? rosterManager->getResources(bareJid)
        : QStringList();

    if (!resources.isEmpty()) {
        for (const auto &resource : resources) {
            send(QXmppMessage({}, bareJid + u"/"_s + resource, message));
        }
    } else {
        send(QXmppMessage({}, bareJid, message));
    }
}

// DiscoveryManager

///
/// Requests information from the specified XMPP entity.
///
/// \param jid  The target entity's JID.
/// \param node The target node (optional).
///
/// \deprecated
///
QString QXmppDiscoveryManager::requestInfo(const QString &jid, const QString &node)
{
    QXmppDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setQueryType(QXmppDiscoveryIq::InfoQuery);
    request.setTo(jid);
    if (!node.isEmpty()) {
        request.setQueryNode(node);
    }
    return client()->sendLegacyId(request);
}

///
/// Requests items from the specified XMPP entity.
///
/// \param jid  The target entity's JID.
/// \param node The target node (optional).
///
/// \deprecated
///
QString QXmppDiscoveryManager::requestItems(const QString &jid, const QString &node)
{
    QXmppDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setQueryType(QXmppDiscoveryIq::ItemsQuery);
    request.setTo(jid);
    if (!node.isEmpty()) {
        request.setQueryNode(node);
    }
    return client()->sendLegacyId(request);
}

///
/// Returns the client's full capabilities.
///
/// \deprecated
///
QXmppDiscoveryIq QXmppDiscoveryManager::capabilities()
{
    auto info = buildClientInfo();

    QXmppDiscoveryIq iq;
    iq.setType(QXmppIq::Result);
    iq.setQueryType(QXmppDiscoveryIq::InfoQuery);
    iq.setFeatures(info.features());
    iq.setIdentities(info.identities());
    iq.setDataForms(info.dataForms());
    return iq;
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
        d->identities = { d->defaultIdentity() };
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
        d->identities = { d->defaultIdentity() };
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
        d->identities = { d->defaultIdentity() };
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

// RemoteMethod

/// \cond
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

QXmppRemoteMethod::QXmppRemoteMethod(const QString &jid, const QString &method, const QVariantList &args, QXmppClient *client) : QObject(client), m_client(client)
{
    m_payload.setTo(jid);
    m_payload.setFrom(client->configuration().jid());
    m_payload.setMethod(method);
    m_payload.setArguments(args);
}

QXmppRemoteMethodResult QXmppRemoteMethod::call()
{
    // FIXME : spinning an event loop is a VERY bad idea, it can cause
    // us to lose incoming packets
    QEventLoop loop(this);
    connect(this, &QXmppRemoteMethod::callDone, &loop, &QEventLoop::quit);
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);  // Timeout in case the other end hangs...

    auto payload = m_payload;
    m_client->send(std::move(m_payload));

    loop.exec(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents);
    return m_result;
}

void QXmppRemoteMethod::gotError(const QXmppRpcErrorIq &iq)
{
    if (iq.id() == m_payload.id()) {
        m_result.hasError = true;
        m_result.errorMessage = iq.error().text();
        m_result.code = iq.error().type();
        Q_EMIT callDone();
    }
}

void QXmppRemoteMethod::gotResult(const QXmppRpcResponseIq &iq)
{
    if (iq.id() == m_payload.id()) {
        m_result.hasError = false;
        // FIXME: we don't handle multiple responses
        const auto values = iq.values();
        m_result.result = values.first();
        Q_EMIT callDone();
    }
}

// RpcManager

/// Constructs a QXmppRpcManager.
QXmppRpcManager::QXmppRpcManager()
{
}

/// Adds a local interface which can be queried using RPC.
void QXmppRpcManager::addInvokableInterface(QXmppInvokable *interface)
{
    m_interfaces[QString::fromUtf8(interface->metaObject()->className())] = interface;
}

/// Invokes a remote interface using RPC.
void QXmppRpcManager::invokeInterfaceMethod(const QXmppRpcInvokeIq &iq)
{
    QXmppStanza::Error error;

    const QStringList methodBits = iq.method().split(u'.');
    if (methodBits.size() != 2) {
        return;
    }
    const QString interface = methodBits.first();
    const QString method = methodBits.last();
    QXmppInvokable *iface = m_interfaces.value(interface);
    if (iface) {
        if (iface->isAuthorized(iq.from())) {

            if (iface->interfaces().contains(method)) {
                QVariant result = iface->dispatch(method.toLatin1(),
                                                  iq.arguments());
                QXmppRpcResponseIq resultIq;
                resultIq.setId(iq.id());
                resultIq.setTo(iq.from());
                resultIq.setValues(QVariantList() << result);
                client()->send(std::move(resultIq));
                return;
            } else {
                error.setType(QXmppStanza::Error::Cancel);
                error.setCondition(QXmppStanza::Error::ItemNotFound);
            }
        } else {
            error.setType(QXmppStanza::Error::Auth);
            error.setCondition(QXmppStanza::Error::Forbidden);
        }
    } else {
        error.setType(QXmppStanza::Error::Cancel);
        error.setCondition(QXmppStanza::Error::ItemNotFound);
    }
    QXmppRpcErrorIq errorIq;
    errorIq.setId(iq.id());
    errorIq.setTo(iq.from());
    errorIq.setQuery(iq);
    errorIq.setError(error);
    client()->send(std::move(errorIq));
}

///
/// Calls a remote method using RPC with the specified arguments.
///
/// \note This method blocks until the response is received, and it may
/// cause XMPP stanzas to be lost!
///
QXmppRemoteMethodResult QXmppRpcManager::callRemoteMethod(const QString &jid,
                                                          const QString &interface,
                                                          const QVariant &arg1,
                                                          const QVariant &arg2,
                                                          const QVariant &arg3,
                                                          const QVariant &arg4,
                                                          const QVariant &arg5,
                                                          const QVariant &arg6,
                                                          const QVariant &arg7,
                                                          const QVariant &arg8,
                                                          const QVariant &arg9,
                                                          const QVariant &arg10)
{
    QVariantList args;
    if (arg1.isValid()) {
        args << arg1;
    }
    if (arg2.isValid()) {
        args << arg2;
    }
    if (arg3.isValid()) {
        args << arg3;
    }
    if (arg4.isValid()) {
        args << arg4;
    }
    if (arg5.isValid()) {
        args << arg5;
    }
    if (arg6.isValid()) {
        args << arg6;
    }
    if (arg7.isValid()) {
        args << arg7;
    }
    if (arg8.isValid()) {
        args << arg8;
    }
    if (arg9.isValid()) {
        args << arg9;
    }
    if (arg10.isValid()) {
        args << arg10;
    }

    bool check;
    Q_UNUSED(check)

    QXmppRemoteMethod method(jid, interface, args, client());
    check = connect(this, SIGNAL(rpcCallResponse(QXmppRpcResponseIq)),
                    &method, SLOT(gotResult(QXmppRpcResponseIq)));
    Q_ASSERT(check);
    check = connect(this, SIGNAL(rpcCallError(QXmppRpcErrorIq)),
                    &method, SLOT(gotError(QXmppRpcErrorIq)));
    Q_ASSERT(check);

    return method.call();
}

QStringList QXmppRpcManager::discoveryFeatures() const
{
    return {
        // XEP-0009: Jabber-RPC
        ns_rpc.toString(),
    };
}

QList<QXmppDiscoveryIq::Identity> QXmppRpcManager::discoveryIdentities() const
{
    QXmppDiscoveryIq::Identity identity;
    identity.setCategory(u"automation"_s);
    identity.setType(u"rpc"_s);
    return QList<QXmppDiscoveryIq::Identity>() << identity;
}

bool QXmppRpcManager::handleStanza(const QDomElement &element)
{
    // XEP-0009: Jabber-RPC
    if (QXmppRpcInvokeIq::isRpcInvokeIq(element)) {
        QXmppRpcInvokeIq rpcIqPacket;
        rpcIqPacket.parse(element);
        invokeInterfaceMethod(rpcIqPacket);
        return true;
    } else if (QXmppRpcResponseIq::isRpcResponseIq(element)) {
        QXmppRpcResponseIq rpcResponseIq;
        rpcResponseIq.parse(element);
        Q_EMIT rpcCallResponse(rpcResponseIq);
        return true;
    } else if (QXmppRpcErrorIq::isRpcErrorIq(element)) {
        QXmppRpcErrorIq rpcErrorIq;
        rpcErrorIq.parse(element);
        Q_EMIT rpcCallError(rpcErrorIq);
        return true;
    }
    return false;
}

QT_WARNING_POP
/// \endcond

#include "moc_QXmppRemoteMethod.cpp"
#include "moc_QXmppRpcManager.cpp"

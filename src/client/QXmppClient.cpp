// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"

#include "QXmppAsync_p.h"
#include "QXmppClientExtension.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppE2eeProvider.h"
#include "QXmppEntityTimeManager.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppMessages.h"
#include "QXmppPacket_p.h"
#include "QXmppPromise.h"
#include "QXmppRosterManager.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"
#include "QXmppVCardManager.h"
#include "QXmppVersionManager.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"
#include "XmppSocket.h"

#include <chrono>

#include <QDomElement>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTimer>

using namespace QXmpp;
using namespace std::chrono_literals;
using namespace QXmpp::Private;
using MessageEncryptResult = QXmppE2eeExtension::MessageEncryptResult;
using IqEncryptResult = QXmppE2eeExtension::IqEncryptResult;
using IqDecryptResult = QXmppE2eeExtension::IqDecryptResult;

static bool isIqResponse(const QDomElement &el)
{
    auto type = el.attribute(u"type"_s);
    return el.tagName() == u"iq" && (type == u"result" || type == u"error");
}

QXmppClientPrivate::QXmppClientPrivate(QXmppClient *qq)
    : clientPresence(QXmppPresence::Available),
      logger(nullptr),
      stream(nullptr),
      incomingMessages(qq, Hooks(&hookRegistry, nullptr), &extensions),
      outgoingMessages(qq, this, Hooks(&hookRegistry, nullptr)),
      encryptedIqs(qq, this, Hooks(&hookRegistry, nullptr)),
      encryptionExtension(nullptr),
      receivedConflict(false),
      reconnectionTries(0),
      reconnectionTimer(nullptr),
      networkAvailable(true),
      reconnectionDeferred(false),
      q(qq)
{
}

void QXmppClientPrivate::resendPresence()
{
    auto presence = clientPresence;
    q->send(std::move(presence));
}

void QXmppClientPrivate::addProperCapability(QXmppPresence &presence)
{
    if (auto *discoManager = q->findExtension<QXmppDiscoveryManager>()) {
        presence.setCapabilityHash(u"sha-1"_s);
        presence.setCapabilityNode(discoManager->clientCapabilitiesNode());
        presence.setCapabilityVer(discoManager->buildClientInfo().calculateEntityCapabilitiesHash());
    }
}

namespace QXmpp::Private {

std::chrono::milliseconds reconnectionDelay(int tries, bool resumable)
{
    // While the server keeps the session, reconnecting is cheap and missing the resumption window
    // is expensive (full resync of roster, presence, PEP, etc.), so retry more often.
    if (resumable) {
        switch (tries) {
        case 0:
            return 2s;
        case 1:
            return 5s;
        case 2:
            return 10s;
        case 3:
            return 15s;
        default:
            return 30s;
        }
    }

    switch (tries) {
    case 0:
    case 1:
        return 10s;
    case 2:
        return 15s;
    case 3:
        return 30s;
    default:
        return 60s;
    }
}

// Whether a stream error is caused by a (probably) temporary condition on the server, so that
// reconnecting later is likely to succeed. Other errors indicate a problem with the client or the
// account that reconnecting would not fix.
static bool isTemporaryStreamError(StreamError error)
{
    switch (error) {
    case StreamError::ConnectionTimeout:
    case StreamError::InternalServerError:
    case StreamError::RemoteConnectionFailed:
    case StreamError::Reset:
    case StreamError::ResourceConstraint:
    case StreamError::SystemShutdown:
        return true;
    default:
        return false;
    }
}

}  // namespace QXmpp::Private

std::chrono::milliseconds QXmppClientPrivate::getNextReconnectTime() const
{
    // ±20 % jitter, so that not all clients of a server reconnect at the same time after an outage
    const auto resumable = stream->c2sStreamManager().resumptionTimeRemaining() > 0ms;
    auto delay = reconnectionDelay(reconnectionTries, resumable);
    auto jitter = QRandomGenerator::global()->bounded(0.4) - 0.2;
    return std::chrono::duration_cast<std::chrono::milliseconds>(delay * (1.0 + jitter));
}

void QXmppClientPrivate::scheduleReconnect(std::chrono::milliseconds delay)
{
    // without network, wait until it is available again (see QXmppClient::setNetworkAvailable())
    if (!networkAvailable && !isServerLoopback()) {
        reconnectionDeferred = true;
        return;
    }
    reconnectionTimer->start(delay);
}

void QXmppClientPrivate::scheduleBackoffReconnect()
{
    // only start the backoff from the beginning if the last connection was stable,
    // so that a flapping server does not defeat it
    if (connectedSince.isValid() && std::chrono::milliseconds(connectedSince.elapsed()) >= StableConnectionDuration) {
        reconnectionTries = 0;
    }
    connectedSince.invalidate();

    scheduleReconnect(getNextReconnectTime());
    reconnectionTries++;
}

// Closes the connection without ending the stream and reconnects immediately, for network changes
// that most likely broke the connection.
void QXmppClientPrivate::reconnectForResumption()
{
    if (isServerLoopback()) {
        return;
    }

    // without automatic reconnection, the connection would not come back, but it may still work
    if (!q->configuration().autoReconnectionEnabled() || receivedConflict) {
        q->checkConnection();
        return;
    }

    if (q->state() != QXmppClient::DisconnectedState) {
        stream->disconnectForResumption();
        reconnectionDeferred = true;
    }
    q->reconnectNow();
}

static bool isLoopbackHost(const QString &host)
{
    return host.compare(u"localhost", Qt::CaseInsensitive) == 0 ||
        host.endsWith(u".localhost", Qt::CaseInsensitive) ||
        QHostAddress(host).isLoopback();
}

static bool usesProxy(const QXmppConfiguration &config, const QString &host)
{
    auto proxy = config.networkProxy();
    if (proxy.type() == QNetworkProxy::DefaultProxy) {
        // application-wide or system proxy
        proxy = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(host, config.port())).value(0);
    }
    return proxy.type() != QNetworkProxy::NoProxy && proxy.type() != QNetworkProxy::DefaultProxy;
}

// Whether the server is on the local machine, so that it can be reached without network.
bool QXmppClientPrivate::isServerLoopback() const
{
    if (!loopbackIgnoresNetwork) {
        return false;
    }

    // with a proxy, the connection may go to a local proxy, but the server is elsewhere
    const auto &config = stream->configuration();
    const auto host = config.host().isEmpty() ? config.domain() : config.host();
    if (usesProxy(config, host)) {
        return false;
    }

    const auto *socket = stream->xmppSocket().internalSocket();
    if (socket->state() == QAbstractSocket::ConnectedState) {
        return socket->peerAddress().isLoopback();
    }
    return lastServerWasLoopback || isLoopbackHost(host);
}

void QXmppClientPrivate::cancelReconnect()
{
    reconnectionTimer->stop();
    reconnectionDeferred = false;
}

QStringList QXmppClientPrivate::discoveryFeatures()
{
    return {
        // XEP-0004: Data Forms
        staticString(ns_data),
        // XEP-0059: Result Set Management
        staticString(ns_rsm),
        // XEP-0066: Out of Band Data
        staticString(ns_oob),
        // XEP-0071: XHTML-IM
        staticString(ns_xhtml_im),
        // XEP-0085: Chat State Notifications
        staticString(ns_chat_states),
        // XEP-0115: Entity Capabilities
        staticString(ns_capabilities),
        // XEP-0249: Direct MUC Invitations
        staticString(ns_conference),
        // XEP-0308: Last Message Correction
        staticString(ns_message_correct),
        // XEP-0333: Chat Markers
        staticString(ns_chat_markers),
        // XEP-0334: Message Processing Hints
        staticString(ns_message_processing_hints),
        // XEP-0359: Unique and Stable Stanza IDs
        staticString(ns_sid),
        // XEP-0367: Message Attaching
        staticString(ns_message_attaching),
        // XEP-0380: Explicit Message Encryption
        staticString(ns_eme),
        // XEP-0382: Spoiler messages
        staticString(ns_spoiler),
        // XEP-0428: Fallback Indication
        staticString(ns_fallback_indication),
        // XEP-0444: Message Reactions
        staticString(ns_reactions),
    };
}

void QXmppClientPrivate::onErrorOccurred(const QString &text, const QXmppOutgoingClient::ConnectionError &err, QXmppClient::Error oldError)
{
    // Skip stream errors that are valid during special procedures such as account
    // creation/deletion.
    if (const auto streamError = std::get_if<QXmpp::StreamError>(&err);
        streamError && ignoredStreamErrors.contains(*streamError)) {
        return;
    }

    if (q->configuration().autoReconnectionEnabled()) {
        if (oldError == QXmppClient::XmppStreamError) {
            // if we receive a resource conflict, inhibit reconnection
            if (stream->xmppStreamError() == QXmppStanza::Error::Conflict) {
                receivedConflict = true;
            } else if (auto *streamError = std::get_if<QXmpp::StreamError>(&err);
                       streamError && isTemporaryStreamError(*streamError)) {
                scheduleBackoffReconnect();
            }
        } else if (receivedConflict) {
            // another client took over the resource
        } else if (auto *socketError = std::get_if<QAbstractSocket::SocketError>(&err);
                   socketError && *socketError == QAbstractSocket::SslHandshakeFailedError) {
            // Retrying does not help until something changes (e.g. the certificate is trusted or
            // a captive portal intercepting TLS has been passed), so only reconnect on
            // reconnectNow() or network changes.
            reconnectionDeferred = true;
        } else if (oldError == QXmppClient::SocketError || oldError == QXmppClient::KeepAliveError) {
            scheduleBackoffReconnect();
        }
    }

    // notify managers
    Q_EMIT q->error(oldError);
    Q_EMIT q->errorOccurred(QXmppError { text, into<std::any>(std::move(err)) });
}

bool QXmppClientPrivate::handleStanza(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
{
    const auto hooks = q->hooks(nullptr);
    if (IqRequests::process(q, hooks, element, e2eeMetadata)) {
        return true;
    }
    if (hooks.run<Client::StanzaReceived>(element, e2eeMetadata) == HookResult::Stop) {
        return true;
    }

    // extensions still implementing the deprecated interface
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const bool unencrypted = !e2eeMetadata.has_value();
    for (auto *extension : extensions) {
        // e2e encrypted stanzas are not passed to the old handleStanza() overload, because such
        // managers are likely not handling the encrypted contents correctly (e.g. sending
        // unencrypted replies and thereby leaking information).
        if (extension->handleStanza(element, e2eeMetadata) ||
            (unencrypted && extension->handleStanza(element))) {
            return true;
        }
    }
    QT_WARNING_POP
    return false;
}

/*!
    \typedef QXmppClient::IqResult

    Result of an IQ request, either contains the QDomElement of the IQ reponse (in case of an
    'result' IQ type) or it contains an QXmppError with a QXmppStanza::Error (on type 'error') or
    a QXmpp::SendError.

    \since QXmpp 1.5
*/

/*!
    \typedef QXmppClient::EmptyResult

    Result of a generic request without a return value. Contains Success in case
    everything went well. If the returned IQ contained an error a
    QXmppStanza::Error is reported.

    \since QXmpp 1.5
*/

/*!
    Creates a QXmppClient object.

    \a initialExtensions can be used to set the initial set of extensions.
    \a parent is passed to the QObject's constructor. The default value is 0.

    \since QXmpp 1.6
*/
QXmppClient::QXmppClient(InitialExtensions initialExtensions, QObject *parent)
    : QXmppLoggable(parent),
      d(new QXmppClientPrivate(this))
{
    d->stream = new QXmppOutgoingClient(this);
    d->addProperCapability(d->clientPresence);

    connect(d->stream, &QXmppOutgoingClient::elementReceived,
            this, &QXmppClient::_q_elementReceived);

    connect(d->stream, &QXmppOutgoingClient::messageReceived,
            this, &QXmppClient::messageReceived);

    connect(d->stream, &QXmppOutgoingClient::presenceReceived,
            this, &QXmppClient::presenceReceived);

    connect(d->stream, &QXmppOutgoingClient::iqReceived,
            this, &QXmppClient::iqReceived);

    connect(d->stream, &QXmppOutgoingClient::sslErrors,
            this, &QXmppClient::sslErrors);

    connect(d->stream, &QXmppOutgoingClient::connected,
            this, &QXmppClient::_q_streamConnected);

    connect(d->stream, &QXmppOutgoingClient::disconnected,
            this, &QXmppClient::_q_streamDisconnected);
    connect(d->stream, &QXmppOutgoingClient::disconnected, this, [this](const SessionEnd &end) {
        // e.g. the server shut down without sending a stream error
        if (end.closedByServer && configuration().autoReconnectionEnabled() && !d->receivedConflict) {
            d->scheduleBackoffReconnect();
        }
    });

    connect(d->stream, &QXmppOutgoingClient::errorOccurred, this, [this](const auto &text, const auto &error, auto oldError) {
        d->onErrorOccurred(text, error, oldError);
    });

    connect(d->stream, &QXmppOutgoingClient::stateChanged, this, &QXmppClient::stateChanged);

    // reconnection
    d->reconnectionTimer = new QTimer(this);
    d->reconnectionTimer->setSingleShot(true);
    // the delays are jittered anyway, second precision lets the system coalesce wakeups
    d->reconnectionTimer->setTimerType(Qt::VeryCoarseTimer);
    connect(d->reconnectionTimer, &QTimer::timeout,
            this, &QXmppClient::_q_reconnect);

    // logging
    setLogger(QXmppLogger::getLogger());

    switch (initialExtensions) {
    case NoExtensions:
        break;
    case BasicExtensions:
        addNewExtension<QXmppRosterManager>(this);
        addNewExtension<QXmppVCardManager>();
        addNewExtension<QXmppVersionManager>();
        addNewExtension<QXmppEntityTimeManager>();
        addNewExtension<QXmppDiscoveryManager>();
        break;
    }
}

/*!
    Creates a QXmppClient object. \a parent is passed to the QObject's constructor.
*/
QXmppClient::QXmppClient(QObject *parent)
    : QXmppClient(BasicExtensions, parent)
{
}

QXmppClient::~QXmppClient() = default;

/*!
    \fn template <typename T, typename... Args> T *QXmppClient::addNewExtension(Args... args)

    Creates a new extension (forwarding \a args to its constructor) and adds it to the client.

    Returns the newly created extension.

    \since QXmpp 1.5
*/

/*! Registers a new \a extension with the client. Returns true on success. */
bool QXmppClient::addExtension(QXmppClientExtension *extension)
{
    return insertExtension(d->extensions.size(), extension);
}

/*! Registers a new \a extension with the client at the given \a index. Returns true on success. */
bool QXmppClient::insertExtension(int index, QXmppClientExtension *extension)
{
    if (d->extensions.contains(extension)) {
        qWarning("Cannot add extension, it has already been added");
        return false;
    }
    if (extension->client()) {
        qWarning("Cannot add extension, it has already been added to another client");
        return false;
    }

    extension->setParent(this);
    d->extensions.insert(index, extension);
    extension->setClient(this);
    return true;
}

/*!
    Returns whether the extension was unregistered. Unregisters the given extension from the client. If the extension
    is found, it will be destroyed.

    \a extension.
*/
bool QXmppClient::removeExtension(QXmppClientExtension *extension)
{
    if (d->extensions.contains(extension)) {
        d->extensions.removeAll(extension);
        extension->setClient(nullptr);
        // also removes handlers added via hooks(extension) of extensions overriding setClient()
        d->hookRegistry.removeAll(extension);
        delete extension;
        return true;
    } else {
        qWarning("Cannot remove extension, it was never added");
        return false;
    }
}

/*!
    Returns the currently used encryption extension.

    \deprecated since QXmpp 1.18, end-to-end encryptions are QXmpp::E2ee::Provider registered via
    QXmpp::E2ee::Providers.

    \since QXmpp 1.5
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
QXmppE2eeExtension *QXmppClient::encryptionExtension() const
{
    return d->encryptionExtension;
}
#endif

QXmppE2eeExtension *QXmpp::Private::LegacyE2ee::encryptionExtension(QXmppClient *client)
{
    return client->d->encryptionExtension;
}

void QXmpp::Private::LegacyE2ee::setEncryptionExtension(QXmppClient *client, QXmppE2eeExtension *extension)
{
    client->d->encryptionExtension = extension;
}

/*!
    Sets the extension to be used for end-to-end-encryption.

    \deprecated since QXmpp 1.18, end-to-end encryptions are QXmpp::E2ee::Provider registered via
    QXmpp::E2ee::Providers, e.g. QXmppOmemoManager. The encryption extension is
    still used for sensitive stanzas sent by QXmpp's managers if no encryption is selected by the
    QXmpp::Messages::SelectEncryption hook.

    \since QXmpp 1.5

    \a extension.
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
void QXmppClient::setEncryptionExtension(QXmppE2eeExtension *extension)
{
    d->encryptionExtension = extension;
}
#endif

/*! Returns a list containing all the client's extensions. */
QList<QXmppClientExtension *> QXmppClient::extensions() const
{
    return d->extensions;
}

/*!
    Returns the hooks of the client with \a owner as the owner of added handlers.

    Handlers added through the returned object are removed when \a owner is destroyed. Running
    hooks does not require an owner, \a owner can be \c nullptr then.

    Extensions should use QXmppClientExtension::hooks() instead.

    \since QXmpp 1.18
*/
Hooks QXmppClient::hooks(QObject *owner)
{
    return { &d->hookRegistry, owner };
}

/*! Returns a modifiable reference to the current configuration of QXmppClient. */
QXmppConfiguration &QXmppClient::configuration()
{
    return d->stream->configuration();
}

/*!
    Attempts to connect to the XMPP server. Server details and other configurations
    are specified using the config parameter. Use signals connected(), error(QXmppClient::Error)
    and disconnected() to know the status of the connection.

    \a config specifies the configuration object for connecting the XMPP server.
    This contains the host name, user, password etc. See QXmppConfiguration for details.
    \a initialPresence is the initial presence which will be set for this user
    after establishing the session. The default value is QXmppPresence::Available.
*/
void QXmppClient::connectToServer(const QXmppConfiguration &config,
                                  const QXmppPresence &initialPresence)
{
    // reset package cache from last connection
    if (d->stream->configuration().jidBare() != config.jidBare()) {
        d->stream->streamAckManager().resetCache();
    }

    d->stream->configuration() = config;
    d->clientPresence = initialPresence;
    d->addProperCapability(d->clientPresence);
    d->cancelReconnect();
    d->reconnectionTries = 0;
    d->lastServerWasLoopback = false;

    d->stream->connectToHost();
}

/*!
    Overloaded function to simply connect to an XMPP server with a \a jid and \a password for
    the account.
*/
void QXmppClient::connectToServer(const QString &jid, const QString &password)
{
    QXmppConfiguration config;
    config.setJid(jid);
    config.setPassword(password);
    connectToServer(config);
}

/*!
    Sends a packet and reports the result via QXmppTask.

    If stream management is enabled, the task continues to be active until the
    server acknowledges the packet. On success, QXmpp::SendSuccess with
    acknowledged == true is reported and the task finishes.

    If connection errors occur, the packet is resent if possible. If
    reconnecting is not possible, an error is reported.

    Messages are sent via the message pipeline (see QXmpp::Messages) and end-to-end encrypted
    with the encryption selected by the QXmpp::Messages::SelectEncryption hook. If no encryption
    is selected, the encryption extension (setEncryptionExtension()) is used like in earlier
    versions. Without encryption extension, the message is sent unencrypted, unless
    QXmppSendStanzaParams::isEncryptionRequired() is set.

    Returns a QXmppTask that makes it possible to track the state of the packet.

    \deprecated since QXmpp 1.18, use send() and select the encryption via the
    QXmpp::Messages::SelectEncryption hook. With
    QXmppSendStanzaParams::setEncryptionRequired(), sending fails if no encryption is selected.
    IQ requests are encrypted via sendSensitiveIq().

    \since QXmpp 1.5

    \a params and \a stanza.
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
QXmppTask<QXmpp::SendResult> QXmppClient::sendSensitive(QXmppStanza &&stanza, const std::optional<QXmppSendStanzaParams> &params)
{
    return Private::LegacyE2ee::sendSensitive(this, std::move(stanza), params);
}
#endif

QXmppTask<QXmpp::SendResult> QXmpp::Private::LegacyE2ee::sendSensitive(QXmppClient *client, QXmppStanza &&stanza, const std::optional<QXmppSendStanzaParams> &params)
{
    using Private::OutgoingMessages;
    if (OutgoingMessages::isPipelineStanza(stanza)) {
        return client->d->outgoingMessages.send(std::move(static_cast<QXmppMessage &>(stanza)), params.value_or(QXmppSendStanzaParams()), OutgoingMessages::Mode::Sensitive);
    }
    if (auto *iq = dynamic_cast<QXmppIq *>(&stanza); iq && client->hooks(nullptr).hasHandlers<Messages::SelectEncryption>()) {
        return client->d->encryptedIqs.send(*iq, Private::EncryptedIqs::serialize(*iq), params.value_or(QXmppSendStanzaParams()), {});
    }
    if (client->d->encryptionExtension) {
        if (dynamic_cast<QXmppMessage *>(&stanza)) {
            return client->d->sendEncryptedMessage(client->d->encryptionExtension->encryptMessage(dynamic_cast<QXmppMessage &&>(stanza), params));
        } else if (dynamic_cast<QXmppIq *>(&stanza)) {
            return client->d->sendEncryptedIq(client->d->encryptionExtension->encryptIq(dynamic_cast<QXmppIq &&>(stanza), params));
        }
    }
    return client->d->stream->streamAckManager().send(stanza);
}

QXmppTask<QXmpp::SendResult> QXmppClientPrivate::sendEncryptedMessage(QXmppTask<QXmppE2eeExtension::MessageEncryptResult> task)
{
    auto result = co_await task.withContext(q);
    if (std::holds_alternative<QXmppError>(result)) {
        co_return std::get<QXmppError>(std::move(result));
    }
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    std::get<std::unique_ptr<QXmppMessage>>(result)->toXml(&writer, QXmpp::ScePublic);

    co_return co_await stream->streamAckManager().send(QXmppPacket(xml, true));
}

QXmppTask<QXmpp::SendResult> QXmppClientPrivate::sendEncryptedIq(QXmppTask<QXmppE2eeExtension::IqEncryptResult> task)
{
    auto result = co_await task.withContext(q);
    if (std::holds_alternative<QXmppError>(result)) {
        co_return std::get<QXmppError>(std::move(result));
    }
    co_return co_await stream->streamAckManager().send(QXmppPacket(*std::get<std::unique_ptr<QXmppIq>>(result)));
}

/*!
    Sends a packet.

    Messages are sent via the message pipeline (see QXmpp::Messages). They are only end-to-end
    encrypted if an encryption is selected by the QXmpp::Messages::SelectEncryption hook. Other
    stanzas are sent without end-to-end encryption.

    Returns a QXmppTask that makes it possible to track the state of the \a stanza.

    \a params.

    \since QXmpp 1.5
*/
QXmppTask<SendResult> QXmppClient::send(QXmppStanza &&stanza, const std::optional<QXmppSendStanzaParams> &params)
{
    using Private::OutgoingMessages;
    if (OutgoingMessages::isPipelineStanza(stanza)) {
        return d->outgoingMessages.send(std::move(static_cast<QXmppMessage &>(stanza)), params.value_or(QXmppSendStanzaParams()), OutgoingMessages::Mode::Default);
    }
    return d->stream->streamAckManager().send(stanza);
}

/*!
    Sends the stanza with the same encryption as \a e2eeMetadata.

    When there is no e2eeMetadata given this sends the stanza like send(), otherwise like
    sendSensitive(). For messages, the encryption of \a e2eeMetadata is the default selection of
    the QXmpp::Messages::SelectEncryption hook if there is a provider for it.
    Intended to be used for replies to IQs and messages.

    \deprecated since QXmpp 1.18, use send() and select the encryption via the
    QXmpp::Messages::SelectEncryption hook. Responses to IQ requests handled via
    QXmpp::handleIqRequests() are encrypted like the request.

    \since QXmpp 1.5

    \a params and \a stanza.
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
QXmppTask<QXmpp::SendResult> QXmppClient::reply(QXmppStanza &&stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata, const std::optional<QXmppSendStanzaParams> &params)
{
    return Private::LegacyE2ee::reply(this, std::move(stanza), e2eeMetadata, params);
}
#endif

QXmppTask<QXmpp::SendResult> QXmpp::Private::LegacyE2ee::reply(QXmppClient *client, QXmppStanza &&stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata, const std::optional<QXmppSendStanzaParams> &params)
{
    using Private::OutgoingMessages;
    if (e2eeMetadata && OutgoingMessages::isPipelineStanza(stanza)) {
        // the encryption of the received message is the default of SelectEncryption
        auto encryption = e2eeMetadata->encryptionNamespace();
        if (!E2ee::Providers(client->hooks(nullptr)).contains(encryption)) {
            encryption.clear();
        }
        return client->d->outgoingMessages.send(std::move(static_cast<QXmppMessage &>(stanza)), params.value_or(QXmppSendStanzaParams()), OutgoingMessages::Mode::Sensitive, std::move(encryption));
    }
    if (auto *iq = dynamic_cast<QXmppIq *>(&stanza); iq && e2eeMetadata) {
        // the encryption of the request is the default of SelectEncryption
        auto provider = E2ee::Providers(client->hooks(nullptr)).find(e2eeMetadata->encryptionNamespace());
        if (provider && provider->encryptIq) {
            return client->d->encryptedIqs.send(*iq, Private::EncryptedIqs::serialize(*iq), params.value_or(QXmppSendStanzaParams()), e2eeMetadata->encryptionNamespace());
        }
    }
    if (e2eeMetadata) {
        return sendSensitive(client, std::move(stanza), params);
    }
    return client->send(std::move(stanza), params);
}

/*!
    Sends an IQ packet and returns the response asynchronously.

    This is useful for further processing and parsing of the returned
    QDomElement. If you don't expect a special response, you may want use
    sendGenericIq().

    IQs of type 'error' are parsed automatically and returned as QXmppError with a contained
    QXmppStanza::Error.

    This does not do any end-to-encryption on the IQ.

    \sa sendSensitiveIq()

    \since QXmpp 1.5

    \a iq.
*/
QXmppTask<QXmppClient::IqResult> QXmppClient::sendIq(QXmppIq &&iq, const std::optional<QXmppSendStanzaParams> &)
{
    return d->stream->sendIq(std::move(iq));
}

/*!
    Tries to encrypt and send an IQ packet and returns the response
    asynchronously.

    This can be used for sensitive IQ requests performed from client to client.
    Most IQ requests like service discovery requests cannot be end-to-end
    encrypted or it only makes little sense to do so. This is why the default
    sendIq() does not do any additional end-to-end encryption.

    IQs of type 'error' are parsed automatically and returned as QXmppError with a contained
    QXmppStanza::Error.

    If there are handlers of the QXmpp::Messages::SelectEncryption hook, the IQ is encrypted with
    the selected encryption via its e2ee provider (QXmpp::E2ee::Provider::encryptIq) and an
    encrypted response, also an error, is decrypted. If no encryption is selected, the
    encryption extension (setEncryptionExtension()) is used like in earlier versions.

    \since QXmpp 1.5

    \a params and \a iq.
*/
QXmppTask<QXmppClient::IqResult> QXmppClient::sendSensitiveIq(QXmppIq &&iq, const std::optional<QXmppSendStanzaParams> &params)
{
    if (hooks(nullptr).hasHandlers<Messages::SelectEncryption>()) {
        // serialized before the IQ is copied, which would lose the payload of subclasses
        auto element = Private::EncryptedIqs::serialize(iq);
        return d->encryptedIqs.sendRequest(iq, std::move(element), params.value_or(QXmppSendStanzaParams()));
    }
    return d->encryptionExtension ? d->sendSensitiveIq(std::move(iq), params) : d->stream->sendIq(std::move(iq));
}

QXmppTask<QXmppClient::IqResult> QXmppClientPrivate::sendSensitiveIq(QXmppIq iq, std::optional<QXmppSendStanzaParams> params)
{
    auto encryptResult = co_await encryptionExtension->encryptIq(std::move(iq), params);

    if (std::holds_alternative<QXmppError>(encryptResult)) {
        co_return std::get<QXmppError>(std::move(encryptResult));
    }

    auto encryptedIq = std::get<std::unique_ptr<QXmppIq>>(std::move(encryptResult));
    auto sendResult = co_await stream->sendIq(std::move(*encryptedIq)).withContext(q);

    if (std::holds_alternative<QXmppError>(sendResult)) {
        co_return std::get<QXmppError>(std::move(sendResult));
    }
    auto responseIqElement = std::get<QDomElement>(sendResult);

    // iq sent, response received
    if (!isIqResponse(responseIqElement)) {
        co_return QXmppError { u"Invalid IQ response received."_s, QXmpp::SendError::EncryptionError };
    }
    if (!encryptionExtension) {
        co_return QXmppError { u"No decryption extension found."_s, QXmpp::SendError::EncryptionError };
    }

    // try to decrypt the result (should be encrypted)
    auto decryptResult = co_await encryptionExtension->decryptIq(responseIqElement);

    co_return map<QXmppClient::IqResult>(
        [&](QXmppE2eeExtension::NotEncrypted) -> QXmppClient::IqResult {
            // the IQ response from the other entity was not encrypted
            // then report IQ response without modifications
            // TODO: should we return an QXmppError instead?
            return responseIqElement;
        },
        std::move(decryptResult));
}

/*!
    Sends an IQ \a iq and returns possible stanza errors.

    If you want to parse a special IQ response in the result case, you can use
    sendIq() and parse the returned QDomElement.

    Returns QXmpp::Success (on response type 'result') or the contained
    QXmppStanza::Error (on response type 'error').

    \since QXmpp 1.5
*/
QXmppTask<QXmppClient::EmptyResult>
QXmppClient::sendGenericIq(QXmppIq &&iq, const std::optional<QXmppSendStanzaParams> &)
{
    co_return mapToSuccess(co_await sendIq(std::move(iq)));
}

/*!
    Disconnects the client and the current presence of client changes to
    QXmppPresence::Unavailable and status text changes to "Logged out".

    \note Make sure that the clientPresence is changed to
    QXmppPresence::Available, if you are again calling connectToServer() after
    calling the disconnectFromServer() function.
*/
void QXmppClient::disconnectFromServer()
{
    d->cancelReconnect();

    d->clientPresence.setType(QXmppPresence::Unavailable);
    d->clientPresence.setStatusText(u"Logged out"_s);
    if (d->stream->isConnected()) {
        d->resendPresence();
    }

    d->stream->disconnectFromHost();
}

/*!
    Reconnects immediately if the client is waiting to reconnect and resets the reconnection
    backoff.

    Use this when reconnecting is likely to succeed now, for example when the application returns
    to the foreground or the user explicitly asks to retry.

    After a failed TLS handshake (e.g. because of an untrusted certificate), the client does not
    retry on its own, as the same error would occur again. It only reconnects when this is called
    (e.g. after the user decided to trust the certificate) or when the network changes (see
    setNetworkAvailable() and checkConnection()).

    Does nothing if the client is connected or connecting, or if it is not going to reconnect
    (automatic reconnection is disabled, disconnectFromServer() was called, another client took
    over the resource or the server reported a permanent stream error).

    \since QXmpp 1.17
*/
void QXmppClient::reconnectNow()
{
    if (!d->reconnectionTimer->isActive() && !d->reconnectionDeferred) {
        return;
    }

    d->cancelReconnect();
    d->reconnectionTries = 0;
    _q_reconnect();
}

/*!
    Tells the client whether a network connection is \a available.

    Applications that monitor the network themselves (e.g. using NetworkManager directly) can use
    this to reconnect based on network changes instead of only a timer:

    \list
    \li When the network becomes unavailable, the connection is closed immediately (without ending
    the stream, so it can be resumed later) instead of waiting for the ping to time out, and no
    reconnection attempts are made.
    \li When the network becomes available, the client reconnects immediately (see
    reconnectNow()).
    \endlist

    An available network does not guarantee that the server can be reached (e.g. behind captive
    portals), so the reconnection timer is still used as a fallback.

    The network is considered available by default.

    Connections to a server on the local machine (\c localhost or a loopback address) are not
    affected, as they work without network, unless a proxy is used.

    To use QNetworkInformation for this, add a QXmppNetworkMonitor to the client instead.

    \sa checkConnection()
    \since QXmpp 1.17
*/
void QXmppClient::setNetworkAvailable(bool available)
{
    d->networkAvailable = available;

    if (available) {
        reconnectNow();
        return;
    }

    // a server on the local machine can still be reached
    if (d->isServerLoopback()) {
        return;
    }

    if (d->reconnectionTimer->isActive()) {
        d->reconnectionTimer->stop();
        d->reconnectionDeferred = true;
    }

    if (state() != DisconnectedState) {
        info(u"Network unavailable, closing connection"_s);
        d->stream->disconnectForResumption();
        d->reconnectionDeferred = configuration().autoReconnectionEnabled() && !d->receivedConflict;
    }
}

/*!
    Returns whether a network connection is available, as set with setNetworkAvailable().

    \since QXmpp 1.17
*/
bool QXmppClient::isNetworkAvailable() const
{
    return d->networkAvailable;
}

/*!
    Checks whether the connection is still alive.

    If the client is connected, a ping is sent immediately. If it is not answered within five
    seconds (or QXmppConfiguration::keepAliveTimeout() if that is shorter), the connection is
    closed (without ending the stream, so it can be resumed) and the client reconnects. If a ping
    is already pending, no other one is sent, but it times out just as soon. If the client is
    waiting to reconnect, it reconnects immediately (see reconnectNow()).

    Use this when the network changed in a way that may have broken the connection without the
    socket noticing, for example when switching from Wi-Fi to mobile data.

    \since QXmpp 1.17
*/
void QXmppClient::checkConnection()
{
    if (d->stream->isConnected()) {
        d->stream->pingNow();
    } else {
        reconnectNow();
    }
}

/*! Returns true if the client has authenticated with the XMPP server. */
bool QXmppClient::isAuthenticated() const
{
    return d->stream->isAuthenticated();
}

/*! Returns true if the client is connected to the XMPP server. */
bool QXmppClient::isConnected() const
{
    return d->stream->isConnected();
}

/*!
    Returns true if the current client state is "active", false if it is
    "inactive". See \xep{0352}{Client State Indication} for details.

    \since QXmpp 1.0
*/
bool QXmppClient::isActive() const
{
    return d->stream->csiManager().state() == CsiManager::Active;
}

/*!
    Sets the client state as described in \xep{0352}{Client State Indication}.

    Since QXmpp 1.8, the state is restored across reconnects. QXmpp will re-send the state of
    'inactive' on connection if that was set before. Stream resumptions are also handled.

    \since QXmpp 1.0

    \a active.
*/
void QXmppClient::setActive(bool active)
{
    d->stream->csiManager().setState(active ? CsiManager::Active : CsiManager::Inactive);
}

/*!
    Returns the current \xep{0198}{Stream Management} state of the connection.

    Upon connection of the client this can be used to check whether the
    previous stream has been resumed.

    \since QXmpp 1.4
*/
QXmppClient::StreamManagementState QXmppClient::streamManagementState() const
{
    if (d->stream->c2sStreamManager().enabled()) {
        if (d->stream->c2sStreamManager().streamResumed()) {
            return ResumedStream;
        }
        return NewStream;
    }
    return NoStreamManagement;
}

QXmppOutgoingClient *QXmppClient::stream() const
{
    return d->stream;
}

/*! Returns the client's current state. */
QXmppClient::State QXmppClient::state() const
{
    return d->stream->state();
}

/*! Returns the client's current presence. */
QXmppPresence QXmppClient::clientPresence() const
{
    return d->clientPresence;
}

/*!
    Changes the presence of the connected client.

    The connection to the server will be updated accordingly:

    \list
    \li If the presence type is QXmppPresence::Unavailable, the connection
    to the server will be closed.
    \li Otherwise, the connection to the server will be established
    as needed.
    \endlist

    \a presence is a QXmppPresence object.
*/
void QXmppClient::setClientPresence(const QXmppPresence &presence)
{
    d->clientPresence = presence;
    d->addProperCapability(d->clientPresence);

    if (presence.type() == QXmppPresence::Unavailable) {
        d->cancelReconnect();

        // NOTE: we can't call disconnect() because it alters
        // the client presence
        if (d->stream->isConnected()) {
            d->resendPresence();
        }

        d->stream->disconnectFromHost();
    } else if (d->stream->isConnected()) {
        d->resendPresence();
    } else {
        connectToServer(d->stream->configuration(), presence);
    }
}

/*! Returns the socket error if error() is QXmppClient::SocketError. */
QAbstractSocket::SocketError QXmppClient::socketError()
{
    return d->stream->xmppSocket().internalSocket()->error();
}

/*! Returns the human-readable description of the last socket error if error() is QXmppClient::SocketError. */
QString QXmppClient::socketErrorString() const
{
    return d->stream->xmppSocket().internalSocket()->errorString();
}

/*! Returns the XMPP stream error if QXmppClient::Error is QXmppClient::XmppStreamError. */
QXmppStanza::Error::Condition QXmppClient::xmppStreamError()
{
    return d->stream->xmppStreamError();
}

void QXmppClient::injectIq(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
{
    if (element.tagName() != u"iq") {
        return;
    }
    if (!d->handleStanza(element, e2eeMetadata)) {
        const auto iqType = element.attribute(u"type"_s);
        if (iqType == u"get" || iqType == u"set") {
            // send error IQ
            using Err = QXmppStanza::Error;

            QXmppIq iq(QXmppIq::Error);
            iq.setTo(element.attribute(u"from"_s));
            iq.setId(element.attribute(u"id"_s));
            const auto errMessage = e2eeMetadata.has_value()
                ? u"Feature not implemented or not supported with end-to-end encryption."_s
                : u"Feature not implemented."_s;
            iq.setError(Err(Err::Cancel, Err::FeatureNotImplemented, errMessage));
            Private::LegacyE2ee::reply(this, std::move(iq), e2eeMetadata);
        }
        // don't do anything for "result" and "error" IQs
    }
}

/*!
    Processes the message like a received one and emits messageReceived() if no extension
    handles it. The message may be processed asynchronously, e.g. if it needs to be decrypted.
    Always returns true.
*/
bool QXmppClient::injectMessage(QXmppMessage &&message)
{
    d->incomingMessages.process(Messages::Context(std::move(message), Messages::Origin::Injected));
    return true;
}

/*!
    Sets stream errors \a errors that are ignored if they occur.

    \since QXmpp 1.9
*/
void QXmppClient::setIgnoredStreamErrors(const QList<QXmpp::StreamError> &errors)
{
    d->ignoredStreamErrors = errors;
}

/*! Give extensions a chance to handle incoming stanzas. */
void QXmppClient::_q_elementReceived(const QDomElement &element, bool &handled)
{
    // The stanza comes directly from the XMPP stream, so it's not end-to-end
    // encrypted and there's no e2ee metadata (std::nullopt).
    // encrypted IQ requests are decrypted by the e2ee providers
    if (d->encryptedIqs.processIncoming(element)) {
        handled = true;
        return;
    }
    handled = d->handleStanza(element, std::nullopt);
    if (!handled && element.tagName() == u"message") {
        // messages not handled by the legacy handleStanza() are always handled by the pipeline
        d->incomingMessages.processElement(element, d->encryptionExtension);
        handled = true;
    }
}

void QXmppClient::_q_reconnect()
{
    if (d->stream->configuration().autoReconnectionEnabled()) {
        debug(u"Reconnecting to server"_s);
        d->stream->connectToHost();
    }
}

/*! At connection establishment, send initial presence. */
void QXmppClient::_q_streamConnected(const QXmpp::Private::SessionBegin &session)
{
    d->receivedConflict = false;
    d->connectedSince.start();
    d->lastServerWasLoopback = d->stream->xmppSocket().internalSocket()->peerAddress().isLoopback();

    // notify managers
    if (session.fastTokenChanged) {
        Q_EMIT credentialsChanged();
    }
    Q_EMIT connected();

    // send initial presence
    if (d->stream->isAuthenticated() && streamManagementState() != ResumedStream) {
        d->resendPresence();
    }
}

void QXmppClient::_q_streamDisconnected()
{
    // notify managers
    Q_EMIT disconnected();
}

/*! Returns the QXmppLogger associated with the current QXmppClient. */
QXmppLogger *QXmppClient::logger() const
{
    return d->logger;
}

/*!
    Sets the QXmppLogger associated with the current QXmppClient.

    \a logger.
*/
void QXmppClient::setLogger(QXmppLogger *logger)
{
    if (logger != d->logger) {
        if (d->logger) {
            disconnect(this, &QXmppLoggable::logMessage,
                       d->logger, &QXmppLogger::log);
            disconnect(this, &QXmppLoggable::setGauge,
                       d->logger, &QXmppLogger::setGauge);
            disconnect(this, &QXmppLoggable::updateCounter,
                       d->logger, &QXmppLogger::updateCounter);
        }

        d->logger = logger;
        if (d->logger) {
            connect(this, &QXmppLoggable::logMessage,
                    d->logger, &QXmppLogger::log);
            connect(this, &QXmppLoggable::setGauge,
                    d->logger, &QXmppLogger::setGauge);
            connect(this, &QXmppLoggable::updateCounter,
                    d->logger, &QXmppLogger::updateCounter);
        }

        Q_EMIT loggerChanged(d->logger);
    }
}

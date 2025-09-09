// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppCallManager.h"

#include "QXmppCall.h"
#include "QXmppCallManager_p.h"
#include "QXmppCall_p.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppExternalServiceDiscoveryIq.h"
#include "QXmppIqHandling.h"
#include "QXmppJingleIq.h"
#include "QXmppRosterManager.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "Async.h"
#include "GstWrapper.h"
#include "Iq.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <gst/gst.h>

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

QXmppTask<ServicesResult> QXmpp::Private::requestExternalServices(QXmppClient *client, const QString &jid)
{
    QXmppExternalServiceDiscoveryIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);

    return chainIq(client->sendIq(std::move(request)), client, [](QXmppExternalServiceDiscoveryIq &&iq) -> ServicesResult {
        return iq.externalServices();
    });
}

QXmppTask<ServiceResult> QXmpp::Private::requestCredentials(QXmppClient *client, const QString &jid, const QString &type, const QString &host)
{
    struct ExtDiscoCredentials {
        QXmppExternalService service;

        void parse(const QDomElement &el)
        {
            auto credentialsEl = el.firstChildElement();
            service = parseOptionalChildElement<QXmppExternalService>(credentialsEl)
                          .value_or(QXmppExternalService());
        }
        void toXml(XmlWriter &w) const
        {
            w.write(Element { { u"credentials", ns_external_service_discovery }, service });
        }
    };

    QXmppExternalService s;
    s.setHost(host);
    s.setType(type);

    CompatIq iq { GetIq<ExtDiscoCredentials> {
        generateSequentialStanzaId(),
        {},
        jid,
        {},
        { std::move(s) },
    } };

    return chain<ServiceResult>(client->sendIq(std::move(iq)), client, [](QXmppClient::IqResult result) {
        return map<ServiceResult>(
            [](ExtDiscoCredentials &&c) { return c.service; },
            parseIqResponseFlat<ExtDiscoCredentials>(std::move(result)));
    });
}

QXmppTask<StunTurnResult> QXmpp::Private::requestStunTurnConfig(QXmppClient *client, QXmppLoggable *q)
{
    QXmppPromise<StunTurnResult> p;
    requestExternalServices(client, client->configuration().domain()).then(q, [p, client, q](const auto &result) mutable {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            qWarning() << u"Could not fetch STUN/TURN external services from server: %1"_s.arg(error->description);
            p.finish(*error);
            return;
        }

        const auto &services = std::get<QVector<QXmppExternalService>>(result);
        // STUN (all)
        auto stunServers = transformFilter<QList<StunServerConfig>>(services, [](const auto &service) -> std::optional<StunServerConfig> {
            if (service.type() == u"stun" && service.port()) {
                return StunServerConfig {
                    { QHostAddress(service.host()), quint16(service.port().value()) },
                    service.expires(),
                };
            }
            return {};
        });

        // TURN (only one; prefer with credentials)
        std::optional<TurnServerConfig> turnServer;
        bool credentialsAvailable = false;
        for (const auto &service : std::as_const(services)) {
            if (service.type() == u"turn" && service.port()) {
                turnServer = {
                    TurnServer {
                        QHostAddress(service.host()),
                        quint16(service.port().value()),
                        service.username().value_or(QString()),
                        service.password().value_or(QString()),
                    },
                    service.expires(),
                };
                credentialsAvailable = service.username() && service.password();

                // prefer TURN server with credentials
                if (credentialsAvailable) {
                    break;
                }
            }
        }

        // request TURN credentials (if not provided already)
        if (turnServer && !credentialsAvailable) {
            // request credentials for TURN server
            chain(requestCredentials(client, client->configuration().domain(), u"turn"_s, turnServer->server.host.toString()),
                  q,
                  std::move(p),
                  [q, stunServers = std::move(stunServers)](auto &&result) mutable -> StunTurnResult {
                      if (auto *error = std::get_if<QXmppError>(&result)) {
                          qWarning() << u"Could not fetch credentials for TURN server: %1"_s.arg(error->description);
                          return StunTurnConfig { std::move(stunServers), {} };
                      }

                      auto &&service = std::get<QXmppExternalService>(std::move(result));
                      if (!service.username() || !service.password() || !service.port()) {
                          qWarning() << u"Server did not return credentials for TURN server upon explicit request"_s;
                          return StunTurnConfig { std::move(stunServers), {} };
                      }

                      return StunTurnConfig {
                          std::move(stunServers),
                          TurnServerConfig {
                              TurnServer {
                                  QHostAddress(service.host()),
                                  quint16(service.port().value()),
                                  service.username().value(),
                                  service.password().value(),
                              },
                              service.expires(),
                          },
                      };
                  });
        } else {
            p.finish(StunTurnConfig { std::move(stunServers), std::move(turnServer) });
        }
    });
    return p.task();
}

QXmppCallManagerPrivate::QXmppCallManagerPrivate(QXmppCallManager *qq)
    : q(qq)
{
    // Initialize GStreamer
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }

    supportsDtls = checkGstFeature("dtlsdec"_L1) && checkGstFeature("dtlsenc"_L1);
}

void QXmppCallManagerPrivate::addCall(QXmppCall *call)
{
    calls.append(call);
    QObject::connect(call, &QObject::destroyed, q, &QXmppCallManager::onCallDestroyed);
}

QList<StunServer> QXmppCallManagerPrivate::stunServers() const
{
    if (stunTurnServers) {
        return transform<QList<StunServer>>(stunTurnServers->stun, [](const auto &c) {
                   return c.server;
               }) +
            fallbackStunServers;
    }
    return fallbackStunServers;
}

std::optional<TurnServer> QXmppCallManagerPrivate::turnServer() const
{
    if (stunTurnServers && stunTurnServers->turn) {
        return stunTurnServers->turn->server;
    }
    return fallbackTurnServer;
}

///
/// \class QXmppCallManager
///
/// \brief The QXmppCallManager class provides support for making and receiving voice calls.
///
/// Session initiation is performed as described by \xep{0166, Jingle}, \xep{0167, Jingle RTP
/// Sessions} and \xep{0176, Jingle ICE-UDP Transport Method}.
///
/// The data stream is connected using Interactive Connectivity Establishment (RFC 5245) and data
/// is transferred using Real Time Protocol (RFC 3550) packets.
///
/// To make use of this manager, you need to instantiate it and load it into the QXmppClient
/// instance as follows:
/// ```cpp
/// auto *client = new QXmppClient();
/// auto *callManager = client->addNewExtension<QXmppCallManager>();
/// ```
///
/// ## Call interaction
///
/// Incoming calls are exposed via the callReceived() signal. You can take ownership of the call by
/// moving the unique_ptr, otherwise the call manager will decline and delete the call. You can
/// accept or reject (hang up) the call.
///
/// Outgoing calls are created using call().
///
/// In both cases you are responsible for taking ownership of the call. Note that QXmppCalls in
/// another state than finished require the QXmppCallManager to be active, though. You must not
/// delete the QXmppCallManager until all QXmppCalls are in finished state.
///
/// ## XEP-0320: Use of DTLS-SRTP in Jingle Sessions
///
/// DTLS-SRTP allows to encrypt peer-to-peer calls. Internally, a TLS handshake is done to
/// negotiate keys for SRTP (Secure RTP). By default DTLS is not enforced, this can be done using
/// setDtlsRequired(), though.
///
/// DTLS-SRTP by default exchanges the fingerprint via unencrypted XMPP packets. This means that
/// the XMPP server could potentially replace the fingerprint or prevent the clients from using
/// DTLS at all. However, the actual media connection is typically peer-to-peer, so the XMPP server
/// does not have access to the transmitted data.
///
/// Support for DTLS-SRTP is available since QXmpp 1.11.
///
/// \warning THIS API IS NOT FINALIZED YET
///
/// \ingroup Managers
///

///
/// \fn QXmppCallManager::callReceived()
///
/// This signal is emitted when an incoming call is received.
///
/// You can take over ownership of the call by moving out the unique pointer. However, this is
/// only possible for one slot connected to this signal, all other slots after that will receive a
/// nullptr.
/// ```
/// std::vector<std::unique_ptr<QXmppCall>> myActiveCalls;
/// connect(manager, &QXmppCallManager::callReceived, this, [&](std::unique_ptr<QXmppCall> &call) {
///     // take over ownership
///     myActiveCalls.push_back(std::move(call));
///     // call is now nullptr
/// });
/// ```
/// Note that you do not need to continue to use a unique pointer for memory management, you can
/// also use QObject-parent ownership or another ownership model.
///
/// \note If you do not take ownership of the call, the call manager will automatically decline
/// the call.
///
/// \note Incoming calls need to be accepted or rejected using QXmppCall::accept() or
/// QXmppCall::hangUp().
///
/// \since QXmpp 1.11, previously this signal had a different signature.
///

///
/// Constructs a QXmppCallManager object to handle incoming and outgoing
/// Voice-Over-IP calls.
///
QXmppCallManager::QXmppCallManager()
    : d(std::make_unique<QXmppCallManagerPrivate>(this))
{
}

///
/// Destroys the QXmppCallManager object.
///
QXmppCallManager::~QXmppCallManager() = default;

///
/// Sets STUN servers that are used as a fallback, additionally to the ones provided by the XMPP
/// server via \xep{0215, External Service Discovery}.
///
/// STUN is used to determine server-reflexive addresses and ports.
///
/// \since QXmpp 1.11
///
void QXmppCallManager::setFallbackStunServers(const QList<StunServer> &servers)
{
    d->fallbackStunServers = servers;
}

///
/// Set a TURN server that is used as a fallback, if the XMPP server does not provide any TURN
/// server via \xep{0215, External Service Discovery}.
///
/// TURN is used to relay packets in double-NAT configurations.
///
/// \since QXmpp 1.11
///
void QXmppCallManager::setFallbackTurnServer(const std::optional<TurnServer> &server)
{
    d->fallbackTurnServer = server;
}

/// \cond
QStringList QXmppCallManager::discoveryFeatures() const
{
    QStringList features = {
        ns_jingle.toString(),      // XEP-0166: Jingle
        ns_jingle_rtp.toString(),  // XEP-0167: Jingle RTP Sessions
        ns_jingle_rtp_audio.toString(),
        ns_jingle_rtp_video.toString(),
        ns_jingle_ice_udp.toString(),  // XEP-0176: Jingle ICE-UDP Transport Method
    };
    if (d->supportsDtls) {
        features.append(ns_jingle_dtls.toString());  // XEP-0320: Use of DTLS-SRTP in Jingle Sessions
    }
    return features;
}

bool QXmppCallManager::handleStanza(const QDomElement &element)
{
    return handleIqRequests<QXmppJingleIq>(element, client(), [this](auto &&iq) {
        return handleIq(std::move(iq));
    });
}

void QXmppCallManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::disconnected, this, &QXmppCallManager::onDisconnected);
    connect(client, &QXmppClient::presenceReceived, this, &QXmppCallManager::onPresenceReceived);
}

void QXmppCallManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
}
/// \endcond

///
/// Initiates a new outgoing call to the specified recipient.
///
/// \param jid Full-JID of the recipient.
/// \param proposedSid Session ID of the call. If empty, a new session ID will be generated. Must be unique.
///
/// \since QXmpp 1.11, previously this function had a different signature.
///
std::unique_ptr<QXmppCall> QXmppCallManager::call(const QString &jid, const QString &proposedSid)
{
    auto sid = proposedSid;

    auto errorCall = [&](QXmppError &&error) {
        warning(error.description);
        return std::unique_ptr<QXmppCall> {
            new QXmppCall(jid, sid, QXmppCall::OutgoingDirection, QXmppCall::FinishedState, std::move(error), this)
        };
    };

    if (jid.isEmpty()) {
        return errorCall({ u"Refusing to call an empty jid"_s, {} });
    }

    if (jid == client()->configuration().jid()) {
        return errorCall({ u"Refusing to call self"_s, {} });
    }

    // verify session id is unique
    if (sid.isEmpty()) {
        sid = QXmppUtils::generateStanzaUuid();
    }
    if (contains(d->calls, sid, &QXmppCall::sid)) {
        return errorCall({ u"Call with the same 'sid' already in progress."_s, {} });
    }

    if (d->dtlsRequired && !d->supportsDtls) {
        return errorCall({ u"DTLS encryption for calls is required, but not supported locally."_s, {} });
    }

    auto call = std::unique_ptr<QXmppCall>(new QXmppCall(jid, sid, QXmppCall::OutgoingDirection, this));

    auto *discoManager = client()->findExtension<QXmppDiscoveryManager>();
    Q_ASSERT_X(discoManager != nullptr, "call", "QXmppCallManager requires QXmppDiscoveryManager to be registered.");
    discoManager->info(jid).then(call.get(), [this, call = call.get()](auto result) {
        auto failure = [&](QString &&text) {
            warning(text);
            call->d->error = QXmppError { std::move(text), {} };
            call->d->setState(QXmppCall::FinishedState);
        };

        if (auto *error = std::get_if<QXmppError>(&result)) {
            failure(u"Error fetching service discovery features for calling %1: %2"_s
                        .arg(call->jid(), error->description));
            return;
        }

        // determine supported features of remote
        auto &&info = std::get<QXmppDiscoInfo>(std::move(result));
        const auto remoteFeatures = info.features();
        auto testFeature = [&](QStringView feature, QString &&text) {
            if (contains(remoteFeatures, feature)) {
                return true;
            }
            failure(std::move(text));
            return false;
        };
        if (!testFeature(ns_jingle, u"Remote does not support Jingle"_s) ||
            !testFeature(ns_jingle_rtp, u"Remote does not support Jingle RTP"_s) ||
            !testFeature(ns_jingle_rtp_audio, u"Remote does not support Jingle RTP audio"_s) ||
            !testFeature(ns_jingle_ice_udp, u"Remote does not support Jingle ICE-UDP"_s)) {
            return;
        }

        call->d->useDtls = d->supportsDtls && contains(remoteFeatures, ns_jingle_dtls);
        call->d->videoSupported = contains(remoteFeatures, ns_jingle_rtp_video);

        if (!call->d->useDtls && d->dtlsRequired) {
            failure(u"Remote does not support DTLS, but required locally."_s);
            return;
        }

        refreshStunTurnConfig().then(call, [this, call] {
            auto *stream = call->d->createStream(u"audio"_s, u"initiator"_s, u"microphone"_s);

            // register call
            d->addCall(call);

            call->d->sendInvite();
        });
    });

    return call;
}

///
/// Returns whether the call manager requires encryption using \xep{0320, Use of DTLS-SRTP in
/// Jingle Sessions} for all calls.
///
/// \since QXmpp 1.11
///
bool QXmppCallManager::dtlsRequired() const
{
    return d->dtlsRequired;
}

///
/// Sets whether the call manager requires encryption using \xep{0320, Use of DTLS-SRTP in
/// Jingle Sessions} for all calls.
///
/// \since QXmpp 1.11
///
void QXmppCallManager::setDtlsRequired(bool dtlsRequired)
{
    d->dtlsRequired = dtlsRequired;
}

void QXmppCallManager::onCallDestroyed(QObject *object)
{
    d->calls.removeAll(static_cast<QXmppCall *>(object));
}

// Handles disconnection from server.
void QXmppCallManager::onDisconnected()
{
    for (auto *call : std::as_const(d->calls)) {
        call->d->terminate({ QXmppJingleReason::Gone, {}, {} });
    }
}

auto QXmppCallManager::handleIq(QXmppJingleIq &&iq) -> IncomingIqResult
{
    using Error = QXmppStanza::Error;

    if (iq.type() != QXmppIq::Set) {
        return Error { Error::Cancel, Error::BadRequest, u"Jingle IQ only supports type 'set'."_s };
    }

    switch (iq.action()) {
    case QXmppJingleIq::SessionInitiate: {
        // incoming new call

        // do not ack and use FailedApplication reason to not interfere with other calls with the same ID
        if (iq.sid().isEmpty() || contains(d->calls, iq.sid(), &QXmppCall::sid)) {
            return Error { Error::Cancel, Error::Conflict, u"Invalid 'sid' value."_s };
        }

        const auto content = iq.contents().isEmpty() ? QXmppJingleIq::Content() : iq.contents().constFirst();
        bool dtlsRequested = !content.transportFingerprint().isEmpty();

        // build call
        auto call = std::unique_ptr<QXmppCall>(new QXmppCall(iq.from(), iq.sid(), QXmppCall::IncomingDirection, this));
        call->d->useDtls = d->supportsDtls && dtlsRequested;

        if (dtlsRequested && !d->supportsDtls) {
            call->d->terminate({ QXmppJingleReason::FailedApplication, u"DTLS is not supported."_s, {} }, true);
            return {};
        }
        if (!dtlsRequested && d->dtlsRequired) {
            call->d->terminate({ QXmppJingleReason::FailedApplication, u"DTLS required."_s, {} }, true);
            return {};
        }

        // register call
        d->addCall(call.get());

        // first send IQ ack (task may finish instantly)
        later(this, [this, content, callPtr = call.release()]() mutable {
            refreshStunTurnConfig().then(this, [this, content, callPtr]() mutable {
                auto call = std::unique_ptr<QXmppCall>(callPtr);

                // create stream (with up-to-date STUN/TURN credentials)
                auto *stream = call->d->createStream(content.descriptionMedia(), content.creator(), content.name());
                if (!stream) {
                    call->d->terminate({ QXmppJingleReason::FailedApplication, {}, {} }, true);
                    return;
                }

                // check content description and transport
                if (!call->d->handleDescription(stream, content) ||
                    !call->d->handleTransport(stream, content)) {

                    // terminate call
                    call->d->terminate({ QXmppJingleReason::FailedApplication, {}, {} }, true);
                    call->terminated();
                    return;
                }

                // send ringing indication
                auto ringing = call->d->createIq(QXmppJingleIq::SessionInfo);
                ringing.setRtpSessionState(QXmppJingleIq::RtpSessionStateRinging());
                client()->sendIq(std::move(ringing));

                // notify user
                Q_EMIT callReceived(call);

                if (call) {
                    // nobody took over the call

                    // take ownership, delete on finished
                    auto *rawCall = call.release();
                    rawCall->setParent(this);
                    connect(rawCall, &QXmppCall::finished, rawCall, &QObject::deleteLater);

                    // decline call
                    rawCall->d->terminate({ QXmppJingleReason::Decline, {}, {} });
                }
                return;
            });
        });
        return {};
    }
    default: {
        // for all other requests, require a valid call
        auto call = find(d->calls, iq.sid(), &QXmppCall::sid);
        // verify call found AND verify sender is correct
        if (!call || call.value()->jid() != iq.from()) {
            warning(u"Remote party %1 sent a request for an unknown call %2"_s.arg(iq.from(), iq.sid()));
            return Error { Error::Cancel, Error::ItemNotFound, u"Unknown call."_s };
        }
        return into<IncomingIqResult>(call.value()->d->handleRequest(std::move(iq)));
    }
    }
}

void QXmppCallManager::onPresenceReceived(const QXmppPresence &presence)
{
    if (presence.type() != QXmppPresence::Unavailable) {
        return;
    }

    if (auto call = find(std::as_const(d->calls), presence.from(), &QXmppCall::jid)) {
        // the remote party has gone away, terminate call
        auto &callObject = call.value();
        callObject->d->error = { u"Received unavailable presence"_s, {} };
        callObject->d->terminate({ QXmppJingleReason::Gone, {}, {} });
    }
}

QXmppTask<void> QXmppCallManager::refreshStunTurnConfig()
{
    if (d->refreshStunTurnConfigPromise.has_value()) {
        // attach to ongoing task
        return d->refreshStunTurnConfigPromise->task();
    } else {
        if (d->stunTurnServers.has_value()) {
            // TODO: check expiry and refresh credentials
            return makeReadyTask();
        }

        // initial request of STUN/TURN credentials
        d->refreshStunTurnConfigPromise = QXmppPromise<void>();

        requestStunTurnConfig(client(), this).then(this, [this](const auto &result) {
            if (auto *error = std::get_if<QXmppError>(&result)) {
                warning(u"Could not fetch STUN/TURN servers: %1"_s.arg(error->description));
                d->stunTurnServers = StunTurnConfig { {}, {} };
            } else {
                d->stunTurnServers = std::get<StunTurnConfig>(result);
            }

            auto p = std::move(d->refreshStunTurnConfigPromise.value());
            d->refreshStunTurnConfigPromise.reset();
            p.finish();
        });

        return d->refreshStunTurnConfigPromise->task();
    }
}

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
#include "QXmppIqHandling.h"
#include "QXmppJingleIq.h"
#include "QXmppRosterManager.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"

#include "Algorithms.h"
#include "Async.h"
#include "GstWrapper.h"
#include "StringLiterals.h"

#include <gst/gst.h>

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

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
/// accept or reject (hangup) the call.
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
/// QXmppCall::hangup().
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
    connect(client, &QXmppClient::disconnected,
            this, &QXmppCallManager::onDisconnected);

    connect(client, &QXmppClient::presenceReceived,
            this, &QXmppCallManager::onPresenceReceived);
}

void QXmppCallManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, &QXmppClient::disconnected,
               this, &QXmppCallManager::onDisconnected);

    disconnect(client, &QXmppClient::presenceReceived,
               this, &QXmppCallManager::onPresenceReceived);
}
/// \endcond

///
/// Initiates a new outgoing call to the specified recipient.
///
/// \since QXmpp 1.11, previously this function had a different signature.
///
std::unique_ptr<QXmppCall> QXmppCallManager::call(const QString &jid)
{
    if (jid.isEmpty()) {
        warning(u"Refusing to call an empty jid"_s);
        return nullptr;
    }

    if (jid == client()->configuration().jid()) {
        warning(u"Refusing to call self"_s);
        return nullptr;
    }

    if (d->dtlsRequired && !d->supportsDtls) {
        warning(u"DTLS encryption for calls is required, but not supported locally."_s);
        return nullptr;
    }

    auto call = std::unique_ptr<QXmppCall>(new QXmppCall(jid, QXmppCall::OutgoingDirection, this));

    auto *discoManager = client()->findExtension<QXmppDiscoveryManager>();
    Q_ASSERT_X(discoManager != nullptr, "call", "QXmppCallManager requires QXmppDiscoveryManager to be registered.");
    discoManager->requestDiscoInfo(jid).then(call.get(), [this, call = call.get()](auto result) {
        auto failure = [&](QString error) {
            warning(error);
            call->terminated();
        };

        if (auto *error = std::get_if<QXmppError>(&result)) {
            failure(u"Error fetching service discovery features for calling %1: %2"_s
                        .arg(call->jid(), error->description));
            return;
        }

        // determine supported features of remote
        auto &&info = std::get<QXmppDiscoveryIq>(std::move(result));
        const auto remoteFeatures = info.features();
        if (!remoteFeatures.contains(ns_jingle)) {
            failure(u"Remote does not support Jingle"_s);
            return;
        }
        if (!remoteFeatures.contains(ns_jingle_rtp)) {
            failure(u"Remote does not support Jingle RTP"_s);
            return;
        }
        if (!remoteFeatures.contains(ns_jingle_rtp_audio)) {
            failure(u"Remote does not support Jingle RTP audio"_s);
            return;
        }
        if (!remoteFeatures.contains(ns_jingle_ice_udp)) {
            failure(u"Remote does not support Jingle ICE-UDP"_s);
            return;
        }

        call->d->useDtls = d->supportsDtls && remoteFeatures.contains(ns_jingle_dtls);

        if (!call->d->useDtls && d->dtlsRequired) {
            warning(u"Remote does not support DTLS, but required locally."_s);
            call->terminated();
            return;
        }

        auto *stream = call->d->createStream(u"audio"_s, u"initiator"_s, u"microphone"_s);
        call->d->sid = QXmppUtils::generateStanzaHash();

        // register call
        d->addCall(call);

        call->d->sendInvite();
    });

    return call;
}

///
/// Sets multiple STUN servers to use to determine server-reflexive addresses
/// and ports.
///
/// \note This may only be called prior to calling bind().
///
/// \param servers List of the STUN servers.
///
/// \since QXmpp 1.3
///
void QXmppCallManager::setStunServers(const QList<QPair<QHostAddress, quint16>> &servers)
{
    d->stunServers = servers;
}

///
/// Sets a single STUN server to use to determine server-reflexive addresses
/// and ports.
///
/// \note This may only be called prior to calling bind().
///
/// \param host The address of the STUN server.
/// \param port The port of the STUN server.
///
void QXmppCallManager::setStunServer(const QHostAddress &host, quint16 port)
{
    d->stunServers.clear();
    d->stunServers.push_back(qMakePair(host, port));
}

///
/// Sets the TURN server to use to relay packets in double-NAT configurations.
///
/// \param host The address of the TURN server.
/// \param port The port of the TURN server.
///
void QXmppCallManager::setTurnServer(const QHostAddress &host, quint16 port)
{
    d->turnHost = host;
    d->turnPort = port;
}

///
/// Sets the \a user used for authentication with the TURN server.
///
/// \param user
///
void QXmppCallManager::setTurnUser(const QString &user)
{
    d->turnUser = user;
}

///
/// Sets the \a password used for authentication with the TURN server.
///
/// \param password
///
void QXmppCallManager::setTurnPassword(const QString &password)
{
    d->turnPassword = password;
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

std::variant<QXmppIq, QXmppStanza::Error> QXmppCallManager::handleIq(QXmppJingleIq &&iq)
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
        auto call = std::unique_ptr<QXmppCall>(new QXmppCall(iq.from(), QXmppCall::IncomingDirection, this));
        call->d->useDtls = d->supportsDtls && dtlsRequested;
        call->d->sid = iq.sid();

        if (dtlsRequested && !d->supportsDtls) {
            call->d->terminate({ QXmppJingleReason::FailedApplication, u"DTLS is not supported."_s, {} }, true);
            return {};
        }
        if (!dtlsRequested && d->dtlsRequired) {
            call->d->terminate({ QXmppJingleReason::FailedApplication, u"DTLS required."_s, {} }, true);
            return {};
        }

        auto *stream = call->d->createStream(content.descriptionMedia(), content.creator(), content.name());
        if (!stream) {
            call->d->terminate({ QXmppJingleReason::FailedApplication, {}, {} }, true);
            return {};
        }

        // check content description and transport
        if (!call->d->handleDescription(stream, content) ||
            !call->d->handleTransport(stream, content)) {

            // terminate call
            call->d->terminate({ QXmppJingleReason::FailedApplication, {}, {} }, true);
            call->terminated();
            return {};
        }

        // register call
        d->addCall(call.get());

        later(this, [this, call = std::move(call)]() mutable {
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
        return call.value()->d->handleRequest(std::move(iq));
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
        call.value()->d->terminate({ QXmppJingleReason::Gone, {}, {} });
    }
}

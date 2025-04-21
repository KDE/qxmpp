// SPDX-FileCopyrightText: 2019 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppCall.h"

#include "QXmppCallManager.h"
#include "QXmppCallManager_p.h"
#include "QXmppCallStream.h"
#include "QXmppCallStream_p.h"
#include "QXmppCall_p.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppJingleIq.h"
#include "QXmppStun.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"

#include <chrono>

// gstreamer
#include <gst/gst.h>

#include <QDomElement>
#include <QTimer>

using namespace std::chrono_literals;
using namespace QXmpp::Private;

QXmppCallPrivate::QXmppCallPrivate(const QString &jid, QXmppCall::Direction direction, QPointer<QXmppCallManager> manager, QXmppCall *qq)
    : direction(direction),
      jid(jid),
      manager(manager),
      q(qq)
{
    qRegisterMetaType<QXmppCall::State>();

    removeIf(videoCodecs, std::not_fn(isCodecSupported));
    removeIf(audioCodecs, std::not_fn(isCodecSupported));

    pipeline = gst_pipeline_new(nullptr);
    if (!pipeline) {
        qFatal("Failed to create pipeline");
        return;
    }
    rtpBin = gst_element_factory_make("rtpbin", nullptr);
    if (!rtpBin) {
        qFatal("Failed to create rtpbin");
        return;
    }
    // We do not want to build up latency over time
    g_object_set(rtpBin, "drop-on-latency", true, "async-handling", true, "latency", 25, nullptr);
    if (!gst_bin_add(GST_BIN(pipeline.get()), rtpBin)) {
        qFatal("Could not add rtpbin to the pipeline");
    }
    g_signal_connect_swapped(rtpBin, "pad-added",
                             G_CALLBACK(+[](QXmppCallPrivate *p, GstPad *pad) {
                                 p->padAdded(pad);
                             }),
                             this);
    g_signal_connect_swapped(rtpBin, "request-pt-map",
                             G_CALLBACK(+[](QXmppCallPrivate *p, uint sessionId, uint pt) {
                                 p->ptMap(sessionId, pt);
                             }),
                             this);
    g_signal_connect_swapped(rtpBin, "on-ssrc-active",
                             G_CALLBACK(+[](QXmppCallPrivate *p, uint sessionId, uint ssrc) {
                                 p->ssrcActive(sessionId, ssrc);
                             }),
                             this);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qFatal("Unable to set the pipeline to the playing state");
        return;
    }
}

QXmppCallPrivate::~QXmppCallPrivate()
{
    if (gst_element_set_state(pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
        qFatal("Unable to set the pipeline to the null state");
    }
    // Delete streams before pipeline.
    // Streams still need to be children of QXmppCall for logging to work.
    qDeleteAll(streams);
}

void QXmppCallPrivate::ssrcActive(uint sessionId, uint ssrc)
{
    Q_UNUSED(ssrc)
    GstElement *rtpSession;
    g_signal_emit_by_name(rtpBin, "get-session", static_cast<uint>(sessionId), &rtpSession);
    // TODO: implement bitrate controller
}

void QXmppCallPrivate::padAdded(GstPad *pad)
{
    auto nameParts = QString::fromUtf8(gst_pad_get_name(pad)).split(u'_');
    if (nameParts.size() < 4) {
        return;
    }
    if (nameParts[0] == u"recv" ||
        nameParts[1] == u"rtp" ||
        nameParts[2] == u"src") {
        if (nameParts.size() != 6) {
            return;
        }

        int sessionId = nameParts[3].toInt();
        int pt = nameParts[5].toInt();
        auto *stream = find(streams, sessionId, &QXmppCallStream::id).value();

        // add decoder for codec
        if (stream->media() == VIDEO_MEDIA) {
            if (auto codec = find(videoCodecs, pt, &GstCodec::pt)) {
                stream->d->addDecoder(pad, *codec);
            }
        } else if (stream->media() == AUDIO_MEDIA) {
            if (auto codec = find(audioCodecs, pt, &GstCodec::pt)) {
                stream->d->addDecoder(pad, *codec);
            }
        }
    }
}

GstCaps *QXmppCallPrivate::ptMap(uint sessionId, uint pt)
{
    auto *stream = find(streams, sessionId, &QXmppCallStream::id).value();
    if (auto payloadType = find(stream->d->payloadTypes, pt, &QXmppJinglePayloadType::id)) {
        return gst_caps_new_simple("application/x-rtp",
                                   "media", G_TYPE_STRING, stream->media().toLatin1().data(),
                                   "clock-rate", G_TYPE_INT, payloadType->clockrate(),
                                   "encoding-name", G_TYPE_STRING, payloadType->name().toLatin1().data(),
                                   nullptr);
    }
    q->warning(u"Remote party %1 transmits wrong %2 payload for call %3"_s.arg(jid, stream->media(), sid));
    return nullptr;
}

bool QXmppCallPrivate::isFormatSupported(const QString &codecName)
{
    return GstElementFactoryPtr(gst_element_factory_find(codecName.toLatin1().data())) != nullptr;
}

bool QXmppCallPrivate::isCodecSupported(const GstCodec &codec)
{
    return isFormatSupported(codec.gstPay) &&
        isFormatSupported(codec.gstDepay) &&
        isFormatSupported(codec.gstEnc) &&
        isFormatSupported(codec.gstDec);
}

bool QXmppCallPrivate::handleDescription(QXmppCallStream *stream, const QXmppJingleIq::Content &content)
{
    stream->d->payloadTypes = content.payloadTypes();
    auto it = stream->d->payloadTypes.begin();
    bool foundCandidate = false;
    while (it != stream->d->payloadTypes.end()) {
        bool dynamic = it->id() >= 96;
        bool supported = false;
        auto codecs = stream->media() == AUDIO_MEDIA ? audioCodecs : videoCodecs;
        for (auto &codec : codecs) {
            if (dynamic) {
                if (codec.name == it->name() &&
                    codec.clockrate == it->clockrate() &&
                    codec.channels == it->channels()) {
                    if (!foundCandidate) {
                        stream->d->addEncoder(codec);
                        foundCandidate = true;
                    }
                    supported = true;
                    /* Adopt id from other side. */
                    codec.pt = it->id();
                }
            } else {
                if (codec.pt == it->id() &&
                    codec.clockrate == it->clockrate() &&
                    codec.channels == it->channels()) {
                    if (!foundCandidate) {
                        stream->d->addEncoder(codec);
                        foundCandidate = true;
                    }
                    supported = true;
                    /* Keep our name just to be sure */
                    codec.name = it->name();
                }
            }
        }

        if (!supported) {
            it = stream->d->payloadTypes.erase(it);
        } else {
            ++it;
        }
    }

    if (stream->d->payloadTypes.empty()) {
        q->warning(u"Remote party %1 did not provide any known %2 payloads for call %3"_s.arg(jid, stream->media(), sid));
        return false;
    }

    return true;
}

bool QXmppCallPrivate::handleTransport(QXmppCallStream *stream, const QXmppJingleIq::Content &content)
{
    if (stream->d->useDtls && !content.transportFingerprint().isEmpty()) {
        if (content.transportFingerprintHash() != u"sha-256") {
            q->warning(u"Unsupported hashing algorithm for DTLS fingerprint: %1."_s.arg(content.transportFingerprintHash()));
            return false;
        }
        stream->d->expectedPeerCertificateDigest = content.transportFingerprint();

        // active/passive part negotiation
        const auto setup = content.transportFingerprintSetup();
        if (setup == u"actpass") {
            stream->d->dtlsPeerSetup = Actpass;
        } else if (setup == u"active") {
            stream->d->dtlsPeerSetup = Active;
        } else if (setup == u"passive") {
            stream->d->dtlsPeerSetup = Passive;
        } else {
            // invalid setup attribute
            return false;
        }

        q->debug(u"Decided to be DTLS %1"_s.arg(stream->d->isDtlsClient() ? u"client (active)" : u"server (passive)"));
        if (stream->d->isDtlsClient()) {
            stream->d->enableDtlsClientMode();
        }
    }

    stream->d->connection->setRemoteUser(content.transportUser());
    stream->d->connection->setRemotePassword(content.transportPassword());
    const auto candidates = content.transportCandidates();
    for (const auto &candidate : candidates) {
        stream->d->connection->addRemoteCandidate(candidate);
    }

    // perform ICE negotiation
    if (!content.transportCandidates().isEmpty()) {
        stream->d->connection->connectToHost();
    }
    return true;
}

std::variant<QXmppIq, QXmppStanza::Error> QXmppCallPrivate::handleRequest(QXmppJingleIq &&iq)
{
    using Error = QXmppStanza::Error;

    Q_ASSERT(manager);  // we are called only from the manager
    const auto content = iq.contents().isEmpty() ? QXmppJingleIq::Content() : iq.contents().constFirst();

    switch (iq.action()) {
    case QXmppJingleIq::SessionAccept: {
        if (direction == QXmppCall::IncomingDirection) {
            return Error { Error::Cancel, Error::BadRequest, u"'session-accept' for outgoing call"_s };
        }

        // check content description and transport
        auto stream = find(streams, content.name(), &QXmppCallStream::name);
        if (!stream ||
            !handleDescription(*stream, content) ||
            !handleTransport(*stream, content)) {

            // terminate call
            terminate({ QXmppJingleReason::FailedApplication, {}, {} }, true);
            return {};
        }

        // check for call establishment
        setState(QXmppCall::ActiveState);
        break;
    }
    case QXmppJingleIq::SessionInfo: {
        // notify user
        later(q, [this] {
            Q_EMIT q->ringing();
        });
        break;
    }
    case QXmppJingleIq::SessionTerminate: {
        // terminate
        q->info(u"Remote party %1 terminated call %2"_s.arg(iq.from(), iq.sid()));
        q->terminated();
        break;
    }
    case QXmppJingleIq::ContentAccept: {
        // check content description and transport
        auto stream = find(streams, content.name(), &QXmppCallStream::name);
        if (!stream ||
            !handleDescription(*stream, content) ||
            !handleTransport(*stream, content)) {

            // FIXME: what action?
            return {};
        }
        break;
    }
    case QXmppJingleIq::ContentAdd: {
        // check media stream does not exist yet
        if (contains(streams, content.name(), &QXmppCallStream::name)) {
            return Error { Error::Cancel, Error::Conflict, u"Media stream already exists."_s };
        }

        // create media stream
        auto *stream = createStream(content.descriptionMedia(), content.creator(), content.name());
        if (!stream) {
            // reject content
            later(this, [this, name = content.name()]() {
                QXmppJingleIq::Content content;
                content.setName(name);

                auto iq = createIq(QXmppJingleIq::ContentReject);
                iq.setContents({ std::move(content) });
                iq.setActionReason(QXmppJingleReason { QXmppJingleReason::FailedApplication, {}, {} });
                manager->client()->sendIq(std::move(iq));
            });
            return {};
        }

        // check content description
        if (!handleDescription(stream, content) ||
            !handleTransport(stream, content)) {

            // reject content
            later(this, [this, name = content.name()]() {
                QXmppJingleIq::Content content;
                content.setName(name);

                auto iq = createIq(QXmppJingleIq::ContentReject);
                iq.setContents({ std::move(content) });
                iq.setActionReason(QXmppJingleReason { QXmppJingleReason::FailedApplication, {}, {} });
                manager->client()->sendIq(std::move(iq));
            });

            streams.removeAll(stream);
            delete stream;
            return {};
        }

        // accept content
        later(this, [this, stream] {
            Q_ASSERT(manager);
            auto iq = createIq(QXmppJingleIq::ContentAccept);
            iq.addContent(localContent(stream));
            manager->client()->sendIq(std::move(iq));
        });
        break;
    }
    case QXmppJingleIq::TransportInfo: {
        // check content transport
        auto stream = find(streams, content.name(), &QXmppCallStream::name);
        if (!stream ||
            !handleTransport(*stream, content)) {
            // FIXME: what action?
            return {};
        }
        break;
    }
    default:
        return Error { Error::Cancel, Error::UnexpectedRequest, u"Unexpected jingle action."_s };
    }

    // send acknowledgement
    return {};
}

QXmppCallStream *QXmppCallPrivate::createStream(const QString &media, const QString &creator, const QString &name)
{
    Q_ASSERT(manager);

    if (media != AUDIO_MEDIA && media != VIDEO_MEDIA) {
        q->warning(u"Unsupported media type %1"_s.arg(media));
        return nullptr;
    }

    if (!isFormatSupported(u"rtpbin"_s)) {
        q->warning(u"The rtpbin GStreamer plugin is missing. Calls are not possible."_s);
        return nullptr;
    }

    auto *stream = new QXmppCallStream(pipeline, rtpBin, media, creator, name, ++nextId, useDtls, q);

    // Fill local payload payload types
    stream->d->payloadTypes = transform<QList<QXmppJinglePayloadType>>(media == AUDIO_MEDIA ? audioCodecs : videoCodecs, [](const auto &codec) {
        QXmppJinglePayloadType payloadType;
        payloadType.setId(codec.pt);
        payloadType.setName(codec.name);
        payloadType.setChannels(codec.channels);
        payloadType.setClockrate(codec.clockrate);
        return payloadType;
    });

    // ICE connection
    stream->d->connection->setIceControlling(direction == QXmppCall::OutgoingDirection);
    stream->d->connection->setStunServers(manager->d->stunServers);
    stream->d->connection->setTurnServer(manager->d->turnHost, manager->d->turnPort);
    stream->d->connection->setTurnUser(manager->d->turnUser);
    stream->d->connection->setTurnPassword(manager->d->turnPassword);
    stream->d->connection->bind(QXmppIceComponent::discoverAddresses());

    // connect signals
    QObject::connect(stream->d->connection, &QXmppIceConnection::localCandidatesChanged,
                     q, [this, stream]() { q->onLocalCandidatesChanged(stream); });

    QObject::connect(stream->d->connection, &QXmppIceConnection::disconnected,
                     q, &QXmppCall::hangup);

    connect(stream->d, &QXmppCallStreamPrivate::peerCertificateReceived, this, [this, stream](bool fingerprintMatches) {
        if (!fingerprintMatches) {
            Q_ASSERT(manager);
            auto reason = QXmppJingleReason { QXmppJingleReason::SecurityError, u"DTLS certificate fingerprint mismatch"_s, {} };

            if (streams.size() > 1 && isOwn(stream)) {
                q->warning(u"DTLS handshake returned unexpected certificate fingerprint."_s);
                auto iq = createIq(QXmppJingleIq::ContentRemove);
                iq.setContents({ localContent(stream) });
                iq.setActionReason(reason);
                manager->client()->sendIq(std::move(iq));

                streams.removeAll(stream);
                stream->deleteLater();
            } else {
                q->warning(u"DTLS handshake returned unexpected certificate fingerprint. Terminating call."_s);
                terminate(std::move(reason));
            }
        } else {
            q->debug(u"DTLS handshake returned certificate with expected fingerprint."_s);
        }
    });

    streams << stream;
    Q_EMIT q->streamCreated(stream);

    return stream;
}

QXmppJingleIq::Content QXmppCallPrivate::localContent(QXmppCallStream *stream) const
{
    QXmppJingleIq::Content content;
    content.setCreator(stream->creator());
    content.setName(stream->name());
    content.setSenders(u"both"_s);

    // description
    content.setDescriptionMedia(stream->media());
    content.setDescriptionSsrc(stream->d->localSsrc);
    content.setPayloadTypes(stream->d->payloadTypes);

    // transport
    content.setTransportUser(stream->d->connection->localUser());
    content.setTransportPassword(stream->d->connection->localPassword());
    content.setTransportCandidates(stream->d->connection->localCandidates());

    // encryption
    if (useDtls) {
        Q_ASSERT(!stream->d->ownCertificateDigest.isEmpty());
        content.setTransportFingerprint(stream->d->ownCertificateDigest);
        content.setTransportFingerprintHash(u"sha-256"_s);

        // choose whether we are DTLS client or server
        if (stream->d->dtlsPeerSetup.has_value()) {
            content.setTransportFingerprintSetup(stream->d->isDtlsClient() ? u"active"_s : u"passive"_s);
        } else {
            // let other end decide
            content.setTransportFingerprintSetup(u"actpass"_s);
        }
    }

    return content;
}

QXmppJingleIq QXmppCallPrivate::createIq(QXmppJingleIq::Action action) const
{
    Q_ASSERT(manager);

    QXmppJingleIq iq;
    iq.setFrom(manager->client()->configuration().jid());
    iq.setTo(jid);
    iq.setType(QXmppIq::Set);
    iq.setAction(action);
    iq.setSid(sid);
    return iq;
}

void QXmppCallPrivate::sendInvite()
{
    Q_ASSERT(manager);

    // create audio stream
    auto *stream = find(streams, AUDIO_MEDIA, &QXmppCallStream::media).value();

    auto iq = createIq(QXmppJingleIq::SessionInitiate);
    iq.setInitiator(manager->client()->configuration().jid());
    iq.addContent(localContent(stream));
    manager->client()->send(std::move(iq));
}

void QXmppCallPrivate::setState(QXmppCall::State newState)
{
    if (state != newState) {
        state = newState;
        Q_EMIT q->stateChanged(state);

        if (state == QXmppCall::ActiveState) {
            Q_EMIT q->connected();
        } else if (state == QXmppCall::FinishedState) {
            Q_EMIT q->finished();
        }
    }
}

///
/// Request graceful call termination
///
void QXmppCallPrivate::terminate(QXmppJingleReason reason, bool delay)
{
    if (state == QXmppCall::DisconnectingState ||
        state == QXmppCall::FinishedState) {
        return;
    }

    // hangup call
    auto iq = createIq(QXmppJingleIq::SessionTerminate);
    iq.setActionReason(std::move(reason));

    setState(QXmppCall::DisconnectingState);

    if (delay) {
        later(this, [this, iq = std::move(iq)]() mutable {
            Q_ASSERT(manager);
            manager->client()->sendIq(std::move(iq)).then(q, [this](auto result) {
                // terminate on both success or error
                q->terminated();
            });
        });
    } else {
        manager->client()->sendIq(std::move(iq)).then(q, [this](auto result) {
            // terminate on both success or error
            q->terminated();
        });
    }

    // schedule forceful termination in 5s
    QTimer::singleShot(5s, q, &QXmppCall::terminated);
}

bool QXmppCallPrivate::isOwn(QXmppCallStream *stream) const
{
    bool outgoingCall = direction == QXmppCall::OutgoingDirection;
    bool initiatorsStream = stream->d->creator == u"initiator";

    return outgoingCall && initiatorsStream || !outgoingCall && !initiatorsStream;
}

///
/// \class QXmppCall
///
/// The QXmppCall class represents a Voice-Over-IP call to a remote party.
///
/// \note THIS API IS NOT FINALIZED YET
///

QXmppCall::QXmppCall(const QString &jid, QXmppCall::Direction direction, QXmppCallManager *manager)
    : QXmppLoggable(nullptr),
      d(std::make_unique<QXmppCallPrivate>(jid, direction, manager, this))
{
}

QXmppCall::~QXmppCall() = default;

///
/// Call this method if you wish to accept an incoming call.
///
void QXmppCall::accept()
{
    if (d->direction == IncomingDirection && d->state == ConnectingState) {
        Q_ASSERT(d->manager);
        Q_ASSERT(d->streams.size() == 1);
        QXmppCallStream *stream = d->streams.first();

        // accept incoming call
        auto iq = d->createIq(QXmppJingleIq::SessionAccept);
        iq.setResponder(d->manager->client()->configuration().jid());
        iq.addContent(d->localContent(stream));
        d->manager->client()->sendIq(std::move(iq));

        // check for call establishment
        d->setState(QXmppCall::ActiveState);
    }
}

///
/// Returns the GStreamer pipeline.
///
/// \since QXmpp 1.3
///
GstElement *QXmppCall::pipeline() const
{
    return d->pipeline;
}

///
/// Returns the RTP stream for the audio data.
///
/// \since QXmpp 1.3
///
QXmppCallStream *QXmppCall::audioStream() const
{
    return find(d->streams, AUDIO_MEDIA, &QXmppCallStream::media).value_or(nullptr);
}

///
/// Returns the RTP stream for the video data.
///
/// \since QXmpp 1.3
///
QXmppCallStream *QXmppCall::videoStream() const
{
    return find(d->streams, VIDEO_MEDIA, &QXmppCallStream::media).value_or(nullptr);
}

void QXmppCall::terminated()
{
    // close streams
    for (auto stream : std::as_const(d->streams)) {
        stream->d->connection->close();
    }

    // update state
    d->setState(QXmppCall::FinishedState);
}

///
/// Returns the call's direction.
///
QXmppCall::Direction QXmppCall::direction() const
{
    return d->direction;
}

///
/// Hangs up the call.
///
void QXmppCall::hangup()
{
    d->terminate({ QXmppJingleReason::None, {}, {} });
}

///
/// Sends a transport-info to inform the remote party of new local candidates.
///
void QXmppCall::onLocalCandidatesChanged(QXmppCallStream *stream)
{
    auto iq = d->createIq(QXmppJingleIq::TransportInfo);
    iq.addContent(d->localContent(stream));
    d->manager->client()->sendIq(std::move(iq));
}

///
/// Returns the remote party's JID.
///
QString QXmppCall::jid() const
{
    return d->jid;
}

///
/// Returns the call's session identifier.
///
QString QXmppCall::sid() const
{
    return d->sid;
}

///
/// Returns the call's state.
///
/// \sa stateChanged()
///
QXmppCall::State QXmppCall::state() const
{
    return d->state;
}

///
/// Starts sending video to the remote party.
///
void QXmppCall::addVideo()
{
    if (d->state != QXmppCall::ActiveState) {
        warning(u"Cannot add video, call is not active"_s);
        return;
    }

    if (contains(d->streams, VIDEO_MEDIA, &QXmppCallStream::media)) {
        return;
    }

    // create video stream
    QString creator = (d->direction == QXmppCall::OutgoingDirection) ? u"initiator"_s : u"responder"_s;
    auto *stream = d->createStream(VIDEO_MEDIA.toString(), creator, u"webcam"_s);

    // build request
    auto iq = d->createIq(QXmppJingleIq::ContentAdd);
    iq.addContent(d->localContent(stream));
    d->manager->client()->sendIq(std::move(iq));
}

///
/// Returns if the call is encrypted
///
/// \since QXmpp 1.11
///
bool QXmppCall::isEncrypted() const
{
    return d->useDtls;
}

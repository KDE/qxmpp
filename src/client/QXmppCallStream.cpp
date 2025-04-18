// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppCallStream.h"

#include "QXmppCallStream_p.h"
#include "QXmppCall_p.h"
#include "QXmppStun.h"

#include "GstWrapper.h"
#include "StringLiterals.h"

#include <cstring>
#include <gst/gst.h>

#include <QRandomGenerator>
#include <QSslCertificate>
#include <QUuid>

using namespace QXmpp::Private;

QXmppCallStreamPrivate::QXmppCallStreamPrivate(QXmppCallStream *parent, GstElement *pipeline_,
                                               GstElement *rtpBin_, QString media_, QString creator_,
                                               QString name_, int id_, bool useDtls_)
    : QObject(parent),
      q(parent),
      pipeline(pipeline_),
      rtpBin(rtpBin_),
      media(std::move(media_)),
      creator(std::move(creator_)),
      name(std::move(name_)),
      id(id_),
      useDtls(useDtls_)
{
    localSsrc = QRandomGenerator::global()->generate();

    iceReceiveBin = gst_bin_new(u"receive_%1"_s.arg(id).toLatin1().data());
    iceSendBin = gst_bin_new(u"send_%1"_s.arg(id).toLatin1().data());
    gst_bin_add_many(GST_BIN(pipeline), iceReceiveBin, iceSendBin, nullptr);

    GstPad *internalRtpPad = gst_ghost_pad_new_no_target(nullptr, GST_PAD_SINK);
    GstPad *internalRtcpPad = gst_ghost_pad_new_no_target(nullptr, GST_PAD_SINK);
    if (!gst_element_add_pad(iceSendBin, internalRtpPad) ||
        !gst_element_add_pad(iceSendBin, internalRtcpPad)) {
        qFatal("Failed to add pads to send bin");
    }

    /* Create DTLS SRTP elements */
    if (useDtls) {
        QString dtlsRtpId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString dtlsRtcpId = QUuid::createUuid().toString(QUuid::WithoutBraces);

        dtlsSrtpDecoder = gst_element_factory_make("dtlssrtpdec", nullptr);
        dtlsSrtcpDecoder = gst_element_factory_make("dtlssrtpdec", nullptr);
        if (!dtlsSrtpDecoder || !dtlsSrtcpDecoder) {
            qFatal("Failed to create dtls srtp decoders");
        }

        g_object_set(dtlsSrtpDecoder, "async-handling", true, "connection-id", dtlsRtpId.toLatin1().data(), nullptr);
        g_object_set(dtlsSrtcpDecoder, "async-handling", true, "connection-id", dtlsRtcpId.toLatin1().data(), nullptr);

        /* Copy the certificate to the RTCP decoder so that they both share the same fingerprint. */
        GCharPtr pem;
        pem = getCharProperty(dtlsSrtpDecoder, "pem"_L1);
        g_object_set(dtlsSrtcpDecoder, "pem", pem.get(), nullptr);  // TODO why does this fail?

        /* Calculate the fingerprint to transmit to the remote party. */
        QSslCertificate certificate(pem.get());
        digest = certificate.digest(QCryptographicHash::Sha256);

        // Setup encoders
        dtlsSrtpEncoder = gst_element_factory_make("dtlssrtpenc", nullptr);
        dtlsSrtcpEncoder = gst_element_factory_make("dtlssrtpenc", nullptr);
        if (!dtlsSrtpEncoder || !dtlsSrtcpEncoder) {
            qFatal("Failed to create dtls srtp encoders");
        }

        g_object_set(dtlsSrtpEncoder, "async-handling", true, "connection-id", dtlsRtpId.toLatin1().data(), "is-client", false, nullptr);
        g_object_set(dtlsSrtcpEncoder, "async-handling", true, "connection-id", dtlsRtcpId.toLatin1().data(), "is-client", false, nullptr);

        g_signal_connect_swapped(dtlsSrtpEncoder, "on-key-set",
                                 G_CALLBACK(+[](QXmppCallStreamPrivate *p) {
                                     // TODO check remote fingerprint (peer-pem on decoders)
                                     qWarning("================ ON_KEY_SET ==============");
                                     p->dtlsHandshakeComplete = true;
                                     if (p->sendPadCB && p->encoderBin) {
                                         p->sendPadCB(p->sendPad);
                                     }
                                     if (p->receivePadCB && p->decoderBin) {
                                         p->receivePadCB(p->receivePad);
                                     }
                                 }),
                                 this);

        if (!gst_bin_add(GST_BIN(iceReceiveBin), dtlsSrtpDecoder) ||
            !gst_bin_add(GST_BIN(iceReceiveBin), dtlsSrtcpDecoder) ||
            !gst_bin_add(GST_BIN(iceSendBin), dtlsSrtpEncoder) ||
            !gst_bin_add(GST_BIN(iceSendBin), dtlsSrtcpEncoder)) {
            qFatal("Failed to add dtls elements to corresponding bins");
        }
    }

    /* Create appsrc / appsink elements */
    connection = new QXmppIceConnection(this);
    connection->addComponent(RTP_COMPONENT);
    connection->addComponent(RTCP_COMPONENT);
    appRtpSink = gst_element_factory_make("appsink", nullptr);
    appRtcpSink = gst_element_factory_make("appsink", nullptr);
    if (!appRtpSink || !appRtcpSink) {
        qFatal("Failed to create appsinks");
    }

    g_signal_connect_swapped(appRtpSink, "new-sample",
                             G_CALLBACK(+[](QXmppCallStreamPrivate *p, GstElement *appsink) -> GstFlowReturn {
                                 return p->sendDatagram(appsink, RTP_COMPONENT);
                             }),
                             this);
    g_signal_connect_swapped(appRtcpSink, "new-sample",
                             G_CALLBACK(+[](QXmppCallStreamPrivate *p, GstElement *appsink) -> GstFlowReturn {
                                 return p->sendDatagram(appsink, RTCP_COMPONENT);
                             }),
                             this);

    appRtpSrc = gst_element_factory_make("appsrc", nullptr);
    appRtcpSrc = gst_element_factory_make("appsrc", nullptr);
    if (!appRtpSrc || !appRtcpSrc) {
        qFatal("Failed to create appsrcs");
    }

    // TODO check these parameters
    g_object_set(appRtpSink, "emit-signals", true, "async", false, "max-buffers", 1, "drop", true, nullptr);
    g_object_set(appRtcpSink, "emit-signals", true, "async", false, nullptr);
    g_object_set(appRtpSrc, "is-live", true, "max-latency", 5000000, nullptr);
    g_object_set(appRtcpSrc, "is-live", true, nullptr);

    connect(connection->component(RTP_COMPONENT), &QXmppIceComponent::datagramReceived,
            q, [&](const QByteArray &datagram) { datagramReceived(datagram, appRtpSrc); });
    connect(connection->component(RTCP_COMPONENT), &QXmppIceComponent::datagramReceived,
            q, [&](const QByteArray &datagram) { datagramReceived(datagram, appRtcpSrc); });

    if (!gst_bin_add(GST_BIN(iceReceiveBin), appRtpSrc) ||
        !gst_bin_add(GST_BIN(iceReceiveBin), appRtcpSrc) ||
        !gst_bin_add(GST_BIN(iceSendBin), appRtpSink) ||
        !gst_bin_add(GST_BIN(iceSendBin), appRtcpSink)) {
        qFatal("Failed to add appsrc / appsink elements to respective bins");
    }

    /* Trigger creation of necessary pads */
    GstPadPtr dummyPad = gst_element_request_pad_simple(rtpBin, u"send_rtp_sink_%1"_s.arg(id).toLatin1().data());
    dummyPad.reset();

    /* Link pads - receiving side */
    GstPadPtr rtpRecvPad = gst_element_get_static_pad(appRtpSrc, "src");
    GstPadPtr rtcpRecvPad = gst_element_get_static_pad(appRtcpSrc, "src");

    if (useDtls) {
        GstPadPtr dtlsRtpSinkPad = gst_element_get_static_pad(dtlsSrtpDecoder, "sink");
        GstPadPtr dtlsRtcpSinkPad = gst_element_get_static_pad(dtlsSrtcpDecoder, "sink");
        gst_pad_link(rtpRecvPad, dtlsRtpSinkPad);
        gst_pad_link(rtcpRecvPad, dtlsRtcpSinkPad);
        rtpRecvPad = gst_element_get_static_pad(dtlsSrtpDecoder, "rtp_src");
        rtcpRecvPad = gst_element_get_static_pad(dtlsSrtcpDecoder, "rtcp_src");
    }

    GstPadPtr rtpSinkPad = gst_element_request_pad_simple(rtpBin, u"recv_rtp_sink_%1"_s.arg(id).toLatin1().data());
    GstPadPtr rtcpSinkPad = gst_element_request_pad_simple(rtpBin, u"recv_rtcp_sink_%1"_s.arg(id).toLatin1().data());
    gst_pad_link(rtpRecvPad, rtpSinkPad);
    gst_pad_link(rtcpRecvPad, rtcpSinkPad);

    /* Link pads - sending side */
    GstPadPtr rtpSendPad = gst_element_get_static_pad(appRtpSink, "sink");
    GstPadPtr rtcpSendPad = gst_element_get_static_pad(appRtcpSink, "sink");

    if (useDtls) {
        GstPadPtr dtlsRtpSrcPad = gst_element_get_static_pad(dtlsSrtpEncoder, "src");
        GstPadPtr dtlsRtcpSrcPad = gst_element_get_static_pad(dtlsSrtcpEncoder, "src");
        gst_pad_link(dtlsRtpSrcPad, rtpSendPad);
        gst_pad_link(dtlsRtcpSrcPad, rtcpSendPad);
        rtpSendPad = gst_element_request_pad_simple(dtlsSrtpEncoder, u"rtp_sink_%1"_s.arg(id).toLatin1().data());
        rtcpSendPad = gst_element_request_pad_simple(dtlsSrtcpEncoder, u"rtcp_sink_%1"_s.arg(id).toLatin1().data());
    }

    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(internalRtpPad), rtpSendPad) ||
        !gst_ghost_pad_set_target(GST_GHOST_PAD(internalRtcpPad), rtcpSendPad)) {
        qFatal("Failed to link rtp send pads to internal ghost pads");
    }

    // We need frequent RTCP reports for the bandwidth controller
    GstElement *rtpSession;
    g_signal_emit_by_name(rtpBin, "get-session", static_cast<uint>(id), &rtpSession);
    g_object_set(rtpSession, "rtcp-min-interval", 100'000'000, nullptr);

    gst_element_sync_state_with_parent(iceReceiveBin);
    gst_element_sync_state_with_parent(iceSendBin);

    GstPadPtr rtpbinRtpSendPad = gst_element_get_static_pad(rtpBin, u"send_rtp_src_%1"_s.arg(id).toLatin1().data());
    GstPadPtr rtpbinRtcpSendPad = gst_element_request_pad_simple(rtpBin, u"send_rtcp_src_%1"_s.arg(id).toLatin1().data());
    if (gst_pad_link(rtpbinRtpSendPad, internalRtpPad) != GST_PAD_LINK_OK ||
        gst_pad_link(rtpbinRtcpSendPad, internalRtcpPad) != GST_PAD_LINK_OK) {
        qFatal("Failed to link rtp pads");
    }
}

QXmppCallStreamPrivate::~QXmppCallStreamPrivate()
{
    connection->close();

    // Remove elements from pipeline
    if ((encoderBin && !gst_bin_remove(GST_BIN(pipeline), encoderBin)) ||
        (decoderBin && !gst_bin_remove(GST_BIN(pipeline), decoderBin)) ||
        !gst_bin_remove(GST_BIN(pipeline), iceSendBin) ||
        !gst_bin_remove(GST_BIN(pipeline), iceReceiveBin)) {
        qFatal("Failed to remove bins from pipeline");
    }
}

GstFlowReturn QXmppCallStreamPrivate::sendDatagram(GstElement *appsink, int component)
{
    GstSamplePtr sample;
    g_signal_emit_by_name(appsink, "pull-sample", sample.reassignRef());
    if (!sample) {
        qFatal("Could not get sample");
        return GST_FLOW_ERROR;
    }

    GstMapInfo mapInfo;
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        qFatal("Could not get buffer");
        return GST_FLOW_ERROR;
    }
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        qFatal("Could not map buffer");
        return GST_FLOW_ERROR;
    }
    QByteArray datagram;
    datagram.resize(mapInfo.size);
    std::memcpy(datagram.data(), mapInfo.data, mapInfo.size);
    gst_buffer_unmap(buffer, &mapInfo);

    if (connection->component(component)->isConnected() &&
        connection->component(component)->sendDatagram(datagram) != datagram.size()) {
        return GST_FLOW_ERROR;
    }
    return GST_FLOW_OK;
}

void QXmppCallStreamPrivate::datagramReceived(const QByteArray &datagram, GstElement *appsrc)
{
    GstBufferPtr buffer = gst_buffer_new_and_alloc(datagram.size());
    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_WRITE)) {
        qFatal("Could not map buffer");
        return;
    }
    std::memcpy(mapInfo.data, datagram.data(), mapInfo.size);
    gst_buffer_unmap(buffer, &mapInfo);
    GstFlowReturn ret;
    g_signal_emit_by_name(appsrc, "push-buffer", buffer.get(), &ret);
}

void QXmppCallStreamPrivate::addEncoder(QXmppCallPrivate::GstCodec &codec)
{
    // Remove old encoder and payloader if they exist
    if (encoderBin) {
        if (!gst_bin_remove(GST_BIN(pipeline), encoderBin)) {
            qFatal("Failed to remove existing encoder bin");
        }
    }
    encoderBin = gst_bin_new(u"encoder_%1"_s.arg(id).toLatin1().data());
    if (!gst_bin_add(GST_BIN(pipeline), encoderBin)) {
        qFatal("Failed to add encoder bin to wrapper");
        return;
    }

    sendPad = gst_ghost_pad_new_no_target(nullptr, GST_PAD_SINK);
    gst_element_add_pad(encoderBin, sendPad);

    // Create new elements
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    if (!queue) {
        qFatal("Failed to create queue");
        return;
    }

    GstElement *pay = gst_element_factory_make(codec.gstPay.toLatin1().data(), nullptr);
    if (!pay) {
        qFatal("Failed to create payloader");
        return;
    }
    g_object_set(pay, "pt", codec.pt, "ssrc", localSsrc, nullptr);

    GstElement *encoder = gst_element_factory_make(codec.gstEnc.toLatin1().data(), nullptr);
    if (!encoder) {
        qFatal("Failed to create encoder");
        return;
    }
    for (auto &encProp : std::as_const(codec.encProps)) {
        g_object_set(encoder, encProp.name.toLatin1().data(), encProp.value, nullptr);
    }

    gst_bin_add_many(GST_BIN(encoderBin), queue, encoder, pay, nullptr);

    if (!gst_element_link_pads(pay, "src", rtpBin, u"send_rtp_sink_%1"_s.arg(id).toLatin1().data()) ||
        !gst_element_link_many(queue, encoder, pay, nullptr)) {
        qFatal("Could not link all encoder pads");
        return;
    }

    GstPadPtr queueSinkPad = gst_element_get_static_pad(queue, "sink");
    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(sendPad), queueSinkPad)) {
        qFatal("Failed to set send pad");
        return;
    }

    if (sendPadCB && (dtlsHandshakeComplete || !useDtls)) {
        sendPadCB(sendPad);
    }

    gst_element_sync_state_with_parent(encoderBin);
}

void QXmppCallStreamPrivate::addDecoder(GstPad *pad, QXmppCallPrivate::GstCodec &codec)
{
    // Remove old decoder and depayloader if they exist
    if (decoderBin) {
        if (!gst_bin_remove(GST_BIN(pipeline), decoderBin)) {
            qFatal("Failed to remove existing decoder bin");
        }
    }
    decoderBin = gst_bin_new(u"decoder_%1"_s.arg(id).toLatin1().data());
    if (!gst_bin_add(GST_BIN(pipeline), decoderBin)) {
        qFatal("Failed to add decoder bin to wrapper");
        return;
    }

    receivePad = gst_ghost_pad_new_no_target(nullptr, GST_PAD_SRC);
    internalReceivePad = gst_ghost_pad_new_no_target(nullptr, GST_PAD_SINK);
    gst_element_add_pad(decoderBin, receivePad);
    gst_element_add_pad(decoderBin, internalReceivePad);

    // Create new elements
    GstElement *depay = gst_element_factory_make(codec.gstDepay.toLatin1().data(), nullptr);
    if (!depay) {
        qFatal("Failed to create depayloader");
        return;
    }

    GstElement *decoder = gst_element_factory_make(codec.gstDec.toLatin1().data(), nullptr);
    if (!decoder) {
        qFatal("Failed to create decoder");
        return;
    }

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    if (!queue) {
        qFatal("Failed to create queue");
        return;
    }

    gst_bin_add_many(GST_BIN(decoderBin), depay, decoder, queue, nullptr);

    GstPadPtr depaySinkPad = gst_element_get_static_pad(depay, "sink");
    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(internalReceivePad), depaySinkPad)) {
        qFatal("Failed to set receive pad");
    }

    GstPadPtr queueSrcPad = gst_element_get_static_pad(queue, "src");
    if (!gst_ghost_pad_set_target(GST_GHOST_PAD(receivePad), queueSrcPad)) {
        qFatal("Failed to set receive pad");
    }

    if (gst_pad_link(pad, internalReceivePad) != GST_PAD_LINK_OK ||
        !gst_element_link_many(depay, decoder, queue, nullptr)) {
        qFatal("Could not link all decoder pads");
        return;
    }

    gst_element_sync_state_with_parent(decoderBin);

    if (receivePadCB && (dtlsHandshakeComplete || !useDtls)) {
        receivePadCB(receivePad);
    }
}

void QXmppCallStreamPrivate::enableDtlsClientMode()
{
    gst_element_set_state(dtlsSrtpEncoder, GST_STATE_READY);
    gst_element_set_state(dtlsSrtcpEncoder, GST_STATE_READY);
    g_object_set(dtlsSrtpEncoder, "is-client", true, nullptr);
    g_object_set(dtlsSrtcpEncoder, "is-client", true, nullptr);
    gst_element_set_state(dtlsSrtpEncoder, GST_STATE_PLAYING);
    gst_element_set_state(dtlsSrtcpEncoder, GST_STATE_PLAYING);
}

///
/// \class QXmppCallStream
///
/// The QXmppCallStream class represents an RTP stream in a VoIP call.
///
/// \note THIS API IS NOT FINALIZED YET
///
/// \since QXmpp 1.3
///

QXmppCallStream::QXmppCallStream(GstElement *pipeline, GstElement *rtpbin, QString media, QString creator, QString name, int id, bool useDtls, QObject *parent)
    : QXmppLoggable(parent),
      d(new QXmppCallStreamPrivate(this, pipeline, rtpbin, std::move(media), std::move(creator), std::move(name), id, useDtls))
{
}

///
/// Returns the JID of the creator of the call stream.
///
QString QXmppCallStream::creator() const
{
    return d->creator;
}

///
/// Returns the media type of the stream, "audio" or "video".
///
QString QXmppCallStream::media() const
{
    return d->media;
}

///
/// Returns the name of the stream (e.g. "webcam" or "voice").
///
/// There is no defined format and there are no predefined values for this.
///
QString QXmppCallStream::name() const
{
    return d->name;
}

///
/// Returns the local ID of the stream.
///
int QXmppCallStream::id() const
{
    return d->id;
}

///
/// Sets a gstreamer receive pad callback.
///
/// Can be used to process or display the received data.
///
void QXmppCallStream::setReceivePadCallback(std::function<void(GstPad *)> cb)
{
    d->receivePadCB = std::move(cb);
    if (d->receivePad) {
        d->receivePadCB(d->receivePad);
    }
}

///
/// Sets a gstreamer send pad callback.
///
/// Can be used to send the stream input.
///
void QXmppCallStream::setSendPadCallback(std::function<void(GstPad *)> cb)
{
    d->sendPadCB = std::move(cb);
    if (d->sendPad) {
        d->sendPadCB(d->sendPad);
    }
}

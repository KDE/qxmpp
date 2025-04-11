// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALLSTREAM_P_H
#define QXMPPCALLSTREAM_P_H

#include "QXmppCall_p.h"
#include "QXmppJingleIq.h"

#include <gst/gst.h>

#include <QList>
#include <QObject>
#include <QString>

class QXmppIceConnection;

//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

static const int RTP_COMPONENT = 1;
static const int RTCP_COMPONENT = 2;

constexpr QStringView AUDIO_MEDIA = u"audio";
constexpr QStringView VIDEO_MEDIA = u"video";

class QXmppCallStreamPrivate : public QObject
{
    Q_OBJECT

public:
    QXmppCallStreamPrivate(QXmppCallStream *parent, GstElement *pipeline_, GstElement *rtpBin_,
                           QString media_, QString creator_, QString name_, int id_, bool useDtls_);
    ~QXmppCallStreamPrivate();

    GstFlowReturn sendDatagram(GstElement *appsink, int component);
    void datagramReceived(const QByteArray &datagram, GstElement *appsrc);

    void addEncoder(QXmppCallPrivate::GstCodec &codec);
    void addDecoder(GstPad *pad, QXmppCallPrivate::GstCodec &codec);
    void enableDtlsClientMode();

    QXmppCallStream *q;

    quint32 localSsrc;

    GstElement *pipeline = nullptr;
    GstElement *rtpBin = nullptr;
    GstPad *sendPad = nullptr;
    GstPad *receivePad = nullptr;
    GstPad *internalReceivePad = nullptr;
    GstElement *encoderBin = nullptr;
    GstElement *decoderBin = nullptr;
    GstElement *iceReceiveBin = nullptr;
    GstElement *iceSendBin = nullptr;
    GstElement *appRtpSrc = nullptr;
    GstElement *appRtcpSrc = nullptr;
    GstElement *appRtpSink = nullptr;
    GstElement *appRtcpSink = nullptr;
    GstElement *dtlsSrtpEncoder = nullptr;
    GstElement *dtlsSrtcpEncoder = nullptr;
    GstElement *dtlsSrtpDecoder = nullptr;
    GstElement *dtlsSrtcpDecoder = nullptr;
    QByteArray digest;

    std::function<void(GstPad *)> sendPadCB;
    std::function<void(GstPad *)> receivePadCB;

    QXmppIceConnection *connection;
    QString media;
    QString creator;
    QString name;
    int id;
    bool useDtls;
    bool dtlsHandshakeComplete = false;

    QList<QXmppJinglePayloadType> payloadTypes;
};

#endif

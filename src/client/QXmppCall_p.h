// SPDX-FileCopyrightText: 2019 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALL_P_H
#define QXMPPCALL_P_H

#include "QXmppCall.h"
#include "QXmppJingleIq.h"

#include "GstWrapper.h"
#include "StringLiterals.h"

#include <gst/gst.h>

#include <QList>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

class QXmppCallStream;

class QXmppCallPrivate : public QObject
{
    Q_OBJECT
public:
    using GstElementPtr = QXmpp::Private::GstElementPtr;

    struct GstCodec {
        int pt;
        QString name;
        int channels;
        uint clockrate;
        QLatin1String gstPay;
        QLatin1String gstDepay;
        QLatin1String gstEnc;
        QLatin1String gstDec;
        struct Property {
            QLatin1String name;
            int value;
        };
        // Use e.g. gst-inspect-1.0 x264enc to find good encoder settings for live streaming
        QList<Property> encProps;
    };

    explicit QXmppCallPrivate(const QString &jid, QXmppCall::Direction direction, QXmppCallManager *manager, QXmppCall *qq);
    ~QXmppCallPrivate();

    void ssrcActive(uint sessionId, uint ssrc);
    void padAdded(GstPad *pad);
    GstCaps *ptMap(uint sessionId, uint pt);
    static bool isFormatSupported(const QString &codecName);
    static bool isCodecSupported(const GstCodec &codec);

    QXmppCallStream *createStream(const QString &media, const QString &creator, const QString &name);
    QXmppJingleIq::Content localContent(QXmppCallStream *stream) const;
    QXmppJingleIq createIq(QXmppJingleIq::Action action) const;

    bool handleDescription(QXmppCallStream *stream, const QXmppJingleIq::Content &content);
    std::variant<QXmppIq, QXmppStanza::Error> handleRequest(QXmppJingleIq &&iq);
    bool handleTransport(QXmppCallStream *stream, const QXmppJingleIq::Content &content);
    void setState(QXmppCall::State state);
    void sendInvite();
    void terminate(QXmppJingleReason reason, bool delay = false);

    bool isOwn(QXmppCallStream *stream) const;

    QXmppCall::Direction direction;
    QString jid;
    bool useDtls = false;
    QXmppCallManager *manager;
    QString sid;
    QXmppCall::State state = QXmppCall::ConnectingState;

    GstElementPtr pipeline;
    GstElement *rtpBin;

    // Media streams
    QList<QXmppCallStream *> streams;
    int nextId = 0;

    // Supported codecs
    QList<GstCodec> videoCodecs = {
        GstCodec { .pt = 100, .name = u"H264"_s, .channels = 1, .clockrate = 90000, .gstPay = "rtph264pay"_L1, .gstDepay = "rtph264depay"_L1, .gstEnc = "x264enc"_L1, .gstDec = "avdec_h264"_L1, .encProps = { { "tune"_L1, 4 }, { "speed-preset"_L1, 3 }, { "byte-stream"_L1, true }, { "bitrate"_L1, 512 } } },
        GstCodec { .pt = 99, .name = u"VP8"_s, .channels = 1, .clockrate = 90000, .gstPay = "rtpvp8pay"_L1, .gstDepay = "rtpvp8depay"_L1, .gstEnc = "vp8enc"_L1, .gstDec = "vp8dec"_L1, .encProps = { { "deadline"_L1, 20000 }, { "target-bitrate"_L1, 512000 } } },
        // vp9enc and x265enc seem to be very slow. Give them a lower priority for now.
        GstCodec { .pt = 102, .name = u"H265"_s, .channels = 1, .clockrate = 90000, .gstPay = "rtph265pay"_L1, .gstDepay = "rtph265depay"_L1, .gstEnc = "x265enc"_L1, .gstDec = "avdec_h265"_L1, .encProps = { { "tune"_L1, 4 }, { "speed-preset"_L1, 3 }, { "bitrate"_L1, 512 } } },
        GstCodec { .pt = 101, .name = u"VP9"_s, .channels = 1, .clockrate = 90000, .gstPay = "rtpvp9pay"_L1, .gstDepay = "rtpvp9depay"_L1, .gstEnc = "vp9enc"_L1, .gstDec = "vp9dec"_L1, .encProps = { { "deadline"_L1, 20000 }, { "target-bitrate"_L1, 512000 } } }
    };

    QList<GstCodec> audioCodecs = {
        { .pt = 98, .name = u"OPUS"_s, .channels = 2, .clockrate = 48000, .gstPay = "rtpopuspay"_L1, .gstDepay = "rtpopusdepay"_L1, .gstEnc = "opusenc"_L1, .gstDec = "opusdec"_L1 },
        { .pt = 98, .name = u"OPUS"_s, .channels = 1, .clockrate = 48000, .gstPay = "rtpopuspay"_L1, .gstDepay = "rtpopusdepay"_L1, .gstEnc = "opusenc"_L1, .gstDec = "opusdec"_L1 },
        { .pt = 97, .name = u"SPEEX"_s, .channels = 1, .clockrate = 48000, .gstPay = "rtpspeexpay"_L1, .gstDepay = "rtpspeexdepay"_L1, .gstEnc = "speexenc"_L1, .gstDec = "speexdec"_L1 },
        { .pt = 97, .name = u"SPEEX"_s, .channels = 1, .clockrate = 44100, .gstPay = "rtpspeexpay"_L1, .gstDepay = "rtpspeexdepay"_L1, .gstEnc = "speexenc"_L1, .gstDec = "speexdec"_L1 },
        { .pt = 96, .name = u"AAC"_s, .channels = 2, .clockrate = 48000, .gstPay = "rtpmp4apay"_L1, .gstDepay = "rtpmp4adepay"_L1, .gstEnc = "avenc_aac"_L1, .gstDec = "avdec_aac"_L1 },
        { .pt = 96, .name = u"AAC"_s, .channels = 2, .clockrate = 44100, .gstPay = "rtpmp4apay"_L1, .gstDepay = "rtpmp4adepay"_L1, .gstEnc = "avenc_aac"_L1, .gstDec = "avdec_aac"_L1 },
        { .pt = 96, .name = u"AAC"_s, .channels = 1, .clockrate = 48000, .gstPay = "rtpmp4apay"_L1, .gstDepay = "rtpmp4adepay"_L1, .gstEnc = "avenc_aac"_L1, .gstDec = "avdec_aac"_L1 },
        { .pt = 96, .name = u"AAC"_s, .channels = 1, .clockrate = 44100, .gstPay = "rtpmp4apay"_L1, .gstDepay = "rtpmp4adepay"_L1, .gstEnc = "avenc_aac"_L1, .gstDec = "avdec_aac"_L1 },
        { .pt = 8, .name = u"PCMA"_s, .channels = 1, .clockrate = 8000, .gstPay = "rtppcmapay"_L1, .gstDepay = "rtppcmadepay"_L1, .gstEnc = "alawenc"_L1, .gstDec = "alawdec"_L1 },
        { .pt = 0, .name = u"PCMU"_s, .channels = 1, .clockrate = 8000, .gstPay = "rtppcmupay"_L1, .gstDepay = "rtppcmudepay"_L1, .gstEnc = "mulawenc"_L1, .gstDec = "mulawdec"_L1 },
    };

private:
    QXmppCall *q;
};

#endif

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <QXmppCall.h>
#include <QXmppCallManager.h>
#include <QXmppClient.h>
#include <QXmppRosterManager.h>
#include <QXmppStunServer.h>

#include <chrono>
#include <gst/gst.h>

#include <QCoreApplication>
#include <QTimer>
#ifdef Q_OS_UNIX
#include <csignal>
#endif

using namespace QXmpp;
using namespace std::chrono_literals;

#ifdef Q_OS_UNIX
void handleSignal(int signal)
{
    // print newline
    qDebug() << "";
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->quit();
    }
}
#endif

void setupAudioStream(GstElement *pipeline, QXmppCallStream *stream)
{
    Q_ASSERT(stream);
    Q_ASSERT(stream->media() == u"audio");

    qDebug() << "[AVCall] Begin audio stream setup";
    // output receiving audio
    stream->setReceivePadCallback([pipeline](GstPad *receivePad) {
        GstElement *output = gst_parse_bin_from_description("audioresample ! audioconvert ! autoaudiosink", true, nullptr);
        if (!gst_bin_add(GST_BIN(pipeline), output)) {
            qFatal("[AVCall] Failed to add audio playback to pipeline");
            return;
        }

        if (gst_pad_link(receivePad, gst_element_get_static_pad(output, "sink")) != GST_PAD_LINK_OK) {
            qFatal("[AVCall] Failed to link receive pad to audio playback.");
        }
        gst_element_sync_state_with_parent(output);

        qDebug() << "[AVCall] Audio playback (receive pad) set up.";
    });

    // record and send microphone
    stream->setSendPadCallback([pipeline](GstPad *sendPad) {
        GstElement *output = gst_parse_bin_from_description("autoaudiosrc ! audioconvert ! audioresample ! queue max-size-time=1000000", true, nullptr);
        if (!gst_bin_add(GST_BIN(pipeline), output)) {
            qFatal("[AVCall] Failed to add audio recorder to pipeline");
            return;
        }

        if (gst_pad_link(gst_element_get_static_pad(output, "src"), sendPad) != GST_PAD_LINK_OK) {
            qFatal("[AVCall] Failed to link audio recorder output to send pad.");
        }
        gst_element_sync_state_with_parent(output);

        qDebug() << "[AVCall] Audio recorder (send pad) set up.";
    });
}

void setupVideoStream(GstElement *pipeline, QXmppCallStream *stream)
{
    Q_ASSERT(stream);
    Q_ASSERT(stream->media() == u"video");

    qDebug() << "[AVCall] Begin video stream setup";
    stream->setReceivePadCallback([pipeline](GstPad *receivePad) {
        GstElement *output = gst_parse_bin_from_description("autovideosink", true, nullptr);
        if (!gst_bin_add(GST_BIN(pipeline), output)) {
            qFatal("[AVCall] Failed to add video playback to pipeline");
            return;
        }

        if (gst_pad_link(receivePad, gst_element_get_static_pad(output, "sink")) != GST_PAD_LINK_OK) {
            qFatal("[AVCall] Failed to link receive pad to video playback.");
        }
        gst_element_sync_state_with_parent(output);

        qDebug() << "[AVCall] Video playback (receive pad) set up.";
    });
    stream->setSendPadCallback([pipeline](GstPad *sendPad) {
        GstElement *output = gst_parse_bin_from_description("videotestsrc", true, nullptr);
        if (!gst_bin_add(GST_BIN(pipeline), output)) {
            qFatal("[AVCall] Failed to add video test source to pipeline");
            return;
        }

        if (gst_pad_link(gst_element_get_static_pad(output, "src"), sendPad) != GST_PAD_LINK_OK) {
            qFatal("[AVCall] Failed to link video test source to send pad.");
        }
        gst_element_sync_state_with_parent(output);

        qDebug() << "[AVCall] Video test source (send pad) set up.";
    });
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QXMPP_JID") || qEnvironmentVariableIsEmpty("QXMPP_PASSWORD")) {
        qDebug() << "'QXMPP_JID' and 'QXMPP_PASSWORD' must be set to connect to a server.";
        return 1;
    }

    QCoreApplication app(argc, argv);

#ifdef Q_OS_UNIX
    // set signal handlers for SIGINT (CTRL+C) and SIGTERM (terminated by QtCreator)
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
#endif

    QXmppClient client;
    auto *rosterManager = client.findExtension<QXmppRosterManager>();
    auto *callManager = client.addNewExtension<QXmppCallManager>();
    client.logger()->setLoggingType(QXmppLogger::StdoutLogging);
    client.logger()->setMessageTypes(QXmppLogger::MessageType::AnyMessage);

    // client config
    QXmppConfiguration config;
    config.setJid(qEnvironmentVariable("QXMPP_JID"));
    config.setPassword(qEnvironmentVariable("QXMPP_PASSWORD"));
    config.setResourcePrefix("Call");
    config.setIgnoreSslErrors(true);

    // call manager config
    callManager->setFallbackStunServers({ StunServer { QHostAddress(QStringLiteral("stun.nextcloud.com")), 443 } });
    // callManager->setTurnServer();

    client.connectToServer(config);

    // our call
    std::unique_ptr<QXmppCall> activeCall;

    auto setupCall = [&](QXmppCall *call) {
        if (auto *audioStream = call->audioStream()) {
            setupAudioStream(call->pipeline(), audioStream);
        }
        if (auto *videoStream = call->videoStream()) {
            setupVideoStream(call->pipeline(), videoStream);
        }

        QObject::connect(call, &QXmppCall::streamCreated, call, [call](QXmppCallStream *stream) {
            if (stream->media() == u"audio") {
                setupAudioStream(call->pipeline(), stream);
            } else if (stream->media() == u"video") {
                setupVideoStream(call->pipeline(), stream);
            } else {
                qDebug() << "[AVCall]" << "Unknown stream added to call";
            }
        });

        QObject::connect(call, &QXmppCall::connected, &app, [=]() {
            qDebug() << "[Call] Call to" << call->jid() << "connected!";

            if (call->videoSupported()) {
                QTimer::singleShot(5s, call, [call] {
                    call->addVideo();
                });
            }
        });
        QObject::connect(call, &QXmppCall::ringing, [=]() {
            qDebug() << "[Call] Ringing" << call->jid() << "...";
        });
        QObject::connect(call, &QXmppCall::finished, [&activeCall]() {
            qDebug() << "[Call] Call with" << activeCall->jid() << "ended. (Deleting)";
            activeCall.release()->deleteLater();
        });
    };

    // on connect
    QObject::connect(&client, &QXmppClient::connected, &app, [&] {
        // wait 1 second for presence of other clients to arrive
        QTimer::singleShot(1s, &app, [&] {
            // other resources of our account
            auto otherResources = rosterManager->getResources(config.jidBare());
            otherResources.removeOne(config.resource());
            if (otherResources.isEmpty()) {
                qDebug() << "[Call] No other clients to call on this account. Start another instance of the example to start a call.";
                return;
            }

            // call first JID
            activeCall = callManager->call(config.jidBare() + u'/' + otherResources.first());
            Q_ASSERT(activeCall != nullptr);

            setupCall(activeCall.get());
        });
    });

    // on call
    QObject::connect(callManager, &QXmppCallManager::callReceived, &app, [&](std::unique_ptr<QXmppCall> &call) {
        // take over ownership
        activeCall = std::move(call);

        qDebug() << "[Call] Received incoming call from" << activeCall->jid() << "-" << "Accepting.";
        activeCall->accept();

        setupCall(activeCall.get());
    });

    // disconnect from server to avoid having multiple open dead sessions when testing
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&client]() {
        qDebug() << "Closing connection...";
        client.disconnectFromServer();
    });

    return app.exec();
}

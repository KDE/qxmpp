// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2019 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppColorGeneration.h"
#include "QXmppCredentials.h"
#include "QXmppE2eeExtension.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppOutgoingClient.h"
#include "QXmppOutgoingClient_p.h"
#include "QXmppPromise.h"
#include "QXmppRegisterIq.h"
#include "QXmppRosterManager.h"
#include "QXmppSasl2UserAgent.h"
#include "QXmppSaslManager_p.h"
#include "QXmppSasl_p.h"
#include "QXmppStreamFeatures.h"
#include "QXmppVCardManager.h"
#include "QXmppVersionManager.h"

#include "Async.h"
#include "Iq.h"
#include "TestClient.h"
#include "util.h"

#include <QObject>

using namespace QXmpp::Private;

class tst_QXmppClient : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testSendMessage();
    Q_SLOT void testIndexOfExtension();
    Q_SLOT void testE2eeExtension();
    Q_SLOT void testTaskDirect();
    Q_SLOT void testTaskStore();
    Q_SLOT void taskMultipleThen();
    Q_SLOT void colorGeneration();
#if QT_GUI_LIB
    Q_SLOT void colorGenerationQColor();
#endif

    // outgoing client
    Q_SLOT void csiManager();
    Q_SLOT void sasl2FastFallbackKeepsListener();

    Q_SLOT void credentialsSerialization();
};

void tst_QXmppClient::testSendMessage()
{
    auto client = std::make_unique<QXmppClient>();

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    client->setLogger(&logger);

    connect(&logger, &QXmppLogger::message, this, [](QXmppLogger::MessageType type, const QString &text) {
        QCOMPARE(type, QXmppLogger::MessageType::SentMessage);

        QXmppMessage msg;
        parsePacket(msg, text.toUtf8());

        QCOMPARE(msg.from(), QString());
        QCOMPARE(msg.to(), u"support@qxmpp.org"_s);
        QCOMPARE(msg.body(), u"implement XEP-* plz"_s);
    });

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    client->sendMessage(u"support@qxmpp.org"_s, u"implement XEP-* plz"_s);
    QT_WARNING_POP

    // see handleMessageSent()

    client->setLogger(nullptr);
}

void tst_QXmppClient::testIndexOfExtension()
{
    auto client = std::make_unique<QXmppClient>();

    for (auto *ext : client->extensions()) {
        client->removeExtension(ext);
    }

    auto rosterManager = new QXmppRosterManager(client.get());
    auto vCardManager = new QXmppVCardManager;

    client->addExtension(rosterManager);
    client->addExtension(vCardManager);

    // This extension is not in the list.
    QCOMPARE(client->indexOfExtension<QXmppVersionManager>(), -1);

    // These extensions are in the list.
    QCOMPARE(client->indexOfExtension<QXmppRosterManager>(), 0);
    QCOMPARE(client->indexOfExtension<QXmppVCardManager>(), 1);
}

class EncryptionExtension : public QXmppE2eeExtension
{
public:
    bool messageCalled = false;
    bool iqCalled = false;

    QXmppTask<MessageEncryptResult> encryptMessage(QXmppMessage &&, const std::optional<QXmppSendStanzaParams> &) override
    {
        messageCalled = true;
        return makeReadyTask<MessageEncryptResult>(QXmppError { "it's only a test", QXmpp::SendError::EncryptionError });
    }
    QXmppTask<MessageDecryptResult> decryptMessage(QXmppMessage &&) override
    {
        return makeReadyTask<MessageDecryptResult>(QXmppError { "it's only a test", QXmpp::SendError::EncryptionError });
    }

    QXmppTask<IqEncryptResult> encryptIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> &) override
    {
        iqCalled = true;
        return makeReadyTask<IqEncryptResult>(QXmppError { "it's only a test", QXmpp::SendError::EncryptionError });
    }

    QXmppTask<IqDecryptResult> decryptIq(const QDomElement &) override
    {
        return makeReadyTask<IqDecryptResult>(QXmppError { "it's only a test", QXmpp::SendError::EncryptionError });
    }

    bool isEncrypted(const QDomElement &) override { return false; };
    bool isEncrypted(const QXmppMessage &) override { return false; };
};

void tst_QXmppClient::testE2eeExtension()
{
    QXmppClient client;
    EncryptionExtension encrypter;
    client.setEncryptionExtension(&encrypter);

    auto result = client.sendSensitive(QXmppMessage("me@qxmpp.org", "somebody@qxmpp.org", "Hello"));
    QVERIFY(encrypter.messageCalled);
    QVERIFY(!encrypter.iqCalled);
    QCoreApplication::processEvents();
    expectFutureVariant<QXmppError>(result.toFuture(this));

    encrypter.messageCalled = false;
    result = client.sendSensitive(QXmppPresence(QXmppPresence::Available));
    QVERIFY(!encrypter.messageCalled);
    QVERIFY(!encrypter.iqCalled);

    auto createRequest = []() {
        CompatIq iq { QXmppIq::Get, QXmppDiscoInfo {} };
        iq.setTo(u"component.qxmpp.org"_s);
        return iq;
    };

    client.sendSensitive(createRequest());
    QVERIFY(encrypter.iqCalled);
    encrypter.iqCalled = false;

    client.send(createRequest());
    QVERIFY(!encrypter.iqCalled);
    encrypter.iqCalled = false;

    client.sendIq(createRequest());
    QVERIFY(!encrypter.iqCalled);
    encrypter.iqCalled = false;

    client.sendSensitiveIq(createRequest());
    QVERIFY(encrypter.iqCalled);
    encrypter.iqCalled = false;
}

void tst_QXmppClient::testTaskDirect()
{
    QXmppPromise<QXmppIq> p;
    QXmppRegisterIq iq;
    iq.setUsername("username");

    bool thenCalled = false;
    p.task().then(this, [&thenCalled](QXmppIq &&iq) {
        thenCalled = true;
        // casting not supported
        QVERIFY(!dynamic_cast<QXmppRegisterIq *>(&iq));
    });
    p.finish(std::move(iq));

    QVERIFY(thenCalled);
    QVERIFY(p.task().isFinished());
    QVERIFY(!p.task().hasResult());
}

static QXmppTask<QXmppIq> generateRegisterIq()
{
    QXmppPromise<QXmppIq> p;
    QXmppRegisterIq iq;
    iq.setFrom("juliet");
    iq.setUsername("username");
    p.finish(std::move(iq));
    return p.task();
}

void tst_QXmppClient::testTaskStore()
{
    auto task = generateRegisterIq();

    bool thenCalled = false;
    task.then(this, [&thenCalled](QXmppIq &&iq) {
        thenCalled = true;

        QCOMPARE(iq.from(), u"juliet"_s);
        // casting not supported
        QVERIFY(!dynamic_cast<QXmppRegisterIq *>(&iq));
    });
    QVERIFY(thenCalled);

    QXmppPromise<QXmppIq> p;
    QXmppRegisterIq iq;
    iq.setUsername("username");
    p.finish(std::move(iq));

    QVERIFY(p.task().hasResult());
    QVERIFY(p.task().isFinished());

    thenCalled = false;
    p.task().then(this, [&thenCalled](QXmppIq &&iq) {
        thenCalled = true;
        // casting not supported
        QVERIFY(!dynamic_cast<QXmppRegisterIq *>(&iq));
    });
    QVERIFY(thenCalled);

    QVERIFY(p.task().isFinished());
    QVERIFY(!p.task().hasResult());
}

void tst_QXmppClient::taskMultipleThen()
{
    bool exec1 = false, exec2 = false, exec3 = false, exec4 = false;
    QString called;

    auto context1 = std::make_unique<QObject>();

    QXmppPromise<QString> p;
    p.task().then(this, [&](QString) {
        called.append(u'1');
    });
    p.task().then(context1.get(), [&](QString) {
        called.append(u'2');
    });
    p.task().then(this, [&](QString) {
        called.append(u'3');
    });
    QVERIFY(called.isEmpty());
    context1.reset();
    p.finish(u"test"_s);
    QCOMPARE(called, u"3"_s);
}

void tst_QXmppClient::colorGeneration()
{
    QCOMPARE(QString::number(generateColorAngle(u"Romeo")), u"327.255");

    auto rgb = QXmppColorGeneration::generateRgb(u"Romeo");
    QCOMPARE(rgb.red, quint8(0.865 * 255));
    QCOMPARE(rgb.green, 0);
    QCOMPARE(rgb.blue, quint8(0.686 * 255));
}

#if QT_GUI_LIB
void tst_QXmppClient::colorGenerationQColor()
{
    auto color = QXmppColorGeneration::generateColor(u"Romeo");
    QCOMPARE(color.red(), quint8(0.865 * 255));
}
#endif

void tst_QXmppClient::csiManager()
{
    TestClient client;
    auto &csi = client.stream()->csiManager();

    QCOMPARE(client.isActive(), true);
    QCOMPARE(csi.state(), CsiManager::Active);

    client.setActive(false);
    client.expectNoPacket();

    // enable CSI and authenticate client
    client.streamPrivate()->isAuthenticated = true;
    QXmppStreamFeatures features;
    features.setClientStateIndicationMode(QXmppStreamFeatures::Enabled);
    csi.onStreamFeatures(features);
    csi.onSessionOpened({});

    client.expect("<inactive xmlns='urn:xmpp:csi:0'/>");

    // we currently can't really test stream resumption because the socket is not actually
    // connected

    // bind2
    Bind2Request r;
    csi.onBind2Request(r, { "urn:xmpp:csi:0" });
    QCOMPARE(r.csiInactive, true);

    SessionBegin session {
        false,
        false,
        true,
    };
    csi.onSessionOpened(session);
    client.expectNoPacket();
}

// Regression test for the SASL2 + FAST listener-replacement bug.
//
// When a stored FAST token (XEP-0484) is rejected by the server, QXmppOutgoingClient retries
// SASL2 authentication with a password-based mechanism. The retry happens from inside the
// failed task's .then() continuation, which calls startSasl2Auth() recursively, which calls
// setListener<Sasl2Manager>() — installing a NEW Sasl2Manager into d->listener while the OLD
// Sasl2Manager's handleElement() call is still on the stack.
//
// Before the fix, handlePacketReceived() compared d->listener.index() before/after the call to
// decide whether to fall back to OutgoingClient as the active listener. Both old and new
// listeners were Sasl2Manager — same variant index — so the check failed to notice the
// replacement and overwrote the new Sasl2Manager with OutgoingClient. The next stanza (the
// SCRAM challenge) then landed on the wrong handler and produced
// "Unexpected element received while handling client session." A monotonic listener generation
// counter, captured before the call and re-checked after, fixes this.
void tst_QXmppClient::sasl2FastFallbackKeepsListener()
{
    TestClient client;
    auto &config = client.stream()->configuration();
    config.setUser(u"bowman"_s);
    config.setPassword(u"1234"_s);
    config.setDomain(u"example.org"_s);
    config.setDisabledSaslMechanisms({});
    config.setSasl2UserAgent(QXmppSasl2UserAgent {
        QUuid::fromString(u"d4565fa7-4d72-4749-b3d3-740edbf87770"_s),
        u"QXmpp"_s,
        u"HAL 9000"_s,
    });

    // Pre-populate a (stale) FAST token, as if from a previous session.
    config.credentialData().htToken = HtToken {
        SaslHtMechanism { IanaHashAlgorithm::Sha3_512, SaslHtMechanism::None },
        u"old-invalid-token"_s,
        QDateTime::fromString(u"2024-07-11T14:00:00Z"_s, Qt::ISODate),
    };

    Sasl2::StreamFeature sasl2Feature {
        { u"PLAIN"_s },
        {},
        FastFeature { { u"HT-SHA3-512-NONE"_s }, false },
        false,
    };

    // Kick off SASL2 auth. The first attempt picks the FAST HT mechanism.
    client.startSasl2Auth(sasl2Feature);

    QVERIFY(std::holds_alternative<Sasl2Manager>(client.streamPrivate()->listener));
    auto firstAuth = client.takePacket();
    QVERIFY(firstAuth.contains(u"mechanism=\"HT-SHA3-512-NONE\""_s));
    QVERIFY(firstAuth.contains(u"<fast xmlns=\"urn:xmpp:fast:0\"/>"_s));

    // Server rejects the token. This synchronously runs the failed task's .then() continuation,
    // which retries by calling startSasl2Auth() → setListener<Sasl2Manager>(). With the fix in
    // place, handlePacketReceived() must NOT overwrite the new Sasl2Manager with OutgoingClient.
    client.handlePacketReceived(xmlToDom(
        "<failure xmlns='urn:xmpp:sasl:2'>"
        "<not-authorized xmlns='urn:ietf:params:xml:ns:xmpp-sasl'/>"
        "</failure>"));

    // Critical assertion: the listener is still a Sasl2Manager (the second one). Without the
    // generation-counter fix this would be QXmppOutgoingClient* and the next stanza would be
    // rejected as "Unexpected element received while handling client session."
    QVERIFY(std::holds_alternative<Sasl2Manager>(client.streamPrivate()->listener));

    // The retry should have sent a second <authenticate>, this time with PLAIN, no <fast/>,
    // and a fresh FAST token request.
    auto secondAuth = client.takePacket();
    QVERIFY(secondAuth.contains(u"mechanism=\"PLAIN\""_s));
    QVERIFY(!secondAuth.contains(u"<fast xmlns=\"urn:xmpp:fast:0\"/>"_s));
    QVERIFY(secondAuth.contains(u"<request-token xmlns=\"urn:xmpp:fast:0\" mechanism=\"HT-SHA3-512-NONE\"/>"_s));
    // Stale token must still be present — server may have been temporarily misconfigured.
    QVERIFY(config.credentialData().htToken.has_value());
    QCOMPARE(config.credentialData().htToken->secret, u"old-invalid-token"_s);

    // Server now accepts the password attempt and provides a fresh token. The same Sasl2Manager
    // instance handles this success element.
    client.handlePacketReceived(xmlToDom(
        "<success xmlns='urn:xmpp:sasl:2'>"
        "<authorization-identifier>bowman@example.org</authorization-identifier>"
        "<token xmlns='urn:xmpp:fast:0' token='new-valid-token' expiry='2024-08-01T14:00:00Z'/>"
        "</success>"));

    QVERIFY(client.streamPrivate()->isAuthenticated);
    QVERIFY(config.credentialData().htToken.has_value());
    QCOMPARE(config.credentialData().htToken->secret, u"new-valid-token"_s);
}

void tst_QXmppClient::credentialsSerialization()
{
    QByteArray xml =
        "<credentials xmlns=\"org.qxmpp.credentials\">"
        "<ht-token mechanism=\"HT-SHA3-384-UNIQ\" secret=\"t0k3n1234\" expiry=\"2024-09-21T18:00:00Z\"/>"
        "</credentials>";
    QXmlStreamReader r(xml);
    r.readNextStartElement();
    auto credentials = unwrap(QXmppCredentials::fromXml(r));

    QString output;
    QXmlStreamWriter w(&output);
    credentials.toXml(w);
    QCOMPARE(output, xml);
}

QTEST_GUILESS_MAIN(tst_QXmppClient)
#include "tst_qxmppclient.moc"

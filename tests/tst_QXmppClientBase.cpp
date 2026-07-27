// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2019 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the client and the managers it always sets up.
// Merging the client, service discovery and roster tests into one translation
// unit parses the shared Qt/QXmpp headers once instead of once per file. Each
// original test keeps its own namespace; main() runs them in turn.

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppColorGeneration.h"
#include "QXmppContactAddresses.h"
#include "QXmppCredentials.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeExtension.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppMovedManager.h"
#include "QXmppOutgoingClient.h"
#include "QXmppOutgoingClient_p.h"
#include "QXmppPromise.h"
#include "QXmppPubSubManager.h"
#include "QXmppRegisterIq.h"
#include "QXmppRosterIq.h"
#include "QXmppRosterManager.h"
#include "QXmppRosterMemoryStorage.h"
#include "QXmppRosterStorage.h"
#include "QXmppSasl2UserAgent.h"
#include "QXmppSaslManager_p.h"
#include "QXmppSasl_p.h"
#include "QXmppStreamFeatures.h"
#include "QXmppTask.h"
#include "QXmppVCardManager.h"
#include "QXmppVersionManager.h"

#include "Algorithms.h"
#include "Iq.h"
#include "StringLiterals.h"
#include "TestClient.h"
#include "util.h"

#include <QCoreApplication>
#include <QObject>

namespace Client {

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
    Q_SLOT void testTaskOptionalNullopt();
    Q_SLOT void testTaskThenChainSuspended();
    Q_SLOT void testChainIq();
    Q_SLOT void colorGeneration();
#if QT_GUI_LIB
    Q_SLOT void colorGenerationQColor();
#endif

    // outgoing client
    Q_SLOT void chooseResource();
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
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }
    QXmppTask<MessageDecryptResult> decryptMessage(QXmppMessage &&) override
    {
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }

    QXmppTask<IqEncryptResult> encryptIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> &) override
    {
        iqCalled = true;
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }

    QXmppTask<IqDecryptResult> decryptIq(const QDomElement &) override
    {
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
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
    expectFutureVariant<QXmppError>(result);

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
}

static QXmppTask<QXmppIq> generateRegisterIq()
{
    QXmppPromise<QXmppIq> p;
    QXmppRegisterIq iq;
    iq.setFrom("juliet");
    iq.setUsername("username");
    auto task = p.task();
    p.finish(std::move(iq));
    return task;
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

void tst_QXmppClient::testTaskOptionalNullopt()
{
    // Regression test: finishing a promise whose T is std::optional<X>
    // with std::nullopt must produce an engaged result holding an empty
    // inner optional, not disengage the internal storage. takeResult()
    // previously dereferenced an empty optional and crashed.
    QXmppPromise<std::optional<int>> p;
    auto task = p.task();
    p.finish(std::nullopt);

    QVERIFY(task.isFinished());
    QVERIFY(task.hasResult());
    QVERIFY(!task.takeResult().has_value());
}

void tst_QXmppClient::testTaskThenChainSuspended()
{
    // Regression test: QXmppTask::then() with a non-void-returning continuation
    // must co_return the continuation's value, so the chained task carries it
    // through. This also exercises the suspension path: the source task is not
    // yet finished when then() is called, so the then() coroutine actually
    // suspends and is resumed later by promise.finish().
    QXmppPromise<int> sourcePromise;
    auto sourceTask = sourcePromise.task();

    auto chainedTask = sourceTask.then(this, [](int &&value) -> QString {
        return QString::number(value * 2);
    });

    // The source has not been finished yet, so the chained task must still be
    // suspended on the inner co_await.
    QVERIFY(!chainedTask.isFinished());

    sourcePromise.finish(21);

    // After finishing the source, then() resumes, runs the continuation and
    // co_returns its result into the chained task.
    QVERIFY(chainedTask.isFinished());
    QVERIFY(chainedTask.hasResult());
    QCOMPARE(chainedTask.takeResult(), u"42"_s);
}

using RosterResult = std::variant<QXmppRosterIq, QXmppError>;

static QXmppTask<RosterResult> parseIqResult(QXmppTask<QXmppClient::IqResult> &&sendTask, QObject *context)
{
    co_return parseIq<QXmppRosterIq>(co_await sendTask);
}

void tst_QXmppClient::testChainIq()
{
    QXmppPromise<QXmppClient::IqResult> iqP;
    auto task = iqP.task();

    auto parsingTask = parseIqResult(std::move(task), this);

    QVERIFY(!parsingTask.isFinished());

    iqP.finish(xmlToDom(
        "<iq id='qx1' from='user@example.org' type='result'>"
        "<query xmlns='jabber:iq:roster'>"
        "<item jid='romeo@example.org'/>"
        "<item jid='juliet@example.org'/>"
        "</query>"
        "</iq>"));

    QVERIFY(parsingTask.isFinished());
    auto result = parsingTask.result();
    QVERIFY(std::holds_alternative<QXmppRosterIq>(result));
    QCOMPARE(std::get<QXmppRosterIq>(result).items().size(), 2);
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

void tst_QXmppClient::chooseResource()
{
    QXmppConfiguration config;

    // by default, a resource is generated from the default prefix "QXmpp"
    QCOMPARE(config.resource(), QString());
    QCOMPARE(config.resourcePrefix(), u"QXmpp"_s);
    auto generated = QXmpp::Private::chooseResource(config);
    QVERIFY(generated.startsWith(u"QXmpp."_s));
    QVERIFY(generated.size() > QString(u"QXmpp.").size());
    // a fresh resource is generated on each call
    QVERIFY(QXmpp::Private::chooseResource(config) != generated);

    // a custom prefix is used to generate the resource
    config.setResourcePrefix(u"phone"_s);
    QVERIFY(QXmpp::Private::chooseResource(config).startsWith(u"phone."_s));

    // an explicitly set resource takes precedence over the prefix
    config.setResource(u"laptop"_s);
    QCOMPARE(QXmpp::Private::chooseResource(config), u"laptop"_s);

    // without resource and prefix, the resource is left to the server
    config.setResource({});
    config.setResourcePrefix({});
    QCOMPARE(QXmpp::Private::chooseResource(config), QString());
}

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

}  // namespace Client

// ============================================================

namespace Discovery {

using namespace QXmpp;

class tst_QXmppDiscoveryManager : public QObject
{
    Q_OBJECT
private:
    Q_SLOT void testInfo();
    Q_SLOT void testItems();
    Q_SLOT void testRequests();
    Q_SLOT void cachingItems();
    Q_SLOT void cachingInfo();

    Q_SLOT void discoverServicesBasic();
    Q_SLOT void discoverServicesWithType();
    Q_SLOT void discoverServicesWithFeatures();
    Q_SLOT void discoverServicesLifetime();
    Q_SLOT void discoverServicesReconnect();
    Q_SLOT void discoverServicesAfterDiscovery();
};

void tst_QXmppDiscoveryManager::testInfo()
{
    TestClient test;
    auto *discoManager = test.addNewExtension<QXmppDiscoveryManager>();

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto task = discoManager->requestDiscoInfo("user@example.org");
    QT_WARNING_POP

    test.expect("<iq id='qx2' to='user@example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>");
    test.inject<QString>(R"(
<iq id='qx2' from='user@example.org' type='result'>
    <query xmlns='http://jabber.org/protocol/disco#info'>
        <identity category='pubsub' type='service'/>
        <feature var='http://jabber.org/protocol/pubsub'/>
        <feature var='urn:xmpp:mix:core:1'/>
    </query>
</iq>)");

    const auto info = expectFutureVariant<QXmppDiscoveryIq>(task);

    const QStringList expFeatures = { "http://jabber.org/protocol/pubsub", "urn:xmpp:mix:core:1" };
    QCOMPARE(info.features(), expFeatures);
    QCOMPARE(info.identities().count(), 1);

    // new API (data is also cached when using the old API)
    auto task2 = discoManager->info("user@example.org");
    test.expectNoPacket();

    const auto info2 = expectFutureVariant<QXmppDiscoInfo>(task2);

    QCOMPARE(info2.features(), expFeatures);
    QCOMPARE(info2.identities().count(), 1);
}

void tst_QXmppDiscoveryManager::testItems()
{
    TestClient test;
    auto *discoManager = test.addNewExtension<QXmppDiscoveryManager>();

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    auto task = discoManager->requestDiscoItems("user@example.org");
    QT_WARNING_POP
    test.expect("<iq id='qx1' to='user@example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>");
    test.inject<QString>(R"(
<iq type='result'
    from='user@example.org'
    id='qx1'>
  <query xmlns='http://jabber.org/protocol/disco#items'>
    <item jid='368866411b877c30064a5f62b917cffe@test.org'/>
    <item jid='3300659945416e274474e469a1f0154c@test.org'/>
    <item jid='4e30f35051b7b8b42abe083742187228@test.org'/>
    <item jid='ae890ac52d0df67ed7cfdf51b644e901@test.org'/>
  </query>
</iq>)");

    const auto items = expectFutureVariant<QList<QXmppDiscoItem>>(task);

    QCOMPARE(items.size(), 4);
    QCOMPARE(items.at(0).jid(), u"368866411b877c30064a5f62b917cffe@test.org"_s);
    QCOMPARE(items.at(1).jid(), u"3300659945416e274474e469a1f0154c@test.org"_s);
    QCOMPARE(items.at(2).jid(), u"4e30f35051b7b8b42abe083742187228@test.org"_s);
    QCOMPARE(items.at(3).jid(), u"ae890ac52d0df67ed7cfdf51b644e901@test.org"_s);
}

void tst_QXmppDiscoveryManager::testRequests()
{
    TestClient test;
    test.configuration().setJid("user@qxmpp.org/a");
    auto *discoManager = test.addNewExtension<QXmppDiscoveryManager>();
    // the default client name is the application name, i.e. the name of the
    // test binary; set it explicitly so this test does not depend on it
    discoManager->setClientName(u"tst_QXmppDiscoveryManager"_s);

    discoManager->handleStanza(xmlToDom(R"(
<iq type='get' from='romeo@montague.net/orchard' to='user@qxmpp.org/a' id='info1'>
  <query xmlns='http://jabber.org/protocol/disco#info'/>
</iq>)"));

    test.expect(
        "<iq id='info1' to='romeo@montague.net/orchard' type='result'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'>"
        "<identity category='client' name='tst_QXmppDiscoveryManager' type='pc'/>"
        "<feature var='http://jabber.org/protocol/caps'/>"
        "<feature var='http://jabber.org/protocol/chatstates'/>"
        "<feature var='http://jabber.org/protocol/disco#info'/>"
        "<feature var='http://jabber.org/protocol/rsm'/>"
        "<feature var='http://jabber.org/protocol/xhtml-im'/>"
        "<feature var='jabber:x:conference'/>"
        "<feature var='jabber:x:data'/>"
        "<feature var='jabber:x:oob'/>"
        "<feature var='urn:xmpp:chat-markers:0'/>"
        "<feature var='urn:xmpp:eme:0'/>"
        "<feature var='urn:xmpp:fallback:0'/>"
        "<feature var='urn:xmpp:hints'/>"
        "<feature var='urn:xmpp:message-attaching:1'/>"
        "<feature var='urn:xmpp:message-correct:0'/>"
        "<feature var='urn:xmpp:reactions:0'/>"
        "<feature var='urn:xmpp:sid:0'/>"
        "<feature var='urn:xmpp:spoiler:0'/>"
        "</query>"
        "</iq>");
}

void tst_QXmppDiscoveryManager::cachingItems()
{
    TestClient test;
    auto *discoManager = test.addNewExtension<QXmppDiscoveryManager>();

    // multiple parallel equal requests only result in one real sent IQ request
    auto t1 = discoManager->items("user@example.org");
    auto t2 = discoManager->items("user@example.org");
    auto t3 = discoManager->items("user@example.org");

    test.expect("<iq id='qx1' to='user@example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>");
    test.inject<QString>(R"(
<iq type='result'
    from='user@example.org'
    id='qx1'>
  <query xmlns='http://jabber.org/protocol/disco#items'>
    <item jid='368866411b877c30064a5f62b917cffe@test.org'/>
    <item jid='3300659945416e274474e469a1f0154c@test.org'/>
    <item jid='4e30f35051b7b8b42abe083742187228@test.org'/>
    <item jid='ae890ac52d0df67ed7cfdf51b644e901@test.org'/>
  </query>
</iq>)");

    auto t4 = discoManager->items("user@example.org");
    test.expectNoPacket();

    const auto items1 = expectFutureVariant<QList<QXmppDiscoItem>>(t1);
    const auto items2 = expectFutureVariant<QList<QXmppDiscoItem>>(t2);
    const auto items3 = expectFutureVariant<QList<QXmppDiscoItem>>(t3);
    const auto items4 = expectFutureVariant<QList<QXmppDiscoItem>>(t4);

    for (const auto &items : { items1, items2, items3, items4 }) {
        QCOMPARE(items.size(), 4);
        QCOMPARE(items.at(0).jid(), u"368866411b877c30064a5f62b917cffe@test.org"_s);
        QCOMPARE(items.at(1).jid(), u"3300659945416e274474e469a1f0154c@test.org"_s);
        QCOMPARE(items.at(2).jid(), u"4e30f35051b7b8b42abe083742187228@test.org"_s);
        QCOMPARE(items.at(3).jid(), u"ae890ac52d0df67ed7cfdf51b644e901@test.org"_s);
    }
}

void tst_QXmppDiscoveryManager::cachingInfo()
{
    TestClient test;
    auto *discoManager = test.addNewExtension<QXmppDiscoveryManager>();

    // multiple parallel equal requests only result in one real sent IQ request
    auto t1 = discoManager->info("user@example.org");
    auto t2 = discoManager->info("user@example.org");
    auto t3 = discoManager->info("user@example.org");

    test.expect("<iq id='qx1' to='user@example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>");
    test.inject<QString>(R"(
<iq id='qx1' from='user@example.org' type='result'>
    <query xmlns='http://jabber.org/protocol/disco#info'>
        <identity category='pubsub' type='service'/>
        <feature var='http://jabber.org/protocol/pubsub'/>
        <feature var='urn:xmpp:mix:core:1'/>
    </query>
</iq>)");

    auto t4 = discoManager->info("user@example.org");
    test.expectNoPacket();

    const auto info1 = expectFutureVariant<QXmppDiscoInfo>(t1);
    const auto info2 = expectFutureVariant<QXmppDiscoInfo>(t2);
    const auto info3 = expectFutureVariant<QXmppDiscoInfo>(t3);
    const auto info4 = expectFutureVariant<QXmppDiscoInfo>(t4);

    for (const auto &info : { info1, info2, info3, info4 }) {
        QCOMPARE(info.identities().size(), 1);
        QCOMPARE(info.features().size(), 2);
        QCOMPARE(info.features().at(0), u"http://jabber.org/protocol/pubsub"_s);
        QCOMPARE(info.features().at(1), u"urn:xmpp:mix:core:1"_s);
    }
}

void tst_QXmppDiscoveryManager::discoverServicesBasic()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    auto watch = disco->discoverServices(Disco::Category::Store);
    QVERIFY(!watch.loaded().value());
    QVERIFY(watch.services().value().isEmpty());

    // Simulate new connection
    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    // Expect items query on server domain
    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='upload.example.org'/>"
                "<item jid='muc.example.org'/>"
                "</query></iq>"_s);

    // Expect info queries for each item
    auto uploadInfoId = test.expectPacketRandomOrder(u"<iq id='qx1' to='upload.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    auto mucInfoId = test.expectPacketRandomOrder(u"<iq id='qx1' to='muc.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);

    // Inject upload service info (matches Store category)
    test.inject(u"<iq id='" + uploadInfoId + u"' from='upload.example.org' type='result'>"
                                             "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                             "<identity category='store' type='file'/>"
                                             "<feature var='urn:xmpp:http:upload:0'/>"
                                             "</query></iq>");

    // Should have one result now, not yet loaded
    QCOMPARE(watch.services().value().size(), 1);
    QCOMPARE(watch.services().value().at(0).jid, u"upload.example.org"_s);
    QVERIFY(!watch.loaded().value());

    // Inject MUC service info (does NOT match Store category)
    test.inject(u"<iq id='" + mucInfoId + u"' from='muc.example.org' type='result'>"
                                          "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                          "<identity category='conference' type='text'/>"
                                          "<feature var='http://jabber.org/protocol/muc'/>"
                                          "</query></iq>");

    // Now loaded, still only the upload service
    QVERIFY(watch.loaded().value());
    QCOMPARE(watch.services().value().size(), 1);
}

void tst_QXmppDiscoveryManager::discoverServicesWithType()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    auto watch = disco->discoverServices(Disco::Category::Conference, Disco::Type::Mix);

    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='mix.example.org'/>"
                "<item jid='muc.example.org'/>"
                "</query></iq>"_s);

    auto mixId = test.expectPacketRandomOrder(u"<iq id='qx1' to='mix.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    auto mucId = test.expectPacketRandomOrder(u"<iq id='qx1' to='muc.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);

    test.inject(u"<iq id='" + mixId + u"' from='mix.example.org' type='result'>"
                                      "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                      "<identity category='conference' type='mix'/>"
                                      "<feature var='urn:xmpp:mix:core:1'/>"
                                      "</query></iq>");

    test.inject(u"<iq id='" + mucId + u"' from='muc.example.org' type='result'>"
                                      "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                      "<identity category='conference' type='text'/>"
                                      "<feature var='http://jabber.org/protocol/muc'/>"
                                      "</query></iq>");

    QVERIFY(watch.loaded().value());
    QCOMPARE(watch.services().value().size(), 1);
    QCOMPARE(watch.services().value().at(0).jid, u"mix.example.org"_s);
}

void tst_QXmppDiscoveryManager::discoverServicesWithFeatures()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    auto watch = disco->discoverServices(Disco::Category::Store, {}, { u"urn:xmpp:http:upload:0"_s });

    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='store1.example.org'/>"
                "<item jid='store2.example.org'/>"
                "</query></iq>"_s);

    auto id1 = test.expectPacketRandomOrder(u"<iq id='qx1' to='store1.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    auto id2 = test.expectPacketRandomOrder(u"<iq id='qx1' to='store2.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);

    // store1 has the required feature
    test.inject(u"<iq id='" + id1 + u"' from='store1.example.org' type='result'>"
                                    "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                    "<identity category='store' type='file'/>"
                                    "<feature var='urn:xmpp:http:upload:0'/>"
                                    "</query></iq>");

    // store2 is a store but lacks the required feature
    test.inject(u"<iq id='" + id2 + u"' from='store2.example.org' type='result'>"
                                    "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                    "<identity category='store' type='file'/>"
                                    "</query></iq>");

    QVERIFY(watch.loaded().value());
    QCOMPARE(watch.services().value().size(), 1);
    QCOMPARE(watch.services().value().at(0).jid, u"store1.example.org"_s);
}

void tst_QXmppDiscoveryManager::discoverServicesLifetime()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    {
        auto watch = disco->discoverServices(Disco::Category::Store);
        // watch goes out of scope here
    }

    // Triggering discovery should not crash even though watch was dropped
    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    // The items query should still not be sent since there are no live watches
    test.expectNoPacket();
}

void tst_QXmppDiscoveryManager::discoverServicesReconnect()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    auto watch = disco->discoverServices(Disco::Category::Conference);

    // First connection
    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='muc.example.org'/>"
                "</query></iq>"_s);

    test.expect(u"<iq id='qx1' to='muc.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='muc.example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#info'>"
                "<identity category='conference' type='text'/>"
                "<feature var='http://jabber.org/protocol/muc'/>"
                "</query></iq>"_s);

    QVERIFY(watch.loaded().value());
    QCOMPARE(watch.services().value().size(), 1);

    // Reconnect (new stream)
    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    // Watch should be reset
    QVERIFY(!watch.loaded().value());
    QVERIFY(watch.services().value().isEmpty());

    // New discovery
    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='muc2.example.org'/>"
                "</query></iq>"_s);

    test.expect(u"<iq id='qx1' to='muc2.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='muc2.example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#info'>"
                "<identity category='conference' type='text'/>"
                "<feature var='http://jabber.org/protocol/muc'/>"
                "</query></iq>"_s);

    QVERIFY(watch.loaded().value());
    QCOMPARE(watch.services().value().size(), 1);
    QCOMPARE(watch.services().value().at(0).jid, u"muc2.example.org"_s);
}

void tst_QXmppDiscoveryManager::discoverServicesAfterDiscovery()
{
    TestClient test;
    test.configuration().setDomain(u"example.org"_s);
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();

    // Register a first watch and complete discovery
    auto watch1 = disco->discoverServices(Disco::Category::Store);

    test.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT test.connected();

    test.expect(u"<iq id='qx1' to='example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                "<item jid='upload.example.org'/>"
                "</query></iq>"_s);

    test.expect(u"<iq id='qx1' to='upload.example.org' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    test.inject(u"<iq id='qx1' from='upload.example.org' type='result'>"
                "<query xmlns='http://jabber.org/protocol/disco#info'>"
                "<identity category='store' type='file'/>"
                "<feature var='urn:xmpp:http:upload:0'/>"
                "</query></iq>"_s);

    QVERIFY(watch1.loaded().value());
    QCOMPARE(watch1.services().value().size(), 1);

    // Register a new watch for the same category — should be immediately populated
    auto watch2 = disco->discoverServices(Disco::Category::Store);
    QVERIFY(watch2.loaded().value());
    QCOMPARE(watch2.services().value().size(), 1);
    QCOMPARE(watch2.services().value().at(0).jid, u"upload.example.org"_s);

    // Register a watch for a non-matching category — should be loaded but empty
    auto watch3 = disco->discoverServices(Disco::Category::Conference);
    QVERIFY(watch3.loaded().value());
    QVERIFY(watch3.services().value().isEmpty());

    // No new IQ should be sent
    test.expectNoPacket();
}

}  // namespace Discovery

// ============================================================

namespace Roster {

using namespace QXmpp::Private;

class tst_QXmppRosterManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testDiscoFeatures();
    Q_SLOT void testRenameItem();
    Q_SLOT void testSubscriptionRequestReceived();
    Q_SLOT void testMovedSubscriptionRequestReceived_data();
    Q_SLOT void testMovedSubscriptionRequestReceived();
    Q_SLOT void testAddItem();
    Q_SLOT void testRemoveItem();
    Q_SLOT void testUpdateGroups();
    Q_SLOT void testDefaultStorage();
    Q_SLOT void testSetStorage();
    Q_SLOT void testPushPersistsAdd();
    Q_SLOT void testPushPersistsRemove();
    Q_SLOT void testClearCache();
    Q_SLOT void testRosterIqVerSerialization();

private:
    QXmppClient client;
    QXmppLogger logger;
    QXmppRosterManager *manager;
};

void tst_QXmppRosterManager::initTestCase()
{
    logger.setLoggingType(QXmppLogger::SignalLogging);
    client.setLogger(&logger);

    manager = client.findExtension<QXmppRosterManager>();
}

void tst_QXmppRosterManager::testDiscoFeatures()
{
    QCOMPARE(manager->discoveryFeatures(), QStringList());
}

void tst_QXmppRosterManager::testRenameItem()
{
    // used to clean up lambda signal connections
    QObject context;

    auto createItem = [](const QString &jid, const QString &ask = {}) -> QXmppRosterIq::Item {
        QXmppRosterIq::Item item;
        item.setBareJid(jid);
        item.setSubscriptionStatus(ask);
        return item;
    };

    // fill roster with initial contacts to rename
    QXmppRosterIq initialItems;
    initialItems.setType(QXmppIq::Result);
    initialItems.addItem(createItem("stpeter@jabber.org"));
    initialItems.addItem(createItem("bob@qxmpp.org"));

    QVERIFY(manager->handleStanza(writePacketToDom(initialItems)));

    // set a subscription state for bob (the subscription state MUST NOT be
    // sent when renaming an item, so we need to check that it's not)
    QXmppRosterIq bobAsk;
    bobAsk.setType(QXmppIq::Set);
    bobAsk.addItem(createItem("bob@qxmpp.org", "subscribe"));

    QVERIFY(manager->handleStanza(writePacketToDom(bobAsk)));
    QCOMPARE(manager->getRosterEntry("bob@qxmpp.org").subscriptionStatus(), u"subscribe"_s);

    // rename bob
    bool requestSent = false;
    connect(&logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            requestSent = true;

            QXmppRosterIq renameRequest;
            parsePacket(renameRequest, text.toUtf8());
            QCOMPARE(renameRequest.items().size(), 1);
            QCOMPARE(renameRequest.items().first().bareJid(), u"bob@qxmpp.org"_s);
            QCOMPARE(renameRequest.items().first().name(), u"Bob"_s);
            // check that subscription state ('ask') for bob is not included
            QVERIFY(renameRequest.items().first().subscriptionStatus().isNull());
        }
    });

    manager->renameItem("bob@qxmpp.org", "Bob");
    QVERIFY(requestSent);
}

void tst_QXmppRosterManager::testSubscriptionRequestReceived()
{
    QXmppPresence presence;
    presence.setType(QXmppPresence::Subscribe);
    presence.setFrom(u"alice@example.org/notebook"_s);
    presence.setStatusText(u"Hi, I'm Alice."_s);

    bool subscriptionRequestReceived = false;

    connect(manager, &QXmppRosterManager::subscriptionRequestReceived, this, [&](const QString &subscriberBareJid, const QXmppPresence &presence) {
        subscriptionRequestReceived = true;

        QCOMPARE(subscriberBareJid, u"alice@example.org"_s);
        QCOMPARE(presence.statusText(), u"Hi, I'm Alice."_s);
    });

    Q_EMIT client.presenceReceived(presence);
    QVERIFY(subscriptionRequestReceived);
}

void tst_QXmppRosterManager::testMovedSubscriptionRequestReceived_data()
{
    QTest::addColumn<bool>("movedManagerAdded");
    QTest::addColumn<QString>("oldJid");
    QTest::addColumn<QString>("oldJidResponse");
    QTest::addColumn<bool>("valid");

    QTest::newRow("noMovedManagerNoJid")
        << false
        << QString()
        << QString()
        << false;
    QTest::newRow("noMovedManagerJid")
        << false
        << u"old@example.org"_s
        << QString()
        << false;
    QTest::newRow("oldJidEmpty")
        << true
        << QString()
        << QString()
        << false;
    QTest::newRow("oldJidNotInRoster")
        << true
        << u"old-invalid@example.org"_s
        << QString()
        << false;
    QTest::newRow("oldJidRespondingWithError")
        << true
        << u"old@example.org"_s
        << u"<iq id='qx1' from='old@example.org' type='error'>"
           u"<error type='cancel'>"
           u"<not-allowed xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
           u"</error>"
           u"</iq>"_s
        << false;
    QTest::newRow("oldJidValid")
        << true
        << u"old@example.org"_s
        << u"<iq id='qx1' from='old@example.org' type='result'>"
           "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<items node='urn:xmpp:moved:1'>"
           "<item id='current'>"
           "<moved xmlns='urn:xmpp:moved:1'>"
           "<new-jid>new@example.org</new-jid>"
           "</moved>"
           "</item>"
           "</items>"
           "</pubsub>"
           "</iq>"_s
        << true;
}

void tst_QXmppRosterManager::testMovedSubscriptionRequestReceived()
{
    TestClient client;
    client.configuration().setJid(u"alice@example.org"_s);
    auto *rosterManager = client.addNewExtension<QXmppRosterManager>(&client);

    QFETCH(bool, movedManagerAdded);
    QFETCH(QString, oldJid);
    QFETCH(QString, oldJidResponse);
    QFETCH(bool, valid);

    if (movedManagerAdded) {
        client.addNewExtension<QXmppDiscoveryManager>();
        client.addNewExtension<QXmppPubSubManager>();
        client.addNewExtension<QXmppMovedManager>();

        QXmppRosterIq::Item rosterItem;
        rosterItem.setBareJid(u"old@example.org"_s);
        rosterItem.setSubscriptionType(QXmppRosterIq::Item::SubscriptionType::Both);

        QXmppRosterIq rosterIq;
        rosterIq.setType(QXmppIq::Set);
        rosterIq.setItems({ rosterItem });
        rosterManager->handleStanza(writePacketToDom(rosterIq));
    }

    QXmppPresence presence;
    presence.setType(QXmppPresence::Subscribe);
    presence.setFrom(u"new@example.org/notebook"_s);
    presence.setOldJid(oldJid);

    bool subscriptionRequestReceived = false;
    client.resetIdCount();

    connect(rosterManager, &QXmppRosterManager::subscriptionRequestReceived, this, [&](const QString &subscriberBareJid, const QXmppPresence &presence) {
        subscriptionRequestReceived = true;
        QCOMPARE(subscriberBareJid, u"new@example.org"_s);
        if (valid && movedManagerAdded) {
            QCOMPARE(oldJid, presence.oldJid());
        } else {
            QVERIFY(presence.oldJid().isEmpty());
        }
    });

    Q_EMIT client.presenceReceived(presence);

    if (!oldJidResponse.isEmpty()) {
        client.inject(oldJidResponse);
    }

    QVERIFY(subscriptionRequestReceived);
}

void tst_QXmppRosterManager::testAddItem()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    auto future = rosterManager->addRosterItem("contact@example.org");
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org'/></query></iq>");
    test.inject<QString>("<iq id='qx1' type='result'/>");
    expectFutureVariant<QXmpp::Success>(future);

    future = rosterManager->addRosterItem("contact@example.org");
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org'/></query></iq>");
    test.inject<QString>(R"(
<iq id='qx1' type='error'>
    <error type='modify'>
        <not-authorized xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>
        <text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>This is not allowed</text>
    </error>
</iq>)");
    auto err = expectFutureVariant<QXmppError>(future);
    auto error = err.value<QXmppStanza::Error>().value();
    QCOMPARE(error.type(), QXmppStanza::Error::Modify);
    QCOMPARE(error.text(), u"This is not allowed"_s);
}

void tst_QXmppRosterManager::testRemoveItem()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    auto future = rosterManager->removeRosterItem("contact@example.org");
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org' subscription='remove'/></query></iq>");
    test.inject<QString>("<iq id='qx1' type='result'/>");
    expectFutureVariant<QXmpp::Success>(future);

    future = rosterManager->removeRosterItem("contact@example.org");
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org' subscription='remove'/></query></iq>");
    test.inject<QString>(R"(
<iq id='qx1' type='error'>
    <error type='cancel'>
        <item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>
        <text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>Not found</text>
    </error>
</iq>)");
    auto err = expectFutureVariant<QXmppError>(future);
    auto error = err.value<QXmppStanza::Error>().value();
    QCOMPARE(error.type(), QXmppStanza::Error::Cancel);
    QCOMPARE(error.text(), u"Not found"_s);
}

void tst_QXmppRosterManager::testUpdateGroups()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    // seed the roster with a contact that has a name (to verify it is preserved);
    // a roster push (type set) is needed, since handleStanza() only stores those
    QXmppRosterIq::Item item;
    item.setBareJid(u"contact@example.org"_s);
    item.setName(u"Romeo"_s);
    QXmppRosterIq initialItems;
    initialItems.setType(QXmppIq::Set);
    initialItems.addItem(item);
    QVERIFY(rosterManager->handleStanza(writePacketToDom(initialItems)));
    // discard the result IQ the manager sends in response to the push
    test.ignore();

    // success: groups are replaced and the name is preserved
    auto future = rosterManager->updateRosterGroups(u"contact@example.org"_s, { u"Friends"_s });
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org' name='Romeo'><group>Friends</group></item></query></iq>");
    test.inject<QString>("<iq id='qx1' type='result'/>");
    expectFutureVariant<QXmpp::Success>(future);

    // unknown JID: ready error, no IQ sent
    auto missing = rosterManager->updateRosterGroups(u"stranger@example.org"_s, { u"Friends"_s });
    test.expectNoPacket();
    expectFutureVariant<QXmppError>(missing);

    // error response
    future = rosterManager->updateRosterGroups(u"contact@example.org"_s, { u"Family"_s });
    test.expect("<iq id='qx1' type='set'><query xmlns='jabber:iq:roster'><item jid='contact@example.org' name='Romeo'><group>Family</group></item></query></iq>");
    test.inject<QString>(R"(
<iq id='qx1' type='error'>
    <error type='modify'>
        <not-authorized xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>
        <text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>This is not allowed</text>
    </error>
</iq>)");
    auto err = expectFutureVariant<QXmppError>(future);
    auto error = err.value<QXmppStanza::Error>().value();
    QCOMPARE(error.type(), QXmppStanza::Error::Modify);
    QCOMPARE(error.text(), u"This is not allowed"_s);
}

void tst_QXmppRosterManager::testDefaultStorage()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    // The manager wires up a default in-memory storage automatically.
    QVERIFY(rosterManager->storage() != nullptr);
}

void tst_QXmppRosterManager::testSetStorage()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    auto external = std::make_unique<QXmppRosterMemoryStorage>();
    auto *externalPtr = external.get();
    rosterManager->setStorage(std::move(external));
    QCOMPARE(rosterManager->storage(), externalPtr);

    // Passing an empty unique_ptr restores the internal default.
    rosterManager->setStorage(nullptr);
    QVERIFY(rosterManager->storage() != nullptr);
}

void tst_QXmppRosterManager::testPushPersistsAdd()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    rosterManager->setStorage(std::make_unique<QXmppRosterMemoryStorage>());

    // Inject a roster-set push carrying a new version.
    QXmppRosterIq::Item item;
    item.setBareJid(u"alice@example.org"_s);
    item.setName(u"Alice"_s);
    QXmppRosterIq push;
    push.setType(QXmppIq::Set);
    push.setVersion(u"v42"_s);
    push.addItem(item);

    QVERIFY(rosterManager->handleStanza(writePacketToDom(push)));

    auto cache = rosterManager->storage()->load().takeResult();
    QCOMPARE(cache.version, u"v42"_s);
    QCOMPARE(cache.items.size(), 1u);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice"_s);
}

void tst_QXmppRosterManager::testPushPersistsRemove()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    rosterManager->setStorage(std::make_unique<QXmppRosterMemoryStorage>());

    // First push: add Alice.
    QXmppRosterIq::Item item;
    item.setBareJid(u"alice@example.org"_s);
    QXmppRosterIq add;
    add.setType(QXmppIq::Set);
    add.setVersion(u"v1"_s);
    add.addItem(item);
    QVERIFY(rosterManager->handleStanza(writePacketToDom(add)));

    // Second push: remove Alice with a new version.
    QXmppRosterIq::Item removed;
    removed.setBareJid(u"alice@example.org"_s);
    removed.setSubscriptionType(QXmppRosterIq::Item::Remove);
    QXmppRosterIq remove;
    remove.setType(QXmppIq::Set);
    remove.setVersion(u"v2"_s);
    remove.addItem(removed);
    QVERIFY(rosterManager->handleStanza(writePacketToDom(remove)));

    auto cache = rosterManager->storage()->load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QVERIFY(cache.items.empty());
}

void tst_QXmppRosterManager::testClearCache()
{
    TestClient test;
    test.configuration().setJid(u"juliet@capulet.lit"_s);
    auto *rosterManager = test.addNewExtension<QXmppRosterManager>(&test);

    rosterManager->setStorage(std::make_unique<QXmppRosterMemoryStorage>());

    QXmppRosterIq::Item item;
    item.setBareJid(u"alice@example.org"_s);
    QXmppRosterIq push;
    push.setType(QXmppIq::Set);
    push.setVersion(u"v1"_s);
    push.addItem(item);
    QVERIFY(rosterManager->handleStanza(writePacketToDom(push)));

    auto task = rosterManager->clearCache();
    QVERIFY(task.isFinished());

    QVERIFY(rosterManager->getRosterBareJids().isEmpty());
    auto cache = rosterManager->storage()->load().takeResult();
    QVERIFY(cache.version.isEmpty());
    QVERIFY(cache.items.empty());
}

void tst_QXmppRosterManager::testRosterIqVerSerialization()
{
    // No version set: ver attribute is omitted entirely.
    {
        QXmppRosterIq iq;
        iq.setType(QXmppIq::Get);
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        iq.toXml(&writer);
        QVERIFY(!xml.contains("ver="));
    }

    // Explicit empty ver (RFC 6121 §2.6 support advertisement): emit ver="".
    {
        QXmppRosterIq iq;
        iq.setType(QXmppIq::Get);
        iq.setVersion(QString());
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        iq.toXml(&writer);
        QVERIFY(xml.contains("ver=\"\""));
    }

    // Explicit non-empty ver: emit ver="abc".
    {
        QXmppRosterIq iq;
        iq.setType(QXmppIq::Get);
        iq.setVersion(u"abc"_s);
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        iq.toXml(&writer);
        QVERIFY(xml.contains("ver=\"abc\""));
    }

    // Parse: an empty IQ result without <query> sets hasQuery() == false.
    {
        QXmppRosterIq iq;
        parsePacket(iq, "<iq id='qx1' type='result'/>");
        QVERIFY(!iq.hasQuery());
        QVERIFY(!iq.versionOpt().has_value());
    }

    // Parse: an IQ result with <query/> sets hasQuery() == true.
    {
        QXmppRosterIq iq;
        parsePacket(iq, "<iq id='qx1' type='result'><query xmlns='jabber:iq:roster' ver='abc'/></iq>");
        QVERIFY(iq.hasQuery());
        QCOMPARE(iq.versionOpt(), std::optional { u"abc"_s });
    }
}

}  // namespace Roster

// ============================================================

namespace RosterMemoryStorage {

using namespace QXmpp::Private;

class tst_QXmppRosterMemoryStorage : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testEmpty();
    Q_SLOT void testReplaceAll();
    Q_SLOT void testUpsert();
    Q_SLOT void testRemove();
    Q_SLOT void testClear();

    static QXmppRosterIq::Item makeItem(const QString &jid, const QString &name = {})
    {
        QXmppRosterIq::Item item;
        item.setBareJid(jid);
        if (!name.isEmpty()) {
            item.setName(name);
        }
        return item;
    }
};

void tst_QXmppRosterMemoryStorage::testEmpty()
{
    QXmppRosterMemoryStorage storage;
    auto future = storage.load();
    QVERIFY(future.isFinished());
    auto cache = future.takeResult();
    QVERIFY(cache.version.isEmpty());
    QVERIFY(cache.items.empty());
}

void tst_QXmppRosterMemoryStorage::testReplaceAll()
{
    QXmppRosterMemoryStorage storage;
    const std::vector<QXmppRosterIq::Item> items = {
        makeItem(u"alice@example.org"_s, u"Alice"_s),
        makeItem(u"bob@example.org"_s, u"Bob"_s),
    };
    auto setFuture = storage.replaceAll(u"v1"_s, items);
    QVERIFY(setFuture.isFinished());

    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v1"_s);
    QCOMPARE(cache.items.size(), 2u);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice"_s);
    QCOMPARE(find(cache.items, u"bob@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Bob"_s);

    // A second replaceAll wipes the prior set.
    storage.replaceAll(u"v2"_s, { makeItem(u"carol@example.org"_s) });
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QVERIFY(find(cache.items, u"carol@example.org"_s, &QXmppRosterIq::Item::bareJid).has_value());
}

void tst_QXmppRosterMemoryStorage::testUpsert()
{
    QXmppRosterMemoryStorage storage;
    storage.upsertItem(u"v1"_s, makeItem(u"alice@example.org"_s, u"Alice"_s));
    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v1"_s);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice"_s);

    // Second upsert with same JID replaces the prior value and advances the version.
    storage.upsertItem(u"v2"_s, makeItem(u"alice@example.org"_s, u"Alice in Wonderland"_s));
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice in Wonderland"_s);
}

void tst_QXmppRosterMemoryStorage::testRemove()
{
    QXmppRosterMemoryStorage storage;
    storage.replaceAll(u"v1"_s, { makeItem(u"alice@example.org"_s), makeItem(u"bob@example.org"_s) });

    storage.removeItem(u"v2"_s, u"alice@example.org"_s);
    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QVERIFY(!find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid).has_value());

    // Removing a non-existing JID is idempotent but still advances the version.
    storage.removeItem(u"v3"_s, u"nonexistent@example.org"_s);
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v3"_s);
    QCOMPARE(cache.items.size(), 1u);
}

void tst_QXmppRosterMemoryStorage::testClear()
{
    QXmppRosterMemoryStorage storage;
    storage.replaceAll(u"v1"_s, { makeItem(u"alice@example.org"_s) });

    storage.clear();
    auto cache = storage.load().takeResult();
    QVERIFY(cache.version.isEmpty());
    QVERIFY(cache.items.empty());
}

}  // namespace RosterMemoryStorage

// ============================================================

namespace DiscoveryIq {

class tst_QXmppDiscoveryIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void discovery();
    Q_SLOT void discoveryWithForm();
    Q_SLOT void discoInfo();
    Q_SLOT void discoItems();
    Q_SLOT void discoContactAddresses();
};

void tst_QXmppDiscoveryIq::discovery()
{
    const QByteArray xml(
        "<iq id=\"disco1\" from=\"benvolio@capulet.lit/230193\" type=\"result\">"
        "<query xmlns=\"http://jabber.org/protocol/disco#info\">"
        "<identity category=\"client\" name=\"Exodus 0.9.1\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "</query>"
        "</iq>");

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppDiscoveryIq disco;
    QT_WARNING_POP
    parsePacket(disco, xml);
    QCOMPARE(disco.verificationString(), QByteArray::fromBase64("QgayPKawpkPSDYmwT/WM94uAlu0="));
    serializePacket(disco, xml);
}

void tst_QXmppDiscoveryIq::discoveryWithForm()
{
    const QByteArray xml(
        "<iq id=\"disco1\" to=\"juliet@capulet.lit/chamber\" from=\"benvolio@capulet.lit/230193\" type=\"result\">"
        "<query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"http://psi-im.org#q07IKJEyjvHSyhy//CH0CxmKi8w=\">"
        "<identity xml:lang=\"en\" category=\"client\" name=\"Psi 0.11\" type=\"pc\"/>"
        "<identity xml:lang=\"el\" category=\"client\" name=\"Ψ 0.11\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "<x xmlns=\"jabber:x:data\" type=\"result\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>urn:xmpp:dataforms:softwareinfo</value>"
        "</field>"
        "<field type=\"text-multi\" var=\"ip_version\">"
        "<value>ipv4</value>"
        "<value>ipv6</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os\">"
        "<value>Mac</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os_version\">"
        "<value>10.5.1</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software\">"
        "<value>Psi</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software_version\">"
        "<value>0.11</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppDiscoveryIq disco;
    QT_WARNING_POP
    parsePacket(disco, xml);
    QCOMPARE(disco.verificationString(), QByteArray::fromBase64("q07IKJEyjvHSyhy//CH0CxmKi8w="));
    serializePacket(disco, xml);

    auto softinfoForm = disco.dataForm(u"urn:xmpp:dataforms:softwareinfo");
    QVERIFY(softinfoForm.has_value());
}

void tst_QXmppDiscoveryIq::discoInfo()
{
    const auto xml = QByteArrayLiteral(
        "<query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"http://psi-im.org#q07IKJEyjvHSyhy//CH0CxmKi8w=\">"
        "<identity xml:lang=\"en\" category=\"client\" name=\"Psi 0.11\" type=\"pc\"/>"
        "<identity xml:lang=\"el\" category=\"client\" name=\"Ψ 0.11\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "<x xmlns=\"jabber:x:data\" type=\"result\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>urn:xmpp:dataforms:softwareinfo</value>"
        "</field>"
        "<field type=\"text-multi\" var=\"ip_version\">"
        "<value>ipv4</value>"
        "<value>ipv6</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os\">"
        "<value>Mac</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os_version\">"
        "<value>10.5.1</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software\">"
        "<value>Psi</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software_version\">"
        "<value>0.11</value>"
        "</field>"
        "</x>"
        "</query>");

    auto info = unwrap(QXmppDiscoInfo::fromDom(xmlToDom(xml)));
    QCOMPARE(info.calculateEntityCapabilitiesHash(), QByteArray::fromBase64("q07IKJEyjvHSyhy//CH0CxmKi8w="));
    serializePacket(info, xml);
}

void tst_QXmppDiscoveryIq::discoItems()
{
    const auto xml = QByteArrayLiteral(
        "<query xmlns='http://jabber.org/protocol/disco#items'>"
        "<item jid='368866411b877c30064a5f62b917cffe@test.org'/>"
        "<item jid='3300659945416e274474e469a1f0154c@test.org'/>"
        "<item jid='4e30f35051b7b8b42abe083742187228@test.org'/>"
        "<item jid='ae890ac52d0df67ed7cfdf51b644e901@test.org'/>"
        "</query>");

    auto items = unwrap(QXmppDiscoItems::fromDom(xmlToDom(xml)));
    QCOMPARE(items.items().size(), 4);
    QCOMPARE(items.items().at(0).jid(), u"368866411b877c30064a5f62b917cffe@test.org");
    serializePacket(items, xml);
}

void tst_QXmppDiscoveryIq::discoContactAddresses()
{
    auto xml = QByteArrayLiteral(
        "<x xmlns='jabber:x:data' type='result'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>http://jabber.org/network/serverinfo</value>"
        "</field>"
        "<field type='list-multi' var='abuse-addresses'>"
        "<value>mailto:abuse@shakespeare.lit</value>"
        "<value>xmpp:abuse@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='admin-addresses'>"
        "<value>mailto:xmpp@shakespeare.lit</value>"
        "<value>xmpp:admins@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='feedback-addresses'>"
        "<value>http://shakespeare.lit/feedback.php</value>"
        "<value>mailto:feedback@shakespeare.lit</value>"
        "<value>xmpp:feedback@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='sales-addresses'>"
        "<value>xmpp:bard@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='security-addresses'>"
        "<value>xmpp:security@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='status-addresses'>"
        "<value>https://status.shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='support-addresses'>"
        "<value>http://shakespeare.lit/support.php</value>"
        "<value>xmpp:support@shakespeare.lit</value>"
        "</field>"
        "</x>");

    QXmppDataForm form;
    parsePacket(form, xml);

    auto parsed = QXmppContactAddresses::fromDataForm(form);
    QVERIFY(parsed.has_value());

    QCOMPARE(parsed->abuseAddresses(), (QStringList { u"mailto:abuse@shakespeare.lit"_s, u"xmpp:abuse@shakespeare.lit"_s }));

    form = parsed->toDataForm();
    form.setType(QXmppDataForm::Result);
    QVERIFY(!form.isNull());
    xml = QString::fromUtf8(xml).remove(QChar('\n')).toUtf8();
    serializePacket(form, xml);

    // findForm with parsing
    QXmppDiscoInfo info;
    info.setDataForms({ parsed->toDataForm() });
    auto contactAddresses = info.dataForm<QXmppContactAddresses>();
    QVERIFY(contactAddresses);
    QCOMPARE(contactAddresses->supportAddresses().constFirst(), u"http://shakespeare.lit/support.php");
}

}  // namespace DiscoveryIq

// ============================================================

namespace RosterIq {

class tst_QXmppRosterIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void rosterItem_data();
    Q_SLOT void rosterItem();
    Q_SLOT void rosterApproved_data();
    Q_SLOT void rosterApproved();
    Q_SLOT void rosterVersion_data();
    Q_SLOT void rosterVersion();
    Q_SLOT void rosterMixAnnotate();
    Q_SLOT void rosterMixChannel();
};

void tst_QXmppRosterIq::rosterItem_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("subscriptionStatus");
    QTest::addColumn<int>("subscriptionType");
    QTest::addColumn<bool>("approved");

    QTest::newRow("none")
        << QByteArray(R"(<item jid="foo@example.com" subscription="none" approved="true"/>)")
        << ""
        << ""
        << int(QXmppRosterIq::Item::None)
        << true;
    QTest::newRow("from")
        << QByteArray(R"(<item jid="foo@example.com" subscription="from"/>)")
        << ""
        << ""
        << int(QXmppRosterIq::Item::From)
        << false;
    QTest::newRow("to")
        << QByteArray(R"(<item jid="foo@example.com" subscription="to"/>)")
        << ""
        << ""
        << int(QXmppRosterIq::Item::To)
        << false;
    QTest::newRow("both")
        << QByteArray(R"(<item jid="foo@example.com" subscription="both"/>)")
        << ""
        << ""
        << int(QXmppRosterIq::Item::Both)
        << false;
    QTest::newRow("remove")
        << QByteArray(R"(<item jid="foo@example.com" subscription="remove"/>)")
        << ""
        << ""
        << int(QXmppRosterIq::Item::Remove)
        << false;
    QTest::newRow("notset")
        << QByteArray("<item jid=\"foo@example.com\"/>")
        << ""
        << ""
        << int(QXmppRosterIq::Item::NotSet)
        << false;

    QTest::newRow("ask-subscribe")
        << QByteArray("<item jid=\"foo@example.com\" ask=\"subscribe\"/>")
        << ""
        << "subscribe"
        << int(QXmppRosterIq::Item::NotSet)
        << false;
    QTest::newRow("ask-unsubscribe")
        << QByteArray("<item jid=\"foo@example.com\" ask=\"unsubscribe\"/>")
        << ""
        << "unsubscribe"
        << int(QXmppRosterIq::Item::NotSet)
        << false;

    QTest::newRow("name")
        << QByteArray(R"(<item jid="foo@example.com" name="foo bar"/>)")
        << "foo bar"
        << ""
        << int(QXmppRosterIq::Item::NotSet)
        << false;
}

void tst_QXmppRosterIq::rosterItem()
{
    QFETCH(QByteArray, xml);
    QFETCH(QString, name);
    QFETCH(QString, subscriptionStatus);
    QFETCH(int, subscriptionType);
    QFETCH(bool, approved);

    QXmppRosterIq::Item item;
    parsePacket(item, xml);
    QCOMPARE(item.bareJid(), QLatin1String("foo@example.com"));
    QCOMPARE(item.groups(), QSet<QString>());
    QCOMPARE(item.name(), name);
    QCOMPARE(item.subscriptionStatus(), subscriptionStatus);
    QCOMPARE(int(item.subscriptionType()), subscriptionType);
    QCOMPARE(item.isApproved(), approved);
    serializePacket(item, xml);

    item = QXmppRosterIq::Item();
    item.setBareJid("foo@example.com");
    item.setName(name);
    item.setSubscriptionStatus(subscriptionStatus);
    item.setSubscriptionType(QXmppRosterIq::Item::SubscriptionType(subscriptionType));
    item.setIsApproved(approved);
    serializePacket(item, xml);
}

void tst_QXmppRosterIq::rosterApproved_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("approved");

    QTest::newRow("true") << QByteArray(R"(<item jid="foo@example.com" approved="true"/>)") << true;
    QTest::newRow("1") << QByteArray(R"(<item jid="foo@example.com" approved="1"/>)") << true;
    QTest::newRow("false") << QByteArray(R"(<item jid="foo@example.com" approved="false"/>)") << false;
    QTest::newRow("0") << QByteArray(R"(<item jid="foo@example.com" approved="0"/>)") << false;
    QTest::newRow("empty") << QByteArray(R"(<item jid="foo@example.com"/>)") << false;
}

void tst_QXmppRosterIq::rosterApproved()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, approved);

    QXmppRosterIq::Item item;
    parsePacket(item, xml);
    QCOMPARE(item.isApproved(), approved);
}

void tst_QXmppRosterIq::rosterVersion_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QString>("version");

    QTest::newRow("noversion")
        << QByteArray(R"(<iq id="woodyisacat" to="woody@zam.tw/cat" type="result"><query xmlns="jabber:iq:roster"/></iq>)")
        << "";

    QTest::newRow("version")
        << QByteArray(R"(<iq id="woodyisacat" to="woody@zam.tw/cat" type="result"><query xmlns="jabber:iq:roster" ver="3345678"/></iq>)")
        << "3345678";
}

void tst_QXmppRosterIq::rosterVersion()
{
    QFETCH(QByteArray, xml);
    QFETCH(QString, version);

    QXmppRosterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.versionOpt().value_or(QString()), version);
    serializePacket(iq, xml);
}

void tst_QXmppRosterIq::rosterMixAnnotate()
{
    const QByteArray xml(
        "<iq id='1' from=\"juliet@example.com/balcony\" "
        "type=\"get\">"
        "<query xmlns=\"jabber:iq:roster\">"
        "<annotate xmlns=\"urn:xmpp:mix:roster:0\"/>"
        "</query>"
        "</iq>");

    QXmppRosterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.mixAnnotate(), true);
    serializePacket(iq, xml);

    iq.setMixAnnotate(false);
    QCOMPARE(iq.mixAnnotate(), false);
}

void tst_QXmppRosterIq::rosterMixChannel()
{
    const QByteArray xml(
        "<item jid=\"balcony@example.net\">"
        "<channel xmlns=\"urn:xmpp:mix:roster:0\" participant-id=\"123456\"/>"
        "</item>");

    QXmppRosterIq::Item item;
    parsePacket(item, xml);
    QCOMPARE(item.isMixChannel(), true);
    QCOMPARE(item.mixParticipantId(), u"123456"_s);
    serializePacket(item, xml);

    item.setIsMixChannel(false);
    QCOMPARE(item.isMixChannel(), false);
    item.setMixParticipantId("23a7n");
    QCOMPARE(item.mixParticipantId(), u"23a7n"_s);
}

}  // namespace RosterIq

QXMPP_TEST_MAIN(Client::tst_QXmppClient, Discovery::tst_QXmppDiscoveryManager, Roster::tst_QXmppRosterManager, RosterMemoryStorage::tst_QXmppRosterMemoryStorage, DiscoveryIq::tst_QXmppDiscoveryIq, RosterIq::tst_QXmppRosterIq)

#include "tst_QXmppClientBase.moc"

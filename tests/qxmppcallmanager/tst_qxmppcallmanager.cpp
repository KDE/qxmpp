// SPDX-FileCopyrightText: 2015 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppCallManager.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppServer.h"

#include "Async.h"
#include "TestClient.h"
#include "util.h"

#include <QBuffer>

using namespace QXmpp::Private;
using Error = QXmppStanza::Error;

class tst_QXmppCallManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void callInvalidJid();
    Q_SLOT void invalidSid();
    Q_SLOT void senderImpersonation();
    Q_SLOT void testCall();
};

void tst_QXmppCallManager::callInvalidJid()
{
    TestClient client;
    client.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = client.addNewExtension<QXmppCallManager>();

    QXmppCall *call = manager->call(QString());
    QVERIFY(!call);

    call = manager->call("test@localhost/r1");
    QVERIFY(call);
    QCOMPARE(call->sid(), QString());
    QCOMPARE(call->jid(), u"test@localhost/r1");
    QCOMPARE(call->direction(), QXmppCall::OutgoingDirection);
}

void tst_QXmppCallManager::invalidSid()
{
    const auto xml =
        u"<iq from='romeo@montague.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='session-initiate' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='96' name='speex' clockrate='16000' />"
        "<payload-type id='97' name='speex' clockrate='8000' />"
        "<payload-type id='18' name='G729' />"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "<payload-type id='103' name='L16' clockrate='16000' channels='2' />"
        "<payload-type id='98' name='x-ISAC' clockrate='8000' />"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;

    TestClient client;
    auto *manager = client.addNewExtension<QXmppCallManager>();

    QVERIFY(manager->handleStanza(xmlToDom(xml.arg("abc1"))));
    QCoreApplication::processEvents();
    client.expect(u"<iq id='ph37a419' to='romeo@montague.lit/orchard' type='result'/>"_s);
    client.expect(u"<iq id='qxmpp3' to='romeo@montague.lit/orchard' type='set'><jingle xmlns='urn:xmpp:jingle:1' action='session-info' sid='abc1'><ringing xmlns='urn:xmpp:jingle:apps:rtp:info:1'/></jingle></iq>"_s);
    // same sid
    auto error = expectVariant<Error>(manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml.arg("abc1")))));
    QCOMPARE(error.type(), Error::Cancel);
    QCOMPARE(error.condition(), Error::Conflict);
}

void tst_QXmppCallManager::senderImpersonation()
{
    const auto xml =
        u"<iq from='romeo@montague.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='session-initiate' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='96' name='speex' clockrate='16000' />"
        "<payload-type id='97' name='speex' clockrate='8000' />"
        "<payload-type id='18' name='G729' />"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "<payload-type id='103' name='L16' clockrate='16000' channels='2' />"
        "<payload-type id='98' name='x-ISAC' clockrate='8000' />"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;

    TestClient client;
    auto *manager = client.addNewExtension<QXmppCallManager>();

    // session initiate
    auto result = manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml.arg("abc1"))));
    QVERIFY(std::holds_alternative<QXmppIq>(result));

    // other JID trying to inject IQs into our call (different 'from')
    const auto xml2 =
        u"<iq from='r0me0@m0ntagu3.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='content-add' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;
    result = manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml2.arg("abc1"))));
    auto error = expectVariant<Error>(std::move(result));
    QCOMPARE(error.type(), Error::Cancel);
    QCOMPARE(error.condition(), Error::ItemNotFound);

    // manager makes use of later() calls
    QCoreApplication::processEvents();
}

void tst_QXmppCallManager::testCall()
{
    if (!qEnvironmentVariableIsEmpty("QXMPP_TESTS_SKIP_CALL_MANAGER")) {
        QSKIP("Skipping because 'QXMPP_TESTS_SKIP_CALL_MANAGER' was set.");
    }

    QXmppCall *receiverCall = nullptr;

    const QString testDomain("localhost");
    const QHostAddress testHost(QHostAddress::LocalHost);
    const quint16 testPort = 12345;

    QXmppLogger logger;
    // logger.setLoggingType(QXmppLogger::StdoutLogging);

    // prepare server
    TestPasswordChecker passwordChecker;
    passwordChecker.addCredentials("sender", "testpwd");
    passwordChecker.addCredentials("receiver", "testpwd");

    QXmppServer server;
    server.setDomain(testDomain);
    server.setPasswordChecker(&passwordChecker);
    server.listenForClients(testHost, testPort);

    // prepare sender
    QXmppClient sender;
    auto *senderManager = new QXmppCallManager;
    sender.addExtension(senderManager);
    sender.setLogger(&logger);

    QEventLoop senderLoop;
    connect(&sender, &QXmppClient::connected, &senderLoop, &QEventLoop::quit);
    connect(&sender, &QXmppClient::disconnected, &senderLoop, &QEventLoop::quit);

    QXmppConfiguration config;
    config.setDomain(testDomain);
    config.setHost(testHost.toString());
    config.setPort(testPort);
    config.setUser("sender");
    config.setPassword("testpwd");
    sender.connectToServer(config);
    senderLoop.exec();
    QCOMPARE(sender.isConnected(), true);

    // prepare receiver
    QXmppClient receiver;
    auto *receiverManager = new QXmppCallManager;
    connect(receiverManager, &QXmppCallManager::callAdded, this, [&receiverCall](QXmppCall *call) {
        receiverCall = call;
        call->accept();
    });
    receiver.addExtension(receiverManager);
    receiver.setLogger(&logger);

    QEventLoop receiverLoop;
    connect(&receiver, &QXmppClient::connected, &receiverLoop, &QEventLoop::quit);
    connect(&receiver, &QXmppClient::disconnected, &receiverLoop, &QEventLoop::quit);

    config.setUser("receiver");
    config.setPassword("testpwd");
    receiver.connectToServer(config);
    receiverLoop.exec();
    QCOMPARE(receiver.isConnected(), true);

    // connect call
    qDebug() << "======== CONNECT ========";
    QEventLoop loop;
    QXmppCall *senderCall = senderManager->call(receiver.configuration().jid());
    QVERIFY(senderCall);
    connect(senderCall, &QXmppCall::connected, &loop, &QEventLoop::quit);
    loop.exec();
    QVERIFY(receiverCall);

    QCOMPARE(senderCall->direction(), QXmppCall::OutgoingDirection);
    QCOMPARE(senderCall->state(), QXmppCall::ActiveState);

    QCOMPARE(receiverCall->direction(), QXmppCall::IncomingDirection);
    QCOMPARE(receiverCall->state(), QXmppCall::ActiveState);

    // exchange some media
    qDebug() << "======== TALK ========";
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    // hangup call
    qDebug() << "======== HANGUP ========";
    connect(senderCall, &QXmppCall::finished, &loop, &QEventLoop::quit);
    senderCall->hangup();
    loop.exec();

    QCOMPARE(senderCall->direction(), QXmppCall::OutgoingDirection);
    QCOMPARE(senderCall->state(), QXmppCall::FinishedState);

    QCOMPARE(receiverCall->direction(), QXmppCall::IncomingDirection);
    QCOMPARE(receiverCall->state(), QXmppCall::FinishedState);
}

QTEST_MAIN(tst_QXmppCallManager)
#include "tst_qxmppcallmanager.moc"

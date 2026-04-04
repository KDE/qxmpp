// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppDiscoveryManager.h"

#include "TestClient.h"

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

    discoManager->handleStanza(xmlToDom(R"(
<iq type='get' from='romeo@montague.net/orchard' to='user@qxmpp.org/a' id='info1'>
  <query xmlns='http://jabber.org/protocol/disco#info'/>
</iq>)"));

    test.expect(
        "<iq id='info1' to='romeo@montague.net/orchard' type='result'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'>"
        "<identity category='client' name='tst_qxmppdiscoverymanager' type='pc'/>"
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

QTEST_MAIN(tst_QXmppDiscoveryManager)

#include "tst_qxmppdiscoverymanager.moc"

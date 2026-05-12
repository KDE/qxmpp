// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMovedManager.h"
#include "QXmppPubSubManager.h"
#include "QXmppRosterManager.h"
#include "QXmppRosterMemoryStorage.h"
#include "QXmppRosterStorage.h"

#include "Algorithms.h"
#include "TestClient.h"

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

QTEST_MAIN(tst_QXmppRosterManager)
#include "tst_QXmppRosterManager.moc"

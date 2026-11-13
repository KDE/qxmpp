// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppAsync_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeProvider.h"
#include "QXmppMessageContext.h"
#include "QXmppMessages.h"
#include "QXmppMixInfoItem.h"
#include "QXmppMixInvitation.h"
#include "QXmppMixIq.h"
#include "QXmppMixManager.h"
#include "QXmppMixParticipantItem.h"
#include "QXmppMucManagerV2.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppRosterManager.h"

#include "TestClient.h"

struct Tester {
    Tester()
    {
        client.addNewExtension<QXmppDiscoveryManager>();
        client.addNewExtension<QXmppPubSubManager>();
        manager = client.addNewExtension<QXmppMixManager>();
    }

    Tester(const QString &jid)
        : Tester()
    {
        client.configuration().setJid(jid);
    }

    TestClient client;
    QXmppMixManager *manager;
};

class tst_QXmppMixManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testDiscoveryFeatures();
    Q_SLOT void testWatchSupport();
    Q_SLOT void testSupportReconnection();
    Q_SLOT void testService();
    Q_SLOT void testServiceDiscovery();
    Q_SLOT void testUpdateSupport_data();
    Q_SLOT void testUpdateSupport();
    Q_SLOT void testAddJidToNode();
    Q_SLOT void testRequestJids();
    Q_SLOT void testJoinChannelPrivate();
    Q_SLOT void testPrepareJoinIq();
    Q_SLOT void testHandlePubSubEvent();
    Q_SLOT void testCreateChannel();
    Q_SLOT void testCreateChannelWithId();
    Q_SLOT void testRequestChannelJids();
    Q_SLOT void testRequestChannelNodes();
    Q_SLOT void testRequestChannelConfiguration();
    Q_SLOT void testUpdateChannelConfiguration();
    Q_SLOT void testRequestChannelInformation();
    Q_SLOT void testUpdateChannelInformation();
    Q_SLOT void testJoinChannel();
    Q_SLOT void testJoinChannelWithNickname();
    Q_SLOT void testJoinChannelWithNodes();
    Q_SLOT void testJoinChannelViaInvitation();
    Q_SLOT void testJoinChannelViaInvitationWithNickname();
    Q_SLOT void testJoinChannelViaInvitationWithNodes();
    Q_SLOT void testUpdateNickname();
    Q_SLOT void testUpdateSubscriptions();
    Q_SLOT void testRequestInvitation();
    Q_SLOT void testRequestAllowedJids();
    Q_SLOT void testAllowJid();
    Q_SLOT void testDisallowJid();
    Q_SLOT void testDisallowAllJids();
    Q_SLOT void testRequestBannedJids();
    Q_SLOT void testBanJid();
    Q_SLOT void testUnbanJid();
    Q_SLOT void testUnbanAllJids();
    Q_SLOT void testRequestParticipants();
    Q_SLOT void testLeaveChannel();
    Q_SLOT void testDeleteChannel();
    Q_SLOT void testSendMessageEncrypted();
    Q_SLOT void testSendMessageEncryptedNoChannel();
    Q_SLOT void testSendMessageEncryptedError();
    Q_SLOT void testSendMessageEncryptedHiddenJids();
    Q_SLOT void testResolveSender();
    Q_SLOT void testMucRoomWithChannelJid();

    template<typename T>
    void testErrorFromChannel(QXmppTask<T> &task, TestClient &client);
    template<typename T>
    void testErrorFromChannel(QXmppTask<T> &task, TestClient &client, const QString &id);
    template<typename T>
    void testError(QXmppTask<T> &task, TestClient &client, const QString &id, const QString &from);
};

void tst_QXmppMixManager::testDiscoveryFeatures()
{
    QCOMPARE((addedDiscoveryFeatures<QXmppMixManager, QXmppPubSubManager>()), QStringList { u"urn:xmpp:mix:core:1"_s });
}

// Connects and answers the disco#info request on the own account with the response in which
// "%1" is replaced by the ID of the request.
static void connectAndAnswerSupport(TestClient &client, const QString &response)
{
    client.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT client.connected();

    auto infoId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='hag66@shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    client.expectPacketRandomOrder(
        u"<iq id='qx1' to='shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    client.inject(response.arg(infoId));
}

static QString supportResponse(const QStringList &features)
{
    QString xml = u"<iq id='%1' from='hag66@shakespeare.example' type='result'>"
                  "<query xmlns='http://jabber.org/protocol/disco#info'>"_s;
    for (const auto &feature : features) {
        xml += u"<feature var='" + feature + u"'/>";
    }
    return xml + u"</query></iq>";
}

void tst_QXmppMixManager::testWatchSupport()
{
    QXmppMixManager unregistered;
    QVERIFY(!unregistered.watchParticipantSupport().supported().value());
    QVERIFY(!unregistered.watchMessageArchivingSupport().supported().value());

    auto tester = Tester(u"hag66@shakespeare.example"_s);
    auto participantSupport = tester.manager->watchParticipantSupport();
    auto messageArchivingSupport = tester.manager->watchMessageArchivingSupport();

    connectAndAnswerSupport(tester.client, supportResponse({ u"urn:xmpp:mix:pam:2"_s }));

    QCOMPARE(participantSupport.state().value(), QXmppDiscoInfoWatch::State::Loaded);
    QVERIFY(participantSupport.supported().value());
    QVERIFY(!messageArchivingSupport.supported().value());
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

void tst_QXmppMixManager::testSupportReconnection()
{
    auto tester = Tester(u"hag66@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;
    QSignalSpy participantSpy(manager, &QXmppMixManager::participantSupportChanged);
    QSignalSpy archivingSpy(manager, &QXmppMixManager::messageArchivingSupportChanged);

    QCOMPARE(manager->participantSupport(), QXmppMixManager::Support::Unknown);

    connectAndAnswerSupport(client, supportResponse({ u"urn:xmpp:mix:pam:2"_s }));

    QCOMPARE(manager->participantSupport(), QXmppMixManager::Support::Supported);
    QCOMPARE(manager->messageArchivingSupport(), QXmppMixManager::Support::Unsupported);
    QCOMPARE(participantSpy.size(), 1);
    QCOMPARE(archivingSpy.size(), 1);

    // the values are kept until the new information arrives
    Q_EMIT client.disconnected();
    client.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT client.connected();

    QCOMPARE(manager->participantSupport(), QXmppMixManager::Support::Supported);
    QCOMPARE(manager->messageArchivingSupport(), QXmppMixManager::Support::Unsupported);
    QCOMPARE(participantSpy.size(), 1);
    QCOMPARE(archivingSpy.size(), 1);

    auto infoId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='hag66@shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    client.inject(supportResponse({ u"urn:xmpp:mix:pam:2"_s, u"urn:xmpp:mix:pam:2#archive"_s }).arg(infoId));

    QCOMPARE(manager->participantSupport(), QXmppMixManager::Support::Supported);
    QCOMPARE(manager->messageArchivingSupport(), QXmppMixManager::Support::Supported);
    QCOMPARE(participantSpy.size(), 1);
    QCOMPARE(archivingSpy.size(), 2);

    // unregistering resets the values
    manager->onUnregistered(&client);

    QCOMPARE(manager->participantSupport(), QXmppMixManager::Support::Unknown);
    QCOMPARE(manager->messageArchivingSupport(), QXmppMixManager::Support::Unknown);
    QCOMPARE(participantSpy.size(), 2);
    QCOMPARE(archivingSpy.size(), 3);
}

QT_WARNING_POP

void tst_QXmppMixManager::testService()
{
    QXmppMixManager::Service service1;

    QVERIFY(service1.jid.isEmpty());
    QVERIFY(!service1.channelsSearchable);
    QVERIFY(!service1.channelCreationAllowed);

    service1.jid = u"mix.shakespeare.example"_s;
    service1.channelsSearchable = true;
    service1.channelCreationAllowed = false;

    QXmppMixManager::Service service2;
    service2.jid = u"mix.shakespeare.example"_s;
    service2.channelsSearchable = true;
    service2.channelCreationAllowed = false;

    QCOMPARE(service1, service2);

    QXmppMixManager::Service service3;
    service3.jid = u"mix.shakespeare.example"_s;
    service3.channelsSearchable = true;
    service3.channelCreationAllowed = true;

    QVERIFY(!(service1 == service3));
}

void tst_QXmppMixManager::testServiceDiscovery()
{
    auto tester = Tester(u"hag66@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    QVERIFY(manager->services().isEmpty());

    // Trigger discovery via connect
    client.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT client.connected();

    // updateSupport info query on own JID and items query from disco manager
    auto supportId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='hag66@shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    auto itemsId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);

    // Answer support query (not relevant for this test)
    client.inject(u"<iq id='" + supportId + u"' from='hag66@shakespeare.example' type='result'>"
                                            "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>");
    client.inject(
        u"<iq id='" + itemsId + u"' from='shakespeare.example' type='result'>"
                                "<query xmlns='http://jabber.org/protocol/disco#items'>"
                                "<item jid='mix.shakespeare.example' name='MIX Service'/>"
                                "<item jid='muc.shakespeare.example' name='MUC Service'/>"
                                "</query></iq>");

    auto mixId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='mix.shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    auto mucId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='muc.shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);

    client.inject(
        u"<iq id='" + mixId + u"' from='mix.shakespeare.example' type='result'>"
                              "<query xmlns='http://jabber.org/protocol/disco#info'>"
                              "<identity category='conference' name='MIX Service' type='mix'/>"
                              "<feature var='urn:xmpp:mix:core:1'/>"
                              "<feature var='urn:xmpp:mix:core:1#searchable'/>"
                              "<feature var='urn:xmpp:mix:core:1#create-channel'/>"
                              "</query></iq>");

    client.inject(
        u"<iq id='" + mucId + u"' from='muc.shakespeare.example' type='result'>"
                              "<query xmlns='http://jabber.org/protocol/disco#info'>"
                              "<identity category='conference' name='MUC Service' type='text'/>"
                              "<feature var='http://jabber.org/protocol/muc'/>"
                              "</query></iq>");

    QCOMPARE(manager->services().size(), 1);
    QCOMPARE(manager->services().at(0).jid, u"mix.shakespeare.example"_s);
    QVERIFY(manager->services().at(0).channelsSearchable);
    QVERIFY(manager->services().at(0).channelCreationAllowed);
}

void tst_QXmppMixManager::testUpdateSupport_data()
{
    QTest::addColumn<QString>("response");
    QTest::addColumn<QXmppMixManager::Support>("participantSupport");
    QTest::addColumn<QXmppMixManager::Support>("messageArchivingSupport");

    QTest::newRow("error")
        << u"<iq id='%1' from='hag66@shakespeare.example' type='error'>"
           "<query xmlns='http://jabber.org/protocol/disco#info'/>"
           "<error type='cancel'>"
           "<not-allowed xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
           "</error>"
           "</iq>"_s
        << QXmppMixManager::Support::Unknown
        << QXmppMixManager::Support::Unknown;
    QTest::newRow("bothUnsupported")
        << supportResponse({})
        << QXmppMixManager::Support::Unsupported
        << QXmppMixManager::Support::Unsupported;
    QTest::newRow("participantSupport")
        << supportResponse({ u"urn:xmpp:mix:pam:2"_s })
        << QXmppMixManager::Support::Supported
        << QXmppMixManager::Support::Unsupported;
    QTest::newRow("bothSupported")
        << supportResponse({ u"urn:xmpp:mix:pam:2"_s, u"urn:xmpp:mix:pam:2#archive"_s })
        << QXmppMixManager::Support::Supported
        << QXmppMixManager::Support::Supported;
}

void tst_QXmppMixManager::testUpdateSupport()
{
    QFETCH(QString, response);
    QFETCH(QXmppMixManager::Support, participantSupport);
    QFETCH(QXmppMixManager::Support, messageArchivingSupport);

    auto tester = Tester(u"hag66@shakespeare.example"_s);
    connectAndAnswerSupport(tester.client, response);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(tester.manager->participantSupport(), participantSupport);
    QCOMPARE(tester.manager->messageArchivingSupport(), messageArchivingSupport);
    QT_WARNING_POP

    QCOMPARE(tester.manager->watchParticipantSupport().supported().value(), participantSupport == QXmppMixManager::Support::Supported);
    QCOMPARE(tester.manager->watchMessageArchivingSupport().supported().value(), messageArchivingSupport == QXmppMixManager::Support::Supported);
}

void tst_QXmppMixManager::testAddJidToNode()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->addJidToNode(u"coven@mix.shakespeare.example"_s, u"urn:xmpp:mix:nodes:allowed"_s, u"alice@wonderland.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:allowed'>"
        "<item id='alice@wonderland.example'/>"
        "</publish>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestJids()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->requestJids(u"coven@mix.shakespeare.example"_s, u"urn:xmpp:mix:nodes:allowed"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:allowed'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:allowed'>"
        "<item id='shakespeare.example'/>"
        "<item id='alice@wonderland.example'/>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto jids = expectFutureVariant<QList<QXmppMixManager::Jid>>(task);
    QCOMPARE(jids.at(0), u"shakespeare.example"_s);
    QCOMPARE(jids.at(1), u"alice@wonderland.example"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testJoinChannelPrivate()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        QXmppMixInvitation invitation;
        invitation.setInviterJid(u"hag66@shakespeare.example"_s);
        invitation.setInviteeJid(u"cat@shakespeare.example"_s);
        invitation.setChannelJid(u"coven@mix.shakespeare.example"_s);
        invitation.setToken(u"ABCDEF"_s);

        QXmppMixIq iq;
        iq.setType(QXmppIq::Set);
        iq.setTo(u"hag66@shakespeare.example"_s);
        iq.setActionType(QXmppMixIq::ClientJoin);
        iq.setChannelJid(invitation.channelJid());
        iq.setNick(u"third witch"_s);
        iq.setSubscriptions(QXmppMixConfigItem::Node::AllowedJids | QXmppMixConfigItem::Node::BannedJids);
        iq.setInvitation(invitation);

        return manager->joinChannel(std::move(iq));
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='hag66@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<nick>third witch</nick>"
        "<invitation xmlns='urn:xmpp:mix:misc:0'>"
        "<inviter>hag66@shakespeare.example</inviter>"
        "<invitee>cat@shakespeare.example</invitee>"
        "<channel>coven@mix.shakespeare.example</channel>"
        "<token>ABCDEF</token>"
        "</invitation>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123456'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<nick>third witch 2</nick>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, u"123456"_s);
    QCOMPARE(result.nickname, u"third witch 2"_s);
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::AllowedJids);

    testError(task = call(), client, u"qx1"_s, u"hag66@shakespeare.example"_s);
}

void tst_QXmppMixManager::testPrepareJoinIq()
{
    auto [client, manager] = Tester(u"hag66@shakespeare.example"_s);
    auto iq = manager->prepareJoinIq(u"coven@mix.shakespeare.example"_s, u"third witch"_s, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);

    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.to(), u"hag66@shakespeare.example"_s);
    QCOMPARE(iq.actionType(), QXmppMixIq::ClientJoin);
    QCOMPARE(iq.channelJid(), u"coven@mix.shakespeare.example"_s);
    QCOMPARE(iq.nick(), u"third witch"_s);
    QCOMPARE(iq.subscriptions(), QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testHandlePubSubEvent()
{
    QXmppMixManager manager;
    QSignalSpy jidAllowedSpy(&manager, &QXmppMixManager::jidAllowed);
    QSignalSpy allJidsAllowedSpy(&manager, &QXmppMixManager::allJidsAllowed);
    QSignalSpy jidDisallowedSpy(&manager, &QXmppMixManager::jidDisallowed);
    QSignalSpy allJidsDisallowedSpy(&manager, &QXmppMixManager::allJidsDisallowed);
    QSignalSpy jidBannedSpy(&manager, &QXmppMixManager::jidBanned);
    QSignalSpy jidUnbannedSpy(&manager, &QXmppMixManager::jidUnbanned);
    QSignalSpy allJidsUnbannedSpy(&manager, &QXmppMixManager::allJidsUnbanned);

    QSignalSpy channelConfigurationUpdatedSpy(&manager, &QXmppMixManager::channelConfigurationUpdated);
    QSignalSpy channelInformationUpdatedSpy(&manager, &QXmppMixManager::channelInformationUpdated);
    QSignalSpy participantReceivedSpy(&manager, &QXmppMixManager::participantReceived);
    QSignalSpy participantLeftSpy(&manager, &QXmppMixManager::participantLeft);
    QSignalSpy channelDeletedSpy(&manager, &QXmppMixManager::channelDeleted);

    const auto channelJid = u"coven@mix.shakespeare.example"_s;
    const auto channelName = u"The Coven"_s;
    const QStringList nodes = { u"urn:xmpp:mix:nodes:allowed"_s, u"urn:xmpp:mix:nodes:banned"_s };
    const auto configurationNode = u"urn:xmpp:mix:nodes:config"_s;
    const auto informationNode = u"urn:xmpp:mix:nodes:info"_s;
    const auto participantNode = u"urn:xmpp:mix:nodes:participants"_s;
    const QStringList jids = { u"hag66@shakespeare.example"_s, u"cat@shakespeare.example"_s };

    const QList<QXmppPubSubEventBase::EventType> eventTypes = { QXmppPubSubEventBase::EventType::Configuration,
                                                                QXmppPubSubEventBase::EventType::Delete,
                                                                QXmppPubSubEventBase::EventType::Items,
                                                                QXmppPubSubEventBase::EventType::Retract,
                                                                QXmppPubSubEventBase::EventType::Purge,
                                                                QXmppPubSubEventBase::EventType::Subscription };

    QXmppPubSubBaseItem allowedOrBannedJidsItem1;
    allowedOrBannedJidsItem1.setId(jids.at(0));

    QXmppPubSubBaseItem allowedOrBannedJidsItem2;
    allowedOrBannedJidsItem2.setId(jids.at(1));

    QXmppPubSubEvent<QXmppPubSubBaseItem> allowedOrBannedJidsEvent;
    allowedOrBannedJidsEvent.setItems({ allowedOrBannedJidsItem1, allowedOrBannedJidsItem2 });
    allowedOrBannedJidsEvent.setRetractIds(jids);

    QXmppMixParticipantItem participantItem1;
    participantItem1.setJid(jids.at(0));

    QXmppMixParticipantItem participantItem2;
    participantItem2.setJid(jids.at(1));

    QXmppPubSubEvent<QXmppMixParticipantItem> participantEvent;
    participantEvent.setItems({ participantItem1, participantItem2 });
    participantEvent.setRetractIds(jids);

    QXmppMixConfigItem configurationItem;
    configurationItem.setFormType(QXmppDataForm::Result);
    configurationItem.setOwnerJids(jids);

    QXmppPubSubEvent<QXmppMixConfigItem> configurationEvent;
    configurationEvent.setItems({ configurationItem });
    configurationEvent.setRetractIds(jids);

    QXmppMixInfoItem informationItem;
    informationItem.setFormType(QXmppDataForm::Result);
    informationItem.setName(channelName);

    QXmppPubSubEvent<QXmppMixInfoItem> informationEvent;
    informationEvent.setItems({ informationItem });
    informationEvent.setRetractIds(jids);

    for (const auto &node : nodes) {
        for (auto eventType : eventTypes) {
            allowedOrBannedJidsEvent.setEventType(eventType);
            manager.handlePubSubEvent(writePacketToDom(allowedOrBannedJidsEvent), channelJid, node);
        }
    }

    for (auto eventType : eventTypes) {
        participantEvent.setEventType(eventType);
        manager.handlePubSubEvent(writePacketToDom(participantEvent), channelJid, participantNode);
    }

    for (auto eventType : eventTypes) {
        configurationEvent.setEventType(eventType);
        manager.handlePubSubEvent(writePacketToDom(configurationEvent), channelJid, configurationNode);
    }

    for (auto eventType : eventTypes) {
        informationEvent.setEventType(eventType);
        manager.handlePubSubEvent(writePacketToDom(informationEvent), channelJid, informationNode);
    }

    for (const auto &spy : { &jidAllowedSpy, &jidDisallowedSpy, &jidBannedSpy, &jidUnbannedSpy, &participantLeftSpy }) {
        QCOMPARE(spy->size(), 2);

        for (auto i = 0; i < spy->size(); i++) {
            const auto &arguments = spy->at(i);
            QCOMPARE(arguments.at(0).toString(), channelJid);
            QCOMPARE(arguments.at(1).toString(), jids.at(i));
        }
    }

    for (const auto &spy : { &allJidsAllowedSpy, &allJidsDisallowedSpy }) {
        QCOMPARE(spy->size(), 1);
        auto arguments = spy->constFirst();
        QCOMPARE(arguments.at(0).toString(), channelJid);
    }

    for (const auto &spy : { &allJidsUnbannedSpy, &channelDeletedSpy }) {
        QCOMPARE(spy->size(), 2);
        for (const auto &arguments : *spy) {
            QCOMPARE(arguments.at(0).toString(), channelJid);
        }
    }

    QCOMPARE(participantReceivedSpy.size(), 2);
    for (auto i = 0; i < participantReceivedSpy.size(); i++) {
        const auto &arguments = participantReceivedSpy.at(i);
        QCOMPARE(arguments.at(0).toString(), channelJid);
        QCOMPARE(arguments.at(1).value<QXmppMixParticipantItem>().jid(), participantEvent.items().at(i).jid());
    }

    for (const auto &spy : { &channelConfigurationUpdatedSpy, &channelInformationUpdatedSpy }) {
        QCOMPARE(spy->size(), 1);
        auto arguments = spy->constFirst();
        QCOMPARE(arguments.at(0).toString(), channelJid);
    }

    QCOMPARE(channelConfigurationUpdatedSpy.first().at(1).value<QXmppMixConfigItem>().ownerJids(), jids);
    QCOMPARE(channelInformationUpdatedSpy.first().at(1).value<QXmppMixInfoItem>().name(), channelName);
}

void tst_QXmppMixManager::testCreateChannel()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->createChannel(u"mix.shakespeare.example"_s);
    };

    auto task = call();

    client.inject(
        "<iq id='qx1' from='mix.shakespeare.example' type='result'>"
        "<create xmlns='urn:xmpp:mix:core:1' channel='A1B2C345'/>"
        "</iq>");
    client.expect(
        "<iq id='qx1' to='mix.shakespeare.example' type='set'>"
        "<create xmlns='urn:xmpp:mix:core:1'/>"
        "</iq>");

    auto channelJid = expectFutureVariant<QXmppMixManager::ChannelJid>(task);
    QCOMPARE(channelJid, u"A1B2C345@mix.shakespeare.example"_s);

    testError(task = call(), client, u"qx1"_s, u"mix.shakespeare.example"_s);
}

void tst_QXmppMixManager::testCreateChannelWithId()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->createChannel(u"mix.shakespeare.example"_s, u"coven"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='mix.shakespeare.example' type='set'>"
        "<create xmlns='urn:xmpp:mix:core:1' channel='coven'/>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='mix.shakespeare.example' type='result'>"
        "<create xmlns='urn:xmpp:mix:core:1' channel='coven'/>"
        "</iq>");

    auto channelJid = expectFutureVariant<QXmppMixManager::ChannelJid>(task);
    QCOMPARE(channelJid, u"coven@mix.shakespeare.example"_s);

    testError(task = call(), client, u"qx1"_s, u"mix.shakespeare.example"_s);
}

void tst_QXmppMixManager::testRequestChannelJids()
{
    auto test = Tester();
    auto &[client, manager] = test;
    auto call = [manager]() {
        return manager->requestChannelJids(u"mix.shakespeare.example"_s);
    };

    auto task = call();
    testError(task, client, u"qx1"_s, u"mix.shakespeare.example"_s);

    task = call();
    client.expect(
        "<iq id='qx1' to='mix.shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#items'/>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='mix.shakespeare.example' type='result'>"
        "<query xmlns='http://jabber.org/protocol/disco#items'>"
        "<item jid='coven@mix.shakespeare.example'/>"
        "<item jid='spells@mix.shakespeare.example'/>"
        "<item jid='wizards@mix.shakespeare.example'/>"
        "</query>"
        "</iq>");

    auto jids = expectFutureVariant<QList<QXmppMixManager::ChannelJid>>(task);
    QCOMPARE(jids.size(), 3);
    QCOMPARE(jids.at(0), u"coven@mix.shakespeare.example"_s);
    QCOMPARE(jids.at(1), u"spells@mix.shakespeare.example"_s);
    QCOMPARE(jids.at(2), u"wizards@mix.shakespeare.example"_s);
}

void tst_QXmppMixManager::testRequestChannelNodes()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->requestChannelNodes(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();
    testErrorFromChannel(task, client);

    task = call();
    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<query xmlns='http://jabber.org/protocol/disco#items' node='mix'/>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<query xmlns='http://jabber.org/protocol/disco#items' node='mix'>"
        "<item jid='coven@mix.shakespeare.example' node='urn:xmpp:mix:nodes:presence'/>"
        "<item jid='coven@mix.shakespeare.example' node='urn:xmpp:mix:nodes:allowed'/>"
        "</query>"
        "</iq>");

    auto nodes = expectFutureVariant<QXmppMixConfigItem::Nodes>(task);
    QCOMPARE(nodes, QXmppMixConfigItem::Node::AllowedJids | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testRequestChannelConfiguration()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->requestChannelConfiguration(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:config'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:config'>"
        "<item id='2016-05-30T09:00:00'>"
        "<x xmlns='jabber:x:data' type='result'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>urn:xmpp:mix:admin:0</value>"
        "</field>"
        "<field type='jid-single' var='Last Change Made By'>"
        "<value>greymalkin@shakespeare.example</value>"
        "</field>"
        "</x>"
        "</item>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto configuration = expectFutureVariant<QXmppMixConfigItem>(task);
    QCOMPARE(configuration.lastEditorJid(), u"greymalkin@shakespeare.example"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testUpdateChannelConfiguration()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    QXmppMixConfigItem configuration;
    configuration.setId(u"2016-05-30T09:00:00"_s);
    configuration.setOwnerJids({ u"greymalkin@shakespeare.example"_s });

    auto call = [manager, configuration]() {
        return manager->updateChannelConfiguration(u"coven@mix.shakespeare.example"_s, configuration);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:config'>"
        "<item id='2016-05-30T09:00:00'>"
        "<x xmlns='jabber:x:data' type='submit'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>urn:xmpp:mix:admin:0</value>"
        "</field>"
        "<field type='jid-multi' var='Owner'>"
        "<value>greymalkin@shakespeare.example</value>"
        "</field>"
        "</x>"
        "</item>"
        "</publish>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:config'>"
        "<item id='2016-05-30T09:00:00'/>"
        "</publish>"
        "</pubsub>"
        "</iq>");

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestChannelInformation()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->requestChannelInformation(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:info'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:info'>"
        "<item id='2016-05-30T09:00:00'>"
        "<x xmlns='jabber:x:data' type='result'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>urn:xmpp:mix:core:1</value>"
        "</field>"
        "<field type='text-single' var='Name'>"
        "<value>Witches Coven</value>"
        "</field>"
        "</x>"
        "</item>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto information = expectFutureVariant<QXmppMixInfoItem>(task);
    QCOMPARE(information.name(), u"Witches Coven"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testUpdateChannelInformation()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    QXmppMixInfoItem information;
    information.setId(u"2016-05-30T09:00:00"_s);
    information.setName(u"The Coven"_s);

    auto call = [manager, information]() {
        return manager->updateChannelInformation(u"coven@mix.shakespeare.example"_s, information);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:info'>"
        "<item id='2016-05-30T09:00:00'>"
        "<x xmlns='jabber:x:data' type='submit'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>urn:xmpp:mix:core:1</value>"
        "</field>"
        "<field type='text-single' var='Name'>"
        "<value>The Coven</value>"
        "</field>"
        "</x>"
        "</item>"
        "</publish>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:info'>"
        "<item id='2016-05-30T09:00:00'/>"
        "</publish>"
        "</pubsub>"
        "</iq>");

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testJoinChannel()
{
    auto tester = Tester(u"hag66@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->joinChannel(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='hag66@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123456'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, u"123456"_s);
    QVERIFY(result.nickname.isEmpty());
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);

    testError(task = call(), client, u"qx1"_s, u"hag66@shakespeare.example"_s);
}

void tst_QXmppMixManager::testJoinChannelWithNickname()
{
    auto [client, manager] = Tester(u"hag66@shakespeare.example"_s);

    auto task = manager->joinChannel(u"coven@mix.shakespeare.example"_s, u"third witch"_s);

    client.expect(
        "<iq id='qx1' to='hag66@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>third witch</nick>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123456'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>third witch</nick>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, u"123456"_s);
    QCOMPARE(result.nickname, u"third witch"_s);
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testJoinChannelWithNodes()
{
    auto [client, manager] = Tester(u"hag66@shakespeare.example"_s);

    auto task = manager->joinChannel(u"coven@mix.shakespeare.example"_s, {}, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);

    client.expect(
        "<iq id='qx1' to='hag66@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123456'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, "123456");
    QVERIFY(result.nickname.isEmpty());
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testJoinChannelViaInvitation()
{
    auto tester = Tester(u"cat@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        QXmppMixInvitation invitation;
        invitation.setInviterJid(u"hag66@shakespeare.example"_s);
        invitation.setInviteeJid(u"cat@shakespeare.example"_s);
        invitation.setChannelJid(u"coven@mix.shakespeare.example"_s);
        invitation.setToken(u"ABCDEF"_s);

        return manager->joinChannel(invitation);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='cat@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<invitation xmlns='urn:xmpp:mix:misc:0'>"
        "<inviter>hag66@shakespeare.example</inviter>"
        "<invitee>cat@shakespeare.example</invitee>"
        "<channel>coven@mix.shakespeare.example</channel>"
        "<token>ABCDEF</token>"
        "</invitation>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123457'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, u"123457"_s);
    QVERIFY(result.nickname.isEmpty());
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);

    testError(task = call(), client, u"qx1"_s, u"cat@shakespeare.example"_s);
}

void tst_QXmppMixManager::testJoinChannelViaInvitationWithNickname()
{
    auto [client, manager] = Tester(u"cat@shakespeare.example"_s);

    QXmppMixInvitation invitation;
    invitation.setInviterJid(u"hag66@shakespeare.example"_s);
    invitation.setInviteeJid(u"cat@shakespeare.example"_s);
    invitation.setChannelJid(u"coven@mix.shakespeare.example"_s);
    invitation.setToken(u"ABCDEF"_s);

    auto task = manager->joinChannel(invitation, u"fourth witch"_s);

    client.expect(
        "<iq id='qx1' to='cat@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>fourth witch</nick>"
        "<invitation xmlns='urn:xmpp:mix:misc:0'>"
        "<inviter>hag66@shakespeare.example</inviter>"
        "<invitee>cat@shakespeare.example</invitee>"
        "<channel>coven@mix.shakespeare.example</channel>"
        "<token>ABCDEF</token>"
        "</invitation>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123457'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>fourth witch</nick>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, u"123457"_s);
    QCOMPARE(result.nickname, u"fourth witch"_s);
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testJoinChannelViaInvitationWithNodes()
{
    auto [client, manager] = Tester(u"cat@shakespeare.example"_s);

    QXmppMixInvitation invitation;
    invitation.setInviterJid(u"hag66@shakespeare.example"_s);
    invitation.setInviteeJid(u"cat@shakespeare.example"_s);
    invitation.setChannelJid(u"coven@mix.shakespeare.example"_s);
    invitation.setToken(u"ABCDEF"_s);

    auto task = manager->joinChannel(invitation, {}, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);

    client.expect(
        "<iq id='qx1' to='cat@shakespeare.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<invitation xmlns='urn:xmpp:mix:misc:0'>"
        "<inviter>hag66@shakespeare.example</inviter>"
        "<invitee>cat@shakespeare.example</invitee>"
        "<channel>coven@mix.shakespeare.example</channel>"
        "<token>ABCDEF</token>"
        "</invitation>"
        "</join>"
        "</client-join>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='123457'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</join>"
        "</client-join>"
        "</iq>");

    auto result = expectFutureVariant<QXmppMixManager::Participation>(task);
    QCOMPARE(result.participantId, "123457");
    QVERIFY(result.nickname.isEmpty());
    QCOMPARE(result.subscriptions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
}

void tst_QXmppMixManager::testUpdateNickname()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->updateNickname(u"coven@mix.shakespeare.example"_s, u"third witch"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<setnick xmlns='urn:xmpp:mix:core:1'>"
        "<nick>third witch</nick>"
        "</setnick>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<setnick xmlns='urn:xmpp:mix:core:1'>"
        "<nick>third witch 2</nick>"
        "</setnick>"
        "</iq>");

    auto nickname = expectFutureVariant<QXmppMixManager::Nickname>(task);
    QCOMPARE(nickname, "third witch 2");

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testUpdateSubscriptions()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto defaultParametersCall = [&client, manager]() {
        return manager->updateSubscriptions("coven@mix.shakespeare.example");
    };

    auto task = defaultParametersCall();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</update-subscription>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "</update-subscription>"
        "</iq>");

    auto subscription = expectFutureVariant<QXmppMixManager::Subscription>(task);
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::AllowedJids));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::AvatarData));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::AvatarMetadata));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::BannedJids));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::Configuration));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::Information));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::JidMap));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::Messages));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::Participants));
    QVERIFY(subscription.additions.testFlag(QXmppMixConfigItem::Node::Presence));
    // "QCOMPARE(subscription.additions, ~QXmppMixConfigItem::Nodes());" does not work.
    QCOMPARE(subscription.additions, QXmppMixConfigItem::Node::AllowedJids | QXmppMixConfigItem::Node::AvatarData | QXmppMixConfigItem::Node::AvatarMetadata | QXmppMixConfigItem::Node::BannedJids | QXmppMixConfigItem::Node::Configuration | QXmppMixConfigItem::Node::Information | QXmppMixConfigItem::Node::JidMap | QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Participants | QXmppMixConfigItem::Node::Presence);
    QCOMPARE(subscription.removals, QXmppMixConfigItem::Nodes());

    auto defaultRemovalParameterCall = [&client, manager]() {
        return manager->updateSubscriptions("coven@mix.shakespeare.example", QXmppMixConfigItem::Node::AllowedJids | QXmppMixConfigItem::Node::BannedJids);
    };

    task = defaultRemovalParameterCall();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "</update-subscription>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "</update-subscription>"
        "</iq>");

    subscription = expectFutureVariant<QXmppMixManager::Subscription>(task);
    QCOMPARE(subscription.additions, QXmppMixConfigItem::Node::AllowedJids | QXmppMixConfigItem::Node::BannedJids);
    QCOMPARE(subscription.removals, QXmppMixConfigItem::Nodes());

    auto call = [&client, manager]() {
        return manager->updateSubscriptions(u"coven@mix.shakespeare.example"_s, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence, QXmppMixConfigItem::Node::Configuration | QXmppMixConfigItem::Node::Information);
    };

    task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<unsubscribe node='urn:xmpp:mix:nodes:config'/>"
        "<unsubscribe node='urn:xmpp:mix:nodes:info'/>"
        "</update-subscription>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<update-subscription xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<unsubscribe node='urn:xmpp:mix:nodes:config'/>"
        "<unsubscribe node='urn:xmpp:mix:nodes:info'/>"
        "</update-subscription>"
        "</iq>");

    subscription = expectFutureVariant<QXmppMixManager::Subscription>(task);
    QCOMPARE(subscription.additions, QXmppMixConfigItem::Node::Messages | QXmppMixConfigItem::Node::Presence);
    QCOMPARE(subscription.removals, QXmppMixConfigItem::Node::Configuration | QXmppMixConfigItem::Node::Information);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestInvitation()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;
    auto logger = client.logger();

    auto call = [&client, manager]() {
        return manager->requestInvitation(u"coven@mix.shakespeare.example"_s, u"cat@shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<invite xmlns='urn:xmpp:mix:misc:0'>"
        "<invitee>cat@shakespeare.example</invitee>"
        "</invite>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<invite xmlns='urn:xmpp:mix:misc:0'>"
        "<invitation xmlns='urn:xmpp:mix:misc:0'>"
        "<inviter>hag66@shakespeare.example</inviter>"
        "<invitee>cat@shakespeare.example</invitee>"
        "<channel>coven@mix.shakespeare.example</channel>"
        "<token>ABCDEF</token>"
        "</invitation>"
        "</invite>"
        "</iq>");

    const auto invitation = expectFutureVariant<QXmppMixInvitation>(task);
    QCOMPARE(invitation.token(), u"ABCDEF"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestAllowedJids()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->requestAllowedJids(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:allowed'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:allowed'>"
        "<item id='shakespeare.example'/>"
        "<item id='alice@wonderland.example'/>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto allowedJids = expectFutureVariant<QList<QXmppMixManager::Jid>>(task);
    QCOMPARE(allowedJids.at(0), u"shakespeare.example"_s);
    QCOMPARE(allowedJids.at(1), u"alice@wonderland.example"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testAllowJid()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->allowJid(u"coven@mix.shakespeare.example"_s, u"alice@wonderland.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:allowed'>"
        "<item id='alice@wonderland.example'/>"
        "</publish>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testDisallowJid()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->disallowJid(u"coven@mix.shakespeare.example"_s, u"alice@wonderland.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<retract node='urn:xmpp:mix:nodes:allowed'>"
        "<item id='alice@wonderland.example'/>"
        "</retract>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testDisallowAllJids()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->disallowAllJids(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub#owner'>"
        "<purge node='urn:xmpp:mix:nodes:allowed'/>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestBannedJids()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->requestBannedJids(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:banned'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:banned'>"
        "<item id='lear@shakespeare.example'/>"
        "<item id='macbeth@shakespeare.example'/>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto allowedJids = expectFutureVariant<QList<QXmppMixManager::Jid>>(task);
    QCOMPARE(allowedJids.at(0), u"lear@shakespeare.example"_s);
    QCOMPARE(allowedJids.at(1), u"macbeth@shakespeare.example"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testBanJid()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->banJid(u"coven@mix.shakespeare.example"_s, u"macbeth@shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:mix:nodes:banned'>"
        "<item id='macbeth@shakespeare.example'/>"
        "</publish>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testUnbanJid()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->unbanJid(u"coven@mix.shakespeare.example"_s, u"macbeth@shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<retract node='urn:xmpp:mix:nodes:banned'>"
        "<item id='macbeth@shakespeare.example'/>"
        "</retract>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testUnbanAllJids()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->unbanAllJids(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub#owner'>"
        "<purge node='urn:xmpp:mix:nodes:banned'/>"
        "</pubsub>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='coven@mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testRequestParticipants()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->requestParticipants(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'/>"
        "</pubsub>"
        "</iq>");
    client.inject(
        "<iq id='qx1' from='coven@mix.shakespeare.example' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'>"
        "<item id='123456'>"
        "<participant xmlns='urn:xmpp:mix:core:1'>"
        "<nick>thirdwitch</nick>"
        "<jid>hag66@shakespeare.example</jid>"
        "</participant>"
        "</item>"
        "<item id='123457'>"
        "<participant xmlns='urn:xmpp:mix:core:1'>"
        "<nick>fourthwitch</nick>"
        "<jid>hag67@shakespeare.example</jid>"
        "</participant>"
        "</item>"
        "</items>"
        "</pubsub>"
        "</iq>");

    auto participants = expectFutureVariant<QList<QXmppMixParticipantItem>>(task);
    QCOMPARE(participants.at(0).jid(), u"hag66@shakespeare.example"_s);
    QCOMPARE(participants.at(1).jid(), u"hag67@shakespeare.example"_s);

    testErrorFromChannel(task = call(), client);
}

void tst_QXmppMixManager::testLeaveChannel()
{
    auto tester = Tester(u"hag66@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->leaveChannel(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='hag66@shakespeare.example' type='set'>"
        "<client-leave xmlns='urn:xmpp:mix:pam:2' channel='coven@mix.shakespeare.example'>"
        "<leave xmlns='urn:xmpp:mix:core:1'/>"
        "</client-leave>"
        "</iq>");
    client.inject(
        "<iq id='qx1' type='result'>"
        "<client-leave xmlns='urn:xmpp:mix:pam:2'>"
        "<leave xmlns='urn:xmpp:mix:core:1'/>"
        "</client-leave>"
        "</iq>");

    expectFutureVariant<QXmpp::Success>(task);

    testError(task = call(), client, u"qx1"_s, u"hag66@shakespeare.example"_s);
}

void tst_QXmppMixManager::testDeleteChannel()
{
    auto tester = Tester();
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [&client, manager]() {
        return manager->deleteChannel(u"coven@mix.shakespeare.example"_s);
    };

    auto task = call();

    client.expect(
        "<iq id='qx1' to='mix.shakespeare.example' type='set'>"
        "<destroy xmlns='urn:xmpp:mix:core:1' channel='coven'/>"
        "</iq>");
    client.inject(u"<iq id='qx1' from='mix.shakespeare.example' type='result'/>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testError(task = call(), client, u"qx1"_s, u"mix.shakespeare.example"_s);
}

// encrypts nothing, records the recipients
class MixTestE2eeManager : public QXmppClientExtension
{
    Q_OBJECT
public:
    static constexpr QStringView Encryption = u"urn:test:mix-e2ee";

    MixTestE2eeManager()
    {
        QXmpp::E2ee::Providers(hooks()).add(QXmpp::E2ee::Provider {
            .encryption = Encryption,
            .encryptMessage = [this](QXmpp::E2ee::EncryptRequest request) {
                recipientJids = request.recipientJids();
                std::ranges::sort(recipientJids);
                return QXmpp::Private::makeReadyTask<QXmpp::E2ee::EncryptResult>(QXmpp::E2ee::Encrypted());
            },
        });
    }

    QList<QString> recipientJids;
};

struct EncryptionTester : Tester {
    EncryptionTester()
        : Tester(u"hag66@shakespeare.example/pda"_s)
    {
        client.addNewExtension<QXmppRosterManager>(&client);
        e2ee = client.addNewExtension<MixTestE2eeManager>();
        client.hooks(&owner).add<QXmpp::Messages::SelectEncryption>([](QXmpp::Messages::EncryptionSelection &selection) {
            selection.setEncryption(MixTestE2eeManager::Encryption);
        });

        // the joined channel is in the roster
        client.inject(u"<iq id='push1' type='set'><query xmlns='jabber:iq:roster'>"
                      "<item jid='coven@mix.shakespeare.example' subscription='both'>"
                      "<channel xmlns='urn:xmpp:mix:roster:0' participant-id='123456'/>"
                      "</item></query></iq>"_s);
        client.ignore();
    }

    QXmppTask<QXmpp::SendResult> send(const QString &to = u"coven@mix.shakespeare.example"_s, QXmppSendStanzaParams params = {})
    {
        QXmppMessage message(u"hag66@shakespeare.example/pda"_s, to, u"Hi"_s);
        message.setType(QXmppMessage::GroupChat);
        return client.send(std::move(message), params);
    }

    // returns the ID of the request
    QString expectParticipantsRequest()
    {
        return client.expectPacketRandomOrder(
            u"<iq id='qx1' to='coven@mix.shakespeare.example' type='get'>"
            "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
            "<items node='urn:xmpp:mix:nodes:participants'/>"
            "</pubsub></iq>"_s);
    }

    // answers the request of the participants with the participants (participant ID, JID)
    void answerParticipants(const std::vector<std::pair<QString, QString>> &participants)
    {
        const auto id = expectParticipantsRequest();
        QString items;
        for (const auto &[participantId, jid] : participants) {
            items += u"<item id='"_s + participantId + u"'><participant xmlns='urn:xmpp:mix:core:1'>"_s +
                (jid.isEmpty() ? QString() : u"<jid>"_s + jid + u"</jid>"_s) + u"</participant></item>"_s;
        }
        client.inject(u"<iq id='"_s + id + u"' from='coven@mix.shakespeare.example' type='result'>"
                                           "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                                           "<items node='urn:xmpp:mix:nodes:participants'>"_s +
                      items + u"</items></pubsub></iq>"_s);
    }

    // the encrypted message has been sent, the stream acknowledges it later
    void expectSent(const QXmppTask<QXmpp::SendResult> &task)
    {
        const auto packet = client.takePacket();
        QVERIFY2(packet.startsWith(u"<message"_s), qPrintable(packet));
        QVERIFY(!task.isFinished());
    }

    void injectParticipantEvent(const QString &content, const QString &from = u"coven@mix.shakespeare.example"_s)
    {
        client.inject(u"<message from='"_s + from + u"' to='hag66@shakespeare.example'>"
                                                    "<event xmlns='http://jabber.org/protocol/pubsub#event'>"_s +
                      content + u"</event></message>"_s);
    }

    void injectParticipantJoined(const QString &participantId, const QString &jid, const QString &from = u"coven@mix.shakespeare.example"_s)
    {
        injectParticipantEvent(u"<items node='urn:xmpp:mix:nodes:participants'><item id='"_s + participantId +
                                   u"'><participant xmlns='urn:xmpp:mix:core:1'><jid>"_s + jid + u"</jid></participant></item></items>"_s,
                               from);
    }

    void injectParticipantLeft(const QString &participantId)
    {
        injectParticipantEvent(u"<items node='urn:xmpp:mix:nodes:participants'><retract id='"_s + participantId + u"'/></items>"_s);
    }

    QObject owner;
    MixTestE2eeManager *e2ee;
};

void tst_QXmppMixManager::testSendMessageEncrypted()
{
    EncryptionTester tester;
    auto &client = tester.client;

    auto task = tester.send();
    // changes received while the participants are retrieved are applied afterwards
    tester.injectParticipantJoined(u"123459"_s, u"hag69@shakespeare.example"_s);
    tester.injectParticipantLeft(u"123457"_s);
    // the offline participants are recipients too, not this account
    tester.answerParticipants({ { u"123456"_s, u"hag66@shakespeare.example"_s },
                                { u"123457"_s, u"hag67@shakespeare.example"_s },
                                { u"123458"_s, u"hag68@shakespeare.example/desktop"_s } });
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag68@shakespeare.example"_s, u"hag69@shakespeare.example"_s }));
    tester.expectSent(task);

    // the events of the participants node are tracked
    tester.injectParticipantJoined(u"123460"_s, u"hag70@shakespeare.example"_s);
    tester.injectParticipantLeft(u"123458"_s);
    // only the channel can publish its participants
    tester.injectParticipantJoined(u"123461"_s, u"mallory@shakespeare.example"_s, u"hag67@shakespeare.example"_s);

    // the participants are not requested again
    task = tester.send();
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag69@shakespeare.example"_s, u"hag70@shakespeare.example"_s }));
    tester.expectSent(task);
    client.expectNoPacket();

    // recipients set by the sender are kept
    QXmppSendStanzaParams params;
    params.setEncryptionJids({ u"hag70@shakespeare.example"_s });
    task = tester.send(u"coven@mix.shakespeare.example"_s, params);
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag70@shakespeare.example"_s }));
    tester.expectSent(task);

    // events may have been missed in a new stream
    client.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT client.connected();
    // the requests of the server features
    client.ignore();
    client.ignore();
    task = tester.send();
    tester.answerParticipants({ { u"123457"_s, u"hag67@shakespeare.example"_s } });
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag67@shakespeare.example"_s }));
    tester.expectSent(task);

    // the participants of a rejoined channel are requested again
    client.inject(u"<iq id='push2' type='set'><query xmlns='jabber:iq:roster'>"
                  "<item jid='coven@mix.shakespeare.example' subscription='remove'/>"
                  "</query></iq>"_s);
    client.inject(u"<iq id='push3' type='set'><query xmlns='jabber:iq:roster'>"
                  "<item jid='coven@mix.shakespeare.example' subscription='both'>"
                  "<channel xmlns='urn:xmpp:mix:roster:0' participant-id='123462'/>"
                  "</item></query></iq>"_s);
    client.ignore();
    client.ignore();
    task = tester.send();
    tester.answerParticipants({ { u"123458"_s, u"hag68@shakespeare.example"_s } });
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag68@shakespeare.example"_s }));
    tester.expectSent(task);
}

void tst_QXmppMixManager::testSendMessageEncryptedNoChannel()
{
    EncryptionTester tester;
    auto &client = tester.client;

    // not a joined channel
    auto task = tester.send(u"group@mix.shakespeare.example"_s);
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"group@mix.shakespeare.example"_s }));
    tester.expectSent(task);
    client.expectNoPacket();
}

void tst_QXmppMixManager::testSendMessageEncryptedError()
{
    EncryptionTester tester;
    auto &client = tester.client;

    // the second message waits for the first one
    auto first = tester.send();
    auto second = tester.send();
    const auto id = tester.expectParticipantsRequest();
    client.inject(u"<iq id='"_s + id + u"' from='coven@mix.shakespeare.example' type='error'>"
                                       "<error type='auth'><forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>"_s);

    // not sent without all participants
    QCOMPARE(expectFutureVariant<QXmppError>(first).value<QXmpp::SendError>(), QXmpp::SendError::EncryptionError);
    QVERIFY(tester.e2ee->recipientJids.isEmpty());

    // the next message retries
    tester.answerParticipants({ { u"123457"_s, u"hag67@shakespeare.example"_s } });
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"hag67@shakespeare.example"_s }));
    tester.expectSent(second);
}

void tst_QXmppMixManager::testSendMessageEncryptedHiddenJids()
{
    EncryptionTester tester;
    auto &client = tester.client;

    auto task = tester.send();
    tester.answerParticipants({ { u"123456"_s, u"hag66@shakespeare.example"_s }, { u"123457"_s, {} } });
    QCOMPARE(expectFutureVariant<QXmppError>(task).value<QXmpp::SendError>(), QXmpp::SendError::EncryptionError);
    QVERIFY(tester.e2ee->recipientJids.isEmpty());
    client.expectNoPacket();
}

// the sender of a received message resolved by the message pipeline
static QString receivedSender(TestClient &client, const QString &from)
{
    QObject owner;
    QString sender;
    client.hooks(&owner).add<QXmpp::Messages::Received>([&](QXmpp::Messages::Context &context) {
        sender = context.senderJid();
        return QXmpp::HookResult::Stop;
    });
    client.injectMessage(u"<message from='"_s + from + u"' to='hag66@shakespeare.example' type='groupchat'>"
                                                       "<body>Hi</body>"
                                                       "<mix xmlns='urn:xmpp:mix:core:1'><nick>thirdwitch</nick><jid>hag67@shakespeare.example</jid></mix>"
                                                       "</message>"_s);
    return sender;
}

void tst_QXmppMixManager::testResolveSender()
{
    EncryptionTester tester;
    auto &client = tester.client;

    // the channel stamps the sender
    QCOMPARE(receivedSender(client, u"coven@mix.shakespeare.example/123457"_s), u"hag67@shakespeare.example"_s);
    // anybody else could add the element, e.g., an occupant of a MUC room
    QCOMPARE(receivedSender(client, u"room@muc.shakespeare.example/firstwitch"_s), u"room@muc.shakespeare.example"_s);
    QCOMPARE(receivedSender(client, u"mallory@shakespeare.example/desktop"_s), u"mallory@shakespeare.example"_s);
}

void tst_QXmppMixManager::testMucRoomWithChannelJid()
{
    EncryptionTester tester;
    auto &client = tester.client;
    auto *muc = client.addNewExtension<QXmppMucManagerV2>();

    // a service provides a MUC room and a MIX channel with the same JID, the room is joined
    auto joinTask = muc->joinRoom(u"coven@mix.shakespeare.example"_s, u"thirdwitch"_s);
    const auto infoId = client.expectPacketRandomOrder(
        u"<iq id='qx1' to='coven@mix.shakespeare.example' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    client.ignore();  // presence
    client.inject(u"<iq id='"_s + infoId + u"' from='coven@mix.shakespeare.example' type='result'>"
                                           "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                           "<identity category='conference' type='text'/>"
                                           "<feature var='http://jabber.org/protocol/muc'/>"
                                           "<feature var='muc_nonanonymous'/>"
                                           "</query></iq>"_s);
    QXmppPresence selfPresence;
    parsePacket(selfPresence, "<presence from='coven@mix.shakespeare.example/thirdwitch'>"
                              "<x xmlns='http://jabber.org/protocol/muc#user'>"
                              "<item affiliation='member' role='participant'/><status code='110'/>"
                              "</x></presence>");
    client.injectPresence(selfPresence);
    client.injectMessage(u"<message from='coven@mix.shakespeare.example' type='groupchat'><subject>Test</subject></message>"_s);
    expectFutureVariant<QXmppMucRoomV2>(joinTask);
    QXmppPresence occupant;
    parsePacket(occupant, "<presence from='coven@mix.shakespeare.example/firstwitch'>"
                          "<x xmlns='http://jabber.org/protocol/muc#user'>"
                          "<item affiliation='member' role='participant' jid='crone1@shakespeare.example/desktop'/>"
                          "</x></presence>");
    client.injectPresence(occupant);

    // the room is handled by the MUC manager: the occupants are the sender and the recipients
    QCOMPARE(receivedSender(client, u"coven@mix.shakespeare.example/firstwitch"_s), u"crone1@shakespeare.example"_s);
    auto task = tester.send();
    QCOMPARE(tester.e2ee->recipientJids, (QList<QString> { u"crone1@shakespeare.example"_s }));
    tester.expectSent(task);
    client.expectNoPacket();
}

template<typename T>
void tst_QXmppMixManager::testErrorFromChannel(QXmppTask<T> &task, TestClient &client)
{
    testErrorFromChannel(task, client, u"qx1"_s);
}

template<typename T>
void tst_QXmppMixManager::testErrorFromChannel(QXmppTask<T> &task, TestClient &client, const QString &id)
{
    testError(task, client, id, u"coven@mix.shakespeare.example"_s);
}

template<typename T>
void tst_QXmppMixManager::testError(QXmppTask<T> &task, TestClient &client, const QString &id, const QString &from)
{
    client.ignore();
    client.inject(u"<iq id='%1' from='%2' type='error'>"
                  u"<error type='cancel'>"
                  u"<not-allowed xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                  u"</error>"
                  u"</iq>"_s
                      .arg(id, from));

    expectFutureVariant<QXmppError>(task);
}

QTEST_MAIN(tst_QXmppMixManager)
#include "MixManager.moc"

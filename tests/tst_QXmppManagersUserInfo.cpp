// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2020 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2021 Germán Márquez Mejía <mancho@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the managers that query user and entity
// information. Merging the version, entity time, user tune, user location and
// vCard manager tests into one translation unit parses the shared Qt/QXmpp
// headers once instead of once per file. Each original test keeps its own
// namespace; main() runs them in turn.

#include "QXmppClient.h"
#include "QXmppEntityTimeIq.h"
#include "QXmppEntityTimeManager.h"
#include "QXmppGeolocItem.h"
#include "QXmppPubSubManager.h"
#include "QXmppUserLocationManager.h"
#include "QXmppUserTuneItem.h"
#include "QXmppUserTuneManager.h"
#include "QXmppVCardIq.h"
#include "QXmppVCardManager.h"
#include "QXmppVersionIq.h"
#include "QXmppVersionManager.h"

#include "IntegrationTesting.h"
#include "TestClient.h"
#include "util.h"

#include <memory>

#include <QCoreApplication>
#include <QObject>

Q_DECLARE_METATYPE(QXmppVersionIq);
Q_DECLARE_METATYPE(QXmppEntityTimeIq);
Q_DECLARE_METATYPE(QXmppVCardIq);

namespace Version {

class tst_QXmppVersionManager : public QObject
{
    Q_OBJECT
    Q_SLOT void initTestCase();
    Q_SLOT void testSendRequest();
    Q_SLOT void testHandleRequest();
};

void tst_QXmppVersionManager::initTestCase()
{
    qRegisterMetaType<QXmppVersionIq>();
}

void tst_QXmppVersionManager::testSendRequest()
{
    TestClient test;
    auto *verManager = test.addNewExtension<QXmppVersionManager>();

    QSignalSpy spy(verManager, &QXmppVersionManager::versionReceived);

    auto id = verManager->requestVersion("juliet@capulet.com/balcony");
    test.expect("<iq id='qx1' to='juliet@capulet.com/balcony' type='get'><query xmlns='jabber:iq:version'/></iq>");
    verManager->handleStanza(xmlToDom(R"(<iq type='result' from='juliet@capulet.com/balcony' id='qx1'>
  <query xmlns='jabber:iq:version'>
    <name>Exodus</name>
    <version>0.7.0.4</version>
    <os>Windows-XP 5.01.2600</os>
  </query>
</iq>)"));

    QCOMPARE(spy.size(), 1);
    auto version = spy.at(0).at(0).value<QXmppVersionIq>();
    QCOMPARE(version.name(), u"Exodus"_s);
    QCOMPARE(version.version(), u"0.7.0.4"_s);
    QCOMPARE(version.os(), u"Windows-XP 5.01.2600"_s);
}

void tst_QXmppVersionManager::testHandleRequest()
{
    TestClient test;
    test.configuration().setJid("juliet@capulet.com/balcony");

    auto *verManager = test.addNewExtension<QXmppVersionManager>();
    verManager->setClientName("Exodus");
    verManager->setClientVersion("0.7.0.4");
    verManager->setClientOs("Windows-XP 5.01.2600");

    verManager->handleStanza(xmlToDom(R"(<iq type='get' from='romeo@montague.net/orchard' to='juliet@capulet.com/balcony' id='version_1'>
  <query xmlns='jabber:iq:version'/>
</iq>)"));
    test.expect(R"(<iq id='version_1' to='romeo@montague.net/orchard' type='result'>)"
                "<query xmlns='jabber:iq:version'><name>Exodus</name><os>Windows-XP 5.01.2600</os><version>0.7.0.4</version>"
                "</query></iq>");
}

}  // namespace Version

// ============================================================

namespace EntityTime {

class tst_QXmppEntityTimeManager : public QObject
{
    Q_OBJECT
    Q_SLOT void initTestCase();
    Q_SLOT void testSendRequest();
    Q_SLOT void testHandleRequest();
};

void tst_QXmppEntityTimeManager::initTestCase()
{
    qRegisterMetaType<QXmppEntityTimeIq>();
}

void tst_QXmppEntityTimeManager::testSendRequest()
{
    TestClient test;
    auto *manager = test.addNewExtension<QXmppEntityTimeManager>();

    QSignalSpy spy(manager, &QXmppEntityTimeManager::timeReceived);

    manager->requestTime("juliet@capulet.com/balcony");
    test.expect("<iq id='qx1' to='juliet@capulet.com/balcony' type='get'><time xmlns='urn:xmpp:time'/></iq>");
    manager->handleStanza(xmlToDom(R"(<iq id='qx1' to='romeo@montague.net/orchard' from='juliet@capulet.com/balcony' type='result'>
  <time xmlns='urn:xmpp:time'>
    <tzo>-06:00</tzo>
    <utc>2006-12-19T17:58:35Z</utc>
  </time>
</iq>)"));

    QCOMPARE(spy.size(), 1);
    auto time = spy.at(0).at(0).value<QXmppEntityTimeIq>();
    QCOMPARE(time.utc(), QDateTime({ 2006, 12, 19 }, { 17, 58, 35 }, TimeZoneUTC));
    QCOMPARE(time.tzo(), -6 * 60 * 60);
}

void tst_QXmppEntityTimeManager::testHandleRequest()
{
    TestClient test;
    test.configuration().setJid("juliet@capulet.com/balcony");

    auto *manager = test.addNewExtension<QXmppEntityTimeManager>();

    manager->handleStanza(xmlToDom(R"(<iq type='get' from='romeo@montague.net/orchard' to='juliet@capulet.com/balcony' id='time_1'>
  <time xmlns='urn:xmpp:time'/>
</iq>)"));

    auto packet = xmlToDom(test.takePacket());

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QVERIFY(QXmppEntityTimeIq::isEntityTimeIq(packet));
    QT_WARNING_POP

    QXmppEntityTimeIq resp;
    resp.parse(packet);

    QCOMPARE(resp.id(), u"time_1"_s);
    QCOMPARE(resp.type(), QXmppIq::Result);
}

}  // namespace EntityTime

// ============================================================

namespace UserTune {

using PSManager = QXmppPubSubManager;

class tst_QXmppUserTuneManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void testRequest();
    Q_SLOT void testPublish();
    Q_SLOT void testEvents();
};

void tst_QXmppUserTuneManager::initTestCase()
{
    qRegisterMetaType<QXmppTuneItem>();
}

void tst_QXmppUserTuneManager::testRequest()
{
    TestClient test;
    test.addNewExtension<QXmppPubSubManager>();
    auto *tuneManager = test.addNewExtension<QXmppUserTuneManager>();

    auto future = tuneManager->request("anthony@qxmpp.org");
    test.expect("<iq id=\"qx1\" to=\"anthony@qxmpp.org\" type=\"get\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\"><items node=\"http://jabber.org/protocol/tune\"/></pubsub></iq>");
    test.inject(QStringLiteral("<iq id=\"qx1\" from=\"anthony@qxmpp.org\" type=\"result\">"
                               "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
                               "<items node=\"http://jabber.org/protocol/tune\">"
                               "<item id='abc3'><tune xmlns='http://jabber.org/protocol/tune'><title>I Kiste girl</title></tune></item>"
                               "</items>"
                               "</pubsub></iq>"));

    QCoreApplication::processEvents();
    auto item = expectFutureVariant<QXmppTuneItem>(future);
    QCOMPARE(item.id(), u"abc3"_s);
    QCOMPARE(item.title(), u"I Kiste girl"_s);
}

void tst_QXmppUserTuneManager::testPublish()
{
    TestClient test;
    test.configuration().setJid("stpeter@jabber.org");
    test.addNewExtension<QXmppPubSubManager>();
    auto *tuneManager = test.addNewExtension<QXmppUserTuneManager>();

    QXmppTuneItem item;
    item.setArtist("Yes");
    item.setLength(686);
    item.setRating(8);
    item.setSource("Yessongs");
    item.setTitle("Heart of the Sunrise");
    item.setTrack("3");
    item.setUri(QUrl("http://www.yesworld.com/lyrics/Fragile.html#9"));

    auto future = tuneManager->publish(item);
    test.expect("<iq id='qx1' to='stpeter@jabber.org' type='set'>"
                "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                "<publish node='http://jabber.org/protocol/tune'>"
                "<item><tune xmlns='http://jabber.org/protocol/tune'>"
                "<artist>Yes</artist><length>686</length><rating>8</rating><source>Yessongs</source><title>Heart of the Sunrise</title><track>3</track><uri>http://www.yesworld.com/lyrics/Fragile.html#9</uri></tune></item>"
                "</publish>"
                "</pubsub></iq>");
    test.inject(QStringLiteral("<iq type='result' from='stpeter@jabber.org' id='qx1'>"
                               "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                               "<publish node='http://jabber.org/protocol/tune'>"
                               "<item id='abcdf'/>"
                               "</publish></pubsub></iq>"));

    QCOMPARE(expectFutureVariant<QString>(future), u"abcdf"_s);
}

void tst_QXmppUserTuneManager::testEvents()
{
    TestClient test;
    test.configuration().setJid("stpeter@jabber.org");
    auto *psManager = test.addNewExtension<QXmppPubSubManager>();
    auto *tuneManager = test.addNewExtension<QXmppUserTuneManager>();

    QSignalSpy spy(tuneManager, &QXmppUserTuneManager::itemReceived);

    psManager->handleStanza(xmlToDom(QStringLiteral("<message from='stpeter@jabber.org' to='maineboy@jabber.org'>"
                                                    "<event xmlns='http://jabber.org/protocol/pubsub#event'>"
                                                    "<items node='http://jabber.org/protocol/tune'>"
                                                    "<item id='bffe6584-0f9c-11dc-84ba-001143d5d5db'>"
                                                    "<tune xmlns='http://jabber.org/protocol/tune'>"
                                                    "<artist>Yes</artist><length>686</length><rating>8</rating><source>Yessongs</source><title>Heart of the Sunrise</title><track>3</track><uri>http://www.yesworld.com/lyrics/Fragile.html#9</uri>"
                                                    "</tune></item></items>"
                                                    "</event></message>")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().at(0).toString(), u"stpeter@jabber.org"_s);
    QCOMPARE(spy.constFirst().at(1).value<QXmppTuneItem>().artist(), u"Yes"_s);
}

}  // namespace UserTune

// ============================================================

namespace UserLocation {

using PSManager = QXmppPubSubManager;

#define COMPARE_OPT(ACTUAL, EXPECTED) \
    QVERIFY(ACTUAL.has_value());      \
    QCOMPARE(ACTUAL.value(), EXPECTED);

class tst_QXmppUserLocationManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void testRequest();
    Q_SLOT void testPublish();
    Q_SLOT void testEvents();
};

void tst_QXmppUserLocationManager::initTestCase()
{
    qRegisterMetaType<QXmppGeolocItem>();
}

void tst_QXmppUserLocationManager::testRequest()
{
    TestClient test;
    test.addNewExtension<QXmppPubSubManager>();
    auto *tuneManager = test.addNewExtension<QXmppUserLocationManager>();

    auto future = tuneManager->request("anthony@qxmpp.org");
    test.expect("<iq id=\"qx1\" to=\"anthony@qxmpp.org\" type=\"get\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\"><items node=\"http://jabber.org/protocol/geoloc\"/></pubsub></iq>");
    test.inject<QString>("<iq id=\"qx1\" from=\"anthony@qxmpp.org\" type=\"result\">"
                         "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
                         "<items node=\"http://jabber.org/protocol/geoloc\">"
                         "<item id='abc3'><geoloc xmlns='http://jabber.org/protocol/geoloc'>"
                         "<accuracy>20</accuracy>"
                         "<country>Italy</country>"
                         "<lat>45.44</lat>"
                         "<locality>Venice</locality>"
                         "<lon>12.33</lon>"
                         "</geoloc></item>"
                         "</items>"
                         "</pubsub></iq>");

    auto item = expectFutureVariant<QXmppGeolocItem>(future);
    QCOMPARE(item.id(), u"abc3"_s);
    COMPARE_OPT(item.accuracy(), 20.0);
    COMPARE_OPT(item.longitude(), 12.33);
    COMPARE_OPT(item.latitude(), 45.44);
    QCOMPARE(item.locality(), u"Venice"_s);
    QCOMPARE(item.country(), u"Italy"_s);
}

void tst_QXmppUserLocationManager::testPublish()
{
    TestClient test;
    test.configuration().setJid("stpeter@jabber.org");
    test.addNewExtension<QXmppPubSubManager>();
    auto *manager = test.addNewExtension<QXmppUserLocationManager>();

    QXmppGeolocItem item;
    item.setId("abc3");
    item.setAccuracy(20);
    item.setCountry("Italy");
    item.setLatitude(45.44);
    item.setLongitude(12.33);
    item.setLocality("Venice");

    auto future = manager->publish(item);
    test.expect("<iq id='qx1' to='stpeter@jabber.org' type='set'>"
                "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                "<publish node='http://jabber.org/protocol/geoloc'>"
                "<item id='abc3'><geoloc xmlns='http://jabber.org/protocol/geoloc'>"
                "<accuracy>20</accuracy>"
                "<country>Italy</country>"
                "<lat>45.44</lat>"
                "<locality>Venice</locality>"
                "<lon>12.33</lon>"
                "</geoloc></item>"
                "</publish>"
                "</pubsub></iq>");
    test.inject<QString>("<iq type='result' from='stpeter@jabber.org' id='qx1'>"
                         "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                         "<publish node='http://jabber.org/protocol/tune'>"
                         "<item id='some-id'/>"
                         "</publish></pubsub></iq>");

    QCOMPARE(expectFutureVariant<QString>(future), u"some-id"_s);
}

void tst_QXmppUserLocationManager::testEvents()
{
    TestClient test;
    test.configuration().setJid("stpeter@jabber.org");
    auto *psManager = test.addNewExtension<QXmppPubSubManager>();
    auto *manager = test.addNewExtension<QXmppUserLocationManager>();

    QSignalSpy spy(manager, &QXmppUserLocationManager::itemReceived);

    const QString event = "<message from='stpeter@jabber.org' to='maineboy@jabber.org'>"
                          "<event xmlns='http://jabber.org/protocol/pubsub#event'>"
                          "<items node='http://jabber.org/protocol/geoloc'>"
                          "<item id='bffe6584-0f9c-11dc-84ba-001143d5d5db'>"
                          "<geoloc xmlns='http://jabber.org/protocol/geoloc'>"
                          "<accuracy>20</accuracy>"
                          "<country>Italy</country>"
                          "<lat>45.44</lat>"
                          "<locality>Venice</locality>"
                          "<lon>12.33</lon>"
                          "</geoloc></item></items>"
                          "</event></message>";
    psManager->handleStanza(xmlToDom(event));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().at(0).toString(), u"stpeter@jabber.org"_s);
    QCOMPARE(spy.constFirst().at(1).value<QXmppGeolocItem>().id(), u"bffe6584-0f9c-11dc-84ba-001143d5d5db"_s);
    QCOMPARE(spy.constFirst().at(1).value<QXmppGeolocItem>().country(), u"Italy"_s);
}

}  // namespace UserLocation

// ============================================================

namespace VCard {

using namespace QXmpp;

class tst_QXmppVCardManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testHandleStanza_data();
    Q_SLOT void testHandleStanza();
    Q_SLOT void fetchVCard();
    Q_SLOT void setVCard();

    // integration tests
    Q_SLOT void testSetClientVCard();

    QXmppClient m_client;
};

void tst_QXmppVCardManager::testHandleStanza_data()
{
    QTest::addColumn<QXmppVCardIq>("expectedIq");
    QTest::addColumn<bool>("isClientVCard");

#define ROW(name, iq, clientVCard) \
    QTest::newRow(QT_STRINGIFY(name)) << iq << clientVCard

    QXmppVCardIq iq;
    iq.setType(QXmppIq::Result);
    iq.setTo("stpeter@jabber.org/roundabout");
    iq.setFullName("Jeremie Miller");

    auto iqFromBare = iq;
    iqFromBare.setFrom("stpeter@jabber.org");

    auto iqFromFull = iq;
    iqFromFull.setFrom("stpeter@jabber.org/roundabout");

    ROW(client - vcard - from - empty, iq, true);
    ROW(client - vcard - from - bare, iqFromBare, true);
    ROW(client - vcard - from - full, iqFromFull, false);

#undef ROW
}

void tst_QXmppVCardManager::testHandleStanza()
{
    QFETCH(QXmppVCardIq, expectedIq);
    QFETCH(bool, isClientVCard);

    // initialize new manager to clear internal values
    QXmppVCardManager *manager = new QXmppVCardManager();
    m_client.addExtension(manager);

    // sets own jid internally
    m_client.connectToServer("stpeter@jabber.org", {});
    m_client.disconnectFromServer();

    bool vCardReceived = false;
    bool clientVCardReceived = false;

    QObject context;
    connect(manager, &QXmppVCardManager::vCardReceived, &context, [&](QXmppVCardIq iq) {
        vCardReceived = true;
        QCOMPARE(iq, expectedIq);
    });
    connect(manager, &QXmppVCardManager::clientVCardReceived, &context, [&]() {
        clientVCardReceived = true;
        QCOMPARE(manager->clientVCard(), expectedIq);
    });

    bool accepted = manager->handleStanza(writePacketToDom(expectedIq));

    QVERIFY(accepted);
    QVERIFY(vCardReceived);
    QCOMPARE(clientVCardReceived, isClientVCard);

    // clean up (client deletes manager)
    m_client.removeExtension(manager);
}

void tst_QXmppVCardManager::fetchVCard()
{
    TestClient test;
    auto *manager = test.addNewExtension<QXmppVCardManager>();
    auto task = manager->fetchVCard("stpeter@jabber.org");
    QVERIFY(!task.isFinished());

    test.expect("<iq id='qx2' to='stpeter@jabber.org' type='get'><vCard xmlns='vcard-temp'><TITLE/><ROLE/></vCard></iq>");
    test.inject("<iq id='qx2' type='result'>"
                "<vCard xmlns='vcard-temp'>"
                "<FN>Peter Saint-Andre</FN>"
                "<N>"
                "<FAMILY>Saint-Andre</FAMILY>"
                "<GIVEN>Peter</GIVEN>"
                "<MIDDLE/>"
                "</N>"
                "<NICKNAME>stpeter</NICKNAME>"
                "<URL>http://www.xmpp.org/xsf/people/stpeter.shtml</URL>"
                "<BDAY>1966-08-06</BDAY>"
                "<ORG>"
                "<ORGNAME>XMPP Standards Foundation</ORGNAME>"
                "<ORGUNIT/>"
                "</ORG>"
                "<TITLE>Executive Director</TITLE>"
                "<ROLE>Patron Saint</ROLE>"
                "<TEL><WORK/><VOICE/><NUMBER>303-308-3282</NUMBER></TEL>"
                "<TEL><WORK/><FAX/><NUMBER/></TEL>"
                "<TEL><WORK/><MSG/><NUMBER/></TEL>"
                "<ADR>"
                "<WORK/>"
                "<EXTADD>Suite 600</EXTADD>"
                "<STREET>1899 Wynkoop Street</STREET>"
                "<LOCALITY>Denver</LOCALITY>"
                "<REGION>CO</REGION>"
                "<PCODE>80202</PCODE>"
                "<CTRY>USA</CTRY>"
                "</ADR>"
                "<TEL><HOME/><VOICE/><NUMBER>303-555-1212</NUMBER></TEL>"
                "<TEL><HOME/><FAX/><NUMBER/></TEL>"
                "<TEL><HOME/><MSG/><NUMBER/></TEL>"
                "<ADR>"
                "<HOME/>"
                "<EXTADD/>"
                "<STREET/>"
                "<LOCALITY>Denver</LOCALITY>"
                "<REGION>CO</REGION>"
                "<PCODE>80209</PCODE>"
                "<CTRY>USA</CTRY>"
                "</ADR>"
                "<EMAIL><INTERNET/><PREF/><USERID>stpeter@jabber.org</USERID></EMAIL>"
                "<JABBERID>stpeter@jabber.org</JABBERID>"
                "<DESC>More information about me is located on my personal website: http://www.saint-andre.com/</DESC>"
                "</vCard>"
                "</iq>");

    auto vCardIq = expectFutureVariant<QXmppVCardIq>(task);
    QCOMPARE(vCardIq.birthday(), QDate(1966, 8, 6));
}

void tst_QXmppVCardManager::setVCard()
{
    TestClient test;
    test.configuration().setJid("stpeter@jabber.org");
    auto *manager = test.addNewExtension<QXmppVCardManager>();

    QXmppVCardIq v;
    v.setFirstName("Peter");
    v.setLastName("Saint-Andre");
    v.setFullName("Peter Saint-Andre");

    auto task = manager->setVCard(v);
    QVERIFY(!task.isFinished());
    test.expect("<iq id='qx2' to='stpeter@jabber.org' type='set'>"
                "<vCard xmlns='vcard-temp'>"
                "<FN>Peter Saint-Andre</FN>"
                "<N>"
                "<GIVEN>Peter</GIVEN>"
                "<FAMILY>Saint-Andre</FAMILY>"
                "</N>"
                "<TITLE/><ROLE/>"
                "</vCard>"
                "</iq>");
    test.inject("<iq id='qx2' type='result'/>");

    expectFutureVariant<Success>(task);
}

void tst_QXmppVCardManager::testSetClientVCard()
{
    SKIP_IF_INTEGRATION_TESTS_DISABLED();

    auto client = std::make_unique<QXmppClient>();
    auto *vCardManager = client->findExtension<QXmppVCardManager>();
    auto config = IntegrationTests::clientConfiguration();

    QSignalSpy connectSpy(client.get(), &QXmppClient::connected);
    QSignalSpy disconnectSpy(client.get(), &QXmppClient::disconnected);
    QSignalSpy vCardSpy(vCardManager, &QXmppVCardManager::clientVCardReceived);

    // connect to server
    client->connectToServer(config);
    QVERIFY2(connectSpy.wait(), "Could not connect to server!");

    // request own vcard
    vCardManager->requestClientVCard();
    QVERIFY(vCardSpy.wait());

    // check our vcard has the correct address
    QCOMPARE(vCardManager->clientVCard().from(), client->configuration().jidBare());

    // set a new vcard
    QXmppVCardIq newVCard;
    newVCard.setFirstName(u"Bob"_s);
    newVCard.setBirthday(QDate(1, 2, 2000));
    newVCard.setEmail(u"bob@qxmpp.org"_s);
    vCardManager->setClientVCard(newVCard);

    // there's currently no signal to see whether the change was successful...

    QCoreApplication::processEvents();

    // reconnect
    client->disconnectFromServer();
    QVERIFY(disconnectSpy.wait());

    client->connectToServer(config);
    QVERIFY2(connectSpy.wait(), "Could not connect to server!");

    // request own vcard
    vCardManager->requestClientVCard();
    QVERIFY(vCardSpy.wait());

    // check our vcard has been changed successfully
    QCOMPARE(vCardManager->clientVCard().from(), client->configuration().jidBare());
    QCOMPARE(vCardManager->clientVCard().firstName(), u"Bob"_s);
    QCOMPARE(vCardManager->clientVCard().birthday(), QDate(01, 02, 2000));
    QCOMPARE(vCardManager->clientVCard().email(), u"bob@qxmpp.org"_s);

    // reset the vcard for future tests
    vCardManager->setClientVCard(QXmppVCardIq());

    // disconnect
    client->disconnectFromServer();
    QVERIFY(disconnectSpy.wait());
}

}  // namespace VCard

// ============================================================

namespace EntityTimeIq {

class tst_QXmppEntityTimeIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testEntityTimeGet();
    Q_SLOT void testEntityTimeResult();
};

void tst_QXmppEntityTimeIq::testEntityTimeGet()
{
    const QByteArray xml("<iq id=\"time_1\" "
                         "to=\"juliet@capulet.com/balcony\" "
                         "from=\"romeo@montague.net/orchard\" type=\"get\">"
                         "<time xmlns=\"urn:xmpp:time\"/>"
                         "</iq>");

    QXmppEntityTimeIq entityTime;
    parsePacket(entityTime, xml);
    QCOMPARE(entityTime.id(), QLatin1String("time_1"));
    QCOMPARE(entityTime.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(entityTime.from(), QLatin1String("romeo@montague.net/orchard"));
    QCOMPARE(entityTime.type(), QXmppIq::Get);
    serializePacket(entityTime, xml);
}

void tst_QXmppEntityTimeIq::testEntityTimeResult()
{
    const QByteArray xml(
        "<iq id=\"time_1\" to=\"romeo@montague.net/orchard\" from=\"juliet@capulet.com/balcony\" type=\"result\">"
        "<time xmlns=\"urn:xmpp:time\">"
        "<tzo>-06:00</tzo>"
        "<utc>2006-12-19T17:58:35Z</utc>"
        "</time>"
        "</iq>");

    QXmppEntityTimeIq entityTime;
    parsePacket(entityTime, xml);
    QCOMPARE(entityTime.id(), QLatin1String("time_1"));
    QCOMPARE(entityTime.from(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(entityTime.to(), QLatin1String("romeo@montague.net/orchard"));
    QCOMPARE(entityTime.type(), QXmppIq::Result);
    QCOMPARE(entityTime.tzo(), -21600);
    QCOMPARE(entityTime.utc(), QDateTime(QDate(2006, 12, 19), QTime(17, 58, 35), TimeZoneUTC));
    serializePacket(entityTime, xml);
}

}  // namespace EntityTimeIq

// ============================================================

namespace VCardIq {

class tst_QXmppVCardIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void vcardAddress_data();
    Q_SLOT void vcardAddress();
    Q_SLOT void vcardEmail_data();
    Q_SLOT void vcardEmail();
    Q_SLOT void vcardPhone_data();
    Q_SLOT void vcardPhone();
    Q_SLOT void vcardBase();
};

void tst_QXmppVCardIq::vcardAddress_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("country");
    QTest::addColumn<QString>("locality");
    QTest::addColumn<QString>("postcode");
    QTest::addColumn<QString>("region");
    QTest::addColumn<QString>("street");
    QTest::addColumn<bool>("equalsEmpty");

    QTest::newRow("none") << QByteArray("<ADR/>") << int(QXmppVCardAddress::None) << ""
                          << ""
                          << ""
                          << ""
                          << "" << true;
    QTest::newRow("HOME") << QByteArray("<ADR><HOME/></ADR>") << int(QXmppVCardAddress::Home) << ""
                          << ""
                          << ""
                          << ""
                          << "" << false;
    QTest::newRow("WORK") << QByteArray("<ADR><WORK/></ADR>") << int(QXmppVCardAddress::Work) << ""
                          << ""
                          << ""
                          << ""
                          << "" << false;
    QTest::newRow("POSTAL") << QByteArray("<ADR><POSTAL/></ADR>") << int(QXmppVCardAddress::Postal) << ""
                            << ""
                            << ""
                            << ""
                            << "" << false;
    QTest::newRow("PREF") << QByteArray("<ADR><PREF/></ADR>") << int(QXmppVCardAddress::Preferred) << ""
                          << ""
                          << ""
                          << ""
                          << "" << false;

    QTest::newRow("country") << QByteArray("<ADR><CTRY>France</CTRY></ADR>") << int(QXmppVCardAddress::None) << "France"
                             << ""
                             << ""
                             << ""
                             << "" << false;
    QTest::newRow("locality") << QByteArray("<ADR><LOCALITY>Paris</LOCALITY></ADR>") << int(QXmppVCardAddress::None) << ""
                              << "Paris"
                              << ""
                              << ""
                              << "" << false;
    QTest::newRow("postcode") << QByteArray("<ADR><PCODE>75008</PCODE></ADR>") << int(QXmppVCardAddress::None) << ""
                              << ""
                              << "75008"
                              << ""
                              << "" << false;
    QTest::newRow("region") << QByteArray("<ADR><REGION>Ile de France</REGION></ADR>") << int(QXmppVCardAddress::None) << ""
                            << ""
                            << ""
                            << "Ile de France"
                            << "" << false;
    QTest::newRow("street") << QByteArray("<ADR><STREET>55 rue du faubourg Saint-Honoré</STREET></ADR>") << int(QXmppVCardAddress::None) << ""
                            << ""
                            << ""
                            << "" << QString::fromUtf8("55 rue du faubourg Saint-Honoré") << false;
}

void tst_QXmppVCardIq::vcardAddress()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);
    QFETCH(QString, country);
    QFETCH(QString, locality);
    QFETCH(QString, postcode);
    QFETCH(QString, region);
    QFETCH(QString, street);
    QFETCH(bool, equalsEmpty);

    QXmppVCardAddress address;
    parsePacket(address, xml);
    QCOMPARE(int(address.type()), type);
    QCOMPARE(address.country(), country);
    QCOMPARE(address.locality(), locality);
    QCOMPARE(address.postcode(), postcode);
    QCOMPARE(address.region(), region);
    QCOMPARE(address.street(), street);
    serializePacket(address, xml);

    QXmppVCardAddress addressCopy = address;
    QVERIFY2(addressCopy == address, "QXmppVCardAddres::operator==() fails");
    QVERIFY2(!(addressCopy != address), "QXmppVCardAddres::operator!=() fails");

    QXmppVCardAddress emptyAddress;
    QCOMPARE(emptyAddress == address, equalsEmpty);
    QCOMPARE(emptyAddress != address, !equalsEmpty);
}

void tst_QXmppVCardIq::vcardEmail_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");

    QTest::newRow("none") << QByteArray("<EMAIL><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::None);
    QTest::newRow("HOME") << QByteArray("<EMAIL><HOME/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::Home);
    QTest::newRow("WORK") << QByteArray("<EMAIL><WORK/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::Work);
    QTest::newRow("INTERNET") << QByteArray("<EMAIL><INTERNET/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::Internet);
    QTest::newRow("X400") << QByteArray("<EMAIL><X400/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::X400);
    QTest::newRow("PREF") << QByteArray("<EMAIL><PREF/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::Preferred);
    QTest::newRow("all") << QByteArray("<EMAIL><HOME/><WORK/><INTERNET/><PREF/><X400/><USERID>foo.bar@example.com</USERID></EMAIL>") << int(QXmppVCardEmail::Home | QXmppVCardEmail::Work | QXmppVCardEmail::Internet | QXmppVCardEmail::Preferred | QXmppVCardEmail::X400);
}

void tst_QXmppVCardIq::vcardEmail()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppVCardEmail email;
    parsePacket(email, xml);
    QCOMPARE(email.address(), QLatin1String("foo.bar@example.com"));
    QCOMPARE(int(email.type()), type);
    serializePacket(email, xml);
}

void tst_QXmppVCardIq::vcardPhone_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");

    QTest::newRow("none") << QByteArray("<TEL><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::None);
    QTest::newRow("HOME") << QByteArray("<TEL><HOME/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Home);
    QTest::newRow("WORK") << QByteArray("<TEL><WORK/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Work);
    QTest::newRow("VOICE") << QByteArray("<TEL><VOICE/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Voice);
    QTest::newRow("FAX") << QByteArray("<TEL><FAX/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Fax);
    QTest::newRow("PAGER") << QByteArray("<TEL><PAGER/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Pager);
    QTest::newRow("MSG") << QByteArray("<TEL><MSG/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Messaging);
    QTest::newRow("CELL") << QByteArray("<TEL><CELL/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Cell);
    QTest::newRow("VIDEO") << QByteArray("<TEL><VIDEO/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Video);
    QTest::newRow("BBS") << QByteArray("<TEL><BBS/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::BBS);
    QTest::newRow("MODEM") << QByteArray("<TEL><MODEM/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Modem);
    QTest::newRow("IDSN") << QByteArray("<TEL><ISDN/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::ISDN);
    QTest::newRow("PCS") << QByteArray("<TEL><PCS/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::PCS);
    QTest::newRow("PREF") << QByteArray("<TEL><PREF/><NUMBER>12345</NUMBER></TEL>") << int(QXmppVCardPhone::Preferred);
}

void tst_QXmppVCardIq::vcardPhone()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppVCardPhone phone;
    parsePacket(phone, xml);
    QCOMPARE(phone.number(), QLatin1String("12345"));
    QCOMPARE(int(phone.type()), type);
    serializePacket(phone, xml);
}

void tst_QXmppVCardIq::vcardBase()
{
    const QByteArray xml(
        "<iq id=\"vcard1\" type=\"set\">"
        "<vCard xmlns=\"vcard-temp\">"
        "<ADR><CTRY>France</CTRY></ADR>"
        "<BDAY>1983-09-14</BDAY>"
        "<DESC>I like XMPP.</DESC>"
        "<EMAIL><INTERNET/><USERID>foo.bar@example.com</USERID></EMAIL>"
        "<FN>Foo Bar!</FN>"
        "<NICKNAME>FooBar</NICKNAME>"
        "<N><GIVEN>Foo</GIVEN><FAMILY>Wiz</FAMILY><MIDDLE>Baz</MIDDLE></N>"
        "<TEL><HOME/><NUMBER>12345</NUMBER></TEL>"
        "<TEL><WORK/><NUMBER>67890</NUMBER></TEL>"
        "<PHOTO>"
        "<TYPE>image/png</TYPE>"
        "<BINVAL>"
        "iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAIAAABLbSncAAAAAXNSR0IArs4c6QAAAAlwSFlzAAA"
        "UIgAAFCIBjw1HyAAAAAd0SU1FB9oIHQInNvuJovgAAAAiSURBVAjXY2TQ+s/AwMDAwPD/GiMDlP"
        "WfgYGBiQEHGJwSAK2BBQ1f3uvpAAAAAElFTkSuQmCC"
        "</BINVAL>"
        "</PHOTO>"
        "<URL>https://github.com/qxmpp-project/qxmpp/</URL>"
        "<ORG>"
        "<ORGNAME>QXmpp foundation</ORGNAME>"
        "<ORGUNIT>Main QXmpp dev unit</ORGUNIT>"
        "</ORG>"
        "<TITLE>Executive Director</TITLE>"
        "<ROLE>Patron Saint</ROLE>"
        "</vCard>"
        "</iq>");

    QXmppVCardIq vcard;
    parsePacket(vcard, xml);
    QCOMPARE(vcard.addresses().size(), 1);
    QCOMPARE(vcard.addresses()[0].country(), QLatin1String("France"));
    QCOMPARE(int(vcard.addresses()[0].type()), int(QXmppVCardEmail::None));
    QCOMPARE(vcard.birthday(), QDate(1983, 9, 14));
    QCOMPARE(vcard.description(), QLatin1String("I like XMPP."));
    QCOMPARE(vcard.email(), QLatin1String("foo.bar@example.com"));
    QCOMPARE(vcard.emails().size(), 1);
    QCOMPARE(vcard.emails()[0].address(), QLatin1String("foo.bar@example.com"));
    QCOMPARE(int(vcard.emails()[0].type()), int(QXmppVCardEmail::Internet));
    QCOMPARE(vcard.nickName(), QLatin1String("FooBar"));
    QCOMPARE(vcard.fullName(), QLatin1String("Foo Bar!"));
    QCOMPARE(vcard.firstName(), QLatin1String("Foo"));
    QCOMPARE(vcard.middleName(), QLatin1String("Baz"));
    QCOMPARE(vcard.lastName(), QLatin1String("Wiz"));
    QCOMPARE(vcard.phones().size(), 2);
    QCOMPARE(vcard.phones()[0].number(), QLatin1String("12345"));
    QCOMPARE(int(vcard.phones()[0].type()), int(QXmppVCardEmail::Home));
    QCOMPARE(vcard.phones()[1].number(), QLatin1String("67890"));
    QCOMPARE(int(vcard.phones()[1].type()), int(QXmppVCardEmail::Work));
    QCOMPARE(vcard.photo(), QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAIAAABLbSncAAAAAXNSR0IArs4c6QAAAAlwSFlzAAA"
                                                   "UIgAAFCIBjw1HyAAAAAd0SU1FB9oIHQInNvuJovgAAAAiSURBVAjXY2TQ+s/AwMDAwPD/GiMDlP"
                                                   "WfgYGBiQEHGJwSAK2BBQ1f3uvpAAAAAElFTkSuQmCC"));
    QCOMPARE(vcard.photoType(), QLatin1String("image/png"));
    QCOMPARE(vcard.url(), QLatin1String("https://github.com/qxmpp-project/qxmpp/"));

    const QXmppVCardOrganization &orgInfo = vcard.organization();
    QCOMPARE(orgInfo.organization(), QLatin1String("QXmpp foundation"));
    QCOMPARE(orgInfo.unit(), QLatin1String("Main QXmpp dev unit"));
    QCOMPARE(orgInfo.title(), QLatin1String("Executive Director"));
    QCOMPARE(orgInfo.role(), QLatin1String("Patron Saint"));

    serializePacket(vcard, xml);
}

}  // namespace VCardIq

// ============================================================

namespace VersionIq {

class tst_QXmppVersionIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void versionGet();
    Q_SLOT void versionResult();
};

void tst_QXmppVersionIq::versionGet()
{
    const QByteArray xmlGet(
        "<iq id=\"version_1\" to=\"juliet@capulet.com/balcony\" "
        "from=\"romeo@montague.net/orchard\" type=\"get\">"
        "<query xmlns=\"jabber:iq:version\"/></iq>");

    QXmppVersionIq verIqGet;
    parsePacket(verIqGet, xmlGet);
    QCOMPARE(verIqGet.id(), QLatin1String("version_1"));
    QCOMPARE(verIqGet.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(verIqGet.from(), QLatin1String("romeo@montague.net/orchard"));
    QCOMPARE(verIqGet.type(), QXmppIq::Get);
    serializePacket(verIqGet, xmlGet);
}

void tst_QXmppVersionIq::versionResult()
{
    const QByteArray xmlResult(
        "<iq id=\"version_1\" to=\"romeo@montague.net/orchard\" "
        "from=\"juliet@capulet.com/balcony\" type=\"result\">"
        "<query xmlns=\"jabber:iq:version\">"
        "<name>qxmpp</name>"
        "<os>Windows-XP</os>"
        "<version>0.2.0</version>"
        "</query></iq>");

    QXmppVersionIq verIqResult;
    parsePacket(verIqResult, xmlResult);
    QCOMPARE(verIqResult.id(), QLatin1String("version_1"));
    QCOMPARE(verIqResult.to(), QLatin1String("romeo@montague.net/orchard"));
    QCOMPARE(verIqResult.from(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(verIqResult.type(), QXmppIq::Result);
    QCOMPARE(verIqResult.name(), u"qxmpp"_s);
    QCOMPARE(verIqResult.version(), u"0.2.0"_s);
    QCOMPARE(verIqResult.os(), u"Windows-XP"_s);

    serializePacket(verIqResult, xmlResult);
}

}  // namespace VersionIq

QXMPP_TEST_MAIN(Version::tst_QXmppVersionManager, EntityTime::tst_QXmppEntityTimeManager, UserTune::tst_QXmppUserTuneManager, UserLocation::tst_QXmppUserLocationManager, VCard::tst_QXmppVCardManager, EntityTimeIq::tst_QXmppEntityTimeIq, VCardIq::tst_QXmppVCardIq, VersionIq::tst_QXmppVersionIq)

#include "tst_QXmppManagersUserInfo.moc"

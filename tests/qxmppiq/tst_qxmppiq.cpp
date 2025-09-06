// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2012 Oliver Goffart <ogoffart@woboq.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppArchiveIq.h"
#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryData.h"
#include "QXmppBitsOfBinaryDataList.h"
#include "QXmppDataForm.h"
#include "QXmppEntityTimeIq.h"
#include "QXmppExternalServiceDiscoveryIq.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppIq.h"
#include "QXmppNonSASLAuth.h"
#include "QXmppPushEnableIq.h"
#include "QXmppRegisterIq.h"
#include "QXmppRosterIq.h"
#include "QXmppRpcIq.h"
#include "QXmppStreamInitiationIq_p.h"
#include "QXmppVCardIq.h"
#include "QXmppVersionIq.h"

#include "util.h"

#include <QObject>

// helpers: RpcIq
static void checkVariant(const QVariant &value, const QByteArray &xml)
{
    // serialise
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter writer(&buffer);
    QXmppRpcMarshaller::marshall(&writer, value);
    if (xml != buffer.data()) {
        qDebug() << "expect " << xml;
        qDebug() << "writing" << buffer.data();
    }
    QCOMPARE(buffer.data(), xml);

    // parse
    QStringList errors;
    QVariant test = QXmppRpcMarshaller::demarshall(xmlToDom(xml), errors);
    if (!errors.isEmpty()) {
        qDebug() << errors;
    }
    QCOMPARE(errors, QStringList());
    QCOMPARE(test, value);
}

class tst_QXmppIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testBasic_data();
    Q_SLOT void testBasic();

    // ArchiveIq
    Q_SLOT void testArchiveList_data();
    Q_SLOT void testArchiveList();
    Q_SLOT void testArchiveChat_data();
    Q_SLOT void testArchiveChat();
    Q_SLOT void testArchiveRemove();
    Q_SLOT void testArchiveRetrieve_data();
    Q_SLOT void testArchiveRetrieve();

    // EntityTimeIq
    Q_SLOT void testEntityTimeGet();
    Q_SLOT void testEntityTimeResult();

    // ExternalServiceDiscoveryIq
    Q_SLOT void esdIsExternalService_data();
    Q_SLOT void esdIsExternalService();
    Q_SLOT void esdBase();

    Q_SLOT void esdIsExternalServiceDiscoveryIq_data();
    Q_SLOT void esdIsExternalServiceDiscoveryIq();
    Q_SLOT void esdIqBase();

    // HttpUploadIq
    Q_SLOT void httpUploadRequest();
    Q_SLOT void httpUploadIsRequest_data();
    Q_SLOT void httpUploadIsRequest();
    Q_SLOT void httpUploadSlot();
    Q_SLOT void httpUploadIsSlot_data();
    Q_SLOT void httpUploadIsSlot();

    // NonSaslAuthIq
    Q_SLOT void nonSaslAuthGet();
    Q_SLOT void nonSaslAuthSetPlain();
    Q_SLOT void nonSaslAuthSetDigest();

    // PushEnableIq
    Q_SLOT void pushEnable();
    Q_SLOT void pushDisable();
    Q_SLOT void pushEnableXmlNs();
    Q_SLOT void pushEnableDataForm();
    Q_SLOT void pushEnableIsEnableIq();

    // RegisterIq
    Q_SLOT void registerGet();
    Q_SLOT void registerResult();
    Q_SLOT void registerResultWithForm();
    Q_SLOT void registerResultWithRedirection();
    Q_SLOT void registerResultWithFormAndRedirection();
    Q_SLOT void registerSet();
    Q_SLOT void registerSetWithForm();
    Q_SLOT void registerBobData();
    Q_SLOT void registerRegistered();
    Q_SLOT void registerRemove();
    Q_SLOT void registerChangePassword();
    Q_SLOT void registerUnregistration();

    // RosterIq
    Q_SLOT void rosterItem_data();
    Q_SLOT void rosterItem();
    Q_SLOT void rosterApproved_data();
    Q_SLOT void rosterApproved();
    Q_SLOT void rosterVersion_data();
    Q_SLOT void rosterVersion();
    Q_SLOT void rosterMixAnnotate();
    Q_SLOT void rosterMixChannel();

    // RpcIq
    Q_SLOT void rpcBase64();
    Q_SLOT void rpcBool();
    Q_SLOT void rpcDateTime();
    Q_SLOT void rpcDouble();
    Q_SLOT void rpcInt();
    Q_SLOT void rpcNil();
    Q_SLOT void rpcString();

    Q_SLOT void rpcArray();
    Q_SLOT void rpcStruct();

    Q_SLOT void rpcInvoke();
    Q_SLOT void rpcResponse();
    Q_SLOT void rpcResponseFault();

    // StreamInitiationIq
#if BUILD_INTERNAL_TESTS
    Q_SLOT void streamInitiationFileInfo_data();
    Q_SLOT void streamInitiationFileInfo();
    Q_SLOT void streamInitiationOffer();
    Q_SLOT void streamInitiationResult();
#endif

    // VCardIq
    Q_SLOT void vcardAddress_data();
    Q_SLOT void vcardAddress();
    Q_SLOT void vcardEmail_data();
    Q_SLOT void vcardEmail();
    Q_SLOT void vcardPhone_data();
    Q_SLOT void vcardPhone();
    Q_SLOT void vcardBase();

    // VersionIq
    Q_SLOT void versionGet();
    Q_SLOT void versionResult();
};

void tst_QXmppIq::testBasic_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");

    QTest::newRow("get")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="get"/>)")
        << int(QXmppIq::Get);

    QTest::newRow("set")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="set"/>)")
        << int(QXmppIq::Set);

    QTest::newRow("result")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="result"/>)")
        << int(QXmppIq::Result);

    QTest::newRow("error")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="error"/>)")
        << int(QXmppIq::Error);
}

void tst_QXmppIq::testBasic()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"a");
    QCOMPARE(iq.to(), u"foo@example.com/QXmpp"_s);
    QCOMPARE(iq.from(), u"bar@example.com/QXmpp"_s);
    QCOMPARE(int(iq.type()), type);
    serializePacket(iq, xml);
}

void tst_QXmppIq::testArchiveList_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("max");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"list_1\" type=\"get\">"
                                   "<list xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\"/>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"list_1\" type=\"get\">"
                                     "<list xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\">"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<max>30</max>"
                                     "</set>"
                                     "</list>"
                                     "</iq>")
                              << 30;
}

void tst_QXmppIq::testArchiveList()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, max);

    QXmppArchiveListIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.id(), u"list_1");
    QCOMPARE(iq.with(), u"juliet@capulet.com");
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.end(), QDateTime(QDate(1479, 7, 21), QTime(4, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.resultSetQuery().max(), max);
    serializePacket(iq, xml);
}

void tst_QXmppIq::testArchiveChat_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("count");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"chat_1\" type=\"result\">"
                                   "<chat xmlns=\"urn:xmpp:archive\""
                                   " with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:56:15Z\""
                                   " subject=\"She speaks!\""
                                   " version=\"4\""
                                   ">"
                                   "<from secs=\"0\"><body>Art thou not Romeo, and a Montague?</body></from>"
                                   "<to secs=\"11\"><body>Neither, fair saint, if either thee dislike.</body></to>"
                                   "<from secs=\"7\"><body>How cam&apos;st thou hither, tell me, and wherefore?</body></from>"
                                   "</chat>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"chat_1\" type=\"result\">"
                                     "<chat xmlns=\"urn:xmpp:archive\""
                                     " with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:56:15Z\""
                                     " subject=\"She speaks!\""
                                     " version=\"4\""
                                     ">"
                                     "<from secs=\"0\"><body>Art thou not Romeo, and a Montague?</body></from>"
                                     "<to secs=\"11\"><body>Neither, fair saint, if either thee dislike.</body></to>"
                                     "<from secs=\"7\"><body>How cam&apos;st thou hither, tell me, and wherefore?</body></from>"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<count>3</count>"
                                     "</set>"
                                     "</chat>"
                                     "</iq>")
                              << 3;
}

void tst_QXmppIq::testArchiveChat()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, count);

    QXmppArchiveChatIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.id(), QLatin1String("chat_1"));
    QCOMPARE(iq.chat().with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.chat().messages().size(), 3);
    QCOMPARE(iq.chat().messages()[0].isReceived(), true);
    QCOMPARE(iq.chat().messages()[0].body(), QLatin1String("Art thou not Romeo, and a Montague?"));
    QCOMPARE(iq.chat().messages()[0].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 15), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[1].isReceived(), false);
    QCOMPARE(iq.chat().messages()[1].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 26), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[1].body(), QLatin1String("Neither, fair saint, if either thee dislike."));
    QCOMPARE(iq.chat().messages()[2].isReceived(), true);
    QCOMPARE(iq.chat().messages()[2].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 33), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[2].body(), QLatin1String("How cam'st thou hither, tell me, and wherefore?"));
    QCOMPARE(iq.resultSetReply().count(), count);
    serializePacket(iq, xml);
}

void tst_QXmppIq::testArchiveRemove()
{
    const QByteArray xml(
        "<iq id=\"remove_1\" type=\"set\">"
        "<remove xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
        " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\"/>"
        "</iq>");

    QXmppArchiveRemoveIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.id(), QLatin1String("remove_1"));
    QCOMPARE(iq.with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.end(), QDateTime(QDate(1479, 7, 21), QTime(4, 0, 0), TimeZoneUTC));
    serializePacket(iq, xml);
}

void tst_QXmppIq::testArchiveRetrieve_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("max");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"retrieve_1\" type=\"get\">"
                                   "<retrieve xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:00:00Z\"/>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"retrieve_1\" type=\"get\">"
                                     "<retrieve xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:00:00Z\">"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<max>30</max>"
                                     "</set>"
                                     "</retrieve>"
                                     "</iq>")
                              << 30;
}

void tst_QXmppIq::testArchiveRetrieve()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, max);

    QXmppArchiveRetrieveIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.id(), QLatin1String("retrieve_1"));
    QCOMPARE(iq.with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.resultSetQuery().max(), max);
    serializePacket(iq, xml);
}

void tst_QXmppIq::testEntityTimeGet()
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

void tst_QXmppIq::testEntityTimeResult()
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

void tst_QXmppIq::esdIsExternalService_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit' type='stun'/>")
        << true;
    QTest::newRow("invalidHost")
        << QByteArrayLiteral("<service type='stun'/>")
        << false;
    QTest::newRow("invalidHostEmpty")
        << QByteArrayLiteral("<service type='stun' host=''/>")
        << false;
    QTest::newRow("invalidType")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit'/>")
        << false;
    QTest::newRow("invalidTypeEmpty")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit' type=''/>")
        << false;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid host='stun.shakespeare.lit' type='stun'/>")
        << false;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid/>")
        << false;
}

void tst_QXmppIq::esdIsExternalService()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppExternalService::isExternalService(xmlToDom(xml)), isValid);
}

void tst_QXmppIq::esdBase()
{
    QByteArray xml { QByteArrayLiteral(
        "<service host='stun.shakespeare.lit'"
        " type='stun'"
        " port='9998'"
        " transport='udp'/>") };

    QXmppExternalService service;
    parsePacket(service, xml);
    QCOMPARE(service.host(), "stun.shakespeare.lit");
    QCOMPARE(service.port(), 9998);
    QCOMPARE(service.transport().has_value(), true);
    QCOMPARE(service.transport().value(), QXmppExternalService::Transport::Udp);
    QCOMPARE(service.type(), "stun");
    serializePacket(service, xml);
}

void tst_QXmppIq::esdIsExternalServiceDiscoveryIq_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<services xmlns='urn:xmpp:extdisco:2'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</services>"
               "</iq>")
        << true;

    QTest::newRow("invalidTag")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<invalid xmlns='urn:xmpp:extdisco:2'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</invalid>"
               "</iq>")
        << false;

    QTest::newRow("invalidNamespace")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<services xmlns='invalid'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</services>"
               "</iq>")
        << false;
}

void tst_QXmppIq::esdIsExternalServiceDiscoveryIq()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppExternalServiceDiscoveryIq::isExternalServiceDiscoveryIq(xmlToDom(xml)), isValid);
    QT_WARNING_POP
}

void tst_QXmppIq::esdIqBase()
{
    QXmpp::Private::globalStanzaIdCounter = 0;

    const QByteArray xml { QByteArrayLiteral(
        "<iq"
        " id='qx2'"
        " type='result'>"
        "<services xmlns='urn:xmpp:extdisco:2'>"
        "<service host='stun.shakespeare.lit'"
        " type='stun'"
        " port='9998'"
        " transport='udp'/>"
        "<service host='relay.shakespeare.lit'"
        " type='turn'"
        " password='jj929jkj5sadjfj93v3n'"
        " port='9999'"
        " transport='udp'"
        " username='nb78932lkjlskjfdb7g8'/>"
        "<service host='192.0.2.1'"
        " type='stun'"
        " port='8888'"
        " transport='udp'/>"
        "<service host='192.0.2.1'"
        " type='turn'"
        " password='93jn3bakj9s832lrjbbz'"
        " port='8889'"
        " transport='udp'"
        " username='auu98sjl2wk3e9fjdsl7'/>"
        "<service host='ftp.shakespeare.lit'"
        " type='ftp'"
        " name='Shakespearean File Server'"
        " password='guest'"
        " port='20'"
        " transport='tcp'"
        " username='guest'/>"
        "</services>"
        "</iq>") };

    QXmppExternalServiceDiscoveryIq iq1;
    iq1.setType(QXmppIq::Result);

    parsePacket(iq1, xml);
    QCOMPARE(iq1.externalServices().length(), 5);
    serializePacket(iq1, xml);

    QXmppExternalService service1;
    service1.setHost("127.0.0.1");
    service1.setType("ftp");

    iq1.addExternalService(service1);

    QXmppExternalService service2;
    service2.setHost("127.0.0.1");
    service2.setType("ftp");

    iq1.addExternalService(service2);

    QCOMPARE(iq1.externalServices().length(), 7);

    const QByteArray xml2 { QByteArrayLiteral(
        "<iq"
        " id='qx2'"
        " type='result'>"
        "<services xmlns='urn:xmpp:extdisco:2'>"
        "<service host='193.169.1.256'"
        " type='turn'/>"
        "<service host='194.170.2.257'"
        " type='stun'/>"
        "<service host='195.171.3.258'"
        " type='ftp'/>"
        "</services>"
        "</iq>") };

    QXmppExternalServiceDiscoveryIq iq2;
    iq2.setType(QXmppIq::Result);

    QXmppExternalService service3;
    service3.setHost("193.169.1.256");
    service3.setType("turn");
    QXmppExternalService service4;
    service4.setHost("194.170.2.257");
    service4.setType("stun");
    QXmppExternalService service5;
    service5.setHost("195.171.3.258");
    service5.setType("ftp");

    iq2.setExternalServices({ service3, service4, service5 });

    QCOMPARE(iq2.externalServices().length(), 3);
    serializePacket(iq2, xml2);
}

void tst_QXmppIq::httpUploadRequest()
{
    const QByteArray xml(
        "<iq id=\"step_03\" "
        "to=\"upload.montague.tld\" "
        "from=\"romeo@montague.tld/garden\" "
        "type=\"get\">"
        "<request xmlns=\"urn:xmpp:http:upload:0\" "
        "filename=\"très cool.jpg\" "
        "size=\"23456\" "
        "content-type=\"image/jpeg\"/>"
        "</iq>");

    QXmppHttpUploadRequestIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.fileName(), u"très cool.jpg"_s);
    QCOMPARE(iq.size(), 23456);
    QCOMPARE(iq.contentType().name(), u"image/jpeg"_s);
    serializePacket(iq, xml);

    // test setters
    iq.setFileName("icon.png");
    QCOMPARE(iq.fileName(), u"icon.png"_s);
    iq.setSize(23421337);
    QCOMPARE(iq.size(), 23421337);
    iq.setContentType(QMimeDatabase().mimeTypeForName("image/png"));
    QCOMPARE(iq.contentType().name(), u"image/png"_s);
}

void tst_QXmppIq::httpUploadIsRequest_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isRequest");

    QTest::newRow("empty-iq")
        << QByteArray("<iq/>")
        << false;
    QTest::newRow("wrong-ns")
        << QByteArray("<iq><request xmlns=\"some:other:request\"/></iq>")
        << false;
    QTest::newRow("correct")
        << QByteArray("<iq><request xmlns=\"urn:xmpp:http:upload:0\"/></iq>")
        << true;
}

void tst_QXmppIq::httpUploadIsRequest()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isRequest);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppHttpUploadRequestIq::isHttpUploadRequestIq(xmlToDom(xml)), isRequest);
    QT_WARNING_POP
}

void tst_QXmppIq::httpUploadSlot()
{
    const QByteArray xml(
        "<iq id=\"step_03\" "
        "to=\"romeo@montague.tld/garden\" "
        "from=\"upload.montague.tld\" "
        "type=\"result\">"
        "<slot xmlns=\"urn:xmpp:http:upload:0\">"
        "<put url=\"https://upload.montague.tld/4a771ac1-f0b2-4a4a-970"
        "0-f2a26fa2bb67/tr%C3%A8s%20cool.jpg\">"
        "<header name=\"Authorization\">Basic Base64String==</header>"
        "<header name=\"Cookie\">foo=bar; user=romeo</header>"
        "</put>"
        "<get url=\"https://download.montague.tld/4a771ac1-f0b2-4a4a-9"
        "700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg\"/>"
        "</slot>"
        "</iq>");

    QXmppHttpUploadSlotIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.putUrl(), QUrl("https://upload.montague.tld/4a771ac1-f0b2-4a4a"
                               "-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    QCOMPARE(iq.getUrl(), QUrl("https://download.montague.tld/4a771ac1-f0b2-4a"
                               "4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    QMap<QString, QString> headers;
    headers["Authorization"] = "Basic Base64String==";
    headers["Cookie"] = "foo=bar; user=romeo";
    QCOMPARE(iq.putHeaders(), headers);
    serializePacket(iq, xml);

    // test setters
    iq.setGetUrl(QUrl("https://dl.example.org/user/file"));
    QCOMPARE(iq.getUrl(), QUrl("https://dl.example.org/user/file"));
    iq.setPutUrl(QUrl("https://ul.example.org/user/file"));
    QCOMPARE(iq.putUrl(), QUrl("https://ul.example.org/user/file"));
    QMap<QString, QString> emptyMap;
    iq.setPutHeaders(emptyMap);
    QCOMPARE(iq.putHeaders(), emptyMap);
}

void tst_QXmppIq::httpUploadIsSlot_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isSlot");

    QTest::newRow("empty-iq")
        << QByteArray("<iq/>")
        << false;
    QTest::newRow("wrong-ns")
        << QByteArray("<iq><slot xmlns=\"some:other:slot\"/></iq>")
        << false;
    QTest::newRow("correct")
        << QByteArray("<iq><slot xmlns=\"urn:xmpp:http:upload:0\"/></iq>")
        << true;
}

void tst_QXmppIq::httpUploadIsSlot()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isSlot);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppHttpUploadSlotIq::isHttpUploadSlotIq(xmlToDom(xml)), isSlot);
    QT_WARNING_POP
}

void tst_QXmppIq::nonSaslAuthGet()
{
    // Client requests authentication fields from server
    const QByteArray xml(
        "<iq id=\"auth1\" to=\"shakespeare.lit\" type=\"get\">"
        "<query xmlns=\"jabber:iq:auth\"/>"
        "</iq>");

    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    serializePacket(iq, xml);
}

void tst_QXmppIq::nonSaslAuthSetPlain()
{
    // Client provides required information (plain)
    const QByteArray xml(
        "<iq id=\"auth2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:auth\">"
        "<username>bill</username>"
        "<password>Calli0pe</password>"
        "<resource>globe</resource>"
        "</query>"
        "</iq>");
    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.digest(), QByteArray());
    QCOMPARE(iq.password(), QLatin1String("Calli0pe"));
    QCOMPARE(iq.resource(), QLatin1String("globe"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::nonSaslAuthSetDigest()
{
    // Client provides required information (digest)
    const QByteArray xml(
        "<iq id=\"auth2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:auth\">"
        "<username>bill</username>"
        "<digest>48fc78be9ec8f86d8ce1c39c320c97c21d62334d</digest>"
        "<resource>globe</resource>"
        "</query>"
        "</iq>");
    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.digest(), QByteArray("\x48\xfc\x78\xbe\x9e\xc8\xf8\x6d\x8c\xe1\xc3\x9c\x32\x0c\x97\xc2\x1d\x62\x33\x4d"));
    QCOMPARE(iq.password(), QString());
    QCOMPARE(iq.resource(), QLatin1String("globe"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::pushEnable()
{
    const QByteArray xml(
        R"(<iq id="x42" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.mode(), QXmppPushEnableIq::Enable);
    QCOMPARE(iq.jid(), u"push-5.client.example"_s);
    QCOMPARE(iq.node(), u"yxs32uqsflafdk3iuqo"_s);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;
    sIq.setJid("push-5.client.example");
    sIq.setMode(QXmppPushEnableIq::Enable);
    sIq.setNode("yxs32uqsflafdk3iuqo");
    sIq.setType(QXmppIq::Set);
    sIq.setId("x42");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushDisable()
{
    const QByteArray xml(
        R"(<iq id="x97" type="set">)"
        R"(<disable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.mode(), QXmppPushEnableIq::Disable);
    QCOMPARE(iq.jid(), u"push-5.client.example"_s);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;
    sIq.setJid("push-5.client.example");
    sIq.setMode(QXmppPushEnableIq::Disable);
    sIq.setNode("yxs32uqsflafdk3iuqo");
    sIq.setType(QXmppIq::Set);
    sIq.setId("x97");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushEnableXmlNs()
{
    const QByteArray xml(
        R"(<iq type="set" id="x97">)"
        R"(<disable xmlns="urn:ympp:wrongns:0" jid="push-5.client.example"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.jid().isEmpty());
}

void tst_QXmppIq::pushEnableDataForm()
{
    const QByteArray xml(
        R"(<iq id="x43" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo">)"
        R"(<x xmlns="jabber:x:data" type="submit">)"
        R"(<field type="hidden" var="FORM_TYPE"><value>http://jabber.org/protocol/pubsub#publish-options</value></field>)"
        R"(<field type="text-single" var="secret"><value>eruio234vzxc2kla-91</value></field>)"
        "</x>"
        "</enable>"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QVERIFY(!iq.dataForm().isNull());
    QCOMPARE(iq.dataForm().constFields().size(), 2);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;

    QXmppDataForm::Field field0;
    field0.setKey("FORM_TYPE");
    field0.setType(QXmppDataForm::Field::HiddenField);
    field0.setValue("http://jabber.org/protocol/pubsub#publish-options");

    QXmppDataForm::Field field1;
    field1.setKey("secret");
    field1.setValue("eruio234vzxc2kla-91");

    QXmppDataForm form;
    form.setType(QXmppDataForm::Submit);
    form.setFields({ field0, field1 });

    sIq.setDataForm(form);

    sIq.setType(QXmppIq::Set);
    sIq.setMode(QXmppPushEnableIq::Enable);
    sIq.setId("x43");
    sIq.setJid("push-5.client.example");
    sIq.setNode("yxs32uqsflafdk3iuqo");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushEnableIsEnableIq()
{
    const QByteArray xml(
        R"(<iq id="x42" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QVERIFY(QXmppPushEnableIq::isPushEnableIq(xmlToDom(xml)));

    const QByteArray xml2(
        R"(<iq id="x97" type="set">)"
        R"(<disable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QVERIFY(QXmppPushEnableIq::isPushEnableIq(xmlToDom(xml2)));
}

void tst_QXmppIq::registerGet()
{
    const QByteArray xml(
        "<iq id=\"reg1\" to=\"shakespeare.lit\" type=\"get\">"
        "<query xmlns=\"jabber:iq:register\"/>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg1"));
    QCOMPARE(iq.to(), QLatin1String("shakespeare.lit"));
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.instructions(), QString());
    QVERIFY(!iq.isRegistered());
    QVERIFY(!iq.isRemove());
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerResult()
{
    const QByteArray xml(
        "<iq id=\"reg1\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Choose a username and password for use with this service. Please also provide your email address.</instructions>"
        "<username/>"
        "<password/>"
        "<email/>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg1"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Choose a username and password for use with this service. Please also provide your email address."));
    QVERIFY(!iq.username().isNull());
    QVERIFY(iq.username().isEmpty());
    QVERIFY(!iq.password().isNull());
    QVERIFY(iq.password().isEmpty());
    QVERIFY(!iq.email().isNull());
    QVERIFY(iq.email().isEmpty());
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerResultWithForm()
{
    const QByteArray xml(
        "<iq id=\"reg3\" to=\"juliet@capulet.com/balcony\" from=\"contests.shakespeare.lit\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php</instructions>"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<title>Contest Registration</title>"
        "<instructions>"
        "Please provide the following information"
        "to sign up for our special contests!"
        "</instructions>"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<required/>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<option label=\"Male\"><value>M</value></option>"
        "<option label=\"Female\"><value>F</value></option>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.from(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QCOMPARE(iq.form().title(), QLatin1String("Contest Registration"));
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerResultWithRedirection()
{
    const QByteArray xml(
        "<iq id=\"reg3\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>"
        "To register, visit http://www.shakespeare.lit/contests.php"
        "</instructions>"
        "<x xmlns=\"jabber:x:oob\">"
        "<url>http://www.shakespeare.lit/contests.php</url>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("To register, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(iq.form().isNull());
    QCOMPARE(iq.outOfBandUrl(), QLatin1String("http://www.shakespeare.lit/contests.php"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerResultWithFormAndRedirection()
{
    const QByteArray xml(
        "<iq id=\"reg3\" to=\"juliet@capulet.com/balcony\" from=\"contests.shakespeare.lit\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php</instructions>"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<title>Contest Registration</title>"
        "<instructions>"
        "Please provide the following information"
        "to sign up for our special contests!"
        "</instructions>"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<required/>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<option label=\"Male\"><value>M</value></option>"
        "<option label=\"Female\"><value>F</value></option>"
        "</field>"
        "</x>"
        "<x xmlns=\"jabber:x:oob\">"
        "<url>http://www.shakespeare.lit/contests.php</url>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.from(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QCOMPARE(iq.form().title(), QLatin1String("Contest Registration"));
    QCOMPARE(iq.outOfBandUrl(), QLatin1String("http://www.shakespeare.lit/contests.php"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerSet()
{
    const QByteArray xml(
        "<iq id=\"reg2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<username>bill</username>"
        "<password>Calliope</password>"
        "<email>bard@shakespeare.lit</email>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg2"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.password(), QLatin1String("Calliope"));
    QCOMPARE(iq.email(), QLatin1String("bard@shakespeare.lit"));
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerSetWithForm()
{
    const QByteArray xml(
        "<iq id=\"reg4\" to=\"contests.shakespeare.lit\" from=\"juliet@capulet.com/balcony\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<x xmlns=\"jabber:x:data\" type=\"submit\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<value>Juliet</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<value>Capulet</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<value>juliet@capulet.com</value>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<value>F</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg4"));
    QCOMPARE(iq.to(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);

    QXmppRegisterIq sIq;
    sIq.setId(QLatin1String("reg4"));
    sIq.setTo(QLatin1String("contests.shakespeare.lit"));
    sIq.setFrom(QLatin1String("juliet@capulet.com/balcony"));
    sIq.setType(QXmppIq::Set);
    sIq.setForm(QXmppDataForm(
        QXmppDataForm::Submit,
        QList<QXmppDataForm::Field>()
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::HiddenField,
                   u"FORM_TYPE"_s,
                   u"jabber:iq:register"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"first"_s,
                   u"Juliet"_s,
                   false,
                   u"Given Name"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"last"_s,
                   u"Capulet"_s,
                   false,
                   u"Family Name"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"email"_s,
                   u"juliet@capulet.com"_s,
                   false,
                   u"Email Address"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::ListSingleField,
                   u"x-gender"_s,
                   u"F"_s,
                   false,
                   u"Gender"_s)));
    serializePacket(sIq, xml);
}

void tst_QXmppIq::registerBobData()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<data xmlns=\"urn:xmpp:bob\" "
        "cid=\"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org\" "
        "type=\"image/png\">"
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC"
        "</data>"
        "</query>"
        "</iq>");

    QXmppBitsOfBinaryData data;
    data.setCid(QXmppBitsOfBinaryContentId::fromContentId(
        u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s));
    data.setContentType(QMimeDatabase().mimeTypeForName(u"image/png"_s));
    data.setData(QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC")));

    QXmppRegisterIq parsedIq;
    parsePacket(parsedIq, xml);
    QCOMPARE(parsedIq.type(), QXmppIq::Result);
    QCOMPARE(parsedIq.id(), u""_s);
    QCOMPARE(parsedIq.bitsOfBinaryData().size(), 1);
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid().algorithm(), data.cid().algorithm());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid().hash(), data.cid().hash());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid(), data.cid());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().contentType(), data.contentType());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().maxAge(), data.maxAge());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().data(), data.data());
    QCOMPARE(parsedIq.bitsOfBinaryData().first(), data);
    serializePacket(parsedIq, xml);

    QXmppRegisterIq iq;
    iq.setType(QXmppIq::Result);
    iq.setId(u""_s);
    QXmppBitsOfBinaryDataList bobDataList;
    bobDataList << data;
    iq.setBitsOfBinaryData(bobDataList);
    serializePacket(iq, xml);

    QXmppRegisterIq iq2;
    iq2.setType(QXmppIq::Result);
    iq2.setId(u""_s);
    iq2.bitsOfBinaryData() << data;
    serializePacket(iq2, xml);

    // test const getter
    const QXmppRegisterIq constIq = iq;
    QCOMPARE(constIq.bitsOfBinaryData(), iq.bitsOfBinaryData());
}

void tst_QXmppIq::registerRegistered()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<registered/>"
        "<username>juliet</username>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.isRegistered());
    QCOMPARE(iq.username(), u"juliet"_s);
    serializePacket(iq, xml);

    iq = QXmppRegisterIq();
    iq.setId(u""_s);
    iq.setType(QXmppIq::Result);
    iq.setIsRegistered(true);
    iq.setUsername(u"juliet"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerRemove()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<remove/>"
        "<username>juliet</username>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.isRemove());
    QCOMPARE(iq.username(), u"juliet"_s);
    serializePacket(iq, xml);

    iq = QXmppRegisterIq();
    iq.setId(u""_s);
    iq.setType(QXmppIq::Result);
    iq.setIsRemove(true);
    iq.setUsername(u"juliet"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerChangePassword()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"changePassword1\" to=\"shakespeare.lit\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<username>bill</username>"
        "<password>m1cr0$0ft</password>"
        "</query>"
        "</iq>");

    auto iq = QXmppRegisterIq::createChangePasswordRequest(
        u"bill"_s,
        u"m1cr0$0ft"_s,
        u"shakespeare.lit"_s);
    iq.setId(u"changePassword1"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::registerUnregistration()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"unreg1\" to=\"shakespeare.lit\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<remove/>"
        "</query>"
        "</iq>");

    auto iq = QXmppRegisterIq::createUnregistrationRequest(u"shakespeare.lit"_s);
    iq.setId(u"unreg1"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::rosterItem_data()
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

void tst_QXmppIq::rosterItem()
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

void tst_QXmppIq::rosterApproved_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("approved");

    QTest::newRow("true") << QByteArray(R"(<item jid="foo@example.com" approved="true"/>)") << true;
    QTest::newRow("1") << QByteArray(R"(<item jid="foo@example.com" approved="1"/>)") << true;
    QTest::newRow("false") << QByteArray(R"(<item jid="foo@example.com" approved="false"/>)") << false;
    QTest::newRow("0") << QByteArray(R"(<item jid="foo@example.com" approved="0"/>)") << false;
    QTest::newRow("empty") << QByteArray(R"(<item jid="foo@example.com"/>)") << false;
}

void tst_QXmppIq::rosterApproved()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, approved);

    QXmppRosterIq::Item item;
    parsePacket(item, xml);
    QCOMPARE(item.isApproved(), approved);
}

void tst_QXmppIq::rosterVersion_data()
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

void tst_QXmppIq::rosterVersion()
{
    QFETCH(QByteArray, xml);
    QFETCH(QString, version);

    QXmppRosterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.version(), version);
    serializePacket(iq, xml);
}

void tst_QXmppIq::rosterMixAnnotate()
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

void tst_QXmppIq::rosterMixChannel()
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

void tst_QXmppIq::rpcBase64()
{
    checkVariant(QByteArray("\0\1\2\3", 4),
                 QByteArray("<value><base64>AAECAw==</base64></value>"));
}

void tst_QXmppIq::rpcBool()
{
    checkVariant(false,
                 QByteArray("<value><boolean>false</boolean></value>"));
    checkVariant(true,
                 QByteArray("<value><boolean>true</boolean></value>"));
}

void tst_QXmppIq::rpcDateTime()
{
    checkVariant(QDateTime(QDate(1998, 7, 17), QTime(14, 8, 55)),
                 QByteArray("<value><dateTime.iso8601>1998-07-17T14:08:55</dateTime.iso8601></value>"));
}

void tst_QXmppIq::rpcDouble()
{
    checkVariant(double(-12.214),
                 QByteArray("<value><double>-12.214</double></value>"));
}

void tst_QXmppIq::rpcInt()
{
    checkVariant(int(-12),
                 QByteArray("<value><i4>-12</i4></value>"));
}

void tst_QXmppIq::rpcNil()
{
    checkVariant(QVariant(),
                 QByteArray("<value><nil/></value>"));
}

void tst_QXmppIq::rpcString()
{
    checkVariant(u"hello world"_s,
                 QByteArray("<value><string>hello world</string></value>"));
}

void tst_QXmppIq::rpcArray()
{
    checkVariant(QVariantList() << u"hello world"_s << double(-12.214),
                 QByteArray("<value><array><data>"
                            "<value><string>hello world</string></value>"
                            "<value><double>-12.214</double></value>"
                            "</data></array></value>"));
}

void tst_QXmppIq::rpcStruct()
{
    QMap<QString, QVariant> map;
    map["bar"] = u"hello \n world"_s;
    map["foo"] = double(-12.214);
    checkVariant(map,
                 QByteArray("<value><struct>"
                            "<member>"
                            "<name>bar</name>"
                            "<value><string>hello \n world</string></value>"
                            "</member>"
                            "<member>"
                            "<name>foo</name>"
                            "<value><double>-12.214</double></value>"
                            "</member>"
                            "</struct></value>"));
}

void tst_QXmppIq::rpcInvoke()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"responder@company-a.com/jrpc-server\""
        " from=\"requester@company-b.com/jrpc-client\""
        " type=\"set\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodCall>"
        "<methodName>examples.getStateName</methodName>"
        "<params>"
        "<param>"
        "<value><i4>6</i4></value>"
        "</param>"
        "<param>"
        "<value><string>two\nlines</string></value>"
        "</param>"
        "<param>"
        "<value><string><![CDATA[\n\n]]></string></value>"
        "</param>"
        "</params>"
        "</methodCall>"
        "</query>"
        "</iq>");

    QXmppRpcInvokeIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.method(), u"examples.getStateName");
    QCOMPARE(iq.arguments(), QVariantList() << int(6) << u"two\nlines"_s << u"\n\n"_s);

    const auto data = packetToXml(iq);
    if (data != xml) {
        qDebug() << "expect " << xml;
        qDebug() << "writing" << data;
    }
    QCOMPARE(data, xml);
}

void tst_QXmppIq::rpcResponse()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"requester@company-b.com/jrpc-client\""
        " from=\"responder@company-a.com/jrpc-server\""
        " type=\"result\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodResponse>"
        "<params>"
        "<param>"
        "<value><string>Colorado</string></value>"
        "</param>"
        "</params>"
        "</methodResponse>"
        "</query>"
        "</iq>");

    QXmppRpcResponseIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.faultCode(), 0);
    QCOMPARE(iq.faultString(), QString());
    QCOMPARE(iq.values(), QVariantList() << u"Colorado"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::rpcResponseFault()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"requester@company-b.com/jrpc-client\""
        " from=\"responder@company-a.com/jrpc-server\""
        " type=\"result\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodResponse>"
        "<fault>"
        "<value>"
        "<struct>"
        "<member>"
        "<name>faultCode</name>"
        "<value><i4>404</i4></value>"
        "</member>"
        "<member>"
        "<name>faultString</name>"
        "<value><string>Not found</string></value>"
        "</member>"
        "</struct>"
        "</value>"
        "</fault>"
        "</methodResponse>"
        "</query>"
        "</iq>");

    QXmppRpcResponseIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.faultCode(), 404);
    QCOMPARE(iq.faultString(), QLatin1String("Not found"));
    QCOMPARE(iq.values(), QVariantList());
    serializePacket(iq, xml);
}

#if BUILD_INTERNAL_TESTS
void tst_QXmppIq::streamInitiationFileInfo_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QDateTime>("date");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QByteArray>("hash");
    QTest::addColumn<QString>("name");
    QTest::addColumn<qint64>("size");

    QTest::newRow("normal")
        << QByteArray("<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" name=\"test.txt\" size=\"1022\"/>")
        << QDateTime().toUTC()
        << QString()
        << QByteArray()
        << u"test.txt"_s
        << qint64(1022);

    QTest::newRow("full")
        << QByteArray("<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" "
                      "date=\"1969-07-21T02:56:15Z\" "
                      "hash=\"552da749930852c69ae5d2141d3766b1\" "
                      "name=\"test.txt\" "
                      "size=\"1022\">"
                      "<desc>This is a test. If this were a real file...</desc>"
                      "</file>")
        << QDateTime(QDate(1969, 7, 21), QTime(2, 56, 15), TimeZoneUTC)
        << u"This is a test. If this were a real file..."_s
        << QByteArray::fromHex("552da749930852c69ae5d2141d3766b1")
        << u"test.txt"_s
        << qint64(1022);
}

void tst_QXmppIq::streamInitiationFileInfo()
{
    QFETCH(QByteArray, xml);
    QFETCH(QDateTime, date);
    QFETCH(QString, description);
    QFETCH(QByteArray, hash);
    QFETCH(QString, name);
    QFETCH(qint64, size);

    QXmppTransferFileInfo info;
    parsePacket(info, xml);
    QCOMPARE(info.date(), date);
    QCOMPARE(info.description(), description);
    QCOMPARE(info.hash(), hash);
    QCOMPARE(info.name(), name);
    QCOMPARE(info.size(), size);
    serializePacket(info, xml);
}

void tst_QXmppIq::streamInitiationOffer()
{
    QByteArray xml(
        "<iq id=\"offer1\" to=\"receiver@jabber.org/resource\" type=\"set\">"
        "<si xmlns=\"http://jabber.org/protocol/si\" id=\"a0\" mime-type=\"text/plain\" profile=\"http://jabber.org/protocol/si/profile/file-transfer\">"
        "<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" name=\"test.txt\" size=\"1022\"/>"
        "<feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<field type=\"list-single\" var=\"stream-method\">"
        "<option><value>http://jabber.org/protocol/bytestreams</value></option>"
        "<option><value>http://jabber.org/protocol/ibb</value></option>"
        "</field>"
        "</x>"
        "</feature>"
        "</si>"
        "</iq>");

    QXmppStreamInitiationIq iq;
    parsePacket(iq, xml);
    QVERIFY(!iq.featureForm().isNull());
    QVERIFY(!iq.fileInfo().isNull());
    QCOMPARE(iq.fileInfo().name(), u"test.txt"_s);
    QCOMPARE(iq.fileInfo().size(), qint64(1022));
    serializePacket(iq, xml);
}

void tst_QXmppIq::streamInitiationResult()
{
    QByteArray xml(
        "<iq id=\"offer1\" to=\"sender@jabber.org/resource\" type=\"result\">"
        "<si xmlns=\"http://jabber.org/protocol/si\">"
        "<feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
        "<x xmlns=\"jabber:x:data\" type=\"submit\">"
        "<field type=\"list-single\" var=\"stream-method\">"
        "<value>http://jabber.org/protocol/bytestreams</value>"
        "</field>"
        "</x>"
        "</feature>"
        "</si>"
        "</iq>");

    QXmppStreamInitiationIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.fileInfo().isNull());
    serializePacket(iq, xml);
}
#endif  // BUILD_INTERNAL_TESTS

void tst_QXmppIq::vcardAddress_data()
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

void tst_QXmppIq::vcardAddress()
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

void tst_QXmppIq::vcardEmail_data()
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

void tst_QXmppIq::vcardEmail()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppVCardEmail email;
    parsePacket(email, xml);
    QCOMPARE(email.address(), QLatin1String("foo.bar@example.com"));
    QCOMPARE(int(email.type()), type);
    serializePacket(email, xml);
}

void tst_QXmppIq::vcardPhone_data()
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

void tst_QXmppIq::vcardPhone()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppVCardPhone phone;
    parsePacket(phone, xml);
    QCOMPARE(phone.number(), QLatin1String("12345"));
    QCOMPARE(int(phone.type()), type);
    serializePacket(phone, xml);
}

void tst_QXmppIq::vcardBase()
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

void tst_QXmppIq::versionGet()
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

void tst_QXmppIq::versionResult()
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

QTEST_MAIN(tst_QXmppIq)
#include "tst_qxmppiq.moc"

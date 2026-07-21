// SPDX-FileCopyrightText: 2012 Oliver Goffart <ogoffart@woboq.com>
// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2012 Andrey Batyiev
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary: merges tst_QXmppPresence, tst_QXmppBitsOfBinary, tst_QXmppStreamFeatures, tst_QXmppDataForm, tst_QXmppResultSet, tst_QXmppSpamReport, tst_QXmppSceEnvelope, tst_QXmppUri into one translation
// unit so the shared Qt/QXmpp headers are parsed once instead of N times.
// main() runs each test class in turn via QTest::qExec().

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryDataList.h"
#include "QXmppBitsOfBinaryIq.h"
#include "QXmppDataForm.h"
#include "QXmppPresence.h"
#include "QXmppResultSet.h"
#include "QXmppSasl_p.h"
#include "QXmppSceEnvelope_p.h"
#include "QXmppSpamReport.h"
#include "QXmppStreamFeatures.h"
#include "QXmppUri.h"
#include "QXmppXmlElement.h"

#include "util.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMimeDatabase>
#include <QMimeType>
#include <QObject>
#include <QtGlobal>

Q_DECLARE_METATYPE(QCryptographicHash::Algorithm)

using namespace QXmpp::Private;
namespace Uri = QXmpp::Uri;

// ===================== tst_QXmppPresence =====================

class tst_QXmppPresence : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testPresence();
    Q_SLOT void testPresence_data();
    Q_SLOT void testPresenceWithCapability();
    Q_SLOT void testPresenceWithExtendedAddresses();
    Q_SLOT void testPresenceWithMucItem();
    Q_SLOT void testPresenceWithMucPassword();
    Q_SLOT void testPresenceWithMucSupport();
    Q_SLOT void testPresenceWithMuji();
    Q_SLOT void testPresenceWithLastUserInteraction();
    Q_SLOT void mucOccupantId();
    Q_SLOT void testPresenceWithMix();
    Q_SLOT void testPresenceWithVCard();
};

void tst_QXmppPresence::testPresence_data()
{
    QXmppPresence foo;

    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("priority");
    QTest::addColumn<int>("statusType");
    QTest::addColumn<QString>("statusText");
    QTest::addColumn<int>("vcardUpdate");
    QTest::addColumn<QByteArray>("photoHash");

    // presence type
    QTest::newRow("available") << QByteArray("<presence/>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("unavailable") << QByteArray("<presence type=\"unavailable\"/>") << int(QXmppPresence::Unavailable) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("error") << QByteArray("<presence type=\"error\"/>") << int(QXmppPresence::Error) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("subscribe") << QByteArray("<presence type=\"subscribe\"/>") << int(QXmppPresence::Subscribe) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("unsubscribe") << QByteArray("<presence type=\"unsubscribe\"/>") << int(QXmppPresence::Unsubscribe) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("subscribed") << QByteArray("<presence type=\"subscribed\"/>") << int(QXmppPresence::Subscribed) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("unsubscribed") << QByteArray("<presence type=\"unsubscribed\"/>") << int(QXmppPresence::Unsubscribed) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("probe") << QByteArray("<presence type=\"probe\"/>") << int(QXmppPresence::Probe) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();

    // status text + priority
    QTest::newRow("full") << QByteArray("<presence><show>away</show><status>In a meeting</status><priority>5</priority></presence>") << int(QXmppPresence::Available) << 5 << int(QXmppPresence::Away) << "In a meeting" << int(QXmppPresence::VCardUpdateNone) << QByteArray();

    // status type
    QTest::newRow("away") << QByteArray("<presence><show>away</show></presence>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Away) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("dnd") << QByteArray("<presence><show>dnd</show></presence>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::DND) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("chat") << QByteArray("<presence><show>chat</show></presence>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Chat) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("xa") << QByteArray("<presence><show>xa</show></presence>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::XA) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();
    QTest::newRow("invisible") << QByteArray("<presence><show>invisible</show></presence>") << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Invisible) << "" << int(QXmppPresence::VCardUpdateNone) << QByteArray();

    // photo
    QTest::newRow("vcard-photo") << QByteArray(
                                        "<presence>"
                                        "<x xmlns=\"vcard-temp:x:update\">"
                                        "<photo>73b908bc</photo>"
                                        "</x>"
                                        "</presence>")
                                 << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateValidPhoto) << QByteArray::fromHex("73b908bc");

    QTest::newRow("vard-not-ready") << QByteArray(
                                           "<presence>"
                                           "<x xmlns=\"vcard-temp:x:update\"/>"
                                           "</presence>")
                                    << int(QXmppPresence::Available) << 0 << int(QXmppPresence::Online) << "" << int(QXmppPresence::VCardUpdateNotReady) << QByteArray();
}

void tst_QXmppPresence::testPresence()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);
    QFETCH(int, priority);
    QFETCH(int, statusType);
    QFETCH(QString, statusText);
    QFETCH(int, vcardUpdate);
    QFETCH(QByteArray, photoHash);

    // test parsing and serialization after parsing
    QXmppPresence parsedPresence;
    parsePacket(parsedPresence, xml);
    QCOMPARE(int(parsedPresence.type()), type);
    QCOMPARE(parsedPresence.priority(), priority);
    QCOMPARE(int(parsedPresence.availableStatusType()), statusType);
    QCOMPARE(parsedPresence.statusText(), statusText);
    QCOMPARE(int(parsedPresence.vCardUpdateType()), vcardUpdate);
    QCOMPARE(parsedPresence.photoHash(), photoHash);

    serializePacket(parsedPresence, xml);

    // test serialization from setters
    QXmppPresence presence;
    presence.setType(static_cast<QXmppPresence::Type>(type));
    presence.setPriority(priority);
    presence.setAvailableStatusType(static_cast<QXmppPresence::AvailableStatusType>(statusType));
    presence.setStatusText(statusText);
    presence.setVCardUpdateType(static_cast<QXmppPresence::VCardUpdateType>(vcardUpdate));
    presence.setPhotoHash(photoHash);

    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithCapability()
{
    const QByteArray xml(
        "<presence to=\"foo@example.com/QXmpp\" from=\"bar@example.com/QXmpp\">"
        "<show>away</show>"
        "<status>In a meeting</status>"
        "<priority>5</priority>"
        "<c xmlns=\"http://jabber.org/protocol/caps\" hash=\"sha-1\" node=\"https://github.com/qxmpp-project/qxmpp\" ver=\"QgayPKawpkPSDYmwT/WM94uAlu0=\"/>"
        "<x xmlns=\"vcard-temp:x:update\">"
        "<photo>73b908bc</photo>"
        "</x>"
        "<x xmlns=\"urn:other:namespace\"/>"
        "</presence>");

    // test parsing and serialization after parsing
    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.to(), u"foo@example.com/QXmpp"_s);
    QCOMPARE(presence.from(), u"bar@example.com/QXmpp"_s);
    QCOMPARE(presence.availableStatusType(), QXmppPresence::Away);
    QCOMPARE(presence.statusText(), u"In a meeting"_s);
    QCOMPARE(presence.priority(), 5);
    QCOMPARE(presence.photoHash(), QByteArray::fromHex("73b908bc"));
    QCOMPARE(presence.vCardUpdateType(), QXmppPresence::VCardUpdateValidPhoto);
    QCOMPARE(presence.capabilityHash(), u"sha-1"_s);
    QCOMPARE(presence.capabilityNode(), u"https://github.com/qxmpp-project/qxmpp"_s);
    QCOMPARE(presence.capabilityVer(), QByteArray::fromBase64("QgayPKawpkPSDYmwT/WM94uAlu0="));
    const auto presenceExts = presence.extensions().getAll<QXmpp::Xml::Element>();
    QCOMPARE(presenceExts.size(), 1);
    QCOMPARE(presenceExts.at(0).tag(), u"x"_s);
    QCOMPARE(presenceExts.at(0).xmlns(), u"urn:other:namespace"_s);

    serializePacket(presence, xml);

    // test serialization from setters
    QXmppPresence presence2;
    presence2.setTo(u"foo@example.com/QXmpp"_s);
    presence2.setFrom(u"bar@example.com/QXmpp"_s);
    presence2.setAvailableStatusType(QXmppPresence::Away);
    presence2.setStatusText(u"In a meeting"_s);
    presence2.setPriority(5);
    presence2.setPhotoHash(QByteArray::fromHex("73b908bc"));
    presence2.setVCardUpdateType(QXmppPresence::VCardUpdateValidPhoto);
    presence2.setCapabilityHash(u"sha-1"_s);
    presence2.setCapabilityNode(u"https://github.com/qxmpp-project/qxmpp"_s);
    presence2.setCapabilityVer(QByteArray::fromBase64("QgayPKawpkPSDYmwT/WM94uAlu0="));

    QXmppElement unknownExtension;
    unknownExtension.setTagName(u"x"_s);
    unknownExtension.setAttribute(u"xmlns"_s, u"urn:other:namespace"_s);
    presence2.setExtensions(QXmppElementList() << unknownExtension);

    serializePacket(presence2, xml);
}

void tst_QXmppPresence::testPresenceWithExtendedAddresses()
{
    const QByteArray xml(
        "<presence to=\"multicast.jabber.org\" from=\"hildjj@jabber.com\" type=\"unavailable\">"
        "<addresses xmlns=\"http://jabber.org/protocol/address\">"
        "<address jid=\"temas@jabber.org\" type=\"bcc\"/>"
        "<address jid=\"jer@jabber.org\" type=\"bcc\"/>"
        "</addresses>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.extendedAddresses().size(), 2);
    QCOMPARE(presence.extendedAddresses()[0].description(), QString());
    QCOMPARE(presence.extendedAddresses()[0].jid(), QLatin1String("temas@jabber.org"));
    QCOMPARE(presence.extendedAddresses()[0].type(), QLatin1String("bcc"));
    QCOMPARE(presence.extendedAddresses()[1].description(), QString());
    QCOMPARE(presence.extendedAddresses()[1].jid(), QLatin1String("jer@jabber.org"));
    QCOMPARE(presence.extendedAddresses()[1].type(), QLatin1String("bcc"));
    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithMucItem()
{
    const QByteArray xml(
        "<presence to=\"pistol@shakespeare.lit/harfleur\" "
        "from=\"harfleur@henryv.shakespeare.lit/pistol\" "
        "type=\"unavailable\">"
        "<x xmlns=\"http://jabber.org/protocol/muc#user\">"
        "<item affiliation=\"none\" role=\"none\">"
        "<actor jid=\"fluellen@shakespeare.lit\"/>"
        "<reason>Avaunt, you cullion!</reason>"
        "</item>"
        "<status code=\"307\"/>"
        "</x>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.to(), QLatin1String("pistol@shakespeare.lit/harfleur"));
    QCOMPARE(presence.from(), QLatin1String("harfleur@henryv.shakespeare.lit/pistol"));
    QCOMPARE(presence.type(), QXmppPresence::Unavailable);
    QCOMPARE(presence.mucParticipantItem().actor(), QLatin1String("fluellen@shakespeare.lit"));
    QCOMPARE(presence.mucParticipantItem().affiliation(), QXmpp::Muc::Affiliation::None);
    QCOMPARE(presence.mucParticipantItem().jid(), QString());
    QCOMPARE(presence.mucParticipantItem().reason(), QLatin1String("Avaunt, you cullion!"));
    QCOMPARE(presence.mucParticipantItem().role(), QXmpp::Muc::Role::None);
    QCOMPARE(presence.mucStatusCodes(), QList<int>() << 307);
    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithMucPassword()
{
    const QByteArray xml(
        "<presence to=\"coven@chat.shakespeare.lit/thirdwitch\" "
        "from=\"hag66@shakespeare.lit/pda\">"
        "<x xmlns=\"http://jabber.org/protocol/muc\">"
        "<password>pass</password>"
        "</x>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.to(), QLatin1String("coven@chat.shakespeare.lit/thirdwitch"));
    QCOMPARE(presence.from(), QLatin1String("hag66@shakespeare.lit/pda"));
    QCOMPARE(presence.type(), QXmppPresence::Available);
    QCOMPARE(presence.isMucSupported(), true);
    QCOMPARE(presence.mucPassword(), QLatin1String("pass"));
    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithMucSupport()
{
    const QByteArray xml(
        "<presence to=\"coven@chat.shakespeare.lit/thirdwitch\" "
        "from=\"hag66@shakespeare.lit/pda\">"
        "<x xmlns=\"http://jabber.org/protocol/muc\"/>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.to(), QLatin1String("coven@chat.shakespeare.lit/thirdwitch"));
    QCOMPARE(presence.from(), QLatin1String("hag66@shakespeare.lit/pda"));
    QCOMPARE(presence.type(), QXmppPresence::Available);
    QCOMPARE(presence.isMucSupported(), true);
    QVERIFY(presence.mucPassword().isEmpty());
    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithMuji()
{
    const QByteArray xml(
        "<presence>"
        "<muji xmlns=\"urn:xmpp:jingle:muji:0\">"
        "<preparing/>"
        "<content creator=\"initiator\" name=\"video\"/>"
        "<content creator=\"initiator\" name=\"voice\"/>"
        "</muji>"
        "</presence>");

    QXmppPresence presence1;
    QVERIFY(!presence1.isPreparingMujiSession());
    QVERIFY(presence1.mujiContents().isEmpty());
    parsePacket(presence1, xml);

    QVERIFY(presence1.isPreparingMujiSession());
    QCOMPARE(presence1.mujiContents().size(), 2);
    QCOMPARE(presence1.mujiContents().at(0).name(), u"video"_s);
    QCOMPARE(presence1.mujiContents().at(1).name(), u"voice"_s);
    serializePacket(presence1, xml);

    QXmppPresence presence2;
    presence2.setIsPreparingMujiSession(true);
    QXmppJingleIq::Content mujiContent1;
    mujiContent1.setCreator(u"initiator"_s);
    mujiContent1.setName(u"video"_s);
    QXmppJingleIq::Content mujiContent2;
    mujiContent2.setCreator(u"initiator"_s);
    mujiContent2.setName(u"voice"_s);
    presence2.setMujiContents({ mujiContent1, mujiContent2 });

    QVERIFY(presence2.isPreparingMujiSession());
    QCOMPARE(presence2.mujiContents().size(), 2);
    QCOMPARE(presence2.mujiContents().at(0).name(), u"video"_s);
    QCOMPARE(presence2.mujiContents().at(1).name(), u"voice"_s);
    serializePacket(presence2, xml);
}

void tst_QXmppPresence::testPresenceWithLastUserInteraction()
{
    const QByteArray xml(
        "<presence to=\"coven@chat.shakespeare.lit/thirdwitch\" "
        "from=\"hag66@shakespeare.lit/pda\">"
        "<idle xmlns=\"urn:xmpp:idle:1\" since=\"1969-07-21T02:56:15Z\"/>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QVERIFY(!presence.lastUserInteraction().isNull());
    QVERIFY(presence.lastUserInteraction().isValid());
    QCOMPARE(presence.lastUserInteraction(), QDateTime(QDate(1969, 7, 21), QTime(2, 56, 15), TimeZoneUTC));
    serializePacket(presence, xml);

    QDateTime another(QDate(2025, 2, 5), QTime(15, 32, 8), TimeZoneUTC);
    presence.setLastUserInteraction(another);
    QCOMPARE(presence.lastUserInteraction(), another);
}

void tst_QXmppPresence::mucOccupantId()
{
    const QByteArray xml(
        "<presence to=\"hag99@shakespeare.example\" "
        "from=\"123435#coven@mix.shakespeare.example/UUID-a1j/7533\">"
        "<show>dnd</show>"
        "<occupant-id xmlns='urn:xmpp:occupant-id:0' id='dd72603deec90a38ba552f7c68cbcc61bca202cd'/>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);
    QCOMPARE(presence.mucOccupantId(), u"dd72603deec90a38ba552f7c68cbcc61bca202cd"_s);
    serializePacket(presence, xml);
}

void tst_QXmppPresence::testPresenceWithMix()
{
    const QByteArray xml(
        "<presence to=\"hag99@shakespeare.example\" "
        "from=\"123435#coven@mix.shakespeare.example/UUID-a1j/7533\">"
        "<show>dnd</show>"
        "<status>Making a Brew</status>"
        "<mix xmlns=\"urn:xmpp:presence:0\">"
        "<jid>hecate@shakespeare.example/UUID-x4r/2491</jid>"
        "<nick>thirdwitch</nick>"
        "</mix>"
        "</presence>");

    QXmppPresence presence;
    parsePacket(presence, xml);

    QCOMPARE(presence.mixUserJid(), u"hecate@shakespeare.example/UUID-x4r/2491"_s);
    QCOMPARE(presence.mixUserNick(), u"thirdwitch"_s);
    serializePacket(presence, xml);

    presence.setMixUserJid("alexander@example.org");
    QCOMPARE(presence.mixUserJid(), u"alexander@example.org"_s);
    presence.setMixUserNick("erik");
    QCOMPARE(presence.mixUserNick(), u"erik"_s);
}

void tst_QXmppPresence::testPresenceWithVCard()
{
}

// ===================== tst_QXmppBitsOfBinary =====================

class tst_QXmppBitsOfBinary : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testIq();
    Q_SLOT void testResult();
    Q_SLOT void testOtherSubelement();
    Q_SLOT void testIsBobIq();
    Q_SLOT void fromByteArray();

    Q_SLOT void testContentId();

    Q_SLOT void testFromContentId_data();
    Q_SLOT void testFromContentId();

    Q_SLOT void testFromCidUrl_data();
    Q_SLOT void testFromCidUrl();

    Q_SLOT void testEmpty();

    Q_SLOT void testIsValid_data();
    Q_SLOT void testIsValid();

    Q_SLOT void testIsBobContentId_data();
    Q_SLOT void testIsBobContentId();

    Q_SLOT void testUnsupportedAlgorithm();

    Q_SLOT void testDataListFind();
};

void tst_QXmppBitsOfBinary::testIq()
{
    const QByteArray xml(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.from(), u"doctor@shakespeare.lit/pda"_s);
    QCOMPARE(iq.id(), u"get-data-1"_s);
    QCOMPARE(iq.to(), u"ladymacbeth@shakespeare.lit/castle"_s);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.cid().toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(iq.contentType(), QMimeType());
    QCOMPARE(iq.data(), QByteArray());
    QCOMPARE(iq.maxAge(), -1);
    serializePacket(iq, xml);

    iq = QXmppBitsOfBinaryIq();
    iq.setFrom(u"doctor@shakespeare.lit/pda"_s);
    iq.setId(u"get-data-1"_s);
    iq.setTo(u"ladymacbeth@shakespeare.lit/castle"_s);
    iq.setType(QXmppIq::Get);
    iq.setCid(QXmppBitsOfBinaryContentId::fromContentId(u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s));
    serializePacket(iq, xml);
}

void tst_QXmppBitsOfBinary::testResult()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"data-result\" "
        "to=\"doctor@shakespeare.lit/pda\" "
        "from=\"ladymacbeth@shakespeare.lit/castle\" "
        "type=\"result\">"
        "<data xmlns=\"urn:xmpp:bob\" "
        "cid=\"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org\" "
        "max-age=\"86400\" "
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
        "</iq>");

    const auto data = QByteArray::fromBase64(QByteArrayLiteral(
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
        "kSuQmCC"));

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.id(), u"data-result"_s);
    QCOMPARE(iq.cid().algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(iq.cid().hash(), QByteArray::fromHex(QByteArrayLiteral("5a4c38d44fc64805cbb2d92d8b208be13ff40c0f")));
    QCOMPARE(iq.contentType(), QMimeDatabase().mimeTypeForName(u"image/png"_s));
    QCOMPARE(iq.maxAge(), 86400);
    QCOMPARE(iq.data(), data);
    serializePacket(iq, xml);

    iq = QXmppBitsOfBinaryIq();
    iq.setId(u"data-result"_s);
    iq.setFrom(u"ladymacbeth@shakespeare.lit/castle"_s);
    iq.setTo(u"doctor@shakespeare.lit/pda"_s);
    iq.setType(QXmppIq::Result);
    iq.setCid(QXmppBitsOfBinaryContentId::fromContentId(
        u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s));
    iq.setContentType(QMimeDatabase().mimeTypeForName(u"image/png"_s));
    iq.setMaxAge(86400);
    iq.setData(data);
    serializePacket(iq, xml);
}

void tst_QXmppBitsOfBinary::testOtherSubelement()
{
    const QByteArray xml(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"org.example.other.data\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.from(), u"doctor@shakespeare.lit/pda"_s);
    QCOMPARE(iq.id(), u"get-data-1"_s);
    QCOMPARE(iq.to(), u"ladymacbeth@shakespeare.lit/castle"_s);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.cid().toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(iq.contentType(), QMimeType());
    QCOMPARE(iq.data(), QByteArray());
    QCOMPARE(iq.maxAge(), -1);
}

void tst_QXmppBitsOfBinary::testIsBobIq()
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const QByteArray xmlSimple(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlSimple)), true);

    // IQs must have only one child element
    const QByteArray xmlMultipleElements(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:other-data-format:0\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlMultipleElements)), false);

    const QByteArray xmlWithoutBobData(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:other-data-format:0\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlWithoutBobData)), false);
    QT_WARNING_POP
}

void tst_QXmppBitsOfBinary::fromByteArray()
{
    auto data = QByteArray::fromBase64(
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
        "kSuQmCC");
    auto size = data.size();
    auto bobData = QXmppBitsOfBinaryData::fromByteArray(std::move(data));
    QCOMPARE(bobData.cid().toContentId(), u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s);
    QCOMPARE(bobData.data().size(), size);
}

void tst_QXmppBitsOfBinary::testContentId()
{
    // test fromCidUrl()
    QXmppBitsOfBinaryContentId cid = QXmppBitsOfBinaryContentId::fromCidUrl(QStringLiteral(
        "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"));

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);

    // test fromContentId()
    cid = QXmppBitsOfBinaryContentId::fromContentId(QStringLiteral(
        "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"));

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);

    // test setters
    cid = QXmppBitsOfBinaryContentId();
    cid.setHash(QByteArray::fromHex(QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242")));
    cid.setAlgorithm(QCryptographicHash::Sha1);

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
}

void tst_QXmppBitsOfBinary::testFromContentId_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << IS_VALID

    ROW("valid", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true);
    ROW("wrong-namespace", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob_222.xmpp.org", false);
    ROW("no-namespace", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@", false);
    ROW("url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("url-and-wrong-namespace", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob_222.xmpp.org", false);
    ROW("too-many-pluses", "sha1+sha256+sha3-256+blake2b256+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("wrong-hash-length", "cid:sha1+08d579a50083ff9308fb6242@bob.xmpp.org", false);

#undef ROW
}

void tst_QXmppBitsOfBinary::testFromContentId()
{
    QFETCH(QString, input);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::fromContentId(input).isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testFromCidUrl_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << IS_VALID

    ROW("valid", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true);
    ROW("no-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("wrong-namespace", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@other", false);
    ROW("too-many-pluses", "cid:sha1+sha256+sha3-256+blake2b256+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
#undef ROW
}

void tst_QXmppBitsOfBinary::testFromCidUrl()
{
    QFETCH(QString, input);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::fromCidUrl(input).isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testEmpty()
{
    QXmppBitsOfBinaryContentId cid;
    QVERIFY(cid.toCidUrl().isEmpty());
    QVERIFY(cid.toContentId().isEmpty());
}

void tst_QXmppBitsOfBinary::testIsValid_data()
{
    QTest::addColumn<QByteArray>("hash");
    QTest::addColumn<QCryptographicHash::Algorithm>("algorithm");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, HASH, ALGORITHM, IS_VALID) \
    QTest::newRow(NAME) << QByteArray::fromHex(HASH) << ALGORITHM << IS_VALID

    ROW("valid",
        "8f35fef110ffc5df08d579a50083ff9308fb6242",
        QCryptographicHash::Sha1,
        true);
    ROW("valid-sha256",
        "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b",
        QCryptographicHash::Sha256,
        true);
    ROW("wrong-hash-length", "8f35fef110ffc5df08", QCryptographicHash::Sha1, false);

#undef ROW
}

void tst_QXmppBitsOfBinary::testIsValid()
{
    QFETCH(QByteArray, hash);
    QFETCH(QCryptographicHash::Algorithm, algorithm);
    QFETCH(bool, isValid);

    QXmppBitsOfBinaryContentId contentId;
    contentId.setAlgorithm(algorithm);
    contentId.setHash(hash);

    QCOMPARE(contentId.isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testIsBobContentId_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("checkIsUrl");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, CHECK_IS_URL, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << CHECK_IS_URL << IS_VALID

    ROW("valid-url-check-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true, true);
    ROW("valid-url-no-check-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, true);
    ROW("valid-id-no-check-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, true);
    ROW("not-an-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true, false);

    ROW("invalid-namespace-id", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org.org.org", false, false);
    ROW("invalid-namespace-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org.org.org", true, false);

    ROW("no-hash-algorithm", "sha18f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, false);
#undef ROW
}

void tst_QXmppBitsOfBinary::testIsBobContentId()
{
    QFETCH(QString, input);
    QFETCH(bool, checkIsUrl);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::isBitsOfBinaryContentId(input, checkIsUrl), isValid);
}

void tst_QXmppBitsOfBinary::testUnsupportedAlgorithm()
{
    QCOMPARE(
        QXmppBitsOfBinaryContentId::fromContentId(
            u"blake2s160+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s),
        QXmppBitsOfBinaryContentId());
}

void tst_QXmppBitsOfBinary::testDataListFind()
{
    auto data1 = QXmppBitsOfBinaryData::fromByteArray(QByteArrayLiteral("hello"));
    auto data2 = QXmppBitsOfBinaryData::fromByteArray(QByteArrayLiteral("world"));

    QXmppBitsOfBinaryDataList list;
    list.append(data1);
    list.append(data2);

    // find existing element
    auto result = list.find(data1.cid());
    QVERIFY(result.has_value());
    QCOMPARE(*result, data1);

    // find second element
    result = list.find(data2.cid());
    QVERIFY(result.has_value());
    QCOMPARE(*result, data2);

    // find non-existing element
    auto data3 = QXmppBitsOfBinaryData::fromByteArray(QByteArrayLiteral("missing"));
    result = list.find(data3.cid());
    QVERIFY(!result.has_value());

    // find in empty list
    QXmppBitsOfBinaryDataList emptyList;
    result = emptyList.find(data1.cid());
    QVERIFY(!result.has_value());
}

// ===================== tst_QXmppStreamFeatures =====================

template<class T>
static void parsePacketWithStream(T &packet, const QByteArray &xml)
{
    QDomDocument doc;
    QByteArray wrappedXml =
        QByteArrayLiteral("<stream:stream xmlns:stream='http://etherx.jabber.org/streams'>") +
        xml + QByteArrayLiteral("</stream:stream>");

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto result = doc.setContent(wrappedXml, QDomDocument::ParseOption::UseNamespaceProcessing); !result) {
        qDebug() << result.errorMessage;
        QFAIL("Could not parse XML.");
    }
#else
    QString err;
    bool parsingSuccess = doc.setContent(wrappedXml, true, &err);
    if (!err.isNull()) {
        qDebug() << err;
    }
    QVERIFY(parsingSuccess);
#endif

    packet.parse(doc.documentElement().firstChildElement());
}

class tst_QXmppStreamFeatures : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testEmpty();
    Q_SLOT void testRequired();
    Q_SLOT void testFull();
    Q_SLOT void testSetters();
    Q_SLOT void testSasl2();
};

void tst_QXmppStreamFeatures::testEmpty()
{
    const QByteArray xml("<stream:features/>");

    QXmppStreamFeatures features;
    parsePacketWithStream(features, xml);
    QCOMPARE(features.bindMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.sessionMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.nonSaslAuthMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.tlsMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.clientStateIndicationMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.registerMode(), QXmppStreamFeatures::Disabled);
    QCOMPARE(features.preApprovedSubscriptionsSupported(), false);
    QCOMPARE(features.rosterVersioningSupported(), false);
    QCOMPARE(features.authMechanisms(), QStringList());
    QCOMPARE(features.compressionMethods(), QStringList());
    serializePacket(features, xml);
}

void tst_QXmppStreamFeatures::testRequired()
{
    const QByteArray xml(
        "<stream:features>"
        "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\">"
        "<required/>"
        "</starttls>"
        "</stream:features>");

    QXmppStreamFeatures features;
    parsePacketWithStream(features, xml);
    QCOMPARE(features.tlsMode(), QXmppStreamFeatures::Required);
    serializePacket(features, xml);
}

void tst_QXmppStreamFeatures::testFull()
{
    const QByteArray xml("<stream:features>"
                         "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>"
                         "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
                         "<auth xmlns=\"http://jabber.org/features/iq-auth\"/>"
                         "<starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\"/>"
                         "<csi xmlns=\"urn:xmpp:csi:0\"/>"
                         "<register xmlns=\"http://jabber.org/features/iq-register\"/>"
                         "<sub xmlns=\"urn:xmpp:features:pre-approval\"/>"
                         "<ver xmlns=\"urn:xmpp:features:rosterver\"/>"
                         "<compression xmlns=\"http://jabber.org/features/compress\"><method>zlib</method></compression>"
                         "<mechanisms xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"><mechanism>PLAIN</mechanism></mechanisms>"
                         "</stream:features>");

    QXmppStreamFeatures features;
    parsePacketWithStream(features, xml);
    QCOMPARE(features.bindMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.sessionMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.nonSaslAuthMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.tlsMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.clientStateIndicationMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.registerMode(), QXmppStreamFeatures::Enabled);
    QCOMPARE(features.preApprovedSubscriptionsSupported(), true);
    QCOMPARE(features.authMechanisms(), QStringList() << "PLAIN");
    QCOMPARE(features.compressionMethods(), QStringList() << "zlib");
    serializePacket(features, xml);

    features = QXmppStreamFeatures();
    features.setBindMode(QXmppStreamFeatures::Enabled);
    features.setSessionMode(QXmppStreamFeatures::Enabled);
    features.setNonSaslAuthMode(QXmppStreamFeatures::Enabled);
    features.setTlsMode(QXmppStreamFeatures::Enabled);
    features.setClientStateIndicationMode(QXmppStreamFeatures::Enabled);
    features.setRegisterMode(QXmppStreamFeatures::Enabled);
    features.setPreApprovedSubscriptionsSupported(true);
    features.setRosterVersioningSupported(true);
    features.setAuthMechanisms(QStringList { u"PLAIN"_s });
    features.setCompressionMethods(QStringList { u"zlib"_s });
    serializePacket(features, xml);
}

void tst_QXmppStreamFeatures::testSetters()
{
    QXmppStreamFeatures features;
    features.setBindMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.bindMode(), QXmppStreamFeatures::Enabled);
    features.setSessionMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.sessionMode(), QXmppStreamFeatures::Enabled);
    features.setNonSaslAuthMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.nonSaslAuthMode(), QXmppStreamFeatures::Enabled);
    features.setTlsMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.tlsMode(), QXmppStreamFeatures::Enabled);
    features.setClientStateIndicationMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.clientStateIndicationMode(), QXmppStreamFeatures::Enabled);
    features.setClientStateIndicationMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.clientStateIndicationMode(), QXmppStreamFeatures::Enabled);
    features.setRegisterMode(QXmppStreamFeatures::Enabled);
    QCOMPARE(features.registerMode(), QXmppStreamFeatures::Enabled);

    features.setAuthMechanisms(QStringList() << "custom-mechanism");
    QCOMPARE(features.authMechanisms(), QStringList() << "custom-mechanism");
    features.setCompressionMethods(QStringList() << "compression-methods");
    QCOMPARE(features.compressionMethods(), QStringList() << "compression-methods");
}

void tst_QXmppStreamFeatures::testSasl2()
{
    auto xml = "<stream:features>"
               "<authentication xmlns='urn:xmpp:sasl:2'>"
               "<mechanism>SCRAM-SHA-1</mechanism>"
               "<mechanism>SCRAM-SHA-1-PLUS</mechanism>"
               "</authentication>"
               "</stream:features>";

    QXmppStreamFeatures features;
    parsePacketWithStream(features, xml);
    QVERIFY(features.sasl2Feature().has_value());
}

// ===================== tst_QXmppDataForm =====================

class tst_QXmppDataForm : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testSimple();
    Q_SLOT void testSubmit();
    Q_SLOT void testMedia();
    Q_SLOT void testMediaSource();
    Q_SLOT void testFormType();
};

void tst_QXmppDataForm::testSimple()
{
    const QByteArray xml(
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<title>Joggle Search</title>"
        "<instructions>Fill out this form to search for information!</instructions>"
        "<field type=\"text-single\" var=\"search_request\">"
        "<required/>"
        "</field>"
        "</x>");

    QXmppDataForm form;
    parsePacket(form, xml);

    QCOMPARE(form.isNull(), false);
    QCOMPARE(form.title(), QLatin1String("Joggle Search"));
    QCOMPARE(form.instructions(), QLatin1String("Fill out this form to search for information!"));
    QVERIFY(form.formType().isNull());
    QCOMPARE(form.constFields().size(), 1);
    QCOMPARE(form.constFields().at(0).type(), QXmppDataForm::Field::TextSingleField);
    QCOMPARE(form.constFields().at(0).isRequired(), true);
    QCOMPARE(form.constFields().at(0).key(), u"search_request"_s);

    serializePacket(form, xml);
}

void tst_QXmppDataForm::testSubmit()
{
    const QByteArray xml(
        "<x xmlns=\"jabber:x:data\" type=\"submit\">"
        "<field type=\"text-single\" var=\"search_request\">"
        "<value>verona</value>"
        "</field>"
        "</x>");

    QXmppDataForm form;
    parsePacket(form, xml);
    QCOMPARE(form.isNull(), false);

    QVERIFY(form.field(u"search_request").has_value());
    QCOMPARE(form.field(u"search_request")->value().toString(), u"verona");

    QVERIFY(form.fieldValue(u"search_request").has_value());
    QCOMPARE(form.fieldValue(u"search_request")->toString(), u"verona");

    serializePacket(form, xml);
}

void tst_QXmppDataForm::testMedia()
{
    const QByteArray xml = QByteArrayLiteral(
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<field type=\"text-single\">"
        "<media xmlns=\"urn:xmpp:media-element\" width=\"290\" height=\"80\">"
        "<uri type=\"image/jpeg\">"
        "http://www.victim.com/challenges/ocr.jpeg?F3A6292C"
        "</uri>"
        "<uri type=\"image/png\">"
        "cid:sha1+f24030b8d91d233bac14777be5ab531ca3b9f102@bob.xmpp.org"
        "</uri>"
        "</media>"
        "</field>"
        "</x>");

    //
    // test parsing
    //

    QXmppDataForm form;
    parsePacket(form, xml);

    QCOMPARE(form.isNull(), false);
    QCOMPARE(form.constFields().size(), 1);
    QCOMPARE(form.constFields().at(0).type(), QXmppDataForm::Field::TextSingleField);
    QCOMPARE(form.constFields().at(0).isRequired(), false);
    QCOMPARE(form.constFields().at(0).mediaSize(), QSize(290, 80));
    QCOMPARE(form.constFields().at(0).mediaSources().size(), 2);
    QCOMPARE(
        form.constFields().at(0).mediaSources().at(0).uri().toString(),
        u"http://www.victim.com/challenges/ocr.jpeg?F3A6292C"_s);
    QCOMPARE(
        form.constFields().at(0).mediaSources().at(0).contentType(),
        QMimeDatabase().mimeTypeForName(u"image/jpeg"_s));
    QCOMPARE(
        form.constFields().at(0).mediaSources().at(1).uri().toString(),
        u"cid:sha1+f24030b8d91d233bac14777be5ab531ca3b9f102@bob.xmpp.org"_s);
    QCOMPARE(
        form.constFields().at(0).mediaSources().at(1).contentType(),
        QMimeDatabase().mimeTypeForName(u"image/png"_s));

    // deprecated
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(form.constFields().at(0).media().isNull(), false);
    QCOMPARE(form.constFields().at(0).media().width(), 290);
    QCOMPARE(form.constFields().at(0).media().height(), 80);
    QCOMPARE(form.constFields().at(0).media().uris().size(), 2);
    QCOMPARE(
        form.constFields().at(0).media().uris().at(0).first,
        u"image/jpeg"_s);
    QCOMPARE(
        form.constFields().at(0).media().uris().at(0).second,
        u"http://www.victim.com/challenges/ocr.jpeg?F3A6292C"_s);
    QCOMPARE(
        form.constFields().at(0).media().uris().at(1).first,
        u"image/png"_s);
    QCOMPARE(
        form.constFields().at(0).media().uris().at(1).second,
        u"cid:sha1+f24030b8d91d233bac14777be5ab531ca3b9f102@bob.xmpp.org"_s);
    QT_WARNING_POP

    serializePacket(form, xml);

    //
    // test non-const getters
    //

    QXmppDataForm::Field mediaField1;
    mediaField1.setMediaSize(QSize(290, 80));
    mediaField1.setMediaSources({
        QXmppDataForm::MediaSource(
            QUrl(u"http://www.victim.com/challenges/ocr.jpeg?F3A6292C"_s),
            QMimeDatabase().mimeTypeForName(u"image/jpeg"_s)),
        QXmppDataForm::MediaSource(
            QUrl(u"cid:sha1+f24030b8d91d233bac14777be5ab531ca3b9f102@bob.xmpp.org"_s),
            QMimeDatabase().mimeTypeForName(u"image/png"_s)),
    });

    QXmppDataForm form2;
    form2.setType(QXmppDataForm::Form);
    form2.setFields(QList<QXmppDataForm::Field>() << mediaField1);
    serializePacket(form2, xml);

    //
    // test setters
    //

    QXmppDataForm::Field mediaField2;
    mediaField2.setMediaSize(QSize(290, 80));
    QList<QXmppDataForm::MediaSource> sources;
    sources << QXmppDataForm::MediaSource(
        QUrl(u"http://www.victim.com/challenges/ocr.jpeg?F3A6292C"_s),
        QMimeDatabase().mimeTypeForName(u"image/jpeg"_s));
    sources << QXmppDataForm::MediaSource(
        QUrl(u"cid:sha1+f24030b8d91d233bac14777be5ab531ca3b9f102@bob.xmpp.org"_s),
        QMimeDatabase().mimeTypeForName(u"image/png"_s));
    mediaField2.setMediaSources(sources);

    QXmppDataForm form3;
    form3.setType(QXmppDataForm::Form);
    form3.setFields({ mediaField2 });
    serializePacket(form3, xml);

    //
    // test compatibility of deprecated methods
    //

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppDataForm::Field mediaFieldBefore = mediaField1;
    mediaField1.setMedia(mediaField1.media());
    QCOMPARE(mediaField1, mediaFieldBefore);

    QXmppDataForm::Field mediaField2Before = mediaField2;
    mediaField2.setMedia(mediaField2.media());
    QCOMPARE(mediaField2, mediaField2Before);
    QT_WARNING_POP
}

void tst_QXmppDataForm::testMediaSource()
{
    QXmppDataForm::MediaSource source;
    QCOMPARE(source.uri().toString(), QString());
    QCOMPARE(source.contentType(), QMimeType());

    source.setUri(QUrl("https://xmpp.org/index.html"));
    QCOMPARE(source.uri(), QUrl("https://xmpp.org/index.html"));
    source.setContentType(QMimeDatabase().mimeTypeForName("application/xml"));
    QCOMPARE(source.contentType(), QMimeDatabase().mimeTypeForName("application/xml"));
}

void tst_QXmppDataForm::testFormType()
{
    const auto xml = QByteArrayLiteral(R"(<x xmlns='jabber:x:data' type='submit'>
    <field var='FORM_TYPE' type='hidden'>
        <value>http://jabber.org/protocol/pubsub#subscribe_options</value>
    </field>
    <field var='pubsub#deliver'><value>1</value></field>
    <field var='pubsub#digest'><value>0</value></field>
    <field var='pubsub#include_body'><value>false</value></field>
    <field var='pubsub#show-values'>
        <value>chat</value>
        <value>online</value>
        <value>away</value>
    </field>
</x>)");

    QXmppDataForm form;
    parsePacket(form, xml);

    QCOMPARE(form.formType(), u"http://jabber.org/protocol/pubsub#subscribe_options"_s);
}

// ===================== tst_QXmppResultSet =====================

class tst_QXmppResultSet : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testQuery_data();
    Q_SLOT void testQuery();
    Q_SLOT void testReply_data();
    Q_SLOT void testReply();
};

void tst_QXmppResultSet::testQuery_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("max");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");

    QTest::newRow("Example 3") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<max>10</max>"
                                             "</set>")
                               << 10 << -1 << QString() << QString();

    QTest::newRow("Example 5") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<max>10</max>"
                                             "<after>peterpan@neverland.lit</after>"
                                             "</set>")
                               << 10 << -1 << QString() << u"peterpan@neverland.lit"_s;

    QTest::newRow("Example 5") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<max>10</max>"
                                             "<before>peter@pixyland.org</before>"
                                             "</set>")
                               << 10 << -1 << u"peter@pixyland.org"_s << QString();

    QTest::newRow("Example 11") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                              "<max>10</max>"
                                              "<before/>"
                                              "</set>")
                                << 10 << -1 << u""_s << QString();

    QTest::newRow("Example 12") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                              "<max>10</max>"
                                              "<index>371</index>"
                                              "</set>")
                                << 10 << 371 << QString() << QString();

    QTest::newRow("Example 15") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                              "<max>0</max>"
                                              "</set>")
                                << 0 << -1 << QString() << QString();
}

void tst_QXmppResultSet::testQuery()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, max);
    QFETCH(int, index);
    QFETCH(QString, before);
    QFETCH(QString, after);

    QXmppResultSetQuery iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.max(), max);
    QCOMPARE(iq.index(), index);
    QCOMPARE(iq.before(), before);
    QCOMPARE(iq.before().isNull(), before.isNull());
    QCOMPARE(iq.after(), after);
    QCOMPARE(iq.after().isNull(), after.isNull());
    serializePacket(iq, xml);
}

void tst_QXmppResultSet::testReply_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("count");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("first");
    QTest::addColumn<QString>("last");

    QTest::newRow("Example 4") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<first index=\"0\">stpeter@jabber.org</first>"
                                             "<last>peterpan@neverland.lit</last>"
                                             "<count>800</count>"
                                             "</set>")
                               << 800 << 0 << u"stpeter@jabber.org"_s << u"peterpan@neverland.lit"_s;

    QTest::newRow("Example 6") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<first index=\"0\">stpeter@jabber.org</first>"
                                             "<last>peterpan@neverland.lit</last>"
                                             "<count>800</count>"
                                             "</set>")
                               << 800 << 0 << u"stpeter@jabber.org"_s << u"peterpan@neverland.lit"_s;
    QTest::newRow("Example 4") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<first index=\"10\">peter@pixyland.org</first>"
                                             "<last>peter@rabbit.lit</last>"
                                             "<count>800</count>"
                                             "</set>")
                               << 800 << 10 << u"peter@pixyland.org"_s << u"peter@rabbit.lit"_s;

    QTest::newRow("Example 7") << QByteArray("<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                             "<count>790</count>"
                                             "</set>")
                               << 790 << -1 << QString() << QString();
}

void tst_QXmppResultSet::testReply()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, count);
    QFETCH(int, index);
    QFETCH(QString, first);
    QFETCH(QString, last);

    QXmppResultSetReply iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.count(), count);
    QCOMPARE(iq.index(), index);
    QCOMPARE(iq.first(), first);
    QCOMPARE(iq.first().isNull(), first.isNull());
    QCOMPARE(iq.last(), last);
    QCOMPARE(iq.last().isNull(), last.isNull());
    serializePacket(iq, xml);
}

// ===================== tst_QXmppSpamReport =====================

class tst_QXmppSpamReport : public QObject
{
    Q_OBJECT
private:
    Q_SLOT void serializeSpam();
    Q_SLOT void serializeTextNoLanguage();
    Q_SLOT void serializeFull();
    Q_SLOT void parseFull();
    Q_SLOT void parseInvalid();
};

void tst_QXmppSpamReport::serializeSpam()
{
    QXmppSpamReport report(QXmppSpamReport::Reason::Spam);
    serializePacket(report, "<report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:spam'/>");
}

void tst_QXmppSpamReport::serializeTextNoLanguage()
{
    QXmppSpamReport report(QXmppSpamReport::Reason::Spam);
    report.setText("please stop");
    serializePacket(report,
                    "<report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:spam'>"
                    "<text>please stop</text></report>");
}

void tst_QXmppSpamReport::serializeFull()
{
    QXmppSpamReport report(QXmppSpamReport::Reason::Abuse);
    report.setText("please stop");
    report.setTextLanguage("en");
    report.setMessageReferences({ QXmppStanzaId { "28482-98726", "romeo@example.net" } });
    report.setForwardToOrigin(true);
    report.setForwardToThirdParty(true);

    serializePacket(report,
                    "<report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:abuse'>"
                    "<stanza-id xmlns='urn:xmpp:sid:0' id='28482-98726' by='romeo@example.net'/>"
                    "<text xml:lang='en'>please stop</text>"
                    "<report-origin/><third-party/></report>");
}

void tst_QXmppSpamReport::parseFull()
{
    auto report = QXmppSpamReport::fromDom(xmlToDom(
        "<report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:abuse'>"
        "<stanza-id xmlns='urn:xmpp:sid:0' id='28482-98726' by='romeo@example.net'/>"
        "<text xml:lang='en'>please stop</text>"
        "<report-origin/><third-party/></report>"));

    QVERIFY(report.has_value());
    QCOMPARE(report->reason(), QXmppSpamReport::Reason::Abuse);
    QCOMPARE(report->text(), u"please stop"_s);
    QCOMPARE(report->textLanguage(), u"en"_s);
    QCOMPARE(report->messageReferences().size(), 1);
    QCOMPARE(report->messageReferences().at(0).id, u"28482-98726"_s);
    QCOMPARE(report->messageReferences().at(0).by, u"romeo@example.net"_s);
    QVERIFY(report->forwardToOrigin());
    QVERIFY(report->forwardToThirdParty());
}

void tst_QXmppSpamReport::parseInvalid()
{
    // wrong namespace
    QVERIFY(!QXmppSpamReport::fromDom(xmlToDom("<report xmlns='urn:example:other' reason='urn:xmpp:reporting:spam'/>")).has_value());
    // wrong element
    QVERIFY(!QXmppSpamReport::fromDom(xmlToDom("<spam xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:spam'/>")).has_value());
    // missing reason
    QVERIFY(!QXmppSpamReport::fromDom(xmlToDom("<report xmlns='urn:xmpp:reporting:1'/>")).has_value());
    // unknown reason
    QVERIFY(!QXmppSpamReport::fromDom(xmlToDom("<report xmlns='urn:xmpp:reporting:1' reason='urn:example:phishing'/>")).has_value());
}

// ===================== tst_QXmppSceEnvelope =====================

class tst_QXmppSceEnvelope : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testReader();
    Q_SLOT void testWriter();
};

void tst_QXmppSceEnvelope::testReader()
{
    const auto xml = QStringLiteral(
        "<envelope xmlns=\"urn:xmpp:sce:1\">"
        "<content><body xmlns=\"jabber:client\">Hello</body><x xmlns=\"jabber:x:oob\"><url>https://en.wikipedia.org/wiki/Fight_Club#Plot</url></x></content>"
        "<time stamp=\"2004-01-25T06:05:00+01:00\"/>"
        "<to jid=\"missioncontrol@houston.nasa.gov\"/>"
        "<from jid=\"opportunity@mars.planet\"/>"
        "<rpad>C1DHN9HK-9A25tSmwK4hU!Jji9%GKYK^syIlHJT9TnI4</rpad>"
        "</envelope>");

    QXmppSceEnvelopeReader reader(xmlToDom(xml));
    QCOMPARE(reader.from(), u"opportunity@mars.planet"_s);
    QCOMPARE(reader.to(), u"missioncontrol@houston.nasa.gov"_s);
    QCOMPARE(reader.timestamp(), QDateTime({ 2004, 01, 25 }, { 05, 05, 00 }, TimeZoneUTC));
    QCOMPARE(reader.contentElement().firstChildElement().tagName(), u"body"_s);
}

void tst_QXmppSceEnvelope::testWriter()
{
    const auto expectedXml = QStringLiteral(
        "<envelope xmlns=\"urn:xmpp:sce:1\">"
        "<content><body xmlns=\"jabber:client\">Hello</body><x xmlns=\"jabber:x:oob\"><url>https://en.wikipedia.org/wiki/Fight_Club#Plot</url></x></content>"
        "<time stamp=\"2004-01-25T05:05:00Z\"/>"
        "<to jid=\"missioncontrol@houston.nasa.gov\"/>"
        "<from jid=\"opportunity@mars.planet\"/>"
        "<rpad>C1DHN9HK-9A25tSmwK4hU!Jji9%GKYK^syIlHJT9TnI4</rpad>"
        "</envelope>");

    QString out;
    QXmlStreamWriter writer(&out);
    QXmppSceEnvelopeWriter envelope(writer);
    envelope.start();
    envelope.writeContent([&writer] {
        writer.writeStartElement("body");
        writer.writeDefaultNamespace("jabber:client");
        writer.writeCharacters("Hello");
        writer.writeEndElement();
        writer.writeStartElement("x");
        writer.writeDefaultNamespace("jabber:x:oob");
        writer.writeTextElement("url", "https://en.wikipedia.org/wiki/Fight_Club#Plot");
        writer.writeEndElement();
    });
    envelope.writeTimestamp(QDateTime({ 2004, 01, 25 }, { 05, 05, 00 }, TimeZoneUTC));
    envelope.writeTo("missioncontrol@houston.nasa.gov");
    envelope.writeFrom("opportunity@mars.planet");
    envelope.writeRpad("C1DHN9HK-9A25tSmwK4hU!Jji9%GKYK^syIlHJT9TnI4");
    envelope.end();

    QCOMPARE(out, expectedXml);
}

// ===================== tst_QXmppUri =====================

class tst_QXmppUri : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void base();
    Q_SLOT void queryMessage();
    Q_SLOT void queryRoster();
    Q_SLOT void queryRemove();
    Q_SLOT void queryOther();
};

void tst_QXmppUri::base()
{
    auto str = u"xmpp:lnj@qxmpp.org"_s;
    auto uri = unwrap(QXmppUri::fromString(str));
    QCOMPARE(uri.jid(), u"lnj@qxmpp.org");
    QVERIFY(!uri.query().has_value());
}

void tst_QXmppUri::queryMessage()
{
    const auto string = u"xmpp:romeo@montague.net?message;subject=Test%20Message;body=Here's%20a%20test%20message"_s;
    auto uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(uri.jid(), u"romeo@montague.net");
    auto message = unwrap<Uri::Message>(uri.query());
    QCOMPARE(message, (Uri::Message { u"Test Message"_s, u"Here's a test message"_s, {}, {}, {}, {} }));

    QCOMPARE(uri.toString(), string);
}

void tst_QXmppUri::queryRoster()
{
    const auto string = u"xmpp:romeo@montague.net?roster;name=Romeo%20Montague;group=Friends"_s;
    auto uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(uri.jid(), u"romeo@montague.net");
    auto message = unwrap<Uri::Roster>(uri.query());
    QCOMPARE(message, (Uri::Roster { u"Romeo Montague"_s, u"Friends"_s }));

    QCOMPARE(uri.toString(), string);
}

void tst_QXmppUri::queryRemove()
{
    const auto string = u"xmpp:romeo@montague.net?remove"_s;
    auto uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(uri.jid(), u"romeo@montague.net");
    auto message = unwrap<Uri::Remove>(uri.query());
    QCOMPARE(message, Uri::Remove {});

    QCOMPARE(uri.toString(), string);
}

void tst_QXmppUri::queryOther()
{
    auto string = u"xmpp:lnj@qxmpp.org?command;node=test2;action=next"_s;
    auto uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Command>(uri.query()), (Uri::Command { u"test2"_s, u"next"_s }));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:xsf@muc.xmpp.org?invite;jid=lnj@qxmpp.org;password=1234"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Invite>(uri.query()), (Uri::Invite { u"lnj@qxmpp.org"_s, u"1234"_s }));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:xsf@muc.xmpp.org?join;password=1234"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Join>(uri.query()), (Uri::Join { u"1234"_s }));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:alice@example.org?login;password=1234"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Login>(uri.query()), (Uri::Login { u"1234"_s }));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?register"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Register>(uri.query()), (Uri::Register {}));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?remove"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Remove>(uri.query()), (Uri::Remove {}));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?subscribe"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Subscribe>(uri.query()), (Uri::Subscribe {}));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?unregister"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Unregister>(uri.query()), (Uri::Unregister {}));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?unsubscribe"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::Unsubscribe>(uri.query()), (Uri::Unsubscribe {}));
    QCOMPARE(uri.toString(), string);

    string = u"xmpp:qxmpp.org?x-new-query;a=b;action=add"_s;
    uri = unwrap(QXmppUri::fromString(string));
    QCOMPARE(unwrap<Uri::CustomQuery>(uri.query()), (Uri::CustomQuery { u"x-new-query"_s, { { u"a"_s, u"b"_s }, { u"action"_s, u"add"_s } } }));
    QCOMPARE(uri.toString(), string);
}

// ============================================================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        tst_QXmppPresence tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppBitsOfBinary tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppStreamFeatures tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppDataForm tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppResultSet tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppSpamReport tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppSceEnvelope tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppUri tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "tst_QXmppMiscData.moc"

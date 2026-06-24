// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMessage.h"
#include "QXmppUtils.h"
#include "QXmppXmlRegistry.h"

#include "Omemo0Data.h"
#include "util.h"

#include <QObject>

using namespace QXmpp;
using namespace QXmpp::Private::Omemo0;
using namespace QXmpp::Xml;

// base64 test vectors reused across the cases
constexpr auto data1 = "Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK";
constexpr auto data2 = "a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK";
constexpr auto data3 = "PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K";

/*!
    Serializes data and compares it to multiple XML representations.
    It fails if no comparison succeeds.
*/
template<class T>
static void serializePacketMulti(T &packet, const QVector<QByteArray> &xmls)
{
    auto isSerializationSuccessful = false;
    const auto data = packetToXml(packet);

    for (const auto &xml : xmls) {
        auto processedXml = xml;
        processedXml.replace(u'\'', u'"');

        if (data == processedXml) {
            isSerializationSuccessful = true;
            break;
        }
    }
    if (!isSerializationSuccessful) {
        qDebug() << "writing" << data;
    }

    QVERIFY2(isSerializationSuccessful, "No XML data equals the serialized packet");
}

class tst_QXmppOmemo0Data : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testIsOmemo0Envelope_data();
    Q_SLOT void testIsOmemo0Envelope();
    Q_SLOT void testOmemo0Envelope_data();
    Q_SLOT void testOmemo0Envelope();
    Q_SLOT void testIsOmemo0Element_data();
    Q_SLOT void testIsOmemo0Element();
    Q_SLOT void testOmemo0Element();
    Q_SLOT void testOmemo0ElementWithoutPayload();
    Q_SLOT void testMessageOmemo0Element();
    Q_SLOT void testIsOmemo0DeviceElement_data();
    Q_SLOT void testIsOmemo0DeviceElement();
    Q_SLOT void testOmemo0DeviceElement();
    Q_SLOT void testIsOmemo0DeviceList_data();
    Q_SLOT void testIsOmemo0DeviceList();
    Q_SLOT void testOmemo0DeviceList();
    Q_SLOT void testIsOmemo0DeviceBundle_data();
    Q_SLOT void testIsOmemo0DeviceBundle();
    Q_SLOT void testOmemo0DeviceBundle();
};

static QDomElement domFromXml(const QByteArray &xml)
{
    return xmlToDom(xml);
}

void tst_QXmppOmemo0Data::testIsOmemo0Envelope_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<key xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << true;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << false;
    QTest::newRow("omemo2Namespace")
        << QByteArrayLiteral("<key xmlns=\"urn:xmpp:omemo:2\"/>")
        << false;
}

void tst_QXmppOmemo0Data::testIsOmemo0Envelope()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);
    QCOMPARE(isElement<Envelope>(domFromXml(xml)), isValid);
}

void tst_QXmppOmemo0Data::testOmemo0Envelope_data()
{
    // The <key/> element inherits its namespace from the <header/> parent and
    // is serialized without an explicit xmlns. The parse input therefore carries
    // the namespace while the serialized form omits it.
    QTest::addColumn<QByteArray>("parseXml");
    QTest::addColumn<QByteArray>("serializedXml");
    QTest::addColumn<uint32_t>("recipientDeviceId");
    QTest::addColumn<bool>("isUsedForKeyExchange");
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("key")
        << QByteArrayLiteral("<key xmlns=\"eu.siacs.conversations.axolotl\" rid=\"1337\">PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</key>")
        << QByteArrayLiteral("<key rid=\"1337\">PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</key>")
        << uint32_t(1337)
        << false
        << QByteArrayLiteral("PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K");
    QTest::newRow("preKey")
        << QByteArrayLiteral("<key xmlns=\"eu.siacs.conversations.axolotl\" rid=\"12321\" prekey=\"true\">a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK</key>")
        << QByteArrayLiteral("<key rid=\"12321\" prekey=\"true\">a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK</key>")
        << uint32_t(12321)
        << true
        << QByteArrayLiteral("a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK");
}

void tst_QXmppOmemo0Data::testOmemo0Envelope()
{
    QFETCH(QByteArray, parseXml);
    QFETCH(QByteArray, serializedXml);
    QFETCH(uint32_t, recipientDeviceId);
    QFETCH(bool, isUsedForKeyExchange);
    QFETCH(QByteArray, data);

    auto envelope = Envelope::fromDom(domFromXml(parseXml));
    QVERIFY(envelope);
    QCOMPARE(envelope->recipientDeviceId, recipientDeviceId);
    QCOMPARE(envelope->isUsedForKeyExchange, isUsedForKeyExchange);
    QCOMPARE(envelope->data.toBase64(), data);
    serializePacket(*envelope, serializedXml);

    Envelope envelope2 {
        .recipientDeviceId = recipientDeviceId,
        .isUsedForKeyExchange = isUsedForKeyExchange,
        .data = QByteArray::fromBase64(data),
    };
    serializePacket(envelope2, serializedXml);
}

void tst_QXmppOmemo0Data::testIsOmemo0Element_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<encrypted xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << true;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << false;
    QTest::newRow("omemo2Namespace")
        << QByteArrayLiteral("<encrypted xmlns=\"urn:xmpp:omemo:2\"/>")
        << false;
}

void tst_QXmppOmemo0Data::testIsOmemo0Element()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);
    QCOMPARE(isElement<Element>(domFromXml(xml)), isValid);
}

void tst_QXmppOmemo0Data::testOmemo0Element()
{
    const QByteArray xml(QByteArrayLiteral(
        "<encrypted xmlns=\"eu.siacs.conversations.axolotl\">"
        "<header sid=\"27183\">"
        "<key rid=\"31415\">Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</key>"
        "<key rid=\"12321\" prekey=\"true\">a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK</key>"
        "<iv>PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</iv>"
        "</header>"
        "<payload>Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</payload>"
        "</encrypted>"));

    auto element = Element::fromDom(domFromXml(xml));
    QVERIFY(element);
    QCOMPARE(element->senderDeviceId, uint32_t(27183));

    // flat key list, order preserved
    QCOMPARE(element->envelopes.size(), 2);
    QCOMPARE(element->envelopes.at(0).recipientDeviceId, uint32_t(31415));
    QVERIFY(!element->envelopes.at(0).isUsedForKeyExchange);
    QCOMPARE(element->envelopes.at(0).data.toBase64(), QByteArray(data1));
    QCOMPARE(element->envelopes.at(1).recipientDeviceId, uint32_t(12321));
    QVERIFY(element->envelopes.at(1).isUsedForKeyExchange);
    QCOMPARE(element->envelopes.at(1).data.toBase64(), QByteArray(data2));

    QCOMPARE(element->iv.toBase64(), QByteArray(data3));
    QVERIFY(element->payload);
    QCOMPARE(element->payload->toBase64(), QByteArray(data1));

    serializePacket(*element, xml);

    // build the same element from aggregate initializers
    Element element2 {
        .senderDeviceId = 27183,
        .iv = QByteArray::fromBase64(data3),
        .payload = QByteArray::fromBase64(data1),
        .envelopes = {
            Envelope { .recipientDeviceId = 31415, .data = QByteArray::fromBase64(data1) },
            Envelope { .recipientDeviceId = 12321, .isUsedForKeyExchange = true, .data = QByteArray::fromBase64(data2) },
        },
    };
    serializePacket(element2, xml);
}

void tst_QXmppOmemo0Data::testOmemo0ElementWithoutPayload()
{
    // KeyTransportElement: no <payload>
    const QByteArray xml(QByteArrayLiteral(
        "<encrypted xmlns=\"eu.siacs.conversations.axolotl\">"
        "<header sid=\"27183\">"
        "<key rid=\"31415\">Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</key>"
        "<iv>PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</iv>"
        "</header>"
        "</encrypted>"));

    auto element = Element::fromDom(domFromXml(xml));
    QVERIFY(element);
    QCOMPARE(element->envelopes.size(), 1);
    QVERIFY(!element->payload);
    serializePacket(*element, xml);
}

void tst_QXmppOmemo0Data::testMessageOmemo0Element()
{
    Registry::registerElement<Element>(Scope::Message);

    const QByteArray xml(QByteArrayLiteral(
        "<message to=\"juliet@capulet.lit\" from=\"romeo@montague.lit\" type=\"chat\">"
        "<encrypted xmlns=\"eu.siacs.conversations.axolotl\">"
        "<header sid=\"27183\">"
        "<key rid=\"31415\">Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</key>"
        "<iv>PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</iv>"
        "</header>"
        "<payload>Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</payload>"
        "</encrypted>"
        "</message>"));

    QXmppMessage message;
    message.parse(xmlToDom(xml));

    auto element = message.extensions().get<Element>();
    QVERIFY(element.has_value());
    QCOMPARE(element->senderDeviceId, uint32_t(27183));
    QCOMPARE(element->envelopes.size(), 1);
    QCOMPARE(element->envelopes.at(0).recipientDeviceId, uint32_t(31415));
    QVERIFY(element->payload);
    QCOMPARE(element->payload->toBase64(), QByteArray(data1));

    Registry::unregisterElement<Element>();
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceElement_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<device xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << true;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << false;
    QTest::newRow("omemo2Namespace")
        << QByteArrayLiteral("<device xmlns=\"urn:xmpp:omemo:2\"/>")
        << false;
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceElement()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);
    QCOMPARE(isElement<DeviceElement>(domFromXml(xml)), isValid);
}

void tst_QXmppOmemo0Data::testOmemo0DeviceElement()
{
    // <device/> inherits the namespace from <list/> and is serialized without
    // an explicit xmlns.
    const QByteArray parseXml(QByteArrayLiteral("<device xmlns=\"eu.siacs.conversations.axolotl\" id=\"12345\"/>"));
    const QByteArray serializedXml(QByteArrayLiteral("<device id=\"12345\"/>"));

    auto deviceElement = DeviceElement::fromDom(domFromXml(parseXml));
    QVERIFY(deviceElement);
    QCOMPARE(deviceElement->id, uint32_t(12345));
    serializePacket(*deviceElement, serializedXml);

    DeviceElement deviceElement2 { .id = 12345 };
    serializePacket(deviceElement2, serializedXml);
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceList_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<list xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << true;
    QTest::newRow("omemo2Tag")
        << QByteArrayLiteral("<devices xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << false;
    QTest::newRow("omemo2Namespace")
        << QByteArrayLiteral("<list xmlns=\"urn:xmpp:omemo:2\"/>")
        << false;
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceList()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);
    QCOMPARE(isElement<DeviceList>(domFromXml(xml)), isValid);
}

void tst_QXmppOmemo0Data::testOmemo0DeviceList()
{
    const QByteArray xml(QByteArrayLiteral(
        "<list xmlns=\"eu.siacs.conversations.axolotl\">"
        "<device id=\"12345\"/>"
        "<device id=\"4223\"/>"
        "</list>"));

    auto deviceList = DeviceList::fromDom(domFromXml(xml));
    QVERIFY(deviceList);
    QCOMPARE(deviceList->devices.size(), 2);
    QVERIFY(deviceList->devices.contains(DeviceElement { .id = 12345 }));
    QVERIFY(deviceList->devices.contains(DeviceElement { .id = 4223 }));
    serializePacket(*deviceList, xml);

    DeviceList deviceList2 {
        .devices = { DeviceElement { .id = 12345 }, DeviceElement { .id = 4223 } },
    };
    serializePacket(deviceList2, xml);
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceBundle_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<bundle xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << true;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid xmlns=\"eu.siacs.conversations.axolotl\"/>")
        << false;
    QTest::newRow("omemo2Namespace")
        << QByteArrayLiteral("<bundle xmlns=\"urn:xmpp:omemo:2\"/>")
        << false;
}

void tst_QXmppOmemo0Data::testIsOmemo0DeviceBundle()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);
    QCOMPARE(isElement<DeviceBundle>(domFromXml(xml)), isValid);
}

void tst_QXmppOmemo0Data::testOmemo0DeviceBundle()
{
    // The pre keys are stored in a hash, so the order is not fixed. Two
    // representations are accepted by serializePacketMulti().
    const QByteArray xml1(QByteArrayLiteral(
        "<bundle xmlns=\"eu.siacs.conversations.axolotl\">"
        "<signedPreKeyPublic signedPreKeyId=\"1\">Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</signedPreKeyPublic>"
        "<signedPreKeySignature>PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</signedPreKeySignature>"
        "<identityKey>a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK</identityKey>"
        "<prekeys>"
        "<preKeyPublic preKeyId=\"1\">eDM2cnBiTmo4MmRGQ1RYTkZ0YnVwajJtNWdPdzkxZ0gK</preKeyPublic>"
        "<preKeyPublic preKeyId=\"2\">aDRHdkcxNDNYUmJSNWVObnNWd0RCSzE1QlVKVGQ1RVEK</preKeyPublic>"
        "</prekeys>"
        "</bundle>"));
    const QByteArray xml2(QByteArrayLiteral(
        "<bundle xmlns=\"eu.siacs.conversations.axolotl\">"
        "<signedPreKeyPublic signedPreKeyId=\"1\">Oy5TSG9vVVV4Wz9wUkUvI1lUXiVLIU5bbGIsUV0wRngK</signedPreKeyPublic>"
        "<signedPreKeySignature>PTEoSk91VnRZSXBzcFlPXy4jZ3NKcGVZZ2d3YVJbVj8K</signedPreKeySignature>"
        "<identityKey>a012U0R9WixWKUYhYipucnZOWG06akFOR3Q1NGNOOmUK</identityKey>"
        "<prekeys>"
        "<preKeyPublic preKeyId=\"2\">aDRHdkcxNDNYUmJSNWVObnNWd0RCSzE1QlVKVGQ1RVEK</preKeyPublic>"
        "<preKeyPublic preKeyId=\"1\">eDM2cnBiTmo4MmRGQ1RYTkZ0YnVwajJtNWdPdzkxZ0gK</preKeyPublic>"
        "</prekeys>"
        "</bundle>"));

    QHash<uint32_t, QByteArray> expectedPreKeys = {
        { 1, QByteArray::fromBase64(QByteArrayLiteral("eDM2cnBiTmo4MmRGQ1RYTkZ0YnVwajJtNWdPdzkxZ0gK")) },
        { 2, QByteArray::fromBase64(QByteArrayLiteral("aDRHdkcxNDNYUmJSNWVObnNWd0RCSzE1QlVKVGQ1RVEK")) },
    };

    auto bundle = DeviceBundle::fromDom(domFromXml(xml1));
    QVERIFY(bundle);
    QCOMPARE(bundle->publicIdentityKey.toBase64(), QByteArray(data2));
    QCOMPARE(bundle->signedPublicPreKeyId, uint32_t(1));
    QCOMPARE(bundle->signedPublicPreKey.toBase64(), QByteArray(data1));
    QCOMPARE(bundle->signedPublicPreKeySignature.toBase64(), QByteArray(data3));
    QCOMPARE(bundle->publicPreKeys, expectedPreKeys);
    serializePacketMulti(*bundle, { xml1, xml2 });

    DeviceBundle bundle2 {
        .publicIdentityKey = QByteArray::fromBase64(data2),
        .signedPublicPreKey = QByteArray::fromBase64(data1),
        .signedPublicPreKeyId = 1,
        .signedPublicPreKeySignature = QByteArray::fromBase64(data3),
        .publicPreKeys = expectedPreKeys,
    };
    serializePacketMulti(bundle2, { xml1, xml2 });
}

QTEST_MAIN(tst_QXmppOmemo0Data)
#include "tst_QXmppOmemo0Data.moc"

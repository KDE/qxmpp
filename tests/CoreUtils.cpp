// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the core utilities. Merging tst_QXmppUtils,
// tst_QXmppLogger and the XML helper tests into one translation unit parses
// the shared Qt/QXmpp headers once instead of once per file. Each original
// test keeps its own namespace; main() runs them in turn.

#include "QXmppError.h"
#include "QXmppHash.h"
#include "QXmppHashing_p.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppPresence.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"
#include "QXmppXmlElement.h"
#include "QXmppXmlExtensions.h"
#include "QXmppXmlFormatter.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"
#include "XmlWriter.h"
#include "util.h"

#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QThread>

Q_DECLARE_METATYPE(QXmpp::HashAlgorithm);

namespace Utils {

using namespace QXmpp;
using namespace QXmpp::Private;

class tst_QXmppUtils : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testCrc32();
    Q_SLOT void testHmac();
    Q_SLOT void testJid();
    Q_SLOT void testMime();
    Q_SLOT void testTimezoneOffset();
    Q_SLOT void testStanzaHash();
    Q_SLOT void testCalculateHashes_data();
    Q_SLOT void testCalculateHashes();
    Q_SLOT void testParseHostAddress_data();
    Q_SLOT void testParseHostAddress();
};

void tst_QXmppUtils::testCrc32()
{
    quint32 crc = QXmppUtils::generateCrc32(QByteArray());
    QCOMPARE(crc, 0u);

    crc = QXmppUtils::generateCrc32(QByteArray("Hi There"));
    QCOMPARE(crc, 0xDB143BBEu);
}

void tst_QXmppUtils::testHmac()
{
    QByteArray hmac = QXmppUtils::generateHmacMd5(QByteArray(16, '\x0b'), QByteArray("Hi There"));
    QCOMPARE(hmac, QByteArray::fromHex("9294727a3638bb1c13f48ef8158bfc9d"));

    hmac = QXmppUtils::generateHmacMd5(QByteArray("Jefe"), QByteArray("what do ya want for nothing?"));
    QCOMPARE(hmac, QByteArray::fromHex("750c783e6ab0b503eaa86e310a5db738"));

    hmac = QXmppUtils::generateHmacMd5(QByteArray(16, '\xaa'), QByteArray(50, '\xdd'));
    QCOMPARE(hmac, QByteArray::fromHex("56be34521d144c88dbb8c733f0e8b3f6"));
}

void tst_QXmppUtils::testJid()
{
    QCOMPARE(QXmppUtils::jidToBareJid("foo@example.com/resource"), QLatin1String("foo@example.com"));
    QCOMPARE(QXmppUtils::jidToBareJid("foo@example.com"), QLatin1String("foo@example.com"));
    QCOMPARE(QXmppUtils::jidToBareJid("example.com"), QLatin1String("example.com"));
    QCOMPARE(QXmppUtils::jidToBareJid(QString()), QString());

    QCOMPARE(QXmppUtils::jidToDomain("foo@example.com/resource"), QLatin1String("example.com"));
    QCOMPARE(QXmppUtils::jidToDomain("foo@example.com"), QLatin1String("example.com"));
    QCOMPARE(QXmppUtils::jidToDomain("example.com"), QLatin1String("example.com"));
    QCOMPARE(QXmppUtils::jidToDomain(QString()), QString());

    QCOMPARE(QXmppUtils::jidToResource("foo@example.com/resource"), QLatin1String("resource"));
    QCOMPARE(QXmppUtils::jidToResource("foo@example.com"), QString());
    QCOMPARE(QXmppUtils::jidToResource("example.com"), QString());
    QCOMPARE(QXmppUtils::jidToResource(QString()), QString());

    QCOMPARE(QXmppUtils::jidToUser("foo@example.com/resource"), QLatin1String("foo"));
    QCOMPARE(QXmppUtils::jidToUser("foo@example.com"), QLatin1String("foo"));
    QCOMPARE(QXmppUtils::jidToUser("example.com"), QString());
    QCOMPARE(QXmppUtils::jidToUser(QString()), QString());
}

// FIXME: how should we test MIME detection without expose getImageType?
#if 0
QString getImageType(const QByteArray &contents);

static void testMimeType(const QString &fileName, const QString fileType)
{
    // load file from resources
    QFile file(":/" + fileName);
    QCOMPARE(file.open(QIODevice::ReadOnly), true);
    QCOMPARE(getImageType(file.readAll()), fileType);
    file.close();
}

void tst_QXmppUtils::testMime()
{
    testMimeType("test.bmp", "image/bmp");
    testMimeType("test.gif", "image/gif");
    testMimeType("test.jpg", "image/jpeg");
    testMimeType("test.mng", "video/x-mng");
    testMimeType("test.png", "image/png");
    testMimeType("test.svg", "image/svg+xml");
    testMimeType("test.xpm", "image/x-xpm");
}
#else
void tst_QXmppUtils::testMime()
{
}
#endif

void tst_QXmppUtils::testTimezoneOffset()
{
    // parsing
    QCOMPARE(QXmppUtils::timezoneOffsetFromString("Z"), 0);
    QCOMPARE(QXmppUtils::timezoneOffsetFromString("+00:00"), 0);
    QCOMPARE(QXmppUtils::timezoneOffsetFromString("-00:00"), 0);
    QCOMPARE(QXmppUtils::timezoneOffsetFromString("+01:30"), 5400);
    QCOMPARE(QXmppUtils::timezoneOffsetFromString("-01:30"), -5400);

    // serialization
    QCOMPARE(QXmppUtils::timezoneOffsetToString(0), QLatin1String("Z"));
    QCOMPARE(QXmppUtils::timezoneOffsetToString(5400), QLatin1String("+01:30"));
    QCOMPARE(QXmppUtils::timezoneOffsetToString(-5400), QLatin1String("-01:30"));
}

void tst_QXmppUtils::testStanzaHash()
{
    for (int i = 0; i < 100; i++) {
        const QString hash = QXmppUtils::generateStanzaHash(i);
        QCOMPARE(hash.size(), i);

        if (i == 36) {
            QCOMPARE(hash.count('-'), 4);
        }
    }

    const QString hash = QXmppUtils::generateStanzaUuid();
    QCOMPARE(hash.size(), 36);
    QCOMPARE(hash.count('-'), 4);
}

void tst_QXmppUtils::testCalculateHashes_data()
{
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<QByteArray>("hash");
    QTest::addColumn<QXmpp::HashAlgorithm>("algorithm");

    QTest::newRow("svg/md5")
        << u":/test.svg"_s
        << QByteArray::fromHex("cf7ab33aca717ed632c32296c8426043")
        << HashAlgorithm::Md5;
    QTest::newRow("svg/sha-1")
        << u":/test.svg"_s
        << QByteArray::fromHex("89d8cf114e4ec0758638ee8199af85d0974834bb")
        << HashAlgorithm::Sha1;
    QTest::newRow("svg/sha-224")
        << u":/test.svg"_s
        << QByteArray::fromHex("f7f29e8e228a0b7529f6a4bc97b0e6bd080a8a91e8386bc1304ececc")
        << HashAlgorithm::Sha224;
    QTest::newRow("svg/sha-256")
        << u":/test.svg"_s
        << QByteArray::fromHex("4736d79aa2912a2693cc17c5548612e1474dd1dfca2e8ddff917358482fd309f")
        << HashAlgorithm::Sha256;
    QTest::newRow("svg/sha-384")
        << u":/test.svg"_s
        << QByteArray::fromHex("2f2572eac288d92a6f8ba09ae6e91c12f4ebaedc00df8bbbd284c4d60a483cfb21bbae417ec0688d71aa5a940637f11c")
        << HashAlgorithm::Sha384;
    QTest::newRow("svg/sha-512")
        << u":/test.svg"_s
        << QByteArray::fromHex("85d34de6e549895d3c62773f589bb93b19c0bae62681f3fd0f3dba7262c96e87f771db4053ff7c9d0305b72222ccfe182596373917c0d109260973c258058196")
        << HashAlgorithm::Sha512;
    QTest::newRow("svg/sha3-256")
        << u":/test.svg"_s
        << QByteArray::fromHex("4079f2effb8968e1540ce7c684a01266175c1af8cb15342fa19b7f7926de9f14")
        << HashAlgorithm::Sha3_256;
    QTest::newRow("svg/sha3-512")
        << u":/test.svg"_s
        << QByteArray::fromHex("4c374d4c52fb57311761877a31a160703e5b67c0d3838758fa3698ae5bce10438145478116e3885cd9a8c30cf30391e7cd579d1c4c5b9c3ea8dba50930417931")
        << HashAlgorithm::Sha3_512;
    QTest::newRow("svg/blake2b-512")
        << u":/test.svg"_s
        << QByteArray::fromHex("a5e86044842e4c8306e9e2ee041fc26d57d172d5cb32346d5ee467c97c5a0b0b2350bc5a4a3dc76b92c48585c2ebbb01cf47fa59a88420fe7bba8f2a18af6f07")
        << HashAlgorithm::Blake2b_512;
    QTest::newRow("bmp/sha3-256")
        << u":/test.bmp"_s
        << QByteArray::fromHex("e50ffd13bb279932923ee10ba6847bec7546f77747074d1a7eeeb82228daf257")
        << HashAlgorithm::Sha3_256;
}

void tst_QXmppUtils::testCalculateHashes()
{
    using Algorithm = QXmpp::HashAlgorithm;
    QFETCH(QString, filePath);
    QFETCH(QByteArray, hash);
    QFETCH(QXmpp::HashAlgorithm, algorithm);

    auto file = std::make_unique<QFile>(filePath);
    QVERIFY(file->open(QFile::ReadOnly));
    auto resultPtr = wait(calculateHashes(std::move(file), { algorithm, Algorithm::Md5, Algorithm::Sha3_512 }));
    auto &[result, _] = *resultPtr;
    auto hashes = expectVariant<std::vector<QXmppHash>>(std::move(result));
    QCOMPARE(int(hashes.size()), int(3));
    QCOMPARE(hashes.front().hash(), hash);
}

void tst_QXmppUtils::testParseHostAddress_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("resultHost");
    QTest::addColumn<int>("resultPort");

    QTest::newRow("host-and-port")
        << u"qxmpp.org:443"_s
        << u"qxmpp.org"_s
        << 443;

    QTest::newRow("no-port")
        << u"qxmpp.org"_s
        << u"qxmpp.org"_s
        << -1;

    QTest::newRow("ipv4-with-port")
        << u"127.0.0.1:443"_s
        << u"127.0.0.1"_s
        << 443;

    QTest::newRow("ipv4-no-port")
        << u"127.0.0.1"_s
        << u"127.0.0.1"_s
        << -1;

    QTest::newRow("ipv6-with-port")
        << u"[2001:41D0:1:A49b::1]:9222"_s
        << u"2001:41d0:1:a49b::1"_s
        << 9222;

    QTest::newRow("ipv6-no-port")
        << u"[2001:41D0:1:A49b::1]"_s
        << u"2001:41d0:1:a49b::1"_s
        << -1;
}

void tst_QXmppUtils::testParseHostAddress()
{
    QFETCH(QString, input);
    QFETCH(QString, resultHost);
    QFETCH(int, resultPort);

    const auto address = parseHostAddress(input);
    QCOMPARE(address.first, resultHost);
    QCOMPARE(address.second, resultPort);
}

}  // namespace Utils

// ============================================================

namespace Logger {

class tst_QXmppLogger : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testFilterStreamManagementAcks();
    Q_SLOT void testStreamManagementAcksNotFilteredWhenDisabled();
    Q_SLOT void testNonAcksAlwaysLogged();
};

void tst_QXmppLogger::testFilterStreamManagementAcks()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    QVERIFY(logger.filterStreamManagementAcks());

    QSignalSpy spy(&logger, &QXmppLogger::message);

    // XEP-0198 ack request/answer, both directions: filtered out
    logger.log(QXmppLogger::SentMessage, u"<r xmlns=\"urn:xmpp:sm:3\"/>"_s);
    logger.log(QXmppLogger::ReceivedMessage, u"<a xmlns=\"urn:xmpp:sm:3\" h=\"3\"/>"_s);
    logger.log(QXmppLogger::ReceivedMessage, u"<r xmlns=\"urn:xmpp:sm:3\"/>"_s);
    logger.log(QXmppLogger::SentMessage, u"<a xmlns=\"urn:xmpp:sm:3\" h=\"0\"/>"_s);

    QCOMPARE(spy.size(), 0);
}

void tst_QXmppLogger::testStreamManagementAcksNotFilteredWhenDisabled()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    logger.setFilterStreamManagementAcks(false);
    QVERIFY(!logger.filterStreamManagementAcks());

    QSignalSpy spy(&logger, &QXmppLogger::message);

    logger.log(QXmppLogger::SentMessage, u"<r xmlns=\"urn:xmpp:sm:3\"/>"_s);
    logger.log(QXmppLogger::ReceivedMessage, u"<a xmlns=\"urn:xmpp:sm:3\" h=\"3\"/>"_s);

    QCOMPARE(spy.size(), 2);
}

void tst_QXmppLogger::testNonAcksAlwaysLogged()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    // default: filtering enabled
    QVERIFY(logger.filterStreamManagementAcks());

    QSignalSpy spy(&logger, &QXmppLogger::message);

    // normal stanzas
    logger.log(QXmppLogger::ReceivedMessage, u"<message from=\"a@b\"><body>hi</body></message>"_s);
    logger.log(QXmppLogger::SentMessage, u"<iq type=\"get\"/>"_s);
    // single leading letter but a longer element name (must not be mistaken for <r>/<a>)
    logger.log(QXmppLogger::SentMessage, u"<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>"_s);
    logger.log(QXmppLogger::ReceivedMessage, u"<response xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>"_s);
    // <r>/<a> element name but not the SM namespace
    logger.log(QXmppLogger::ReceivedMessage, u"<a xmlns=\"urn:example\"/>"_s);
    // non Sent/Received types are never affected
    logger.log(QXmppLogger::InformationMessage, u"<r xmlns=\"urn:xmpp:sm:3\"/>"_s);

    QCOMPARE(spy.size(), 6);
}

}  // namespace Logger

// ============================================================

namespace XmlHelpers {

// Combined test binary: merges tst_QXmppXmlElement, tst_QXmppXmlRegistry, tst_QXmppXmlFormatter into one translation
// unit so the shared Qt/QXmpp headers are parsed once instead of N times.
// main() runs each test class in turn via QTest::qExec().

using namespace QXmpp;
using namespace QXmpp::Xml;

// ===================== tst_QXmppXmlElement =====================

class tst_QXmppXmlElement : public QObject
{
    Q_OBJECT
private:
    Q_SLOT void testParseSimple();
    Q_SLOT void testRoundTripSimple();
    Q_SLOT void testRoundTripText();
    Q_SLOT void testRoundTripComplex();
    Q_SLOT void testAttributes();
    Q_SLOT void testChildren();
    Q_SLOT void testEquality();
};

static QString serialize(const Xml::Element &element)
{
    QString output;
    QXmlStreamWriter writer(&output);
    QXmpp::Private::XmlWriter xmlWriter(&writer);
    xmlWriter.write(element);
    return output;
}

void tst_QXmppXmlElement::testParseSimple()
{
    auto element = Xml::Element::fromDom(xmlToDom(QByteArrayLiteral("<foo xmlns=\"urn:example\"/>")));
    QCOMPARE(element.tag(), u"foo"_s);
    QCOMPARE(element.xmlns(), u"urn:example"_s);
    QVERIFY(element.text().isEmpty());
    QVERIFY(element.attributes().empty());
    QVERIFY(element.children().empty());
}

void tst_QXmppXmlElement::testRoundTripSimple()
{
    const auto xml = QByteArrayLiteral("<foo xmlns=\"urn:example\"/>");
    auto element = Xml::Element::fromDom(xmlToDom(xml));
    QCOMPARE(serialize(element).toUtf8(), xml);
}

void tst_QXmppXmlElement::testRoundTripText()
{
    const auto xml = QByteArrayLiteral("<foo xmlns=\"urn:example\" id=\"a1\">hello</foo>");
    auto element = Xml::Element::fromDom(xmlToDom(xml));
    QCOMPARE(element.text(), u"hello"_s);
    QVERIFY(element.attribute(u"id").has_value());
    QCOMPARE(*element.attribute(u"id"), u"a1"_s);
    QCOMPARE(serialize(element).toUtf8(), xml);
}

void tst_QXmppXmlElement::testRoundTripComplex()
{
    const auto xml = QByteArrayLiteral(
        "<message xmlns=\"jabber:client\" type=\"chat\" id=\"42\">"
        "<body>text</body>"
        "<x xmlns=\"urn:example:other\" a=\"1\" b=\"2\">"
        "<inner>deep</inner>"
        "</x>"
        "</message>");

    auto element = Xml::Element::fromDom(xmlToDom(xml));

    QCOMPARE(element.tag(), u"message"_s);
    QCOMPARE(element.xmlns(), u"jabber:client"_s);
    QCOMPARE(element.children().size(), size_t(2));
    QCOMPARE(element.children().at(0).tag(), u"body"_s);
    QCOMPARE(element.children().at(0).text(), u"text"_s);
    QCOMPARE(element.children().at(1).tag(), u"x"_s);
    QCOMPARE(element.children().at(1).xmlns(), u"urn:example:other"_s);
    QCOMPARE(element.children().at(1).children().at(0).text(), u"deep"_s);

    // Serialization re-declares inherited namespaces, so the output is not
    // byte-identical to the input. Verify semantic round-trip stability instead:
    // re-parsing the serialized output yields the same structure.
    auto reparsed = Xml::Element::fromDom(xmlToDom(serialize(element).toUtf8()));
    QCOMPARE(element, reparsed);
}

void tst_QXmppXmlElement::testAttributes()
{
    Xml::Element element(u"foo"_s);

    element.setAttribute(u"a"_s, u"1"_s);
    element.setAttribute(u"b"_s, u"2"_s);
    QCOMPARE(element.attributes().size(), size_t(2));

    // add-or-replace: same name keeps a single entry with the new value
    element.setAttribute(u"a"_s, u"3"_s);
    QCOMPARE(element.attributes().size(), size_t(2));
    QCOMPARE(*element.attribute(u"a"), u"3"_s);

    QVERIFY(!element.attribute(u"missing").has_value());

    element.removeAttribute(u"a");
    QCOMPARE(element.attributes().size(), size_t(1));
    QVERIFY(!element.attribute(u"a").has_value());
    QCOMPARE(*element.attribute(u"b"), u"2"_s);
}

void tst_QXmppXmlElement::testChildren()
{
    Xml::Element element(u"root"_s);
    element.addChild(Xml::Element(u"a"_s));
    element.addChild(Xml::Element(u"b"_s));
    QCOMPARE(element.children().size(), size_t(2));
    QCOMPARE(element.children().at(0).tag(), u"a"_s);

    element.setChildren({ Xml::Element(u"c"_s) });
    QCOMPARE(element.children().size(), size_t(1));
    QCOMPARE(element.children().at(0).tag(), u"c"_s);
}

void tst_QXmppXmlElement::testEquality()
{
    Xml::Element a(u"foo"_s, u"urn:x"_s);
    a.setAttribute(u"k"_s, u"v"_s);
    a.addChild(Xml::Element(u"child"_s));

    auto b = a;
    QCOMPARE(a, b);

    b.setText(u"different"_s);
    QVERIFY(!(a == b));
}

// ===================== tst_QXmppXmlRegistry =====================

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

// V3 parse style (fromDom → optional<T>).
struct FooExtension {
    static constexpr std::tuple XmlTag = { u"foo", u"urn:test:foo" };

    QString value;

    static std::optional<FooExtension> fromDom(const QDomElement &el)
    {
        if (el.tagName() != u"foo" || el.namespaceURI() != u"urn:test:foo") {
            return {};
        }
        return FooExtension { el.attribute(u"value"_s) };
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeStartElement(u"foo"_s);
        w->writeDefaultNamespace(u"urn:test:foo"_s);
        w->writeAttribute(u"value"_s, value);
        w->writeEndElement();
    }
};

// V2 parse style (parse → bool).
struct BarExtension {
    static constexpr std::tuple XmlTag = { u"bar", u"urn:test:bar" };

    int count = 0;

    bool parse(const QDomElement &el)
    {
        if (el.tagName() != u"bar" || el.namespaceURI() != u"urn:test:bar") {
            return false;
        }
        count = el.attribute(u"n"_s).toInt();
        return true;
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeStartElement(u"bar"_s);
        w->writeDefaultNamespace(u"urn:test:bar"_s);
        w->writeAttribute(u"n"_s, QString::number(count));
        w->writeEndElement();
    }
};

// Used for Generic scope tests.
struct GenericExtension {
    static constexpr std::tuple XmlTag = { u"debug", u"urn:test:debug" };

    static std::optional<GenericExtension> fromDom(const QDomElement &el)
    {
        if (el.tagName() != u"debug" || el.namespaceURI() != u"urn:test:debug") {
            return {};
        }
        return GenericExtension {};
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeEmptyElement(u"debug"_s);
        w->writeDefaultNamespace(u"urn:test:debug"_s);
    }
};

// ---------------------------------------------------------------------------

static QString serializeExtensions(const Extensions &ext)
{
    QString output;
    QXmlStreamWriter writer(&output);
    ext.toXml(&writer);
    return output;
}

class tst_QXmppXmlRegistry : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testRegisterAndParseMessage();
    Q_SLOT void testRegisterAndParsePresence();
    Q_SLOT void testParseV2Style();
    Q_SLOT void testGenericScope();
    Q_SLOT void testFallbackElement();
    Q_SLOT void testUnregister();
    Q_SLOT void testContainerApi();
    Q_SLOT void testRoundTripMessage();
};

void tst_QXmppXmlRegistry::testRegisterAndParseMessage()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message to='alice@example.org'>"
        "<foo xmlns='urn:test:foo' value='hello'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    QVERIFY(!msg.extensions().isEmpty());
    auto foo = msg.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"hello"_s);

    Registry::unregisterElement<FooExtension>();
}

void tst_QXmppXmlRegistry::testRegisterAndParsePresence()
{
    Registry::registerElement<FooExtension>(Scope::Presence);

    const auto xml = QByteArrayLiteral(
        "<presence>"
        "<foo xmlns='urn:test:foo' value='world'/>"
        "</presence>");

    QXmppPresence pres;
    pres.parse(xmlToDom(xml));

    QVERIFY(!pres.extensions().isEmpty());
    auto foo = pres.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"world"_s);

    Registry::unregisterElement<FooExtension>();
}

void tst_QXmppXmlRegistry::testParseV2Style()
{
    Registry::registerElement<BarExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message>"
        "<bar xmlns='urn:test:bar' n='7'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    auto bar = msg.extensions().get<BarExtension>();
    QVERIFY(bar.has_value());
    QCOMPARE(bar->count, 7);

    Registry::unregisterElement<BarExtension>();
}

void tst_QXmppXmlRegistry::testGenericScope()
{
    Registry::registerElement<GenericExtension>(Scope::Generic);

    const auto msgXml = QByteArrayLiteral(
        "<message>"
        "<debug xmlns='urn:test:debug'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(msgXml));
    QVERIFY(msg.extensions().contains<GenericExtension>());

    const auto presXml = QByteArrayLiteral(
        "<presence>"
        "<debug xmlns='urn:test:debug'/>"
        "</presence>");

    QXmppPresence pres;
    pres.parse(xmlToDom(presXml));
    QVERIFY(pres.extensions().contains<GenericExtension>());

    Registry::unregisterElement<GenericExtension>();
}

void tst_QXmppXmlRegistry::testFallbackElement()
{
    // No registration → unknown element must land as Xml::Element fallback.
    const auto xml = QByteArrayLiteral(
        "<message>"
        "<unknown xmlns='urn:test:unknown' key='v'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    QVERIFY(!msg.extensions().isEmpty());
    auto elements = msg.extensions().getAll<Element>();
    QCOMPARE(elements.size(), 1);
    QCOMPARE(elements.at(0).tag(), u"unknown"_s);
    QCOMPARE(elements.at(0).xmlns(), u"urn:test:unknown"_s);
    QCOMPARE(*elements.at(0).attribute(u"key"), u"v"_s);

    // Serialization round-trip: the fallback element is written back out.
    QString serialized = serializeExtensions(msg.extensions());
    QVERIFY(serialized.contains(u"unknown"_s));
    QVERIFY(serialized.contains(u"urn:test:unknown"_s));
}

void tst_QXmppXmlRegistry::testUnregister()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message>"
        "<foo xmlns='urn:test:foo' value='x'/>"
        "</message>");

    // With registration: typed.
    {
        QXmppMessage msg;
        msg.parse(xmlToDom(xml));
        QVERIFY(msg.extensions().contains<FooExtension>());
    }

    Registry::unregisterElement<FooExtension>();

    // After unregister: stored as Xml::Element fallback.
    {
        QXmppMessage msg;
        msg.parse(xmlToDom(xml));
        QVERIFY(!msg.extensions().contains<FooExtension>());
        QVERIFY(msg.extensions().contains<Element>());
    }
}

void tst_QXmppXmlRegistry::testContainerApi()
{
    Extensions ext;
    QVERIFY(ext.isEmpty());
    QCOMPARE(ext.size(), 0);

    ext.add(FooExtension { u"a"_s });
    ext.add(FooExtension { u"b"_s });
    ext.add(BarExtension { 3 });

    QCOMPARE(ext.size(), 3);
    QVERIFY(ext.contains<FooExtension>());
    QVERIFY(ext.contains<BarExtension>());

    auto foos = ext.getAll<FooExtension>();
    QCOMPARE(foos.size(), 2);
    QCOMPARE(foos.at(0).value, u"a"_s);
    QCOMPARE(foos.at(1).value, u"b"_s);

    QVERIFY(ext.remove<FooExtension>());
    QCOMPARE(ext.size(), 2);

    QCOMPARE(ext.removeAll<FooExtension>(), 1);
    QCOMPARE(ext.size(), 1);

    ext.clear();
    QVERIFY(ext.isEmpty());
}

void tst_QXmppXmlRegistry::testRoundTripMessage()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    // Build a message with a registered extension.
    QXmppMessage out;
    out.setTo(u"bob@example.org"_s);
    out.extensions().add(FooExtension { u"roundtrip"_s });

    QByteArray serialized;
    QXmlStreamWriter sw(&serialized);
    out.toXml(&sw);

    // Parse back and verify.
    QXmppMessage in;
    in.parse(xmlToDom(serialized));

    auto foo = in.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"roundtrip"_s);

    Registry::unregisterElement<FooExtension>();
}

// ===================== tst_QXmppXmlFormatter =====================

class tst_QXmppXmlFormatter : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void roundTripIq();
    Q_SLOT void selfClosingEmptyElement();
    Q_SLOT void streamFragmentWithPrefix();
    Q_SLOT void textContentNotIndented();
    Q_SLOT void colorOffNoEscapes();
    Q_SLOT void colorOnHasEscapes();
    Q_SLOT void malformedInputPassThrough();
    Q_SLOT void emptyInput();
    Q_SLOT void escapesPreserved();
    Q_SLOT void noIndent();
    Q_SLOT void loggerDefaultUnchanged();
    Q_SLOT void loggerPrettyXmlAppliesToSentReceivedOnly();
    Q_SLOT void streamOpenFragment();
    Q_SLOT void streamCloseFragment();
    Q_SLOT void streamOpenWithXmlDecl();
};

void tst_QXmppXmlFormatter::roundTripIq()
{
    auto in = u"<iq from='a@b' to='c@d' id='1' type='get'><query xmlns='jabber:iq:roster'/></iq>"_s;
    auto out = QXmpp::formatXmlForDebug(in);
    QCOMPARE(out,
             u"<iq from=\"a@b\" to=\"c@d\" id=\"1\" type=\"get\">\n"
             u"  <query xmlns=\"jabber:iq:roster\"/>\n"
             u"</iq>"_s);
}

void tst_QXmppXmlFormatter::selfClosingEmptyElement()
{
    auto out = QXmpp::formatXmlForDebug(u"<ping/>"_s);
    QCOMPARE(out, u"<ping/>"_s);
}

void tst_QXmppXmlFormatter::streamFragmentWithPrefix()
{
    auto out = QXmpp::formatXmlForDebug(u"<stream:features><bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/></stream:features>"_s);
    QCOMPARE(out,
             u"<stream:features>\n"
             u"  <bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>\n"
             u"</stream:features>"_s);
}

void tst_QXmppXmlFormatter::textContentNotIndented()
{
    auto out = QXmpp::formatXmlForDebug(u"<message><body>hello world</body></message>"_s);
    QCOMPARE(out,
             u"<message>\n"
             u"  <body>hello world</body>\n"
             u"</message>"_s);
}

void tst_QXmppXmlFormatter::colorOffNoEscapes()
{
    auto out = QXmpp::formatXmlForDebug(u"<iq type='get'/>"_s, true, 2, false);
    QVERIFY(!out.contains(QChar(0x1b)));
}

void tst_QXmppXmlFormatter::colorOnHasEscapes()
{
    auto out = QXmpp::formatXmlForDebug(u"<iq type='get'/>"_s, true, 2, true);
    QVERIFY(out.contains(QChar(0x1b)));
    // After stripping ANSI escapes, content should match no-color output.
    static const QRegularExpression ansiRe(u"\x1b\\[[0-9;]*m"_s);
    auto stripped = out;
    stripped.remove(ansiRe);
    QCOMPARE(stripped, QXmpp::formatXmlForDebug(u"<iq type='get'/>"_s, true, 2, false));
}

void tst_QXmppXmlFormatter::malformedInputPassThrough()
{
    auto in = u"STUN packet to 1.2.3.4 port 3478\n<some non-xml stuff>"_s;
    auto out = QXmpp::formatXmlForDebug(in);
    QCOMPARE(out, in);
}

void tst_QXmppXmlFormatter::emptyInput()
{
    QCOMPARE(QXmpp::formatXmlForDebug({}), QString());
}

void tst_QXmppXmlFormatter::escapesPreserved()
{
    auto out = QXmpp::formatXmlForDebug(u"<body>5 &lt; 6 &amp;&amp; 7 &gt; 6</body>"_s);
    QCOMPARE(out, u"<body>5 &lt; 6 &amp;&amp; 7 &gt; 6</body>"_s);
}

void tst_QXmppXmlFormatter::noIndent()
{
    auto out = QXmpp::formatXmlForDebug(u"<a><b/></a>"_s, false);
    // No newlines added, no indentation, but unchanged structure.
    QVERIFY(!out.contains(u'\n'));
    QVERIFY(out.contains(u"<a>"));
    QVERIFY(out.contains(u"<b/>"));
    QVERIFY(out.contains(u"</a>"));
}

void tst_QXmppXmlFormatter::loggerDefaultUnchanged()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    QSignalSpy spy(&logger, &QXmppLogger::message);
    auto in = u"<iq type='get'><foo/></iq>"_s;
    logger.log(QXmppLogger::SentMessage, in);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toString(), in);
}

void tst_QXmppXmlFormatter::loggerPrettyXmlAppliesToSentReceivedOnly()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    logger.setPrettyXml(true);
    logger.setColorMode(QXmppLogger::ColorOff);

    QSignalSpy spy(&logger, &QXmppLogger::message);
    logger.log(QXmppLogger::SentMessage, u"<iq><foo/></iq>"_s);
    logger.log(QXmppLogger::InformationMessage, u"<iq><foo/></iq>"_s);

    QCOMPARE(spy.count(), 2);
    // Sent: pretty-printed.
    QCOMPARE(spy.at(0).at(1).toString(),
             u"<iq>\n  <foo/>\n</iq>"_s);
    // Info: untouched
    QCOMPARE(spy.at(1).at(1).toString(), u"<iq><foo/></iq>"_s);
}

void tst_QXmppXmlFormatter::streamOpenFragment()
{
    auto in = u"<stream:stream from='x@y' to='y' version='1.0' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>"_s;
    auto out = QXmpp::formatXmlForDebug(in);
    QCOMPARE(out, u"<stream:stream xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" from=\"x@y\" to=\"y\" version=\"1.0\">"_s);
}

void tst_QXmppXmlFormatter::streamCloseFragment()
{
    auto out = QXmpp::formatXmlForDebug(u"</stream:stream>"_s);
    QCOMPARE(out, u"</stream:stream>"_s);

    auto colored = QXmpp::formatXmlForDebug(u"</stream:stream>"_s, true, 2, true);
    QVERIFY(colored.contains(QChar(0x1b)));
    static const QRegularExpression ansiRe(u"\x1b\\[[0-9;]*m"_s);
    auto stripped = colored;
    stripped.remove(ansiRe);
    QCOMPARE(stripped, u"</stream:stream>"_s);
}

void tst_QXmppXmlFormatter::streamOpenWithXmlDecl()
{
    auto in = u"<?xml version='1.0'?><stream:stream from='x@y' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>"_s;
    auto out = QXmpp::formatXmlForDebug(in);
    QCOMPARE(out,
             u"<?xml version='1.0'?>\n"
             u"<stream:stream xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" from=\"x@y\">"_s);
}

// ============================================================

}  // namespace XmlHelpers

QXMPP_TEST_MAIN(Utils::tst_QXmppUtils, Logger::tst_QXmppLogger, XmlHelpers::tst_QXmppXmlElement, XmlHelpers::tst_QXmppXmlRegistry, XmlHelpers::tst_QXmppXmlFormatter)

#include "CoreUtils.moc"

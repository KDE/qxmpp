// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@jahnson.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the core utilities. Merging tst_QXmppUtils,
// tst_QXmppLogger and the XML helper tests into one translation unit parses
// the shared Qt/QXmpp headers once instead of once per file. Each original
// test keeps its own namespace; main() runs them in turn.

#include "QXmppError.h"
#include "QXmppHash.h"
#include "QXmppHashing_p.h"
#include "QXmppJid.h"
#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppNamespaces.h"
#include "QXmppPresence.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"
#include "QXmppXmlElement.h"
#include "QXmppXmlExtensions.h"
#include "QXmppXmlFormatter_p.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"
#include "XmlWriter.h"
#include "util.h"

#include <QCoreApplication>
#include <QFile>
#include <QObject>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>

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
    Q_SLOT void testNamespaces();
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

void tst_QXmppUtils::testNamespaces()
{
    using Ns = QXmpp::Namespace;

    QCOMPARE(namespaceUri(Ns::Xml), u"http://www.w3.org/XML/1998/namespace"_s);
    QCOMPARE(namespaceUri(Ns::Mam2), u"urn:xmpp:mam:2"_s);
    QCOMPARE(namespaceUri(Ns::MixPam2Archive), u"urn:xmpp:mix:pam:2#archive"_s);
    QCOMPARE(namespaceUri(Ns::Fast0), u"urn:xmpp:fast:0"_s);

    QCOMPARE(namespaceFromUri(u"urn:xmpp:carbons:2"), Ns::Carbons2);
    QCOMPARE(namespaceFromUri(u"urn:xmpp:carbons:1"), std::nullopt);
    QCOMPARE(namespaceFromUri(u"org.qxmpp.credentials"), std::nullopt);
    QCOMPARE(namespaceFromUri(u""), std::nullopt);

    // every namespace has a unique URI
    for (int i = 0; i <= int(Ns::Mam2GroupchatAvailable); i++) {
        const auto ns = Ns(i);
        QVERIFY(!namespaceUri(ns).isEmpty());
        QCOMPARE(namespaceFromUri(namespaceUri(ns)), ns);
    }
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
    Q_SLOT void testRawOutputByDefault();
    Q_SLOT void testFileLoggingIsRaw();
    Q_SLOT void testElideLogMessagesAbove();
    Q_SLOT void testElideXmlTextAboveNeedsPrettyOutput();
    Q_SLOT void testEnableEliding();
    Q_SLOT void testPrettyOutputElides();
    Q_SLOT void testPrettyXmlMapsToOutputMode();
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

static QString hugeStanza(qsizetype dataSize = 100000)
{
    return u"<iq type=\"result\"><data>"_s + QString(dataSize, u'A') + u"</data></iq>"_s;
}

void tst_QXmppLogger::testRawOutputByDefault()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    QCOMPARE(logger.outputMode(), QXmppLogger::OutputMode::Auto);
    // the limits are set, but they only apply to pretty output
    QCOMPARE(logger.elideXmlTextAbove().value(), QXmppLogger::DefaultElideXmlTextAbove);
    QCOMPARE(logger.elideLogMessagesAbove().value(), QXmppLogger::DefaultElideLogMessagesAbove);

    QSignalSpy spy(&logger, &QXmppLogger::message);

    // XML is passed through 1:1, so it can be reparsed
    auto stanza = hugeStanza();
    logger.log(QXmppLogger::SentMessage, stanza);

    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(1).toString(), stanza);
}

void tst_QXmppLogger::testFileLoggingIsRaw()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath(u"log.txt"_s);

    QXmppLogger logger;
    logger.setLogFilePath(path);
    logger.setLoggingType(QXmppLogger::FileLogging);

    auto stanza = hugeStanza();
    logger.log(QXmppLogger::SentMessage, stanza);
    logger.reopen();

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(stanza));
    QVERIFY(!contents.contains(u"elided"));
}

void tst_QXmppLogger::testElideLogMessagesAbove()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    logger.setOutputMode(QXmppLogger::OutputMode::Pretty);
    logger.setColorMode(QXmppLogger::ColorOff);
    logger.setElideXmlTextAbove(std::nullopt);
    logger.setElideLogMessagesAbove(1000);
    QCOMPARE(logger.elideLogMessagesAbove().value(), qsizetype(1000));

    QSignalSpy spy(&logger, &QXmppLogger::message);

    auto stanza = hugeStanza();
    logger.log(QXmppLogger::SentMessage, stanza);
    // long messages of other types are never elided
    logger.log(QXmppLogger::DebugMessage, stanza);
    // short messages stay untouched
    logger.log(QXmppLogger::ReceivedMessage, u"<iq type=\"get\"/>"_s);

    QCOMPARE(spy.size(), 3);

    auto elided = spy.at(0).at(1).toString();
    QVERIFY(elided.size() < 1500);
    QVERIFY(elided.startsWith(u"<iq type=\"result\">\n  <data>AAA"));
    QVERIFY(elided.endsWith(u"AAA</data>\n</iq>"));
    QVERIFY(elided.contains(u"characters elided"));

    QCOMPARE(spy.at(1).at(1).toString(), stanza);
    QCOMPARE(spy.at(2).at(1).toString(), u"<iq type=\"get\"/>"_s);
}

void tst_QXmppLogger::testElideXmlTextAboveNeedsPrettyOutput()
{
    auto stanza = hugeStanza();

    // with raw output the XML text limit has no effect
    QXmppLogger plain;
    plain.setLoggingType(QXmppLogger::SignalLogging);
    plain.setElideXmlTextAbove(100);
    QCOMPARE(plain.elideXmlTextAbove().value(), qsizetype(100));

    QSignalSpy plainSpy(&plain, &QXmppLogger::message);
    plain.log(QXmppLogger::SentMessage, stanza);
    QCOMPARE(plainSpy.size(), 1);
    QCOMPARE(plainSpy.at(0).at(1).toString(), stanza);

    // with pretty output the text node is elided, the XML structure is kept
    QXmppLogger pretty;
    pretty.setLoggingType(QXmppLogger::SignalLogging);
    pretty.setOutputMode(QXmppLogger::OutputMode::Pretty);
    pretty.setColorMode(QXmppLogger::ColorOff);
    pretty.setElideXmlTextAbove(100);

    QSignalSpy prettySpy(&pretty, &QXmppLogger::message);
    pretty.log(QXmppLogger::SentMessage, stanza);
    QCOMPARE(prettySpy.size(), 1);

    auto payload = prettySpy.at(0).at(1).toString();
    QVERIFY(payload.size() < 1000);
    QVERIFY(payload.startsWith(u"<iq type=\"result\">\n  <data>AAA"));
    QVERIFY(payload.endsWith(u"AAA</data>\n</iq>"));
    QVERIFY(payload.contains(u"…[99900 characters elided]…"));
}

void tst_QXmppLogger::testEnableEliding()
{
    QXmppLogger logger;
    logger.enableEliding();
    QCOMPARE(logger.elideXmlTextAbove().value(), QXmppLogger::DefaultElideXmlTextAbove);
    QCOMPARE(logger.elideLogMessagesAbove().value(), QXmppLogger::DefaultElideLogMessagesAbove);

    logger.enableEliding(false);
    QVERIFY(!logger.elideXmlTextAbove());
    QVERIFY(!logger.elideLogMessagesAbove());
}

void tst_QXmppLogger::testPrettyOutputElides()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    logger.setOutputMode(QXmppLogger::OutputMode::Pretty);
    logger.setColorMode(QXmppLogger::ColorOff);

    QSignalSpy spy(&logger, &QXmppLogger::message);
    logger.log(QXmppLogger::SentMessage, hugeStanza());

    QCOMPARE(spy.size(), 1);
    auto payload = spy.at(0).at(1).toString();
    QVERIFY(payload.size() < QXmppLogger::DefaultElideLogMessagesAbove + 1000);
    QVERIFY(payload.contains(u"characters elided"));

    // pretty output can be kept while logging full stanzas
    logger.enableEliding(false);
    spy.clear();
    logger.log(QXmppLogger::SentMessage, hugeStanza());

    QCOMPARE(spy.size(), 1);
    QVERIFY(!spy.at(0).at(1).toString().contains(u"elided"));
}

void tst_QXmppLogger::testPrettyXmlMapsToOutputMode()
{
    QXmppLogger logger;
    QSignalSpy modeSpy(&logger, &QXmppLogger::outputModeChanged);
    QSignalSpy prettySpy(&logger, &QXmppLogger::prettyXmlChanged);

    QVERIFY(!logger.prettyXml());

    logger.setPrettyXml(true);
    QCOMPARE(logger.outputMode(), QXmppLogger::OutputMode::Pretty);
    QVERIFY(logger.prettyXml());
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(prettySpy.size(), 1);

    logger.setPrettyXml(false);
    QCOMPARE(logger.outputMode(), QXmppLogger::OutputMode::Raw);
    QVERIFY(!logger.prettyXml());
    QCOMPARE(modeSpy.size(), 2);
    QCOMPARE(prettySpy.size(), 2);
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
    Q_SLOT void testSensitivitySerialization();
    Q_SLOT void testSensitivityParsing();
    Q_SLOT void testEncryptedContainer();
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

static QString serializeMessage(const QXmppMessage &message, QXmpp::SceMode sceMode)
{
    QString output;
    QXmlStreamWriter writer(&output);
    message.toXml(&writer, sceMode);
    return output;
}

void tst_QXmppXmlRegistry::testSensitivitySerialization()
{
    Registry::registerElement<FooExtension>(Scope::Message);
    Registry::registerElement<BarExtension>(Scope::Message, Sensitivity::Public);
    auto cleanup = qScopeGuard([] {
        Registry::unregisterElement<FooExtension>();
        Registry::unregisterElement<BarExtension>();
    });

    QCOMPARE(Registry::sensitivity(std::any(FooExtension {})), Sensitivity::Sensitive);
    QCOMPARE(Registry::sensitivity(std::any(BarExtension {})), Sensitivity::Public);
    QCOMPARE(Registry::sensitivity(std::any(42)), Sensitivity::Sensitive);

    QXmppMessage message;
    message.extensions().add(FooExtension { u"secret"_s });
    message.extensions().add(BarExtension { 3 });
    message.publicExtensions().add(FooExtension { u"fallback"_s });

    const auto all = serializeMessage(message, QXmpp::SceAll);
    QVERIFY(all.contains(u"secret"));
    QVERIFY(all.contains(u"<bar"));
    QVERIFY(!all.contains(u"fallback"));

    const auto publicPart = serializeMessage(message, QXmpp::ScePublic);
    QVERIFY(!publicPart.contains(u"secret"));
    QVERIFY(publicPart.contains(u"<bar"));
    QVERIFY(publicPart.contains(u"fallback"));

    const auto sensitivePart = serializeMessage(message, QXmpp::SceSensitive);
    QVERIFY(sensitivePart.contains(u"secret"));
    QVERIFY(!sensitivePart.contains(u"<bar"));
    QVERIFY(!sensitivePart.contains(u"fallback"));
}

void tst_QXmppXmlRegistry::testSensitivityParsing()
{
    Registry::registerElement<FooExtension>(Scope::Message);
    Registry::registerElement<BarExtension>(Scope::Message, Sensitivity::Public);
    auto cleanup = qScopeGuard([] {
        Registry::unregisterElement<FooExtension>();
        Registry::unregisterElement<BarExtension>();
    });

    const auto xml = xmlToDom(QByteArrayLiteral(
        "<message to='alice@example.org'>"
        "<foo xmlns='urn:test:foo' value='outer'/>"
        "<bar xmlns='urn:test:bar' n='2'/>"
        "<unknown xmlns='urn:test:unknown'/>"
        "</message>"));

    // public part of an encrypted message: sensitive and unknown elements are kept apart
    QXmppMessage publicPart;
    publicPart.parse(xml, QXmpp::ScePublic);
    QVERIFY(!publicPart.extensions().contains<FooExtension>());
    QVERIFY(!publicPart.extensions().contains<QXmpp::Xml::Element>());
    QCOMPARE(publicPart.extensions().get<BarExtension>()->count, 2);
    QCOMPARE(publicPart.publicExtensions().get<FooExtension>()->value, u"outer"_s);
    QVERIFY(publicPart.publicExtensions().contains<QXmpp::Xml::Element>());

    QXmppMessage unencrypted;
    unencrypted.parse(xml, QXmpp::SceAll);
    QCOMPARE(unencrypted.extensions().get<FooExtension>()->value, u"outer"_s);
    QVERIFY(unencrypted.extensions().contains<BarExtension>());
    QVERIFY(unencrypted.publicExtensions().isEmpty());
}

void tst_QXmppXmlRegistry::testEncryptedContainer()
{
    QVERIFY(Registry::encryption(xmlToDom(QByteArrayLiteral("<message><debug xmlns='urn:test:debug'/></message>"))).isNull());

    // an encryption unknown to QXmpp
    Registry::registerEncryptedContainer<GenericExtension>(Scope::Message);
    auto cleanup = qScopeGuard([] { Registry::unregisterElement<GenericExtension>(); });

    QCOMPARE(Registry::encryption(xmlToDom(QByteArrayLiteral("<message><body>x</body><debug xmlns='urn:test:debug'/></message>"))), u"urn:test:debug");
    QVERIFY(Registry::encryption(xmlToDom(QByteArrayLiteral("<message><body>x</body></message>"))).isNull());
    QCOMPARE(Registry::encryption(std::any(GenericExtension {})), u"urn:test:debug");
    QCOMPARE(Registry::sensitivity(std::any(GenericExtension {})), Sensitivity::Public);
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
    Q_SLOT void elideLongTextNode();
    Q_SLOT void elideKeepsShortTextAndAttributes();
    Q_SLOT void elideColorized();
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
    logger.setOutputMode(QXmppLogger::OutputMode::Pretty);
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

void tst_QXmppXmlFormatter::elideLongTextNode()
{
    using namespace QXmpp::Private;

    QString in = u"<message><data>"_s + QString(1000, u'A') + u"</data><thread>x</thread></message>"_s;
    auto out = formatXmlForDebug(in, { true, 2, false, 100 });

    QVERIFY(out.startsWith(u"<message>\n  <data>"));
    QVERIFY(out.endsWith(u"</data>\n  <thread>x</thread>\n</message>"));
    QVERIFY(out.contains(QString(50, u'A') + u"…[900 characters elided]…"_s + QString(50, u'A')));
    QVERIFY(out.size() < in.size());

    QCOMPARE(formatXmlForDebug(in, { true, 2, false, {} }), QXmpp::formatXmlForDebug(in));
}

void tst_QXmppXmlFormatter::elideKeepsShortTextAndAttributes()
{
    using namespace QXmpp::Private;

    // text shorter than the limit is left alone
    auto out = formatXmlForDebug(u"<body>hello world</body>"_s, { true, 2, false, 100 });
    QCOMPARE(out, u"<body>hello world</body>"_s);

    // attribute values are not elided
    auto value = QString(1000, u'A');
    QString withAttr = u"<data value=\""_s + value + u"\"/>"_s;
    QVERIFY(formatXmlForDebug(withAttr, { true, 2, false, 100 }).contains(value));
}

void tst_QXmppXmlFormatter::elideColorized()
{
    using namespace QXmpp::Private;

    QString in = u"<data>"_s + QString(1000, u'A') + u"</data>"_s;
    auto out = formatXmlForDebug(in, { true, 2, true, 100 });

    QVERIFY(out.contains(u"characters elided"));
    // no escape sequence was cut in half
    static const QRegularExpression ansiRe(u"\x1b\\[[0-9;]*m"_s);
    auto stripped = out;
    stripped.remove(ansiRe);
    QVERIFY(!stripped.contains(QChar(0x1b)));
    QVERIFY(stripped.contains(u"…[900 characters elided]…"));
}

// ============================================================

}  // namespace XmlHelpers

// ============================================================
// QXmpp::Jid

namespace JidTests {

using namespace QXmpp;

class tst_QXmppJid : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void valid_data();
    Q_SLOT void valid();
    Q_SLOT void invalid_data();
    Q_SLOT void invalid();
    Q_SLOT void parts();
    Q_SLOT void comparison();
    Q_SLOT void lengthLimits();
    Q_SLOT void modifiers();
    Q_SLOT void escaping_data();
    Q_SLOT void escaping();
    Q_SLOT void unescaping();
    Q_SLOT void userInput_data();
    Q_SLOT void userInput();
    Q_SLOT void userInputInvalid();
    Q_SLOT void display();
};

static Jid parsed(QStringView string)
{
    auto result = Jid::parse(string);
    if (auto *error = std::get_if<JidParsingError>(&result)) {
        qWarning() << "Unexpected error:" << error->description;
        return {};
    }
    return std::get<Jid>(result);
}

void tst_QXmppJid::valid_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("canonical");

    // RFC 7622, section 3.5, table 1
    QTest::newRow("rfc-1") << u"juliet@example.com"_s << u"juliet@example.com"_s;
    QTest::newRow("rfc-2") << u"juliet@example.com/foo"_s << u"juliet@example.com/foo"_s;
    QTest::newRow("rfc-3") << u"juliet@example.com/foo bar"_s << u"juliet@example.com/foo bar"_s;
    QTest::newRow("rfc-4") << u"juliet@example.com/foo@bar"_s << u"juliet@example.com/foo@bar"_s;
    QTest::newRow("rfc-5") << u"foo\\20bar@example.com"_s << u"foo\\20bar@example.com"_s;
    QTest::newRow("rfc-6") << u"fussball@example.com"_s << u"fussball@example.com"_s;
    QTest::newRow("rfc-7") << u"fu\u00DFball@example.com"_s << u"fu\u00DFball@example.com"_s;
    QTest::newRow("rfc-8") << u"\u03C0@example.com"_s << u"\u03C0@example.com"_s;
    QTest::newRow("rfc-9") << u"\u03A3@example.com/foo"_s << u"\u03C3@example.com/foo"_s;
    QTest::newRow("rfc-10") << u"\u03C3@example.com/foo"_s << u"\u03C3@example.com/foo"_s;
    QTest::newRow("rfc-11") << u"\u03C2@example.com/foo"_s << u"\u03C2@example.com/foo"_s;
    QTest::newRow("rfc-12") << u"king@example.com/\u265A"_s << u"king@example.com/\u265A"_s;
    QTest::newRow("rfc-13") << u"example.com"_s << u"example.com"_s;
    QTest::newRow("rfc-14") << u"example.com/foobar"_s << u"example.com/foobar"_s;
    QTest::newRow("rfc-15") << u"a.example.com/b@example.net"_s << u"a.example.com/b@example.net"_s;
    // RFC 7622 erratum 4560: example 18 is valid
    QTest::newRow("rfc-18") << u"juliet@example.com/ foo"_s << u"juliet@example.com/ foo"_s;

    // localpart
    QTest::newRow("case-mapping") << u"Juliet@Example.COM/Balcony"_s << u"juliet@example.com/Balcony"_s;
    QTest::newRow("width-mapping") << u"\uFF4A\uFF55\uFF4C\uFF49\uFF45\uFF54@example.com"_s << u"juliet@example.com"_s;
    QTest::newRow("nfc-localpart") << u"e\u0301@example.com"_s << u"\u00E9@example.com"_s;
    QTest::newRow("special-casing") << u"\u0130@example.com"_s << u"i\u0307@example.com"_s;
    QTest::newRow("hebrew") << u"\u05E9\u05DC\u05D5\u05DD@example.com"_s << u"\u05E9\u05DC\u05D5\u05DD@example.com"_s;
    QTest::newRow("middle-dot") << u"l\u00B7l@example.com"_s << u"l\u00B7l@example.com"_s;
    QTest::newRow("zwnj-after-virama") << u"\u0915\u094D\u200C\u0937@example.com"_s << u"\u0915\u094D\u200C\u0937@example.com"_s;
    QTest::newRow("zwnj-arabic") << u"\u0628\u200C\u0628@example.com"_s << u"\u0628\u200C\u0628@example.com"_s;
    QTest::newRow("rtl-digits") << u"\u05D0123@example.com"_s << u"\u05D0123@example.com"_s;
    QTest::newRow("non-ascii-case-mapping") << u"\u00C4B@example.com"_s << u"\u00E4b@example.com"_s;
    QTest::newRow("backslash") << u"a\\b@example.com"_s << u"a\\b@example.com"_s;

    // resourcepart
    QTest::newRow("nbsp-resource") << u"juliet@example.com/foo\u00A0bar"_s << u"juliet@example.com/foo bar"_s;
    QTest::newRow("nfc-resource") << u"juliet@example.com/e\u0301"_s << u"juliet@example.com/\u00E9"_s;
    QTest::newRow("fullwidth-resource") << u"juliet@example.com/\uFF21"_s << u"juliet@example.com/\uFF21"_s;
    QTest::newRow("slash-in-resource") << u"juliet@example.com/a/b"_s << u"juliet@example.com/a/b"_s;

    // domainpart
    QTest::newRow("trailing-dot") << u"juliet@example.com./foo"_s << u"juliet@example.com/foo"_s;
    QTest::newRow("idn") << u"juliet@M\u00DCLLER.de"_s << u"juliet@m\u00FCller.de"_s;
    QTest::newRow("a-label") << u"juliet@xn--mller-kva.de"_s << u"juliet@m\u00FCller.de"_s;
    QTest::newRow("fullwidth-domain") << u"\uFF45\uFF58\uFF41\uFF4D\uFF50\uFF4C\uFF45.com"_s << u"example.com"_s;
    QTest::newRow("ideographic-dot") << u"\u4F8B\u3048\u3002\u30C6\u30B9\u30C8"_s << u"\u4F8B\u3048.\u30C6\u30B9\u30C8"_s;
    QTest::newRow("unqualified") << u"juliet@localhost"_s << u"juliet@localhost"_s;
    QTest::newRow("ipv4") << u"juliet@192.0.2.1/foo"_s << u"juliet@192.0.2.1/foo"_s;
    QTest::newRow("ipv6") << u"juliet@[2001:DB8::0:1]"_s << u"juliet@[2001:db8::1]"_s;
}

void tst_QXmppJid::valid()
{
    QFETCH(QString, input);
    QFETCH(QString, canonical);

    auto jid = parsed(input);
    QVERIFY(!jid.isNull());
    QCOMPARE(jid.toString(), canonical);
    // enforcement is idempotent
    QCOMPARE(parsed(canonical), jid);
}

void tst_QXmppJid::invalid_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("type");

    // RFC 7622, section 3.5, table 2
    QTest::newRow("rfc-16") << u"\"juliet\"@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rfc-17") << u"foo bar@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rfc-19") << u"@example.com/"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rfc-20") << u"henry\u2163@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rfc-21") << u"\u265A@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rfc-22") << u"juliet@"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("rfc-23") << u"/foobar"_s << int(JidParsingError::InvalidDomainpart);

    QTest::newRow("empty") << QString() << int(JidParsingError::Empty);
    QTest::newRow("empty-resource") << u"juliet@example.com/"_s << int(JidParsingError::InvalidResourcepart);

    // localpart
    for (auto c : { u'&', u'\'', u':', u'<', u'>' }) {
        QTest::addRow("excluded-%c", char(c)) << u"a"_s + QChar(c) + u"b@example.com"_s << int(JidParsingError::InvalidLocalpart);
    }
    QTest::newRow("fullwidth-at") << u"a\uFF20b@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("compat-ligature") << u"\uFB01@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("bidi-mixed") << u"a\u05E9@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("middle-dot-context") << u"a\u00B7b@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("zwnj-context") << u"a\u200Cb@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("rtl-mixed-digits") << u"\u05D01\u0661@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("zwj-context") << u"a\u200Db@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("arabic-digits-mixed") << u"\u0660\u06F0@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("soft-hyphen") << u"a\u00ADb@example.com"_s << int(JidParsingError::InvalidLocalpart);
    QTest::newRow("unassigned") << u"a\u0378b@example.com"_s << int(JidParsingError::InvalidLocalpart);

    // resourcepart
    QTest::newRow("control-resource") << u"juliet@example.com/a\u0007"_s << int(JidParsingError::InvalidResourcepart);
    QTest::newRow("zwj-resource") << u"juliet@example.com/a\u200Db"_s << int(JidParsingError::InvalidResourcepart);
    QTest::newRow("private-use-resource") << u"juliet@example.com/\uE000"_s << int(JidParsingError::InvalidResourcepart);
    QTest::newRow("lone-surrogate") << u"juliet@example.com/a"_s + QChar(0xD800) << int(JidParsingError::InvalidResourcepart);

    // domainpart
    QTest::newRow("at-in-domain") << u"a@b@example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("underscore") << u"a_b.example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("leading-hyphen") << u"-a.example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("reserved-hyphens") << u"ab--c.example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("bad-a-label") << u"xn--zz.example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("empty-label") << u"a..example.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("space") << u"exa mple.com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("long-label") << QString(64, u'a') + u".com"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("numeric-tld") << u"1.2.3"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("bad-ipv4") << u"1.2.3.256"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("ipv6-zone") << u"[fe80::1%25eth0]"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("ipv6-unbracketed") << u"::1"_s << int(JidParsingError::InvalidDomainpart);
    QTest::newRow("ipv4-in-brackets") << u"[192.0.2.1]"_s << int(JidParsingError::InvalidDomainpart);
}

void tst_QXmppJid::invalid()
{
    QFETCH(QString, input);
    QFETCH(int, type);

    auto result = Jid::parse(input);
    QVERIFY(std::holds_alternative<JidParsingError>(result));
    const auto &error = std::get<JidParsingError>(result);
    QCOMPARE(int(error.type), type);
    QVERIFY(!error.description.isEmpty());
    QVERIFY(!Jid::fromString(input));
}

void tst_QXmppJid::parts()
{
    auto full = parsed(u"juliet@example.com/balcony");
    QCOMPARE(full.localpart(), u"juliet");
    QCOMPARE(full.domain(), u"example.com");
    QCOMPARE(full.resource(), u"balcony");
    QVERIFY(full.hasLocalpart());
    QVERIFY(full.hasResource());
    QVERIFY(full.isFull());
    QVERIFY(!full.isBare());
    QVERIFY(!full.isDomain());

    auto domainWithResource = parsed(u"a.example.com/b@example.net");
    QVERIFY(!domainWithResource.hasLocalpart());
    QCOMPARE(domainWithResource.localpart(), QStringView());
    QCOMPARE(domainWithResource.domain(), u"a.example.com");
    QCOMPARE(domainWithResource.resource(), u"b@example.net");

    auto domain = parsed(u"example.com");
    QVERIFY(domain.isDomain());
    QVERIFY(domain.isBare());
    QVERIFY(!domain.isFull());
    QCOMPARE(domain.resource(), QStringView());

    Jid null;
    QVERIFY(null.isNull());
    QVERIFY(!null.isBare());
    QVERIFY(!null.isFull());
    QVERIFY(!null.isDomain());
    QCOMPARE(null.domain(), QStringView());

    QCOMPARE(parsed(u"juliet@m\u00FCller.de").domainAce(), QByteArray("xn--mller-kva.de"));
    QCOMPARE(parsed(u"juliet@[::1]").domainAce(), QByteArray("[::1]"));

    auto fromParts = Jid::fromParts(u"Juliet", u"Example.com", u"balcony");
    QCOMPARE(std::get<Jid>(fromParts), full);
    QCOMPARE(std::get<Jid>(Jid::fromParts({}, u"example.com")), domain);
    QVERIFY(std::holds_alternative<JidParsingError>(Jid::fromParts(u"a@b", u"example.com")));
    QVERIFY(std::holds_alternative<JidParsingError>(Jid::fromParts(u"juliet", {})));
}

void tst_QXmppJid::comparison()
{
    QCOMPARE(parsed(u"\u03A3@example.com"), parsed(u"\u03C3@example.com"));
    QVERIFY(parsed(u"\u03C3@example.com") != parsed(u"\u03C2@example.com"));
    QVERIFY(parsed(u"fussball@example.com") != parsed(u"fu\u00DFball@example.com"));
    QCOMPARE(parsed(u"JULIET@EXAMPLE.COM"), parsed(u"juliet@example.com"));
    QVERIFY(parsed(u"juliet@example.com/Foo") != parsed(u"juliet@example.com/foo"));
    QCOMPARE(parsed(u"juliet@xn--mller-kva.de"), parsed(u"juliet@M\u00FCLLER.DE."));

    QSet<Jid> set { parsed(u"Juliet@Example.com"), parsed(u"juliet@example.com"), parsed(u"romeo@example.com") };
    QCOMPARE(set.size(), 2);

    QVERIFY(parsed(u"a@example.com") < parsed(u"b@example.com"));
    QCOMPARE(parsed(u"a@example.com") <=> parsed(u"A@example.com"), std::strong_ordering::equal);
}

void tst_QXmppJid::lengthLimits()
{
    QVERIFY(Jid::fromString(QString(QString(1023, u'a') + u"@example.com"_s)));
    QVERIFY(!Jid::fromString(QString(QString(1024, u'a') + u"@example.com"_s)));

    // limits are in bytes of UTF-8: U+20AC needs three
    QVERIFY(Jid::fromString(QString(u"example.com/"_s + QString(341, QChar(0x20AC)))));
    QVERIFY(!Jid::fromString(QString(u"example.com/"_s + QString(342, QChar(0x20AC)))));

    QString longDomain;
    for (int i = 0; i < 4; ++i) {
        longDomain += QString(63, u'a') + u'.';
    }
    longDomain.chop(2);  // 4 * 64 - 2 = 254 characters
    QVERIFY(!Jid::fromString(longDomain));
    longDomain.chop(1);
    QVERIFY(Jid::fromString(longDomain));
}

void tst_QXmppJid::modifiers()
{
    auto full = parsed(u"juliet@example.com/balcony");
    QCOMPARE(full.bare(), parsed(u"juliet@example.com"));
    QCOMPARE(full.bare().bare(), full.bare());
    QCOMPARE(full.domainJid(), parsed(u"example.com"));
    QCOMPARE(std::get<Jid>(full.withResource(u"garden")), parsed(u"juliet@example.com/garden"));
    QCOMPARE(std::get<Jid>(parsed(u"example.com").withResource(u"x\u00A0y")), parsed(u"example.com/x y"));
    QVERIFY(std::holds_alternative<JidParsingError>(full.withResource({})));
    QVERIFY(std::holds_alternative<JidParsingError>(full.withResource(u"\u0007")));
    QVERIFY(std::holds_alternative<JidParsingError>(Jid().withResource(u"foo")));
    QVERIFY(Jid().bare().isNull());
    QVERIFY(Jid().domainJid().isNull());
}

void tst_QXmppJid::escaping_data()
{
    QTest::addColumn<QString>("unescaped");
    QTest::addColumn<QString>("escaped");

    // XEP-0106, section 5
    QTest::newRow("1") << u"space cadet"_s << u"space\\20cadet"_s;
    QTest::newRow("2") << u"call me \"ishmael\""_s << u"call\\20me\\20\\22ishmael\\22"_s;
    QTest::newRow("3") << u"at&t guy"_s << u"at\\26t\\20guy"_s;
    QTest::newRow("4") << u"d'artagnan"_s << u"d\\27artagnan"_s;
    QTest::newRow("5") << u"/.fanboy"_s << u"\\2f.fanboy"_s;
    QTest::newRow("6") << u"::foo::"_s << u"\\3a\\3afoo\\3a\\3a"_s;
    QTest::newRow("7") << u"<foo>"_s << u"\\3cfoo\\3e"_s;
    QTest::newRow("8") << u"user@host"_s << u"user\\40host"_s;
    QTest::newRow("9") << u"c:\\net"_s << u"c\\3a\\net"_s;
    QTest::newRow("10") << u"c:\\\\net"_s << u"c\\3a\\\\net"_s;
    QTest::newRow("11") << u"c:\\cool stuff"_s << u"c\\3a\\cool\\20stuff"_s;
    QTest::newRow("12") << u"c:\\5commas"_s << u"c\\3a\\5c5commas"_s;
    // XEP-0106, section 4.3
    QTest::newRow("existing-escape") << u"\\27"_s << u"\\5c27"_s;
    QTest::newRow("existing-backslash-escape") << u"\\5c"_s << u"\\5c5c"_s;
    QTest::newRow("wv") << u"\\3and\\2is\\5cool"_s << u"\\5c3and\\2is\\5c5cool"_s;
}

void tst_QXmppJid::escaping()
{
    QFETCH(QString, unescaped);
    QFETCH(QString, escaped);

    QCOMPARE(Jid::escapeLocalpart(unescaped), escaped);
    QCOMPARE(Jid::unescapeLocalpart(escaped), unescaped);

    auto jid = Jid::fromParts(escaped, u"example.com");
    QVERIFY(std::holds_alternative<Jid>(jid));
    QCOMPARE(std::get<Jid>(jid).unescapedLocalpart(), unescaped);
}

void tst_QXmppJid::unescaping()
{
    // leading and trailing spaces cannot be escaped
    QCOMPARE(Jid::escapeLocalpart(u"  foo bar "), u"foo\\20bar"_s);
    // case mapping of the localpart must not turn an escape-like sequence into an escape
    QCOMPARE(Jid::escapeLocalpart(u"\\2F"), u"\\5c2F"_s);
    // only the specified sequences are unescaped
    QCOMPARE(Jid::unescapeLocalpart(u"\\21\\2\\"), u"\\21\\2\\"_s);
    QCOMPARE(Jid::unescapeLocalpart(u"\\2F"), u"\\2F"_s);
}

void tst_QXmppJid::userInput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    auto row = [](const char *name, QString input, QString expected) {
        QTest::newRow(name) << input << expected;
    };
    row("plain", u"Juliet@Example.com/Balcony"_s, u"juliet@example.com/Balcony"_s);
    row("whitespace", u" \tjuliet@example.com\n"_s, u"juliet@example.com"_s);
    row("invisible", u"\u200Ejuliet@example.com\uFEFF"_s, u"juliet@example.com"_s);
    row("whitespace-in-parts", u"Juliet @ example.com / balcony"_s, u"juliet@example.com/balcony"_s);
    row("trailing-slash", u"example.com/"_s, u"example.com"_s);
    row("space", u"space cadet@example.com"_s, u"space\\20cadet@example.com"_s);
    row("apostrophe", u"d'artagnan@musketeers.lit"_s, u"d\\27artagnan@musketeers.lit"_s);
    row("at", u"user@host@example.com"_s, u"user\\40host@example.com"_s);
    row("colon-backslash", u"c:\\net@example.com"_s, u"c\\3a\\net@example.com"_s);
    row("already-escaped", u"d\\27artagnan@musketeers.lit"_s, u"d\\27artagnan@musketeers.lit"_s);
    row("resource", u"juliet@example.com/a@b/c d"_s, u"juliet@example.com/a@b/c d"_s);
}

void tst_QXmppJid::userInput()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    auto result = Jid::fromUserInput(input);
    if (auto *error = std::get_if<JidParsingError>(&result)) {
        QFAIL(qPrintable(error->description));
    }
    QCOMPARE(std::get<Jid>(result).toString(), expected);
}

void tst_QXmppJid::userInputInvalid()
{
    using enum JidParsingError::Type;
    auto errorType = [](QStringView input) {
        auto result = Jid::fromUserInput(input);
        return std::holds_alternative<JidParsingError>(result) ? std::get<JidParsingError>(result).type : JidParsingError::Type(-1);
    };
    QCOMPARE(errorType(u" \u200B "), Empty);
    QCOMPARE(errorType(u"@example.com"), InvalidLocalpart);
    QCOMPARE(errorType(u"juliet@"), InvalidDomainpart);
    QCOMPARE(errorType(u"\u265A@example.com"), InvalidLocalpart);
}

void tst_QXmppJid::display()
{
    QCOMPARE(parsed(u"d\\27artagnan@musketeers.lit/Foo\\27").toDisplayString(), u"d'artagnan@musketeers.lit/Foo\\27"_s);
    QCOMPARE(parsed(u"musketeers.lit/d\\27").toDisplayString(), u"musketeers.lit/d\\27"_s);
    QCOMPARE(Jid().toDisplayString(), QString());

    QString string = parsed(u"Juliet@Example.com");
    QCOMPARE(string, u"juliet@example.com"_s);
    QCOMPARE(u"xmpp:"_s + parsed(u"Juliet@Example.com") + u'?' + u"message", u"xmpp:juliet@example.com?message"_s);

    QString debugOutput;
    QDebug(&debugOutput) << parsed(u"juliet@example.com");
    QCOMPARE(debugOutput.trimmed(), u"QXmpp::Jid(juliet@example.com)"_s);
}

}  // namespace JidTests

QXMPP_TEST_MAIN(Utils::tst_QXmppUtils, Logger::tst_QXmppLogger, XmlHelpers::tst_QXmppXmlElement, XmlHelpers::tst_QXmppXmlRegistry, XmlHelpers::tst_QXmppXmlFormatter, JidTests::tst_QXmppJid)

#include "CoreUtils.moc"

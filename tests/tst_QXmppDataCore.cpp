// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the core stanza and stream data classes. Merging
// tst_QXmppStanza and tst_QXmppStream into one translation unit parses the
// shared Qt/QXmpp headers once instead of once per file. Each original test
// keeps its own namespace; main() runs them in turn.

#include "QXmppBindIq.h"
#include "QXmppConstants_p.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppLogger.h"
#include "QXmppStanza.h"

#include "Stream.h"
#include "StreamError.h"
#include "XmppSocket.h"
#include "compat/QXmppSessionIq.h"
#include "compat/QXmppStartTlsPacket.h"
#include "util.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QObject>

Q_DECLARE_METATYPE(QDomElement);

namespace Stanza {

using namespace QXmpp;

class QXmppStanzaStub : public QXmppStanza
{
public:
    void toXml(QXmlStreamWriter *) const override { };
};

class tst_QXmppStanza : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testExtendedAddress_data();
    Q_SLOT void testExtendedAddress();

    Q_SLOT void testErrorCases_data();
    Q_SLOT void testErrorCases();
    Q_SLOT void testErrorFileTooLarge();
    Q_SLOT void testErrorRetry();
    Q_SLOT void testErrorEnums();
    Q_SLOT void errorJingleCondition();

    Q_SLOT void testEncryption();
    Q_SLOT void testSenderKey();
    Q_SLOT void testSceTimestamp();
};

void tst_QXmppStanza::testExtendedAddress_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("delivered");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QString>("jid");
    QTest::addColumn<QString>("type");

    QTest::newRow("simple")
        << QByteArray(R"(<address jid="foo@example.com/QXmpp" type="bcc"/>)")
        << false
        << QString()
        << u"foo@example.com/QXmpp"_s
        << u"bcc"_s;

    QTest::newRow("full")
        << QByteArray(R"(<address delivered="true" desc="some description" jid="foo@example.com/QXmpp" type="bcc"/>)")
        << true
        << u"some description"_s
        << u"foo@example.com/QXmpp"_s
        << u"bcc"_s;
}

void tst_QXmppStanza::testExtendedAddress()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, delivered);
    QFETCH(QString, description);
    QFETCH(QString, jid);
    QFETCH(QString, type);

    QXmppExtendedAddress address;
    parsePacket(address, xml);
    QCOMPARE(address.isDelivered(), delivered);
    QCOMPARE(address.description(), description);
    QCOMPARE(address.jid(), jid);
    QCOMPARE(address.type(), type);
    serializePacket(address, xml);
}

void tst_QXmppStanza::testErrorCases_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QXmppStanza::Error::Type>("type");
    QTest::addColumn<QXmppStanza::Error::Condition>("condition");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("redirectionUri");
    QTest::addColumn<QString>("by");

#define ROW(name, xml, type, condition, text, redirect, by) \
    QTest::newRow(QT_STRINGIFY(name))                       \
        << QByteArrayLiteral(xml)                           \
        << QXmppStanza::Error::type                         \
        << QXmppStanza::Error::condition                    \
        << text                                             \
        << redirect                                         \
        << by

#define BASIC(xml, type, condition) \
    ROW(condition, xml, type, condition, QString(), QString(), QString())

    ROW(
        empty - text,
        "<error type=\"modify\">"
        "<bad-request xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        BadRequest,
        QString(),
        QString(),
        QString());
    ROW(
        redirection - uri - gone,
        "<error by=\"example.net\" type=\"cancel\">"
        "<gone xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\">"
        "xmpp:romeo@afterlife.example.net"
        "</gone>"
        "</error>",
        Cancel,
        Gone,
        QString(),
        "xmpp:romeo@afterlife.example.net",
        "example.net");
    ROW(
        redirection - uri - redirect,
        "<error type=\"cancel\">"
        "<redirect xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\">"
        "xmpp:rms@afterlife.example.net"
        "</redirect>"
        "</error>",
        Cancel,
        Redirect,
        QString(),
        "xmpp:rms@afterlife.example.net",
        QString());
    ROW(
        redirection - uri - empty,
        "<error type=\"cancel\">"
        "<redirect xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        Redirect,
        QString(),
        QString(),
        QString());
    ROW(
        policy - violation - text,
        "<error by=\"example.net\" type=\"modify\">"
        "<policy-violation xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "<text xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\" xml:lang=\"en\">The used words are not allowed on this server.</text>"
        "</error>",
        Modify,
        PolicyViolation,
        "The used words are not allowed on this server.",
        QString(),
        "example.net");
    ROW(
        jid - malformed - with - by,
        "<error by=\"muc.example.com\" type=\"modify\">"
        "<jid-malformed xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        JidMalformed,
        QString(),
        QString(),
        "muc.example.com");

    BASIC(
        "<error type=\"modify\">"
        "<bad-request xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        BadRequest);
    BASIC(
        "<error type=\"cancel\">"
        "<conflict xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        Conflict);
    BASIC(
        "<error type=\"cancel\">"
        "<feature-not-implemented xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        FeatureNotImplemented);
    BASIC(
        "<error type=\"auth\">"
        "<forbidden xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Auth,
        Forbidden);
    BASIC(
        "<error type=\"cancel\">"
        "<gone xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        Gone);
    BASIC(
        "<error type=\"cancel\">"
        "<internal-server-error xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        InternalServerError);
    BASIC(
        "<error type=\"cancel\">"
        "<item-not-found xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        ItemNotFound);
    BASIC(
        "<error type=\"modify\">"
        "<jid-malformed xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        JidMalformed);
    BASIC(
        "<error type=\"modify\">"
        "<not-acceptable xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        NotAcceptable);
    BASIC(
        "<error type=\"cancel\">"
        "<not-allowed xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        NotAllowed);
    BASIC(
        "<error type=\"auth\">"
        "<not-authorized xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Auth,
        NotAuthorized);
    BASIC(
        "<error type=\"modify\">"
        "<policy-violation xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        PolicyViolation);
    BASIC(
        "<error type=\"wait\">"
        "<recipient-unavailable xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Wait,
        RecipientUnavailable);
    BASIC(
        "<error type=\"modify\">"
        "<redirect xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        Redirect);
    BASIC(
        "<error type=\"auth\">"
        "<registration-required xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Auth,
        RegistrationRequired);
    BASIC(
        "<error type=\"cancel\">"
        "<remote-server-not-found xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        RemoteServerNotFound);
    BASIC(
        "<error type=\"wait\">"
        "<remote-server-timeout xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Wait,
        RemoteServerTimeout);
    BASIC(
        "<error type=\"wait\">"
        "<resource-constraint xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Wait,
        ResourceConstraint);
    BASIC(
        "<error type=\"cancel\">"
        "<service-unavailable xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Cancel,
        ServiceUnavailable);
    BASIC(
        "<error type=\"auth\">"
        "<subscription-required xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Auth,
        SubscriptionRequired);
    BASIC(
        "<error type=\"modify\">"
        "<undefined-condition xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "</error>",
        Modify,
        UndefinedCondition);

#undef BASIC
#undef ROW
}

void tst_QXmppStanza::testErrorCases()
{
    QFETCH(QByteArray, xml);
    QFETCH(QXmppStanza::Error::Type, type);
    QFETCH(QXmppStanza::Error::Condition, condition);
    QFETCH(QString, text);
    QFETCH(QString, redirectionUri);
    QFETCH(QString, by);

    // parsing
    QXmppStanza::Error error;
    parsePacket(error, xml);
    QCOMPARE(error.type(), type);
    QCOMPARE(error.condition(), condition);
    QCOMPARE(error.text(), text);
    QCOMPARE(error.redirectionUri(), redirectionUri);
    QCOMPARE(error.by(), by);
    // check parsed error results in the same xml
    serializePacket(error, xml);

    // serialization from setters
    error = QXmppStanza::Error();
    error.setType(type);
    error.setCondition(condition);
    error.setText(text);
    error.setRedirectionUri(redirectionUri);
    error.setBy(by);
    serializePacket(error, xml);
}

void tst_QXmppStanza::testErrorFileTooLarge()
{
    const QByteArray xml(
        "<error type=\"modify\">"
        "<not-acceptable xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "<text xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\" xml:lang=\"en\">"
        "File too large. The maximum file size is 20000 bytes"
        "</text>"
        "<file-too-large xmlns=\"urn:xmpp:http:upload:0\">"
        "<max-file-size>20000</max-file-size>"
        "</file-too-large>"
        "</error>");

    QXmppStanza::Error error;
    parsePacket(error, xml);
    QCOMPARE(error.type(), QXmppStanza::Error::Modify);
    QCOMPARE(error.text(), QStringLiteral("File too large. The maximum file size is "
                                          "20000 bytes"));
    QCOMPARE(error.condition(), QXmppStanza::Error::NotAcceptable);
    QVERIFY(error.fileTooLarge());
    QCOMPARE(error.maxFileSize(), 20000);
    serializePacket(error, xml);

    // test setters
    error.setMaxFileSize(60000);
    QCOMPARE(error.maxFileSize(), 60000);
    error.setFileTooLarge(false);
    QVERIFY(!error.fileTooLarge());

    QXmppStanza::Error e2;
    e2.setMaxFileSize(123000);
    QVERIFY(e2.fileTooLarge());
}

void tst_QXmppStanza::testErrorRetry()
{
    const QByteArray xml(
        "<error type=\"wait\">"
        "<resource-constraint "
        "xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/>"
        "<text xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\" xml:lang=\"en\">"
        "Quota reached. You can only upload 5 files in 5 minutes"
        "</text>"
        "<retry xmlns=\"urn:xmpp:http:upload:0\" "
        "stamp=\"2017-12-03T23:42:05Z\"/>"
        "</error>");

    QXmppStanza::Error error;
    parsePacket(error, xml);
    QCOMPARE(error.type(), QXmppStanza::Error::Wait);
    QCOMPARE(error.text(), QStringLiteral("Quota reached. You can only upload 5 "
                                          "files in 5 minutes"));
    QCOMPARE(error.condition(), QXmppStanza::Error::ResourceConstraint);
    QCOMPARE(error.retryDate(), QDateTime(QDate(2017, 12, 03), QTime(23, 42, 05), TimeZoneUTC));
    serializePacket(error, xml);

    // test setter
    error.setRetryDate(QDateTime(QDate(1985, 10, 26), QTime(1, 35)));
    QCOMPARE(error.retryDate(), QDateTime(QDate(1985, 10, 26), QTime(1, 35)));
}

void tst_QXmppStanza::testErrorEnums()
{
    QXmppStanza::Error err;
    QCOMPARE(err.condition(), QXmppStanza::Error::NoCondition);
    QCOMPARE(err.type(), QXmppStanza::Error::NoType);

    err.setCondition(QXmppStanza::Error::BadRequest);
    err.setType(QXmppStanza::Error::Cancel);

    err.setCondition(QXmppStanza::Error::Condition(-1));
    err.setType(QXmppStanza::Error::Type(-1));

    QCOMPARE(err.condition(), QXmppStanza::Error::NoCondition);
    QCOMPARE(err.type(), QXmppStanza::Error::NoType);
}

void tst_QXmppStanza::errorJingleCondition()
{
    const auto xml = "<error type='cancel'><item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/><unknown-session xmlns='urn:xmpp:jingle:errors:1'/></error>";
    QXmppStanza::Error error;
    parsePacket(error, xml);
    QCOMPARE(error.type(), QXmppStanza::Error::Cancel);
    QCOMPARE(error.condition(), QXmppStanza::Error::ItemNotFound);
    QCOMPARE(error.jingleErrorCondition(), JingleErrorCondition::UnknownSession);

    QXmppStanza::Error error2;
    error2.setType(QXmppStanza::Error::Cancel);
    error2.setCondition(QXmppStanza::Error::ItemNotFound);
    error2.setJingleErrorCondition(JingleErrorCondition::UnknownSession);
    serializePacket(error2, xml);
}

void tst_QXmppStanza::testEncryption()
{
    QXmppStanzaStub stanza;
    QVERIFY(!stanza.e2eeMetadata().has_value());
    QXmppE2eeMetadata e2eeMetadata;
    e2eeMetadata.setEncryption(QXmpp::Omemo2);
    stanza.setE2eeMetadata(e2eeMetadata);
    QCOMPARE(stanza.e2eeMetadata()->encryption(), QXmpp::Omemo2);
}

void tst_QXmppStanza::testSenderKey()
{
    QXmppStanzaStub stanza;
    QVERIFY(!stanza.e2eeMetadata().has_value());
    QXmppE2eeMetadata e2eeMetadata;
    e2eeMetadata.setSenderKey(QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));
    stanza.setE2eeMetadata(e2eeMetadata);
    QCOMPARE(stanza.e2eeMetadata()->senderKey(), QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));
}

void tst_QXmppStanza::testSceTimestamp()
{
    QXmppStanzaStub stanza;
    QVERIFY(!stanza.e2eeMetadata().has_value());
    QXmppE2eeMetadata e2eeMetadata;
    QVERIFY(e2eeMetadata.senderKey().isNull());
    QVERIFY(e2eeMetadata.sceTimestamp().isNull());
    e2eeMetadata.setSceTimestamp(QDateTime(QDate(2022, 01, 01), QTime()));
    stanza.setE2eeMetadata(e2eeMetadata);
    QCOMPARE(stanza.e2eeMetadata()->sceTimestamp(), QDateTime(QDate(2022, 01, 01), QTime()));
}

}  // namespace Stanza

// ============================================================

using namespace QXmpp;
using namespace QXmpp::Private;

class tst_QXmppStream : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void testProcessData();
    Q_SLOT void testLogReceived();
    Q_SLOT void streamOpen();
    Q_SLOT void testStreamError();
    Q_SLOT void starttlsPackets();

    // parsing
    Q_SLOT void testStartTlsPacket_data();
    Q_SLOT void testStartTlsPacket();

    Q_SLOT void testNoResource();
    Q_SLOT void testResource();
    Q_SLOT void testResult();

    Q_SLOT void testSessionIq();
};

void tst_QXmppStream::initTestCase()
{
    qRegisterMetaType<QDomElement>();
    qRegisterMetaType<QXmpp::Private::StreamOpen>();
}

void tst_QXmppStream::testProcessData()
{
    XmppSocket socket(this);

    QSignalSpy onStarted(&socket, &XmppSocket::started);
    QSignalSpy onStreamReceived(&socket, &XmppSocket::streamReceived);
    QSignalSpy onStanzaReceived(&socket, &XmppSocket::stanzaReceived);

    socket.processData(R"(<?xml version="1.0" encoding="UTF-8"?>)");
    socket.processData(R"(
        <stream:stream from='juliet@im.example.com'
                       to='im.example.com'
                       version='1.0'
                       xml:lang='en'
                       xmlns='jabber:client'
                       xmlns:stream='http://etherx.jabber.org/streams'>)");

    // check stream was found
    QCOMPARE(onStreamReceived.size(), 1);
    QCOMPARE(onStanzaReceived.size(), 0);
    QCOMPARE(onStarted.size(), 0);

    // check stream information
    const auto stream = onStreamReceived[0][0].value<StreamOpen>();
    QCOMPARE(stream.from, u"juliet@im.example.com"_s);
    QCOMPARE(stream.to, u"im.example.com"_s);
    QCOMPARE(stream.version, u"1.0"_s);

    socket.processData(R"(<stream:features>
            <starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'>
                <required/>
            </starttls>
        </stream:features>)");

    QCOMPARE(onStreamReceived.size(), 1);
    QCOMPARE(onStanzaReceived.size(), 1);
    QCOMPARE(onStarted.size(), 0);

    const auto features = onStanzaReceived[0][0].value<QDomElement>();
    QCOMPARE(features.tagName(), u"features"_s);
    QCOMPARE(features.namespaceURI(), u"http://etherx.jabber.org/streams"_s);

    // test partial data
    socket.processData(R"(<message from="juliet@im.example.co)");
    QCOMPARE(onStreamReceived.size(), 1);
    QCOMPARE(onStanzaReceived.size(), 1);
    QCOMPARE(onStarted.size(), 0);
    socket.processData(R"(m" to="stpeter@im.example.com">)");
    socket.processData(R"(<body>Moin</body>)");
    socket.processData(R"(</message>)");
    QCOMPARE(onStreamReceived.size(), 1);
    QCOMPARE(onStanzaReceived.size(), 2);
    QCOMPARE(onStarted.size(), 0);

    const auto message = onStanzaReceived[1][0].value<QDomElement>();
    QCOMPARE(message.tagName(), u"message"_s);
    QCOMPARE(message.namespaceURI(), u"jabber:client"_s);

    socket.processData(R"(</stream:stream>)");
}

void tst_QXmppStream::testLogReceived()
{
    XmppSocket socket(this);

    QSignalSpy onStanzaReceived(&socket, &XmppSocket::stanzaReceived);
    QSignalSpy onStreamClosed(&socket, &XmppSocket::streamClosed);
    QSignalSpy onLog(&socket, &XmppSocket::logMessage);

    // collect the text of all 'received' log messages emitted so far
    auto received = [&onLog]() {
        QStringList out;
        for (const auto &args : onLog) {
            if (args.at(0).toInt() == QXmppLogger::ReceivedMessage) {
                out << args.at(1).toString();
            }
        }
        return out;
    };

    // The XML declaration arrives separately, as real servers send it. It is not
    // logged as a unit; the stream-open header is logged exactly, without it.
    socket.processData(uR"(<?xml version="1.0" encoding="UTF-8"?>)"_s);
    socket.processData(uR"(<stream:stream from='juliet@im.example.com' version='1.0' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>)"_s);
    QCOMPARE(received().size(), 1);
    QVERIFY(received().constLast().trimmed().startsWith(u"<stream:stream"));
    QVERIFY(received().constLast().trimmed().endsWith(u">"));
    QVERIFY(!received().constLast().contains(u"<?xml"));

    // A complete stanza is logged byte-for-byte as received (original namespace
    // prefixes and indentation preserved, no QDom 'n1'/'n2' normalization).
    const auto features = uR"(<stream:features>
            <starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'>
                <required/>
            </starttls>
        </stream:features>)"_s;
    socket.processData(features);
    QCOMPARE(received().size(), 2);
    QCOMPARE(received().constLast(), features);

    // An empty (socket-level) ping is not logged, but still emits an (empty) stanza
    // so the keepalive/timeout logic keeps reacting to it.
    auto logsBefore = received().size();
    auto stanzasBefore = onStanzaReceived.size();
    socket.processData({});
    QCOMPARE(received().size(), logsBefore);
    QCOMPARE(onStanzaReceived.size(), stanzasBefore + 1);

    // A whitespace ping between stanzas is not logged either, but still emits an
    // (empty) keepalive stanza. The reader only yields the whitespace token once the
    // following stanza arrives, which is then logged on its own (without the
    // whitespace), exactly as received.
    logsBefore = received().size();
    stanzasBefore = onStanzaReceived.size();
    socket.processData(u" \n\t "_s);
    socket.processData(u"<iq id='1'/>"_s);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(), u"<iq id='1'/>"_s);
    QCOMPARE(onStanzaReceived.size(), stanzasBefore + 2);  // keepalive + the <iq/>

    // A stanza split across several chunks (including a split inside the start tag
    // and inside the body) is logged exactly once, as the full reassembled stanza.
    logsBefore = received().size();
    socket.processData(uR"(<message from="juliet@im.example.co)"_s);
    socket.processData(uR"(m" to="stpeter@im.example.com"><body>Mo)"_s);
    socket.processData(uR"(in</body></message>)"_s);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(),
             uR"(<message from="juliet@im.example.com" to="stpeter@im.example.com"><body>Moin</body></message>)"_s);

    // Multibyte (ö) and surrogate-pair (emoji) content stays byte-exact: proves the
    // character-offset slicing stays aligned with the UTF-16 QString buffer.
    const auto emojiMsg = u"<message><body>Möin \U0001F600</body></message>"_s;
    logsBefore = received().size();
    socket.processData(emojiMsg);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(), emojiMsg);

    // ... and the same with a chunk boundary before the emoji.
    logsBefore = received().size();
    socket.processData(u"<message><body>Mö"_s);
    socket.processData(u"in \U0001F600</body></message>"_s);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(), emojiMsg);

    // Self-closing top-level stanza.
    logsBefore = received().size();
    socket.processData(u"<presence/>"_s);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(), u"<presence/>"_s);

    // Stream close is logged and emits streamClosed().
    logsBefore = received().size();
    socket.processData(u"</stream:stream>"_s);
    QCOMPARE(received().size(), logsBefore + 1);
    QCOMPARE(received().constLast(), u"</stream:stream>"_s);
    QCOMPARE(onStreamClosed.size(), 1);
}

void tst_QXmppStream::streamOpen()
{
    auto xml = "<?xml version='1.0' encoding='UTF-8'?><stream:stream from='juliet@im.example.com' to='im.example.com' id='abcdefg' version='1.0' xml:lang='en' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>";

    StreamOpen s {
        .to = "im.example.com",
        .from = "juliet@im.example.com",
        .id = "abcdefg",
        .version = "1.0",
        .xmlns = ns_client.toString(),
        .xmlLang = "en",
    };
    serializePacket(s, xml);

    QXmlStreamReader r(xml);
    QCOMPARE(r.readNext(), QXmlStreamReader::StartDocument);
    QCOMPARE(r.readNext(), QXmlStreamReader::StartElement);
    auto streamOpen = StreamOpen::fromXml(r);
    QCOMPARE(streamOpen.from, "juliet@im.example.com");
    QCOMPARE(streamOpen.to, "im.example.com");
    QCOMPARE(streamOpen.id, "abcdefg");
    QCOMPARE(streamOpen.version, "1.0");
    QCOMPARE(streamOpen.xmlns, ns_client);
    QCOMPARE(streamOpen.xmlLang, "en");
}

void tst_QXmppStream::testStreamError()
{
    auto values = {
        std::tuple {
            "<stream:error><bad-format xmlns='urn:ietf:params:xml:ns:xmpp-streams'/></stream:error>",
            StreamErrorElement {
                StreamError::BadFormat,
                {},
            },
        },
        std::tuple {
            "<stream:error><see-other-host xmlns='urn:ietf:params:xml:ns:xmpp-streams'>[2001:41D0:1:A49b::1]:9222</see-other-host><text xmlns='urn:ietf:params:xml:ns:xmpp-streams'>Moved</text></stream:error>",
            StreamErrorElement {
                StreamErrorElement::SeeOtherHost { "2001:41d0:1:a49b::1", 9222 },
                "Moved",
            },
        },
    };
    const auto streamWrapper =
        u"<stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>%1</stream:stream>"_s;

    for (const auto &[xml, error] : values) {
        auto result = StreamErrorElement::fromDom(xmlToDom(streamWrapper.arg(xml)).firstChildElement());
        if (auto *parseErr = std::get_if<QXmppError>(&result)) {
            qDebug() << parseErr->description;
        }
        Q_ASSERT(std::holds_alternative<StreamErrorElement>(result));

        auto parsed = std::get<StreamErrorElement>(std::move(result));
        if (!(parsed == error)) {
            qDebug() << xml;
        }
        QCOMPARE(parsed, error);
    }
}

void tst_QXmppStream::starttlsPackets()
{
    auto xml1 = "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>";
    auto request = unwrap(StarttlsRequest::fromDom(xmlToDom(xml1)));
    serializePacket(request, xml1);

    auto xml2 = "<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>";
    auto proceed = unwrap(StarttlsProceed::fromDom(xmlToDom(xml2)));
    serializePacket(proceed, xml2);
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

void tst_QXmppStream::testStartTlsPacket_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QXmppStartTlsPacket::Type>("type");

#define ROW(name, xml, valid, type) \
    QTest::newRow(name)             \
        << QByteArrayLiteral(xml)   \
        << valid                    \
        << type

    ROW("starttls", R"(<starttls xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>)", true, QXmppStartTlsPacket::StartTls);
    ROW("proceed", R"(<proceed xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>)", true, QXmppStartTlsPacket::Proceed);
    ROW("failure", R"(<failure xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>)", true, QXmppStartTlsPacket::Failure);

    ROW("invalid-tag", R"(<invalid-tag-name xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>)", false, QXmppStartTlsPacket::StartTls);

#undef ROW
}

void tst_QXmppStream::testStartTlsPacket()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, valid);
    QFETCH(QXmppStartTlsPacket::Type, type);

    auto element = xmlToDom(xml);
    QCOMPARE(QXmppStartTlsPacket::isStartTlsPacket(element), valid);
    QCOMPARE(QXmppStartTlsPacket::isStartTlsPacket(element, type), valid);

    // test other types return false
    for (auto testValue : { QXmppStartTlsPacket::StartTls,
                            QXmppStartTlsPacket::Proceed,
                            QXmppStartTlsPacket::Failure }) {
        QCOMPARE(QXmppStartTlsPacket::isStartTlsPacket(element, testValue), testValue == type && valid);
    }

    if (valid) {
        QXmppStartTlsPacket packet;
        parsePacket(packet, xml);
        QCOMPARE(packet.type(), type);
        serializePacket(packet, xml);

        QXmppStartTlsPacket packet2(type);
        serializePacket(packet2, xml);

        QXmppStartTlsPacket packet3;
        packet3.setType(type);
        serializePacket(packet2, xml);
    }
}

void tst_QXmppStream::testNoResource()
{
    const QByteArray xml(
        "<iq id=\"bind_1\" type=\"set\">"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>"
        "</iq>");

    QXmppBindIq bind;
    parsePacket(bind, xml);
    QCOMPARE(bind.type(), QXmppIq::Set);
    QCOMPARE(bind.id(), u"bind_1"_s);
    QCOMPARE(bind.jid(), QString());
    QCOMPARE(bind.resource(), QString());
    serializePacket(bind, xml);
}

void tst_QXmppStream::testResource()
{
    const QByteArray xml(
        "<iq id=\"bind_2\" type=\"set\">"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
        "<resource>someresource</resource>"
        "</bind>"
        "</iq>");

    QXmppBindIq bind;
    parsePacket(bind, xml);
    QCOMPARE(bind.type(), QXmppIq::Set);
    QCOMPARE(bind.id(), u"bind_2"_s);
    QCOMPARE(bind.jid(), QString());
    QCOMPARE(bind.resource(), u"someresource"_s);
    serializePacket(bind, xml);
}

void tst_QXmppStream::testResult()
{
    const QByteArray xml(
        "<iq id=\"bind_2\" type=\"result\">"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
        "<jid>somenode@example.com/someresource</jid>"
        "</bind>"
        "</iq>");

    QXmppBindIq bind;
    parsePacket(bind, xml);
    QCOMPARE(bind.type(), QXmppIq::Result);
    QCOMPARE(bind.id(), u"bind_2"_s);
    QCOMPARE(bind.jid(), u"somenode@example.com/someresource"_s);
    QCOMPARE(bind.resource(), QString());
    serializePacket(bind, xml);
}
QT_WARNING_POP

void tst_QXmppStream::testSessionIq()
{
    const QByteArray xml(
        "<iq id=\"session_1\" to=\"example.com\" type=\"set\">"
        "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
        "</iq>");

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppSessionIq session;
    QT_WARNING_POP

    parsePacket(session, xml);
    serializePacket(session, xml);
}

QXMPP_TEST_MAIN(Stanza::tst_QXmppStanza, tst_QXmppStream)

#include "tst_QXmppDataCore.moc"

// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBindIq.h"
#include "QXmppConstants_p.h"
#include "QXmppLogger.h"

#include "Stream.h"
#include "StreamError.h"
#include "XmppSocket.h"
#include "compat/QXmppSessionIq.h"
#include "compat/QXmppStartTlsPacket.h"
#include "util.h"

using namespace QXmpp;
using namespace QXmpp::Private;

Q_DECLARE_METATYPE(QDomElement)

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

QTEST_MAIN(tst_QXmppStream)
#include "tst_QXmppStream.moc"

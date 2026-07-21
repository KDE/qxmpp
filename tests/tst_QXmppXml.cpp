// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary: merges tst_QXmppXmlElement, tst_QXmppXmlRegistry, tst_QXmppXmlFormatter into one translation
// unit so the shared Qt/QXmpp headers are parsed once instead of N times.
// main() runs each test class in turn via QTest::qExec().

#include "QXmppLogger.h"
#include "QXmppMessage.h"
#include "QXmppPresence.h"
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        tst_QXmppXmlElement tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppXmlRegistry tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppXmlFormatter tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "tst_QXmppXml.moc"

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@jahnson.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppLogger.h"
#include "QXmppXmlFormatter.h"

#include "util.h"

#include <QObject>
#include <QSignalSpy>

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
    // Sent: pretty-printed and nested under the log header.
    QCOMPARE(spy.at(0).at(1).toString(),
             u"  <iq>\n    <foo/>\n  </iq>"_s);
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

QTEST_MAIN(tst_QXmppXmlFormatter)
#include "tst_QXmppXmlFormatter.moc"

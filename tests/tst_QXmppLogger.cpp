// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppLogger.h"

#include "util.h"

#include <QSignalSpy>

class tst_QXmppLogger : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testFilterStreamManagementAcks();
    Q_SLOT void testStreamManagementAcksNotFilteredWhenDisabled();
    Q_SLOT void testNonAcksAlwaysLogged();
    Q_SLOT void testElidingDisabledByDefault();
    Q_SLOT void testElideLogMessagesAbove();
    Q_SLOT void testElideXmlTextAboveNeedsPrettyXml();
    Q_SLOT void testEnableEliding();
    Q_SLOT void testPrettyXmlEnablesEliding();
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

void tst_QXmppLogger::testElidingDisabledByDefault()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
    QVERIFY(!logger.elideXmlTextAbove());
    QVERIFY(!logger.elideLogMessagesAbove());

    QSignalSpy spy(&logger, &QXmppLogger::message);

    auto stanza = hugeStanza();
    logger.log(QXmppLogger::SentMessage, stanza);

    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(1).toString(), stanza);
}

void tst_QXmppLogger::testElideLogMessagesAbove()
{
    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::SignalLogging);
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
    QVERIFY(elided.startsWith(u"<iq type=\"result\"><data>AAA"));
    QVERIFY(elided.endsWith(u"AAA</data></iq>"));
    QVERIFY(elided.contains(u"characters elided"));

    QCOMPARE(spy.at(1).at(1).toString(), stanza);
    QCOMPARE(spy.at(2).at(1).toString(), u"<iq type=\"get\"/>"_s);
}

void tst_QXmppLogger::testElideXmlTextAboveNeedsPrettyXml()
{
    auto stanza = hugeStanza();

    // without pretty printing the XML text limit has no effect
    QXmppLogger plain;
    plain.setLoggingType(QXmppLogger::SignalLogging);
    plain.setElideXmlTextAbove(100);
    QCOMPARE(plain.elideXmlTextAbove().value(), qsizetype(100));

    QSignalSpy plainSpy(&plain, &QXmppLogger::message);
    plain.log(QXmppLogger::SentMessage, stanza);
    QCOMPARE(plainSpy.size(), 1);
    QCOMPARE(plainSpy.at(0).at(1).toString(), stanza);

    // with pretty printing the text node is elided, the XML structure is kept
    QXmppLogger pretty;
    pretty.setLoggingType(QXmppLogger::SignalLogging);
    pretty.setPrettyXml(true);
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

void tst_QXmppLogger::testPrettyXmlEnablesEliding()
{
    QXmppLogger logger;
    logger.enablePrettyXml();
    QVERIFY(logger.prettyXml());
    QCOMPARE(logger.elideXmlTextAbove().value(), QXmppLogger::DefaultElideXmlTextAbove);
    QCOMPARE(logger.elideLogMessagesAbove().value(), QXmppLogger::DefaultElideLogMessagesAbove);

    // pretty printing can be kept while logging full stanzas
    logger.enableEliding(false);
    QVERIFY(logger.prettyXml());
    QVERIFY(!logger.elideXmlTextAbove());
    QVERIFY(!logger.elideLogMessagesAbove());
}

QTEST_MAIN(tst_QXmppLogger)
#include "tst_QXmppLogger.moc"

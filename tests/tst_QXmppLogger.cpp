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

QTEST_MAIN(tst_QXmppLogger)
#include "tst_QXmppLogger.moc"

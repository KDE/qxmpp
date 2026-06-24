// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppSpamReport.h"

#include "util.h"

#include <QObject>

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

QTEST_MAIN(tst_QXmppSpamReport)
#include "tst_QXmppSpamReport.moc"

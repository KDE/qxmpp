// SPDX-FileCopyrightText: 2016 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2016 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the message-related managers. Merging the MAM,
// carbon, attention and message receipt manager tests into one translation
// unit parses the shared Qt/QXmpp headers once instead of once per file. Each
// original test keeps its own namespace; main() runs them in turn.

#include "QXmppArchiveIq.h"
#include "QXmppAttentionManager.h"
#include "QXmppCarbonManager.h"
#include "QXmppCarbonManagerV2.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMamManager.h"
#include "QXmppMessage.h"
#include "QXmppMessageHandler.h"
#include "QXmppMessageReceiptManager.h"
#include "QXmppRosterManager.h"
#include "QXmppUtils.h"

#include "Async.h"
#include "TestClient.h"
#include "util.h"

#include <QCoreApplication>
#include <QObject>

namespace Mam {

using namespace QXmpp::Private;

class EncryptionExtension : public QXmppE2eeExtension
{
public:
    QXmppTask<MessageEncryptResult> encryptMessage(QXmppMessage &&, const std::optional<QXmppSendStanzaParams> &) override
    {
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }
    QXmppTask<MessageDecryptResult> decryptMessage(QXmppMessage &&m) override
    {
        m.setBody(m.e2eeFallbackBody());
        m.setE2eeFallbackBody({});
        co_return m;
    }

    QXmppTask<IqEncryptResult> encryptIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> &) override
    {
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }

    QXmppTask<IqDecryptResult> decryptIq(const QDomElement &) override
    {
        co_return QXmppError { "it's only a test", QXmpp::SendError::EncryptionError };
    }

    bool isEncrypted(const QDomElement &e) override { return !e.firstChildElement("test-encrypted").isNull(); };
    bool isEncrypted(const QXmppMessage &) override { return false; };
};

class QXmppMamTestHelper : public QObject
{
    Q_OBJECT

public:
    Q_SLOT void archivedMessageReceived(const QString &queryId, const QXmppMessage &message);
    Q_SLOT void resultsRecieved(const QString &queryId, const QXmppResultSetReply &resultSetReply, bool complete);

    QXmppMessage m_expectedMessage;
    QXmppResultSetReply m_expectedResultSetReply;
    QString m_expectedQueryId;
    bool m_expectedComplete;
    bool m_signalTriggered;

    void compareMessages(const QXmppMessage &lhs, const QXmppMessage &rhs) const;
    void compareResultSetReplys(const QXmppResultSetReply &lhs, const QXmppResultSetReply &rhs) const;
};

class tst_QXmppMamManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testHandleStanza_data();
    Q_SLOT void testHandleStanza();

    Q_SLOT void testHandleResultIq_data();
    Q_SLOT void testHandleResultIq();

    // test for task-based API
    Q_SLOT void retrieveMessagesUnencrypted();
    Q_SLOT void retrieveMessagesEncrypted();

    QXmppMamTestHelper m_helper;
    QXmppMamManager m_manager;
};

void tst_QXmppMamManager::initTestCase()
{
    connect(&m_manager, &QXmppMamManager::archivedMessageReceived,
            &m_helper, &QXmppMamTestHelper::archivedMessageReceived);

    connect(&m_manager, &QXmppMamManager::resultsRecieved,
            &m_helper, &QXmppMamTestHelper::resultsRecieved);
}

void tst_QXmppMamManager::testHandleStanza_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accept");
    QTest::addColumn<QByteArray>("expectedMessage");
    QTest::addColumn<QString>("expectedQueryId");

    QTest::newRow("stanza1")
        << QByteArray("<message id='aeb213' to='juliet@capulet.lit/chamber'>"
                      "<result xmlns='urn:xmpp:mam:2' queryid='f27' id='28482-98726-73623'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
                      "<message xmlns='jabber:client'"
                      " to='juliet@capulet.lit/balcony'"
                      " from='romeo@montague.lit/orchard'"
                      " type='chat'>"
                      "<body>Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.</body>"
                      "</message>"
                      "</forwarded>"
                      "</result>"
                      "</message>")
        << true
        << QByteArray("<message xmlns='jabber:client'"
                      " to='juliet@capulet.lit/balcony'"
                      " from='romeo@montague.lit/orchard'"
                      " type='chat'>"
                      "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
                      "<body>Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.</body>"
                      "</message>")
        << u"f27"_s;

    QTest::newRow("stanza2")
        << QByteArray("<message id='aeb214' to='juliet@capulet.lit/chamber'>"
                      "<result queryid='f27' id='5d398-28273-f7382'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:09:32Z'/>"
                      "<message xmlns='jabber:client'"
                      " to='romeo@montague.lit/orchard'"
                      " from='juliet@capulet.lit/balcony'"
                      " type='chat' id='8a54s'>"
                      "<body>What man art thou that thus bescreen'd in night so stumblest on my counsel?</body>"
                      "</message>"
                      "</forwarded>"
                      "</result>"
                      "</message>")
        << false
        << QByteArray()
        << QString();

    QTest::newRow("stanza3")
        << QByteArray(
               "<message id='aeb214' xmlns='urn:xmpp:mam:2' to='juliet@capulet.lit/chamber'>"
               "<forwarded xmlns='urn:xmpp:forward:0'>"
               "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
               "<message xmlns='jabber:client'"
               " to='juliet@capulet.lit/balcony'"
               " from='romeo@montague.lit/orchard'"
               " type='chat'>"
               "<body>Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.</body>"
               "</message>"
               "</forwarded>"
               "</message>")
        << false
        << QByteArray()
        << QString();
}

void tst_QXmppMamManager::testHandleStanza()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accept);
    QFETCH(QByteArray, expectedMessage);
    QFETCH(QString, expectedQueryId);

    m_helper.m_signalTriggered = false;
    m_helper.m_expectedMessage = QXmppMessage();
    if (!expectedMessage.isEmpty()) {
        parsePacket(m_helper.m_expectedMessage, expectedMessage);
    }
    m_helper.m_expectedQueryId = expectedQueryId;

    bool accepted = m_manager.handleStanza(xmlToDom(xml));
    QCOMPARE(accepted, accept);
    QCOMPARE(m_helper.m_signalTriggered, accept);
}

void tst_QXmppMamManager::testHandleResultIq_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accept");
    QTest::addColumn<QByteArray>("expectedResultSetReply");
    QTest::addColumn<bool>("expectedComplete");

    QTest::newRow("stanza1")
        << QByteArray("<iq type='result' id='juliet1'>"
                      "<fin xmlns='urn:xmpp:mam:2'>"
                      "<set xmlns='http://jabber.org/protocol/rsm'>"
                      "<first index='0'>28482-98726-73623</first>"
                      "<last>09af3-cc343-b409f</last>"
                      "</set>"
                      "</fin>"
                      "</iq>")
        << true
        << QByteArray("<set xmlns='http://jabber.org/protocol/rsm'>"
                      "<first index='0'>28482-98726-73623</first>"
                      "<last>09af3-cc343-b409f</last>"
                      "</set>")
        << false;

    QTest::newRow("stanza2")
        << QByteArray("<iq type='result' id='juliet1'>"
                      "<fin xmlns='urn:xmpp:mam:2' complete='true'>"
                      "<set xmlns='http://jabber.org/protocol/rsm'>"
                      "<first index='0'>28482-98726-73623</first>"
                      "<last>09af3-cc343-b409f</last>"
                      "</set>"
                      "</fin>"
                      "</iq>")
        << true
        << QByteArray("<set xmlns='http://jabber.org/protocol/rsm'>"
                      "<first index='0'>28482-98726-73623</first>"
                      "<last>09af3-cc343-b409f</last>"
                      "</set>")
        << true;
}

void tst_QXmppMamManager::testHandleResultIq()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accept);
    QFETCH(QByteArray, expectedResultSetReply);
    QFETCH(bool, expectedComplete);

    m_helper.m_signalTriggered = false;
    m_helper.m_expectedResultSetReply = QXmppResultSetReply();
    if (!expectedResultSetReply.isEmpty()) {
        parsePacket(m_helper.m_expectedResultSetReply, expectedResultSetReply);
    }
    m_helper.m_expectedComplete = expectedComplete;

    bool accepted = m_manager.handleStanza(xmlToDom(xml));
    QCOMPARE(accepted, accept);
    QCOMPARE(m_helper.m_signalTriggered, accept);
}

void tst_QXmppMamManager::retrieveMessagesUnencrypted()
{
    TestClient test;
    auto *mam = test.addNewExtension<QXmppMamManager>();
    auto task = mam->retrieveMessages("mam.server.org");
    test.expect("<iq id='qx1' to='mam.server.org' type='set'>"
                "<query xmlns='urn:xmpp:mam:2' queryid='qx1'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'><value>urn:xmpp:mam:2</value></field>"
                "</x>"
                "</query>"
                "</iq>");
    mam->handleStanza(xmlToDom("<message id='aeb213' to='juliet@capulet.lit/chamber' from='mam.server.org'>"
                               "<result xmlns='urn:xmpp:mam:2' queryid='qx1' id='28482-98726-73623'>"
                               "<forwarded xmlns='urn:xmpp:forward:0'>"
                               "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
                               "<message xmlns='jabber:client'"
                               " to='juliet@capulet.lit/balcony'"
                               " from='romeo@montague.lit/orchard'"
                               " type='chat'>"
                               "<body>Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.</body>"
                               "</message>"
                               "</forwarded>"
                               "</result>"
                               "</message>"));
    mam->handleStanza(xmlToDom("<message id='aeb214' to='juliet@capulet.lit/chamber' from='mam.server.org'>"
                               "<result xmlns='urn:xmpp:mam:2' queryid='qx1' id='5d398-28273-f7382'>"
                               "<forwarded xmlns='urn:xmpp:forward:0'>"
                               "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:09:32Z'/>"
                               "<message xmlns='jabber:client'"
                               " to='romeo@montague.lit/orchard'"
                               " from='juliet@capulet.lit/balcony'"
                               " type='chat' id='8a54s'>"
                               "<body>What man art thou that thus bescreen'd in night so stumblest on my counsel?</body>"
                               "</message>"
                               "</forwarded>"
                               "</result>"
                               "</message>"));
    test.inject("<iq type='result' id='qx1'>"
                "<fin xmlns='urn:xmpp:mam:2'>"
                "<set xmlns='http://jabber.org/protocol/rsm'>"
                "<first index='0'>28482-98726-73623</first>"
                "<last>09af3-cc343-b409f</last>"
                "</set>"
                "</fin>"
                "</iq>");

    auto retrieved = expectFutureVariant<QXmppMamManager::RetrievedMessages>(task);
    QCOMPARE(retrieved.messages.size(), 2);
    QCOMPARE(retrieved.messages.at(0).body(), "Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.");
    QCOMPARE(retrieved.messages.at(1).body(), "What man art thou that thus bescreen'd in night so stumblest on my counsel?");
    QCOMPARE(retrieved.result.resultSetReply().first(), "28482-98726-73623");
}

void tst_QXmppMamManager::retrieveMessagesEncrypted()
{
    TestClient test;
    // e2ee
    auto e2ee = std::make_unique<EncryptionExtension>();
    test.setEncryptionExtension(e2ee.get());
    // mam manager
    auto *mam = test.addNewExtension<QXmppMamManager>();

    // start request
    auto task = mam->retrieveMessages("mam.server.org");
    test.expect("<iq id='qx1' to='mam.server.org' type='set'>"
                "<query xmlns='urn:xmpp:mam:2' queryid='qx1'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'><value>urn:xmpp:mam:2</value></field>"
                "</x>"
                "</query>"
                "</iq>");
    mam->handleStanza(xmlToDom("<message id='aeb213' to='juliet@capulet.lit/chamber' from='mam.server.org'>"
                               "<result xmlns='urn:xmpp:mam:2' queryid='qx1' id='28482-98726-73623'>"
                               "<forwarded xmlns='urn:xmpp:forward:0'>"
                               "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
                               "<message xmlns='jabber:client'"
                               " to='juliet@capulet.lit/balcony'"
                               " from='romeo@montague.lit/orchard'"
                               " type='chat'>"
                               "<test-encrypted/>"
                               "<body>Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.</body>"
                               "</message>"
                               "</forwarded>"
                               "</result>"
                               "</message>"));
    mam->handleStanza(xmlToDom("<message id='aeb214' to='juliet@capulet.lit/chamber' from='mam.server.org'>"
                               "<result xmlns='urn:xmpp:mam:2' queryid='qx1' id='5d398-28273-f7382'>"
                               "<forwarded xmlns='urn:xmpp:forward:0'>"
                               "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:09:32Z'/>"
                               "<message xmlns='jabber:client'"
                               " to='romeo@montague.lit/orchard'"
                               " from='juliet@capulet.lit/balcony'"
                               " type='chat' id='8a54s'>"
                               "<body>What man art thou that thus bescreen'd in night so stumblest on my counsel?</body>"
                               "</message>"
                               "</forwarded>"
                               "</result>"
                               "</message>"));
    test.inject("<iq type='result' id='qx1'>"
                "<fin xmlns='urn:xmpp:mam:2'>"
                "<set xmlns='http://jabber.org/protocol/rsm'>"
                "<first index='0'>28482-98726-73623</first>"
                "<last>09af3-cc343-b409f</last>"
                "</set>"
                "</fin>"
                "</iq>");

    // check results
    auto retrieved = expectFutureVariant<QXmppMamManager::RetrievedMessages>(task);
    QCOMPARE(retrieved.messages.size(), 2);
    QCOMPARE(retrieved.messages.at(0).body(), "Call me but love, and I'll be new baptized; Henceforth I never will be Romeo.");
    QCOMPARE(retrieved.messages.at(1).body(), "What man art thou that thus bescreen'd in night so stumblest on my counsel?");
    QCOMPARE(retrieved.result.resultSetReply().first(), "28482-98726-73623");
}

void QXmppMamTestHelper::archivedMessageReceived(const QString &queryId, const QXmppMessage &message)
{
    m_signalTriggered = true;

    compareMessages(message, m_expectedMessage);
    QCOMPARE(queryId, m_expectedQueryId);
}

void QXmppMamTestHelper::resultsRecieved(const QString &queryId, const QXmppResultSetReply &resultSetReply, bool complete)
{
    Q_UNUSED(queryId);
    m_signalTriggered = true;

    compareResultSetReplys(resultSetReply, m_expectedResultSetReply);
    QCOMPARE(complete, m_expectedComplete);
}

void QXmppMamTestHelper::compareMessages(const QXmppMessage &lhs, const QXmppMessage &rhs) const
{
    QCOMPARE(lhs.body(), rhs.body());
    QCOMPARE(lhs.from(), rhs.from());
    QCOMPARE(lhs.id(), rhs.id());
    QCOMPARE(lhs.isAttentionRequested(), rhs.isAttentionRequested());
    QCOMPARE(lhs.isMarkable(), rhs.isMarkable());
    QCOMPARE(lhs.isPrivate(), rhs.isPrivate());
    QCOMPARE(lhs.isReceiptRequested(), rhs.isReceiptRequested());
    QCOMPARE(lhs.lang(), rhs.lang());
    QCOMPARE(lhs.to(), rhs.to());
    QCOMPARE(lhs.thread(), rhs.thread());
    QCOMPARE(lhs.stamp(), rhs.stamp());
    QCOMPARE(lhs.type(), rhs.type());
}

void QXmppMamTestHelper::compareResultSetReplys(const QXmppResultSetReply &lhs, const QXmppResultSetReply &rhs) const
{
    QCOMPARE(lhs.first(), rhs.first());
    QCOMPARE(lhs.last(), rhs.last());
    QCOMPARE(lhs.count(), rhs.count());
    QCOMPARE(lhs.index(), rhs.index());
    QCOMPARE(lhs.isNull(), rhs.isNull());
}

}  // namespace Mam

// ============================================================

namespace Carbon {

void compareMessages(const QXmppMessage &lhs, const QXmppMessage &rhs)
{
    QCOMPARE(lhs.body(), rhs.body());
    QCOMPARE(lhs.from(), rhs.from());
    QCOMPARE(lhs.id(), rhs.id());
    QCOMPARE(lhs.isAttentionRequested(), rhs.isAttentionRequested());
    QCOMPARE(lhs.isMarkable(), rhs.isMarkable());
    QCOMPARE(lhs.isPrivate(), rhs.isPrivate());
    QCOMPARE(lhs.isReceiptRequested(), rhs.isReceiptRequested());
    QCOMPARE(lhs.lang(), rhs.lang());
    QCOMPARE(lhs.to(), rhs.to());
    QCOMPARE(lhs.thread(), rhs.thread());
    QCOMPARE(lhs.stamp(), rhs.stamp());
    QCOMPARE(lhs.type(), rhs.type());
    QCOMPARE(lhs.isCarbonForwarded(), rhs.isCarbonForwarded());
}

class QXmppCarbonTestHelper : public QObject
{
    Q_OBJECT

public:
    Q_SLOT void messageSent(const QXmppMessage &msg)
    {
        m_signalTriggered = true;
        QCOMPARE(m_expectSent, true);

        compareMessages(m_expectedMessage, msg);
    }
    Q_SLOT void messageReceived(const QXmppMessage &msg)
    {
        m_signalTriggered = true;
        QCOMPARE(m_expectSent, false);

        compareMessages(m_expectedMessage, msg);
    }

    QXmppMessage m_expectedMessage;
    bool m_expectSent;
    bool m_signalTriggered;
};

class MessageHandler : public QXmppClientExtension, public QXmppMessageHandler
{
public:
    bool handleMessage(const QXmppMessage &msg) override
    {
        received.push_back(msg);
        return false;
    }

    std::vector<QXmppMessage> received;
};

class tst_QXmppCarbonManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testHandleStanza_data();
    Q_SLOT void testHandleStanza();

    QXmppCarbonTestHelper m_helper;
    MessageHandler *m_messageHandler;
    QXmppCarbonManager *m_managerV1;
    QXmppCarbonManagerV2 *m_managerV2;
    QXmppClient client;
};

void tst_QXmppCarbonManager::initTestCase()
{
    client.configuration().setJid("romeo@montague.example");
    m_managerV1 = client.addNewExtension<QXmppCarbonManager>();
    m_managerV2 = client.addNewExtension<QXmppCarbonManagerV2>();
    m_messageHandler = client.addNewExtension<MessageHandler>();

    connect(m_managerV1, &QXmppCarbonManager::messageSent,
            &m_helper, &QXmppCarbonTestHelper::messageSent);

    connect(m_managerV1, &QXmppCarbonManager::messageReceived,
            &m_helper, &QXmppCarbonTestHelper::messageReceived);
}

void tst_QXmppCarbonManager::testHandleStanza_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("sent");
    QTest::addColumn<QByteArray>("forwardedxml");

    QTest::newRow("received1")
        << QByteArray("<message xmlns='jabber:client'"
                      " from='romeo@montague.example'"
                      " to='romeo@montague.example/home'"
                      " type='chat'>"
                      "<received xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client'"
                      " from='juliet@capulet.example/balcony'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</received>"
                      "</message>")
        << true << false
        << QByteArray("<message xmlns='jabber:client'"
                      " from='juliet@capulet.example/balcony'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>");

    QTest::newRow("sent1")
        << QByteArray("<message xmlns='jabber:client'"
                      " from='romeo@montague.example'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<sent xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client'"
                      " to='juliet@capulet.example/balcony'"
                      " from='romeo@montague.example/home'"
                      " type='chat'>"
                      "<body>Neither, fair saint, if either thee dislike.</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</sent>"
                      "</message>")
        << true << true
        << QByteArray("<message xmlns='jabber:client'"
                      " to='juliet@capulet.example/balcony'"
                      " from='romeo@montague.example/home'"
                      " type='chat'>"
                      "<body>Neither, fair saint, if either thee dislike.</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>");

    QTest::newRow("received-wrong-from")
        << QByteArray("<message xmlns='jabber:client'"
                      " from='not-romeo@montague.example'"
                      " to='romeo@montague.example/home'"
                      " type='chat'>"
                      "<received xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client'"
                      " from='juliet@capulet.example/balcony'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</received>"
                      "</message>")
        << false << false
        << QByteArray("<message xmlns='jabber:client'"
                      " from='juliet@capulet.example/balcony'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>");

    QTest::newRow("sent-wrong-from")
        << QByteArray("<message xmlns='jabber:client'"
                      " from='not-romeo@montague.example'"
                      " to='romeo@montague.example/garden'"
                      " type='chat'>"
                      "<sent xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client'"
                      " to='juliet@capulet.example/balcony'"
                      " from='romeo@montague.example/home'"
                      " type='chat'>"
                      "<body>Neither, fair saint, if either thee dislike.</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</sent>"
                      "</message>")
        << false << true
        << QByteArray("<message xmlns='jabber:client'"
                      " to='juliet@capulet.example/balcony'"
                      " from='romeo@montague.example/home'"
                      " type='chat'>"
                      "<body>Neither, fair saint, if either thee dislike.</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>");

    QTest::newRow("forwarded_normal")
        << QByteArray("<message to='mercutio@verona.lit' from='romeo@montague.lit/orchard' type='chat' id='28gs'>"
                      "<body>A most courteous exposition!</body>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<delay xmlns='urn:xmpp:delay' stamp='2010-07-10T23:08:25Z'/>"
                      "<message from='juliet@capulet.lit/orchard'"
                      " id='0202197'"
                      " to='romeo@montague.lit'"
                      " type='chat'"
                      " xmlns='jabber:client'>"
                      "<body>Yet I should kill thee with much cherishing.</body>"
                      "<mood xmlns='http://jabber.org/protocol/mood'>"
                      "<amorous/>"
                      "</mood>"
                      "</message>"
                      "</forwarded>"
                      "</message>")
        << false << false
        << QByteArray();
}

void tst_QXmppCarbonManager::testHandleStanza()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accept);
    QFETCH(bool, sent);
    QFETCH(QByteArray, forwardedxml);

    QXmppMessage expectedMessage;
    if (!forwardedxml.isEmpty()) {
        parsePacket(expectedMessage, forwardedxml);
    }
    expectedMessage.setCarbonForwarded(true);

    {
        m_helper.m_expectedMessage = expectedMessage;
        m_helper.m_expectSent = sent;
        m_helper.m_signalTriggered = false;

        bool accepted = m_managerV1->handleStanza(xmlToDom(xml));

        QCOMPARE(accepted, accept);
        QCOMPARE(m_helper.m_signalTriggered, accept);
    }
    {
        m_messageHandler->received.clear();

        bool accepted = m_managerV2->handleStanza(xmlToDom(xml), {});
        QCOMPARE(accepted, accept);

        if (accept) {
            QCOMPARE(m_messageHandler->received.size(), size_t(1));
            compareMessages(m_messageHandler->received[0], expectedMessage);
        } else {
            QCOMPARE(m_messageHandler->received.size(), size_t(0));
        }
    }
}

}  // namespace Carbon

// ============================================================

namespace Attention {

class tst_QXmppAttentionManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testDiscoFeatures();
    Q_SLOT void testReceived_data();
    Q_SLOT void testReceived();
    Q_SLOT void testRateLimiting();
    Q_SLOT void testSendRequest();

    void setOwnJid(const QString &jid);
    void addToRoster(const QString &jid);

    QXmppClient client;
    QXmppLogger logger;
    QXmppAttentionManager *manager;
};

void tst_QXmppAttentionManager::initTestCase()
{
    logger.setLoggingType(QXmppLogger::SignalLogging);
    client.setLogger(&logger);

    manager = new QXmppAttentionManager();
    client.addExtension(manager);
}

void tst_QXmppAttentionManager::testDiscoFeatures()
{
    QCOMPARE(manager->discoveryFeatures(), QStringList() << "urn:xmpp:attention:0");
}

void tst_QXmppAttentionManager::testReceived_data()
{
    QTest::addColumn<QXmppMessage>("msg");
    QTest::addColumn<bool>("accepted");
    QTest::addColumn<bool>("rateLimited");

    auto createMessage = [](const QString &from, bool attention, const QDateTime &stamp = {}) -> QXmppMessage {
        QXmppMessage msg;
        msg.setBody("Moin moin");
        msg.setFrom(from);
        msg.setAttentionRequested(attention);
        msg.setStamp(stamp);
        return msg;
    };

    QTest::newRow("basic")
        << createMessage("other-user@qxmpp.org/Qlient", true)
        << true;
    QTest::newRow("no-attention-requested")
        << createMessage("other-user@qxmpp.org/Qlient", false)
        << false;
    QTest::newRow("with-stamp")
        << createMessage("other-user@qxmpp.org/Qlient", true, QDateTime::currentDateTimeUtc())
        << false;
    QTest::newRow("own-account")
        << createMessage("me@qxmpp.org/Klient", true)
        << false;
    QTest::newRow("trusted")
        << createMessage("other-user@qxmpp.org/Klient", true)
        << true;
}

void tst_QXmppAttentionManager::testReceived()
{
    QFETCH(QXmppMessage, msg);
    QFETCH(bool, accepted);

    QObject context;
    setOwnJid("me@qxmpp.org");
    addToRoster("contact@qxmpp.org");
    bool signalCalled = false;
    bool limitedCalled = false;

    connect(manager, &QXmppAttentionManager::attentionRequested, &context, [&](const QXmppMessage &receivedMsg, bool isTrusted) {
        signalCalled = true;
        QCOMPARE(isTrusted, QXmppUtils::jidToBareJid(receivedMsg.from()) == u"contact@qxmpp.org");
        QCOMPARE(receivedMsg.body(), msg.body());
    });

    connect(manager, &QXmppAttentionManager::attentionRequestRateLimited, &context, [&](const QXmppMessage &) {
        limitedCalled = true;
    });

    Q_EMIT client.messageReceived(msg);

    QCOMPARE(signalCalled, accepted);
    QVERIFY(!limitedCalled);
}

void tst_QXmppAttentionManager::testRateLimiting()
{
    int count = 1e3;
    int allowed = 3;

    client.removeExtension(manager);
    manager = new QXmppAttentionManager(allowed, QTime(0, 0, 1));
    client.addExtension(manager);

    QObject context;
    setOwnJid("me@qxmpp.org");

    int signalCalled = 0;
    int rateLimitedCalled = 0;

    connect(manager, &QXmppAttentionManager::attentionRequested, &context, [&](const QXmppMessage &, bool) {
        signalCalled++;
    });

    connect(manager, &QXmppAttentionManager::attentionRequestRateLimited, &context, [&](const QXmppMessage &) {
        rateLimitedCalled++;
    });

    QXmppMessage msg;
    msg.setAttentionRequested(true);

    for (int i = 0; i < count; i++) {
        Q_EMIT client.messageReceived(msg);
    }

    QCOMPARE(signalCalled, allowed);
    QCOMPARE(rateLimitedCalled, count - allowed);

    // wait 1 s + 50 ms because of QTimer precision problems
    QThread::currentThread()->usleep(1000e3 + 50e3);
    QCoreApplication::processEvents();

    for (int i = 0; i < count; i++) {
        Q_EMIT client.messageReceived(msg);
    }

    QCOMPARE(signalCalled, allowed * 2);
    QCOMPARE(rateLimitedCalled, (count - allowed) * 2);
}

void tst_QXmppAttentionManager::testSendRequest()
{
    QObject context;

    bool signalCalled = false;
    connect(&logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &message) {
        if (type == QXmppLogger::SentMessage) {
            signalCalled = true;

            QXmppMessage msg;
            parsePacket(msg, message.toUtf8());
            QCOMPARE(msg.type(), QXmppMessage::Chat);
            QCOMPARE(msg.id().size(), 36);
            QCOMPARE(msg.originId().size(), 36);
            QCOMPARE(msg.to(), u"account@qxmpp.org"_s);
            QCOMPARE(msg.body(), u"Hello"_s);
            QVERIFY(msg.isAttentionRequested());
        }
    });

    // the client is offline, so the message can't be sent and no id is returned
    QVERIFY(manager->requestAttention("account@qxmpp.org", "Hello").isEmpty());
    QVERIFY(signalCalled);
}

void tst_QXmppAttentionManager::setOwnJid(const QString &jid)
{
    client.connectToServer(jid, {});
    client.disconnectFromServer();
}

void tst_QXmppAttentionManager::addToRoster(const QString &jid)
{
    auto *rosterManager = client.findExtension<QXmppRosterManager>();

    QXmppRosterIq::Item newItem;
    newItem.setBareJid(jid);
    newItem.setSubscriptionType(QXmppRosterIq::Item::Both);

    QXmppRosterIq iq;
    iq.setFrom("qxmpp.org");
    iq.setType(QXmppIq::Set);
    iq.addItem(newItem);

    rosterManager->handleStanza(writePacketToDom(iq));
}

}  // namespace Attention

// ============================================================

namespace MessageReceipt {

class tst_QXmppMessageReceiptManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testReceipt_data();
    Q_SLOT void testReceipt();

    void handleMessageDelivered(const QString &, const QString &)
    {
        m_messageDelivered = true;
    }
    void onLoggerMessage(QXmppLogger::MessageType, const QString &)
    {
        m_receiptSent = true;
    }

    QXmppMessageReceiptManager *m_manager;
    QXmppClient m_client;
    QXmppLogger m_logger;
    bool m_messageDelivered = false;
    bool m_receiptSent = false;
};

void tst_QXmppMessageReceiptManager::initTestCase()
{
    m_manager = new QXmppMessageReceiptManager();

    m_client.addExtension(m_manager);
    m_logger.setLoggingType(QXmppLogger::SignalLogging);
    m_client.setLogger(&m_logger);

    connect(&m_logger, &QXmppLogger::message,
            this, &tst_QXmppMessageReceiptManager::onLoggerMessage);

    connect(m_manager, &QXmppMessageReceiptManager::messageDelivered,
            this, &tst_QXmppMessageReceiptManager::handleMessageDelivered);
}

void tst_QXmppMessageReceiptManager::testReceipt_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("sent");
    QTest::addColumn<bool>("handled");

    QTest::newRow("correct")
        << QByteArray(
               "<message id=\"bi29sg183b4v\" "
               "to=\"northumberland@shakespeare.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"normal\">"
               "<received xmlns=\"urn:xmpp:receipts\" id=\"richard2-4.1.247\"/>"
               "</message>")
        << true
        << false
        << true;
    QTest::newRow("from-to-equal")
        << QByteArray(
               "<message id=\"bi29sg183b4v\" "
               "to=\"kingrichard@royalty.england.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"normal\">"
               "<received xmlns=\"urn:xmpp:receipts\" id=\"richard2-4.1.247\"/>"
               "</message>")
        << false
        << false
        << true;
    QTest::newRow("error-request")
        << QByteArray(
               "<message xml:lang=\"en\" "
               "to=\"northumberland@shakespeare.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"error\" id=\"bi29sg183b4v\" "
               "> "
               "<archived xmlns=\"urn:xmpp:mam:tmp\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<stanza-id xmlns=\"urn:xmpp:sid:0\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<delay xmlns=\"urn:xmpp:delay\" stamp=\"2020-03-26T20:30:41.678Z\"/> "
               "<request xmlns=\"urn:xmpp:receipts\"/> "
               "<error code=\"500\" type=\"wait\"> "
               "<resource-constraint xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/> "
               "<text xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\" xml:lang=\"en\">"
               "Your contact offline message queue is full. The message has been discarded."
               "</text>"
               "</error>"
               "<body>1</body> "
               "</message>")
        << false
        << false
        << false;
    QTest::newRow("error-receipt")
        << QByteArray(
               "<message xml:lang=\"en\" "
               "to=\"northumberland@shakespeare.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"error\" id=\"bi29sg183b4v\" "
               "> "
               "<received xmlns=\"urn:xmpp:receipts\" id=\"richard2-4.1.247\"/>"
               "<error code=\"500\" type=\"wait\"> "
               "<resource-constraint xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/> "
               "<text xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\" xml:lang=\"en\">"
               "Your contact offline message queue is full. The message has been discarded."
               "</text>"
               "</error>"
               "<body>1</body> "
               "</message>")
        << false
        << false
        << false;
    QTest::newRow("message with receipt request")
        << QByteArray(
               "<message xml:lang=\"en\" "
               "to=\"northumberland@shakespeare.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"chat\" id=\"bi29sg183b4v\" "
               "> "
               "<archived xmlns=\"urn:xmpp:mam:tmp\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<stanza-id xmlns=\"urn:xmpp:sid:0\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<request xmlns=\"urn:xmpp:receipts\"/> "
               "<body>1</body> "
               "</message>")
        << false
        << true
        << false;

    QTest::newRow("message with no receipt request")
        << QByteArray(
               "<message xml:lang=\"en\" "
               "to=\"northumberland@shakespeare.lit/westminster\" "
               "from=\"kingrichard@royalty.england.lit/throne\" "
               "type=\"chat\" id=\"bi29sg183b4v\" "
               "> "
               "<archived xmlns=\"urn:xmpp:mam:tmp\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<stanza-id xmlns=\"urn:xmpp:sid:0\" by=\"kingrichard@royalty.england.lit\" id=\"1585254642941569\"/> "
               "<body>1</body> "
               "</message>")
        << false
        << false
        << false;
}

void tst_QXmppMessageReceiptManager::testReceipt()
{
    m_messageDelivered = false;
    m_receiptSent = false;

    QFETCH(QByteArray, xml);
    QFETCH(bool, accept);
    QFETCH(bool, sent);
    QFETCH(bool, handled);

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    QCOMPARE(m_manager->handleMessage(msg), handled);
    QCOMPARE(m_messageDelivered, accept);
    QCOMPARE(m_receiptSent, sent);
}

}  // namespace MessageReceipt

// ============================================================

namespace ArchiveIq {

class tst_QXmppArchiveIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testArchiveList_data();
    Q_SLOT void testArchiveList();
    Q_SLOT void testArchiveChat_data();
    Q_SLOT void testArchiveChat();
    Q_SLOT void testArchiveRemove();
    Q_SLOT void testArchiveRetrieve_data();
    Q_SLOT void testArchiveRetrieve();
};

void tst_QXmppArchiveIq::testArchiveList_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("max");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"list_1\" type=\"get\">"
                                   "<list xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\"/>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"list_1\" type=\"get\">"
                                     "<list xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\">"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<max>30</max>"
                                     "</set>"
                                     "</list>"
                                     "</iq>")
                              << 30;
}

void tst_QXmppArchiveIq::testArchiveList()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, max);

    QXmppArchiveListIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.id(), u"list_1");
    QCOMPARE(iq.with(), u"juliet@capulet.com");
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.end(), QDateTime(QDate(1479, 7, 21), QTime(4, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.resultSetQuery().max(), max);
    serializePacket(iq, xml);
}

void tst_QXmppArchiveIq::testArchiveChat_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("count");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"chat_1\" type=\"result\">"
                                   "<chat xmlns=\"urn:xmpp:archive\""
                                   " with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:56:15Z\""
                                   " subject=\"She speaks!\""
                                   " version=\"4\""
                                   ">"
                                   "<from secs=\"0\"><body>Art thou not Romeo, and a Montague?</body></from>"
                                   "<to secs=\"11\"><body>Neither, fair saint, if either thee dislike.</body></to>"
                                   "<from secs=\"7\"><body>How cam&apos;st thou hither, tell me, and wherefore?</body></from>"
                                   "</chat>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"chat_1\" type=\"result\">"
                                     "<chat xmlns=\"urn:xmpp:archive\""
                                     " with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:56:15Z\""
                                     " subject=\"She speaks!\""
                                     " version=\"4\""
                                     ">"
                                     "<from secs=\"0\"><body>Art thou not Romeo, and a Montague?</body></from>"
                                     "<to secs=\"11\"><body>Neither, fair saint, if either thee dislike.</body></to>"
                                     "<from secs=\"7\"><body>How cam&apos;st thou hither, tell me, and wherefore?</body></from>"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<count>3</count>"
                                     "</set>"
                                     "</chat>"
                                     "</iq>")
                              << 3;
}

void tst_QXmppArchiveIq::testArchiveChat()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, count);

    QXmppArchiveChatIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.id(), QLatin1String("chat_1"));
    QCOMPARE(iq.chat().with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.chat().messages().size(), 3);
    QCOMPARE(iq.chat().messages()[0].isReceived(), true);
    QCOMPARE(iq.chat().messages()[0].body(), QLatin1String("Art thou not Romeo, and a Montague?"));
    QCOMPARE(iq.chat().messages()[0].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 15), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[1].isReceived(), false);
    QCOMPARE(iq.chat().messages()[1].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 26), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[1].body(), QLatin1String("Neither, fair saint, if either thee dislike."));
    QCOMPARE(iq.chat().messages()[2].isReceived(), true);
    QCOMPARE(iq.chat().messages()[2].date(), QDateTime(QDate(1469, 7, 21), QTime(2, 56, 33), TimeZoneUTC));
    QCOMPARE(iq.chat().messages()[2].body(), QLatin1String("How cam'st thou hither, tell me, and wherefore?"));
    QCOMPARE(iq.resultSetReply().count(), count);
    serializePacket(iq, xml);
}

void tst_QXmppArchiveIq::testArchiveRemove()
{
    const QByteArray xml(
        "<iq id=\"remove_1\" type=\"set\">"
        "<remove xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
        " start=\"1469-07-21T02:00:00Z\" end=\"1479-07-21T04:00:00Z\"/>"
        "</iq>");

    QXmppArchiveRemoveIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.id(), QLatin1String("remove_1"));
    QCOMPARE(iq.with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.end(), QDateTime(QDate(1479, 7, 21), QTime(4, 0, 0), TimeZoneUTC));
    serializePacket(iq, xml);
}

void tst_QXmppArchiveIq::testArchiveRetrieve_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("max");

    QTest::newRow("no rsm") << QByteArray(
                                   "<iq id=\"retrieve_1\" type=\"get\">"
                                   "<retrieve xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                   " start=\"1469-07-21T02:00:00Z\"/>"
                                   "</iq>")
                            << -1;

    QTest::newRow("with rsm") << QByteArray(
                                     "<iq id=\"retrieve_1\" type=\"get\">"
                                     "<retrieve xmlns=\"urn:xmpp:archive\" with=\"juliet@capulet.com\""
                                     " start=\"1469-07-21T02:00:00Z\">"
                                     "<set xmlns=\"http://jabber.org/protocol/rsm\">"
                                     "<max>30</max>"
                                     "</set>"
                                     "</retrieve>"
                                     "</iq>")
                              << 30;
}

void tst_QXmppArchiveIq::testArchiveRetrieve()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, max);

    QXmppArchiveRetrieveIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.id(), QLatin1String("retrieve_1"));
    QCOMPARE(iq.with(), QLatin1String("juliet@capulet.com"));
    QCOMPARE(iq.start(), QDateTime(QDate(1469, 7, 21), QTime(2, 0, 0), TimeZoneUTC));
    QCOMPARE(iq.resultSetQuery().max(), max);
    serializePacket(iq, xml);
}

}  // namespace ArchiveIq

QXMPP_TEST_MAIN(Mam::tst_QXmppMamManager, Carbon::tst_QXmppCarbonManager, Attention::tst_QXmppAttentionManager, MessageReceipt::tst_QXmppMessageReceiptManager, ArchiveIq::tst_QXmppArchiveIq)

#include "ManagersMessages.moc"

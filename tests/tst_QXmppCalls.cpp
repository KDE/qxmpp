// SPDX-FileCopyrightText: 2015 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2023 Tibor Csötönyi <work@taibsu.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the managers involved in setting up calls. Merging
// the Jingle message initiation, call invite, call and external service
// discovery manager tests into one translation unit parses the shared Qt/QXmpp
// headers once instead of once per file. Each original test keeps its own
// namespace; main() runs them in turn.
//
// The call manager only exists with GStreamer support, so its tests are
// guarded by WITH_GSTREAMER.

#include "QXmppCallInviteManager.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppExternalServiceDiscoveryIq.h"
#include "QXmppExternalServiceDiscoveryManager.h"
#include "QXmppJingleMessageInitiationManager.h"
#include "QXmppMessage.h"
#include "QXmppServer.h"
#include "QXmppUtils.h"

#include "IntegrationTesting.h"
#include "TestClient.h"
#include "TestPasswordChecker.h"
#include "util.h"

#ifdef WITH_GSTREAMER
#include "QXmppCall.h"
#include "QXmppCallManager.h"

#include <gst/gst.h>
#endif

#include <QBuffer>
#include <QCoreApplication>
#include <QObject>
#include <QTest>
#include <QTimer>

using Jmi = QXmppJingleMessageInitiation;
using JmiType = QXmppJingleMessageInitiationElement::Type;
using JmiResult = QXmppJingleMessageInitiation::Result;

constexpr QStringView ns_jingle_rtp = u"urn:xmpp:jingle:apps:rtp:1";

class tst_QXmppJingleMessageInitiationManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testClear();
    Q_SLOT void testClearAll();

    Q_SLOT void proposeBase();
    Q_SLOT void testRing();
    Q_SLOT void testProceed();
    Q_SLOT void testReject();
    Q_SLOT void testRetract();
    Q_SLOT void testFinish();

    Q_SLOT void testPropose();
    Q_SLOT void testSendMessage();
    Q_SLOT void testHandleNonExistingSessionLowerId();
    Q_SLOT void testHandleNonExistingSessionHigherId();
    Q_SLOT void testHandleExistingSession();
    Q_SLOT void testHandleTieBreak();
    Q_SLOT void testHandleProposeJmiElement();
    Q_SLOT void testHandleExistingJmi();
    Q_SLOT void testHandleJmiElement();
    Q_SLOT void testHandleMessage_data();
    Q_SLOT void testHandleMessage();
    Q_SLOT void testHandleMessageRinging();
    Q_SLOT void testHandleMessageProceeded();
    Q_SLOT void testHandleMessageClosedRejected();
    Q_SLOT void testHandleMessageClosedRetracted();
    Q_SLOT void testHandleMessageClosedFinished();

    QXmppClient m_client;
    QXmppLogger m_logger;
    QXmppJingleMessageInitiationManager m_manager;
};

void tst_QXmppJingleMessageInitiationManager::initTestCase()
{
    m_client.addExtension(&m_manager);

    m_logger.setLoggingType(QXmppLogger::SignalLogging);
    m_client.setLogger(&m_logger);

    m_client.connectToServer(IntegrationTests::clientConfiguration());

    qRegisterMetaType<QXmppJingleMessageInitiation::Result>();
}

void tst_QXmppJingleMessageInitiationManager::testClear()
{
    QCOMPARE(m_manager.jmis().size(), 0);
    auto jmi1 { m_manager.addJmi("test1", "j@id.org") };
    auto jmi2 { m_manager.addJmi("test2", "j@id.org") };
    QCOMPARE(m_manager.jmis().size(), 2);

    m_manager.clear(jmi1);
    m_manager.clear(jmi2);
    QCOMPARE(m_manager.jmis().size(), 0);
}

void tst_QXmppJingleMessageInitiationManager::testClearAll()
{
    QCOMPARE(m_manager.jmis().size(), 0);
    m_manager.addJmi("test1", "j@id.org");
    m_manager.addJmi("test2", "j@id.org");
    m_manager.addJmi("test3", "j@id.org");
    m_manager.addJmi("test4", "j@id.org");
    m_manager.addJmi("test5", "j@id.org");
    QCOMPARE(m_manager.jmis().size(), 5);

    m_manager.clearAll();
    QCOMPARE(m_manager.jmis().size(), 0);
}

void tst_QXmppJingleMessageInitiationManager::proposeBase()
{
    TestClient test;
    auto *jmiM = test.addNewExtension<QXmppJingleMessageInitiationManager>();

    auto result = jmiM->propose(u"id-test1"_s, u"a@example.com"_s, { { u"audio"_s }, { u"video"_s } });

    test.expect(
        u"<message to=\"a@example.com\" type=\"chat\">"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "<propose xmlns=\"urn:xmpp:jingle-message:0\" id=\"id-test1\">"
        "<description xmlns=\"urn:xmpp:jingle:apps:rtp:1\" media=\"audio\"/>"
        "<description xmlns=\"urn:xmpp:jingle:apps:rtp:1\" media=\"video\"/>"
        "</propose>"
        "</message>"_s);
}

void tst_QXmppJingleMessageInitiationManager::testRing()
{
    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietRing@capulet.example");

    connect(&m_logger, &QXmppLogger::message, this, [jmicallPartnerJid = jmi->remoteJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmicallPartnerJid) {
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), JmiType::Ringing);
            }
        }
    });

    auto future = jmi->ring();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testProceed()
{
    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietProceed@capulet.example");

    connect(&m_logger, &QXmppLogger::message, this, [jmiCallPartnerJid = jmi->remoteJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmiCallPartnerJid) {
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), JmiType::Proceed);
            }
        }
    });

    auto future = jmi->proceed();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testReject()
{
    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietReject@capulet.example");

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Decline);
    reason.setText("Declined");

    connect(&m_logger, &QXmppLogger::message, this, [jmiCallPartnerJid = jmi->remoteJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmiCallPartnerJid) {
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), JmiType::Reject);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->type(), QXmppJingleReason::Decline);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->text(), "Declined");
                QCOMPARE(message.jingleMessageInitiationElement()->containsTieBreak(), true);
            }
        }
    });

    auto future = jmi->reject(reason, true);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testRetract()
{
    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietRetract@capulet.example");

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Gone);
    reason.setText("Gone");

    connect(&m_logger, &QXmppLogger::message, this, [jmicallPartnerJid = jmi->remoteJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmicallPartnerJid) {
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), JmiType::Retract);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->type(), QXmppJingleReason::Gone);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->text(), "Gone");
                QCOMPARE(message.jingleMessageInitiationElement()->containsTieBreak(), true);
            }
        }
    });

    auto future = jmi->retract(reason, true);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testFinish()
{
    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietFinish@capulet.example");

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Success);
    reason.setText("Finished");

    connect(&m_logger, &QXmppLogger::message, this, [jmicallPartnerJid = jmi->remoteJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmicallPartnerJid) {
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), JmiType::Finish);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->type(), QXmppJingleReason::Success);
                QCOMPARE(message.jingleMessageInitiationElement()->reason()->text(), "Finished");
                QCOMPARE(message.jingleMessageInitiationElement()->migratedTo(), "fecbea35-08d3-404f-9ec7-2b57c566fa74");
            }
        }
    });

    auto future = jmi->finish(reason, "fecbea35-08d3-404f-9ec7-2b57c566fa74");

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testPropose()
{
    QString jid { "julietPropose@capulet.example" };

    QXmppJingleRtpDescription description;
    description.setMedia(u"audio"_s);
    description.setSsrc(123);

    connect(&m_logger, &QXmppLogger::message, this, [&, jid, description](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jid) {
                const auto &jmiElement { message.jingleMessageInitiationElement() };
                QVERIFY(jmiElement);

                QCOMPARE(jmiElement->type(), JmiType::Propose);
                QVERIFY(!jmiElement->id().isEmpty());
                QVERIFY(jmiElement->description());
                QCOMPARE(jmiElement->description()->media(), description.media());
                QCOMPARE(jmiElement->description()->ssrc(), description.ssrc());
            }
        }
    });

    auto future = m_manager.propose(jid, description);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testSendMessage()
{
    QString jid { "julietSendMessage@capulet.example" };

    QXmppJingleMessageInitiationElement jmiElement;
    jmiElement.setType(JmiType::Propose);
    jmiElement.setId(u"fecbea35-08d3-404f-9ec7-2b57c566fa74"_s);

    connect(&m_logger, &QXmppLogger::message, this, [jid, jmiElement](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jid) {
                QVERIFY(message.hasHint(QXmppMessage::Store));
                QVERIFY(message.jingleMessageInitiationElement());
                QCOMPARE(message.jingleMessageInitiationElement()->type(), jmiElement.type());
            }
        }
    });

    auto future = m_manager.sendMessage(jmiElement, jid);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleNonExistingSessionLowerId()
{
    // --- request with lower id sends propose to request with higher id ---

    QByteArray xmlProposeLowId {
        "<message from='romeoNonExistingSession@montague.example/low' to='juliet@capulet.example' type='chat'>"
        "<propose xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
        "</propose>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmiWithHigherId = m_manager.addJmi("fecbea35-08d3-404f-9ec7-2b57c566fa74", "romeoNonExistingSession@montague.example");

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Expired);
    reason.setText("Tie-Break");

    // make sure that request with higher ID is being retracted
    connect(&m_logger, &QXmppLogger::message, this, [jmiWithHigherId, reason](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmiWithHigherId->remoteJid()) {
                const auto &jmiElement { message.jingleMessageInitiationElement() };
                QVERIFY(jmiElement);

                QCOMPARE(jmiElement->type(), JmiType::Retract);
                QCOMPARE(jmiElement->id(), "fecbea35-08d3-404f-9ec7-2b57c566fa74");
                QVERIFY(jmiElement->reason());
                QCOMPARE(jmiElement->reason()->type(), reason.type());
                QCOMPARE(jmiElement->reason()->text(), reason.text());

                SKIP_IF_INTEGRATION_TESTS_DISABLED()

                // verify that the JMI ID has been changed and the JMI was processed
                QCOMPARE(jmiWithHigherId->id(), "ca3cf894-5325-482f-a412-a6e9f832298d");
                QVERIFY(jmiWithHigherId->isProceeded());
            }
        }
    });

    QXmppMessage message;
    message.parse(xmlToDom(xmlProposeLowId));

    QVERIFY(m_manager.handleMessage(message));
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleNonExistingSessionHigherId()
{
    // --- request with higher id sends propose to request with lower id ---
    QByteArray xmlProposeHighId {
        "<message from='julietNonExistingSession@capulet.example/high' to='romeo@montague.example' type='chat'>"
        "<propose xmlns='urn:xmpp:jingle-message:0' id='fecbea35-08d3-404f-9ec7-2b57c566fa74'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
        "</propose>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Expired);
    reason.setText("Tie-Break");

    auto jmiWithLowerId = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietNonExistingSession@capulet.example");

    // make sure that request with lower id rejects request with higher id
    connect(&m_logger, &QXmppLogger::message, this, [jid = jmiWithLowerId->remoteJid(), reason](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jid) {
                const auto &jmiElement { message.jingleMessageInitiationElement() };
                QVERIFY(jmiElement);

                QCOMPARE(jmiElement->type(), JmiType::Reject);
                QCOMPARE(jmiElement->id(), "fecbea35-08d3-404f-9ec7-2b57c566fa74");
                QVERIFY(jmiElement->reason());
                QCOMPARE(jmiElement->reason()->type(), reason.type());
                QCOMPARE(jmiElement->reason()->text(), reason.text());
            }
        }
    });

    QXmppMessage message;
    message.parse(xmlToDom(xmlProposeHighId));

    QVERIFY(m_manager.handleMessage(message));
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleExistingSession()
{
    QXmppMessage message;

    QByteArray xmlPropose {
        "<message from='julietExistingSession@capulet.example/tablet' to='romeo@montague.example' type='chat'>"
        "<propose xmlns='urn:xmpp:jingle-message:0' id='989a46a6-f202-4910-a7c3-83c6ba3f3947'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
        "</propose>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi { m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "julietExistingSession@capulet.example") };
    jmi->setIsProceeded(true);

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Expired);
    reason.setText("Session migrated");

    connect(&m_logger, &QXmppLogger::message, this, [jmi, reason](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jmi->remoteJid()) {
                const auto &jmiElement { message.jingleMessageInitiationElement() };
                QVERIFY(jmiElement);

                QCOMPARE(jmiElement->type(), JmiType::Finish);
                QCOMPARE(jmiElement->id(), jmi->id());
                QCOMPARE(jmiElement->migratedTo(), "989a46a6-f202-4910-a7c3-83c6ba3f3947");
                QVERIFY(jmiElement->reason());
                QCOMPARE(jmiElement->reason()->type(), reason.type());
                QCOMPARE(jmiElement->reason()->text(), reason.text());
            }
        }
    });

    message.parse(xmlToDom(xmlPropose));

    QVERIFY(m_manager.handleMessage(message));
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleTieBreak()
{
    QString callPartnerJid { "romeoHandleTieBreakExistingSession@montague.example/orchard" };
    QString jmiId { "ca3cf894-5325-482f-a412-a6e9f832298d" };
    auto jmi = m_manager.addJmi(jmiId, QXmppUtils::jidToBareJid(callPartnerJid));

    QXmppJingleMessageInitiationElement jmiElement;
    QString newJmiId("989a46a6-f202-4910-a7c3-83c6ba3f3947");
    jmiElement.setId(newJmiId);

    // Signal spy assertions depend on sends succeeding, which requires a real
    // server connection with valid JIDs. Only verify the return value here.
    jmi->setIsProceeded(true);
    QVERIFY(m_manager.handleTieBreak(jmi, jmiElement, QXmppUtils::jidToResource(callPartnerJid)));

    jmi->setIsProceeded(false);
    QVERIFY(m_manager.handleTieBreak(jmi, jmiElement, QXmppUtils::jidToResource(callPartnerJid)));

    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleProposeJmiElement()
{
    QXmppJingleMessageInitiationElement jmiElement;
    jmiElement.setId("ca3cf123-5325-482f-a412-a6e9f832298d");
    jmiElement.setDescription(QXmppJingleRtpDescription { u"audio"_s, 321 });

    QString callPartnerJid = u"juliet@capulet.example"_s;

    // --- Tie break ---

    auto jmi = m_manager.addJmi("989a4123-f202-4910-a7c3-83c6ba3f3947", callPartnerJid);

    QVERIFY(m_manager.handleProposeJmiElement(jmiElement, callPartnerJid));
    QCOMPARE(m_manager.jmis().size(), 1);
    m_manager.clearAll();

    // --- usual JMI proposal ---

    connect(&m_manager, &QXmppJingleMessageInitiationManager::proposed, this, [&, jmiElement](const std::shared_ptr<Jmi> &, const QString &jmiElementId, const std::optional<QXmppJingleRtpDescription> &description) {
        if (jmiElement.id() == jmiElementId) {
            QCOMPARE(m_manager.jmis().size(), 1);
            QVERIFY(description.has_value());
            QCOMPARE(description->media(), jmiElement.description()->media());
            QCOMPARE(description->ssrc(), jmiElement.description()->ssrc());
        }
    });

    callPartnerJid = "romeoHandleProposeJmiElement@montague.example";

    QVERIFY(m_manager.handleProposeJmiElement(jmiElement, callPartnerJid));
    QCOMPARE(m_manager.jmis().size(), 1);
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleExistingJmi()
{
    QString callPartnerJid { "juliet@capulet.example" };
    QString jmiId { "989a46a6-f202-4910-a7c3-83c6ba3f3947" };

    auto jmi = m_manager.addJmi(jmiId, callPartnerJid);

    QXmppJingleMessageInitiationElement jmiElement;
    jmiElement.setId(jmiId);

    // --- ringing ---

    QSignalSpy ringingSpy(jmi.get(), &QXmppJingleMessageInitiation::ringing);

    jmiElement.setType(JmiType::Ringing);

    QVERIFY(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid));
    QCOMPARE(ringingSpy.count(), 1);
    m_manager.clearAll();

    // --- proceeded ---

    jmi = m_manager.addJmi(jmiId, callPartnerJid);

    jmiElement.setType(JmiType::Proceed);
    connect(jmi.get(), &QXmppJingleMessageInitiation::proceeded, this, [jmiElement](const QString &jmiElementId) {
        if (jmiElementId == jmiElement.id()) {
            QVERIFY(true);
        }
    });

    QVERIFY(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid));
    m_manager.clearAll();

    // --- closed: rejected ---

    jmi = m_manager.addJmi(jmiId, callPartnerJid);

    QXmppJingleReason reason;
    reason.setType(QXmppJingleReason::Expired);
    reason.setText("Rejected because expired.");

    jmiElement.setType(JmiType::Reject);
    jmiElement.setReason(reason);

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [jmiElement](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Rejected;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &rejectedJmiElement { std::get<ResultType>(result) };

        QVERIFY(rejectedJmiElement.reason);
        QCOMPARE(rejectedJmiElement.reason->type(), jmiElement.reason()->type());
        QCOMPARE(rejectedJmiElement.reason->text(), jmiElement.reason()->text());
        QCOMPARE(rejectedJmiElement.containsTieBreak, jmiElement.containsTieBreak());
    });

    QVERIFY(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid));
    m_manager.clearAll();

    // --- closed: retracted ---

    jmi = m_manager.addJmi(jmiId, callPartnerJid);

    reason.setType(QXmppJingleReason::ConnectivityError);
    reason.setText("Retracted due to connectivity error.");

    jmiElement.setType(JmiType::Retract);
    jmiElement.setReason(reason);

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [jmiElement](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Retracted;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &rejectedJmiElement { std::get<ResultType>(result) };

        QVERIFY(rejectedJmiElement.reason);
        QCOMPARE(rejectedJmiElement.reason->type(), jmiElement.reason()->type());
        QCOMPARE(rejectedJmiElement.reason->text(), jmiElement.reason()->text());
        QCOMPARE(rejectedJmiElement.containsTieBreak, jmiElement.containsTieBreak());
    });

    QVERIFY(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid));
    m_manager.clearAll();

    // --- closed: finished ---

    jmi = m_manager.addJmi(jmiId, callPartnerJid);

    reason.setType(QXmppJingleReason::Success);
    reason.setText("Finished.");

    jmiElement.setType(JmiType::Finish);
    jmiElement.setReason(reason);
    jmiElement.setMigratedTo("ca3cf894-5325-482f-a412-a6e9f832298d");

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [jmiElement](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Finished;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &rejectedJmiElement { std::get<ResultType>(result) };

        QVERIFY(rejectedJmiElement.reason);
        QCOMPARE(rejectedJmiElement.reason->type(), jmiElement.reason()->type());
        QCOMPARE(rejectedJmiElement.reason->text(), jmiElement.reason()->text());
        QCOMPARE(rejectedJmiElement.migratedTo, jmiElement.migratedTo());
    });

    QVERIFY(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid));
    m_manager.clearAll();

    // --- none ---

    jmi = m_manager.addJmi(jmiId, callPartnerJid);

    jmiElement.setType(JmiType::None);

    QCOMPARE(m_manager.handleExistingJmi(jmi, jmiElement, callPartnerJid), false);
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleJmiElement()
{
    QString callPartnerJid { "romeoHandleJmiElement@montague.example/orchard" };
    QString jmiId { "ca3cf894-5325-482f-a412-a6e9f832298d" };

    // case 1: no JMI found in JMIs vector and jmiElement is not a propose element
    QXmppJingleMessageInitiationElement jmiElement;
    jmiElement.setType(JmiType::None);

    QCOMPARE(m_manager.handleJmiElement(std::move(jmiElement), {}), false);

    // case 2: no JMI found in JMIs vector and jmiElement is a propose element
    jmiElement = {};
    jmiElement.setType(JmiType::Propose);
    jmiElement.setId(jmiId);

    QSignalSpy proposedSpy(&m_manager, &QXmppJingleMessageInitiationManager::proposed);
    QVERIFY(m_manager.handleJmiElement(std::move(jmiElement), callPartnerJid));
    QCOMPARE(proposedSpy.count(), 1);
    m_manager.clearAll();

    // case 3: JMI found in JMIs vector, existing session
    jmiElement = {};
    jmiElement.setType(JmiType::Ringing);
    jmiElement.setId(jmiId);
    auto jmi = m_manager.addJmi(jmiId, QXmppUtils::jidToBareJid(callPartnerJid));

    QSignalSpy ringingSpy(jmi.get(), &QXmppJingleMessageInitiation::ringing);
    QVERIFY(m_manager.handleJmiElement(std::move(jmiElement), callPartnerJid));
    QCOMPARE(ringingSpy.count(), 1);
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessage_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("xmlValid")
        << QByteArray(
               "<message to='julietHandleMessageValid@capulet.example' from='romeoHandleMessageValid@montague.example/orchard' type='chat'>"
               "<store xmlns=\"urn:xmpp:hints\"/>"
               "<propose xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
               "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
               "</propose>"
               "</message>")
        << true;

    QTest::newRow("xmlInvalidTypeNotChat")
        << QByteArray(
               "<message to='julietHandleMessageNoChat@capulet.example' from='romeoHandleMessageNoChat@montague.example/orchard' type='normal'>"
               "<store xmlns=\"urn:xmpp:hints\"/>"
               "<propose xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
               "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'/>"
               "</propose>"
               "</message>")
        << false;

    QTest::newRow("xmlInvalidNoJmiElement")
        << QByteArray("<message to='julietHandleMessageNoJmi@capulet.example' from='romeoHandleMessageNoJmi@montague.example/orchard' type='chat'/>")
        << false;
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessage()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QXmppMessage message;

    parsePacket(message, xml);
    QCOMPARE(m_manager.handleMessage(message), isValid);
    serializePacket(message, xml);

    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessageRinging()
{
    QXmppMessage message;
    QByteArray xmlRinging {
        "<message from='juliet@capulet.example/phone' to='romeo@montague.example' type='chat'>"
        "<ringing xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'/>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "juliet@capulet.example");

    QSignalSpy ringingSpy(jmi.get(), &QXmppJingleMessageInitiation::ringing);

    message.parse(xmlToDom(xmlRinging));

    QVERIFY(m_manager.handleMessage(message));
    QCOMPARE(ringingSpy.count(), 1);
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessageProceeded()
{
    QXmppMessage message;
    QByteArray xmlProceed {
        "<message from='juliet@capulet.example/phone' to='romeo@montague.example' type='chat'>"
        "<proceed xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'/>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "juliet@capulet.example");

    QSignalSpy proceededSpy(jmi.get(), &QXmppJingleMessageInitiation::proceeded);

    message.parse(xmlToDom(xmlProceed));

    QVERIFY(m_manager.handleMessage(message));
    QCOMPARE(proceededSpy.count(), 1);
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessageClosedRejected()
{
    QXmppMessage message;
    QByteArray xmlReject {
        "<message from='juliet@capulet.example/phone' to='romeo@montague.example' type='chat'>"
        "<reject xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
        "<reason xmlns=\"urn:xmpp:jingle:1\">"
        "<busy/>"
        "<text>Busy</text>"
        "</reason>"
        "</reject>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "juliet@capulet.example");

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Rejected;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &rejectedJmiElement { std::get<ResultType>(result) };

        QCOMPARE(rejectedJmiElement.reason->type(), QXmppJingleReason::Busy);
        QCOMPARE(rejectedJmiElement.reason->text(), "Busy");
    });

    message.parse(xmlToDom(xmlReject));

    QVERIFY(m_manager.handleMessage(message));
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessageClosedRetracted()
{
    QXmppMessage message;
    QByteArray xmlRetract {
        "<message from='romeo@montague.example/orchard' to='juliet@capulet.example' type='chat'>"
        "<retract xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
        "<reason xmlns=\"urn:xmpp:jingle:1\">"
        "<cancel/>"
        "<text>Retracted</text>"
        "</reason>"
        "</retract>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "romeo@montague.example");

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Retracted;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &retractedJmiElement { std::get<ResultType>(result) };

        QCOMPARE(retractedJmiElement.reason->type(), QXmppJingleReason::Cancel);
        QCOMPARE(retractedJmiElement.reason->text(), "Retracted");
    });

    message.parse(xmlToDom(xmlRetract));

    QVERIFY(m_manager.handleMessage(message));
    m_manager.clearAll();
}

void tst_QXmppJingleMessageInitiationManager::testHandleMessageClosedFinished()
{
    QXmppMessage message;
    QByteArray xmlFinish {
        "<message from='romeo@montague.example/orchard' to='juliet@capulet.example' type='chat'>"
        "<finish xmlns='urn:xmpp:jingle-message:0' id='ca3cf894-5325-482f-a412-a6e9f832298d'>"
        "<reason xmlns=\"urn:xmpp:jingle:1\">"
        "<success/>"
        "<text>Success</text>"
        "</reason>"
        "<migrated to='989a46a6-f202-4910-a7c3-83c6ba3f3947'/>"
        "</finish>"
        "<store xmlns=\"urn:xmpp:hints\"/>"
        "</message>"
    };

    auto jmi = m_manager.addJmi("ca3cf894-5325-482f-a412-a6e9f832298d", "romeo@montague.example");

    connect(jmi.get(), &QXmppJingleMessageInitiation::closed, this, [](const JmiResult &result) {
        using ResultType = QXmppJingleMessageInitiation::Finished;

        QVERIFY(std::holds_alternative<ResultType>(result));
        const ResultType &finishedJmiElement { std::get<ResultType>(result) };

        QCOMPARE(finishedJmiElement.reason->type(), QXmppJingleReason::Success);
        QCOMPARE(finishedJmiElement.reason->text(), "Success");
        QCOMPARE(finishedJmiElement.migratedTo, "989a46a6-f202-4910-a7c3-83c6ba3f3947");
    });

    // XEP-0353 §3.7: both parties SHOULD send <finish/>. Verify that receiving
    // a <finish/> causes our own <finish/> to be sent back.
    bool finishEchoed = false;
    connect(&m_logger, &QXmppLogger::message, this, [&finishEchoed](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage sent;
            parsePacket(sent, text.toUtf8());

            if (auto el = sent.jingleMessageInitiationElement()) {
                if (el->type() == JmiType::Finish) {
                    QCOMPARE(el->id(), u"ca3cf894-5325-482f-a412-a6e9f832298d");
                    QVERIFY(el->reason());
                    QCOMPARE(el->reason()->type(), QXmppJingleReason::Success);
                    finishEchoed = true;
                }
            }
        }
    });

    message.parse(xmlToDom(xmlFinish));

    QVERIFY(m_manager.handleMessage(message));
    QVERIFY(finishEchoed);
    m_manager.clearAll();
}

// ============================================================

using CallInviteType = QXmppCallInviteElement::Type;
using CallInviteResult = QXmppCallInvite::Result;

class tst_QXmppCallInviteManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testClear();
    Q_SLOT void testClearAll();

    Q_SLOT void testAccept();
    Q_SLOT void testReject();
    Q_SLOT void testRetract();
    Q_SLOT void testLeft();

    Q_SLOT void testInvite();
    Q_SLOT void testSendMessage();

    Q_SLOT void testHandleExistingCallInvite();
    Q_SLOT void testHandleCallInviteElement();
    Q_SLOT void testHandleMessage_data();
    Q_SLOT void testHandleMessage();
    Q_SLOT void testHandleMessageAccepted();
    Q_SLOT void testHandleMessageRejected();
    Q_SLOT void testHandleMessageRetracted();
    Q_SLOT void testHandleMessageLeft();

    QXmppClient m_client;
    QXmppLogger m_logger;
    QXmppCallInviteManager m_manager;
};

void tst_QXmppCallInviteManager::initTestCase()
{
    m_client.addExtension(&m_manager);

    m_logger.setLoggingType(QXmppLogger::SignalLogging);
    m_client.setLogger(&m_logger);

    m_client.connectToServer(IntegrationTests::clientConfiguration());
    m_client.configuration().setJid("mixer@example.com");

    qRegisterMetaType<QXmppCallInvite::Result>();
    qRegisterMetaType<std::shared_ptr<QXmppCallInvite>>();
}

void tst_QXmppCallInviteManager::testClear()
{
    QCOMPARE(m_manager.callInvites().size(), 0);
    auto callInvite1 { m_manager.addCallInvite("test1") };
    auto callInvite2 { m_manager.addCallInvite("test2") };
    QCOMPARE(m_manager.callInvites().size(), 2);

    m_manager.clear(callInvite1);
    m_manager.clear(callInvite2);
    QCOMPARE(m_manager.callInvites().size(), 0);
}

void tst_QXmppCallInviteManager::testClearAll()
{
    QCOMPARE(m_manager.callInvites().size(), 0);
    m_manager.addCallInvite("test1");
    m_manager.addCallInvite("test2");
    m_manager.addCallInvite("test3");
    m_manager.addCallInvite("test4");
    m_manager.addCallInvite("test5");
    QCOMPARE(m_manager.callInvites().size(), 5);

    m_manager.clearAll();
    QCOMPARE(m_manager.callInvites().size(), 0);
}

void tst_QXmppCallInviteManager::testAccept()
{
    auto callInvite { m_manager.addCallInvite("maraTestAccept@example.com") };
    callInvite->setId("id1_testAccept");

    connect(&m_logger, &QXmppLogger::message, this, [callInviteCallPartnerJid = callInvite->callPartnerJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == callInviteCallPartnerJid) {
                QVERIFY(message.callInviteElement());
                QCOMPARE(message.callInviteElement()->type(), CallInviteType::Accept);
            }
        }
    });

    auto future = callInvite->accept();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testReject()
{
    auto callInvite { m_manager.addCallInvite("maraTestReject@example.com") };
    callInvite->setId("id1_testReject");

    connect(&m_logger, &QXmppLogger::message, this, [callInviteCallPartnerJid = callInvite->callPartnerJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == callInviteCallPartnerJid) {
                QVERIFY(message.callInviteElement());
                QCOMPARE(message.callInviteElement()->id(), u"id1_testReject"_s);
                QCOMPARE(message.callInviteElement()->type(), CallInviteType::Reject);
            }
        }
    });

    auto future = callInvite->reject();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testRetract()
{
    auto callInvite { m_manager.addCallInvite("maraTestRetract@example.com") };
    callInvite->setId("id1_testRetract");

    connect(&m_logger, &QXmppLogger::message, this, [callInviteCallPartnerJid = callInvite->callPartnerJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == callInviteCallPartnerJid) {
                QVERIFY(message.callInviteElement());
                QCOMPARE(message.callInviteElement()->id(), u"id1_testRetract"_s);
                QCOMPARE(message.callInviteElement()->type(), CallInviteType::Retract);
            }
        }
    });

    auto future = callInvite->retract();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testLeft()
{
    auto callInvite { m_manager.addCallInvite("maraTestLeft@example.com") };
    callInvite->setId("id1_testLeft");

    connect(&m_logger, &QXmppLogger::message, this, [callInviteCallPartnerJid = callInvite->callPartnerJid()](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == callInviteCallPartnerJid) {
                QVERIFY(message.callInviteElement());
                QCOMPARE(message.callInviteElement()->id(), u"id1_testLeft"_s);
                QCOMPARE(message.callInviteElement()->type(), CallInviteType::Left);
            }
        }
    });

    auto future = callInvite->leave();

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testInvite()
{
    QString jid { "maraTestInvite@example.com" };
    bool video { true };
    bool audio { false };

    QXmppCallInviteElement::Jingle jingle;
    jingle.jid = "mixer@example.com/uuid";
    jingle.sid = "sid1";

    QList<QXmppCallInviteElement::External> external;
    external.append({ "https://example.com/uuid" });
    external.append({ "tel:+12345678" });

    connect(&m_logger, &QXmppLogger::message, this, [&, jid, video, audio, jingle, external](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jid) {
                const auto &callInviteElement { message.callInviteElement() };
                QVERIFY(callInviteElement);

                QCOMPARE(callInviteElement->type(), CallInviteType::Invite);
                QVERIFY(!callInviteElement->id().isEmpty());
                QCOMPARE(callInviteElement->video(), video);
                QCOMPARE(callInviteElement->audio(), audio);
                QVERIFY(callInviteElement->jingle());
                QCOMPARE(callInviteElement->jingle().value(), jingle);
                QVERIFY(callInviteElement->external());
                QCOMPARE(callInviteElement->external().value(), external);
            }
        }
    });

    auto future = m_manager.invite(jid, audio, video, jingle, external);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testSendMessage()
{
    QString jid { "maraSendMessage@example.com" };

    QXmppCallInviteElement callInviteElement;
    callInviteElement.setType(CallInviteType::Invite);
    callInviteElement.setId(u"id1_testSendMessage"_s);

    connect(&m_logger, &QXmppLogger::message, this, [jid, callInviteElement](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == jid) {
                QVERIFY(message.callInviteElement());
                QCOMPARE(message.callInviteElement()->type(), callInviteElement.type());
                QCOMPARE(message.callInviteElement()->id(), callInviteElement.id());
            }
        }
    });

    auto future = m_manager.sendMessage(callInviteElement, jid);

    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY(future.isFinished());
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleExistingCallInvite()
{
    QString callPartnerJid { "maraTestHandleExistingCallInvite@example.com" };
    QString callInviteId { "id1_testHandleExistingCallInvite" };

    auto callInvite { m_manager.addCallInvite(callPartnerJid) };
    callInvite->setId(callInviteId);

    QXmppCallInviteElement callInviteElement;
    callInviteElement.setId(callInviteId);

    // --- closed: rejected ---

    callInvite = m_manager.addCallInvite(callPartnerJid);
    callInvite->setId(callInviteId);

    callInviteElement.setType(CallInviteType::Reject);

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [callInviteElement](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Rejected>(result));
    });

    QVERIFY(m_manager.handleExistingCallInvite(callInvite, callInviteElement, callPartnerJid));
    m_manager.clearAll();

    // --- closed: retracted ---

    callInvite = m_manager.addCallInvite(callPartnerJid);
    callInvite->setId(callInviteId);

    callInviteElement.setType(CallInviteType::Retract);

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [callInviteElement](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Retracted>(result));
    });

    QVERIFY(m_manager.handleExistingCallInvite(callInvite, callInviteElement, callPartnerJid));
    m_manager.clearAll();

    // --- closed: left ---

    callInvite = m_manager.addCallInvite(callPartnerJid);
    callInvite->setId(callInviteId);

    callInviteElement.setType(CallInviteType::Left);

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [callInviteElement](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Left>(result));
    });

    QVERIFY(m_manager.handleExistingCallInvite(callInvite, callInviteElement, callPartnerJid));
    m_manager.clearAll();

    // --- none ---

    callInvite = m_manager.addCallInvite(callPartnerJid);
    callInvite->setId(callInviteId);

    callInviteElement.setType(CallInviteType::None);

    QCOMPARE(m_manager.handleExistingCallInvite(callInvite, callInviteElement, callPartnerJid), false);
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleCallInviteElement()
{
    QString callPartnerJid { "maraTestHandleCallInviteElement@example.com/orchard" };
    QString callInviteId { "id1_HandleCallInviteElement" };

    // case 1: no Call Invite element found in Call Invites vector and callInviteElement is not an invite element
    QXmppCallInviteElement callInviteElement;
    callInviteElement.setType(CallInviteType::None);

    QCOMPARE(m_manager.handleCallInviteElement(std::move(callInviteElement), {}), false);

    // case 2: no Call Invite found in Call Invites vector and callInviteElement is an invite element
    callInviteElement = {};
    callInviteElement.setType(CallInviteType::Invite);
    callInviteElement.setId(callInviteId);

    QSignalSpy invitedSpy(&m_manager, &QXmppCallInviteManager::invited);
    QVERIFY(m_manager.handleCallInviteElement(std::move(callInviteElement), callPartnerJid));
    QCOMPARE(invitedSpy.count(), 1);
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleMessage_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("xmlValid")
        << QByteArray(
               "<message id='id1' to='mara@example.com' type='chat'>"
               "<invite xmlns='urn:xmpp:call-invites:0' video='true'>"
               "<jingle sid='sid1'/>"
               "</invite>"
               "</message>")
        << true;

    QTest::newRow("xmlValidWithJingleJid")
        << QByteArray(
               "<message id='id1' to='mara@example.com' type='chat'>"
               "<invite xmlns='urn:xmpp:call-invites:0' video='true'>"
               "<jingle sid='sid1' jid='mixer@example.com/uuid'/>"
               "</invite>"
               "</message>")
        << true;

    QTest::newRow("xmlValidWithExternal")
        << QByteArray(
               "<message id='id1' to='mara@example.com' type='chat'>"
               "<invite xmlns='urn:xmpp:call-invites:0' video='true'>"
               "<jingle sid='sid1'/>"
               "<external uri='https://example.com/uuid'/>"
               "<external uri='tel:+12345678'/>"
               "</invite>"
               "</message>")
        << true;

    QTest::newRow("xmlInvalidNoJingle")
        << QByteArray(
               "<message id='id1' to='mara@example.com' type='chat'>"
               "<invite xmlns='urn:xmpp:call-invites:0' video='true'/>"
               "</message>")
        << true;

    QTest::newRow("xmlInvalidTypeNotChat")
        << QByteArray(
               "<message id='id1' to='mara@example.com' type='normal'>"
               "<invite xmlns='urn:xmpp:call-invites:0' video='true'>"
               "<jingle sid='sid1'/>"
               "</invite>"
               "</message>")
        << false;

    QTest::newRow("xmlInvalidNoCallInviteElement")
        << QByteArray("<message id='id1' to='mara@example.com' type='chat'/>")
        << false;
}

void tst_QXmppCallInviteManager::testHandleMessage()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QXmppMessage message;

    parsePacket(message, xml);
    QCOMPARE(m_manager.handleMessage(message), isValid);
    serializePacket(message, xml);

    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleMessageAccepted()
{
    QXmppMessage message;
    QByteArray xmlAccept {
        "<message to='maraTestHandleMessageAccepted@example.com' type='chat'>"
        "<accept id='id1_testHandleMessageAccepted' xmlns='urn:xmpp:call-invites:0'>"
        "<jingle sid='sid1' jid='mixer@example.com/uuid'/>"
        "</accept>"
        "</message>"
    };

    auto callInvite { m_manager.addCallInvite("mixer@example.com") };
    callInvite->setId("id1_testHandleMessageAccepted");

    QSignalSpy acceptedSpy(callInvite.get(), &QXmppCallInvite::accepted);

    message.parse(xmlToDom(xmlAccept));

    QVERIFY(m_manager.handleMessage(message));
    QCOMPARE(acceptedSpy.count(), 1);
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleMessageRejected()
{
    QXmppMessage message;
    QByteArray xmlReject {
        "<message to='maraTestHandleMessageRejected@example.com' type='chat'>"
        "<reject xmlns='urn:xmpp:call-invites:0' id='id1_testHandleMessageRejected'/>"
        "</message>"
    };

    auto callInvite { m_manager.addCallInvite("mixer@example.com") };
    callInvite->setId("id1_testHandleMessageRejected");

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Rejected>(result));
    });

    message.parse(xmlToDom(xmlReject));

    QVERIFY(m_manager.handleMessage(message));
    serializePacket(message, xmlReject);

    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleMessageRetracted()
{
    QXmppMessage message;
    QByteArray xmlRetract {
        "<message to='maraTestHandleMessageRetracted@example.com' type='chat'>"
        "<retract xmlns='urn:xmpp:call-invites:0' id='id1_testHandleMessageRetracted'/>"
        "</message>"
    };

    auto callInvite { m_manager.addCallInvite("mixer@example.com") };
    callInvite->setId("id1_testHandleMessageRetracted");

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Retracted>(result));
    });

    message.parse(xmlToDom(xmlRetract));

    QVERIFY(m_manager.handleMessage(message));
    serializePacket(message, xmlRetract);
    m_manager.clearAll();
}

void tst_QXmppCallInviteManager::testHandleMessageLeft()
{
    QXmppMessage message;
    QByteArray xmlLeft {
        "<message to='maraTestHandleMessageLeft@example.com' type='chat'>"
        "<left xmlns='urn:xmpp:call-invites:0' id='id1_testHandleMessageLeft'/>"
        "</message>"
    };

    auto callInvite { m_manager.addCallInvite("mixer@example.com") };
    callInvite->setId("id1_testHandleMessageLeft");

    connect(callInvite.get(), &QXmppCallInvite::closed, this, [](const CallInviteResult &result) {
        QVERIFY(std::holds_alternative<QXmppCallInvite::Left>(result));
    });

    message.parse(xmlToDom(xmlLeft));

    QVERIFY(m_manager.handleMessage(message));
    serializePacket(message, xmlLeft);
    m_manager.clearAll();
}

// ============================================================

#ifdef WITH_GSTREAMER

using namespace QXmpp::Private;
using Error = QXmppStanza::Error;

class tst_QXmppCallManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void callInvalidJid();
    Q_SLOT void invalidSid();
    Q_SLOT void senderImpersonation();
    Q_SLOT void testCall();
};

void tst_QXmppCallManager::initTestCase()
{
    auto *rtpBin = gst_element_factory_make("rtpbin", nullptr);
    if (!rtpBin) {
        QSKIP("GStreamer rtpbin element not available (install gstreamer-good plugins)");
    }
    gst_object_unref(rtpBin);
}

void tst_QXmppCallManager::callInvalidJid()
{
    TestClient client;
    client.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = client.addNewExtension<QXmppCallManager>();

    auto call = manager->call(QString());
    QCOMPARE(call->state(), QXmppCall::FinishedState);
    QVERIFY(call->error().has_value());

    call = manager->call("test@localhost/r1");
    QVERIFY(call);
    QCOMPARE(call->sid().size(), 36);
    QCOMPARE(call->jid(), u"test@localhost/r1");
    QCOMPARE(call->direction(), QXmppCall::OutgoingDirection);
}

void tst_QXmppCallManager::invalidSid()
{
    const auto xml =
        u"<iq from='romeo@montague.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='session-initiate' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='96' name='speex' clockrate='16000' />"
        "<payload-type id='97' name='speex' clockrate='8000' />"
        "<payload-type id='18' name='G729' />"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "<payload-type id='103' name='L16' clockrate='16000' channels='2' />"
        "<payload-type id='98' name='x-ISAC' clockrate='8000' />"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;

    TestClient client;
    auto *manager = client.addNewExtension<QXmppCallManager>();
    client.configuration().setJid(u"juliet@capulet.lit/balcony"_s);

    // take over ownership of all incoming calls (so they are not deleted)
    std::vector<std::unique_ptr<QXmppCall>> calls;
    connect(manager, &QXmppCallManager::callReceived, this, [&](std::unique_ptr<QXmppCall> &call) {
        calls.push_back(std::move(call));
    });

    // start first call
    QVERIFY(manager->handleStanza(xmlToDom(xml.arg("abc1"))));
    QCoreApplication::processEvents();
    client.expect(u"<iq id='ph37a419' to='romeo@montague.lit/orchard' type='result'/>"_s);
    client.expect(u"<iq id=\"qx3\" to=\"capulet.lit\" type=\"get\"><services xmlns=\"urn:xmpp:extdisco:2\"/></iq>"_s);
    client.inject(u"<iq id='qx3' from='capulet.lit' type='result'><services xmlns='urn:xmpp:extdisco:2'/></iq>"_s);
    QCoreApplication::processEvents();
    client.expect(u"<iq id=\"qx2\" to=\"romeo@montague.lit/orchard\" from=\"juliet@capulet.lit/balcony\" type=\"set\"><jingle xmlns=\"urn:xmpp:jingle:1\" action=\"session-info\" sid=\"abc1\"><ringing xmlns=\"urn:xmpp:jingle:apps:rtp:info:1\"/></jingle></iq>"_s);

    // same sid
    auto error = expectVariant<Error>(manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml.arg("abc1")))));
    QCOMPARE(error.type(), Error::Cancel);
    QCOMPARE(error.condition(), Error::Conflict);
}

void tst_QXmppCallManager::senderImpersonation()
{
    const auto xml =
        u"<iq from='romeo@montague.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='session-initiate' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='96' name='speex' clockrate='16000' />"
        "<payload-type id='97' name='speex' clockrate='8000' />"
        "<payload-type id='18' name='G729' />"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "<payload-type id='103' name='L16' clockrate='16000' channels='2' />"
        "<payload-type id='98' name='x-ISAC' clockrate='8000' />"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;

    TestClient client;
    auto *manager = client.addNewExtension<QXmppCallManager>();
    client.configuration().setJid(u"juliet@capulet.lit/balcony"_s);

    // session initiate
    auto result = manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml.arg("abc1"))));
    QVERIFY(std::holds_alternative<QXmppIq>(result));

    // other JID trying to inject IQs into our call (different 'from')
    const auto xml2 =
        u"<iq from='r0me0@m0ntagu3.lit/orchard' id='ph37a419' to='juliet@capulet.lit/balcony' type='set'>"
        "<jingle xmlns='urn:xmpp:jingle:1' action='content-add' initiator='romeo@montague.lit/orchard' sid='%1'>"
        "<content creator='initiator' name='voice'>"
        "<description xmlns='urn:xmpp:jingle:apps:rtp:1' media='audio'>"
        "<payload-type id='0' name='PCMU' clockrate='8000'/>"
        "</description>"
        "<transport xmlns='urn:xmpp:jingle:transports:ice-udp:1' pwd='asd88fgpdd777uzjYhagZg' ufrag='8hhy'>"
        "<candidate component='1' foundation='1' generation='0' id='el0747fg11' ip='10.0.1.1' network='1' port='8998' priority='2130706431' protocol='udp' type='host' />"
        "<candidate component='1' foundation='2' generation='0' id='y3s2b30v3r' ip='192.0.2.3' network='1' port='45664' priority='1694498815' protocol='udp' rel-addr='10.0.1.1' rel-port='8998' type='srflx' />"
        "</transport>"
        "</content>"
        "</jingle></iq>"_s;
    result = manager->handleIq(parseInto<QXmppJingleIq>(xmlToDom(xml2.arg("abc1"))));
    auto error = expectVariant<Error>(std::move(result));
    QCOMPARE(error.type(), Error::Cancel);
    QCOMPARE(error.condition(), Error::ItemNotFound);

    // manager makes use of later() calls
    QCoreApplication::processEvents();
}

void tst_QXmppCallManager::testCall()
{
    if (!qEnvironmentVariableIsEmpty("QXMPP_TESTS_SKIP_CALL_MANAGER")) {
        QSKIP("Skipping because 'QXMPP_TESTS_SKIP_CALL_MANAGER' was set.");
    }

    std::unique_ptr<QXmppCall> receiverCall;

    const QString testDomain("localhost");
    const QHostAddress testHost(QHostAddress::LocalHost);
    const quint16 testPort = 12002;

    // prepare server
    TestPasswordChecker passwordChecker;
    passwordChecker.addCredentials("sender", "testpwd");
    passwordChecker.addCredentials("receiver", "testpwd");

    QXmppServer server;
    server.setDomain(testDomain);
    server.setPasswordChecker(&passwordChecker);
    server.listenForClients(testHost, testPort);

    // prepare sender
    TestClient sender;
    sender.addNewExtension<QXmppDiscoveryManager>();
    auto *senderManager = sender.addNewExtension<QXmppCallManager>();

    QXmppConfiguration config;
    config.setDomain(testDomain);
    config.setHost(testHost.toString());
    config.setPort(testPort);
    config.setUser("sender");
    config.setPassword("testpwd");
    sender.connectToServer(config);
    sender.waitForConnect();
    QCOMPARE(sender.isConnected(), true);

    // prepare receiver
    TestClient receiver;
    receiver.addNewExtension<QXmppDiscoveryManager>();
    auto *receiverManager = receiver.addNewExtension<QXmppCallManager>();
    connect(receiverManager, &QXmppCallManager::callReceived, this, [&receiverCall](std::unique_ptr<QXmppCall> &call) {
        receiverCall = std::move(call);
        receiverCall->accept();
    });

    config.setUser("receiver");
    config.setPassword("testpwd");
    receiver.connectToServer(config);
    receiver.waitForConnect();
    QCOMPARE(receiver.isConnected(), true);

    // connect call
    qDebug() << "======== CONNECT ========";
    QEventLoop loop;
    auto senderCall = senderManager->call(receiver.configuration().jid());
    QVERIFY(senderCall);
    connect(senderCall.get(), &QXmppCall::connected, &loop, &QEventLoop::quit);
    loop.exec();
    QVERIFY(receiverCall);

    QCOMPARE(senderCall->direction(), QXmppCall::OutgoingDirection);
    QCOMPARE(senderCall->state(), QXmppCall::ActiveState);

    QCOMPARE(receiverCall->direction(), QXmppCall::IncomingDirection);
    QCOMPARE(receiverCall->state(), QXmppCall::ActiveState);

    // exchange some media
    qDebug() << "======== TALK ========";
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    // hangup call
    qDebug() << "======== HANGUP ========";
    connect(senderCall.get(), &QXmppCall::finished, &loop, &QEventLoop::quit);
    senderCall->hangUp();
    loop.exec();

    QCOMPARE(senderCall->direction(), QXmppCall::OutgoingDirection);
    QCOMPARE(senderCall->state(), QXmppCall::FinishedState);

    QCOMPARE(receiverCall->direction(), QXmppCall::IncomingDirection);
    QCOMPARE(receiverCall->state(), QXmppCall::FinishedState);
}

#endif  // WITH_GSTREAMER

// ============================================================

class tst_QXmppExternalServiceDiscoveryManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testRequestServices();
    Q_SLOT void testDiscoveryFeatures();
};

void tst_QXmppExternalServiceDiscoveryManager::testRequestServices()
{
    TestClient test;
    auto *extDiscoManager { test.addNewExtension<QXmppExternalServiceDiscoveryManager>() };

    auto future { extDiscoManager->requestServices("shakespeare.lit") };

    test.expect("<iq"
                " id='qx1'"
                " to='shakespeare.lit'"
                " type='get'>"
                "<services xmlns='urn:xmpp:extdisco:2'/>"
                "</iq>");

    test.inject<QString>("<iq"
                         " id='qx1'"
                         " from='shakespeare.lit'"
                         " type='result'>"
                         "<services xmlns='urn:xmpp:extdisco:2'>"
                         "<service host='stun.shakespeare.lit'"
                         " port='9998'"
                         " transport='udp'"
                         " type='stun'/>"
                         "<service host='relay.shakespeare.lit'"
                         " password='jj929jkj5sadjfj93v3n'"
                         " port='9999'"
                         " transport='udp'"
                         " type='turn'"
                         " username='nb78932lkjlskjfdb7g8'/>"
                         "<service host='192.0.2.1'"
                         " port='8888'"
                         " transport='udp'"
                         " type='stun'/>"
                         "<service host='192.0.2.1'"
                         " port='8889'"
                         " password='93jn3bakj9s832lrjbbz'"
                         " transport='udp'"
                         " type='turn'"
                         " username='auu98sjl2wk3e9fjdsl7'/>"
                         "<service host='ftp.shakespeare.lit'"
                         " name='Shakespearean File Server'"
                         " password='guest'"
                         " port='20'"
                         " transport='tcp'"
                         " type='ftp'"
                         " username='guest'/>"
                         "</services>"
                         "</iq>");

    const auto items { expectFutureVariant<QList<QXmppExternalService>>(future.toFuture(this)) };

    QCOMPARE(items.size(), 5);
    QCOMPARE(items.at(0).host(), u"stun.shakespeare.lit"_s);
    QCOMPARE(items.at(4).host(), u"ftp.shakespeare.lit"_s);
}

void tst_QXmppExternalServiceDiscoveryManager::testDiscoveryFeatures()
{
    TestClient test;
    auto *m = test.addNewExtension<QXmppExternalServiceDiscoveryManager>();

    QVERIFY(m->discoveryFeatures().contains(u"urn:xmpp:extdisco:2"));
}

// ============================================================

namespace ExternalServiceDiscoveryIq {

class tst_QXmppExternalServiceDiscoveryIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void esdIsExternalService_data();
    Q_SLOT void esdIsExternalService();
    Q_SLOT void esdBase();
    Q_SLOT void esdIsExternalServiceDiscoveryIq_data();
    Q_SLOT void esdIsExternalServiceDiscoveryIq();
    Q_SLOT void esdIqBase();
};

void tst_QXmppExternalServiceDiscoveryIq::esdIsExternalService_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit' type='stun'/>")
        << true;
    QTest::newRow("invalidHost")
        << QByteArrayLiteral("<service type='stun'/>")
        << false;
    QTest::newRow("invalidHostEmpty")
        << QByteArrayLiteral("<service type='stun' host=''/>")
        << false;
    QTest::newRow("invalidType")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit'/>")
        << false;
    QTest::newRow("invalidTypeEmpty")
        << QByteArrayLiteral("<service host='stun.shakespeare.lit' type=''/>")
        << false;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid host='stun.shakespeare.lit' type='stun'/>")
        << false;
    QTest::newRow("invalidTag")
        << QByteArrayLiteral("<invalid/>")
        << false;
}

void tst_QXmppExternalServiceDiscoveryIq::esdIsExternalService()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppExternalService::isExternalService(xmlToDom(xml)), isValid);
}

void tst_QXmppExternalServiceDiscoveryIq::esdBase()
{
    QByteArray xml { QByteArrayLiteral(
        "<service host='stun.shakespeare.lit'"
        " type='stun'"
        " port='9998'"
        " transport='udp'/>") };

    QXmppExternalService service;
    parsePacket(service, xml);
    QCOMPARE(service.host(), "stun.shakespeare.lit");
    QCOMPARE(service.port(), 9998);
    QCOMPARE(service.transport().has_value(), true);
    QCOMPARE(service.transport().value(), QXmppExternalService::Transport::Udp);
    QCOMPARE(service.type(), "stun");
    serializePacket(service, xml);
}

void tst_QXmppExternalServiceDiscoveryIq::esdIsExternalServiceDiscoveryIq_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<services xmlns='urn:xmpp:extdisco:2'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</services>"
               "</iq>")
        << true;

    QTest::newRow("invalidTag")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<invalid xmlns='urn:xmpp:extdisco:2'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</invalid>"
               "</iq>")
        << false;

    QTest::newRow("invalidNamespace")
        << QByteArrayLiteral(
               "<iq from='shakespeare.lit'"
               " id='ul2bc7y6'"
               " to='bard@shakespeare.lit/globe'"
               " type='result'>"
               "<services xmlns='invalid'>"
               "<service host='stun.shakespeare.lit'"
               " type='stun'"
               " port='9998'"
               " transport='udp'/>"
               "</services>"
               "</iq>")
        << false;
}

void tst_QXmppExternalServiceDiscoveryIq::esdIsExternalServiceDiscoveryIq()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppExternalServiceDiscoveryIq::isExternalServiceDiscoveryIq(xmlToDom(xml)), isValid);
    QT_WARNING_POP
}

void tst_QXmppExternalServiceDiscoveryIq::esdIqBase()
{
    QXmpp::Private::globalStanzaIdCounter = 0;

    const QByteArray xml { QByteArrayLiteral(
        "<iq"
        " id='qx2'"
        " type='result'>"
        "<services xmlns='urn:xmpp:extdisco:2'>"
        "<service host='stun.shakespeare.lit'"
        " type='stun'"
        " port='9998'"
        " transport='udp'/>"
        "<service host='relay.shakespeare.lit'"
        " type='turn'"
        " password='jj929jkj5sadjfj93v3n'"
        " port='9999'"
        " transport='udp'"
        " username='nb78932lkjlskjfdb7g8'/>"
        "<service host='192.0.2.1'"
        " type='stun'"
        " port='8888'"
        " transport='udp'/>"
        "<service host='192.0.2.1'"
        " type='turn'"
        " password='93jn3bakj9s832lrjbbz'"
        " port='8889'"
        " transport='udp'"
        " username='auu98sjl2wk3e9fjdsl7'/>"
        "<service host='ftp.shakespeare.lit'"
        " type='ftp'"
        " name='Shakespearean File Server'"
        " password='guest'"
        " port='20'"
        " transport='tcp'"
        " username='guest'/>"
        "</services>"
        "</iq>") };

    QXmppExternalServiceDiscoveryIq iq1;
    iq1.setType(QXmppIq::Result);

    parsePacket(iq1, xml);
    QCOMPARE(iq1.externalServices().length(), 5);
    serializePacket(iq1, xml);

    QXmppExternalService service1;
    service1.setHost("127.0.0.1");
    service1.setType("ftp");

    iq1.addExternalService(service1);

    QXmppExternalService service2;
    service2.setHost("127.0.0.1");
    service2.setType("ftp");

    iq1.addExternalService(service2);

    QCOMPARE(iq1.externalServices().length(), 7);

    const QByteArray xml2 { QByteArrayLiteral(
        "<iq"
        " id='qx2'"
        " type='result'>"
        "<services xmlns='urn:xmpp:extdisco:2'>"
        "<service host='193.169.1.256'"
        " type='turn'/>"
        "<service host='194.170.2.257'"
        " type='stun'/>"
        "<service host='195.171.3.258'"
        " type='ftp'/>"
        "</services>"
        "</iq>") };

    QXmppExternalServiceDiscoveryIq iq2;
    iq2.setType(QXmppIq::Result);

    QXmppExternalService service3;
    service3.setHost("193.169.1.256");
    service3.setType("turn");
    QXmppExternalService service4;
    service4.setHost("194.170.2.257");
    service4.setType("stun");
    QXmppExternalService service5;
    service5.setHost("195.171.3.258");
    service5.setType("ftp");

    iq2.setExternalServices({ service3, service4, service5 });

    QCOMPARE(iq2.externalServices().length(), 3);
    serializePacket(iq2, xml2);
}

}  // namespace ExternalServiceDiscoveryIq

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = runTests<tst_QXmppJingleMessageInitiationManager,
                          tst_QXmppCallInviteManager,
                          tst_QXmppExternalServiceDiscoveryManager, ExternalServiceDiscoveryIq::tst_QXmppExternalServiceDiscoveryIq>(argc, argv);
#ifdef WITH_GSTREAMER
    status |= runTests<tst_QXmppCallManager>(argc, argv);
#endif
    return status;
}

#include "tst_QXmppCalls.moc"

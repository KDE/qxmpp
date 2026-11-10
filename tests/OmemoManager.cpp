// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the OMEMO manager and its storage. Merging
// tst_QXmppOmemoManager and tst_QXmppOmemoMemoryStorage into one translation
// unit parses the shared Qt/QXmpp headers once instead of once per file. Each
// original test keeps its own namespace; main() runs them in turn.

#include "QXmppAtmManager.h"
#include "QXmppAtmTrustMemoryStorage.h"
#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryIq.h"
#include "QXmppCarbonManagerV2.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMessage.h"
#include "QXmppMessageContext.h"
#include "QXmppMessages.h"
#include "QXmppMucForms.h"
#include "QXmppMucManagerV2.h"
#include "QXmppOmemoElement_p.h"
#include "QXmppOmemoManager.h"
#include "QXmppOmemoManager_p.h"
#include "QXmppOmemoMemoryStorage.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils.h"

#include "IntegrationTesting.h"
#include "StringLiterals.h"
#include "util.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QObject>
#include <QScopeGuard>
#include <QSignalSpy>

using namespace QXmpp;
using namespace QXmpp::Private;

struct OmemoUser {
    QXmppClient client;
    QXmppLogger logger;
    QXmppOmemoManager *manager = nullptr;
    QXmppCarbonManagerV2 *carbonManager = nullptr;
    QXmppMucManagerV2 *mucManager = nullptr;
    QXmppDiscoveryManager *discoveryManager = nullptr;
    QXmppPubSubManager *pubSubManager = nullptr;
    std::unique_ptr<QXmppOmemoMemoryStorage> omemoStorage;
    std::unique_ptr<QXmppAtmTrustStorage> trustStorage;
    QXmppAtmManager *trustManager = nullptr;
};

class OmemoIqHandler : public QXmppClientExtension
{
public:
    OmemoIqHandler(const QXmppBitsOfBinaryIq &requestIq, const QXmppBitsOfBinaryIq &responseIq)
        : m_requestIq(requestIq),
          m_responseIq(responseIq)
    {
    }

    bool handleStanza(const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata) override
    {
        if (isIqElement<QXmppBitsOfBinaryIq>(stanza)) {
            QXmppBitsOfBinaryIq iq;
            iq.parse(stanza);

            if (iq.cid().toContentId() != m_requestIq.cid().toContentId()) {
                return false;
            }

            m_responseIq.setId(iq.id());
            client()->reply(std::move(m_responseIq), e2eeMetadata);
            return true;
        }

        return false;
    }

private:
    QXmppBitsOfBinaryIq m_requestIq;
    QXmppBitsOfBinaryIq m_responseIq;
};

class tst_QXmppOmemoManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void testSecurityPolicies();
    Q_SLOT void testTrustLevels();
    Q_SLOT void initOmemoUser(OmemoUser &omemoUser);
    Q_SLOT void testInit();
    Q_SLOT void testSetUp();
    Q_SLOT void testLoad();
    Q_SLOT void testSendMessage();
    Q_SLOT void testSendIq();
    Q_SLOT void testMucMessage();
    Q_SLOT void finish(OmemoUser &omemoUser);

    OmemoUser m_alice1;
    OmemoUser m_alice2;
    OmemoUser m_bob;
};

void tst_QXmppOmemoManager::initTestCase()
{
    initOmemoUser(m_alice1);
    initOmemoUser(m_alice2);
    initOmemoUser(m_bob);
}

void tst_QXmppOmemoManager::testSecurityPolicies()
{
    auto futureSecurityPolicy = m_alice1.manager->securityPolicy();
    QVERIFY(futureSecurityPolicy.isFinished());
    auto resultSecurityPolicy = futureSecurityPolicy.result();
    QCOMPARE(resultSecurityPolicy, NoSecurityPolicy);

    m_alice1.manager->setSecurityPolicy(Toakafa);

    futureSecurityPolicy = m_alice1.manager->securityPolicy();
    QVERIFY(futureSecurityPolicy.isFinished());
    resultSecurityPolicy = futureSecurityPolicy.result();
    QCOMPARE(resultSecurityPolicy, Toakafa);
}

void tst_QXmppOmemoManager::testTrustLevels()
{
    auto futureTrustLevel = m_alice1.manager->trustLevel(u"alice@example.org"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")));
    QVERIFY(futureTrustLevel.isFinished());
    auto resultTrustLevel = futureTrustLevel.result();
    QCOMPARE(resultTrustLevel, TrustLevel::Undecided);

    m_alice1.manager->setTrustLevel(
        { { u"alice@example.org"_s,
            QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")) },
          { u"bob@example.com"_s,
            QByteArray::fromBase64(QByteArrayLiteral("9E51lG3vVmUn8CM7/AIcmIlLP2HPl6Ao0/VSf4VT/oA=")) } },
        TrustLevel::Authenticated);

    futureTrustLevel = m_alice1.manager->trustLevel(u"alice@example.org"_s,
                                                    QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")));
    QVERIFY(futureTrustLevel.isFinished());
    resultTrustLevel = futureTrustLevel.result();
    QCOMPARE(resultTrustLevel, TrustLevel::Authenticated);
}

void tst_QXmppOmemoManager::initOmemoUser(OmemoUser &omemoUser)
{
    omemoUser.discoveryManager = new QXmppDiscoveryManager;
    omemoUser.client.addExtension(omemoUser.discoveryManager);

    omemoUser.pubSubManager = new QXmppPubSubManager;
    omemoUser.client.addExtension(omemoUser.pubSubManager);

    omemoUser.trustStorage = std::make_unique<QXmppAtmTrustMemoryStorage>();
    omemoUser.trustManager = new QXmppAtmManager(omemoUser.trustStorage.get());
    omemoUser.client.addExtension(omemoUser.trustManager);

    omemoUser.omemoStorage = std::make_unique<QXmppOmemoMemoryStorage>();
    omemoUser.manager = new QXmppOmemoManager(omemoUser.omemoStorage.get());
    omemoUser.client.addExtension(omemoUser.manager);
    omemoUser.client.setEncryptionExtension(omemoUser.manager);

    omemoUser.carbonManager = new QXmppCarbonManagerV2;
    omemoUser.client.addExtension(omemoUser.carbonManager);

    omemoUser.mucManager = new QXmppMucManagerV2;
    omemoUser.client.addExtension(omemoUser.mucManager);

    omemoUser.logger.setLoggingType(QXmppLogger::SignalLogging);
    omemoUser.client.setLogger(&omemoUser.logger);
}

void tst_QXmppOmemoManager::testInit()
{
    auto omemoStorage = std::make_unique<QXmppOmemoMemoryStorage>();
    auto manager = std::make_unique<QXmppOmemoManager>(omemoStorage.get());
    QVERIFY(manager->d->initGlobalContext());
    QVERIFY(manager->d->initLocking());
    QVERIFY(manager->d->initCryptoProvider());
    // TODO: Test initStores()
}

void tst_QXmppOmemoManager::testSetUp()
{
    SKIP_IF_INTEGRATION_TESTS_DISABLED()

    auto isManagerSetUp = false;
    const QObject context;

    connect(&m_alice1.client, &QXmppClient::connected, &context, [&] {
        auto future = m_alice1.manager->setUp();
        future.then(this, [&isManagerSetUp](bool isSetUp) {
            if (isSetUp) {
                isManagerSetUp = true;
            }
        });
    });

    connect(&m_alice1.logger, &QXmppLogger::message, &context, [=](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            qDebug() << "SENT: " << text;
        } else {
            qDebug() << "RECEIVED: " << text;
        }
    });

    m_alice1.client.connectToServer(IntegrationTests::clientConfiguration());

    QTRY_VERIFY(isManagerSetUp);
    finish(m_alice1);
}

void tst_QXmppOmemoManager::testLoad()
{
    auto future = m_alice1.manager->load();
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }
    auto result = future.result();
    QVERIFY(!result);

    const QXmppOmemoStorage::OwnDevice ownDevice = { 1,
                                                     u"notebook"_s,
                                                     QByteArray::fromBase64(QByteArrayLiteral("OU5HM3loYnFjZVVaYmpSbHdab0FPTDhJVHRzUFVUcFMK")),
                                                     QByteArray::fromBase64(QByteArrayLiteral("TkhodEZ6cnFDeGtENWRuT1ZZdUsyaGIwQkRPdHFRSE8K")),
                                                     2,
                                                     3 };
    m_alice1.omemoStorage->setOwnDevice(ownDevice);
    m_alice1.omemoStorage->addSignedPreKeyPair(2,
                                               { QDateTime::currentDateTimeUtc(),
                                                 QByteArray::fromBase64(QByteArrayLiteral("VEZBOTZFRjNQSVRzVE1OcnIzYmV2ZFFuM0R3WmduUWwK")) });
    m_alice1.omemoStorage->addPreKeyPairs({ { 3,
                                              QByteArray::fromBase64(QByteArrayLiteral("RmVmQ0RTTzB0Z2R2T0ZjckQ4N29PN01VTGFFMVZjUmIK")) } });

    future = m_alice1.manager->load();
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }
    result = future.result();
    QVERIFY(result);

    const auto storedOwnDevice = m_alice1.manager->ownDevice();
    //    QCOMPARE(storedOwnDevice.keyId(), ownDevice.publicIdentityKey);
    QCOMPARE(storedOwnDevice.label(), ownDevice.label);

    m_alice1.omemoStorage->resetAll();
}

// Waits for \a task and returns its result.
template<typename T>
static T awaitTask(QXmppTask<T> &&task, std::chrono::milliseconds timeout = std::chrono::seconds(20))
{
    QDeadlineTimer deadline(timeout);
    while (!task.isFinished() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    VERIFY2(task.isFinished(), "Task did not finish in time");
    if constexpr (!std::is_void_v<T>) {
        return task.result();
    }
}

// Connects the user and sets up OMEMO, trusting all keys automatically.
static bool connectAndSetUp(OmemoUser &user, const QXmppConfiguration &config)
{
    QSignalSpy connectedSpy(&user.client, &QXmppClient::connected);
    user.client.connectToServer(config);
    if (!connectedSpy.wait(10'000)) {
        return false;
    }
    awaitTask(user.manager->setSecurityPolicy(Toakafa));
    return awaitTask(user.manager->setUp());
}

static bool hasDecryptedMessage(const QSignalSpy &spy, const QString &body)
{
    return std::any_of(spy.begin(), spy.end(), [&](const QList<QVariant> &arguments) {
        const auto message = arguments.at(0).value<QXmppMessage>();
        return message.e2eeMetadata() && message.e2eeMetadata()->encryption() == QXmpp::Omemo2 && message.body() == body;
    });
}

static int devicesAdded(const QSignalSpy &spy, const QString &jid)
{
    return int(std::count_if(spy.begin(), spy.end(), [&](const QList<QVariant> &arguments) {
        return arguments.at(0).toString() == jid;
    }));
}

void tst_QXmppOmemoManager::testSendMessage()
{
    SKIP_IF_INTEGRATION_TESTS_DISABLED()
    if (!IntegrationTests::secondAccountAvailable()) {
        QSKIP("Export 'QXMPP_JID_2' and 'QXMPP_PASSWORD_2' for a second account.");
    }

    const auto configAlice1 = IntegrationTests::clientConfiguration();
    auto configAlice2 = configAlice1;
    configAlice2.setResource(u"second"_s);
    const auto configBob = IntegrationTests::secondClientConfiguration();
    const auto aliceJid = configAlice1.jidBare();
    const auto bobJid = configBob.jidBare();

    QSignalSpy alice1DeviceSpy(m_alice1.manager, &QXmppOmemoManager::deviceAdded);

    QVERIFY(connectAndSetUp(m_alice1, configAlice1));
    QVERIFY(connectAndSetUp(m_bob, configBob));
    QVERIFY(connectAndSetUp(m_alice2, configAlice2));

    // Alice 1 learns the device of Alice 2 from the own device list and the one of Bob by
    // requesting it.
    QTRY_VERIFY_WITH_TIMEOUT(devicesAdded(alice1DeviceSpy, aliceJid) >= 1, 20'000);
    awaitTask(m_alice1.manager->requestDeviceLists({ bobJid }));
    QTRY_VERIFY_WITH_TIMEOUT(devicesAdded(alice1DeviceSpy, bobJid) >= 1, 20'000);

    QSignalSpy bobReceivedSpy(&m_bob.client, &QXmppClient::messageReceived);
    QSignalSpy alice1ReceivedSpy(&m_alice1.client, &QXmppClient::messageReceived);
    QSignalSpy alice2ReceivedSpy(&m_alice2.client, &QXmppClient::messageReceived);

    // Alice 1 to Bob, Alice 2 gets a carbon copy
    QXmppMessage message1;
    message1.setTo(bobJid);
    message1.setBody(u"Hello Bob!"_s);
    QVERIFY(std::holds_alternative<QXmpp::SendSuccess>(awaitTask(m_alice1.client.sendSensitive(std::move(message1)))));

    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(bobReceivedSpy, u"Hello Bob!"_s), 20'000);
    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(alice2ReceivedSpy, u"Hello Bob!"_s), 20'000);

    // Bob to both devices of Alice
    // Bob knows the device of Alice 1 from her message and learns the one of Alice 2.
    awaitTask(m_bob.manager->requestDeviceLists({ aliceJid }));
    QTRY_VERIFY_WITH_TIMEOUT(awaitTask(m_bob.manager->devices({ aliceJid })).size() >= 2, 20'000);

    QXmppMessage message2;
    message2.setTo(aliceJid);
    message2.setBody(u"Hello Alice!"_s);
    QVERIFY(std::holds_alternative<QXmpp::SendSuccess>(awaitTask(m_bob.client.sendSensitive(std::move(message2)))));

    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(alice1ReceivedSpy, u"Hello Alice!"_s), 20'000);
    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(alice2ReceivedSpy, u"Hello Alice!"_s), 20'000);

    // Bob selects OMEMO via the message pipeline, it is used as e2ee provider
    QObject owner;
    m_bob.client.hooks(&owner).add<QXmpp::Messages::SelectEncryption>([](QXmpp::Messages::EncryptionSelection &selection) {
        selection.setEncryption(QXmpp::E2ee::Omemo2);
    });
    QXmppMessage message3;
    message3.setTo(aliceJid);
    message3.setBody(u"Hello again!"_s);
    QVERIFY(std::holds_alternative<QXmpp::SendSuccess>(awaitTask(m_bob.client.send(std::move(message3)))));

    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(alice1ReceivedSpy, u"Hello again!"_s), 20'000);
    QTRY_VERIFY_WITH_TIMEOUT(hasDecryptedMessage(alice2ReceivedSpy, u"Hello again!"_s), 20'000);
    m_bob.client.hooks(&owner).removeAll();

    // Decrypted messages used only for building sessions (empty OMEMO messages) are not
    // delivered.
    for (const auto *spy : { &bobReceivedSpy, &alice1ReceivedSpy, &alice2ReceivedSpy }) {
        for (const auto &arguments : *spy) {
            const auto message = arguments.at(0).value<QXmppMessage>();
            QVERIFY(!(message.e2eeMetadata() && message.body().isEmpty()));
        }
    }

    finish(m_alice1);
    finish(m_alice2);
    finish(m_bob);
}

void tst_QXmppOmemoManager::testSendIq()
{
    SKIP_IF_INTEGRATION_TESTS_DISABLED()
    if (!IntegrationTests::secondAccountAvailable()) {
        QSKIP("Export 'QXMPP_JID_2' and 'QXMPP_PASSWORD_2' for a second account.");
    }

    auto configAlice = IntegrationTests::clientConfiguration();
    configAlice.setResource(u"first"_s);
    auto configBob = IntegrationTests::secondClientConfiguration();
    configBob.setResource(u"phone"_s);
    const auto bobJid = configBob.jidBare();

    QXmppBitsOfBinaryIq requestIq;
    requestIq.setTo(configBob.jid());
    requestIq.setCid(QXmppBitsOfBinaryContentId::fromContentId(u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s));

    QXmppBitsOfBinaryIq responseIq;
    responseIq.setType(QXmppIq::Result);
    responseIq.setTo(configAlice.jid());
    responseIq.setData(QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAMAAAC67D+PAAAAclBMVEUAAADYZArfaA9GIAoBAAGN"
        "QA3MXgniaAiEOgZMIATDXRXZZhHUZBHIXhDrbQ6sUQ7OYA2TRAubRwqMQQq7VQlKHgMAAAK5WRfJ"
        "YBOORBFoMBCwUQ/ycA6FPgvbZQpeKglNJQmrTQeOPgQyFwR6MwACAABRPE/oAAAAW0lEQVQI1xXI"
        "Rw6EMBTAUP8kJKENnaF37n9FQPLCekAgzklhgCwfrlNHEXhrvCsxaU/SwLGAFuIWZFpBERtKm9Xf"
        "JqH+vVWh4POqgHrsAtht095b+geYRSl57QHSPgP3+CwvAAAAAABJRU5ErkJggg==")));

    QVERIFY(connectAndSetUp(m_alice1, configAlice));
    QVERIFY(connectAndSetUp(m_bob, configBob));

    awaitTask(m_alice1.manager->requestDeviceLists({ bobJid }));
    QTRY_VERIFY_WITH_TIMEOUT(!awaitTask(m_alice1.manager->devices({ bobJid })).isEmpty(), 20'000);

    // Bob can't handle the request and replies with an encrypted error.
    auto request = requestIq;
    auto result = awaitTask(m_alice1.client.sendSensitiveIq(std::move(request)));
    auto stanzaError = expectVariant<QXmppError>(std::move(result)).value<QXmppStanza::Error>();
    QVERIFY(stanzaError.has_value());
    // The error is in the encrypted content, but QXmppClient::sendSensitiveIq() reports error
    // responses before decrypting them.
    QEXPECT_FAIL("", "Encrypted error responses are not decrypted", Continue);
    QCOMPARE(stanzaError->condition(), QXmppStanza::Error::FeatureNotImplemented);

    // Bob handles the request and replies with the encrypted result.
    auto *iqHandler = new OmemoIqHandler(requestIq, responseIq);
    m_bob.client.addExtension(iqHandler);
    auto cleanup = qScopeGuard([&] { m_bob.client.removeExtension(iqHandler); });

    request = requestIq;
    result = awaitTask(m_alice1.client.sendSensitiveIq(std::move(request)));
    // QXmppClient::sendSensitiveIq() and QXmppOmemoManager::encryptIq() copy the request into a
    // QXmppIq, which loses the payload of subclasses.
    QEXPECT_FAIL("", "The payload of encrypted IQs is lost", Continue);
    QVERIFY(std::holds_alternative<QDomElement>(result));
    if (const auto *response = std::get_if<QDomElement>(&result)) {
        QVERIFY(isIqElement<QXmppBitsOfBinaryIq>(*response));
        QXmppBitsOfBinaryIq resultIq;
        resultIq.parse(*response);
        QCOMPARE(resultIq.data(), responseIq.data());
    }

    finish(m_alice1);
    finish(m_bob);
}

void tst_QXmppOmemoManager::testMucMessage()
{
    SKIP_IF_INTEGRATION_TESTS_DISABLED()
    if (!IntegrationTests::secondAccountAvailable()) {
        QSKIP("Export 'QXMPP_JID_2' and 'QXMPP_PASSWORD_2' for a second account.");
    }
    const auto mucService = qEnvironmentVariable("QXMPP_TESTS_MUC_SERVICE");
    if (mucService.isEmpty()) {
        QSKIP("Export 'QXMPP_TESTS_MUC_SERVICE' with the JID of a MUC service.");
    }

    const auto configAlice = IntegrationTests::clientConfiguration();
    const auto configBob = IntegrationTests::secondClientConfiguration();
    QVERIFY(connectAndSetUp(m_alice1, configAlice));
    QVERIFY(connectAndSetUp(m_bob, configBob));
    awaitTask(m_alice1.manager->requestDeviceLists({ configBob.jidBare() }));
    awaitTask(m_bob.manager->requestDeviceLists({ configAlice.jidBare() }));
    QTRY_VERIFY_WITH_TIMEOUT(!awaitTask(m_alice1.manager->devices({ configBob.jidBare() })).isEmpty(), 20'000);
    QTRY_VERIFY_WITH_TIMEOUT(!awaitTask(m_bob.manager->devices({ configAlice.jidBare() })).isEmpty(), 20'000);

    // non-anonymous room, as needed for OMEMO
    auto created = awaitTask(m_alice1.mucManager->createRoom(mucService, u"alice"_s, QXmppUtils::generateStanzaUuid().toLower()));
    auto room = expectVariant<QXmppMucRoomV2>(std::move(created));
    QTRY_VERIFY_WITH_TIMEOUT(room.roomConfig().value().has_value(), 20'000);
    auto config = *room.roomConfig().value();
    config.setWhoCanDiscoverJids(QXmppMucRoomConfig::WhoCanDiscoverJids::Anyone);
    expectVariant<QXmpp::Success>(awaitTask(room.setRoomConfig(config)));
    auto cleanup = qScopeGuard([&] { awaitTask(room.destroyRoom()); });

    expectVariant<QXmppMucRoomV2>(awaitTask(m_bob.mucManager->joinRoom(room.jid(), u"bob"_s)));
    QTRY_VERIFY_WITH_TIMEOUT(room.participants().size() >= 2, 20'000);

    // the sender is resolved before the decryption
    QStringList bobSenders;
    QObject owner;
    m_bob.client.hooks(&owner).add<QXmpp::Messages::Received>([&](QXmpp::Messages::Context &context) {
        if (context.decryptionState() == QXmpp::Messages::DecryptionState::Decrypted) {
            bobSenders << context.senderJid();
        }
        return QXmpp::HookResult::Continue;
    });
    QSignalSpy bobReceivedSpy(m_bob.mucManager, &QXmppMucManagerV2::messageReceived);
    m_alice1.client.hooks(&owner).add<QXmpp::Messages::SelectEncryption>([](QXmpp::Messages::EncryptionSelection &selection) {
        selection.setEncryption(QXmpp::E2ee::Omemo2);
    });

    // encrypted for the occupants, finishes with the reflection of the room
    QXmppMessage message;
    message.setBody(u"Hello room!"_s);
    expectVariant<QXmpp::Success>(awaitTask(room.sendMessage(std::move(message))));

    // Bob decrypts it with the keys of the real sender
    QTRY_VERIFY_WITH_TIMEOUT(std::ranges::any_of(bobReceivedSpy, [](const QList<QVariant> &arguments) {
                                 const auto message = arguments.at(1).value<QXmppMessage>();
                                 return message.e2eeMetadata() && message.body() == u"Hello room!";
                             }),
                             20'000);
    QCOMPARE(bobSenders, QStringList { configAlice.jidBare() });

    m_alice1.client.hooks(&owner).removeAll();
    m_bob.client.hooks(&owner).removeAll();
    cleanup.dismiss();
    awaitTask(room.destroyRoom());
    finish(m_alice1);
    finish(m_bob);
}

void tst_QXmppOmemoManager::finish(OmemoUser &omemoUser)
{
    QSignalSpy disconnectedSpy(&omemoUser.client, &QXmppClient::disconnected);

    bool isManagerReset = false;

    auto future = omemoUser.manager->resetAll();
    future.then(this, [=, &isManagerReset, &omemoUser](bool isReset) {
        if (isReset) {
            isManagerReset = true;
        }

        omemoUser.client.disconnectFromServer();
    });

    QVERIFY2(disconnectedSpy.wait(), "Could not disconnect from server!");
    QTRY_VERIFY(isManagerReset);
}

// ============================================================

class tst_QXmppOmemoMemoryStorage : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testOwnDevice();
    Q_SLOT void testSignedPreKeyPairs();
    Q_SLOT void testPreKeyPairs();
    Q_SLOT void testDevices();
    Q_SLOT void testResetAll();

    QXmppOmemoMemoryStorage m_omemoStorage;
};

void tst_QXmppOmemoMemoryStorage::testOwnDevice()
{
    auto future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    auto optionalResult = future.result().ownDevice;
    QVERIFY(!optionalResult);

    QXmppOmemoStorage::OwnDevice ownDevice;

    m_omemoStorage.setOwnDevice(ownDevice);

    // Check the default values.
    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    optionalResult = future.result().ownDevice;
    QVERIFY(optionalResult);
    auto result = optionalResult.value();
    QCOMPARE(result.id, 0);
    QVERIFY(result.label.isEmpty());
    QVERIFY(result.privateIdentityKey.isEmpty());
    QVERIFY(result.publicIdentityKey.isEmpty());
    QCOMPARE(result.latestSignedPreKeyId, 1);
    QCOMPARE(result.latestPreKeyId, 1);

    ownDevice.id = 1;
    ownDevice.label = u"Notebook"_s;
    ownDevice.privateIdentityKey = QByteArray::fromBase64(QByteArrayLiteral("ZDVNZFdJeFFUa3N6ZWdSUG9scUdoQXFpWERGbHRsZTIK"));
    ownDevice.publicIdentityKey = QByteArray::fromBase64(QByteArrayLiteral("dUsxSTJyM2tKVHE1TzNXbk1Xd0tpMGY0TnFleDRYUGkK"));
    ownDevice.latestSignedPreKeyId = 2;
    ownDevice.latestPreKeyId = 100;

    m_omemoStorage.setOwnDevice(ownDevice);

    // Check the set values.
    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    optionalResult = future.result().ownDevice;
    QVERIFY(optionalResult);
    result = optionalResult.value();
    QCOMPARE(result.id, 1);
    QCOMPARE(result.label, u"Notebook"_s);
    QCOMPARE(result.privateIdentityKey, QByteArray::fromBase64(QByteArrayLiteral("ZDVNZFdJeFFUa3N6ZWdSUG9scUdoQXFpWERGbHRsZTIK")));
    QCOMPARE(result.publicIdentityKey, QByteArray::fromBase64(QByteArrayLiteral("dUsxSTJyM2tKVHE1TzNXbk1Xd0tpMGY0TnFleDRYUGkK")));
    QCOMPARE(result.latestSignedPreKeyId, 2);
    QCOMPARE(result.latestPreKeyId, 100);
}

void tst_QXmppOmemoMemoryStorage::testSignedPreKeyPairs()
{
    auto future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    auto result = future.result().signedPreKeyPairs;
    QVERIFY(result.isEmpty());

    QXmppOmemoStorage::SignedPreKeyPair signedPreKeyPair1;
    signedPreKeyPair1.creationDate = QDateTime(QDate(2022, 01, 01), QTime());
    signedPreKeyPair1.data = QByteArrayLiteral("FaZmWjwqppAoMff72qTzUIktGUbi4pAmds1Cuh6OElmi");

    QXmppOmemoStorage::SignedPreKeyPair signedPreKeyPair2;
    signedPreKeyPair2.creationDate = QDateTime(QDate(2022, 01, 02), QTime());
    signedPreKeyPair2.data = QByteArrayLiteral("jsrj4UYQqaHJrlysNu0uoHgmAU8ffknPpwKJhdqLYgIU");

    QHash<uint32_t, QXmppOmemoStorage::SignedPreKeyPair> signedPreKeyPairs = { { 1, signedPreKeyPair1 },
                                                                               { 2, signedPreKeyPair2 } };

    m_omemoStorage.addSignedPreKeyPair(1, signedPreKeyPair1);
    m_omemoStorage.addSignedPreKeyPair(2, signedPreKeyPair2);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().signedPreKeyPairs;
    const auto signedPreKeyPairResult1 = result.value(1);
    QCOMPARE(signedPreKeyPairResult1.creationDate, QDateTime(QDate(2022, 01, 01), QTime()));
    QCOMPARE(signedPreKeyPairResult1.data, QByteArrayLiteral("FaZmWjwqppAoMff72qTzUIktGUbi4pAmds1Cuh6OElmi"));
    const auto signedPreKeyPairResult2 = result.value(2);
    QCOMPARE(signedPreKeyPairResult2.creationDate, QDateTime(QDate(2022, 01, 02), QTime()));
    QCOMPARE(signedPreKeyPairResult2.data, QByteArrayLiteral("jsrj4UYQqaHJrlysNu0uoHgmAU8ffknPpwKJhdqLYgIU"));

    signedPreKeyPairs.remove(1);
    m_omemoStorage.removeSignedPreKeyPair(1);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().signedPreKeyPairs;
    const auto signedPreKeyPairResult = result.value(2);
    QCOMPARE(signedPreKeyPairResult.creationDate, QDateTime(QDate(2022, 01, 02), QTime()));
    QCOMPARE(signedPreKeyPairResult.data, QByteArrayLiteral("jsrj4UYQqaHJrlysNu0uoHgmAU8ffknPpwKJhdqLYgIU"));
}

void tst_QXmppOmemoMemoryStorage::testPreKeyPairs()
{
    auto future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    auto result = future.result().preKeyPairs;
    QVERIFY(result.isEmpty());

    const QHash<uint32_t, QByteArray> preKeyPairs1 = { { 1, QByteArrayLiteral("RZLgD0lmL2WpJbskbGKFRMZL4zqSSvU0rElmO7UwGSVt") },
                                                       { 2, QByteArrayLiteral("3PGPNsf9P7pPitp9dt2uvZYT4HkxdHJAbWqLvOPXUeca") } };
    const QHash<uint32_t, QByteArray> preKeyPairs2 = { { 3, QByteArrayLiteral("LpLBVXejfU4d0qcPOJCRNDDg9IMbOujpV3UTYtZU9LTy") } };

    QHash<uint32_t, QByteArray> preKeyPairs;
    preKeyPairs.insert(preKeyPairs1);
    preKeyPairs.insert(preKeyPairs2);

    m_omemoStorage.addPreKeyPairs(preKeyPairs1);
    m_omemoStorage.addPreKeyPairs(preKeyPairs2);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().preKeyPairs;
    QCOMPARE(result, preKeyPairs);

    preKeyPairs.remove(1);
    m_omemoStorage.removePreKeyPair(1);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().preKeyPairs;
    QCOMPARE(result, preKeyPairs);
}

void tst_QXmppOmemoMemoryStorage::testDevices()
{
    auto future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    auto result = future.result().devices;
    QVERIFY(result.isEmpty());

    QXmppOmemoStorage::Device deviceAlice;
    deviceAlice.label = u"Desktop"_s;
    deviceAlice.keyId = QByteArray::fromBase64(QByteArrayLiteral("bEFLaDRQRkFlYXdyakE2aURoN0wyMzk2NTJEM2hRMgo="));
    deviceAlice.session = QByteArray::fromBase64(QByteArrayLiteral("Cs8CCAQSIQWIhBRMdJ80tLVT7ius0H1LutRLeXBid68NH90M/kwhGxohBT+2kM/wVQ2UrZZPJBRmGZP0ZoCCWiET7KxA3ieAa888IiBSTWnp4qrTeo7z9kfKRaAFy+fYwPBI2HCSOxfC0anyPigAMmsKIQXZ95Xs7I+tOsg76eLtp266XTuCF8STa+VZkXPPJ00WSRIgmJ73wjhXPZqIt9ofB0NVwbWOKnYzQ90SHJEd/hyBHkUaJAgAEiDxXDT00+zpJd+TKJrD6nWQxQZhB8I7vCRdD/Oxw61MYjpJCiEFmTV1l+cOLEytoTp17VOEunYlCZmDqn/qoUYI/8P9ZQsaJAgBEiB/QP+9Lb0YOhSQmIr/X75Vs1FME1qzmohSzqBVTzbfZFCnf1jsR2AAaiEFPxj3VK+knGrndOjcgMXI4wEfH/0VrbgJqobGWbewYyA="));
    deviceAlice.unrespondedSentStanzasCount = 10;
    deviceAlice.unrespondedReceivedStanzasCount = 11;
    deviceAlice.removalFromDeviceListDate = QDateTime(QDate(2022, 01, 01), QTime());

    QXmppOmemoStorage::Device deviceBob1;
    deviceBob1.label = u"Phone"_s;
    deviceBob1.keyId = QByteArray::fromBase64(QByteArrayLiteral("WTV6c3B2UFhYbE9OQ1d0N0ZScUhLWXpmYnY2emJoego="));
    deviceBob1.session = QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQXZwE+G9R6ECMxKWPMidwcx3lPboUT2KEoea3B2T3vjUBohBQ7qW+Fb9Gi/SLsuQTv2TRixF0zLx2/mw0V4arjYSmgHIiCwuvEP2eyFU7FsbtSZBWKt+hH/DwBF7C0WrfxDrSu1bSgAMmsKIQXm5tRa73ZcUWn7fQa2YlDv+yLw1copPjdRZCrGcK7cNRIg0OXBvqBTAfyiUlLKW3LDIiSMHkRYYWDyknSJz3s+81oaJAgAEiAQlSKV+70EMYAjjW88dO52dp9e/aDhT8YUDHNFaCFUxTpJCiEF2OE4fb7Quwg0PMeJfT1uXmq/YXVaos9A7bn37TySiWkaJAgAEiDJlr5w0mBHBHZzttfVyvd2y2IzBV7bGdoX+lKHaEGIoUonCAwSIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECRgCUMgnWMgnYABqIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECQ=="));
    deviceBob1.unrespondedSentStanzasCount = 20;
    deviceBob1.unrespondedReceivedStanzasCount = 21;
    deviceBob1.removalFromDeviceListDate = QDateTime(QDate(2022, 01, 02), QTime());

    QXmppOmemoStorage::Device deviceBob2;
    deviceBob2.label = u"Tablet"_s;
    deviceBob2.keyId = QByteArray::fromBase64(QByteArrayLiteral("U0tXcUlSVHVISzZLYUdGcW53czBtdXYxTEt2blVsbQo="));
    deviceBob2.session = QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQU/tpDP8FUNlK2WTyQUZhmT9GaAglohE+ysQN4ngGvPPBohBdnAT4b1HoQIzEpY8yJ3BzHeU9uhRPYoSh5rcHZPe+NQIiBNmwyjLm5xdbf5f9ab9AASopfdiSybMFMdS4SQR5pSTygAMmsKIQW5FhVKpKUzKlhUCfoCmMwoo5jUFn7+NrcOQl6CQYraZRIgkNHGSWgeoLUvYMM8wsgqU4RUv8ymv/Kv4LLJb8q4vlEaJAgAEiA/GmWir7/6tWyOTrGXsehUnnPZhFs6zGvTDNe1LZaIeTpJCiEFa7t/sVQV2uofS36GbijY63d2B4yJKFGDu6K96cU5PFsaJAgAEiA6kX2jqwfZkN0AmNOZGLPg9J8ryrSSpo74DxU85z0q/konCE4SIQWZRzzFf3M1/gzbg9/xUsNcyiUnr5jAjLpSPOj7BOW6BBgCUKd/WKd/YABqIQWZRzzFf3M1/gzbg9/xUsNcyiUnr5jAjLpSPOj7BOW6BA=="));
    deviceBob2.unrespondedSentStanzasCount = 30;
    deviceBob2.unrespondedReceivedStanzasCount = 31;
    deviceBob2.removalFromDeviceListDate = QDateTime(QDate(2022, 01, 03), QTime());

    m_omemoStorage.addDevice(u"alice@example.org"_s, 1, deviceAlice);
    m_omemoStorage.addDevice(u"bob@example.com"_s, 1, deviceBob1);
    m_omemoStorage.addDevice(u"bob@example.com"_s, 2, deviceBob2);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().devices;
    QCOMPARE(result.size(), 2);

    auto resultDevicesAlice = result.value(u"alice@example.org"_s);
    QCOMPARE(resultDevicesAlice.size(), 1);

    auto resultDeviceAlice = resultDevicesAlice.value(1);
    QCOMPARE(resultDeviceAlice.label, u"Desktop"_s);
    QCOMPARE(resultDeviceAlice.keyId, QByteArray::fromBase64(QByteArrayLiteral("bEFLaDRQRkFlYXdyakE2aURoN0wyMzk2NTJEM2hRMgo=")));
    QCOMPARE(resultDeviceAlice.session, QByteArray::fromBase64(QByteArrayLiteral("Cs8CCAQSIQWIhBRMdJ80tLVT7ius0H1LutRLeXBid68NH90M/kwhGxohBT+2kM/wVQ2UrZZPJBRmGZP0ZoCCWiET7KxA3ieAa888IiBSTWnp4qrTeo7z9kfKRaAFy+fYwPBI2HCSOxfC0anyPigAMmsKIQXZ95Xs7I+tOsg76eLtp266XTuCF8STa+VZkXPPJ00WSRIgmJ73wjhXPZqIt9ofB0NVwbWOKnYzQ90SHJEd/hyBHkUaJAgAEiDxXDT00+zpJd+TKJrD6nWQxQZhB8I7vCRdD/Oxw61MYjpJCiEFmTV1l+cOLEytoTp17VOEunYlCZmDqn/qoUYI/8P9ZQsaJAgBEiB/QP+9Lb0YOhSQmIr/X75Vs1FME1qzmohSzqBVTzbfZFCnf1jsR2AAaiEFPxj3VK+knGrndOjcgMXI4wEfH/0VrbgJqobGWbewYyA=")));
    QCOMPARE(resultDeviceAlice.unrespondedSentStanzasCount, 10);
    QCOMPARE(resultDeviceAlice.unrespondedReceivedStanzasCount, 11);
    QCOMPARE(resultDeviceAlice.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 01), QTime()));

    auto resultDevicesBob = result.value(u"bob@example.com"_s);
    QCOMPARE(resultDevicesBob.size(), 2);

    auto resultDeviceBob1 = resultDevicesBob.value(1);
    QCOMPARE(resultDeviceBob1.label, u"Phone"_s);
    QCOMPARE(resultDeviceBob1.keyId, QByteArray::fromBase64(QByteArrayLiteral("WTV6c3B2UFhYbE9OQ1d0N0ZScUhLWXpmYnY2emJoego=")));
    QCOMPARE(resultDeviceBob1.session, QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQXZwE+G9R6ECMxKWPMidwcx3lPboUT2KEoea3B2T3vjUBohBQ7qW+Fb9Gi/SLsuQTv2TRixF0zLx2/mw0V4arjYSmgHIiCwuvEP2eyFU7FsbtSZBWKt+hH/DwBF7C0WrfxDrSu1bSgAMmsKIQXm5tRa73ZcUWn7fQa2YlDv+yLw1copPjdRZCrGcK7cNRIg0OXBvqBTAfyiUlLKW3LDIiSMHkRYYWDyknSJz3s+81oaJAgAEiAQlSKV+70EMYAjjW88dO52dp9e/aDhT8YUDHNFaCFUxTpJCiEF2OE4fb7Quwg0PMeJfT1uXmq/YXVaos9A7bn37TySiWkaJAgAEiDJlr5w0mBHBHZzttfVyvd2y2IzBV7bGdoX+lKHaEGIoUonCAwSIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECRgCUMgnWMgnYABqIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECQ==")));
    QCOMPARE(resultDeviceBob1.unrespondedSentStanzasCount, 20);
    QCOMPARE(resultDeviceBob1.unrespondedReceivedStanzasCount, 21);
    QCOMPARE(resultDeviceBob1.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 02), QTime()));

    auto resultDeviceBob2 = resultDevicesBob.value(2);
    QCOMPARE(resultDeviceBob2.label, u"Tablet"_s);
    QCOMPARE(resultDeviceBob2.keyId, QByteArray::fromBase64(QByteArrayLiteral("U0tXcUlSVHVISzZLYUdGcW53czBtdXYxTEt2blVsbQo=")));
    QCOMPARE(resultDeviceBob2.session, QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQU/tpDP8FUNlK2WTyQUZhmT9GaAglohE+ysQN4ngGvPPBohBdnAT4b1HoQIzEpY8yJ3BzHeU9uhRPYoSh5rcHZPe+NQIiBNmwyjLm5xdbf5f9ab9AASopfdiSybMFMdS4SQR5pSTygAMmsKIQW5FhVKpKUzKlhUCfoCmMwoo5jUFn7+NrcOQl6CQYraZRIgkNHGSWgeoLUvYMM8wsgqU4RUv8ymv/Kv4LLJb8q4vlEaJAgAEiA/GmWir7/6tWyOTrGXsehUnnPZhFs6zGvTDNe1LZaIeTpJCiEFa7t/sVQV2uofS36GbijY63d2B4yJKFGDu6K96cU5PFsaJAgAEiA6kX2jqwfZkN0AmNOZGLPg9J8ryrSSpo74DxU85z0q/konCE4SIQWZRzzFf3M1/gzbg9/xUsNcyiUnr5jAjLpSPOj7BOW6BBgCUKd/WKd/YABqIQWZRzzFf3M1/gzbg9/xUsNcyiUnr5jAjLpSPOj7BOW6BA==")));
    QCOMPARE(resultDeviceBob2.unrespondedSentStanzasCount, 30);
    QCOMPARE(resultDeviceBob2.unrespondedReceivedStanzasCount, 31);
    QCOMPARE(resultDeviceBob2.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 03), QTime()));

    m_omemoStorage.removeDevice(u"bob@example.com"_s, 2);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().devices;
    QCOMPARE(result.size(), 2);

    resultDevicesAlice = result.value(u"alice@example.org"_s);
    QCOMPARE(resultDevicesAlice.size(), 1);

    resultDeviceAlice = resultDevicesAlice.value(1);
    QCOMPARE(resultDeviceAlice.label, u"Desktop"_s);
    QCOMPARE(resultDeviceAlice.keyId, QByteArray::fromBase64(QByteArrayLiteral("bEFLaDRQRkFlYXdyakE2aURoN0wyMzk2NTJEM2hRMgo=")));
    QCOMPARE(resultDeviceAlice.session, QByteArray::fromBase64(QByteArrayLiteral("Cs8CCAQSIQWIhBRMdJ80tLVT7ius0H1LutRLeXBid68NH90M/kwhGxohBT+2kM/wVQ2UrZZPJBRmGZP0ZoCCWiET7KxA3ieAa888IiBSTWnp4qrTeo7z9kfKRaAFy+fYwPBI2HCSOxfC0anyPigAMmsKIQXZ95Xs7I+tOsg76eLtp266XTuCF8STa+VZkXPPJ00WSRIgmJ73wjhXPZqIt9ofB0NVwbWOKnYzQ90SHJEd/hyBHkUaJAgAEiDxXDT00+zpJd+TKJrD6nWQxQZhB8I7vCRdD/Oxw61MYjpJCiEFmTV1l+cOLEytoTp17VOEunYlCZmDqn/qoUYI/8P9ZQsaJAgBEiB/QP+9Lb0YOhSQmIr/X75Vs1FME1qzmohSzqBVTzbfZFCnf1jsR2AAaiEFPxj3VK+knGrndOjcgMXI4wEfH/0VrbgJqobGWbewYyA=")));
    QCOMPARE(resultDeviceAlice.unrespondedSentStanzasCount, 10);
    QCOMPARE(resultDeviceAlice.unrespondedReceivedStanzasCount, 11);
    QCOMPARE(resultDeviceAlice.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 01), QTime()));

    resultDevicesBob = result.value(u"bob@example.com"_s);
    QCOMPARE(resultDevicesBob.size(), 1);

    resultDeviceBob1 = resultDevicesBob.value(1);
    QCOMPARE(resultDeviceBob1.label, u"Phone"_s);
    QCOMPARE(resultDeviceBob1.keyId, QByteArray::fromBase64(QByteArrayLiteral("WTV6c3B2UFhYbE9OQ1d0N0ZScUhLWXpmYnY2emJoego=")));
    QCOMPARE(resultDeviceBob1.session, QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQXZwE+G9R6ECMxKWPMidwcx3lPboUT2KEoea3B2T3vjUBohBQ7qW+Fb9Gi/SLsuQTv2TRixF0zLx2/mw0V4arjYSmgHIiCwuvEP2eyFU7FsbtSZBWKt+hH/DwBF7C0WrfxDrSu1bSgAMmsKIQXm5tRa73ZcUWn7fQa2YlDv+yLw1copPjdRZCrGcK7cNRIg0OXBvqBTAfyiUlLKW3LDIiSMHkRYYWDyknSJz3s+81oaJAgAEiAQlSKV+70EMYAjjW88dO52dp9e/aDhT8YUDHNFaCFUxTpJCiEF2OE4fb7Quwg0PMeJfT1uXmq/YXVaos9A7bn37TySiWkaJAgAEiDJlr5w0mBHBHZzttfVyvd2y2IzBV7bGdoX+lKHaEGIoUonCAwSIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECRgCUMgnWMgnYABqIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECQ==")));
    QCOMPARE(resultDeviceBob1.unrespondedSentStanzasCount, 20);
    QCOMPARE(resultDeviceBob1.unrespondedReceivedStanzasCount, 21);
    QCOMPARE(resultDeviceBob1.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 02), QTime()));

    m_omemoStorage.removeDevice(u"alice@example.org"_s, 1);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().devices;
    QCOMPARE(result.size(), 1);

    resultDevicesBob = result.value(u"bob@example.com"_s);
    QCOMPARE(resultDevicesBob.size(), 1);

    resultDeviceBob1 = resultDevicesBob.value(1);
    QCOMPARE(resultDeviceBob1.label, u"Phone"_s);
    QCOMPARE(resultDeviceBob1.keyId, QByteArray::fromBase64(QByteArrayLiteral("WTV6c3B2UFhYbE9OQ1d0N0ZScUhLWXpmYnY2emJoego=")));
    QCOMPARE(resultDeviceBob1.session, QByteArray::fromBase64(QByteArrayLiteral("CvgCCAQSIQXZwE+G9R6ECMxKWPMidwcx3lPboUT2KEoea3B2T3vjUBohBQ7qW+Fb9Gi/SLsuQTv2TRixF0zLx2/mw0V4arjYSmgHIiCwuvEP2eyFU7FsbtSZBWKt+hH/DwBF7C0WrfxDrSu1bSgAMmsKIQXm5tRa73ZcUWn7fQa2YlDv+yLw1copPjdRZCrGcK7cNRIg0OXBvqBTAfyiUlLKW3LDIiSMHkRYYWDyknSJz3s+81oaJAgAEiAQlSKV+70EMYAjjW88dO52dp9e/aDhT8YUDHNFaCFUxTpJCiEF2OE4fb7Quwg0PMeJfT1uXmq/YXVaos9A7bn37TySiWkaJAgAEiDJlr5w0mBHBHZzttfVyvd2y2IzBV7bGdoX+lKHaEGIoUonCAwSIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECRgCUMgnWMgnYABqIQXN7Y76Vwcsaubw8EHYaIPnBB11WjEEYcEPalwlgEUECQ==")));
    QCOMPARE(resultDeviceBob1.unrespondedSentStanzasCount, 20);
    QCOMPARE(resultDeviceBob1.unrespondedReceivedStanzasCount, 21);
    QCOMPARE(resultDeviceBob1.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 02), QTime()));

    m_omemoStorage.addDevice(u"alice@example.org"_s, 1, deviceAlice);
    m_omemoStorage.addDevice(u"bob@example.com"_s, 2, deviceBob2);
    m_omemoStorage.removeDevices(u"bob@example.com"_s);

    future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    result = future.result().devices;
    QCOMPARE(result.size(), 1);

    resultDevicesAlice = result.value(u"alice@example.org"_s);
    QCOMPARE(resultDevicesAlice.size(), 1);

    resultDeviceAlice = resultDevicesAlice.value(1);
    QCOMPARE(resultDeviceAlice.label, u"Desktop"_s);
    QCOMPARE(resultDeviceAlice.keyId, QByteArray::fromBase64(QByteArrayLiteral("bEFLaDRQRkFlYXdyakE2aURoN0wyMzk2NTJEM2hRMgo=")));
    QCOMPARE(resultDeviceAlice.session, QByteArray::fromBase64(QByteArrayLiteral("Cs8CCAQSIQWIhBRMdJ80tLVT7ius0H1LutRLeXBid68NH90M/kwhGxohBT+2kM/wVQ2UrZZPJBRmGZP0ZoCCWiET7KxA3ieAa888IiBSTWnp4qrTeo7z9kfKRaAFy+fYwPBI2HCSOxfC0anyPigAMmsKIQXZ95Xs7I+tOsg76eLtp266XTuCF8STa+VZkXPPJ00WSRIgmJ73wjhXPZqIt9ofB0NVwbWOKnYzQ90SHJEd/hyBHkUaJAgAEiDxXDT00+zpJd+TKJrD6nWQxQZhB8I7vCRdD/Oxw61MYjpJCiEFmTV1l+cOLEytoTp17VOEunYlCZmDqn/qoUYI/8P9ZQsaJAgBEiB/QP+9Lb0YOhSQmIr/X75Vs1FME1qzmohSzqBVTzbfZFCnf1jsR2AAaiEFPxj3VK+knGrndOjcgMXI4wEfH/0VrbgJqobGWbewYyA=")));
    QCOMPARE(resultDeviceAlice.unrespondedSentStanzasCount, 10);
    QCOMPARE(resultDeviceAlice.unrespondedReceivedStanzasCount, 11);
    QCOMPARE(resultDeviceAlice.removalFromDeviceListDate, QDateTime(QDate(2022, 01, 01), QTime()));
}

void tst_QXmppOmemoMemoryStorage::testResetAll()
{
    m_omemoStorage.setOwnDevice(QXmppOmemoStorage::OwnDevice());

    QXmppOmemoStorage::SignedPreKeyPair signedPreKeyPair;
    signedPreKeyPair.creationDate = QDateTime(QDate(2022, 01, 01), QTime());
    signedPreKeyPair.data = QByteArrayLiteral("FaZmWjwqppAoMff72qTzUIktGUbi4pAmds1Cuh6OElmi");
    m_omemoStorage.addSignedPreKeyPair(1, signedPreKeyPair);

    m_omemoStorage.addPreKeyPairs({ { 1, QByteArrayLiteral("RZLgD0lmL2WpJbskbGKFRMZL4zqSSvU0rElmO7UwGSVt") },
                                    { 2, QByteArrayLiteral("3PGPNsf9P7pPitp9dt2uvZYT4HkxdHJAbWqLvOPXUeca") } });
    m_omemoStorage.addDevice(u"alice@example.org"_s,
                             123,
                             QXmppOmemoStorage::Device());

    m_omemoStorage.resetAll();

    auto future = m_omemoStorage.allData();
    QVERIFY(future.isFinished());
    auto result = future.result();
    QVERIFY(!result.ownDevice);
    QVERIFY(result.signedPreKeyPairs.isEmpty());
    QVERIFY(result.preKeyPairs.isEmpty());
    QVERIFY(result.devices.isEmpty());
}

QXMPP_TEST_MAIN(tst_QXmppOmemoManager, tst_QXmppOmemoMemoryStorage)

#include "OmemoManager.moc"

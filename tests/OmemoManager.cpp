// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for OMEMO: the managers, their storages and the components of the new
// manager are tested in one translation unit, so the shared Qt/QXmpp headers are parsed once
// instead of once per file. main() runs the test classes in turn.

#include "QXmppAtmManager.h"
#include "QXmppAtmTrustMemoryStorage.h"
#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryIq.h"
#include "QXmppCarbonManagerV2.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppIqHandling.h"
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

#include "Async.h"
#include "FakePubSubService.h"
#include "IntegrationTesting.h"
#include "OmemoDeviceList.h"
#include "OmemoKeyMaintenance.h"
#include "OmemoManager_p.h"
#include "OmemoPayloadCipher.h"
#include "OmemoPubSub.h"
#include "OmemoSession.h"
#include "StringLiterals.h"
#include "TestClient.h"
#include "util.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QObject>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QTest>

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
        hooks().add<QXmpp::Connection::StanzaReceived>([this](const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata) {
            return handleIncomingStanza(stanza, e2eeMetadata) ? QXmpp::HookResult::Stop : QXmpp::HookResult::Continue;
        });
    }

private:
    bool handleIncomingStanza(const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
    {
        if (isIqElement<QXmppBitsOfBinaryIq>(stanza)) {
            QXmppBitsOfBinaryIq iq;
            iq.parse(stanza);

            if (iq.cid().toContentId() != m_requestIq.cid().toContentId()) {
                return false;
            }

            // replied with the encryption of the request
            auto response = m_responseIq;
            response.setId(iq.id());
            QT_WARNING_PUSH
            QT_WARNING_DISABLE_DEPRECATED
            client()->reply(std::move(response), e2eeMetadata);
            QT_WARNING_POP
            return true;
        }

        return false;
    }

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
    // for the tests of the deprecated API
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    omemoUser.client.setEncryptionExtension(omemoUser.manager);
    QT_WARNING_POP

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

// tests the deprecated API
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
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
QT_WARNING_POP

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

    // Via the e2ee provider, selected by the SelectEncryption hook
    QObject owner;
    m_alice1.client.hooks(&owner).add<QXmpp::Iq::SelectEncryption>([](QXmpp::Iq::EncryptionSelection &selection) {
        selection.setEncryption(QXmpp::E2ee::Omemo2);
    });

    // Bob can't handle the request and replies with an encrypted error.
    auto request = requestIq;
    auto result = awaitTask(m_alice1.client.sendSensitiveIq(std::move(request)));
    auto stanzaError = expectVariant<QXmppError>(std::move(result)).value<QXmppStanza::Error>();
    QVERIFY(stanzaError.has_value());
    QCOMPARE(stanzaError->condition(), QXmppStanza::Error::ServiceUnavailable);

    // Bob handles the request and replies with the encrypted result.
    auto *iqHandler = new OmemoIqHandler(requestIq, responseIq);
    m_bob.client.addExtension(iqHandler);
    auto cleanup = qScopeGuard([&] { m_bob.client.removeExtension(iqHandler); });

    auto checkResult = [&](QXmppClient::IqResult &&result) {
        QVERIFY(std::holds_alternative<QDomElement>(result));
        if (const auto *response = std::get_if<QDomElement>(&result)) {
            QVERIFY(isIqElement<QXmppBitsOfBinaryIq>(*response));
            QXmppBitsOfBinaryIq resultIq;
            resultIq.parse(*response);
            QCOMPARE(resultIq.data(), responseIq.data());
        }
    };
    request = requestIq;
    checkResult(awaitTask(m_alice1.client.sendSensitiveIq(std::move(request))));

    // Via the encryption extension like before
    m_alice1.client.hooks(&owner).removeAll();
    request = requestIq;
    result = awaitTask(m_alice1.client.sendSensitiveIq(std::move(request)));
    // QXmppClient::sendSensitiveIq() and QXmppOmemoManager::encryptIq() copy the request into a
    // QXmppIq, which loses the payload of subclasses.
    QEXPECT_FAIL("", "The payload of IQs encrypted via the encryption extension is lost", Continue);
    QVERIFY(std::holds_alternative<QDomElement>(result));

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

    // non-anonymous and members-only room, as recommended for OMEMO
    auto created = awaitTask(m_alice1.mucManager->createRoom(mucService, u"alice"_s, QXmppUtils::generateStanzaUuid().toLower()));
    auto room = expectVariant<QXmppMucRoomV2>(std::move(created));
    QTRY_VERIFY_WITH_TIMEOUT(room.roomConfig().value().has_value(), 20'000);
    auto config = *room.roomConfig().value();
    config.setWhoCanDiscoverJids(QXmppMucRoomConfig::WhoCanDiscoverJids::Anyone);
    config.setMembersOnly(true);
    expectVariant<QXmpp::Success>(awaitTask(room.setRoomConfig(config)));
    auto cleanup = qScopeGuard([&] { awaitTask(room.destroyRoom()); });
    QXmpp::Muc::Item member;
    member.setJid(configBob.jidBare());
    member.setAffiliation(QXmpp::Muc::Affiliation::Member);
    expectVariant<QXmpp::Success>(awaitTask(room.setAffiliations({ member })));

    auto bobRoom = expectVariant<QXmppMucRoomV2>(awaitTask(m_bob.mucManager->joinRoom(room.jid(), u"bob"_s)));
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
    QSignalSpy bobReceivedSpy(&m_bob.client, &QXmppClient::messageReceived);
    QSignalSpy aliceReceivedSpy(&m_alice1.client, &QXmppClient::messageReceived);
    for (auto *client : { &m_alice1.client, &m_bob.client }) {
        client->hooks(&owner).add<QXmpp::Messages::SelectEncryption>([](QXmpp::Messages::EncryptionSelection &selection) {
            selection.setEncryption(QXmpp::E2ee::Omemo2);
        });
    }

    // encrypted for the members, finishes with the reflection of the room
    QXmppMessage message;
    message.setBody(u"Hello room!"_s);
    expectVariant<QXmpp::Success>(awaitTask(room.sendMessage(std::move(message))));

    // Bob decrypts it with the keys of the real sender
    QTRY_VERIFY_WITH_TIMEOUT(std::ranges::any_of(bobReceivedSpy, [](const QList<QVariant> &arguments) {
                                 const auto message = arguments.at(0).value<QXmppMessage>();
                                 return message.e2eeMetadata() && message.body() == u"Hello room!";
                             }),
                             20'000);
    QCOMPARE(bobSenders, QStringList { configAlice.jidBare() });

    // members may retrieve the affiliation lists too
    QStringList bobListRequests;
    QObject::connect(&m_bob.logger, &QXmppLogger::message, &owner, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage && text.contains(u"http://jabber.org/protocol/muc#admin"_s)) {
            bobListRequests << text;
        }
    });
    QXmppMessage reply;
    reply.setBody(u"Hello Alice!"_s);
    expectVariant<QXmpp::Success>(awaitTask(bobRoom.sendMessage(std::move(reply))));
    QTRY_VERIFY_WITH_TIMEOUT(std::ranges::any_of(aliceReceivedSpy, [](const QList<QVariant> &arguments) {
                                 const auto message = arguments.at(0).value<QXmppMessage>();
                                 return message.e2eeMetadata() && message.body() == u"Hello Alice!";
                             }),
                             20'000);
    QCOMPARE(bobListRequests.size(), 3);

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

// ============================================================

namespace SessionLayerTests {

// also declared by the managers
using QXmpp::Omemo::Private::DecryptedKey;

using namespace QXmpp;
using namespace QXmpp::Omemo;
using namespace QXmpp::Omemo::Private;

constexpr uint32_t SIGNED_PRE_KEY_ID = 1;
constexpr uint32_t PRE_KEY_COUNT = 5;

struct Device {
    Device(const SignalContext &context, QString jid, std::optional<IdentityKeyPair> identity = {})
        : jid(std::move(jid))
    {
        device.deviceId = *context.generateDeviceId();
        device.identity = identity ? *identity : *context.generateIdentityKeyPair();
        device.signedPreKeys.insert(SIGNED_PRE_KEY_ID, { *context.generateSignedPreKey(device.identity, SIGNED_PRE_KEY_ID, 0), {} });
        keys.preKeys = context.generatePreKeys(1, PRE_KEY_COUNT);
    }

    Address address() const { return { jid, device.deviceId }; }

    QString jid;
    DeviceKeys device;
    VersionKeys keys;
};

static SecureByteArray secure(QByteArrayView data)
{
    return SecureByteArray(data.toByteArray());
}

class tst_OmemoSession : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void keyGeneration();
    Q_SLOT void bundleFormat_data();
    Q_SLOT void bundleFormat();
    Q_SLOT void roundtrip_data();
    Q_SLOT void roundtrip();
    Q_SLOT void sharedIdentity();
    Q_SLOT void outOfOrder_data();
    Q_SLOT void outOfOrder();
    Q_SLOT void duplicate();
    Q_SLOT void invalidSignature_data();
    Q_SLOT void invalidSignature();
    Q_SLOT void versionMismatch();
    Q_SLOT void unknownPreKey();
    Q_SLOT void noSession();
    Q_SLOT void persistence();
    Q_SLOT void callbacks();

    void addVersions();
    void establish(SessionLayer &alice, Device &aliceDevice, SessionLayer &bob, Device &bobDevice);

    std::unique_ptr<SignalContext> m_context;
};

void tst_OmemoSession::initTestCase()
{
    m_context = std::make_unique<SignalContext>();
    QVERIFY(m_context->isValid());
}

void tst_OmemoSession::addVersions()
{
    QTest::addColumn<Version>("version");
    QTest::newRow("omemo0") << Version::Omemo0;
    QTest::newRow("omemo2") << Version::Omemo2;
}

void tst_OmemoSession::keyGeneration()
{
    const auto identity = m_context->generateIdentityKeyPair();
    QVERIFY(identity);
    QCOMPARE(identity->publicKey.size(), 32);
    QVERIFY(!identity->privateKey.isEmpty());
    QVERIFY(identity->publicKey != m_context->generateIdentityKeyPair()->publicKey);

    for (int i = 0; i < 100; ++i) {
        const auto id = m_context->generateDeviceId();
        QVERIFY(id);
        QVERIFY(*id >= 1);
        QVERIFY(*id <= uint32_t(std::numeric_limits<int32_t>::max()));
    }

    const auto preKeys = m_context->generatePreKeys(10, 3);
    QCOMPARE(preKeys.size(), 3);
    QVERIFY(preKeys.contains(10));
    QVERIFY(preKeys.contains(12));
    QCOMPARE(m_context->generatePreKeys(0xFFFFFE, 1).size(), 1);
    QVERIFY(m_context->generatePreKeys(0xFFFFFF, 1).isEmpty());

    QVERIFY(m_context->generateSignedPreKey(*identity, 1, 0));
    QVERIFY(!m_context->generateSignedPreKey(IdentityKeyPair {}, 1, 0));
}

void tst_OmemoSession::bundleFormat_data()
{
    addVersions();
}

void tst_OmemoSession::bundleFormat()
{
    QFETCH(Version, version);

    Device device(*m_context, u"alice@example.org"_s);
    SessionLayer layer(*m_context, version, device.device, device.keys);

    const auto bundle = expectVariant<Bundle>(layer.ownBundle(SIGNED_PRE_KEY_ID, 1));
    QCOMPARE(bundle.signedPreKeyId, SIGNED_PRE_KEY_ID);
    QCOMPARE(bundle.preKeyId, 1u);
    QCOMPARE(bundle.signedPreKeySignature.size(), 64);

    if (version == Version::Omemo2) {
        // Ed25519 identity key, Curve25519 pre keys without prefix
        QCOMPARE(bundle.identityKey, device.device.identity.publicKey);
        QCOMPARE(bundle.signedPreKey.size(), 32);
        QCOMPARE(bundle.preKey.size(), 32);
    } else {
        // Curve25519 keys with type prefix
        QCOMPARE(bundle.identityKey.size(), 33);
        QCOMPARE(bundle.identityKey.at(0), char(0x05));
        QCOMPARE(bundle.signedPreKey.size(), 33);
        QCOMPARE(bundle.signedPreKey.at(0), char(0x05));
        QCOMPARE(bundle.preKey.size(), 33);
        QCOMPARE(bundle.preKey.at(0), char(0x05));
    }

    const auto preKeys = expectVariant<QHash<uint32_t, QByteArray>>(layer.publicPreKeys());
    QCOMPARE(preKeys.size(), int(PRE_KEY_COUNT));
    QCOMPARE(preKeys.value(1), bundle.preKey);

    QVERIFY(std::holds_alternative<QXmppError>(layer.ownBundle(SIGNED_PRE_KEY_ID + 1, 1)));
    QVERIFY(std::holds_alternative<QXmppError>(layer.ownBundle(SIGNED_PRE_KEY_ID, PRE_KEY_COUNT + 1)));
}

void tst_OmemoSession::establish(SessionLayer &alice, Device &aliceDevice, SessionLayer &bob, Device &bobDevice)
{
    const auto bundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 2));
    expectVariant<Success>(alice.buildSession(bobDevice.address(), bundle));

    const auto keyExchange = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("hello")));
    QVERIFY(keyExchange.isKeyExchange);
    const auto decrypted = expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), keyExchange.data, true));
    QCOMPARE(decrypted.data, secure("hello"));

    // Bob answers, so Alice stops sending key exchanges.
    const auto answer = expectVariant<EncryptedKey>(bob.encrypt(aliceDevice.address(), secure("hi")));
    QVERIFY(!answer.isKeyExchange);
    QCOMPARE(expectVariant<DecryptedKey>(alice.decrypt(bobDevice.address(), answer.data, false)).data, secure("hi"));
}

void tst_OmemoSession::roundtrip_data()
{
    addVersions();
}

void tst_OmemoSession::roundtrip()
{
    QFETCH(Version, version);

    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, version, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.device, bobDevice.keys);

    const auto bundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 3));
    expectVariant<Success>(alice.buildSession(bobDevice.address(), bundle));
    QVERIFY(alice.hasSession(bobDevice.address()));
    QVERIFY(!bob.hasSession(aliceDevice.address()));

    // Key exchanges are sent until the other device answered.
    const auto first = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("key 1")));
    const auto second = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("key 2")));
    QVERIFY(first.isKeyExchange);
    QVERIFY(second.isKeyExchange);

    const auto decrypted = expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), first.data, true));
    QCOMPARE(decrypted.data, secure("key 1"));
    QCOMPARE(decrypted.senderIdentityKey, alice.ownIdentityKey());
    QCOMPARE(decrypted.usedPreKeyId, std::optional<uint32_t>(3));
    QVERIFY(!bobDevice.keys.preKeys.contains(3));
    QCOMPARE(bobDevice.keys.preKeys.size(), int(PRE_KEY_COUNT - 1));
    QVERIFY(bob.hasSession(aliceDevice.address()));

    // The second key exchange uses the same pre key, which is already consumed.
    const auto decrypted2 = expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), second.data, true));
    QCOMPARE(decrypted2.data, secure("key 2"));
    QCOMPARE(decrypted2.usedPreKeyId, std::nullopt);

    const auto answer = expectVariant<EncryptedKey>(bob.encrypt(aliceDevice.address(), secure("key 3")));
    QVERIFY(!answer.isKeyExchange);
    const auto decryptedAnswer = expectVariant<DecryptedKey>(alice.decrypt(bobDevice.address(), answer.data, false));
    QCOMPARE(decryptedAnswer.data, secure("key 3"));
    QVERIFY(decryptedAnswer.senderIdentityKey.isEmpty());

    const auto third = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("key 4")));
    QVERIFY(!third.isKeyExchange);
    QCOMPARE(expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), third.data, false)).data, secure("key 4"));

    // 32 bytes as used by OMEMO 0 (key and GCM tag) and 48 bytes as used by OMEMO 2 (key and HMAC)
    for (const auto size : { 32, 48 }) {
        const auto key = SecureByteArray(QByteArray(size, 'k'));
        const auto encrypted = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), key));
        QCOMPARE(expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), encrypted.data, false)).data, key);
    }
}

void tst_OmemoSession::sharedIdentity()
{
    // One device uses the same identity key and signed pre key for both versions, with separate
    // pre keys and sessions.
    const auto aliceIdentity = *m_context->generateIdentityKeyPair();
    const auto bobIdentity = *m_context->generateIdentityKeyPair();

    Device alice0(*m_context, u"alice@example.org"_s, aliceIdentity);
    Device bob0(*m_context, u"bob@example.org"_s, bobIdentity);
    Device alice2 = alice0;
    Device bob2 = bob0;
    alice2.keys.preKeys = m_context->generatePreKeys(1, PRE_KEY_COUNT);
    bob2.keys.preKeys = m_context->generatePreKeys(1, PRE_KEY_COUNT);

    SessionLayer aliceLayer0(*m_context, Version::Omemo0, alice0.device, alice0.keys);
    SessionLayer bobLayer0(*m_context, Version::Omemo0, bob0.device, bob0.keys);
    SessionLayer aliceLayer2(*m_context, Version::Omemo2, alice2.device, alice2.keys);
    SessionLayer bobLayer2(*m_context, Version::Omemo2, bob2.device, bob2.keys);

    establish(aliceLayer0, alice0, bobLayer0, bob0);
    establish(aliceLayer2, alice2, bobLayer2, bob2);

    // Both bundles carry the same identity key in different forms.
    const auto bundle0 = expectVariant<Bundle>(bobLayer0.ownBundle(SIGNED_PRE_KEY_ID, 1));
    const auto bundle2 = expectVariant<Bundle>(bobLayer2.ownBundle(SIGNED_PRE_KEY_ID, 1));
    QCOMPARE(bundle2.identityKey, bobIdentity.publicKey);
    QCOMPARE(bundle0.signedPreKey.mid(1), bundle2.signedPreKey);

    // A key exchange reports the identity key in the format of the version.
    Device carol0(*m_context, u"carol@example.org"_s);
    Device carol2 = carol0;
    carol2.keys.preKeys = m_context->generatePreKeys(1, PRE_KEY_COUNT);
    SessionLayer carolLayer0(*m_context, Version::Omemo0, carol0.device, carol0.keys);
    SessionLayer carolLayer2(*m_context, Version::Omemo2, carol2.device, carol2.keys);

    expectVariant<Success>(aliceLayer0.buildSession(carol0.address(), expectVariant<Bundle>(carolLayer0.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    expectVariant<Success>(aliceLayer2.buildSession(carol2.address(), expectVariant<Bundle>(carolLayer2.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    const auto message0 = expectVariant<EncryptedKey>(aliceLayer0.encrypt(carol0.address(), secure("v0")));
    const auto message2 = expectVariant<EncryptedKey>(aliceLayer2.encrypt(carol2.address(), secure("v2")));
    const auto decrypted0 = expectVariant<DecryptedKey>(carolLayer0.decrypt(alice0.address(), message0.data, true));
    const auto decrypted2 = expectVariant<DecryptedKey>(carolLayer2.decrypt(alice2.address(), message2.data, true));
    QCOMPARE(decrypted0.senderIdentityKey, aliceLayer0.ownIdentityKey());
    QCOMPARE(decrypted0.senderIdentityKey.size(), 33);
    QCOMPARE(decrypted2.senderIdentityKey, aliceIdentity.publicKey);
    QCOMPARE(aliceLayer2.ownIdentityKey(), aliceIdentity.publicKey);
}

void tst_OmemoSession::outOfOrder_data()
{
    addVersions();
}

void tst_OmemoSession::outOfOrder()
{
    QFETCH(Version, version);

    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, version, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.device, bobDevice.keys);
    establish(alice, aliceDevice, bob, bobDevice);

    QList<EncryptedKey> messages;
    for (int i = 0; i < 3; ++i) {
        messages.append(expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure(QByteArray::number(i)))));
    }
    for (const auto i : { 2, 0, 1 }) {
        QCOMPARE(expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), messages.at(i).data, false)).data, secure(QByteArray::number(i)));
    }
}

void tst_OmemoSession::duplicate()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.device, bobDevice.keys);
    establish(alice, aliceDevice, bob, bobDevice);

    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("once")));
    expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), message.data, false));
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), message.data, false));
}

void tst_OmemoSession::invalidSignature_data()
{
    addVersions();
}

void tst_OmemoSession::invalidSignature()
{
    QFETCH(Version, version);

    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, version, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.device, bobDevice.keys);

    auto bundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    bundle.signedPreKeySignature[0] = char(bundle.signedPreKeySignature.at(0) ^ 0x01);
    expectVariant<QXmppError>(alice.buildSession(bobDevice.address(), bundle));
    QVERIFY(!alice.hasSession(bobDevice.address()));

    // signed by another identity key
    auto otherBundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    Device mallory(*m_context, u"mallory@example.org"_s);
    SessionLayer malloryLayer(*m_context, version, mallory.device, mallory.keys);
    otherBundle.identityKey = expectVariant<Bundle>(malloryLayer.ownBundle(SIGNED_PRE_KEY_ID, 1)).identityKey;
    expectVariant<QXmppError>(alice.buildSession(bobDevice.address(), otherBundle));

    auto truncated = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    truncated.preKey.chop(1);
    expectVariant<QXmppError>(alice.buildSession(bobDevice.address(), truncated));

    // keys in the format of the other version
    auto otherFormat = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    if (version == Version::Omemo2) {
        otherFormat.preKey.prepend(char(0x05));
    } else {
        otherFormat.preKey.remove(0, 1);
    }
    expectVariant<QXmppError>(alice.buildSession(bobDevice.address(), otherFormat));
}

void tst_OmemoSession::versionMismatch()
{
    const auto aliceIdentity = *m_context->generateIdentityKeyPair();
    Device aliceDevice(*m_context, u"alice@example.org"_s, aliceIdentity);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, Version::Omemo0, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob0(*m_context, Version::Omemo0, bobDevice.device, bobDevice.keys);
    SessionLayer bob2(*m_context, Version::Omemo2, bobDevice.device, bobDevice.keys);

    expectVariant<Success>(alice.buildSession(bobDevice.address(), expectVariant<Bundle>(bob0.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("v0")));
    expectVariant<QXmppError>(bob2.decrypt(aliceDevice.address(), message.data, true));
    QVERIFY(!bob2.hasSession(aliceDevice.address()));
    QVERIFY(bobDevice.keys.preKeys.contains(1));
}

void tst_OmemoSession::unknownPreKey()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.device, bobDevice.keys);

    expectVariant<Success>(alice.buildSession(bobDevice.address(), expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("x")));
    bobDevice.keys.preKeys.remove(1);
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), message.data, true));
}

void tst_OmemoSession::noSession()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.device, bobDevice.keys);
    establish(alice, aliceDevice, bob, bobDevice);

    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("x")));
    bobDevice.keys.sessions.clear();
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), message.data, false));
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), QByteArray("garbage"), false));
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), QByteArray("garbage"), true));
}

void tst_OmemoSession::persistence()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    {
        SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.device, aliceDevice.keys);
        SessionLayer bob(*m_context, Version::Omemo2, bobDevice.device, bobDevice.keys);
        establish(alice, aliceDevice, bob, bobDevice);
    }

    // The serialized state is enough to continue, also with a new context.
    SignalContext context;
    auto aliceKeys = aliceDevice.keys;
    auto bobKeys = bobDevice.keys;
    SessionLayer alice(context, Version::Omemo2, aliceDevice.device, aliceKeys);
    SessionLayer bob(context, Version::Omemo2, bobDevice.device, bobKeys);

    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("after restart")));
    QVERIFY(!message.isKeyExchange);
    QCOMPARE(expectVariant<DecryptedKey>(bob.decrypt(aliceDevice.address(), message.data, false)).data, secure("after restart"));
}

void tst_OmemoSession::callbacks()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);

    QHash<Address, QByteArray> stored;
    QList<uint32_t> removedPreKeys;
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.device, aliceDevice.keys);
    SessionLayer bob(*m_context,
                     Version::Omemo2,
                     bobDevice.device, bobDevice.keys,
                     {
                         [&](const Address &address, const QByteArray &session) { stored.insert(address, session); },
                         {},
                         [&](uint32_t id) { removedPreKeys.append(id); },
                     });

    establish(alice, aliceDevice, bob, bobDevice);
    QCOMPARE(removedPreKeys, QList<uint32_t> { 2 });
    QCOMPARE(stored.keys(), QList<Address> { aliceDevice.address() });
    QCOMPARE(stored.value(aliceDevice.address()), bobDevice.keys.sessions.value(aliceDevice.address()));
}

}  // namespace SessionLayerTests

// ============================================================

namespace PayloadCipherTests {

using namespace QXmpp::Omemo;
using namespace QXmpp::Omemo::Private;
namespace PC = QXmpp::Omemo::Private::PayloadCipher;

// Test vectors created with pyca/cryptography.
constexpr QByteArrayView plaintext = "Hello from an independent implementation!";
constexpr auto omemo0Ciphertext = "520722c9b422de88f4537771e8ea428254ed089afccdb834804fd8facc9e929edf922645ff7148ffab";
constexpr auto omemo0Tag = "d6b97de42d242fca2f10886719574267";
// 16 byte IV (bytes 50 to 65), tag appended
constexpr auto omemo0LegacyIvCiphertext = "66432903869a4d98de5c392750cd855cf55d6f9c6968d948011622f5143545dd95c3dfaf80a43ee56e4589e1e61e4f358108238dba71d6b9d4";
constexpr auto omemo2Ciphertext = "087bc0697b25c1e372654e73642179b75bdb94f676a36a746002e39ac6f6c860bad0f90cff9e575264a2e380643ff713";
constexpr auto omemo2Mac = "fa1c0336fc3db14eda70778fd87a96e6";

static QByteArray range(int first, int count)
{
    QByteArray data(count, Qt::Uninitialized);
    for (int i = 0; i < count; ++i) {
        data[i] = char(first + i);
    }
    return data;
}

static SecureByteArray secure(const QByteArray &data)
{
    return SecureByteArray(data);
}

class tst_OmemoPayloadCipher : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void omemo0Vector();
    Q_SLOT void omemo2Vector();
    Q_SLOT void roundtrip_data();
    Q_SLOT void roundtrip();
    Q_SLOT void tampering_data();
    Q_SLOT void tampering();
    Q_SLOT void omemo0LegacyFormats();
    Q_SLOT void invalidSizes();
};

void tst_OmemoPayloadCipher::omemo0Vector()
{
    const auto key = range(0, 16);
    const auto iv = range(100, 12);

    const auto encrypted = expectVariant<PC::Encrypted>(PC::encrypt(Version::Omemo0, plaintext.toByteArray(), secure(key), iv));
    QCOMPARE(encrypted.ciphertext.toHex(), QByteArray(omemo0Ciphertext));
    QCOMPARE(encrypted.keyMaterial.toByteArray().toHex(), QByteArray(key + QByteArray::fromHex(omemo0Tag)).toHex());
    QCOMPARE(encrypted.iv, iv);

    const auto decrypted = expectVariant<QByteArray>(PC::decrypt(Version::Omemo0, encrypted.keyMaterial, iv, QByteArray::fromHex(omemo0Ciphertext)));
    QCOMPARE(decrypted, plaintext.toByteArray());
}

void tst_OmemoPayloadCipher::omemo2Vector()
{
    const auto key = range(32, 32);

    const auto encrypted = expectVariant<PC::Encrypted>(PC::encrypt(Version::Omemo2, plaintext.toByteArray(), secure(key), {}));
    QCOMPARE(encrypted.ciphertext.toHex(), QByteArray(omemo2Ciphertext));
    QCOMPARE(encrypted.keyMaterial.toByteArray().toHex(), QByteArray(key + QByteArray::fromHex(omemo2Mac)).toHex());
    QVERIFY(encrypted.iv.isEmpty());

    const auto decrypted = expectVariant<QByteArray>(PC::decrypt(Version::Omemo2, encrypted.keyMaterial, {}, QByteArray::fromHex(omemo2Ciphertext)));
    QCOMPARE(decrypted, plaintext.toByteArray());
}

void tst_OmemoPayloadCipher::roundtrip_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<int>("keyMaterialSize");

    for (const auto &[name, version, size] : { std::tuple { "omemo0", Version::Omemo0, 32 }, std::tuple { "omemo2", Version::Omemo2, 48 } }) {
        QTest::addRow("%s-empty", name) << version << QByteArray() << size;
        QTest::addRow("%s-block", name) << version << QByteArray(16, 'a') << size;
        QTest::addRow("%s-long", name) << version << QByteArray(1000, 'b') << size;
    }
}

void tst_OmemoPayloadCipher::roundtrip()
{
    QFETCH(Version, version);
    QFETCH(QByteArray, plaintext);
    QFETCH(int, keyMaterialSize);

    const auto encrypted = expectVariant<PC::Encrypted>(PC::encrypt(version, plaintext));
    QCOMPARE(encrypted.keyMaterial.size(), keyMaterialSize);
    QCOMPARE(encrypted.iv.size(), version == Version::Omemo0 ? 12 : 0);
    QCOMPARE(expectVariant<QByteArray>(PC::decrypt(version, encrypted.keyMaterial, encrypted.iv, encrypted.ciphertext)), plaintext);

    // random keys
    const auto other = expectVariant<PC::Encrypted>(PC::encrypt(version, plaintext));
    QVERIFY(other.keyMaterial != encrypted.keyMaterial);
}

void tst_OmemoPayloadCipher::tampering_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QString>("part");

    for (const auto &[name, version] : { std::pair { "omemo0", Version::Omemo0 }, std::pair { "omemo2", Version::Omemo2 } }) {
        QTest::addRow("%s-ciphertext", name) << version << u"ciphertext"_s;
        QTest::addRow("%s-key", name) << version << u"key"_s;
        QTest::addRow("%s-tag", name) << version << u"tag"_s;
        QTest::addRow("%s-truncated", name) << version << u"truncated"_s;
    }
    QTest::addRow("omemo0-iv") << Version::Omemo0 << u"iv"_s;
}

void tst_OmemoPayloadCipher::tampering()
{
    QFETCH(Version, version);
    QFETCH(QString, part);

    const auto encrypted = expectVariant<PC::Encrypted>(PC::encrypt(version, QByteArray(40, 'x')));
    auto keyMaterial = encrypted.keyMaterial.toByteArray();
    auto iv = encrypted.iv;
    auto ciphertext = encrypted.ciphertext;

    if (part == u"ciphertext") {
        ciphertext[5] = char(ciphertext.at(5) ^ 0x01);
    } else if (part == u"key") {
        keyMaterial[0] = char(keyMaterial.at(0) ^ 0x01);
    } else if (part == u"tag") {
        keyMaterial[keyMaterial.size() - 1] = char(keyMaterial.back() ^ 0x01);
    } else if (part == u"truncated") {
        ciphertext.chop(1);
    } else if (part == u"iv") {
        iv[0] = char(iv.at(0) ^ 0x01);
    }

    expectVariant<QXmppError>(PC::decrypt(version, secure(keyMaterial), iv, ciphertext));
}

void tst_OmemoPayloadCipher::omemo0LegacyFormats()
{
    const auto key = range(0, 16);

    // tag appended to the ciphertext, only the key as key material
    const auto withTag = QByteArray::fromHex(omemo0Ciphertext) + QByteArray::fromHex(omemo0Tag);
    QCOMPARE(expectVariant<QByteArray>(PC::decrypt(Version::Omemo0, secure(key), range(100, 12), withTag)), plaintext.toByteArray());

    // 16 byte IV
    QCOMPARE(expectVariant<QByteArray>(PC::decrypt(Version::Omemo0, secure(key), range(50, 16), QByteArray::fromHex(omemo0LegacyIvCiphertext))), plaintext.toByteArray());
}

void tst_OmemoPayloadCipher::invalidSizes()
{
    const auto data = QByteArray(32, 'x');

    expectVariant<QXmppError>(PC::encrypt(Version::Omemo0, data, secure(range(0, 32)), range(0, 12)));
    expectVariant<QXmppError>(PC::encrypt(Version::Omemo0, data, secure(range(0, 16)), range(0, 16)));
    expectVariant<QXmppError>(PC::encrypt(Version::Omemo2, data, secure(range(0, 16)), {}));
    expectVariant<QXmppError>(PC::encrypt(Version::Omemo2, data, secure(range(0, 32)), range(0, 16)));

    expectVariant<QXmppError>(PC::decrypt(Version::Omemo0, secure(range(0, 24)), range(0, 12), data));
    expectVariant<QXmppError>(PC::decrypt(Version::Omemo0, secure(range(0, 32)), range(0, 8), data));
    expectVariant<QXmppError>(PC::decrypt(Version::Omemo0, secure(range(0, 16)), range(0, 12), range(0, 8)));
    expectVariant<QXmppError>(PC::decrypt(Version::Omemo2, secure(range(0, 32)), {}, data));
    expectVariant<QXmppError>(PC::decrypt(Version::Omemo2, SecureByteArray(), {}, data));
}

}  // namespace PayloadCipherTests

// ============================================================

namespace KeyMaintenanceTests {

// also declared by the managers
using QXmpp::Omemo::Private::DecryptedKey;
using QXmpp::Omemo::Private::KeyMaintenance::PRE_KEY_ID_MAX;

using namespace QXmpp;
using namespace QXmpp::Omemo;
using namespace QXmpp::Omemo::Private;
using namespace QXmpp::Omemo::Private::KeyMaintenance;
using namespace std::chrono_literals;

static const auto start = QDateTime(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);
constexpr auto week = 24h * 7;

static QDateTime after(std::chrono::seconds duration)
{
    return start.addSecs(duration.count());
}

class tst_OmemoKeyMaintenance : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();
    Q_SLOT void keyIds();
    Q_SLOT void initialSignedPreKey();
    Q_SLOT void signedPreKeyRotation();
    Q_SLOT void signedPreKeyIdWrap();
    Q_SLOT void graceAllowsPendingKeyExchanges();
    Q_SLOT void preKeyRefill();
    Q_SLOT void preKeyIdWrap();

    DeviceKeys createDevice() const;

    std::unique_ptr<SignalContext> m_context;
};

void tst_OmemoKeyMaintenance::initTestCase()
{
    m_context = std::make_unique<SignalContext>();
    QVERIFY(m_context->isValid());
}

DeviceKeys tst_OmemoKeyMaintenance::createDevice() const
{
    DeviceKeys device;
    device.deviceId = 1234;
    device.identity = *m_context->generateIdentityKeyPair();
    return device;
}

void tst_OmemoKeyMaintenance::keyIds()
{
    const QSet<uint32_t> none;
    QCOMPARE(nextKeyId(0, none), 1u);
    QCOMPARE(nextKeyId(5, none), 6u);
    QCOMPARE(nextKeyId(KEY_ID_MAX, none), 1u);
    QCOMPARE(nextKeyId(KEY_ID_MAX + 10u, none), 1u);

    const QSet<uint32_t> used { 1, 2, 6 };
    QCOMPARE(nextKeyId(5, used), 7u);
    QCOMPARE(nextKeyId(KEY_ID_MAX, used), 3u);
    QCOMPARE(nextKeyId(KEY_ID_MAX - 1, QSet<uint32_t> { KEY_ID_MAX }), 1u);
}

void tst_OmemoKeyMaintenance::initialSignedPreKey()
{
    auto device = createDevice();
    QVERIFY(!nextSignedPreKeyCheck(device).isValid());

    const auto changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, start));
    QCOMPARE(changes.addedId, std::optional<uint32_t>(1));
    QVERIFY(changes.removedIds.isEmpty());
    QCOMPARE(device.latestSignedPreKeyId, 1u);
    QCOMPARE(device.signedPreKeys.value(1).creationDate, start);
    QVERIFY(!device.signedPreKeys.value(1).record.isEmpty());
    QCOMPARE(nextSignedPreKeyCheck(device), after(4 * week));

    // usable for both versions
    VersionKeys keys;
    keys.preKeys = m_context->generatePreKeys(1, 1);
    for (const auto version : { Version::Omemo0, Version::Omemo2 }) {
        SessionLayer layer(*m_context, version, device, keys);
        expectVariant<Bundle>(layer.ownBundle(1, 1));
    }

    // invalid identity
    DeviceKeys invalid;
    expectVariant<QXmppError>(rotateSignedPreKey(*m_context, invalid, start));
    QVERIFY(invalid.signedPreKeys.isEmpty());
}

void tst_OmemoKeyMaintenance::signedPreKeyRotation()
{
    auto device = createDevice();
    expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, start));

    // nothing to do before the renewal interval
    QVERIFY(expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(4 * week - 1s))).isEmpty());

    // a new key, the previous one stays for the grace period
    auto changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(4 * week)));
    QCOMPARE(changes.addedId, std::optional<uint32_t>(2));
    QVERIFY(changes.removedIds.isEmpty());
    QCOMPARE(device.latestSignedPreKeyId, 2u);
    QCOMPARE(device.signedPreKeys.keys().size(), 2);
    QCOMPARE(nextSignedPreKeyCheck(device), after(8 * week));

    QVERIFY(expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(8 * week - 1s))).isEmpty());

    // The renewal of key 2 and the removal of key 1 are due at the same time; key 2 stays.
    changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(8 * week)));
    QCOMPARE(changes.addedId, std::optional<uint32_t>(3));
    QCOMPARE(changes.removedIds, QList<uint32_t> { 1 });
    QVERIFY(device.signedPreKeys.contains(2));
    QVERIFY(device.signedPreKeys.contains(3));

    // with a shorter grace period, the removal is due before the renewal
    const Config config { .signedPreKeyGracePeriod = 1 * week };
    QCOMPARE(nextSignedPreKeyCheck(device, config), after(9 * week));
    changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(9 * week), config));
    QCOMPARE(changes.addedId, std::nullopt);
    QCOMPARE(changes.removedIds, QList<uint32_t> { 2 });
    QCOMPARE(device.signedPreKeys.keys(), QList<uint32_t> { 3 });
    QCOMPARE(nextSignedPreKeyCheck(device, config), after(12 * week));

    // after a long time offline, the old key is replaced, but kept for the grace period
    changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(52 * week)));
    QCOMPARE(changes.addedId, std::optional<uint32_t>(4));
    QVERIFY(changes.removedIds.isEmpty());
    QCOMPARE(device.signedPreKeys.size(), 2);
}

void tst_OmemoKeyMaintenance::signedPreKeyIdWrap()
{
    // Older versions reused ID 1 after the maximum ID and overwrote that key.
    auto device = createDevice();
    device.latestSignedPreKeyId = KEY_ID_MAX;
    device.signedPreKeys.insert(KEY_ID_MAX, { *m_context->generateSignedPreKey(device.identity, KEY_ID_MAX, 0), start });
    device.signedPreKeys.insert(1, { *m_context->generateSignedPreKey(device.identity, 1, 0), start.addSecs(-1) });
    const auto key1 = device.signedPreKeys.value(1).record;

    const auto changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, after(4 * week)));
    QCOMPARE(changes.addedId, std::optional<uint32_t>(2));
    QVERIFY(device.signedPreKeys.value(2).record != key1);
    // key 1 was replaced by the key with the maximum ID four weeks ago
    QCOMPARE(changes.removedIds, QList<uint32_t> { 1 });
}

void tst_OmemoKeyMaintenance::graceAllowsPendingKeyExchanges()
{
    // Alice uses Bob's bundle with the old signed pre key while Bob rotates it.
    auto bobDevice = createDevice();
    VersionKeys bobKeys;
    expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, bobDevice, start));
    expectVariant<QList<uint32_t>>(refillPreKeys(*m_context, bobKeys));
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice, bobKeys);
    const auto oldBundle = expectVariant<Bundle>(bob.ownBundle(1, 1));

    auto aliceDevice = createDevice();
    aliceDevice.deviceId = 5678;
    VersionKeys aliceKeys;
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice, aliceKeys);
    const Address aliceAddress { u"alice@example.org"_s, aliceDevice.deviceId };
    const Address bobAddress { u"bob@example.org"_s, bobDevice.deviceId };

    expectVariant<Success>(alice.buildSession(bobAddress, oldBundle));
    const auto pending = expectVariant<EncryptedKey>(alice.encrypt(bobAddress, SecureByteArray(QByteArray("pending"))));

    expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, bobDevice, after(4 * week)));
    QCOMPARE(bobDevice.latestSignedPreKeyId, 2u);
    QCOMPARE(expectVariant<DecryptedKey>(bob.decrypt(aliceAddress, pending.data, true)).data, SecureByteArray(QByteArray("pending")));

    // After the grace period, key exchanges with the old bundle fail.
    const auto changes = expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, bobDevice, after(8 * week - 1s)));
    QVERIFY(changes.isEmpty());
    expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, bobDevice, after(8 * week)));
    QVERIFY(!bobDevice.signedPreKeys.contains(1));

    auto carolDevice = createDevice();
    carolDevice.deviceId = 9012;
    VersionKeys carolKeys;
    SessionLayer carol(*m_context, Version::Omemo2, carolDevice, carolKeys);
    expectVariant<Success>(carol.buildSession(bobAddress, oldBundle));
    const auto late = expectVariant<EncryptedKey>(carol.encrypt(bobAddress, SecureByteArray(QByteArray("late"))));
    expectVariant<QXmppError>(bob.decrypt({ u"carol@example.org"_s, carolDevice.deviceId }, late.data, true));
}

void tst_OmemoKeyMaintenance::preKeyRefill()
{
    VersionKeys keys;
    auto added = expectVariant<QList<uint32_t>>(refillPreKeys(*m_context, keys));
    QCOMPARE(added.size(), 100);
    QCOMPARE(added.first(), 1u);
    QCOMPARE(added.last(), 100u);
    QCOMPARE(keys.preKeys.size(), 100);
    QCOMPARE(keys.latestPreKeyId, 100u);

    QVERIFY(expectVariant<QList<uint32_t>>(refillPreKeys(*m_context, keys)).isEmpty());

    // consumed keys are replaced with new IDs
    keys.preKeys.remove(3);
    keys.preKeys.remove(50);
    added = expectVariant<QList<uint32_t>>(refillPreKeys(*m_context, keys));
    QCOMPARE(added, (QList<uint32_t> { 101, 102 }));
    QVERIFY(!keys.preKeys.contains(3));

    // the keys are distinct and usable
    DeviceKeys device = createDevice();
    expectVariant<SignedPreKeyChanges>(rotateSignedPreKey(*m_context, device, start));
    SessionLayer layer(*m_context, Version::Omemo0, device, keys);
    const auto publicKeys = expectVariant<QHash<uint32_t, QByteArray>>(layer.publicPreKeys());
    QCOMPARE(publicKeys.size(), 100);
    QCOMPARE(QSet<QByteArray>(publicKeys.cbegin(), publicKeys.cend()).size(), 100);
}

void tst_OmemoKeyMaintenance::preKeyIdWrap()
{
    VersionKeys keys;
    keys.preKeys = m_context->generatePreKeys(PRE_KEY_ID_MAX - 1, 1);
    keys.preKeys.insert(2, m_context->generatePreKeys(2, 1).value(2));
    keys.latestPreKeyId = PRE_KEY_ID_MAX - 1;
    const auto key2 = keys.preKeys.value(2);

    const auto added = expectVariant<QList<uint32_t>>(refillPreKeys(*m_context, keys, Config { .preKeyCount = 5 }));
    QCOMPARE(added, (QList<uint32_t> { PRE_KEY_ID_MAX, 1, 3 }));
    QCOMPARE(keys.preKeys.value(2), key2);
    QCOMPARE(keys.latestPreKeyId, 3u);
}

}  // namespace KeyMaintenanceTests

// ============================================================

namespace DeviceListTests {

using namespace QXmpp::Omemo::Private;
using namespace QXmpp::Omemo::Private::DeviceList;
using namespace std::chrono_literals;

using Ids = QList<uint32_t>;
using Devices = QHash<uint32_t, DeviceInfo>;

static const auto start = QDateTime(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);
constexpr auto week = 24h * 7;

static QDateTime after(std::chrono::seconds duration)
{
    return start.addSecs(duration.count());
}

class tst_OmemoDeviceList : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void addAndChange();
    Q_SLOT void removalAndReturn();
    Q_SLOT void removalDateNotRefreshed();
    Q_SLOT void invalidEntries();
    Q_SLOT void maximum();
    Q_SLOT void ownList_data();
    Q_SLOT void ownList();
    Q_SLOT void clearList();
    Q_SLOT void expiry();
    Q_SLOT void listToPublish();
};

void tst_OmemoDeviceList::addAndChange()
{
    Devices devices;
    auto changes = apply(devices, { { 1, u"Phone"_s }, { 2, {} } }, start, 10);
    QCOMPARE(changes.added, (Ids { 1, 2 }));
    QVERIFY(changes.changed.isEmpty());
    QVERIFY(changes.removed.isEmpty());
    QVERIFY(!changes.republishOwnList);
    QCOMPARE(devices.value(1), (DeviceInfo { u"Phone"_s, {} }));
    QCOMPARE(devices.value(2), (DeviceInfo {}));

    // same list
    QVERIFY(apply(devices, { { 2, {} }, { 1, u"Phone"_s } }, start, 10).isEmpty());

    // new label
    changes = apply(devices, { { 1, u"Laptop"_s }, { 2, {} } }, start, 10);
    QCOMPARE(changes.changed, Ids { 1 });
    QCOMPARE(devices.value(1).label, u"Laptop"_s);
}

void tst_OmemoDeviceList::removalAndReturn()
{
    Devices devices;
    apply(devices, { { 1, {} }, { 2, {} } }, start, 10);

    auto changes = apply(devices, { { 2, {} } }, after(1 * week), 10);
    QCOMPARE(changes.removed, Ids { 1 });
    QCOMPARE(devices.value(1).removalDate, after(1 * week));
    QVERIFY(devices.contains(1));

    // listed again
    changes = apply(devices, { { 1, {} }, { 2, {} } }, after(2 * week), 10);
    QCOMPARE(changes.changed, Ids { 1 });
    QVERIFY(!devices.value(1).removalDate.isValid());
}

void tst_OmemoDeviceList::removalDateNotRefreshed()
{
    // Older versions set the removal date again with every update of the list, so devices of
    // users with frequently changing lists were never removed.
    Devices devices;
    apply(devices, { { 1, {} }, { 2, {} } }, start, 10);
    apply(devices, { { 2, {} } }, after(1 * week), 10);

    const auto changes = apply(devices, { { 2, {} }, { 3, {} } }, after(5 * week), 10);
    QVERIFY(changes.removed.isEmpty());
    QCOMPARE(devices.value(1).removalDate, after(1 * week));
    QCOMPARE(expired(devices, after(13 * week)), Ids { 1 });
}

void tst_OmemoDeviceList::invalidEntries()
{
    Devices devices;
    const auto changes = apply(devices, { { 0, {} }, { 1, u"first"_s }, { 1, u"second"_s } }, start, 10);
    QCOMPARE(changes.added, Ids { 1 });
    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.value(1).label, u"first"_s);
    QVERIFY(!changes.republishOwnList);
}

void tst_OmemoDeviceList::maximum()
{
    Devices devices;
    const auto changes = apply(devices, { { 5, {} }, { 4, {} }, { 3, {} }, { 2, {} } }, start, 2);
    QCOMPARE(changes.added, (Ids { 4, 5 }));
    QCOMPARE(changes.ignoredCount, 2);
    QCOMPARE(devices.size(), 2);

    // known devices beyond the maximum are treated as removed
    Devices known;
    apply(known, { { 1, {} }, { 2, {} }, { 3, {} } }, start, 10);
    QCOMPARE(apply(known, { { 1, {} }, { 2, {} }, { 3, {} } }, start, 2).removed, Ids { 3 });
}

void tst_OmemoDeviceList::ownList_data()
{
    QTest::addColumn<QList<Entry>>("list");
    QTest::addColumn<std::optional<QString>>("ownLabel");
    QTest::addColumn<bool>("republish");

    const auto own = Entry { 7, u"This"_s };
    QTest::newRow("correct") << QList<Entry> { { 1, {} }, own } << std::optional<QString>(u"This"_s) << false;
    QTest::newRow("missing") << QList<Entry> { { 1, {} } } << std::optional<QString>(u"This"_s) << true;
    QTest::newRow("empty") << QList<Entry> {} << std::optional<QString>(u"This"_s) << true;
    QTest::newRow("wrong-label") << QList<Entry> { { 7, u"Other"_s } } << std::optional<QString>(u"This"_s) << true;
    QTest::newRow("duplicate") << QList<Entry> { own, { 1, {} }, { 1, {} } } << std::optional<QString>(u"This"_s) << true;
    QTest::newRow("duplicate-own") << QList<Entry> { own, own } << std::optional<QString>(u"This"_s) << true;
    QTest::newRow("omemo0-label-ignored") << QList<Entry> { { 7, {} } } << std::optional<QString>() << false;
}

void tst_OmemoDeviceList::ownList()
{
    QFETCH(QList<Entry>, list);
    QFETCH(std::optional<QString>, ownLabel);
    QFETCH(bool, republish);

    Devices devices;
    const auto changes = apply(devices, list, start, 10, OwnDevice { 7, ownLabel });
    QCOMPARE(changes.republishOwnList, republish);
    QVERIFY(!devices.contains(7));
}

void tst_OmemoDeviceList::clearList()
{
    Devices devices;
    apply(devices, { { 1, {} }, { 2, {} } }, start, 10);
    apply(devices, { { 2, {} } }, after(1 * week), 10);

    const auto changes = clear(devices, after(2 * week));
    QCOMPARE(changes.removed, Ids { 2 });
    QCOMPARE(devices.value(1).removalDate, after(1 * week));
    QCOMPARE(devices.value(2).removalDate, after(2 * week));
}

void tst_OmemoDeviceList::expiry()
{
    Devices devices;
    QVERIFY(!nextExpiry(devices).isValid());

    apply(devices, { { 1, {} }, { 2, {} }, { 3, {} } }, start, 10);
    apply(devices, { { 2, {} }, { 3, {} } }, after(1 * week), 10);
    apply(devices, { { 3, {} } }, after(2 * week), 10);

    QCOMPARE(nextExpiry(devices), after(13 * week));
    QVERIFY(expired(devices, after(13 * week - 1s)).isEmpty());
    QCOMPARE(expired(devices, after(13 * week)), Ids { 1 });
    QCOMPARE(expired(devices, after(14 * week)), (Ids { 1, 2 }));
    QCOMPARE(expired(devices, after(2 * week), 1 * week), Ids { 1 });
}

void tst_OmemoDeviceList::listToPublish()
{
    Devices devices;
    apply(devices, { { 9, u"Tablet"_s }, { 3, u"Laptop"_s }, { 4, {} } }, start, 10, OwnDevice { 7, u"Phone"_s });
    apply(devices, { { 9, u"Tablet"_s }, { 3, u"Laptop"_s } }, after(1 * week), 10, OwnDevice { 7, u"Phone"_s });

    QCOMPARE(DeviceList::ownList(devices, OwnDevice { 7, u"Phone"_s }),
             (QList<Entry> { { 7, u"Phone"_s }, { 3, u"Laptop"_s }, { 9, u"Tablet"_s } }));
    QCOMPARE(DeviceList::ownList({}, OwnDevice { 7, std::nullopt }), (QList<Entry> { { 7, {} } }));
}

}  // namespace DeviceListTests

// ============================================================

namespace StorageTests {

// also declared by the managers
using QXmpp::Omemo::Private::DecryptedKey;

using namespace QXmpp;
using namespace QXmpp::Omemo;
using namespace QXmpp::Omemo::Private;

using StorageDevice = Storage::Device;

static const auto start = QDateTime(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);

class tst_OmemoStorage : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void ownDevice();
    Q_SLOT void signedPreKeys();
    Q_SLOT void preKeysPerVersion();
    Q_SLOT void devicesPerVersion();
    Q_SLOT void resetAll();
    Q_SLOT void migrateFields();
    Q_SLOT void migrateKeepsSession();
};

void tst_OmemoStorage::ownDevice()
{
    MemoryStorage storage;
    QVERIFY(!storage.allData().result().ownDevice);

    const Storage::OwnDevice device { 123, "private", "public", 4, { { Version::Omemo0, 7 }, { Version::Omemo2, 9 } } };
    storage.setOwnDevice(device);
    QCOMPARE(storage.allData().result().ownDevice, device);

    storage.setOwnDevice({});
    QVERIFY(!storage.allData().result().ownDevice);
}

void tst_OmemoStorage::signedPreKeys()
{
    MemoryStorage storage;
    storage.addSignedPreKey({ 1, "one", start });
    storage.addSignedPreKey({ 2, "two", start.addDays(1) });
    storage.addSignedPreKey({ 1, "one again", start.addDays(2) });
    QCOMPARE(storage.allData().result().signedPreKeys,
             (QList<Storage::SignedPreKey> { { 1, "one again", start.addDays(2) }, { 2, "two", start.addDays(1) } }));

    storage.removeSignedPreKey(1);
    QCOMPARE(storage.allData().result().signedPreKeys, (QList<Storage::SignedPreKey> { { 2, "two", start.addDays(1) } }));
}

void tst_OmemoStorage::preKeysPerVersion()
{
    MemoryStorage storage;
    storage.addPreKeys({ { Version::Omemo0, 1, "v0 1" }, { Version::Omemo0, 2, "v0 2" } });
    storage.addPreKeys({ { Version::Omemo2, 1, "v2 1" } });

    QCOMPARE(storage.allData().result().preKeys,
             (QList<Storage::PreKey> { { Version::Omemo0, 1, "v0 1" }, { Version::Omemo0, 2, "v0 2" }, { Version::Omemo2, 1, "v2 1" } }));

    storage.removePreKey(Version::Omemo2, 1);
    storage.removePreKey(Version::Omemo2, 2);
    QCOMPARE(storage.allData().result().preKeys,
             (QList<Storage::PreKey> { { Version::Omemo0, 1, "v0 1" }, { Version::Omemo0, 2, "v0 2" } }));
}

void tst_OmemoStorage::devicesPerVersion()
{
    MemoryStorage storage;
    const StorageDevice alice0 { Version::Omemo0, u"alice@example.org"_s, 1, {}, "key", "session 0", 1, 2, {} };
    const StorageDevice alice2 { Version::Omemo2, u"alice@example.org"_s, 1, u"Phone"_s, "key", "session 2", 0, 0, {} };
    const StorageDevice bob2 { Version::Omemo2, u"bob@example.org"_s, 5, {}, "bob key", {}, 0, 0, start };

    storage.addDevice(alice0);
    storage.addDevice(alice2);
    storage.addDevice(bob2);
    QCOMPARE(storage.allData().result().devices, (QList<StorageDevice> { alice0, alice2, bob2 }));

    auto updated = alice2;
    updated.session = "new session";
    storage.addDevice(updated);
    QCOMPARE(storage.allData().result().devices, (QList<StorageDevice> { alice0, updated, bob2 }));

    storage.removeDevice(Version::Omemo0, u"alice@example.org"_s, 1);
    QCOMPARE(storage.allData().result().devices, (QList<StorageDevice> { updated, bob2 }));

    storage.addDevice(alice0);
    storage.removeDevices(u"alice@example.org"_s);
    QCOMPARE(storage.allData().result().devices, QList<StorageDevice> { bob2 });
}

void tst_OmemoStorage::resetAll()
{
    MemoryStorage storage;
    storage.setOwnDevice(Storage::OwnDevice { 1 });
    storage.addSignedPreKey({ 1, "key", start });
    storage.addPreKeys({ { Version::Omemo0, 1, "key" } });
    storage.addDevice({ Version::Omemo0, u"alice@example.org"_s, 1 });

    storage.resetAll();
    const auto data = storage.allData().result();
    QVERIFY(!data.ownDevice);
    QVERIFY(data.signedPreKeys.isEmpty());
    QVERIFY(data.preKeys.isEmpty());
    QVERIFY(data.devices.isEmpty());
}

void tst_OmemoStorage::migrateFields()
{
    QXmppOmemoStorage::OmemoData old;
    old.ownDevice = QXmppOmemoStorage::OwnDevice { 123, u"Phone"_s, "private", "public", 3, 42 };
    old.signedPreKeyPairs.insert(3, { start, "signed" });
    old.preKeyPairs.insert(41, "pre 41");
    old.preKeyPairs.insert(42, "pre 42");
    old.devices[u"bob@example.org"_s].insert(5, { u"Laptop"_s, "bob key", "session", 2, 3, start });
    old.devices[u"alice@example.org"_s].insert(7, { {}, "alice key", {}, 0, 0, {} });

    const auto data = Storage::migrate(old);
    QCOMPARE(data.ownDevice, (Storage::OwnDevice { 123, "private", "public", 3, { { Version::Omemo2, 42 } } }));
    QCOMPARE(data.signedPreKeys, (QList<Storage::SignedPreKey> { { 3, "signed", start } }));
    QCOMPARE(data.preKeys, (QList<Storage::PreKey> { { Version::Omemo2, 41, "pre 41" }, { Version::Omemo2, 42, "pre 42" } }));
    QCOMPARE(data.devices,
             (QList<StorageDevice> {
                 { Version::Omemo2, u"alice@example.org"_s, 7, {}, "alice key", {}, 0, 0, {} },
                 { Version::Omemo2, u"bob@example.org"_s, 5, u"Laptop"_s, "bob key", "session", 2, 3, start },
             }));

    QVERIFY(!Storage::migrate({}).ownDevice);
}

void tst_OmemoStorage::migrateKeepsSession()
{
    // Data of the current manager has the same format as libomemo-c uses, so the migrated keys
    // and sessions continue to work, also for OMEMO 0.
    SignalContext context;
    const auto aliceJid = u"alice@example.org"_s;
    const auto bobJid = u"bob@example.org"_s;

    DeviceKeys alice { 1111, *context.generateIdentityKeyPair(), {}, 0 };
    VersionKeys aliceKeys;
    DeviceKeys bob { 2222, *context.generateIdentityKeyPair(), {}, 0 };
    VersionKeys bobKeys;
    expectVariant<KeyMaintenance::SignedPreKeyChanges>(KeyMaintenance::rotateSignedPreKey(context, alice, start));
    expectVariant<KeyMaintenance::SignedPreKeyChanges>(KeyMaintenance::rotateSignedPreKey(context, bob, start));
    expectVariant<QList<uint32_t>>(KeyMaintenance::refillPreKeys(context, aliceKeys));
    expectVariant<QList<uint32_t>>(KeyMaintenance::refillPreKeys(context, bobKeys));

    {
        SessionLayer aliceLayer(context, Version::Omemo2, alice, aliceKeys);
        SessionLayer bobLayer(context, Version::Omemo2, bob, bobKeys);
        expectVariant<Success>(bobLayer.buildSession({ aliceJid, alice.deviceId }, expectVariant<Bundle>(aliceLayer.ownBundle(1, 1))));
        const auto message = expectVariant<EncryptedKey>(bobLayer.encrypt({ aliceJid, alice.deviceId }, SecureByteArray(QByteArray("hi"))));
        expectVariant<DecryptedKey>(aliceLayer.decrypt({ bobJid, bob.deviceId }, message.data, true));
    }

    // Alice's data as stored by the current manager
    QXmppOmemoStorage::OmemoData old;
    old.ownDevice = QXmppOmemoStorage::OwnDevice { alice.deviceId, {}, alice.identity.privateKey.toByteArray(), alice.identity.publicKey, 1, aliceKeys.latestPreKeyId };
    old.signedPreKeyPairs.insert(1, { start, alice.signedPreKeys.value(1).record.toByteArray() });
    for (auto it = aliceKeys.preKeys.cbegin(); it != aliceKeys.preKeys.cend(); ++it) {
        old.preKeyPairs.insert(it.key(), it->toByteArray());
    }
    old.devices[bobJid].insert(bob.deviceId, { {}, bob.identity.publicKey, aliceKeys.sessions.value({ bobJid, bob.deviceId }), 0, 0, {} });

    const auto data = Storage::migrate(old);

    DeviceKeys migrated { data.ownDevice->id,
                          { SecureByteArray(data.ownDevice->privateIdentityKey), data.ownDevice->publicIdentityKey },
                          {},
                          data.ownDevice->latestSignedPreKeyId };
    for (const auto &key : data.signedPreKeys) {
        migrated.signedPreKeys.insert(key.id, { SecureByteArray(key.data), key.creationDate });
    }
    VersionKeys migrated2;
    for (const auto &key : data.preKeys) {
        migrated2.preKeys.insert(key.id, SecureByteArray(key.data));
    }
    for (const auto &device : data.devices) {
        migrated2.sessions.insert({ device.jid, device.id }, device.session);
    }

    // existing OMEMO 2 session
    {
        SessionLayer aliceLayer(context, Version::Omemo2, migrated, migrated2);
        SessionLayer bobLayer(context, Version::Omemo2, bob, bobKeys);
        const auto message = expectVariant<EncryptedKey>(aliceLayer.encrypt({ bobJid, bob.deviceId }, SecureByteArray(QByteArray("after migration"))));
        QVERIFY(!message.isKeyExchange);
        QCOMPARE(expectVariant<DecryptedKey>(bobLayer.decrypt({ aliceJid, alice.deviceId }, message.data, false)).data, SecureByteArray(QByteArray("after migration")));
    }

    // new OMEMO 0 session with the migrated identity and signed pre key
    VersionKeys migrated0;
    expectVariant<QList<uint32_t>>(KeyMaintenance::refillPreKeys(context, migrated0));
    VersionKeys bobKeys0;
    SessionLayer aliceLayer0(context, Version::Omemo0, migrated, migrated0);
    SessionLayer bobLayer0(context, Version::Omemo0, bob, bobKeys0);
    expectVariant<Success>(bobLayer0.buildSession({ aliceJid, alice.deviceId }, expectVariant<Bundle>(aliceLayer0.ownBundle(1, 1))));
    const auto message = expectVariant<EncryptedKey>(bobLayer0.encrypt({ aliceJid, alice.deviceId }, SecureByteArray(QByteArray("legacy"))));
    const auto decrypted = expectVariant<DecryptedKey>(aliceLayer0.decrypt({ bobJid, bob.deviceId }, message.data, true));
    QCOMPARE(decrypted.data, SecureByteArray(QByteArray("legacy")));
    QCOMPARE(decrypted.senderIdentityKey, bobLayer0.ownIdentityKey());
}

}  // namespace StorageTests

// ============================================================

namespace PubSubTests {

using namespace QXmpp;
using namespace QXmpp::Omemo;
using namespace QXmpp::Omemo::Private;
namespace OmemoPubSub = QXmpp::Omemo::Private::Pep;

using Entries = QList<DeviceList::Entry>;

static const auto PUBLISH_OPTIONS_OPEN = u"<publish-options><x xmlns='jabber:x:data' type='submit'>"
                                         "<field type='hidden' var='FORM_TYPE'><value>http://jabber.org/protocol/pubsub#publish-options</value></field>"
                                         "<field type='list-single' var='pubsub#access_model'><value>open</value></field>"
                                         "</x></publish-options>"_s;

static const auto V0_DEVICE_LIST_PUBLISH =
    u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
    "<publish node='eu.siacs.conversations.axolotl.devicelist'><item id='current'>"
    "<list xmlns='eu.siacs.conversations.axolotl'><device id='1'/><device id='2'/></list>"
    "</item></publish>"_s +
    PUBLISH_OPTIONS_OPEN + u"</pubsub></iq>"_s;

static const auto V0_BUNDLE =
    u"<bundle xmlns='eu.siacs.conversations.axolotl'>"
    "<signedPreKeyPublic signedPreKeyId='1'>c3Br</signedPreKeyPublic>"
    "<signedPreKeySignature>c2ln</signedPreKeySignature>"
    "<identityKey>aWs=</identityKey>"
    "<prekeys><preKeyPublic preKeyId='3'>cGsz</preKeyPublic></prekeys>"
    "</bundle>"_s;

static const auto V2_BUNDLE =
    u"<bundle xmlns='urn:xmpp:omemo:2'><ik>aWs=</ik><spk id='1'>c3Br</spk><spks>c2ln</spks>"
    "<prekeys><pk id='3'>cGsz</pk></prekeys></bundle>"_s;

static const OmemoPubSub::PublicBundle BUNDLE { "ik", 1, "spk", "sig", { { 3, "pk3" } } };

struct Fixture {
    Fixture()
    {
        test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
        pubSub = test.addNewExtension<QXmppPubSubManager>();
    }

    TestClient test;
    QXmppPubSubManager *pubSub;
};

class tst_OmemoPubSub : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void nodes();
    Q_SLOT void deviceListItem();
    Q_SLOT void requestDeviceList_data();
    Q_SLOT void requestDeviceList();
    Q_SLOT void requestDeviceListWithoutCurrentItem();
    Q_SLOT void requestDeviceListNotFound();
    Q_SLOT void requestDeviceListError();
    Q_SLOT void requestBundle_data();
    Q_SLOT void requestBundle();
    Q_SLOT void publishDeviceList();
    Q_SLOT void publishBundle_data();
    Q_SLOT void publishBundle();
    Q_SLOT void publishReconfiguresNode();
    Q_SLOT void publishError();
    Q_SLOT void deleteBundle_data();
    Q_SLOT void deleteBundle();
};

void tst_OmemoPubSub::nodes()
{
    QCOMPARE(OmemoPubSub::deviceListNode(Version::Omemo0), u"eu.siacs.conversations.axolotl.devicelist"_s);
    QCOMPARE(OmemoPubSub::deviceListNode(Version::Omemo2), u"urn:xmpp:omemo:2:devices"_s);
    QCOMPARE(OmemoPubSub::bundleNode(Version::Omemo0, 42), u"eu.siacs.conversations.axolotl.bundles:42"_s);
    QCOMPARE(OmemoPubSub::bundleNode(Version::Omemo2, 42), u"urn:xmpp:omemo:2:bundles"_s);
}

void tst_OmemoPubSub::deviceListItem()
{
    auto parse = [](const QString &xml) {
        OmemoPubSub::DeviceListItem item;
        item.parse(xmlToDom(xml));
        return item;
    };

    const auto v0 = parse(u"<item id='current'><list xmlns='eu.siacs.conversations.axolotl'><device id='1'/><device id='2'/></list></item>"_s);
    QCOMPARE(v0.version, std::optional(Version::Omemo0));
    QCOMPARE(v0.devices, (Entries { { 1, {} }, { 2, {} } }));

    const auto v2 = parse(u"<item id='current'><devices xmlns='urn:xmpp:omemo:2'><device id='1' label='Phone'/></devices></item>"_s);
    QCOMPARE(v2.version, std::optional(Version::Omemo2));
    QCOMPARE(v2.devices, (Entries { { 1, u"Phone"_s } }));

    QVERIFY(!parse(u"<item id='current'><list xmlns='urn:xmpp:omemo:2'/></item>"_s).version);
}

void tst_OmemoPubSub::requestDeviceList_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QString>("request");
    QTest::addColumn<QString>("items");
    QTest::addColumn<Entries>("expected");

    QTest::newRow("omemo0")
        << Version::Omemo0
        << u"<iq id='qx1' to='romeo@montague.lit' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<items node='eu.siacs.conversations.axolotl.devicelist'/></pubsub></iq>"_s
        << u"<items node='eu.siacs.conversations.axolotl.devicelist'><item id='current'>"
           "<list xmlns='eu.siacs.conversations.axolotl'><device id='1'/><device id='2'/></list>"
           "</item></items>"_s
        << Entries { { 1, {} }, { 2, {} } };
    QTest::newRow("omemo2")
        << Version::Omemo2
        << u"<iq id='qx1' to='romeo@montague.lit' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<items node='urn:xmpp:omemo:2:devices'/></pubsub></iq>"_s
        << u"<items node='urn:xmpp:omemo:2:devices'><item id='current'>"
           "<devices xmlns='urn:xmpp:omemo:2'><device id='1' label='Phone'/><device id='2'/></devices>"
           "</item></items>"_s
        << Entries { { 1, u"Phone"_s }, { 2, {} } };
}

void tst_OmemoPubSub::requestDeviceList()
{
    QFETCH(Version, version);
    QFETCH(QString, request);
    QFETCH(QString, items);
    QFETCH(Entries, expected);

    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, version).requestDeviceList(u"romeo@montague.lit"_s);
    f.test.expect(std::move(request));
    f.test.inject(u"<iq id='qx1' from='romeo@montague.lit' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"_s +
                  items + u"</pubsub></iq>"_s);
    QCOMPARE(expectFutureVariant<Entries>(task), expected);
}

void tst_OmemoPubSub::requestDeviceListWithoutCurrentItem()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo0).requestDeviceList(u"romeo@montague.lit"_s);
    f.test.ignore();
    f.test.inject(
        u"<iq id='qx1' from='romeo@montague.lit' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='eu.siacs.conversations.axolotl.devicelist'>"
        "<item id='old'><list xmlns='eu.siacs.conversations.axolotl'><device id='1'/></list></item>"
        "<item id='new'><list xmlns='eu.siacs.conversations.axolotl'><device id='2'/></list></item>"
        "</items></pubsub></iq>"_s);
    QCOMPARE(expectFutureVariant<Entries>(task), (Entries { { 2, {} } }));
}

void tst_OmemoPubSub::requestDeviceListNotFound()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo2).requestDeviceList(u"romeo@montague.lit"_s);
    f.test.ignore();
    f.test.inject(u"<iq id='qx1' from='romeo@montague.lit' type='error'>"
                  "<error type='cancel'><item-not-found xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>"_s);
    QVERIFY(expectFutureVariant<Entries>(task).isEmpty());
}

void tst_OmemoPubSub::requestDeviceListError()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo2).requestDeviceList(u"romeo@montague.lit"_s);
    f.test.ignore();
    f.test.inject(u"<iq id='qx1' from='romeo@montague.lit' type='error'>"
                  "<error type='auth'><forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>"_s);
    expectFutureVariant<QXmppError>(task);
}

void tst_OmemoPubSub::requestBundle_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QString>("request");
    QTest::addColumn<QString>("items");

    // OMEMO 0 has no fixed item ID: all items are requested and 'current' or the last one is used.
    const auto v0Request = u"<iq id='qx1' to='romeo@montague.lit' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                           "<items node='eu.siacs.conversations.axolotl.bundles:5'/></pubsub></iq>"_s;
    QTest::newRow("omemo0")
        << Version::Omemo0
        << v0Request
        << u"<items node='eu.siacs.conversations.axolotl.bundles:5'><item id='current'>"_s + V0_BUNDLE + u"</item></items>"_s;
    // e.g. Dino
    QTest::newRow("omemo0-random-id")
        << Version::Omemo0
        << v0Request
        << u"<items node='eu.siacs.conversations.axolotl.bundles:5'><item id='5f3c8a2e'>"_s + V0_BUNDLE + u"</item></items>"_s;
    QTest::newRow("omemo0-current-and-others")
        << Version::Omemo0
        << v0Request
        << u"<items node='eu.siacs.conversations.axolotl.bundles:5'><item id='current'>"_s + V0_BUNDLE + u"</item><item id='old'><bundle xmlns='eu.siacs.conversations.axolotl'/></item></items>"_s;
    QTest::newRow("omemo2")
        << Version::Omemo2
        << u"<iq id='qx1' to='romeo@montague.lit' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<items node='urn:xmpp:omemo:2:bundles'><item id='5'/></items></pubsub></iq>"_s
        << u"<items node='urn:xmpp:omemo:2:bundles'><item id='5'>"_s + V2_BUNDLE + u"</item></items>"_s;
}

void tst_OmemoPubSub::requestBundle()
{
    QFETCH(Version, version);
    QFETCH(QString, request);
    QFETCH(QString, items);

    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, version).requestBundle(u"romeo@montague.lit"_s, 5);
    f.test.expect(std::move(request));
    f.test.inject(u"<iq id='qx1' from='romeo@montague.lit' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"_s +
                  items + u"</pubsub></iq>"_s);
    QCOMPARE(expectFutureVariant<OmemoPubSub::PublicBundle>(task), BUNDLE);
}

void tst_OmemoPubSub::publishDeviceList()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo0).publishDeviceList({ { 1, u"Phone"_s }, { 2, {} } });
    f.test.expect(QString(V0_DEVICE_LIST_PUBLISH));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='result'/>"_s);
    expectFutureVariant<Success>(task);

    task = OmemoPubSub::Client(f.pubSub, Version::Omemo2).publishDeviceList({ { 1, u"Phone"_s }, { 2, {} } });
    f.test.expect(u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                  "<publish node='urn:xmpp:omemo:2:devices'><item id='current'>"
                  "<devices xmlns='urn:xmpp:omemo:2'><device id='1' label='Phone'/><device id='2'/></devices>"
                  "</item></publish>"_s +
                  PUBLISH_OPTIONS_OPEN + u"</pubsub></iq>"_s);
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='result'/>"_s);
    expectFutureVariant<Success>(task);
}

void tst_OmemoPubSub::publishBundle_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QString>("request");

    QTest::newRow("omemo0")
        << Version::Omemo0
        << u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<publish node='eu.siacs.conversations.axolotl.bundles:5'><item id='current'>"_s +
            V0_BUNDLE + u"</item></publish>"_s + PUBLISH_OPTIONS_OPEN + u"</pubsub></iq>"_s;
    QTest::newRow("omemo2")
        << Version::Omemo2
        << u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<publish node='urn:xmpp:omemo:2:bundles'><item id='5'>"_s +
            V2_BUNDLE +
            u"</item></publish><publish-options><x xmlns='jabber:x:data' type='submit'>"
            "<field type='hidden' var='FORM_TYPE'><value>http://jabber.org/protocol/pubsub#publish-options</value></field>"
            "<field type='list-single' var='pubsub#access_model'><value>open</value></field>"
            "<field type='text-single' var='pubsub#max_items'><value>max</value></field>"
            "</x></publish-options></pubsub></iq>"_s;
}

void tst_OmemoPubSub::publishBundle()
{
    QFETCH(Version, version);
    QFETCH(QString, request);

    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, version).publishBundle(5, BUNDLE);
    f.test.expect(std::move(request));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='result'/>"_s);
    expectFutureVariant<Success>(task);
}

void tst_OmemoPubSub::publishReconfiguresNode()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo0).publishDeviceList({ { 1, {} }, { 2, {} } });
    f.test.expect(QString(V0_DEVICE_LIST_PUBLISH));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='error'><error type='cancel'>"
                  "<conflict xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                  "<precondition-not-met xmlns='http://jabber.org/protocol/pubsub#errors'/>"
                  "</error></iq>"_s);

    f.test.expect(u"<iq id='qx2' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub#owner'>"
                  "<configure node='eu.siacs.conversations.axolotl.devicelist'><x xmlns='jabber:x:data' type='submit'>"
                  "<field type='hidden' var='FORM_TYPE'><value>http://jabber.org/protocol/pubsub#node_config</value></field>"
                  "<field type='list-single' var='pubsub#access_model'><value>open</value></field>"
                  "</x></configure></pubsub></iq>"_s);
    f.test.inject(u"<iq id='qx2' from='juliet@capulet.lit' type='result'/>"_s);

    f.test.expect(QString(V0_DEVICE_LIST_PUBLISH));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='result'/>"_s);
    expectFutureVariant<Success>(task);
}

void tst_OmemoPubSub::publishError()
{
    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, Version::Omemo0).publishDeviceList({ { 1, {} }, { 2, {} } });
    f.test.expect(QString(V0_DEVICE_LIST_PUBLISH));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='error'><error type='cancel'>"
                  "<feature-not-implemented xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                  "</error></iq>"_s);
    expectFutureVariant<QXmppError>(task);
    f.test.expectNoPacket();
}

void tst_OmemoPubSub::deleteBundle_data()
{
    QTest::addColumn<Version>("version");
    QTest::addColumn<QString>("request");

    // OMEMO 0 has a node per device
    QTest::newRow("omemo0")
        << Version::Omemo0
        << u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub#owner'>"
           "<delete node='eu.siacs.conversations.axolotl.bundles:5'/></pubsub></iq>"_s;
    QTest::newRow("omemo2")
        << Version::Omemo2
        << u"<iq id='qx1' to='juliet@capulet.lit' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
           "<retract node='urn:xmpp:omemo:2:bundles'><item id='5'/></retract></pubsub></iq>"_s;
}

void tst_OmemoPubSub::deleteBundle()
{
    QFETCH(Version, version);
    QFETCH(QString, request);

    Fixture f;
    auto task = OmemoPubSub::Client(f.pubSub, version).deleteBundle(5);
    f.test.expect(std::move(request));
    f.test.inject(u"<iq id='qx1' from='juliet@capulet.lit' type='result'/>"_s);
    expectFutureVariant<Success>(task);
}

}  // namespace PubSubTests

// ============================================================

using namespace QXmpp;
using namespace QXmpp::Omemo;

static const auto OWN_JID = u"juliet@capulet.lit"_s;
static const auto CONTACT_JID = u"romeo@montague.lit"_s;
static const auto V0_DEVICE_LIST = u"eu.siacs.conversations.axolotl.devicelist"_s;
static const auto V2_DEVICE_LIST = u"urn:xmpp:omemo:2:devices"_s;
static const auto V2_BUNDLES = u"urn:xmpp:omemo:2:bundles"_s;

static QString v0Bundles(uint32_t id)
{
    return u"eu.siacs.conversations.axolotl.bundles:"_s + QString::number(id);
}

static QString v0List(const QList<uint32_t> &ids)
{
    QString list = u"<list xmlns='eu.siacs.conversations.axolotl'>"_s;
    for (auto id : ids) {
        list += u"<device id='"_s + QString::number(id) + u"'/>"_s;
    }
    return list + u"</list>"_s;
}

static QString v2List(const QList<std::pair<uint32_t, QString>> &devices)
{
    QString list = u"<devices xmlns='urn:xmpp:omemo:2'>"_s;
    for (const auto &[id, label] : devices) {
        list += u"<device id='"_s + QString::number(id) + u"'"_s + (label.isEmpty() ? QString() : u" label='"_s + label + u"'"_s) + u"/>"_s;
    }
    return list + u"</devices>"_s;
}

static QString itemsEvent(const QString &from, const QString &node, const QString &payload)
{
    return u"<message from='"_s + from + u"' to='juliet@capulet.lit/balcony' type='headline'>"
                                         "<event xmlns='http://jabber.org/protocol/pubsub#event'><items node='"_s +
        node + u"'><item id='current'>"_s + payload + u"</item></items></event></message>"_s;
}

// device IDs in a device list payload
static QList<uint32_t> listedIds(const QString &payload)
{
    QList<uint32_t> ids;
    const auto list = xmlToDom(payload);
    for (auto device = list.firstChildElement(u"device"_s); !device.isNull(); device = device.nextSiblingElement(u"device"_s)) {
        ids.append(device.attribute(u"id"_s).toUInt());
    }
    return ids;
}

struct Account {
    Account(std::shared_ptr<Storage> storage = {}, FakePubSubService *sharedService = nullptr, const QString &jid = OWN_JID + u"/balcony"_s)
        : storage(storage ? storage : std::make_shared<MemoryStorage>()), service(sharedService ? *sharedService : ownService)
    {
        client.configuration().setJid(jid);
        service.addClient(client);
        client.addNewExtension<QXmppDiscoveryManager>();
        client.addNewExtension<QXmppPubSubManager>();
        trust = client.addNewExtension<QXmppAtmManager>(&trustStorage);
        manager = client.addNewExtension<Manager>(this->storage);
        trust->setSecurityPolicy(E2ee::Omemo0.toString(), Toakafa);
        trust->setSecurityPolicy(E2ee::Omemo2.toString(), Toakafa);

        // the version is selected by the test
        client.hooks(&owner).add<QXmpp::Messages::SelectEncryption>([this](QXmpp::Messages::EncryptionSelection &selection) {
            if (encryption) {
                selection.setEncryption(*encryption == Version::Omemo0 ? E2ee::Omemo0 : E2ee::Omemo2);
            }
            return QXmpp::Private::makeReadyTask();
        });
    }

    ~Account() { service.removeClient(client); }

    // Logs in, which sets up the manager.
    Result<> setUp(const QString &label = {})
    {
        if (!label.isEmpty()) {
            manager->setDeviceLabel(label);
        }
        QSignalSpy errors(manager, &Manager::errorOccurred);
        client.setStreamManagementState(QXmppClient::NewStream);
        client.simulateConnected();
        service.process();
        if (!errors.isEmpty()) {
            return errors.constFirst().at(0).value<QXmppError>();
        }
        return isReady() ? Result<>(Success()) : Result<>(QXmppError { u"not ready"_s, {} });
    }

    bool isReady() const { return manager->state().value() == Manager::State::Ready; }
    uint32_t deviceId() const { return manager->ownDeviceId().value(); }

    QString jid() { return client.configuration().jidBare(); }

    // Sends a message with the selected encryption and returns the sent XML.
    QString sendMessage(const QString &to, const QString &body)
    {
        QXmppMessage message;
        message.setTo(to);
        message.setBody(body);
        auto task = client.send(std::move(message));
        service.process();
        // Without a stream, the task only finishes on errors.
        if (auto *error = task.isFinished() ? std::get_if<QXmppError>(&task.result()) : nullptr) {
            sendError = error->description;
            return {};
        }
        return takeMessage();
    }

    // the last message sent by the client
    QString takeMessage()
    {
        const auto packets = service.takeOtherPackets(client);
        return packets.isEmpty() ? QString() : packets.constLast();
    }

    // Injects a message sent by another account and returns the received messages.
    QList<QXmppMessage> receive(Account &sender, QString xml)
    {
        QSignalSpy spy(&client, &QXmppClient::messageReceived);
        xml.replace(u"<message "_s, u"<message from='"_s + sender.client.configuration().jid() + u"' "_s);
        client.injectMessage(xml);
        service.process();
        QList<QXmppMessage> messages;
        for (const auto &arguments : std::as_const(spy)) {
            messages.append(arguments.at(0).value<QXmppMessage>());
        }
        return messages;
    }

    TestClient client;
    QObject owner;
    QXmppAtmTrustMemoryStorage trustStorage;
    QXmppAtmManager *trust;
    std::shared_ptr<Storage> storage;
    FakePubSubService ownService;
    FakePubSubService &service;
    Manager *manager;
    std::optional<Version> encryption;
    QString sendError;
};

static QString versionNamespace(Version version)
{
    return version == Version::Omemo0 ? E2ee::Omemo0.toString() : E2ee::Omemo2.toString();
}

static void addVersions()
{
    QTest::addColumn<Version>("version");
    QTest::newRow("omemo0") << Version::Omemo0;
    QTest::newRow("omemo2") << Version::Omemo2;
}

class tst_OmemoManager2 : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void discoFeatures();
    Q_SLOT void setUpPublishes();
    Q_SLOT void setUpOnlyEnabledVersions();
    Q_SLOT void setUpKeepsOtherOwnDevices();
    Q_SLOT void setUpReconfiguresNodes();
    Q_SLOT void load();
    Q_SLOT void ownDeviceIsRelisted();
    Q_SLOT void ownDeviceListIsRepublishedAfterDeletion();
    Q_SLOT void contactDeviceLists();
    Q_SLOT void removedDevicesExpire();
    Q_SLOT void supportedVersions();
    Q_SLOT void republishOnLogin();
    Q_SLOT void messages_data();
    Q_SLOT void messages();
    Q_SLOT void untrustedDevicesAreSkipped_data();
    Q_SLOT void untrustedDevicesAreSkipped();
    Q_SLOT void unlistedDevicesAreNotEncryptedFor_data();
    Q_SLOT void unlistedDevicesAreNotEncryptedFor();
    Q_SLOT void ownDevicesReceiveCopies_data();
    Q_SLOT void ownDevicesReceiveCopies();
    Q_SLOT void heartbeat_data();
    Q_SLOT void heartbeat();
    Q_SLOT void keyExchangeWithAnotherKey_data();
    Q_SLOT void keyExchangeWithAnotherKey();
    Q_SLOT void deviceLabel();
    Q_SLOT void enabledVersionsAtRuntime();
};

void tst_OmemoManager2::discoFeatures()
{
    Account account;
    auto features = account.client.findExtension<QXmppDiscoveryManager>()->buildClientInfo().features();
    QVERIFY(features.contains(V0_DEVICE_LIST + u"+notify"_s));
    QVERIFY(features.contains(V2_DEVICE_LIST + u"+notify"_s));

    account.manager->setEnabledVersions({ Version::Omemo2 });
    features = account.client.findExtension<QXmppDiscoveryManager>()->buildClientInfo().features();
    QVERIFY(!features.contains(V0_DEVICE_LIST + u"+notify"_s));
    QVERIFY(features.contains(V2_DEVICE_LIST + u"+notify"_s));
}

void tst_OmemoManager2::setUpPublishes()
{
    Account account;
    expectVariant<Success>(account.setUp(u"Phone"_s));

    const auto id = account.manager->ownDeviceId().value();
    QVERIFY(id > 0);
    QVERIFY(account.isReady());
    QCOMPARE(account.manager->ownKey().value().size(), 32);

    // device lists
    auto &service = account.service;
    QCOMPARE(listedIds(service.itemPayload(OWN_JID, V0_DEVICE_LIST, u"current"_s)), QList { id });
    QCOMPARE(listedIds(service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s)), QList { id });
    QVERIFY(service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s).contains(u"label=\"Phone\""_s));
    QCOMPARE(service.node(OWN_JID, V0_DEVICE_LIST)->accessModel, u"open"_s);
    QCOMPARE(service.node(OWN_JID, V2_DEVICE_LIST)->accessModel, u"open"_s);

    // bundles
    const auto v0Bundle = xmlToDom(service.itemPayload(OWN_JID, v0Bundles(id), u"current"_s));
    QCOMPARE(v0Bundle.firstChildElement(u"prekeys"_s).childNodes().size(), 100);
    QCOMPARE(QByteArray::fromBase64(v0Bundle.firstChildElement(u"identityKey"_s).text().toLatin1()).size(), 33);
    QCOMPARE(service.node(OWN_JID, v0Bundles(id))->accessModel, u"open"_s);

    const auto v2Bundle = xmlToDom(service.itemPayload(OWN_JID, V2_BUNDLES, QString::number(id)));
    QCOMPARE(v2Bundle.firstChildElement(u"prekeys"_s).childNodes().size(), 100);
    QCOMPARE(QByteArray::fromBase64(v2Bundle.firstChildElement(u"ik"_s).text().toLatin1()), account.manager->ownKey().value());
    QCOMPARE(service.node(OWN_JID, V2_BUNDLES)->accessModel, u"open"_s);
    QCOMPARE(service.node(OWN_JID, V2_BUNDLES)->maxItems, u"max"_s);

    // storage
    const auto data = account.storage->allData().result();
    QCOMPARE(data.ownDevice->id, id);
    QCOMPARE(data.signedPreKeys.size(), 1);
    QCOMPARE(std::ranges::count(data.preKeys, Version::Omemo0, &Storage::PreKey::version), 100);
    QCOMPARE(std::ranges::count(data.preKeys, Version::Omemo2, &Storage::PreKey::version), 100);

    // set up again: nothing new is created
    expectVariant<Success>(account.setUp());
    QCOMPARE(account.manager->ownDeviceId().value(), id);
    QCOMPARE(account.storage->allData().result().preKeys, data.preKeys);
}

void tst_OmemoManager2::setUpOnlyEnabledVersions()
{
    Account account;
    account.manager->setEnabledVersions({ Version::Omemo0 });
    expectVariant<Success>(account.setUp());

    QCOMPARE(account.service.nodeNames(OWN_JID), (QList { v0Bundles(account.manager->ownDeviceId().value()), V0_DEVICE_LIST }));
    const auto data = account.storage->allData().result();
    QCOMPARE(std::ranges::count(data.preKeys, Version::Omemo2, &Storage::PreKey::version), 0);
}

void tst_OmemoManager2::setUpKeepsOtherOwnDevices()
{
    Account account;
    account.service.setItem(OWN_JID, V0_DEVICE_LIST, u"current"_s, v0List({ 1111 }));
    account.service.setItem(OWN_JID, V2_DEVICE_LIST, u"current"_s, v2List({ { 2222, u"Laptop"_s } }));
    expectVariant<Success>(account.setUp());

    const auto id = account.manager->ownDeviceId().value();
    QVERIFY(id != 1111 && id != 2222);
    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V0_DEVICE_LIST, u"current"_s)), (QList<uint32_t> { id, 1111 }));
    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s)), (QList<uint32_t> { id, 2222 }));
    QVERIFY(account.service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s).contains(u"label=\"Laptop\""_s));
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, OWN_JID), QList<uint32_t> { 1111 });
    QCOMPARE(account.manager->deviceIds(Version::Omemo2, OWN_JID), QList<uint32_t> { 2222 });
}

void tst_OmemoManager2::setUpReconfiguresNodes()
{
    // nodes created by other clients with the default access model
    Account account;
    account.service.createNode(OWN_JID, V0_DEVICE_LIST, u"presence"_s, u"1"_s);
    account.service.createNode(OWN_JID, V2_DEVICE_LIST, u"presence"_s, u"1"_s);
    expectVariant<Success>(account.setUp());

    QCOMPARE(account.service.node(OWN_JID, V0_DEVICE_LIST)->accessModel, u"open"_s);
    QCOMPARE(account.service.node(OWN_JID, V2_DEVICE_LIST)->accessModel, u"open"_s);
}

void tst_OmemoManager2::load()
{
    auto storage = std::make_shared<MemoryStorage>();
    uint32_t id;
    QByteArray key;
    {
        Account account(storage);
        account.service.setItem(OWN_JID, V2_DEVICE_LIST, u"current"_s, v2List({ { 2222, {} } }));
        QCOMPARE(account.manager->state().value(), Manager::State::NotSetUp);
        expectVariant<Success>(account.setUp());
        id = account.manager->ownDeviceId().value();
        key = account.manager->ownKey().value();
    }

    // loaded when added to the client
    Account account(storage);
    QVERIFY(account.isReady());
    QCOMPARE(account.manager->ownDeviceId().value(), id);
    QCOMPARE(account.manager->ownKey().value(), key);
    QCOMPARE(account.manager->deviceIds(Version::Omemo2, OWN_JID), QList<uint32_t> { 2222 });
}

void tst_OmemoManager2::ownDeviceIsRelisted()
{
    Account account;
    expectVariant<Success>(account.setUp());
    const auto id = account.manager->ownDeviceId().value();

    // another client publishes a device list without this device
    account.service.setItem(OWN_JID, V0_DEVICE_LIST, u"current"_s, v0List({ 1111 }));
    account.client.inject(itemsEvent(OWN_JID, V0_DEVICE_LIST, v0List({ 1111 })));
    account.service.process();

    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V0_DEVICE_LIST, u"current"_s)), (QList<uint32_t> { id, 1111 }));
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, OWN_JID), QList<uint32_t> { 1111 });

    // the republished list does not lead to another publication
    account.service.resetRequestCounts();
    account.service.process();
    QCOMPARE(account.service.requestCount(u"publish"_s), 0);
}

void tst_OmemoManager2::ownDeviceListIsRepublishedAfterDeletion()
{
    Account account;
    expectVariant<Success>(account.setUp());

    account.client.inject(u"<message from='juliet@capulet.lit' to='juliet@capulet.lit/balcony' type='headline'>"
                          "<event xmlns='http://jabber.org/protocol/pubsub#event'><delete node='urn:xmpp:omemo:2:devices'/></event></message>"_s);
    account.service.resetRequestCounts();
    account.service.process();
    QCOMPARE(account.service.requestCount(u"publish"_s), 1);
}

void tst_OmemoManager2::contactDeviceLists()
{
    Account account;
    expectVariant<Success>(account.setUp());
    QSignalSpy spy(account.manager, &Manager::devicesChanged);

    account.client.inject(itemsEvent(CONTACT_JID, V0_DEVICE_LIST, v0List({ 1, 2 })));
    account.client.inject(itemsEvent(CONTACT_JID, V2_DEVICE_LIST, v2List({ { 3, u"Phone"_s } })));
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, CONTACT_JID), (QList<uint32_t> { 1, 2 }));
    QCOMPARE(account.manager->deviceIds(Version::Omemo2, CONTACT_JID), QList<uint32_t> { 3 });
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy.at(0).at(0).value<Version>(), Version::Omemo0);
    QCOMPARE(spy.at(0).at(1).toString(), CONTACT_JID);

    // the same list again changes nothing
    account.client.inject(itemsEvent(CONTACT_JID, V0_DEVICE_LIST, v0List({ 1, 2 })));
    QCOMPARE(spy.size(), 2);

    account.client.inject(itemsEvent(CONTACT_JID, V0_DEVICE_LIST, v0List({ 2 })));
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, CONTACT_JID), QList<uint32_t> { 2 });
    QCOMPARE(spy.size(), 3);

    // stored
    const auto devices = account.storage->allData().result().devices;
    QCOMPARE(std::ranges::count(devices, CONTACT_JID, &Storage::Device::jid), 3);

    // nothing is published for contacts
    account.service.resetRequestCounts();
    account.service.process();
    QCOMPARE(account.service.requestCount(u"publish"_s), 0);
}

void tst_OmemoManager2::removedDevicesExpire()
{
    auto storage = std::make_shared<MemoryStorage>();
    QDateTime start = QDateTime::currentDateTimeUtc();
    {
        Account account(storage);
        expectVariant<Success>(account.setUp());
        account.manager->d->fixedTime = start;
        account.client.inject(itemsEvent(CONTACT_JID, V0_DEVICE_LIST, v0List({ 1, 2 })));
        account.client.inject(itemsEvent(CONTACT_JID, V0_DEVICE_LIST, v0List({ 2 })));
    }

    // not yet expired
    {
        Account account(storage);
        account.manager->d->fixedTime = start.addDays(7 * 12 - 1);
        account.manager->d->load();
        QCOMPARE(std::ranges::count(storage->allData().result().devices, CONTACT_JID, &Storage::Device::jid), 2);
    }

    Account account(storage);
    account.manager->d->fixedTime = start.addDays(7 * 12);
    account.manager->d->load();
    const auto devices = storage->allData().result().devices;
    QCOMPARE(std::ranges::count(devices, CONTACT_JID, &Storage::Device::jid), 1);
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, CONTACT_JID), QList<uint32_t> { 2 });
}

void tst_OmemoManager2::supportedVersions()
{
    Account account;
    expectVariant<Success>(account.setUp());
    account.service.setItem(CONTACT_JID, V0_DEVICE_LIST, u"current"_s, v0List({ 1 }));

    auto task = account.manager->supportedVersions(CONTACT_JID);
    account.service.process();
    QCOMPARE(expectFutureVariant<QList<Version>>(task), QList { Version::Omemo0 });
    QCOMPARE(account.manager->deviceIds(Version::Omemo0, CONTACT_JID), QList<uint32_t> { 1 });

    account.service.setItem(CONTACT_JID, V2_DEVICE_LIST, u"current"_s, v2List({ { 2, {} } }));
    task = account.manager->supportedVersions(CONTACT_JID);
    account.service.process();
    QCOMPARE(expectFutureVariant<QList<Version>>(task), (QList { Version::Omemo0, Version::Omemo2 }));

    task = account.manager->supportedVersions(u"mercutio@verona.lit"_s);
    account.service.process();
    QVERIFY(expectFutureVariant<QList<Version>>(task).isEmpty());
}

void tst_OmemoManager2::republishOnLogin()
{
    Account account;
    expectVariant<Success>(account.setUp());
    const auto id = account.manager->ownDeviceId().value();

    // e.g. removed by another client
    account.service.setNotificationsEnabled(false);
    account.service.setItem(OWN_JID, V2_DEVICE_LIST, u"current"_s, v2List({ { 2222, {} } }));

    account.client.setStreamManagementState(QXmppClient::NewStream);
    account.client.simulateConnected();
    account.service.process();
    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s)), (QList<uint32_t> { id, 2222 }));

    // not after resuming the stream
    account.service.resetRequestCounts();
    account.client.setStreamManagementState(QXmppClient::ResumedStream);
    account.client.simulateConnected();
    account.service.process();
    QCOMPARE(account.service.requestCount(u"publish"_s), 0);
}

void tst_OmemoManager2::messages_data()
{
    addVersions();
}

void tst_OmemoManager2::messages()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;
    bob.encryption = version;

    // The first message is a key exchange.
    const auto first = alice.sendMessage(bob.jid(), u"Hello Bob"_s);
    QVERIFY2(!first.isEmpty(), qPrintable(alice.sendError));
    QVERIFY(!first.contains(u"Hello Bob"_s));
    QVERIFY(first.contains(versionNamespace(version)));
    QVERIFY(first.contains(u"prekey=\"true\""_s) || first.contains(u"kex=\"true\""_s));

    server.resetRequestCounts();
    auto received = bob.receive(alice, first);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received.constFirst().body(), u"Hello Bob"_s);
    // The used pre key is replaced in the bundle.
    QCOMPARE(server.requestCount(u"publish"_s), 1);
    const auto metadata = *received.constFirst().e2eeMetadata();
    QCOMPARE(metadata.encryptionNamespace(), versionNamespace(version));
    QCOMPARE(metadata.senderKey(), alice.manager->d->versions.at(version).sessions->ownIdentityKey());
    QCOMPARE(bob.trust->trustLevel(versionNamespace(version), alice.jid(), metadata.senderKey()).result(), TrustLevel::AutomaticallyTrusted);

    // Bob completes the key exchange with an empty message, which is not delivered.
    const auto empty = bob.takeMessage();
    QVERIFY(!empty.isEmpty());
    QVERIFY(alice.receive(bob, empty).isEmpty());

    // no key exchange anymore
    const auto second = alice.sendMessage(bob.jid(), u"How are you?"_s);
    QVERIFY(!second.contains(u"prekey=\"true\""_s) && !second.contains(u"kex=\"true\""_s));
    received = bob.receive(alice, second);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received.constFirst().body(), u"How are you?"_s);

    // answer
    const auto answer = bob.sendMessage(alice.jid(), u"Fine"_s);
    QVERIFY2(!answer.isEmpty(), qPrintable(bob.sendError));
    received = alice.receive(bob, answer);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received.constFirst().body(), u"Fine"_s);

    // the replaced pre key is stored
    const auto preKeys = bob.storage->allData().result().preKeys;
    QCOMPARE(std::ranges::count(preKeys, version, &Storage::PreKey::version), 100);
}

void tst_OmemoManager2::untrustedDevicesAreSkipped_data()
{
    addVersions();
}

void tst_OmemoManager2::untrustedDevicesAreSkipped()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;

    // Alice has authenticated another key of Bob, so new keys are distrusted.
    alice.trust->addKeys(versionNamespace(version), bob.jid(), { "other key" }, TrustLevel::Authenticated);
    QVERIFY(alice.sendMessage(bob.jid(), u"Hello"_s).isEmpty());
    QCOMPARE(alice.sendError, u"There is no device of the recipients with an accepted trust level"_s);

    // trusted manually
    const auto bobKey = bob.manager->d->versions.at(version).sessions->ownIdentityKey();
    QCOMPARE(alice.trust->trustLevel(versionNamespace(version), bob.jid(), bobKey).result(), TrustLevel::AutomaticallyDistrusted);
    alice.trust->setTrustLevel(versionNamespace(version), { { bob.jid(), bobKey } }, TrustLevel::ManuallyTrusted);
    QVERIFY(!alice.sendMessage(bob.jid(), u"Hello"_s).isEmpty());
}

void tst_OmemoManager2::unlistedDevicesAreNotEncryptedFor_data()
{
    addVersions();
}

void tst_OmemoManager2::unlistedDevicesAreNotEncryptedFor()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;
    bob.encryption = version;

    // Bob's device is removed from his device list, but its bundle is still there.
    server.setNotificationsEnabled(false);
    server.setItem(bob.jid(), version == Version::Omemo0 ? V0_DEVICE_LIST : V2_DEVICE_LIST, u"current"_s, version == Version::Omemo0 ? v0List({ 1 }) : v2List({ { 1, {} } }));
    server.setNotificationsEnabled(true);
    alice.client.inject(itemsEvent(bob.jid(), version == Version::Omemo0 ? V0_DEVICE_LIST : V2_DEVICE_LIST, version == Version::Omemo0 ? v0List({ 1 }) : v2List({ { 1, {} } })));
    QVERIFY(!alice.manager->deviceIds(version, bob.jid()).contains(bob.manager->ownDeviceId().value()));

    // A message from the device is decrypted, but the device is still not encrypted for.
    const auto message = bob.sendMessage(alice.jid(), u"Hi"_s);
    QCOMPARE(alice.receive(bob, message).size(), 1);
    alice.takeMessage();
    QVERIFY(!alice.manager->deviceIds(version, bob.jid()).contains(bob.manager->ownDeviceId().value()));
    QVERIFY(alice.sendMessage(bob.jid(), u"Hello"_s).isEmpty());
}

void tst_OmemoManager2::ownDevicesReceiveCopies_data()
{
    addVersions();
}

void tst_OmemoManager2::ownDevicesReceiveCopies()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account alicePhone(nullptr, &server, u"alice@wonderland.lit/phone"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(alicePhone.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;

    QCOMPARE(alice.manager->deviceIds(version, alice.jid()), QList { alicePhone.manager->ownDeviceId().value() });
    const auto message = alice.sendMessage(bob.jid(), u"Hello"_s);

    // e.g. a carbon copy
    auto received = alicePhone.receive(alice, message);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received.constFirst().body(), u"Hello"_s);
}

void tst_OmemoManager2::heartbeat_data()
{
    addVersions();
}

void tst_OmemoManager2::heartbeat()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;

    // the key exchange is answered
    bob.receive(alice, alice.sendMessage(bob.jid(), u"0"_s));
    alice.receive(bob, bob.takeMessage());

    for (int i = 1; i < UNRESPONDED_STANZAS_UNTIL_HEARTBEAT; ++i) {
        bob.receive(alice, alice.sendMessage(bob.jid(), QString::number(i)));
        QVERIFY(bob.takeMessage().isEmpty());
    }
    bob.receive(alice, alice.sendMessage(bob.jid(), u"last"_s));
    QVERIFY(!bob.takeMessage().isEmpty());
}

void tst_OmemoManager2::keyExchangeWithAnotherKey_data()
{
    addVersions();
}

void tst_OmemoManager2::keyExchangeWithAnotherKey()
{
    QFETCH(Version, version);

    FakePubSubService server;
    Account alice(nullptr, &server, u"alice@wonderland.lit/rabbithole"_s);
    Account bob(nullptr, &server, u"bob@builder.lit/site"_s);
    expectVariant<Success>(alice.setUp());
    expectVariant<Success>(bob.setUp());
    alice.encryption = version;
    bob.encryption = version;
    bob.receive(alice, alice.sendMessage(bob.jid(), u"Hello"_s));
    alice.receive(bob, bob.takeMessage());
    const auto bobKey = alice.manager->d->storedDevice(version, bob.jid(), bob.manager->ownDeviceId().value())->keyId;
    QVERIFY(!bobKey.isEmpty());

    // Another device, e.g. created by the server, uses the ID of Bob's device with another
    // identity key.
    Account mallory(nullptr, &server, u"bob@builder.lit/mallory"_s);
    expectVariant<Success>(mallory.setUp());
    mallory.encryption = version;
    mallory.manager->d->device.deviceId = bob.manager->ownDeviceId().value();
    const auto forged = mallory.sendMessage(alice.jid(), u"Forged"_s);
    QVERIFY2(!forged.isEmpty(), qPrintable(mallory.sendError));
    for (const auto &message : alice.receive(mallory, forged)) {
        QVERIFY(message.body() != u"Forged"_s);
    }
    QCOMPARE(alice.manager->d->storedDevice(version, bob.jid(), bob.manager->ownDeviceId().value())->keyId, bobKey);

    // The session with Bob's device is kept.
    const auto message = bob.sendMessage(alice.jid(), u"Still here"_s);
    const auto received = alice.receive(bob, message);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received.constFirst().body(), u"Still here"_s);
}

void tst_OmemoManager2::deviceLabel()
{
    QCoreApplication::setApplicationName(u"Wonderland"_s);
    QVERIFY(Manager::defaultDeviceLabel().startsWith(u"Wonderland ("_s));
    // the display name of a QGuiApplication is preferred
    qApp->setProperty("applicationDisplayName", u"Wonderland Gardens"_s);
    QVERIFY(Manager::defaultDeviceLabel().startsWith(u"Wonderland Gardens ("_s));
    qApp->setProperty("applicationDisplayName", QVariant());
    QVERIFY(Manager::defaultDeviceLabel().startsWith(u"Wonderland ("_s));

    FakePubSubService server;
    const auto published = [&] { return server.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s); };
    auto storage = std::make_shared<MemoryStorage>();
    {
        Account account(storage, &server);
        expectVariant<Success>(account.setUp());
        QCOMPARE(account.manager->deviceLabel(), Manager::defaultDeviceLabel());
        QVERIFY(published().contains(u"label=\""_s + Manager::defaultDeviceLabel() + u'"'));

        // published immediately while being logged in
        account.manager->setDeviceLabel(u"Laptop"_s);
        server.process();
        QVERIFY(published().contains(u"label=\"Laptop\""_s));
    }

    // The label is not stored, the default label is published again on the next login.
    Account account(storage, &server);
    expectVariant<Success>(account.setUp());
    QVERIFY(published().contains(u"label=\""_s + Manager::defaultDeviceLabel() + u'"'));
}

void tst_OmemoManager2::enabledVersionsAtRuntime()
{
    Account account;
    account.manager->setEnabledVersions({ Version::Omemo2 });
    expectVariant<Success>(account.setUp());
    const auto id = account.deviceId();
    QVERIFY(!account.service.node(OWN_JID, V0_DEVICE_LIST));

    account.manager->setEnabledVersions({ Version::Omemo0, Version::Omemo2 });
    account.service.process();
    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V0_DEVICE_LIST, u"current"_s)), QList { id });
    QVERIFY(account.service.node(OWN_JID, v0Bundles(id)));

    account.manager->setEnabledVersions({ Version::Omemo0 });
    account.service.process();
    QVERIFY(listedIds(account.service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s)).isEmpty());
    QVERIFY(account.service.itemPayload(OWN_JID, V2_BUNDLES, QString::number(id)).isEmpty());

    // disabled while being logged out
    account.client.simulateDisconnected();
    account.manager->setEnabledVersions({ Version::Omemo2 });
    expectVariant<Success>(account.setUp());
    QVERIFY(listedIds(account.service.itemPayload(OWN_JID, V0_DEVICE_LIST, u"current"_s)).isEmpty());
    QCOMPARE(listedIds(account.service.itemPayload(OWN_JID, V2_DEVICE_LIST, u"current"_s)), QList { id });
}

QXMPP_TEST_MAIN(tst_QXmppOmemoManager, tst_QXmppOmemoMemoryStorage, SessionLayerTests::tst_OmemoSession, PayloadCipherTests::tst_OmemoPayloadCipher, KeyMaintenanceTests::tst_OmemoKeyMaintenance, DeviceListTests::tst_OmemoDeviceList, StorageTests::tst_OmemoStorage, PubSubTests::tst_OmemoPubSub, tst_OmemoManager2)

#include "OmemoManager.moc"

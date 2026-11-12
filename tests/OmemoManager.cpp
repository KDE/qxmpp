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

#include "IntegrationTesting.h"
#include "OmemoPayloadCipher.h"
#include "OmemoSession.h"
#include "StringLiterals.h"
#include "util.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QObject>
#include <QScopeGuard>
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
using namespace QXmpp::Omemo::Private;

}  // namespace SessionLayerTests

Q_DECLARE_METATYPE(SessionLayerTests::Version)

namespace SessionLayerTests {

constexpr uint32_t SIGNED_PRE_KEY_ID = 1;
constexpr uint32_t PRE_KEY_COUNT = 5;

struct Device {
    Device(const SignalContext &context, QString jid, std::optional<IdentityKeyPair> identity = {})
        : jid(std::move(jid))
    {
        keys.deviceId = *context.generateDeviceId();
        keys.identity = identity ? *identity : *context.generateIdentityKeyPair();
        keys.signedPreKeys.insert(SIGNED_PRE_KEY_ID, *context.generateSignedPreKey(keys.identity, SIGNED_PRE_KEY_ID, 0));
        keys.preKeys = context.generatePreKeys(1, PRE_KEY_COUNT);
    }

    Address address() const { return { jid, keys.deviceId }; }

    QString jid;
    KeyStore keys;
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
    SessionLayer layer(*m_context, version, device.keys);

    const auto bundle = expectVariant<Bundle>(layer.ownBundle(SIGNED_PRE_KEY_ID, 1));
    QCOMPARE(bundle.signedPreKeyId, SIGNED_PRE_KEY_ID);
    QCOMPARE(bundle.preKeyId, 1u);
    QCOMPARE(bundle.signedPreKeySignature.size(), 64);

    if (version == Version::Omemo2) {
        // Ed25519 identity key, Curve25519 pre keys without prefix
        QCOMPARE(bundle.identityKey, device.keys.identity.publicKey);
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
    SessionLayer alice(*m_context, version, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.keys);

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
    QCOMPARE(decrypted.senderIdentityKey, aliceDevice.keys.identity.publicKey);
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

    SessionLayer aliceLayer0(*m_context, Version::Omemo0, alice0.keys);
    SessionLayer bobLayer0(*m_context, Version::Omemo0, bob0.keys);
    SessionLayer aliceLayer2(*m_context, Version::Omemo2, alice2.keys);
    SessionLayer bobLayer2(*m_context, Version::Omemo2, bob2.keys);

    establish(aliceLayer0, alice0, bobLayer0, bob0);
    establish(aliceLayer2, alice2, bobLayer2, bob2);

    // Both bundles carry the same identity key in different forms.
    const auto bundle0 = expectVariant<Bundle>(bobLayer0.ownBundle(SIGNED_PRE_KEY_ID, 1));
    const auto bundle2 = expectVariant<Bundle>(bobLayer2.ownBundle(SIGNED_PRE_KEY_ID, 1));
    QCOMPARE(bundle2.identityKey, bobIdentity.publicKey);
    QCOMPARE(bundle0.signedPreKey.mid(1), bundle2.signedPreKey);

    // A key exchange reports the same identity key in both versions.
    Device carol0(*m_context, u"carol@example.org"_s);
    Device carol2 = carol0;
    carol2.keys.preKeys = m_context->generatePreKeys(1, PRE_KEY_COUNT);
    SessionLayer carolLayer0(*m_context, Version::Omemo0, carol0.keys);
    SessionLayer carolLayer2(*m_context, Version::Omemo2, carol2.keys);

    expectVariant<Success>(aliceLayer0.buildSession(carol0.address(), expectVariant<Bundle>(carolLayer0.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    expectVariant<Success>(aliceLayer2.buildSession(carol2.address(), expectVariant<Bundle>(carolLayer2.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    const auto message0 = expectVariant<EncryptedKey>(aliceLayer0.encrypt(carol0.address(), secure("v0")));
    const auto message2 = expectVariant<EncryptedKey>(aliceLayer2.encrypt(carol2.address(), secure("v2")));
    const auto decrypted0 = expectVariant<DecryptedKey>(carolLayer0.decrypt(alice0.address(), message0.data, true));
    const auto decrypted2 = expectVariant<DecryptedKey>(carolLayer2.decrypt(alice2.address(), message2.data, true));
    QCOMPARE(decrypted0.senderIdentityKey, aliceIdentity.publicKey);
    QCOMPARE(decrypted2.senderIdentityKey, aliceIdentity.publicKey);
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
    SessionLayer alice(*m_context, version, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.keys);
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
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.keys);
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
    SessionLayer alice(*m_context, version, aliceDevice.keys);
    SessionLayer bob(*m_context, version, bobDevice.keys);

    auto bundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    bundle.signedPreKeySignature[0] = char(bundle.signedPreKeySignature.at(0) ^ 0x01);
    expectVariant<QXmppError>(alice.buildSession(bobDevice.address(), bundle));
    QVERIFY(!alice.hasSession(bobDevice.address()));

    // signed by another identity key
    auto otherBundle = expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1));
    Device mallory(*m_context, u"mallory@example.org"_s);
    SessionLayer malloryLayer(*m_context, version, mallory.keys);
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
    SessionLayer alice(*m_context, Version::Omemo0, aliceDevice.keys);
    SessionLayer bob0(*m_context, Version::Omemo0, bobDevice.keys);
    SessionLayer bob2(*m_context, Version::Omemo2, bobDevice.keys);

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
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.keys);

    expectVariant<Success>(alice.buildSession(bobDevice.address(), expectVariant<Bundle>(bob.ownBundle(SIGNED_PRE_KEY_ID, 1))));
    const auto message = expectVariant<EncryptedKey>(alice.encrypt(bobDevice.address(), secure("x")));
    bobDevice.keys.preKeys.remove(1);
    expectVariant<QXmppError>(bob.decrypt(aliceDevice.address(), message.data, true));
}

void tst_OmemoSession::noSession()
{
    Device aliceDevice(*m_context, u"alice@example.org"_s);
    Device bobDevice(*m_context, u"bob@example.org"_s);
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.keys);
    SessionLayer bob(*m_context, Version::Omemo2, bobDevice.keys);
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
        SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.keys);
        SessionLayer bob(*m_context, Version::Omemo2, bobDevice.keys);
        establish(alice, aliceDevice, bob, bobDevice);
    }

    // The serialized state is enough to continue, also with a new context.
    SignalContext context;
    auto aliceKeys = aliceDevice.keys;
    auto bobKeys = bobDevice.keys;
    SessionLayer alice(context, Version::Omemo2, aliceKeys);
    SessionLayer bob(context, Version::Omemo2, bobKeys);

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
    SessionLayer alice(*m_context, Version::Omemo2, aliceDevice.keys);
    SessionLayer bob(*m_context,
                     Version::Omemo2,
                     bobDevice.keys,
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

QXMPP_TEST_MAIN(tst_QXmppOmemoManager, tst_QXmppOmemoMemoryStorage, SessionLayerTests::tst_OmemoSession, PayloadCipherTests::tst_OmemoPayloadCipher)

#include "OmemoManager.moc"

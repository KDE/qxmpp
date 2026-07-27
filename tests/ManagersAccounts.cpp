// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the account-related managers. Merging the
// registration, account migration, moved and blocking manager tests into one
// translation unit parses the shared Qt/QXmpp headers once instead of once per
// file. Each original test keeps its own namespace; main() runs them in turn.

#include "QXmppAccountMigrationManager.h"
#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryData.h"
#include "QXmppBitsOfBinaryDataList.h"
#include "QXmppBlockingManager.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMixManager.h"
#include "QXmppMovedItem_p.h"
#include "QXmppMovedManager.h"
#include "QXmppPubSubManager.h"
#include "QXmppRegisterIq.h"
#include "QXmppRegistrationManager.h"
#include "QXmppRosterManager.h"
#include "QXmppSpamReport.h"
#include "QXmppStreamFeatures.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"
#include "QXmppVCardManager.h"

#include "Iq.h"
#include "StringLiterals.h"
#include "TestClient.h"
#include "util.h"

#include <QCoreApplication>
#include <QMimeDatabase>

namespace Registration {

using namespace QXmpp::Private;

class tst_QXmppRegistrationManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void initTestCase();

    Q_SLOT void testDiscoFeatures();

    Q_SLOT void testChangePassword_data();
    Q_SLOT void testChangePassword();
    Q_SLOT void testDeleteAccount();
    Q_SLOT void testRequestRegistrationForm_data();
    Q_SLOT void testRequestRegistrationForm();
    Q_SLOT void testRegisterOnConnectGetSet();
    Q_SLOT void testServiceDiscovery();
    Q_SLOT void testSendCachedRegistrationForm_data();
    Q_SLOT void testSendCachedRegistrationForm();
    Q_SLOT void testStreamFeaturesCheck_data();
    Q_SLOT void testStreamFeaturesCheck();
    Q_SLOT void testRegistrationResult_data();
    Q_SLOT void testRegistrationResult();
    Q_SLOT void testChangePasswordResult_data();
    Q_SLOT void testChangePasswordResult();
    Q_SLOT void testDeleteAccountResult_data();
    Q_SLOT void testDeleteAccountResult();
    Q_SLOT void testRegistrationFormReceived();

    Q_SLOT void sendStreamFeaturesToManager(bool registrationEnabled = true);
    Q_SLOT void setManagerConfig(const QString &username, const QString &server = u"example.org"_s, const QString &password = {});

    QXmppClient client;
    QXmppLogger logger;
    QXmppRegistrationManager *manager;
    QString expectedXml;
};

void tst_QXmppRegistrationManager::initTestCase()
{
    manager = new QXmppRegistrationManager;
    client.addExtension(manager);

    logger.setLoggingType(QXmppLogger::SignalLogging);
    client.setLogger(&logger);
}

void tst_QXmppRegistrationManager::testDiscoFeatures()
{
    QCOMPARE(manager->discoveryFeatures(), QStringList() << "jabber:iq:register");
}

void tst_QXmppRegistrationManager::testChangePassword_data()
{
    QTest::addColumn<QString>("username");
    QTest::addColumn<QString>("password");

#define ROW(name, username, password) \
    QTest::newRow(name) << QStringLiteral(username) << QStringLiteral(password)

    ROW("user-bill", "bill", "m1cr0$0ft");
    ROW("user-alice", "alice", "bitten-apple");

#undef ROW
}

void tst_QXmppRegistrationManager::testChangePassword()
{
    QFETCH(QString, username);
    QFETCH(QString, password);

    setManagerConfig(username, u"example.org"_s, password);

    QObject *context = new QObject(this);
    connect(&logger, &QXmppLogger::message, context, [=](QXmppLogger::MessageType type, const QString &text) {
        QCOMPARE(type, QXmppLogger::SentMessage);

        QXmppRegisterIq iq;
        parsePacket(iq, text.toUtf8());

        QVERIFY(!iq.id().isEmpty());
        QCOMPARE(iq.type(), QXmppIq::Set);
        QCOMPARE(iq.username(), username);
        QCOMPARE(iq.password(), password);

        delete context;  // disconnects lambda
    });

    manager->changePassword(password);
}

void tst_QXmppRegistrationManager::testDeleteAccount()
{
    setManagerConfig(u"bob"_s);

    QObject *context = new QObject(this);
    connect(&logger, &QXmppLogger::message, context, [=](QXmppLogger::MessageType type, const QString &text) {
        QCOMPARE(type, QXmppLogger::SentMessage);

        QXmppRegisterIq iq;
        parsePacket(iq, text.toUtf8());

        QVERIFY(!iq.id().isEmpty());
        // to address must be the server or empty
        QVERIFY(iq.to() == u"example.org" || iq.to().isEmpty());
        QCOMPARE(iq.type(), QXmppIq::Set);
        QVERIFY(iq.isRemove());

        delete context;  // disconnects lambda
    });

    manager->deleteAccount();
}

void tst_QXmppRegistrationManager::testRequestRegistrationForm_data()
{
    QTest::addColumn<bool>("triggerManually");

#define ROW(name, enabled) \
    QTest::newRow(name) << enabled

    ROW("trigger-manually", true);
    ROW("request-form-upon-stream-features", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testRequestRegistrationForm()
{
    QFETCH(bool, triggerManually);

    setManagerConfig(u"bob"_s);

    manager->setRegistrationFormToSend(QXmppRegisterIq());
    manager->setRegisterOnConnectEnabled(true);

    bool signalCalled = false;
    QObject *context = new QObject(this);
    connect(&logger, &QXmppLogger::message, context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            signalCalled = true;

            QVERIFY(text.contains(u"<query xmlns=\"jabber:iq:register\"/>"_s));

            QXmppRegisterIq iq;
            parsePacket(iq, text.toUtf8());

            QVERIFY(!iq.id().isEmpty());
            QCOMPARE(iq.type(), QXmppIq::Get);
        }
    });

    if (triggerManually) {
        manager->requestRegistrationForm();
    } else {
        sendStreamFeaturesToManager(true);
    }

    QVERIFY(signalCalled);
    delete context;
    manager->setRegisterOnConnectEnabled(false);
}

void tst_QXmppRegistrationManager::testRegisterOnConnectGetSet()
{
    manager->setRegisterOnConnectEnabled(true);
    QVERIFY(manager->registerOnConnectEnabled());

    manager->setRegisterOnConnectEnabled(false);
    QVERIFY(!manager->registerOnConnectEnabled());
}

void tst_QXmppRegistrationManager::testServiceDiscovery()
{
    TestClient test(true);
    test.configuration().setJid(u"bob@example.org"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *localManager = test.addNewExtension<QXmppRegistrationManager>();

    bool signalEmitted = false;
    auto context = std::make_unique<QObject>(this);
    connect(localManager, &QXmppRegistrationManager::supportedByServerChanged, context.get(), [&]() {
        signalEmitted = true;
        QCOMPARE(localManager->supportedByServer(), true);
    });

    ResultIq<QXmppDiscoInfo> iq {
        u"qx2"_s,
        u"example.org"_s,
        u"bob@example.org"_s,
        {},
        QXmppDiscoInfo { {}, {}, QStringList { u"jabber:iq:register"_s } },
    };
    Q_EMIT test.connected();
    test.inject(writePacketToDom(iq));

    QVERIFY(signalEmitted);
    QVERIFY(localManager->supportedByServer());
    context.reset();

    // on disconnect, supportedByServer needs to be reset
    Q_EMIT test.disconnected();
    QVERIFY(!localManager->supportedByServer());
}

void tst_QXmppRegistrationManager::testSendCachedRegistrationForm_data()
{
    QTest::addColumn<bool>("triggerSendingManually");

#define ROW(name, enabled) \
    QTest::newRow(name) << enabled

    ROW("manually-trigger-sending", true);
    ROW("sending-upon-correct-stream-features", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testSendCachedRegistrationForm()
{
    QFETCH(bool, triggerSendingManually);

    setManagerConfig(u"bob"_s);

    QXmppRegisterIq iq;
    iq.setUsername(u"someone"_s);
    iq.setPassword(u"s3cr3t"_s);
    iq.setEmail(u"1234@example.org"_s);

    bool signalCalled = false;
    QObject *context = new QObject(this);
    connect(&logger, &QXmppLogger::message, context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            signalCalled = true;

            QXmppRegisterIq parsedIq;
            parsePacket(parsedIq, text.toUtf8());

            QCOMPARE(parsedIq.id(), iq.id());
            QCOMPARE(parsedIq.type(), QXmppIq::Set);
            QCOMPARE(parsedIq.username(), u"someone"_s);
            QCOMPARE(parsedIq.password(), u"s3cr3t"_s);
            QCOMPARE(parsedIq.email(), u"1234@example.org"_s);
        }
    });

    manager->setRegistrationFormToSend(iq);
    if (triggerSendingManually) {
        manager->sendCachedRegistrationForm();
    } else {
        sendStreamFeaturesToManager(true);
    }

    delete context;
}

void tst_QXmppRegistrationManager::testStreamFeaturesCheck_data()
{
    QTest::addColumn<bool>("registrationEnabled");

#define ROW(name, enabled) \
    QTest::newRow(name) << enabled

    ROW("registration-enabled", true);
    ROW("registration-disabled", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testStreamFeaturesCheck()
{
    QFETCH(bool, registrationEnabled);

    bool signalEmitted = false;
    QObject *context = new QObject(this);
    connect(manager, &QXmppRegistrationManager::registrationFailed, context, [&](const QXmppStanza::Error &error) {
        signalEmitted = true;

        QCOMPARE(error.type(), QXmppStanza::Error::Cancel);
        QCOMPARE(error.condition(), QXmppStanza::Error::FeatureNotImplemented);
    });

    manager->setRegisterOnConnectEnabled(true);
    sendStreamFeaturesToManager(registrationEnabled);

    QCOMPARE(signalEmitted, !registrationEnabled);
    delete context;
    manager->setRegisterOnConnectEnabled(false);
}

void tst_QXmppRegistrationManager::testRegistrationResult_data()
{
    QTest::addColumn<bool>("isSuccess");

#define ROW(name, isSuccess) \
    QTest::newRow(name) << isSuccess

    ROW("success", true);
    ROW("error", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testRegistrationResult()
{
    QFETCH(bool, isSuccess);

    QXmppRegisterIq registrationRequestForm;
    registrationRequestForm.setUsername(u"someone"_s);
    registrationRequestForm.setPassword(u"s3cr3t"_s);
    registrationRequestForm.setEmail(u"1234@example.org"_s);
    registrationRequestForm.setId(u"register1"_s);

    bool succeededSignalCalled = false;
    bool failedSignalCalled = false;

    QObject *context = new QObject(this);

    connect(manager, &QXmppRegistrationManager::registrationSucceeded, context, [&]() {
        succeededSignalCalled = true;
    });
    connect(manager, &QXmppRegistrationManager::registrationFailed, context, [&](const QXmppStanza::Error &) {
        failedSignalCalled = true;
    });

    manager->setRegistrationFormToSend(registrationRequestForm);
    manager->sendCachedRegistrationForm();

    QXmppIq serverResult(isSuccess ? QXmppIq::Result : QXmppIq::Error);
    serverResult.setId(registrationRequestForm.id());

    manager->handleStanza(writePacketToDom(serverResult));

    QCOMPARE(succeededSignalCalled, isSuccess);
    QCOMPARE(failedSignalCalled, !isSuccess);

    delete context;
}

void tst_QXmppRegistrationManager::testChangePasswordResult_data()
{
    QTest::addColumn<bool>("isSuccess");

#define ROW(name, isSuccess) \
    QTest::newRow(name) << isSuccess

    ROW("success", true);
    ROW("error", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testChangePasswordResult()
{
    QFETCH(bool, isSuccess);

    QString changePasswordRequestIqId;

    bool requestSentSignalCalled = false;
    QObject *requestSentSignalContext = new QObject(this);
    connect(&logger, &QXmppLogger::message, requestSentSignalContext, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            requestSentSignalCalled = true;

            QXmppIq parsedIq;
            parsePacket(parsedIq, text.toUtf8());
            changePasswordRequestIqId = parsedIq.id();
        }
    });

    manager->changePassword({});
    QVERIFY(requestSentSignalCalled);
    QVERIFY(!changePasswordRequestIqId.isEmpty());
    delete requestSentSignalContext;

    bool resultSignalCalled = false;
    QObject *resultContext = new QObject(this);
    if (isSuccess) {
        connect(manager, &QXmppRegistrationManager::passwordChanged, resultContext, [&](const QString &) {
            resultSignalCalled = true;
        });
    } else {
        connect(manager, &QXmppRegistrationManager::passwordChangeFailed, resultContext, [&](QXmppStanza::Error) {
            resultSignalCalled = true;
        });
    }

    QXmppIq serverResult(isSuccess ? QXmppIq::Result : QXmppIq::Error);
    serverResult.setId(changePasswordRequestIqId);

    manager->handleStanza(writePacketToDom(serverResult));

    QVERIFY(resultSignalCalled);
    delete resultContext;
}

void tst_QXmppRegistrationManager::testDeleteAccountResult_data()
{
    QTest::addColumn<bool>("isSuccess");

#define ROW(name, isSuccess) \
    QTest::newRow(name) << isSuccess

    ROW("success", true);
    ROW("error", false);

#undef ROW
}

void tst_QXmppRegistrationManager::testDeleteAccountResult()
{
    QFETCH(bool, isSuccess);

    QString deleteAccountRequestIqId;

    bool requestSentSignalCalled = false;
    QObject *requestSentSignalContext = new QObject(this);
    connect(&logger, &QXmppLogger::message, requestSentSignalContext, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            requestSentSignalCalled = true;

            QXmppIq parsedIq;
            parsePacket(parsedIq, text.toUtf8());
            deleteAccountRequestIqId = parsedIq.id();
        }
    });

    manager->deleteAccount();
    QVERIFY(requestSentSignalCalled);
    QVERIFY(!deleteAccountRequestIqId.isEmpty());
    delete requestSentSignalContext;

    bool resultSignalCalled = false;
    QObject *resultContext = new QObject(this);
    if (isSuccess) {
        connect(manager, &QXmppRegistrationManager::accountDeleted, resultContext, [&]() {
            resultSignalCalled = true;
        });
    } else {
        connect(manager, &QXmppRegistrationManager::accountDeletionFailed, resultContext, [&](QXmppStanza::Error) {
            resultSignalCalled = true;
        });
    }

    QXmppIq serverResult(isSuccess ? QXmppIq::Result : QXmppIq::Error);
    serverResult.setId(deleteAccountRequestIqId);

    manager->handleStanza(writePacketToDom(serverResult));

    QVERIFY(resultSignalCalled);
    delete resultContext;
}

void tst_QXmppRegistrationManager::testRegistrationFormReceived()
{
    QXmppRegisterIq iq;
    iq.setUsername("");
    iq.setPassword("");

    bool signalCalled = false;
    QObject *context = new QObject(this);
    connect(manager, &QXmppRegistrationManager::registrationFormReceived, context, [&](const QXmppRegisterIq &) {
        signalCalled = true;
        QCOMPARE(iq.username(), u""_s);
        QCOMPARE(iq.password(), u""_s);
    });

    manager->handleStanza(writePacketToDom(iq));

    delete context;
}

void tst_QXmppRegistrationManager::sendStreamFeaturesToManager(bool registrationEnabled)
{
    QXmppStreamFeatures features;
    features.setBindMode(QXmppStreamFeatures::Enabled);
    if (registrationEnabled) {
        features.setRegisterMode(QXmppStreamFeatures::Enabled);
    }

    auto writeFeaturesToDom = [&]() -> QDomElement {
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QXmlStreamWriter writer(&buffer);
        features.toXml(&writer);

        // hacky hack to include stream namespace
        QByteArray manipulatedXml = buffer.data();
        manipulatedXml.replace("stream:", QByteArray());
        manipulatedXml.insert(9, QByteArrayLiteral(" xmlns=\"http://etherx.jabber.org/streams\""));

        return xmlToDom(manipulatedXml);
    };

    manager->handleStanza(writeFeaturesToDom());
}

void tst_QXmppRegistrationManager::setManagerConfig(const QString &username, const QString &server, const QString &password)
{
    client.connectToServer(username + u'@' + server, password);
    client.disconnectFromServer();
}

}  // namespace Registration

// ============================================================

using Manager = QXmppAccountMigrationManager;
using namespace QXmpp;
using namespace QXmpp::Private;

bool operator==(const QXmppRosterIq::Item &left, const QXmppRosterIq::Item &right)
{
    return left.bareJid() == right.bareJid() && left.groups() == right.groups() && left.name() == right.name() && left.subscriptionStatus() == right.subscriptionStatus() && left.subscriptionType() == right.subscriptionType() && left.isApproved() == right.isApproved() && left.isMixChannel() == right.isMixChannel() && left.mixParticipantId() == right.mixParticipantId();
}

bool operator==(const QXmppRosterIq &left, const QXmppRosterIq &right)
{
    return left.versionOpt() == right.versionOpt() && left.items() == right.items() &&
        left.mixAnnotate() == right.mixAnnotate();
}

static QXmppRosterIq::Item newRosterItem(const QString &bareJid, const QString &name, const QSet<QString> &groups = {})
{
    QXmppRosterIq::Item item;
    item.setBareJid(bareJid);
    item.setName(name);
    item.setGroups(groups);
    item.setSubscriptionType(QXmppRosterIq::Item::NotSet);
    return item;
}

static QXmppRosterIq::Item newMixRosterItem(const QString &channelId, const QString &channelName, const QString &participantId)
{
    QXmppRosterIq::Item item;
    item.setBareJid(channelId);
    item.setName(channelName);
    item.setIsMixChannel(true);
    item.setMixParticipantId(participantId);
    item.setSubscriptionType(QXmppRosterIq::Item::NotSet);
    return item;
}

static QXmppRosterIq newRoster(TestClient *client, int version, const std::optional<QString> &id, const std::optional<QXmppIq::Type> &type = {}, int index = -1)
{
    QXmppRosterIq roster;
    roster.setId(id.value_or(QString()));
    roster.setType(type.value_or(QXmppIq::Result));

    if (roster.type() == QXmppIq::Get) {
        roster.setFrom(client->configuration().jid());
        roster.setMixAnnotate(true);
    }

    if (roster.type() == QXmppIq::Result || roster.type() == QXmppIq::Set) {
        switch (version) {
        case 0:
            if (index == -1 || index == 0) {
                roster.addItem(newRosterItem(u"1@bare.com"_s, u"1 Bare"_s, { u"all"_s }));
            }
            if (index == -1 || index == 1) {
                roster.addItem(newMixRosterItem(u"mix1@bare.com"_s, u"Mix 1 Bare"_s, u"mix1BareId"_s));
            }
            break;
        case 1:
            if (index == -1 || index == 0) {
                roster.addItem(newRosterItem(u"3@gamer.com"_s, u"3 Gamer"_s, { u"gamers"_s }));
            }
            if (index == -1 || index == 1) {
                roster.addItem(newMixRosterItem(u"mix2@gamer.com"_s, u"Mix 2 Gamer"_s, u"mix2BareId"_s));
            }
            break;
        default:
            Q_UNREACHABLE();
        }
    }

    return roster;
}

static QXmppVCardIq newClientVCard(TestClient *client, int version, const std::optional<QString> &id, const std::optional<QXmppIq::Type> &type = {})
{
    Q_UNUSED(client)

    QXmppVCardIq vcard;
    vcard.setId(id.has_value() ? *id : QString());
    vcard.setType(type.has_value() ? *type : QXmppIq::Result);

    if (vcard.type() == QXmppIq::Get) {
    }

    if (vcard.type() == QXmppIq::Result || vcard.type() == QXmppIq::Set) {
        switch (version) {
        case 0:
            vcard.setFirstName(u"Nox"_s);
            vcard.setLastName(u"PasNox"_s);
            vcard.setNickName(u"It is me PasNox"_s);
            break;
        case 1:
            vcard.setFirstName(u"Nox"_s);
            vcard.setLastName(u"Bookri"_s);
            vcard.setNickName(u"It is me Bookri"_s);
            break;
        default:
            Q_UNREACHABLE();
        }
    }

    return vcard;
}

static std::unique_ptr<TestClient> newClient(bool withManagers, bool autoResetEnabled = true)
{
    auto client = std::make_unique<TestClient>(false, autoResetEnabled);

    client->addNewExtension<QXmppAccountMigrationManager>();
    client->configuration().setJid("pasnox@xmpp.example");

    if (withManagers) {
        client->addNewExtension<QXmppVCardManager>();
        client->addNewExtension<QXmppDiscoveryManager>();
        client->addNewExtension<QXmppPubSubManager>();
        client->addNewExtension<QXmppRosterManager>(client.get());
        client->addNewExtension<QXmppMixManager>();
    }

    return client;
}

class tst_QXmppAccountMigrationManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testImportExport();
    Q_SLOT void testRealImportExport();
    Q_SLOT void testSerialization();
    Q_SLOT void testSerializationXep0227();
};

struct DataExtension {
    QString data;
};

static auto parseDataExtension(const QDomElement &el)
{
    return Manager::Result<DataExtension>(DataExtension { el.text() });
}

static auto serializeDataExtension(const DataExtension &ext, QXmlStreamWriter &w)
{
    w.writeStartElement("extension");
    w.writeDefaultNamespace("org.qxmpp.tests");
    w.writeCharacters(ext.data);
    w.writeEndElement();
}

void tst_QXmppAccountMigrationManager::testImportExport()
{
    QXmppExportData::registerExtension<DataExtension, parseDataExtension, serializeDataExtension>(u"extension", u"org.qxmpp.tests");

    auto client = newClient(false);
    auto *manager = client->findExtension<QXmppAccountMigrationManager>();
    Q_ASSERT(manager);
    std::optional<DataExtension> currentState;

    manager->registerExportData<DataExtension>(
        [&](const DataExtension &data) -> QXmppTask<Manager::Result<>> {
            currentState = data;
            co_return Success();
        },
        [&]() -> QXmppTask<Manager::Result<DataExtension>> {
            if (currentState) {
                co_return *currentState;
            }
            co_return QXmppError { "No data.", {} };
        });

    auto importTask = manager->importData(QXmppExportData {});
    expectFutureVariant<Success>(importTask);

    // currently no data in 'currentState': expect error
    auto exportTask = manager->exportData();
    expectFutureVariant<QXmppError>(exportTask);

    // set data and expect export to work
    currentState = { "Hello this is a test." };
    exportTask = manager->exportData();
    auto exportData = expectFutureVariant<QXmppExportData>(exportTask);

    // reset state and import data again
    currentState.reset();
    importTask = manager->importData(exportData);
    QVERIFY(currentState);
    QCOMPARE(currentState->data, "Hello this is a test.");

    manager->unregisterExportData<DataExtension>();

    // exporting/importing works without extensions
    // and import data with unknown extensions works
    exportTask = manager->exportData();
    importTask = manager->importData(exportData);
    expectFutureVariant<QXmppExportData>(exportTask);
    expectFutureVariant<Success>(importTask);
}

void tst_QXmppAccountMigrationManager::testRealImportExport()
{
    auto client = newClient(true, false);
    auto *manager = client->findExtension<QXmppAccountMigrationManager>();
    auto *rosterManager = client->findExtension<QXmppRosterManager>();
    auto *vcardManager = client->findExtension<QXmppVCardManager>();

    QVERIFY(manager);
    QVERIFY(rosterManager);
    QVERIFY(vcardManager);

    auto exportTask = manager->exportData();
    QVERIFY(!exportTask.isFinished());

    auto id = client->expectPacketRandomOrder(
        u"<iq from='pasnox@xmpp.example' type='get'>"
        "<query xmlns='jabber:iq:roster' ver=''>"
        "<annotate xmlns='urn:xmpp:mix:roster:0'/>"
        "</query>"
        "</iq>"_s);
    client->inject(packetToXml(newRoster(client.get(), 1, id, QXmppIq::Result)));

    id = client->expectPacketRandomOrder(
        u"<iq from='pasnox@xmpp.example' type='get'>"
        "<query xmlns='jabber:iq:roster' ver=''>"
        "<annotate xmlns='urn:xmpp:mix:roster:0'/>"
        "</query>"
        "</iq>"_s);
    client->inject(packetToXml(newRoster(client.get(), 1, id, QXmppIq::Result)));

    id = client->expectPacketRandomOrder(
        u"<iq to='pasnox@xmpp.example' type='get'>"
        "<vCard xmlns='vcard-temp'>"
        "<TITLE/>"
        "<ROLE/>"
        "</vCard>"
        "</iq>"_s);
    client->inject(packetToXml(newClientVCard(client.get(), 1, id, QXmppIq::Result)));

    id = client->expectPacketRandomOrder(
        u"<iq to='mix2@gamer.com' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'/>"
        "</pubsub>"
        "</iq>"_s);
    client->inject(
        u"<iq id='%1' from='mix2@gamer.com' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'>"
        "<item id='mix2BareId'>"
        "<participant xmlns='urn:xmpp:mix:core:1'>"
        "<nick>Joe @ Mix 2 Gamer</nick>"
        "<jid>mix_user@domain.ext</jid>"
        "</participant>"
        "</item>"
        "</items>"
        "</pubsub>"
        "</iq>"_s.arg(id));

    client->expectNoPacket();

    auto data = expectFutureVariant<QXmppExportData>(exportTask);

    // import exported data
    auto importTask = manager->importData(data);

    id = client->expectPacketRandomOrder(
        u"<iq to='pasnox@xmpp.example' type='set'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2' channel='mix2@gamer.com'>"
        "<join xmlns='urn:xmpp:mix:core:1'>"
        "<subscribe node='urn:xmpp:mix:nodes:allowed'/>"
        "<subscribe node='urn:xmpp:avatar:data'/>"
        "<subscribe node='urn:xmpp:avatar:metadata'/>"
        "<subscribe node='urn:xmpp:mix:nodes:banned'/>"
        "<subscribe node='urn:xmpp:mix:nodes:config'/>"
        "<subscribe node='urn:xmpp:mix:nodes:info'/>"
        "<subscribe node='urn:xmpp:mix:nodes:jidmap'/>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:participants'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>Joe @ Mix 2 Gamer</nick>"
        "</join>"
        "</client-join>"
        "</iq>"_s);
    client->inject(
        u"<iq id='%1' type='result'>"
        "<client-join xmlns='urn:xmpp:mix:pam:2'>"
        "<join xmlns='urn:xmpp:mix:core:1' id='mix2BareId'>"
        "<subscribe node='urn:xmpp:mix:nodes:messages'/>"
        "<subscribe node='urn:xmpp:mix:nodes:presence'/>"
        "<nick>Joe @ Mix 2 Gamer</nick>"
        "</join>"
        "</client-join>"
        "</iq>"_s.arg(id));

    id = client->expectPacketRandomOrder(
        u"<iq to='pasnox@xmpp.example' type='set'>"
        "<vCard xmlns='vcard-temp'>"
        "<NICKNAME>It is me Bookri</NICKNAME>"
        "<N><GIVEN>Nox</GIVEN><FAMILY>Bookri</FAMILY></N>"
        "<TITLE/>"
        "<ROLE/>"
        "</vCard>"
        "</iq>"_s);
    client->inject(packetToXml(newClientVCard(client.get(), 1, id, QXmppIq::Result)));

    id = client->expectPacketRandomOrder(
        u"<iq type='set'>"
        "<query xmlns='jabber:iq:roster'>"
        "<item jid='3@gamer.com' name='3 Gamer'>"
        "<group>gamers</group>"
        "</item>"
        "</query>"
        "</iq>"_s);
    client->inject(packetToXml(newRoster(client.get(), 1, id, QXmppIq::Result, 0)));

    client->expectNoPacket();

    expectFutureVariant<Success>(importTask);
}

void tst_QXmppAccountMigrationManager::testSerialization()
{
    auto client = newClient(true, false);
    auto *manager = client->findExtension<QXmppAccountMigrationManager>();
    auto *rosterManager = client->findExtension<QXmppRosterManager>();
    auto *vcardManager = client->findExtension<QXmppVCardManager>();

    QVERIFY(manager);
    QVERIFY(rosterManager);
    QVERIFY(vcardManager);

    // generate export data
    auto exportTask = manager->exportData();
    QVERIFY(!exportTask.isFinished());

    client->expect(u"<iq id='qx2' from='pasnox@xmpp.example' type='get'>"
                   "<query xmlns='jabber:iq:roster' ver=''>"
                   "<annotate xmlns='urn:xmpp:mix:roster:0'/>"
                   "</query>"
                   "</iq>"_s);
    client->inject(packetToXml(newRoster(client.get(), 1, "qx2", QXmppIq::Result)));

    auto rosterPacketId = client->expectPacketRandomOrder(
        u"<iq from='pasnox@xmpp.example' type='get'>"
        "<query xmlns='jabber:iq:roster' ver=''>"
        "<annotate xmlns='urn:xmpp:mix:roster:0'/>"
        "</query>"
        "</iq>"_s);
    client->inject(packetToXml(newRoster(client.get(), 1, rosterPacketId, QXmppIq::Result)));

    auto vcardPacketId = client->expectPacketRandomOrder(
        u"<iq to='pasnox@xmpp.example' type='get'>"
        "<vCard xmlns='vcard-temp'>"
        "<TITLE/>"
        "<ROLE/>"
        "</vCard>"
        "</iq>"_s);
    client->inject(packetToXml(newClientVCard(client.get(), 1, vcardPacketId, QXmppIq::Result)));

    auto packetId = client->expectPacketRandomOrder(
        u"<iq to='mix2@gamer.com' type='get'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'/>"
        "</pubsub>"
        "</iq>"_s);
    client->inject(
        u"<iq id='%1' from='mix2@gamer.com' type='result'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<items node='urn:xmpp:mix:nodes:participants'>"
        "<item id='mix2BareId'>"
        "<participant xmlns='urn:xmpp:mix:core:1'>"
        "<nick>Joe @ Mix 2 Gamer</nick>"
        "<jid>mix_user@domain.ext</jid>"
        "</participant>"
        "</item>"
        "</items>"
        "</pubsub>"
        "</iq>"_s
            .arg(packetId));

    client->expectNoPacket();

    // test serialize
    const auto data = expectFutureVariant<QXmppExportData>(exportTask);

    const auto xml1 = packetToXml(data, QXmppExportData::Format::QXmpp);
    const QByteArray xml2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<account-data xmlns=\"org.qxmpp.export\" jid=\"pasnox@xmpp.example\">"
        "<mix>"
        "<item jid=\"mix2@gamer.com\" nick=\"Joe @ Mix 2 Gamer\"/>"
        "</mix>"
        "<roster>"
        "<item xmlns=\"jabber:iq:roster\" jid=\"3@gamer.com\" name=\"3 Gamer\"><group>gamers</group></item>"
        "</roster>"
        "<vcard>"
        "<vCard xmlns=\"vcard-temp\">"
        "<NICKNAME>It is me Bookri</NICKNAME>"
        "<N><GIVEN>Nox</GIVEN><FAMILY>Bookri</FAMILY></N>"
        "<TITLE/><ROLE/>"
        "</vCard>"
        "</vcard>"
        "</account-data>\n";

    if (xml1 != xml2) {
        qDebug() << "Actual:\n"
                 << xml1.constData();
        qDebug() << "Expected:\n"
                 << xml2.constData();
    }
    QCOMPARE(xml1, xml2);

    // test parse (and re-serialize)
    auto parsedData = expectVariant<QXmppExportData>(QXmppExportData::fromDom(xmlToDom(xml2)));
    const auto xml3 = packetToXml(parsedData, QXmppExportData::Format::QXmpp);
    const QByteArray xml4Mix =
        "<mix>"
        "<item jid=\"mix2@gamer.com\" nick=\"Joe @ Mix 2 Gamer\"/>"
        "</mix>";
    const QByteArray xml4VCard =
        "<vcard>"
        "<vCard xmlns=\"vcard-temp\">"
        "<NICKNAME>It is me Bookri</NICKNAME>"
        "<N><GIVEN>Nox</GIVEN><FAMILY>Bookri</FAMILY></N>"
        "<TITLE/><ROLE/>"
        "</vCard>"
        "</vcard>";
    const QByteArray xml4Roster =
        "<roster>"
        "<item xmlns=\"jabber:iq:roster\" jid=\"3@gamer.com\" name=\"3 Gamer\"><group>gamers</group></item>"
        "</roster>";

    QVERIFY(xml3.startsWith(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<account-data xmlns=\"org.qxmpp.export\" jid=\"pasnox@xmpp.example\">"));
    QVERIFY(xml3.endsWith("</account-data>\n"));
    QVERIFY(xml3.contains(xml4Mix));
    QVERIFY(xml3.contains(xml4Roster));
    QVERIFY(xml3.contains(xml4VCard));
}

void tst_QXmppAccountMigrationManager::testSerializationXep0227()
{
    // Ensure the roster/vCard/mix extensions are registered.
    auto client = newClient(true, false);

    // Build export data from the QXmpp-format document. It covers a roster and a vCard
    // (both have a native XEP-0227 representation) plus QXmpp-only MIX data.
    const QByteArray qxmppXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<account-data xmlns=\"org.qxmpp.export\" jid=\"pasnox@xmpp.example\">"
        "<mix>"
        "<item jid=\"mix2@gamer.com\" nick=\"Joe @ Mix 2 Gamer\"/>"
        "</mix>"
        "<roster>"
        "<item xmlns=\"jabber:iq:roster\" jid=\"3@gamer.com\" name=\"3 Gamer\"><group>gamers</group></item>"
        "</roster>"
        "<vcard>"
        "<vCard xmlns=\"vcard-temp\">"
        "<NICKNAME>It is me Bookri</NICKNAME>"
        "<N><GIVEN>Nox</GIVEN><FAMILY>Bookri</FAMILY></N>"
        "<TITLE/><ROLE/>"
        "</vCard>"
        "</vcard>"
        "</account-data>\n";

    const auto data = expectVariant<QXmppExportData>(QXmppExportData::fromDom(xmlToDom(qxmppXml)));

    // Serialize as XEP-0227. Roster and vCard use their native elements; MIX has no native
    // XEP-0227 form, so <mix/> is written inside a foreign <account-data/> element under
    // <user/> in the org.qxmpp.export namespace.
    const auto xml1 = packetToXml(data, QXmppExportData::Format::Xep0227);
    const QByteArray xml2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<server-data xmlns=\"urn:xmpp:pie:0\">"
        "<host jid=\"xmpp.example\">"
        "<user name=\"pasnox\">"
        "<query xmlns=\"jabber:iq:roster\">"
        "<item jid=\"3@gamer.com\" name=\"3 Gamer\"><group>gamers</group></item>"
        "</query>"
        "<vCard xmlns=\"vcard-temp\">"
        "<NICKNAME>It is me Bookri</NICKNAME>"
        "<N><GIVEN>Nox</GIVEN><FAMILY>Bookri</FAMILY></N>"
        "<TITLE/><ROLE/>"
        "</vCard>"
        "<account-data xmlns=\"org.qxmpp.export\">"
        "<mix>"
        "<item jid=\"mix2@gamer.com\" nick=\"Joe @ Mix 2 Gamer\"/>"
        "</mix>"
        "</account-data>"
        "</user>"
        "</host>"
        "</server-data>\n";

    if (xml1 != xml2) {
        qDebug() << "Actual:\n"
                 << xml1.constData();
        qDebug() << "Expected:\n"
                 << xml2.constData();
    }
    QCOMPARE(xml1, xml2);

    // Round-trip: parse the XEP-0227 document, reconstruct the JID and re-serialize as the
    // QXmpp format to get back the original document.
    const auto parsed = expectVariant<QXmppExportData>(QXmppExportData::fromDom(xmlToDom(xml2)));
    QCOMPARE(parsed.accountJid(), u"pasnox@xmpp.example"_s);
    QCOMPARE(packetToXml(parsed, QXmppExportData::Format::QXmpp), qxmppXml);

    // "Prefer XEP-0227": when a document carries both a native <query/> and a QXmpp
    // <roster/> fallback for the same data, the native one wins.
    const QByteArray bothXml =
        "<server-data xmlns=\"urn:xmpp:pie:0\">"
        "<host jid=\"xmpp.example\">"
        "<user name=\"pasnox\">"
        "<query xmlns=\"jabber:iq:roster\">"
        "<item jid=\"native@example\"/>"
        "</query>"
        "<account-data xmlns=\"org.qxmpp.export\">"
        "<roster>"
        "<item xmlns=\"jabber:iq:roster\" jid=\"fallback@example\"/>"
        "</roster>"
        "</account-data>"
        "</user>"
        "</host>"
        "</server-data>";

    const auto preferred = expectVariant<QXmppExportData>(QXmppExportData::fromDom(xmlToDom(bothXml)));
    const auto preferredXml = packetToXml(preferred, QXmppExportData::Format::QXmpp);
    QVERIFY(preferredXml.contains("native@example"));
    QVERIFY(!preferredXml.contains("fallback@example"));
}

// ============================================================

using namespace QXmpp::Private;

struct Tester {
    Tester()
    {
        client.addNewExtension<QXmppDiscoveryManager>();
        client.addNewExtension<QXmppPubSubManager>();
        manager = client.addNewExtension<QXmppMovedManager>();
    }

    Tester(const QString &jid)
        : Tester()
    {
        client.configuration().setJid(jid);
    }

    TestClient client;
    QXmppMovedManager *manager;
};

class tst_QXmppMovedManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testMovedItem();
    Q_SLOT void testMovedPresence();
    Q_SLOT void testDiscoveryFeatures();
    Q_SLOT void testSupportedByServer();
    Q_SLOT void testResetCachedData();
    Q_SLOT void testHandleDiscoInfo();
    Q_SLOT void testOnRegistered();
    Q_SLOT void testOnUnregistered();
    Q_SLOT void testPublishMoved();
    Q_SLOT void testVerifyMoved();
    Q_SLOT void testNotify();

    template<typename T>
    void testError(QXmppTask<T> &task, TestClient &client, const QString &id, const QString &from);
};

void tst_QXmppMovedManager::testMovedItem()
{
    const auto expected = u"<item id=\"current\"><moved xmlns=\"urn:xmpp:moved:1\"><new-jid>new@shakespeare.example</new-jid></moved></item>"_s;
    const QDomElement expectedElement = xmlToDom(expected);

    {
        QXmppMovedItem packet;
        packet.setNewJid(u"new@shakespeare.example"_s);

        QCOMPARE(packetToXml(packet), expected);
    }

    {
        QXmppMovedItem packet;
        packet.parse(expectedElement);

        QVERIFY(!packet.newJid().isEmpty());
    }
}

void tst_QXmppMovedManager::testMovedPresence()
{
    const auto expected =
        u"<presence to=\"contact@shakespeare.example\" type=\"subscribe\">"
        "<moved xmlns=\"urn:xmpp:moved:1\"><old-jid>old@shakespeare.example</old-jid></moved>"
        "</presence>"_s;
    const QDomElement expectedElement = xmlToDom(expected);

    {
        QXmppPresence packet;
        packet.setTo(u"contact@shakespeare.example"_s);
        packet.setType(QXmppPresence::Subscribe);
        packet.setOldJid(u"old@shakespeare.example"_s);

        QCOMPARE(packetToXml(packet), expected);
    }

    {
        QXmppPresence packet;
        packet.parse(expectedElement);

        QVERIFY(!packet.oldJid().isEmpty());
    }
}

void tst_QXmppMovedManager::testDiscoveryFeatures()
{
    QXmppMovedManager manager;

    QCOMPARE(manager.discoveryFeatures(), QStringList { ns_moved.toString() });
}

void tst_QXmppMovedManager::testSupportedByServer()
{
    QXmppMovedManager manager;
    QSignalSpy spy(&manager, &QXmppMovedManager::supportedByServerChanged);

    QVERIFY(!manager.supportedByServer());

    manager.setSupportedByServer(true);

    QVERIFY(manager.supportedByServer());
    QCOMPARE(spy.size(), 1);
}

void tst_QXmppMovedManager::testResetCachedData()
{
    QXmppMovedManager manager;

    manager.setSupportedByServer(true);
    manager.resetCachedData();

    QVERIFY(!manager.supportedByServer());
}

void tst_QXmppMovedManager::testHandleDiscoInfo()
{
    auto [client, manager] = Tester(u"hag66@shakespeare.example"_s);
    client.setStreamManagementState(QXmppClient::NewStream);

    ResultIq<QXmppDiscoInfo> iq {
        u"qx1"_s,
        u"shakespeare.example"_s,
        u"hag66@shakespeare.example"_s,
        {},
        QXmppDiscoInfo { {}, {}, QStringList { u"urn:xmpp:moved:1"_s } },
    };
    Q_EMIT client.connected();
    client.inject(writePacketToDom(iq));

    QVERIFY(manager->supportedByServer());

    Q_EMIT client.connected();
    iq.payload.setFeatures({});
    client.inject(writePacketToDom(iq));

    QVERIFY(!manager->supportedByServer());
}

void tst_QXmppMovedManager::testOnRegistered()
{
    TestClient client;
    QXmppMovedManager manager;

    client.addNewExtension<QXmppDiscoveryManager>();
    client.addNewExtension<QXmppPubSubManager>();
    client.configuration().setJid(u"hag66@shakespeare.example"_s);
    client.addExtension(&manager);

    manager.setSupportedByServer(true);

    client.setStreamManagementState(QXmppClient::NewStream);
    Q_EMIT client.connected();

    QVERIFY(!manager.supportedByServer());

    ResultIq<QXmppDiscoInfo> iq {
        u"qx1"_s,
        u"shakespeare.example"_s,
        u"hag66@shakespeare.example"_s,
        {},
        QXmppDiscoInfo { {}, {}, QStringList { u"urn:xmpp:moved:1"_s } },
    };
    Q_EMIT client.connected();
    client.inject(writePacketToDom(iq));

    QVERIFY(manager.supportedByServer());
}

void tst_QXmppMovedManager::testOnUnregistered()
{
    QXmppClient client;
    QXmppMovedManager manager;

    client.addNewExtension<QXmppDiscoveryManager>();
    client.addNewExtension<QXmppPubSubManager>();
    client.configuration().setJid(u"hag66@shakespeare.example"_s);
    client.addExtension(&manager);

    manager.setSupportedByServer(true);
    manager.onUnregistered(&client);

    QVERIFY(!manager.supportedByServer());

    manager.setSupportedByServer(true);
    Q_EMIT client.connected();

    QVERIFY(manager.supportedByServer());
}

void tst_QXmppMovedManager::testPublishMoved()
{
    auto tester = Tester(u"old@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->publishStatement(u"moved@shakespeare.example"_s);
    };

    auto task = call();

    client.expect(u"<iq id='qx1' to='old@shakespeare.example' type='set'>"
                  "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                  "<publish node='urn:xmpp:moved:1'>"
                  "<item id='current'>"
                  "<moved xmlns='urn:xmpp:moved:1'>"
                  "<new-jid>moved@shakespeare.example</new-jid>"
                  "</moved>"
                  "</item>"
                  "</publish>"
                  "</pubsub>"
                  "</iq>"_s);
    client.inject(u"<iq id='qx1' from='old@shakespeare.example' type='result'>"
                  "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                  "<publish node='uurn:xmpp:moved:1'>"
                  "<item id='current'/>"
                  "</publish>"
                  "</pubsub>"
                  "</iq>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testError(task = call(), client, u"qx1"_s, u"old@shakespeare.example"_s);
}

void tst_QXmppMovedManager::testVerifyMoved()
{
    auto tester = Tester(u"contact@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->verifyStatement(u"old@shakespeare.example"_s, u"moved@shakespeare.example"_s);
    };

    auto task = call();

    client.expect(u"<iq id='qx1' to='old@shakespeare.example' type='get'>"
                  "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                  "<items node='urn:xmpp:moved:1'>"
                  "<item id='current'/>"
                  "</items>"
                  "</pubsub>"
                  "</iq>"_s);
    client.inject(u"<iq id='qx1' from='old@shakespeare.example' type='result'>"
                  "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                  "<items node='urn:xmpp:moved:1'>"
                  "<item id='current'>"
                  "<moved xmlns='urn:xmpp:moved:1'>"
                  "<new-jid>moved@shakespeare.example</new-jid>"
                  "</moved>"
                  "</item>"
                  "</items>"
                  "</pubsub>"
                  "</iq>"_s);

    expectFutureVariant<QXmpp::Success>(task);

    testError(task = call(), client, u"qx1"_s, u"old@shakespeare.example"_s);
}

void tst_QXmppMovedManager::testNotify()
{
    auto tester = Tester(u"moved@shakespeare.example"_s);
    auto &client = tester.client;
    auto manager = tester.manager;

    auto call = [manager]() {
        return manager->notifyContact(u"contact@shakespeare.example"_s, u"old@shakespeare.example"_s, true, u"I moved."_s);
    };

    auto task = call();

    client.expect(u"<presence to='contact@shakespeare.example' type='subscribe'>"
                  "<status>I moved.</status>"
                  "<moved xmlns='urn:xmpp:moved:1'>"
                  "<old-jid>old@shakespeare.example</old-jid>"
                  "</moved>"
                  "</presence>"_s);
}

template<typename T>
void tst_QXmppMovedManager::testError(QXmppTask<T> &task, TestClient &client, const QString &id, const QString &from)
{
    client.ignore();
    client.inject(u"<iq id='%1' from='%2' type='error'>"
                  u"<error type='cancel'>"
                  u"<not-allowed xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                  u"</error>"
                  u"</iq>"_s
                      .arg(id, from));

    expectFutureVariant<QXmppError>(task);
}

// ============================================================

namespace Blocking {

using namespace QXmpp;

class tst_QXmppBlockingManager : public QObject
{
    Q_OBJECT
private:
    Q_SLOT void basic();
    Q_SLOT void fetch();
    Q_SLOT void block();
    Q_SLOT void unblock();
    Q_SLOT void reportAndBlock();
    Q_SLOT void reportAndBlockFull();
    Q_SLOT void pushBlocked();
    Q_SLOT void blockedState();
};

void tst_QXmppBlockingManager::basic()
{
    QXmppBlockingManager m;
    QVERIFY(!m.isSubscribed());
}

void tst_QXmppBlockingManager::fetch()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    QVERIFY(!m->isSubscribed());

    // multiple calls should only trigger one IQ request
    auto task = m->fetchBlocklist();
    auto task2 = m->fetchBlocklist();
    auto task3 = m->fetchBlocklist();

    // expect only one IQ
    t.expect("<iq id='qx1' type='get'><blocklist xmlns='urn:xmpp:blocking'/></iq>");
    t.inject("<iq type='result' id='qx1'><blocklist xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/><item jid='iago@shakespeare.lit'/></blocklist></iq>");

    // we should be subscribed to the blocklist now
    QVERIFY(m->isSubscribed());

    // check all three results
    QList<QString> expected { "romeo@montague.net", "iago@shakespeare.lit" };
    auto blocklist = expectFutureVariant<QXmppBlocklist>(task);
    QCOMPARE(blocklist.entries(), expected);
    blocklist = expectFutureVariant<QXmppBlocklist>(task2);
    QCOMPARE(blocklist.entries(), expected);
    blocklist = expectFutureVariant<QXmppBlocklist>(task3);
    QCOMPARE(blocklist.entries(), expected);

    // now the blocklist is cached
    task = m->fetchBlocklist();
    blocklist = expectFutureVariant<QXmppBlocklist>(task);
    QCOMPARE(blocklist.entries(), expected);

    QVERIFY(m->isSubscribed());
}

void tst_QXmppBlockingManager::block()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    auto task = m->block("romeo@montague.net");
    t.expect("<iq id='qx1' type='set'><block xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/></block></iq>");
    t.inject("<iq type='result' id='qx1'/>");
    expectFutureVariant<Success>(task);
}

void tst_QXmppBlockingManager::unblock()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    auto task = m->unblock("romeo@montague.net");
    t.expect("<iq id='qx1' type='set'><unblock xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/></unblock></iq>");
    t.inject("<iq type='result' id='qx1'/>");
    expectFutureVariant<Success>(task);
}

void tst_QXmppBlockingManager::reportAndBlock()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    auto task = m->reportAndBlock("romeo@montague.net", QXmppSpamReport(QXmppSpamReport::Reason::Spam));
    t.expect("<iq id='qx1' type='set'><block xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'><report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:spam'/></item></block></iq>");
    t.inject("<iq type='result' id='qx1'/>");
    expectFutureVariant<Success>(task);
}

void tst_QXmppBlockingManager::reportAndBlockFull()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    QXmppSpamReport report(QXmppSpamReport::Reason::Abuse);
    report.setText("please stop");
    report.setTextLanguage("en");
    report.setMessageReferences({ QXmppStanzaId { "28482-98726", "romeo@example.net" } });
    report.setForwardToOrigin(true);
    report.setForwardToThirdParty(true);

    auto task = m->reportAndBlock("romeo@montague.net", report);
    t.expect("<iq id='qx1' type='set'><block xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'>"
             "<report xmlns='urn:xmpp:reporting:1' reason='urn:xmpp:reporting:abuse'>"
             "<stanza-id xmlns='urn:xmpp:sid:0' id='28482-98726' by='romeo@example.net'/>"
             "<text xml:lang='en'>please stop</text>"
             "<report-origin/><third-party/></report></item></block></iq>");
    t.inject("<iq type='result' id='qx1'/>");
    expectFutureVariant<Success>(task);
}

void tst_QXmppBlockingManager::pushBlocked()
{
    TestClient t;
    t.configuration().setJid("juliet@capulet.com/balcony");
    auto *m = t.addNewExtension<QXmppBlockingManager>();

    m->fetchBlocklist();
    t.expect("<iq id='qx1' type='get'><blocklist xmlns='urn:xmpp:blocking'/></iq>");
    t.inject("<iq type='result' id='qx1'><blocklist xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/><item jid='iago@shakespeare.lit'/></blocklist></iq>");

    QVERIFY(m->isSubscribed());

    QSignalSpy blockedSpy(m, &QXmppBlockingManager::blocked);
    QSignalSpy unblockedSpy(m, &QXmppBlockingManager::unblocked);

    auto dom = xmlToDom("<iq to='juliet@capulet.com/balcony' type='set' id='push4'><unblock xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/></unblock></iq>");
    QVERIFY(m->handleStanza(dom, {}));

    QCOMPARE(blockedSpy.size(), 0);
    QCOMPARE(unblockedSpy.size(), 1);
    QCOMPARE(unblockedSpy[0][0].value<QList<QString>>(), QList<QString> { "romeo@montague.net" });

    auto blocklist = std::get<QXmppBlocklist>(m->fetchBlocklist().result()).entries();
    QCOMPARE(blocklist, QList<QString> { "iago@shakespeare.lit" });

    dom = xmlToDom("<iq to='juliet@capulet.com/balcony' type='set' id='push3'><block xmlns='urn:xmpp:blocking'><item jid='romeo@montague.net'/></block></iq>");
    QVERIFY(m->handleStanza(dom, {}));

    QCOMPARE(blockedSpy.size(), 1);
    QCOMPARE(blockedSpy[0][0].value<QList<QString>>(), QList<QString> { "romeo@montague.net" });
    QCOMPARE(unblockedSpy.size(), 1);

    blocklist = std::get<QXmppBlocklist>(m->fetchBlocklist().result()).entries();
    auto expected = QList<QString> { "iago@shakespeare.lit", "romeo@montague.net" };
    QCOMPARE(blocklist, expected);
}

void tst_QXmppBlockingManager::blockedState()
{
    using L = QXmppBlocklist;
    auto entries = QList<QString> {
        "iago@shakespeare.lit", "romeo@montague.net"
    };
    QXmppBlocklist l(entries);

    QVERIFY(l.containsEntry(u"iago@shakespeare.lit"));
    QVERIFY(!l.containsEntry(u"shakespeare.lit"));
    QCOMPARE(l.entries(), entries);

    auto state = l.blockingState("iago@shakespeare.lit");
    auto blocked = expectVariant<L::Blocked>(state);
    QCOMPARE(blocked.blockingEntries, QList<QString> { "iago@shakespeare.lit" });
    QVERIFY(blocked.partiallyBlockingEntries.isEmpty());

    state = l.blockingState("iago@shakespeare.lit/res");
    blocked = expectVariant<L::Blocked>(state);
    QCOMPARE(blocked.blockingEntries, QList<QString> { "iago@shakespeare.lit" });
    QVERIFY(blocked.partiallyBlockingEntries.isEmpty());

    state = l.blockingState("shakespeare.lit");
    auto partially = expectVariant<L::PartiallyBlocked>(state);
    QCOMPARE(partially.partiallyBlockingEntries, QList<QString> { "iago@shakespeare.lit" });

    state = l.blockingState("qxmpp.org");
    expectVariant<L::NotBlocked>(state);
}

}  // namespace Blocking

// ============================================================

namespace RegisterIq {

class tst_QXmppRegisterIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void registerGet();
    Q_SLOT void registerResult();
    Q_SLOT void registerResultWithForm();
    Q_SLOT void registerResultWithRedirection();
    Q_SLOT void registerResultWithFormAndRedirection();
    Q_SLOT void registerSet();
    Q_SLOT void registerSetWithForm();
    Q_SLOT void registerBobData();
    Q_SLOT void registerRegistered();
    Q_SLOT void registerRemove();
    Q_SLOT void registerChangePassword();
    Q_SLOT void registerUnregistration();
};

void tst_QXmppRegisterIq::registerGet()
{
    const QByteArray xml(
        "<iq id=\"reg1\" to=\"shakespeare.lit\" type=\"get\">"
        "<query xmlns=\"jabber:iq:register\"/>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg1"));
    QCOMPARE(iq.to(), QLatin1String("shakespeare.lit"));
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.instructions(), QString());
    QVERIFY(!iq.isRegistered());
    QVERIFY(!iq.isRemove());
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerResult()
{
    const QByteArray xml(
        "<iq id=\"reg1\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Choose a username and password for use with this service. Please also provide your email address.</instructions>"
        "<username/>"
        "<password/>"
        "<email/>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg1"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Choose a username and password for use with this service. Please also provide your email address."));
    QVERIFY(!iq.username().isNull());
    QVERIFY(iq.username().isEmpty());
    QVERIFY(!iq.password().isNull());
    QVERIFY(iq.password().isEmpty());
    QVERIFY(!iq.email().isNull());
    QVERIFY(iq.email().isEmpty());
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerResultWithForm()
{
    const QByteArray xml(
        "<iq id=\"reg3\" to=\"juliet@capulet.com/balcony\" from=\"contests.shakespeare.lit\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php</instructions>"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<title>Contest Registration</title>"
        "<instructions>"
        "Please provide the following information"
        "to sign up for our special contests!"
        "</instructions>"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<required/>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<option label=\"Male\"><value>M</value></option>"
        "<option label=\"Female\"><value>F</value></option>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.from(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QCOMPARE(iq.form().title(), QLatin1String("Contest Registration"));
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerResultWithRedirection()
{
    const QByteArray xml(
        "<iq id=\"reg3\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>"
        "To register, visit http://www.shakespeare.lit/contests.php"
        "</instructions>"
        "<x xmlns=\"jabber:x:oob\">"
        "<url>http://www.shakespeare.lit/contests.php</url>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("To register, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(iq.form().isNull());
    QCOMPARE(iq.outOfBandUrl(), QLatin1String("http://www.shakespeare.lit/contests.php"));
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerResultWithFormAndRedirection()
{
    const QByteArray xml(
        "<iq id=\"reg3\" to=\"juliet@capulet.com/balcony\" from=\"contests.shakespeare.lit\" type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<instructions>Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php</instructions>"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<title>Contest Registration</title>"
        "<instructions>"
        "Please provide the following information"
        "to sign up for our special contests!"
        "</instructions>"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<required/>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<required/>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<option label=\"Male\"><value>M</value></option>"
        "<option label=\"Female\"><value>F</value></option>"
        "</field>"
        "</x>"
        "<x xmlns=\"jabber:x:oob\">"
        "<url>http://www.shakespeare.lit/contests.php</url>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg3"));
    QCOMPARE(iq.to(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.from(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.instructions(), QLatin1String("Use the enclosed form to register. If your Jabber client does not support Data Forms, visit http://www.shakespeare.lit/contests.php"));
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QCOMPARE(iq.form().title(), QLatin1String("Contest Registration"));
    QCOMPARE(iq.outOfBandUrl(), QLatin1String("http://www.shakespeare.lit/contests.php"));
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerSet()
{
    const QByteArray xml(
        "<iq id=\"reg2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<username>bill</username>"
        "<password>Calliope</password>"
        "<email>bard@shakespeare.lit</email>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg2"));
    QCOMPARE(iq.to(), QString());
    QCOMPARE(iq.from(), QString());
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.password(), QLatin1String("Calliope"));
    QCOMPARE(iq.email(), QLatin1String("bard@shakespeare.lit"));
    QVERIFY(iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerSetWithForm()
{
    const QByteArray xml(
        "<iq id=\"reg4\" to=\"contests.shakespeare.lit\" from=\"juliet@capulet.com/balcony\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<x xmlns=\"jabber:x:data\" type=\"submit\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>jabber:iq:register</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Given Name\" var=\"first\">"
        "<value>Juliet</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Family Name\" var=\"last\">"
        "<value>Capulet</value>"
        "</field>"
        "<field type=\"text-single\" label=\"Email Address\" var=\"email\">"
        "<value>juliet@capulet.com</value>"
        "</field>"
        "<field type=\"list-single\" label=\"Gender\" var=\"x-gender\">"
        "<value>F</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("reg4"));
    QCOMPARE(iq.to(), QLatin1String("contests.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("juliet@capulet.com/balcony"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QVERIFY(iq.username().isNull());
    QVERIFY(iq.password().isNull());
    QVERIFY(iq.email().isNull());
    QVERIFY(!iq.form().isNull());
    QVERIFY(iq.outOfBandUrl().isNull());
    serializePacket(iq, xml);

    QXmppRegisterIq sIq;
    sIq.setId(QLatin1String("reg4"));
    sIq.setTo(QLatin1String("contests.shakespeare.lit"));
    sIq.setFrom(QLatin1String("juliet@capulet.com/balcony"));
    sIq.setType(QXmppIq::Set);
    sIq.setForm(QXmppDataForm(
        QXmppDataForm::Submit,
        QList<QXmppDataForm::Field>()
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::HiddenField,
                   u"FORM_TYPE"_s,
                   u"jabber:iq:register"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"first"_s,
                   u"Juliet"_s,
                   false,
                   u"Given Name"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"last"_s,
                   u"Capulet"_s,
                   false,
                   u"Family Name"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::TextSingleField,
                   u"email"_s,
                   u"juliet@capulet.com"_s,
                   false,
                   u"Email Address"_s)
            << QXmppDataForm::Field(
                   QXmppDataForm::Field::ListSingleField,
                   u"x-gender"_s,
                   u"F"_s,
                   false,
                   u"Gender"_s)));
    serializePacket(sIq, xml);
}

void tst_QXmppRegisterIq::registerBobData()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<data xmlns=\"urn:xmpp:bob\" "
        "cid=\"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org\" "
        "type=\"image/png\">"
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC"
        "</data>"
        "</query>"
        "</iq>");

    QXmppBitsOfBinaryData data;
    data.setCid(QXmppBitsOfBinaryContentId::fromContentId(
        u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s));
    data.setContentType(QMimeDatabase().mimeTypeForName(u"image/png"_s));
    data.setData(QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC")));

    QXmppRegisterIq parsedIq;
    parsePacket(parsedIq, xml);
    QCOMPARE(parsedIq.type(), QXmppIq::Result);
    QCOMPARE(parsedIq.id(), u""_s);
    QCOMPARE(parsedIq.bitsOfBinaryData().size(), 1);
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid().algorithm(), data.cid().algorithm());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid().hash(), data.cid().hash());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().cid(), data.cid());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().contentType(), data.contentType());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().maxAge(), data.maxAge());
    QCOMPARE(parsedIq.bitsOfBinaryData().first().data(), data.data());
    QCOMPARE(parsedIq.bitsOfBinaryData().first(), data);
    serializePacket(parsedIq, xml);

    QXmppRegisterIq iq;
    iq.setType(QXmppIq::Result);
    iq.setId(u""_s);
    QXmppBitsOfBinaryDataList bobDataList;
    bobDataList << data;
    iq.setBitsOfBinaryData(bobDataList);
    serializePacket(iq, xml);

    QXmppRegisterIq iq2;
    iq2.setType(QXmppIq::Result);
    iq2.setId(u""_s);
    iq2.bitsOfBinaryData() << data;
    serializePacket(iq2, xml);

    // test const getter
    const QXmppRegisterIq constIq = iq;
    QCOMPARE(constIq.bitsOfBinaryData(), iq.bitsOfBinaryData());
}

void tst_QXmppRegisterIq::registerRegistered()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<registered/>"
        "<username>juliet</username>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.isRegistered());
    QCOMPARE(iq.username(), u"juliet"_s);
    serializePacket(iq, xml);

    iq = QXmppRegisterIq();
    iq.setId(u""_s);
    iq.setType(QXmppIq::Result);
    iq.setIsRegistered(true);
    iq.setUsername(u"juliet"_s);
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerRemove()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id='' type=\"result\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<remove/>"
        "<username>juliet</username>"
        "</query>"
        "</iq>");

    QXmppRegisterIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.isRemove());
    QCOMPARE(iq.username(), u"juliet"_s);
    serializePacket(iq, xml);

    iq = QXmppRegisterIq();
    iq.setId(u""_s);
    iq.setType(QXmppIq::Result);
    iq.setIsRemove(true);
    iq.setUsername(u"juliet"_s);
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerChangePassword()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"changePassword1\" to=\"shakespeare.lit\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<username>bill</username>"
        "<password>m1cr0$0ft</password>"
        "</query>"
        "</iq>");

    auto iq = QXmppRegisterIq::createChangePasswordRequest(
        u"bill"_s,
        u"m1cr0$0ft"_s,
        u"shakespeare.lit"_s);
    iq.setId(u"changePassword1"_s);
    serializePacket(iq, xml);
}

void tst_QXmppRegisterIq::registerUnregistration()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"unreg1\" to=\"shakespeare.lit\" type=\"set\">"
        "<query xmlns=\"jabber:iq:register\">"
        "<remove/>"
        "</query>"
        "</iq>");

    auto iq = QXmppRegisterIq::createUnregistrationRequest(u"shakespeare.lit"_s);
    iq.setId(u"unreg1"_s);
    serializePacket(iq, xml);
}

}  // namespace RegisterIq

QXMPP_TEST_MAIN(Registration::tst_QXmppRegistrationManager, tst_QXmppAccountMigrationManager, tst_QXmppMovedManager, Blocking::tst_QXmppBlockingManager, RegisterIq::tst_QXmppRegisterIq)

#include "ManagersAccounts.moc"

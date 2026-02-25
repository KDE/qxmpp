// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppAtmManager.h"
#include "QXmppAtmTrustMemoryStorage.h"
#include "QXmppCarbonManager.h"
#include "QXmppClient.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMessage.h"
#include "QXmppTrustMessageElement.h"
#include "QXmppTrustMessageKeyOwner.h"
#include "QXmppUtils.h"

#include "util.h"

#include <QObject>
#include <QSet>

using namespace QXmpp;

Q_DECLARE_METATYPE(QList<QXmppTrustMessageKeyOwner>)

// time period (in ms) to wait for a trust message that should not be sent.
constexpr int UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT = 1000;

static const char *ns_atm = "urn:xmpp:atm:1";
static const char *ns_omemo = "eu.siacs.conversations.axolotl";
static const char *ns_ox = "urn:xmpp:openpgp:0";

class tst_QXmppTrustManager : public QObject
{
    Q_OBJECT

public:
    Q_SIGNAL void unexpectedTrustMessageSent();

private:
    // TrustMemoryStorage
    Q_SLOT void memoryStorageSecurityPolicy();
    Q_SLOT void memoryStorageOwnKeys();
    Q_SLOT void memoryStorageKeys();
    Q_SLOT void memoryStorageTrustLevels();
    Q_SLOT void memoryStorageResetAll();

    // AtmTrustMemoryStorage
    Q_SLOT void atmStorageKeysForPostponedTrustDecisions();
    Q_SLOT void atmStorageResetAll();

    // AtmManager
    Q_SLOT void initTestCase();
    Q_SLOT void testSendTrustMessage();
    Q_SLOT void testMakePostponedTrustDecisions();
    Q_SLOT void testDistrustAutomaticallyTrustedKeys();
    Q_SLOT void testDistrust();
    Q_SLOT void testAuthenticate_data();
    Q_SLOT void testAuthenticate();
    Q_SLOT void testMakeTrustDecisions();
    Q_SLOT void testHandleMessage_data();
    Q_SLOT void testHandleMessage();
    Q_SLOT void testMakeTrustDecisionsNoKeys();
    Q_SLOT void testMakeTrustDecisionsOwnKeys();
    Q_SLOT void testMakeTrustDecisionsOwnKeysNoOwnEndpoints();
    Q_SLOT void testMakeTrustDecisionsOwnKeysNoOwnEndpointsWithAuthenticatedKeys();
    Q_SLOT void testMakeTrustDecisionsOwnKeysNoContactsWithAuthenticatedKeys();
    Q_SLOT void testMakeTrustDecisionsSoleOwnKeyDistrusted();
    Q_SLOT void testMakeTrustDecisionsContactKeys();
    Q_SLOT void testMakeTrustDecisionsContactKeysNoOwnEndpoints();
    Q_SLOT void testMakeTrustDecisionsContactKeysNoOwnEndpointsWithAuthenticatedKeys();
    Q_SLOT void testMakeTrustDecisionsSoleContactKeyDistrusted();

    void testMakeTrustDecisionsOwnKeysDone();
    void testMakeTrustDecisionsContactKeysDone();
    void clearTrustStorage();

    QXmppClient m_client;
    QXmppLogger m_logger;
    QXmppAtmTrustMemoryStorage m_trustStorage;
    QXmppAtmManager m_manager { &m_trustStorage };
    QXmppCarbonManager *m_carbonManager;
};

void tst_QXmppTrustManager::memoryStorageSecurityPolicy()
{
    QXmppTrustMemoryStorage storage;

    auto future = storage.securityPolicy(ns_ox);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(result, NoSecurityPolicy);

    storage.setSecurityPolicy(ns_omemo, Toakafa);

    future = storage.securityPolicy(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, NoSecurityPolicy);

    future = storage.securityPolicy(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, Toakafa);

    storage.resetSecurityPolicy(ns_omemo);

    future = storage.securityPolicy(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, NoSecurityPolicy);
}

void tst_QXmppTrustManager::memoryStorageOwnKeys()
{
    QXmppTrustMemoryStorage storage;

    auto future = storage.ownKey(ns_ox);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QVERIFY(result.isEmpty());

    storage.setOwnKey(ns_ox, QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));
    storage.setOwnKey(ns_omemo, QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")));

    // own OX key
    future = storage.ownKey(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));

    // own OMEMO key
    future = storage.ownKey(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")));

    storage.resetOwnKey(ns_omemo);

    // own OX key
    future = storage.ownKey(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));

    // no own OMEMO key
    future = storage.ownKey(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());
}

void tst_QXmppTrustManager::memoryStorageKeys()
{
    QXmppTrustMemoryStorage storage;

    // no OMEMO keys
    auto future = storage.keys(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QVERIFY(result.isEmpty());

    // no OMEMO keys (via JIDs)
    auto futureForJids = storage.keys(ns_omemo,
                                      { u"alice@example.org"_s, u"bob@example.com"_s });
    QVERIFY(futureForJids.isFinished());
    auto resultForJids = futureForJids.result();

    // no automatically trusted and authenticated OMEMO keys
    future = storage.keys(ns_omemo,
                          TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());

    // no automatically trusted and authenticated OMEMO key from Alice
    auto futureBool = storage.hasKey(ns_omemo,
                                     u"alice@example.org"_s,
                                     TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureBool.isFinished());
    auto resultBool = futureBool.result();
    QVERIFY(!resultBool);

    // Store keys with the default trust level.
    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("WaAnpWyW1hnFooH3oJo9Ba5XYoksnLPeJRTAjxPbv38=")),
          QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")) });

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) },
        TrustLevel::ManuallyDistrusted);

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tCP1CI3pqSTVGzFYFyPYUMfMZ9Ck/msmfD0wH/VtJBM=")),
          QByteArray::fromBase64(QByteArrayLiteral("2fhJtrgoMJxfLI3084/YkYh9paqiSiLFDVL2m0qAgX4=")) },
        TrustLevel::ManuallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")) },
        TrustLevel::Authenticated);

    storage.addKeys(
        ns_ox,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")),
          QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")) },
        TrustLevel::Authenticated);

    QMultiHash<QString, QByteArray> automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("WaAnpWyW1hnFooH3oJo9Ba5XYoksnLPeJRTAjxPbv38=")) },
                                                                    { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")) } };
    QMultiHash<QString, QByteArray> manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) } };
    QMultiHash<QString, QByteArray> automaticallyTrustedKeys = { { u"alice@example.org"_s,
                                                                   QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")) },
                                                                 { u"bob@example.com"_s,
                                                                   QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")) } };
    QMultiHash<QString, QByteArray> manuallyTrustedKeys = { { u"bob@example.com"_s,
                                                              QByteArray::fromBase64(QByteArrayLiteral("tCP1CI3pqSTVGzFYFyPYUMfMZ9Ck/msmfD0wH/VtJBM=")) },
                                                            { u"bob@example.com"_s,
                                                              QByteArray::fromBase64(QByteArrayLiteral("2fhJtrgoMJxfLI3084/YkYh9paqiSiLFDVL2m0qAgX4=")) } };
    QMultiHash<QString, QByteArray> authenticatedKeys = { { u"bob@example.com"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")) } };

    QHash<QByteArray, TrustLevel> keysAlice = { { QByteArray::fromBase64(QByteArrayLiteral("WaAnpWyW1hnFooH3oJo9Ba5XYoksnLPeJRTAjxPbv38=")),
                                                  TrustLevel::AutomaticallyDistrusted },
                                                { QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")),
                                                  TrustLevel::AutomaticallyDistrusted },
                                                { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")),
                                                  TrustLevel::ManuallyDistrusted },
                                                { QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")),
                                                  TrustLevel::AutomaticallyTrusted } };
    QHash<QByteArray, TrustLevel> keysBob = { { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")),
                                                TrustLevel::AutomaticallyTrusted },
                                              { QByteArray::fromBase64(QByteArrayLiteral("tCP1CI3pqSTVGzFYFyPYUMfMZ9Ck/msmfD0wH/VtJBM=")),
                                                TrustLevel::ManuallyTrusted },
                                              { QByteArray::fromBase64(QByteArrayLiteral("2fhJtrgoMJxfLI3084/YkYh9paqiSiLFDVL2m0qAgX4=")),
                                                TrustLevel::ManuallyTrusted },
                                              { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")),
                                                TrustLevel::Authenticated } };

    // all OMEMO keys
    future = storage.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::AutomaticallyDistrusted,
                    automaticallyDistrustedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys),
                std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::ManuallyTrusted,
                    manuallyTrustedKeys),
                std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys) }));

    // automatically trusted and authenticated OMEMO keys
    future = storage.keys(ns_omemo,
                          TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys) }));

    // all OMEMO keys (via JIDs)
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s, u"bob@example.com"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
                    u"alice@example.org"_s,
                    keysAlice),
                std::pair(
                    u"bob@example.com"_s,
                    keysBob) }));

    // Alice's OMEMO keys
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"alice@example.org"_s,
            keysAlice) }));

    keysAlice = { { QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")),
                    TrustLevel::AutomaticallyTrusted } };
    keysBob = { { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")),
                  TrustLevel::AutomaticallyTrusted },
                { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")),
                  TrustLevel::Authenticated } };

    // automatically trusted and authenticated OMEMO keys (via JIDs)
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s, u"bob@example.com"_s },
                                 TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
                    u"alice@example.org"_s,
                    keysAlice),
                std::pair(
                    u"bob@example.com"_s,
                    keysBob) }));

    // Alice's automatically trusted and authenticated OMEMO keys
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s },
                                 TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"alice@example.org"_s,
            keysAlice) }));

    // at least one automatically trusted or authenticated OMEMO key from Alice
    futureBool = storage.hasKey(ns_omemo,
                                u"alice@example.org"_s,
                                TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureBool.isFinished());
    resultBool = futureBool.result();
    QVERIFY(resultBool);

    storage.removeKeys(ns_omemo,
                       { QByteArray::fromBase64(QByteArrayLiteral("WaAnpWyW1hnFooH3oJo9Ba5XYoksnLPeJRTAjxPbv38=")),
                         QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")) });

    automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                      QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")) } };
    automaticallyTrustedKeys = { { u"bob@example.com"_s,
                                   QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")) } };

    keysAlice = { { QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")),
                    TrustLevel::AutomaticallyDistrusted },
                  { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")),
                    TrustLevel::ManuallyDistrusted } };
    keysBob = { { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")),
                  TrustLevel::AutomaticallyTrusted },
                { QByteArray::fromBase64(QByteArrayLiteral("tCP1CI3pqSTVGzFYFyPYUMfMZ9Ck/msmfD0wH/VtJBM=")),
                  TrustLevel::ManuallyTrusted },
                { QByteArray::fromBase64(QByteArrayLiteral("2fhJtrgoMJxfLI3084/YkYh9paqiSiLFDVL2m0qAgX4=")),
                  TrustLevel::ManuallyTrusted },
                { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")),
                  TrustLevel::Authenticated } };

    // OMEMO keys after removal
    future = storage.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::AutomaticallyDistrusted,
                    automaticallyDistrustedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys),
                std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::ManuallyTrusted,
                    manuallyTrustedKeys),
                std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys) }));

    // OMEMO keys after removal (via JIDs)
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s, u"bob@example.com"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
                    u"alice@example.org"_s,
                    keysAlice),
                std::pair(
                    u"bob@example.com"_s,
                    keysBob) }));

    // Alice's OMEMO keys after removal
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"alice@example.org"_s,
            keysAlice) }));

    keysAlice = { { QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")),
                    TrustLevel::AutomaticallyTrusted } };
    keysBob = { { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")),
                  TrustLevel::AutomaticallyTrusted },
                { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")),
                  TrustLevel::Authenticated } };

    // automatically trusted and authenticated OMEMO keys after removal (via JIDs)
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s, u"bob@example.com"_s },
                                 TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"bob@example.com"_s,
            keysBob) }));

    // Alice's automatically trusted and authenticated OMEMO keys after removal
    futureForJids = storage.keys(ns_omemo,
                                 { u"alice@example.org"_s },
                                 TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QVERIFY(resultForJids.isEmpty());

    storage.removeKeys(ns_omemo, u"alice@example.org"_s);

    // OMEMO keys after removing Alice's keys
    future = storage.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::ManuallyTrusted,
                    manuallyTrustedKeys),
                std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys) }));

    storage.removeKeys(ns_omemo);

    // no stored OMEMO keys
    future = storage.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());

    authenticatedKeys = { { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) },
                          { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")) } };

    keysAlice = { { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")),
                    TrustLevel::Authenticated },
                  { QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")),
                    TrustLevel::Authenticated } };

    // remaining OX keys
    future = storage.keys(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
            TrustLevel::Authenticated,
            authenticatedKeys) }));

    // remaining OX keys (via JIDs)
    futureForJids = storage.keys(ns_ox,
                                 { u"alice@example.org"_s, u"bob@example.com"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"alice@example.org"_s,
            keysAlice) }));

    // Alice's remaining OX keys
    futureForJids = storage.keys(ns_ox,
                                 { u"alice@example.org"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QCOMPARE(
        resultForJids,
        QHash({ std::pair(
            u"alice@example.org"_s,
            keysAlice) }));

    storage.removeKeys(ns_ox);

    // no stored OX keys
    future = storage.keys(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());

    // no stored OX keys (via JIDs)
    futureForJids = storage.keys(ns_ox,
                                 { u"alice@example.org"_s, u"bob@example.com"_s });
    QVERIFY(futureForJids.isFinished());
    resultForJids = futureForJids.result();
    QVERIFY(resultForJids.isEmpty());

    // no automatically trusted or authenticated OX key from Alice
    futureBool = storage.hasKey(ns_ox,
                                u"alice@example.org"_s,
                                TrustLevel::AutomaticallyTrusted | TrustLevel::Authenticated);
    QVERIFY(futureBool.isFinished());
    resultBool = futureBool.result();
    QVERIFY(!resultBool);
}

void tst_QXmppTrustManager::memoryStorageTrustLevels()
{
    QXmppTrustMemoryStorage storage;

    storage.addKeys(
        ns_ox,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")),
          QByteArray::fromBase64(QByteArrayLiteral("JU4pT7Ivpigtl+7QE87Bkq4r/C/mhI1FCjY5Wmjbtwg=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) },
        TrustLevel::ManuallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("9E51lG3vVmUn8CM7/AIcmIlLP2HPl6Ao0/VSf4VT/oA=")) },
        TrustLevel::AutomaticallyTrusted);

    auto future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")));
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(result, TrustLevel::AutomaticallyTrusted);

    storage.setTrustLevel(
        ns_omemo,
        { { u"alice@example.org"_s,
            QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")) },
          { u"bob@example.com"_s,
            QByteArray::fromBase64(QByteArrayLiteral("9E51lG3vVmUn8CM7/AIcmIlLP2HPl6Ao0/VSf4VT/oA=")) } },
        TrustLevel::Authenticated);

    future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    future = storage.trustLevel(
        ns_omemo,
        u"bob@example.com"_s,
        QByteArray::fromBase64(QByteArrayLiteral("9E51lG3vVmUn8CM7/AIcmIlLP2HPl6Ao0/VSf4VT/oA=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    // Set the trust level of a key that is not stored yet.
    // It is added to the storage automatically.
    storage.setTrustLevel(
        ns_omemo,
        { { u"alice@example.org"_s,
            QByteArray::fromBase64(QByteArrayLiteral("9w6oPjKyGSALd9gHq7sNOdOAkD5bHUVOKACNs89FjkA=")) } },
        TrustLevel::ManuallyTrusted);

    future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("9w6oPjKyGSALd9gHq7sNOdOAkD5bHUVOKACNs89FjkA=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::ManuallyTrusted);

    // Try to retrieve the trust level of a key that is not stored yet.
    // The default value is returned.
    future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("WXL4EDfzUGbVPQWjT9pmBeiCpCBzYZv3lUAaj+UbPyE=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Undecided);

    // Set the trust levels of all authenticated keys belonging to Alice and
    // Bob.
    storage.setTrustLevel(
        ns_omemo,
        { u"alice@example.org"_s,
          u"bob@example.com"_s },
        TrustLevel::Authenticated,
        TrustLevel::ManuallyDistrusted);

    future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("AZ/cF4OrUOILKO1gQBf62pQevOhBJ2NyHnXLwM4FDZU=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);

    future = storage.trustLevel(
        ns_omemo,
        u"bob@example.com"_s,
        QByteArray::fromBase64(QByteArrayLiteral("9E51lG3vVmUn8CM7/AIcmIlLP2HPl6Ao0/VSf4VT/oA=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);

    // Verify that the default trust level is returned for an unknown key.
    future = storage.trustLevel(
        ns_omemo,
        u"alice@example.org"_s,
        QByteArray::fromBase64(QByteArrayLiteral("wE06Gwf8f4DvDLFDoaCsGs8ibcUjf84WIOA2FAjPI3o=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Undecided);

    storage.removeKeys(ns_ox);
    storage.removeKeys(ns_omemo);
}

void tst_QXmppTrustManager::memoryStorageResetAll()
{
    QXmppTrustMemoryStorage storage;

    storage.setSecurityPolicy(ns_ox, Toakafa);
    storage.setSecurityPolicy(ns_omemo, Toakafa);

    storage.setOwnKey(ns_ox, QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));
    storage.setOwnKey(ns_omemo, QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")));

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("WaAnpWyW1hnFooH3oJo9Ba5XYoksnLPeJRTAjxPbv38=")),
          QByteArray::fromBase64(QByteArrayLiteral("/1eK3R2LtjPBT3el8f0q4DvzqUJSfFy5fkKkKPNFNYw=")) });

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) },
        TrustLevel::ManuallyDistrusted);

    storage.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("Ciemp4ZNzRJxnRD+k28vAie0kXJrwl4IrbfDy7n6OxE=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("rvSXBRd+EICMhQvVgcREQJxxP+T4EBmai4mYHBfJQGg=")) },
        TrustLevel::AutomaticallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tCP1CI3pqSTVGzFYFyPYUMfMZ9Ck/msmfD0wH/VtJBM=")),
          QByteArray::fromBase64(QByteArrayLiteral("2fhJtrgoMJxfLI3084/YkYh9paqiSiLFDVL2m0qAgX4=")) },
        TrustLevel::ManuallyTrusted);

    storage.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("YjVI04NcbTPvXLaA95RO84HPcSvyOgEZ2r5cTyUs0C8=")) },
        TrustLevel::Authenticated);

    storage.addKeys(
        ns_ox,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")),
          QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")) },
        TrustLevel::Authenticated);

    storage.resetAll(ns_omemo);

    auto future = storage.securityPolicy(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(result, NoSecurityPolicy);

    future = storage.securityPolicy(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, Toakafa);

    auto futureKey = storage.ownKey(ns_omemo);
    QVERIFY(futureKey.isFinished());
    auto resultKey = futureKey.result();
    QVERIFY(resultKey.isEmpty());

    futureKey = storage.ownKey(ns_ox);
    QVERIFY(futureKey.isFinished());
    resultKey = futureKey.result();
    QCOMPARE(resultKey, QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")));

    auto futureKeys = storage.keys(ns_omemo);
    QVERIFY(futureKeys.isFinished());
    auto resultKeys = futureKeys.result();
    QVERIFY(resultKeys.isEmpty());

    const QMultiHash<QString, QByteArray> authenticatedKeys = { { u"alice@example.org"_s,
                                                                  QByteArray::fromBase64(QByteArrayLiteral("aFABnX7Q/rbTgjBySYzrT2FsYCVYb49mbca5yB734KQ=")) },
                                                                { u"alice@example.org"_s,
                                                                  QByteArray::fromBase64(QByteArrayLiteral("IhpPjiKLchgrAG5cpSfTvdzPjZ5v6vTOluHEUehkgCA=")) } };

    futureKeys = storage.keys(ns_ox);
    QVERIFY(futureKeys.isFinished());
    resultKeys = futureKeys.result();
    QCOMPARE(
        resultKeys,
        QHash({ std::pair(
            TrustLevel::Authenticated,
            authenticatedKeys) }));
}

void tst_QXmppTrustManager::atmStorageKeysForPostponedTrustDecisions()
{
    QXmppAtmTrustMemoryStorage storage;

    // The key 7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=
    // is set for both postponed authentication and distrusting.
    // Thus, it is only stored for postponed distrusting.
    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("Wl53ZchbtAtCZQCHROiD20W7UnKTQgWQrjTHAVNw1ic=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("mB98hhdVps++skUuy4TGy/Vp6RQXLJO4JGf86FAUjyc=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=")) });

    QXmppTrustMessageKeyOwner keyOwnerBobTrustedKeys;
    keyOwnerBobTrustedKeys.setJid(u"bob@example.com"_s);
    keyOwnerBobTrustedKeys.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("GgTqeRLp1M+MEenzFQym2oqer9PfHukS4brJDQl5ARE=")) });

    storage.addKeysForPostponedTrustDecisions(ns_omemo,
                                              QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")),
                                              { keyOwnerAlice, keyOwnerBobTrustedKeys });

    QXmppTrustMessageKeyOwner keyOwnerBobDistrustedKeys;
    keyOwnerBobDistrustedKeys.setJid(u"bob@example.com"_s);
    keyOwnerBobDistrustedKeys.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA=")),
                                                  QByteArray::fromBase64(QByteArrayLiteral("X5tJ1D5rEeaeQE8eqhBKAj4KUZGYe3x+iHifaTBY1kM=")) });

    storage.addKeysForPostponedTrustDecisions(ns_omemo,
                                              QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")),
                                              { keyOwnerBobDistrustedKeys });

    QXmppTrustMessageKeyOwner keyOwnerCarol;
    keyOwnerCarol.setJid(u"carol@example.net"_s);
    keyOwnerCarol.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("WcL+cEMpEeK+dpqg3Xd3amctzwP8h2MqwXcEzFf6LpU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("bH3R31z0N97K1fUwG3+bdBrVPuDfXguQapHudkfa5nE=")) });
    keyOwnerCarol.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("N0B2StHKk1/slwg1rzybTFzjdg7FChc+3cXmTU/rS8g=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("wsEN32UHCiNjYqTG/J63hY4Nu8tZT42Ni1FxrgyRQ5g=")) });

    storage.addKeysForPostponedTrustDecisions(ns_ox,
                                              QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")),
                                              { keyOwnerCarol });

    QMultiHash<QString, QByteArray> trustedKeys = { { u"alice@example.org"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("Wl53ZchbtAtCZQCHROiD20W7UnKTQgWQrjTHAVNw1ic=")) },
                                                    { u"alice@example.org"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE=")) },
                                                    { u"bob@example.com"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("GgTqeRLp1M+MEenzFQym2oqer9PfHukS4brJDQl5ARE=")) } };
    QMultiHash<QString, QByteArray> distrustedKeys = { { u"alice@example.org"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("mB98hhdVps++skUuy4TGy/Vp6RQXLJO4JGf86FAUjyc=")) },
                                                       { u"alice@example.org"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=")) } };

    auto future = storage.keysForPostponedTrustDecisions(ns_omemo,
                                                         { QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")) });
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    distrustedKeys = { { u"alice@example.org"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("mB98hhdVps++skUuy4TGy/Vp6RQXLJO4JGf86FAUjyc=")) },
                       { u"alice@example.org"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=")) },
                       { u"bob@example.com"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA=")) },
                       { u"bob@example.com"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("X5tJ1D5rEeaeQE8eqhBKAj4KUZGYe3x+iHifaTBY1kM=")) } };

    future = storage.keysForPostponedTrustDecisions(ns_omemo,
                                                    { QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")),
                                                      QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")) });
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    // Retrieve all keys.
    future = storage.keysForPostponedTrustDecisions(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    keyOwnerBobTrustedKeys.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA=")) });

    // Invert the trust in Bob's key
    // sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA= for the
    // sending endpoint with the key
    // IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=.
    storage.addKeysForPostponedTrustDecisions(
        ns_omemo,
        QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")),
        { keyOwnerBobTrustedKeys });

    trustedKeys = { { u"bob@example.com"_s,
                      QByteArray::fromBase64(QByteArrayLiteral("sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA=")) } };
    distrustedKeys = { { u"bob@example.com"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("X5tJ1D5rEeaeQE8eqhBKAj4KUZGYe3x+iHifaTBY1kM=")) } };

    future = storage.keysForPostponedTrustDecisions(ns_omemo,
                                                    { QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")) });
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    storage.removeKeysForPostponedTrustDecisions(ns_omemo,
                                                 { QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")) });

    future = storage.keysForPostponedTrustDecisions(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    storage.addKeysForPostponedTrustDecisions(
        ns_omemo,
        QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")),
        { keyOwnerAlice });

    // The key QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE= is not removed
    // because its ID is passed within the parameter "keyIdsForDistrusting" but
    // stored for postponed authentication.
    storage.removeKeysForPostponedTrustDecisions(ns_omemo,
                                                 { QByteArray::fromBase64(QByteArrayLiteral("Wl53ZchbtAtCZQCHROiD20W7UnKTQgWQrjTHAVNw1ic=")),
                                                   QByteArray::fromBase64(QByteArrayLiteral("sD6ilugEBeKxPsdDEyX43LSGKHKWd5MFEdhT+4RpsxA=")) },
                                                 { QByteArray::fromBase64(QByteArrayLiteral("mB98hhdVps++skUuy4TGy/Vp6RQXLJO4JGf86FAUjyc=")),
                                                   QByteArray::fromBase64(QByteArrayLiteral("QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE=")) });

    trustedKeys = { { u"alice@example.org"_s,
                      QByteArray::fromBase64(QByteArrayLiteral("QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE=")) } };
    distrustedKeys = { { u"alice@example.org"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("7y1t0LnmNBeXJka43XejFPLrKtQlSFATrYmy7xHaKYU=")) },
                       { u"bob@example.com"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("X5tJ1D5rEeaeQE8eqhBKAj4KUZGYe3x+iHifaTBY1kM=")) } };

    future = storage.keysForPostponedTrustDecisions(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    // Remove all OMEMO keys.
    storage.removeKeysForPostponedTrustDecisions(ns_omemo);

    future = storage.keysForPostponedTrustDecisions(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());

    trustedKeys = { { u"carol@example.net"_s,
                      QByteArray::fromBase64(QByteArrayLiteral("WcL+cEMpEeK+dpqg3Xd3amctzwP8h2MqwXcEzFf6LpU=")) },
                    { u"carol@example.net"_s,
                      QByteArray::fromBase64(QByteArrayLiteral("bH3R31z0N97K1fUwG3+bdBrVPuDfXguQapHudkfa5nE=")) } };
    distrustedKeys = { { u"carol@example.net"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("N0B2StHKk1/slwg1rzybTFzjdg7FChc+3cXmTU/rS8g=")) },
                       { u"carol@example.net"_s,
                         QByteArray::fromBase64(QByteArrayLiteral("wsEN32UHCiNjYqTG/J63hY4Nu8tZT42Ni1FxrgyRQ5g=")) } };

    // remaining OX keys
    future = storage.keysForPostponedTrustDecisions(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    storage.removeKeysForPostponedTrustDecisions(ns_ox);

    // no OX keys
    future = storage.keysForPostponedTrustDecisions(ns_ox);
    QVERIFY(future.isFinished());
    result = future.result();
    QVERIFY(result.isEmpty());
}

void tst_QXmppTrustManager::atmStorageResetAll()
{
    QXmppAtmTrustMemoryStorage storage;

    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("Wl53ZchbtAtCZQCHROiD20W7UnKTQgWQrjTHAVNw1ic=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("QR05jrab7PFkSLhtdzyXrPfCqhkNCYCrlWATaBMTenE=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("mB98hhdVps++skUuy4TGy/Vp6RQXLJO4JGf86FAUjyc=")) });

    QXmppTrustMessageKeyOwner keyOwnerBobTrustedKeys;
    keyOwnerBobTrustedKeys.setJid(u"bob@example.com"_s);
    keyOwnerBobTrustedKeys.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("GgTqeRLp1M+MEenzFQym2oqer9PfHukS4brJDQl5ARE=")) });

    storage.addKeysForPostponedTrustDecisions(ns_omemo,
                                              QByteArray::fromBase64(QByteArrayLiteral("Mp6Y4wOF3aMcl38lb/VNbdPF9ucGFqSx2eyaEsqyHKE=")),
                                              { keyOwnerAlice, keyOwnerBobTrustedKeys });

    QXmppTrustMessageKeyOwner keyOwnerCarol;
    keyOwnerCarol.setJid(u"carol@example.net"_s);
    keyOwnerCarol.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("WcL+cEMpEeK+dpqg3Xd3amctzwP8h2MqwXcEzFf6LpU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("bH3R31z0N97K1fUwG3+bdBrVPuDfXguQapHudkfa5nE=")) });
    keyOwnerCarol.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("N0B2StHKk1/slwg1rzybTFzjdg7FChc+3cXmTU/rS8g=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("wsEN32UHCiNjYqTG/J63hY4Nu8tZT42Ni1FxrgyRQ5g=")) });

    storage.addKeysForPostponedTrustDecisions(ns_ox,
                                              QByteArray::fromBase64(QByteArrayLiteral("IL5iwDQwquH7yjb5RAiIP+nvYiBUsNCXtKB8IpKc9QU=")),
                                              { keyOwnerCarol });

    storage.resetAll(ns_omemo);

    auto futureKeysForPostponedTrustDecisions = storage.keysForPostponedTrustDecisions(ns_omemo);
    QVERIFY(futureKeysForPostponedTrustDecisions.isFinished());
    auto resultKeysForPostponedTrustDecisions = futureKeysForPostponedTrustDecisions.result();
    QVERIFY(resultKeysForPostponedTrustDecisions.isEmpty());

    QMultiHash<QString, QByteArray> trustedKeys = { { u"carol@example.net"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("WcL+cEMpEeK+dpqg3Xd3amctzwP8h2MqwXcEzFf6LpU=")) },
                                                    { u"carol@example.net"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("bH3R31z0N97K1fUwG3+bdBrVPuDfXguQapHudkfa5nE=")) } };
    QMultiHash<QString, QByteArray> distrustedKeys = { { u"carol@example.net"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("N0B2StHKk1/slwg1rzybTFzjdg7FChc+3cXmTU/rS8g=")) },
                                                       { u"carol@example.net"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("wsEN32UHCiNjYqTG/J63hY4Nu8tZT42Ni1FxrgyRQ5g=")) } };

    futureKeysForPostponedTrustDecisions = storage.keysForPostponedTrustDecisions(ns_ox);
    QVERIFY(futureKeysForPostponedTrustDecisions.isFinished());
    resultKeysForPostponedTrustDecisions = futureKeysForPostponedTrustDecisions.result();
    QCOMPARE(
        resultKeysForPostponedTrustDecisions,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));
}

void tst_QXmppTrustManager::initTestCase()
{
    m_client.addExtension(&m_manager);
    m_client.configuration().setJid("alice@example.org/phone");

    m_carbonManager = new QXmppCarbonManager;
    m_carbonManager->setCarbonsEnabled(true);
    m_client.addExtension(m_carbonManager);

    m_logger.setLoggingType(QXmppLogger::SignalLogging);
    m_client.setLogger(&m_logger);
}

void tst_QXmppTrustManager::testSendTrustMessage()
{
    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) });

    QXmppTrustMessageKeyOwner keyOwnerBob;
    keyOwnerBob.setJid(u"bob@example.com"_s);
    keyOwnerBob.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) });
    keyOwnerBob.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")),
                                    QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) });

    bool isMessageSent = false;
    const QObject context;

    // trust message to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [=, &isMessageSent, &keyOwnerAlice, &keyOwnerBob](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            isMessageSent = true;

            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            const std::optional<QXmppTrustMessageElement> trustMessageElement = message.trustMessageElement();

            QVERIFY(trustMessageElement);
            QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
            QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

            const auto sentKeyOwners = trustMessageElement->keyOwners();

            QCOMPARE(sentKeyOwners.size(), 2);

            for (auto &sentKeyOwner : sentKeyOwners) {
                if (sentKeyOwner.jid() == keyOwnerAlice.jid()) {
                    QCOMPARE(sentKeyOwner.trustedKeys(), keyOwnerAlice.trustedKeys());
                    QCOMPARE(sentKeyOwner.distrustedKeys(), keyOwnerAlice.distrustedKeys());
                } else if (sentKeyOwner.jid() == keyOwnerBob.jid()) {
                    QCOMPARE(sentKeyOwner.trustedKeys(), keyOwnerBob.trustedKeys());
                    QCOMPARE(sentKeyOwner.distrustedKeys(), keyOwnerBob.distrustedKeys());
                } else {
                    QFAIL("Unexpected key owner sent!");
                }
            }
        }
    });

    m_manager.sendTrustMessage(ns_omemo, { keyOwnerAlice, keyOwnerBob }, u"alice@example.org"_s);

    QVERIFY(isMessageSent);
}

void tst_QXmppTrustManager::testMakePostponedTrustDecisions()
{
    clearTrustStorage();

    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("wzsLdCDtOGUIoLkHAQN3Fdt86GLjE0716F0mnci/pVY=")),
                                                     { keyOwnerAlice });

    QXmppTrustMessageKeyOwner keyOwnerBob;
    keyOwnerBob.setJid(u"bob@example.com"_s);
    keyOwnerBob.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("3bqdCfhQalsOp3LcrFVucCQB4pRRWCyoBTV8KM/oOhY=")) });
    keyOwnerBob.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("baIfLxQhTrtY5JmZvsLaU1MenAU1wwZcI1B7MyWa0Is=")),
                                    QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("cF3Li3ddEJzt9rw/1eAmMS31/G/G4ZTpf+9wbEs51HA=")),
                                                     { keyOwnerBob });

    QXmppTrustMessageKeyOwner keyOwnerCarol;
    keyOwnerCarol.setJid(u"carol@example.net"_s);
    keyOwnerCarol.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("OxRakAGKtXyuB9sdH3gJDa1XzsV18BAMcVf/m1vD3Xg=")) });
    keyOwnerCarol.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("TKZIGhEMc+gyCgrJEyCnf7OtuoBFhOupOWhdwFhfZBk=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("Zgk0SxGFbeSgDw/Zanza/jzNrr6t1LU0jYX2d7RReKY=")),
                                                     { keyOwnerCarol });

    auto futureVoid = m_manager.makePostponedTrustDecisions(ns_omemo,
                                                            { QByteArray::fromBase64(QByteArrayLiteral("wzsLdCDtOGUIoLkHAQN3Fdt86GLjE0716F0mnci/pVY=")),
                                                              QByteArray::fromBase64(QByteArrayLiteral("cF3Li3ddEJzt9rw/1eAmMS31/G/G4ZTpf+9wbEs51HA=")) });
    while (!futureVoid.isFinished()) {
        QCoreApplication::processEvents();
    }

    auto futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                         { QByteArray::fromBase64(QByteArrayLiteral("wzsLdCDtOGUIoLkHAQN3Fdt86GLjE0716F0mnci/pVY=")),
                                                                           QByteArray::fromBase64(QByteArrayLiteral("cF3Li3ddEJzt9rw/1eAmMS31/G/G4ZTpf+9wbEs51HA=")) });
    QVERIFY(futurePostponed.isFinished());
    auto resultPostponed = futurePostponed.result();
    QVERIFY(resultPostponed.isEmpty());

    QMultiHash<QString, QByteArray> trustedKeys = { { u"carol@example.net"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("OxRakAGKtXyuB9sdH3gJDa1XzsV18BAMcVf/m1vD3Xg=")) } };
    QMultiHash<QString, QByteArray> distrustedKeys = { { u"carol@example.net"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("TKZIGhEMc+gyCgrJEyCnf7OtuoBFhOupOWhdwFhfZBk=")) } };

    futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                    { QByteArray::fromBase64(QByteArrayLiteral("Zgk0SxGFbeSgDw/Zanza/jzNrr6t1LU0jYX2d7RReKY=")) });
    QVERIFY(futurePostponed.isFinished());
    resultPostponed = futurePostponed.result();
    QCOMPARE(
        resultPostponed,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));

    QMultiHash<QString, QByteArray> authenticatedKeys = { { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")) },
                                                          { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                                                          { u"bob@example.com"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")) },
                                                          { u"bob@example.com"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("3bqdCfhQalsOp3LcrFVucCQB4pRRWCyoBTV8KM/oOhY=")) } };

    auto future = m_manager.keys(ns_omemo,
                                 TrustLevel::Authenticated);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
            TrustLevel::Authenticated,
            authenticatedKeys) }));

    QMultiHash<QString, QByteArray> manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")) },
                                                               { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) },
                                                               { u"bob@example.com"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("baIfLxQhTrtY5JmZvsLaU1MenAU1wwZcI1B7MyWa0Is=")) },
                                                               { u"bob@example.com"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) } };

    future = m_manager.keys(ns_omemo,
                            TrustLevel::ManuallyDistrusted);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
            TrustLevel::ManuallyDistrusted,
            manuallyDistrustedKeys) }));
}

void tst_QXmppTrustManager::testDistrustAutomaticallyTrustedKeys()
{
    clearTrustStorage();

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
        TrustLevel::AutomaticallyTrusted);

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) },
        TrustLevel::Authenticated);

    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("dZVdoBINK2n8BkWeTzVg0lVOah4n/9IA/IvQpzUuo1w=")) },
        TrustLevel::AutomaticallyTrusted);

    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("We+r1A/kixDad8e383oTmhPDy8g+F5/ircMJmEET8MA=")) },
        TrustLevel::ManuallyTrusted);

    m_manager.distrustAutomaticallyTrustedKeys(ns_omemo,
                                               { u"alice@example.org"_s,
                                                 u"bob@example.com"_s });

    QMultiHash<QString, QByteArray> automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                                                                    { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
                                                                    { u"bob@example.com"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("dZVdoBINK2n8BkWeTzVg0lVOah4n/9IA/IvQpzUuo1w=")) } };

    auto future = m_manager.keys(ns_omemo,
                                 TrustLevel::AutomaticallyDistrusted);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
            TrustLevel::AutomaticallyDistrusted,
            automaticallyDistrustedKeys) }));
}

void tst_QXmppTrustManager::testDistrust()
{
    clearTrustStorage();

    QMultiHash<QString, QByteArray> authenticatedKeys = { { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                                                          { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        authenticatedKeys.values(),
        TrustLevel::Authenticated);

    QMultiHash<QString, QByteArray> automaticallyTrustedKeys = { { u"bob@example.com"_s,
                                                                   QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        automaticallyTrustedKeys.values(),
        TrustLevel::AutomaticallyTrusted);

    QMultiHash<QString, QByteArray> manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("6FjJDKcwUxncGka8RvrTGSho+LVDX/7E0+pi5ueqOBQ=")) },
                                                               { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("QfXYzw7lmiD3Qoto6l2kx+HuM1tmKQYW2wCR+u78q8A=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        manuallyDistrustedKeys.values(),
        TrustLevel::ManuallyDistrusted);

    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                                     { keyOwnerAlice });

    QXmppTrustMessageKeyOwner keyOwnerBob;
    keyOwnerBob.setJid(u"bob@example.com"_s);
    keyOwnerBob.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")) });
    keyOwnerBob.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")),
                                                     { keyOwnerAlice, keyOwnerBob });

    // The entries for the sender key
    // tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=
    // and the keys of keyOwnerBob remain in the storage.
    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")),
                                                     { keyOwnerBob });

    auto futureVoid = m_manager.distrust(ns_omemo, {});
    QVERIFY(futureVoid.isFinished());

    auto future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys),
                std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys) }));

    futureVoid = m_manager.distrust(ns_omemo,
                                    { std::pair(
                                          u"alice@example.org"_s,
                                          QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI="))),
                                      std::pair(
                                          u"bob@example.com"_s,
                                          QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY="))) });
    while (!futureVoid.isFinished()) {
        QCoreApplication::processEvents();
    }

    authenticatedKeys = { { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) } };

    manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("6FjJDKcwUxncGka8RvrTGSho+LVDX/7E0+pi5ueqOBQ=")) },
                               { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("QfXYzw7lmiD3Qoto6l2kx+HuM1tmKQYW2wCR+u78q8A=")) },
                               { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                               { u"bob@example.com"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")) } };

    future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys) }));

    auto futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                         { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                                                           QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")) });
    QVERIFY(futurePostponed.isFinished());
    auto resultPostponed = futurePostponed.result();
    QVERIFY(resultPostponed.isEmpty());

    QMultiHash<QString, QByteArray> trustedKeys = { { u"bob@example.com"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")) } };
    QMultiHash<QString, QByteArray> distrustedKeys = { { u"bob@example.com"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) } };

    futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                    { QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) });
    QVERIFY(futurePostponed.isFinished());
    resultPostponed = futurePostponed.result();
    QCOMPARE(
        resultPostponed,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));
}

void tst_QXmppTrustManager::testAuthenticate_data()
{
    QTest::addColumn<TrustSecurityPolicy>("securityPolicy");

    QTest::newRow("noSecurityPolicy")
        << NoSecurityPolicy;

    QTest::newRow("toakafa")
        << Toakafa;
}

void tst_QXmppTrustManager::testAuthenticate()
{
    clearTrustStorage();

    QFETCH(TrustSecurityPolicy, securityPolicy);
    m_manager.setSecurityPolicy(ns_omemo, securityPolicy);

    QMultiHash<QString, QByteArray> authenticatedKeys = { { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("rQIL2albuSR1i06EZAp1uZ838zUeEgGIq2whwu3s+Zg=")) },
                                                          { u"carol@example.net"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("+CQZlFyxdeTGgbPby7YvvZT3YIVcIi+1E8N5nSc6QTA=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        authenticatedKeys.values(u"alice@example.org"_s),
        TrustLevel::Authenticated);

    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        authenticatedKeys.values(u"carol@example.net"_s),
        TrustLevel::Authenticated);

    QMultiHash<QString, QByteArray> automaticallyTrustedKeys = { { u"bob@example.com"_s,
                                                                   QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")) },
                                                                 { u"bob@example.com"_s,
                                                                   QByteArray::fromBase64(QByteArrayLiteral("/dqv0+RNyFIPdMQiJ7mSEJWKVExFeUBEvTXxOtqIMDg=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        automaticallyTrustedKeys.values(),
        TrustLevel::AutomaticallyTrusted);

    QMultiHash<QString, QByteArray> manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("6FjJDKcwUxncGka8RvrTGSho+LVDX/7E0+pi5ueqOBQ=")) },
                                                               { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("QfXYzw7lmiD3Qoto6l2kx+HuM1tmKQYW2wCR+u78q8A=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        manuallyDistrustedKeys.values(),
        TrustLevel::ManuallyDistrusted);

    QMultiHash<QString, QByteArray> automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                                                                    { u"alice@example.org"_s,
                                                                      QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) } };

    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        automaticallyDistrustedKeys.values(),
        TrustLevel::AutomaticallyDistrusted);

    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) });
    keyOwnerAlice.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")),
                                      QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                                     { keyOwnerAlice });

    QXmppTrustMessageKeyOwner keyOwnerBob;
    keyOwnerBob.setJid(u"bob@example.com"_s);
    keyOwnerBob.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")) });
    keyOwnerBob.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) });

    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")),
                                                     { keyOwnerAlice, keyOwnerBob });

    QXmppTrustMessageKeyOwner keyOwnerCarol;
    keyOwnerCarol.setJid(u"carol@example.net"_s);
    keyOwnerCarol.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("ikwzympBsVXz3AxqofZKWSPswNJIGiLGD1ItfGBQmHE=")) });
    keyOwnerCarol.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("+CQZlFyxdeTGgbPby7YvvZT3YIVcIi+1E8N5nSc6QTA=")) });

    // The keys of keyOwnerCarol are used for trust decisions once Bob's key
    // z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw= is
    // authenticated by the authentication of key
    // mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=.
    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")),
                                                     { keyOwnerCarol });

    // The entries for the sender key
    // LpzzOVOECo4N3P4B7CxYl7DBhCHBbtOBNa4FHOK+pD4=
    // and the keys of keyOwnerCarol are removed from the storage
    // because they are already used for trust decisions once Bob's key
    // z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw= is
    // authenticated.
    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("LpzzOVOECo4N3P4B7CxYl7DBhCHBbtOBNa4FHOK+pD4=")),
                                                     { keyOwnerCarol });

    keyOwnerCarol.setTrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("s/fRdN1iurUbZUHGdnIC7l7nllzv6ArLuwsK1GcgI58=")) });
    keyOwnerCarol.setDistrustedKeys({ QByteArray::fromBase64(QByteArrayLiteral("9D5EokNlchfgWRkfd7L+cpvkcTCCqwf5sKwcx0HfHbs=")) });

    // The entries for the sender key
    // KXVnPIqbak7+7XZ+58dkPoe6w3cN/GyjKj8IdJtcbt8=
    // and the keys of keyOwnerCarol remain in the storage.
    m_trustStorage.addKeysForPostponedTrustDecisions(ns_omemo,
                                                     QByteArray::fromBase64(QByteArrayLiteral("KXVnPIqbak7+7XZ+58dkPoe6w3cN/GyjKj8IdJtcbt8=")),
                                                     { keyOwnerCarol });

    auto futureVoid = m_manager.authenticate(ns_omemo, {});
    QVERIFY(futureVoid.isFinished());

    auto future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys),
                std::pair(
                    TrustLevel::AutomaticallyTrusted,
                    automaticallyTrustedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys),
                std::pair(
                    TrustLevel::AutomaticallyDistrusted,
                    automaticallyDistrustedKeys) }));

    futureVoid = m_manager.authenticate(ns_omemo,
                                        { std::pair(
                                              u"alice@example.org"_s,
                                              QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI="))),
                                          std::pair(
                                              u"bob@example.com"_s,
                                              QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY="))) });
    while (!futureVoid.isFinished()) {
        QCoreApplication::processEvents();
    }

    authenticatedKeys = { { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("rQIL2albuSR1i06EZAp1uZ838zUeEgGIq2whwu3s+Zg=")) },
                          { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                          { u"bob@example.com"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")) },
                          { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")) },
                          { u"alice@example.org"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                          { u"bob@example.com"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")) },
                          { u"carol@example.net"_s,
                            QByteArray::fromBase64(QByteArrayLiteral("ikwzympBsVXz3AxqofZKWSPswNJIGiLGD1ItfGBQmHE=")) } };

    manuallyDistrustedKeys = { { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("6FjJDKcwUxncGka8RvrTGSho+LVDX/7E0+pi5ueqOBQ=")) },
                               { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("QfXYzw7lmiD3Qoto6l2kx+HuM1tmKQYW2wCR+u78q8A=")) },
                               { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("eIpA0OrlpAQJ1Gh6NtMQa742GXGuwCRVmFcee2Ke3Gs=")) },
                               { u"alice@example.org"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("tsIeERvU+e0G7gSFyzAr8SOOkLiZhqBAYeSNSd2+lcs=")) },
                               { u"bob@example.com"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("U3+UnkTp12gusKbzWwN0lqDLEPb2CdMxP4bY85q9pxA=")) },
                               { u"carol@example.net"_s,
                                 QByteArray::fromBase64(QByteArrayLiteral("+CQZlFyxdeTGgbPby7YvvZT3YIVcIi+1E8N5nSc6QTA=")) } };

    if (securityPolicy == NoSecurityPolicy) {
        automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) } };

        automaticallyTrustedKeys = { { u"bob@example.com"_s,
                                       QByteArray::fromBase64(QByteArrayLiteral("/dqv0+RNyFIPdMQiJ7mSEJWKVExFeUBEvTXxOtqIMDg=")) } };
    } else if (securityPolicy == Toakafa) {
        automaticallyDistrustedKeys = { { u"alice@example.org"_s,
                                          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
                                        { u"bob@example.com"_s,
                                          QByteArray::fromBase64(QByteArrayLiteral("/dqv0+RNyFIPdMQiJ7mSEJWKVExFeUBEvTXxOtqIMDg=")) } };
    }

    future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    result = future.result();
    switch (securityPolicy) {
    case NoSecurityPolicy:
        QCOMPARE(
            result,
            QHash({ std::pair(
                        TrustLevel::Authenticated,
                        authenticatedKeys),
                    std::pair(
                        TrustLevel::AutomaticallyTrusted,
                        automaticallyTrustedKeys),
                    std::pair(
                        TrustLevel::ManuallyDistrusted,
                        manuallyDistrustedKeys),
                    std::pair(
                        TrustLevel::AutomaticallyDistrusted,
                        automaticallyDistrustedKeys) }));
        break;
    case Toakafa:
        QCOMPARE(
            result,
            QHash({ std::pair(
                        TrustLevel::Authenticated,
                        authenticatedKeys),
                    std::pair(
                        TrustLevel::ManuallyDistrusted,
                        manuallyDistrustedKeys),
                    std::pair(
                        TrustLevel::AutomaticallyDistrusted,
                        automaticallyDistrustedKeys) }));
        break;
    }

    auto futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                         { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                                                           QByteArray::fromBase64(QByteArrayLiteral("mwT0Hwr7aG1p+x0q60H0UDSEnr8cr7hxvxDEhFGrLmY=")),
                                                                           QByteArray::fromBase64(QByteArrayLiteral("z6MVV3MHGCZkKgapng8hQHCh57iZmlcQogmTmsy3/Kw=")),
                                                                           QByteArray::fromBase64(QByteArrayLiteral("LpzzOVOECo4N3P4B7CxYl7DBhCHBbtOBNa4FHOK+pD4=")) });
    QVERIFY(futurePostponed.isFinished());
    auto resultPostponed = futurePostponed.result();
    QVERIFY(resultPostponed.isEmpty());

    QMultiHash<QString, QByteArray> trustedKeys = { { u"carol@example.net"_s,
                                                      QByteArray::fromBase64(QByteArrayLiteral("s/fRdN1iurUbZUHGdnIC7l7nllzv6ArLuwsK1GcgI58=")) } };
    QMultiHash<QString, QByteArray> distrustedKeys = { { u"carol@example.net"_s,
                                                         QByteArray::fromBase64(QByteArrayLiteral("9D5EokNlchfgWRkfd7L+cpvkcTCCqwf5sKwcx0HfHbs=")) } };

    futurePostponed = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                    { QByteArray::fromBase64(QByteArrayLiteral("KXVnPIqbak7+7XZ+58dkPoe6w3cN/GyjKj8IdJtcbt8=")) });
    QVERIFY(futurePostponed.isFinished());
    resultPostponed = futurePostponed.result();
    QCOMPARE(
        resultPostponed,
        QHash({ std::pair(
                    true,
                    trustedKeys),
                std::pair(
                    false,
                    distrustedKeys) }));
}

void tst_QXmppTrustManager::testMakeTrustDecisions()
{
    clearTrustStorage();

    QMultiHash<QString, QByteArray> keysBeingAuthenticated = { { u"alice@example.org"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("b4XbD7VaiMNyHfb2cq7PLGTaW3iAM75iXQpLkcr3r0M=")) },
                                                               { u"bob@example.com"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("PJz644fYarsYEO1ECZhpqmrtboAB4lqNgSjhQik0jSM=")) } };
    QMultiHash<QString, QByteArray> keysBeingDistrusted = { { u"alice@example.org"_s,
                                                              QByteArray::fromBase64(QByteArrayLiteral("Pw4KZ2uLdEVuGTWaeSbwZsSstBzN2+prK0GDeD8HyKA=")) },
                                                            { u"bob@example.com"_s,
                                                              QByteArray::fromBase64(QByteArrayLiteral("Pw4KZ2uLdEVuGTWaeSbwZsSstBzN2+prK0GDeD8HyKA=")) } };

    auto futureVoid = m_manager.makeTrustDecisions(ns_omemo,
                                                   keysBeingAuthenticated,
                                                   keysBeingDistrusted);
    while (!futureVoid.isFinished()) {
        QCoreApplication::processEvents();
    }

    auto future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::Authenticated,
                    keysBeingAuthenticated),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    keysBeingDistrusted) }));
}

void tst_QXmppTrustManager::testHandleMessage_data()
{
    QTest::addColumn<QXmppMessage>("message");
    QTest::addColumn<bool>("areTrustDecisionsValid");
    QTest::addColumn<bool>("isSenderKeyAuthenticated");

    QXmppTrustMessageKeyOwner keyOwnerAlice;
    keyOwnerAlice.setJid(u"alice@example.org"_s);
    keyOwnerAlice.setTrustedKeys({ { QByteArray::fromBase64(QByteArrayLiteral("YHiLgLpE3dvoy4MayxycR+BABFY9w6D/rKZjUnu2jSY=")) },
                                   { QByteArray::fromBase64(QByteArrayLiteral("Ocp5ah/API6Ph83N3fFJZqObX7Rywg++D4EowImgFrw=")) } });
    keyOwnerAlice.setDistrustedKeys({ { QByteArray::fromBase64(QByteArrayLiteral("0PO+OhpTQkuM3Fd/CuhdWVuRZzYoUfQzOUvpcCIvKZQ=")) },
                                      { QByteArray::fromBase64(QByteArrayLiteral("fkcPYIctqF+bzuvkd6dVMv8z0EpFoA7sEuUNe/lvEx4=")) } });

    QXmppTrustMessageKeyOwner keyOwnerBob;
    keyOwnerBob.setJid(u"bob@example.com"_s);
    keyOwnerBob.setTrustedKeys({ { QByteArray::fromBase64(QByteArrayLiteral("nKT6zqFRNDq6GpWQIV/CwbA65fqN9Bo4qVxMfFjwl1w=")) },
                                 { QByteArray::fromBase64(QByteArrayLiteral("E4z5Qz9cWDt49j8JXxjSHGlQ9Xx6YESBX7ukfet2LhY=")) } });
    keyOwnerBob.setDistrustedKeys({ { QByteArray::fromBase64(QByteArrayLiteral("b3EsvoNBgUpiQD9KRHmosP/rR7T+3BA84MQw4N6eZmU=")) },
                                    { QByteArray::fromBase64(QByteArrayLiteral("guRlZo0QVxX3TbzdhyOwzdlorG0Znndo/P9NsWtMkk4=")) } });

    QXmppE2eeMetadata e2eeMetadata;
    e2eeMetadata.setSenderKey(QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")));

    QList<QXmppTrustMessageKeyOwner> keyOwners;
    keyOwners << keyOwnerAlice << keyOwnerBob;

    QXmppTrustMessageElement trustMessageElement;
    trustMessageElement.setUsage(ns_atm);
    trustMessageElement.setEncryption(ns_omemo);
    trustMessageElement.setKeyOwners(keyOwners);

    QXmppMessage message;
    message.setFrom(m_client.configuration().jid());
    message.setE2eeMetadata(e2eeMetadata);
    message.setTrustMessageElement(trustMessageElement);

    QTest::newRow("carbonForOwnMessage")
        << message
        << false
        << true;

    message.setFrom(u"alice@example.org/desktop"_s);
    message.setTrustMessageElement({});

    QTest::newRow("noTrustMessageElement")
        << message
        << false
        << true;

    trustMessageElement.setUsage(u"invalid-usage"_s);
    message.setTrustMessageElement(trustMessageElement);

    QTest::newRow("trustMessageElementNotForAtm")
        << message
        << false
        << true;

    trustMessageElement.setUsage(ns_atm);
    trustMessageElement.setKeyOwners({});
    message.setTrustMessageElement(trustMessageElement);

    QTest::newRow("trustMessageElementWithoutKeyOwners")
        << message
        << false
        << true;

    trustMessageElement.setKeyOwners(keyOwners);
    trustMessageElement.setEncryption(ns_ox);
    message.setTrustMessageElement(trustMessageElement);

    QTest::newRow("wrongEncryption")
        << message
        << false
        << true;

    trustMessageElement.setEncryption(ns_omemo);
    message.setTrustMessageElement(trustMessageElement);
    message.setFrom(u"carol@example.com/tablet"_s);

    QTest::newRow("senderNotQualifiedForTrustDecisions")
        << message
        << false
        << true;

    message.setFrom(u"alice@example.org/desktop"_s);

    QTest::newRow("senderKeyFromOwnEndpointNotAuthenticated")
        << message
        << true
        << false;

    QTest::newRow("trustMessageFromOwnEndpoint")
        << message
        << true
        << true;

    e2eeMetadata.setSenderKey(QByteArray::fromBase64(QByteArrayLiteral("qfNJsEMZ8jru0dS76DtYaTxZjiVQ5lpJWBiyaUj9UGU=")));
    message.setFrom(u"bob@example.com/notebook"_s);
    message.setE2eeMetadata(e2eeMetadata);

    QTest::newRow("senderKeyFromContactNotAuthenticated")
        << message
        << true
        << false;

    QTest::newRow("trustMessageFromContactEndpoint")
        << message
        << true
        << true;
}

void tst_QXmppTrustManager::testHandleMessage()
{
    clearTrustStorage();

    QFETCH(QXmppMessage, message);
    QFETCH(bool, areTrustDecisionsValid);
    QFETCH(bool, isSenderKeyAuthenticated);

    const auto senderJid = QXmppUtils::jidToBareJid(message.from());
    const auto senderKey = message.e2eeMetadata()->senderKey();

    // Add the sender key in preparation for the test.
    if (areTrustDecisionsValid) {
        if (isSenderKeyAuthenticated) {
            m_manager.addKeys(ns_omemo,
                              senderJid,
                              { senderKey },
                              TrustLevel::Authenticated);
        } else {
            m_manager.addKeys(ns_omemo,
                              senderJid,
                              { senderKey });
        }
    }

    auto future = m_manager.handleMessage(message);
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    // Remove the sender key as soon as the method being tested is executed.
    if (areTrustDecisionsValid) {
        m_manager.removeKeys(ns_omemo, QList { senderKey });
    }

    if (areTrustDecisionsValid) {
        const auto isOwnMessage = senderJid == m_client.configuration().jidBare();
        const auto keyOwners = message.trustMessageElement()->keyOwners();

        if (isSenderKeyAuthenticated) {
            QMultiHash<QString, QByteArray> authenticatedKeys;
            QMultiHash<QString, QByteArray> manuallyDistrustedKeys;

            if (isOwnMessage) {
                for (const auto &keyOwner : keyOwners) {
                    for (const auto &trustedKey : keyOwner.trustedKeys()) {
                        authenticatedKeys.insert(keyOwner.jid(), trustedKey);
                    }

                    for (const auto &distrustedKey : keyOwner.distrustedKeys()) {
                        manuallyDistrustedKeys.insert(keyOwner.jid(), distrustedKey);
                    }
                }

                auto future = m_manager.keys(ns_omemo);
                QVERIFY(future.isFinished());
                auto result = future.result();
                QCOMPARE(
                    result,
                    QHash({ std::pair(
                                TrustLevel::Authenticated,
                                authenticatedKeys),
                            std::pair(
                                TrustLevel::ManuallyDistrusted,
                                manuallyDistrustedKeys) }));

            } else {
                for (const auto &keyOwner : keyOwners) {
                    if (keyOwner.jid() == senderJid) {
                        for (const auto &trustedKey : keyOwner.trustedKeys()) {
                            authenticatedKeys.insert(keyOwner.jid(), trustedKey);
                        }

                        for (const auto &distrustedKey : keyOwner.distrustedKeys()) {
                            manuallyDistrustedKeys.insert(keyOwner.jid(), distrustedKey);
                        }
                    }
                }

                auto future = m_manager.keys(ns_omemo);
                QVERIFY(future.isFinished());
                auto result = future.result();
                QCOMPARE(
                    result,
                    QHash({ std::pair(
                                TrustLevel::Authenticated,
                                authenticatedKeys),
                            std::pair(
                                TrustLevel::ManuallyDistrusted,
                                manuallyDistrustedKeys) }));
            }
        } else {
            if (isOwnMessage) {
                QMultiHash<QString, QByteArray> trustedKeys;
                QMultiHash<QString, QByteArray> distrustedKeys;

                for (const auto &keyOwner : keyOwners) {
                    for (const auto &trustedKey : keyOwner.trustedKeys()) {
                        trustedKeys.insert(keyOwner.jid(), trustedKey);
                    }

                    for (const auto &distrustedKey : keyOwner.distrustedKeys()) {
                        distrustedKeys.insert(keyOwner.jid(), distrustedKey);
                    }
                }

                auto future = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                            { senderKey });
                QVERIFY(future.isFinished());
                auto result = future.result();
                QCOMPARE(
                    result,
                    QHash({ std::pair(
                                true,
                                trustedKeys),
                            std::pair(
                                false,
                                distrustedKeys) }));
            } else {
                QMultiHash<QString, QByteArray> trustedKeys;
                QMultiHash<QString, QByteArray> distrustedKeys;

                for (const auto &keyOwner : keyOwners) {
                    if (keyOwner.jid() == senderJid) {
                        for (const auto &trustedKey : keyOwner.trustedKeys()) {
                            trustedKeys.insert(keyOwner.jid(), trustedKey);
                        }

                        for (const auto &distrustedKey : keyOwner.distrustedKeys()) {
                            distrustedKeys.insert(keyOwner.jid(), distrustedKey);
                        }
                    }
                }

                auto futureHash = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo,
                                                                                { senderKey });
                QVERIFY(futureHash.isFinished());
                auto result = futureHash.result();
                QCOMPARE(
                    result,
                    QHash({ std::pair(
                                true,
                                trustedKeys),
                            std::pair(
                                false,
                                distrustedKeys) }));
            }
        }
    } else {
        auto futureHash = m_manager.keys(ns_omemo);
        QVERIFY(futureHash.isFinished());
        auto resultHash = futureHash.result();
        QVERIFY(resultHash.isEmpty());

        auto futureHash2 = m_trustStorage.keysForPostponedTrustDecisions(ns_omemo);
        QVERIFY(futureHash2.isFinished());
        auto resultHash2 = futureHash2.result();
        QVERIFY(resultHash2.isEmpty());
    }
}

void tst_QXmppTrustManager::testMakeTrustDecisionsNoKeys()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // key of own endpoints
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoints
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) },
        TrustLevel::ManuallyDistrusted);

    const QObject context;

    // unexpected trust message
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &) {
        if (type == QXmppLogger::SentMessage) {
            Q_EMIT unexpectedTrustMessageSent();
        }
    });

    auto futureVoid = m_manager.makeTrustDecisions(ns_omemo,
                                                   u"alice@example.org"_s,
                                                   {},
                                                   {});
    while (!futureVoid.isFinished()) {
        QCoreApplication::processEvents();
    }

    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    QMultiHash<QString, QByteArray> authenticatedKeys = { { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
                                                          { u"alice@example.org"_s,
                                                            QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) } };

    QMultiHash<QString, QByteArray> manuallyDistrustedKeys = { { u"bob@example.com"_s,
                                                                 QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) } };

    auto future = m_manager.keys(ns_omemo);
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(
        result,
        QHash({ std::pair(
                    TrustLevel::Authenticated,
                    authenticatedKeys),
                std::pair(
                    TrustLevel::ManuallyDistrusted,
                    manuallyDistrustedKeys) }));
}

void tst_QXmppTrustManager::testMakeTrustDecisionsOwnKeys()
{
    clearTrustStorage();

    // keys of own endpoints
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
        TrustLevel::Authenticated);
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) },
        TrustLevel::ManuallyDistrusted);

    // keys of contact's endpoints
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) },
        TrustLevel::ManuallyDistrusted);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own keys to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for own keys to Carol
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"carol@example.net") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for all keys to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 3);

                for (const auto &keyOwner : keyOwners) {
                    const auto keyOwnerJid = keyOwner.jid();

                    if (keyOwnerJid == u"alice@example.org") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                         QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) }));
                        QCOMPARE(keyOwner.distrustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) }));
                    } else if (keyOwnerJid == u"bob@example.com") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) }));
                        QCOMPARE(keyOwner.distrustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) }));
                    } else if (keyOwnerJid == u"carol@example.net") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) }));
                        QVERIFY(keyOwner.distrustedKeys().isEmpty());
                    } else {
                        QFAIL("Unexpected key owner sent!");
                    }
                }
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"alice@example.org"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 3);

    testMakeTrustDecisionsOwnKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsOwnKeysNoOwnEndpoints()
{
    clearTrustStorage();

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own keys to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for own keys to Carol
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"carol@example.net") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for contacts' keys to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 2);

                for (const auto &keyOwner : keyOwners) {
                    const auto keyOwnerJid = keyOwner.jid();

                    if (keyOwnerJid == u"bob@example.com") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) }));
                        QVERIFY(keyOwner.distrustedKeys().isEmpty());
                    } else if (keyOwnerJid == u"carol@example.net") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) }));
                        QVERIFY(keyOwner.distrustedKeys().isEmpty());
                    } else {
                        QFAIL("Unexpected key owner sent!");
                    }
                }
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"alice@example.org"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 3);

    testMakeTrustDecisionsOwnKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsOwnKeysNoOwnEndpointsWithAuthenticatedKeys()
{
    clearTrustStorage();

    // key of own endpoint
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) },
        TrustLevel::ManuallyDistrusted);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own keys to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for own keys to Carol
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"carol@example.net") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) }));
            }
        }
    });

    // trust message for all keys to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 3);

                for (const auto &keyOwner : keyOwners) {
                    const auto keyOwnerJid = keyOwner.jid();

                    if (keyOwnerJid == u"alice@example.org") {
                        QVERIFY(keyOwner.trustedKeys().isEmpty());
                        QCOMPARE(keyOwner.distrustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) }));
                    } else if (keyOwnerJid == u"bob@example.com") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) }));
                        QVERIFY(keyOwner.distrustedKeys().isEmpty());
                    } else if (keyOwnerJid == u"carol@example.net") {
                        QCOMPARE(keyOwner.trustedKeys(),
                                 QList({ QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) }));
                        QVERIFY(keyOwner.distrustedKeys().isEmpty());
                    } else {
                        QFAIL("Unexpected key owner sent!");
                    }
                }
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"alice@example.org"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 3);

    testMakeTrustDecisionsOwnKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsOwnKeysNoContactsWithAuthenticatedKeys()
{
    clearTrustStorage();

    // keys of own endpoints
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
        TrustLevel::Authenticated);

    // keys of contact's endpoints
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) },
        TrustLevel::AutomaticallyDistrusted);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own keys to own endpoints with authenticated keys
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);

                if (keyOwner.trustedKeys() == QList({ QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")), QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) }) &&
                    keyOwner.distrustedKeys() == QList({ QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) })) {
                    sentMessagesCount++;
                }
            }
        }
    });

    // trust message for own keys to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);

                const auto trustedKeys = keyOwner.trustedKeys();
                if (trustedKeys == QList({ QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")), QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) })) {
                    sentMessagesCount++;

                    QVERIFY(keyOwner.distrustedKeys().isEmpty());
                }
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"alice@example.org"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 2);

    testMakeTrustDecisionsOwnKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsSoleOwnKeyDistrusted()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // key of own endpoint
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own key to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QVERIFY(keyOwner.trustedKeys().isEmpty());
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) }));
            }
        }
    });

    // trust message for own key to Carol
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"carol@example.net") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QVERIFY(keyOwner.trustedKeys().isEmpty());
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) }));
            }
        }
    });

    // unexpected trust message for contacts' keys to own endpoint
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                Q_EMIT unexpectedTrustMessageSent();
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"alice@example.org"_s,
                                               {},
                                               { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 2);
    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    auto futureTrustLevel = m_manager.trustLevel(ns_omemo,
                                                 u"alice@example.org"_s,
                                                 QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")));
    QVERIFY(futureTrustLevel.isFinished());
    auto result = futureTrustLevel.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);
}

void tst_QXmppTrustManager::testMakeTrustDecisionsContactKeys()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // keys of own endpoints
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
          QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) },
        TrustLevel::Authenticated);
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) },
        TrustLevel::ManuallyDistrusted);

    // keys of contact's endpoints
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")),
          QByteArray::fromBase64(QByteArrayLiteral("T+dplAB8tGSdbYBbRiOm/jrS+8CPuzGHrH8ZmbjyvPo=")) },
        TrustLevel::Authenticated);
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) },
        TrustLevel::ManuallyDistrusted);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for Bob's keys to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"bob@example.com"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("mzDeKTQBVm1cTmzF9DjCGKa14pDADZOVLT9Kh7CK7AM=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("GHzmri+1U53eFRglbQhoXgU8vOpnXZ012Vg90HiLvWw=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("T+dplAB8tGSdbYBbRiOm/jrS+8CPuzGHrH8ZmbjyvPo=")) }));
            }
        }
    });

    // trust message for own keys to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QCOMPARE(keyOwner.trustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")),
                                 QByteArray::fromBase64(QByteArrayLiteral("tfskruc1xcfC+VKzuqvLZUJVZccZX/Pg5j88ukpuY2M=")) }));
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) }));
            }
        }
    });

    // unexpected trust message to Carol
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"carol@example.net") {
                Q_EMIT unexpectedTrustMessageSent();
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"bob@example.com"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("mzDeKTQBVm1cTmzF9DjCGKa14pDADZOVLT9Kh7CK7AM=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("GHzmri+1U53eFRglbQhoXgU8vOpnXZ012Vg90HiLvWw=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("T+dplAB8tGSdbYBbRiOm/jrS+8CPuzGHrH8ZmbjyvPo=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 2);
    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    testMakeTrustDecisionsContactKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsContactKeysNoOwnEndpoints()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    const QObject context;

    // unexpected trust message
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            Q_EMIT unexpectedTrustMessageSent();
        }
    });

    m_manager.makeTrustDecisions(ns_omemo,
                                 u"bob@example.com"_s,
                                 { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")),
                                   QByteArray::fromBase64(QByteArrayLiteral("mzDeKTQBVm1cTmzF9DjCGKa14pDADZOVLT9Kh7CK7AM=")) },
                                 { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) });

    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    testMakeTrustDecisionsContactKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsContactKeysNoOwnEndpointsWithAuthenticatedKeys()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // key of own endpoint
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) },
        TrustLevel::ManuallyDistrusted);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for own key to Bob
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"bob@example.com") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"alice@example.org"_s);
                QVERIFY(keyOwner.trustedKeys().isEmpty());
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("GaHysNhcfDSzG2q6OAThRGUpuFB9E7iCRR/1mK1TL+Q=")) }));
            }
        }
    });

    // unexpected trust message
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() != u"bob@example.com") {
                Q_EMIT unexpectedTrustMessageSent();
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"bob@example.com"_s,
                                               { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")),
                                                 QByteArray::fromBase64(QByteArrayLiteral("mzDeKTQBVm1cTmzF9DjCGKa14pDADZOVLT9Kh7CK7AM=")) },
                                               { QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 1);
    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    testMakeTrustDecisionsContactKeysDone();
}

void tst_QXmppTrustManager::testMakeTrustDecisionsSoleContactKeyDistrusted()
{
    clearTrustStorage();

    QSignalSpy unexpectedTrustMessageSentSpy(this, &tst_QXmppTrustManager::unexpectedTrustMessageSent);

    // key of own endpoint
    m_manager.addKeys(
        ns_omemo,
        u"alice@example.org"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("RwyI/3m9l4wgju9JduFxb5MEJvBNRDfPfo1Ewhl1DEI=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"bob@example.com"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) },
        TrustLevel::Authenticated);

    // key of contact's endpoint
    m_manager.addKeys(
        ns_omemo,
        u"carol@example.net"_s,
        { QByteArray::fromBase64(QByteArrayLiteral("tVy3ygBnW4q6V2TYe8p4i904zD+x4rNMRegxPnPI7fw=")) },
        TrustLevel::Authenticated);

    int sentMessagesCount = 0;
    const QObject context;

    // trust message for Bob's key to own endpoints
    connect(&m_logger, &QXmppLogger::message, &context, [&](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() == u"alice@example.org") {
                sentMessagesCount++;

                const auto trustMessageElement = message.trustMessageElement();

                QVERIFY(trustMessageElement);
                QCOMPARE(trustMessageElement->usage(), QString(ns_atm));
                QCOMPARE(trustMessageElement->encryption(), QString(ns_omemo));

                const auto keyOwners = trustMessageElement->keyOwners();
                QCOMPARE(keyOwners.size(), 1);

                const auto keyOwner = keyOwners.at(0);
                QCOMPARE(keyOwner.jid(), u"bob@example.com"_s);
                QVERIFY(keyOwner.trustedKeys().isEmpty());
                QCOMPARE(keyOwner.distrustedKeys(),
                         QList({ QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) }));
            }
        }
    });

    // unexpected trust message
    connect(&m_logger, &QXmppLogger::message, &context, [this](QXmppLogger::MessageType type, const QString &text) {
        if (type == QXmppLogger::SentMessage) {
            QXmppMessage message;
            parsePacket(message, text.toUtf8());

            if (message.to() != u"alice@example.org") {
                Q_EMIT unexpectedTrustMessageSent();
            }
        }
    });

    auto future = m_manager.makeTrustDecisions(ns_omemo,
                                               u"bob@example.com"_s,
                                               {},
                                               { QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")) });
    while (!future.isFinished()) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(sentMessagesCount, 1);
    QVERIFY2(!unexpectedTrustMessageSentSpy.wait(UNEXPECTED_TRUST_MESSAGE_WAITING_TIMEOUT), "Unexpected trust message sent!");

    const auto futureTrustLevel = m_manager.trustLevel(ns_omemo,
                                                       u"bob@example.com"_s,
                                                       QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")));
    QVERIFY(futureTrustLevel.isFinished());
    const auto result = futureTrustLevel.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);
}

void tst_QXmppTrustManager::testMakeTrustDecisionsOwnKeysDone()
{
    auto future = m_manager.trustLevel(ns_omemo,
                                       u"alice@example.org"_s,
                                       QByteArray::fromBase64(QByteArrayLiteral("0RcVsGk3LnpEFsqqztTzAgCDgVXlfa03paSqJFOOWOU=")));
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    future = m_manager.trustLevel(ns_omemo,
                                  u"alice@example.org"_s,
                                  QByteArray::fromBase64(QByteArrayLiteral("tYn/wcIOxBSoW4W1UfPr/zgbLipBK2KsFfC7F1bzut0=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    future = m_manager.trustLevel(ns_omemo,
                                  u"alice@example.org"_s,
                                  QByteArray::fromBase64(QByteArrayLiteral("4iBsyJPVAfNWM/OgyA9fasOvkJ8K1/0wuYpwVGw4Q5M=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);
}

void tst_QXmppTrustManager::testMakeTrustDecisionsContactKeysDone()
{
    auto future = m_manager.trustLevel(ns_omemo,
                                       u"bob@example.com"_s,
                                       QByteArray::fromBase64(QByteArrayLiteral("+1VJvMLCGvkDquZ6mQZ+SS+gTbQ436BJUwFOoW0Ma1g=")));
    QVERIFY(future.isFinished());
    auto result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    future = m_manager.trustLevel(ns_omemo,
                                  u"bob@example.com"_s,
                                  QByteArray::fromBase64(QByteArrayLiteral("mzDeKTQBVm1cTmzF9DjCGKa14pDADZOVLT9Kh7CK7AM=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::Authenticated);

    future = m_manager.trustLevel(ns_omemo,
                                  u"bob@example.com"_s,
                                  QByteArray::fromBase64(QByteArrayLiteral("8gBTC1fspYkO4akS6QKN+XFA9Nmf9NEIg7hjtlpTjII=")));
    QVERIFY(future.isFinished());
    result = future.result();
    QCOMPARE(result, TrustLevel::ManuallyDistrusted);
}

void tst_QXmppTrustManager::clearTrustStorage()
{
    m_manager.removeKeys(ns_omemo);
    m_trustStorage.removeKeysForPostponedTrustDecisions(ns_omemo);
}

QTEST_MAIN(tst_QXmppTrustManager)
#include "tst_qxmpptrustmanager.moc"

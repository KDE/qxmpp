// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOMANAGER_P_H
#define OMEMOMANAGER_P_H

#include "QXmppE2eeProvider.h"
#include "QXmppOmemoManager.h"
#include "QXmppPromise.h"
#include "QXmppTrustLevel.h"

#include "OmemoDeviceList.h"
#include "OmemoKeyMaintenance.h"
#include "OmemoPubSub.h"
#include "OmemoSession.h"

#include <array>

#include <QProperty>

class QXmppPubSubManager;
class QXmppRosterManager;
class QXmppTrustManager;

namespace QXmpp::Omemo {

struct DeviceWatch::Data {
    QString jid;
    std::function<void()> release;
    QProperty<DeviceWatch::State> state { DeviceWatch::State::Loading };
    QProperty<QList<Device>> devices;
    QProperty<QList<Version>> supportedVersions;

    ~Data()
    {
        if (release) {
            release();
        }
    }
};

}  // namespace QXmpp::Omemo

namespace QXmpp::Omemo::Private {

// maximum number of devices per JID and version
constexpr int MAX_DEVICES_PER_JID = 200;
// Maximum number of devices per JID and version that are not in the device list, e.g. only known
// from received stanzas. Stanzas of further unknown devices are not decrypted.
constexpr int MAX_UNLISTED_DEVICES_PER_JID = 20;
// Encryption for a device is stopped after this number of sent stanzas without an answer.
constexpr int UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_STOPS = 106;
// An empty message is sent after this number of received stanzas without sending one.
constexpr int UNRESPONDED_STANZAS_UNTIL_HEARTBEAT = 53;
constexpr auto DEFAULT_ACCEPTED_TRUST_LEVELS = TrustLevel::AutomaticallyTrusted | TrustLevel::ManuallyTrusted | TrustLevel::Authenticated;

struct Envelope {
    QString jid;
    uint32_t deviceId = 0;
    QByteArray data;
    bool isKeyExchange = false;
};

struct DecryptedEnvelope {
    SecureByteArray data;
    // in the format of the version
    QByteArray senderKey;
};

struct VersionState {
    Version version;
    VersionKeys keys;
    // devices of other JIDs and other own devices
    QHash<QString, QHash<uint32_t, OmemoStorage::Device>> devices;
    std::unique_ptr<SessionLayer> sessions;
    // a bundle publication is running / another one is needed after it
    bool isPublishingBundle = false;
    bool isBundleOutdated = false;
    // JIDs whose device list has been requested again since the login because of an unlisted device
    QSet<QString> refreshedDeviceLists;
    // JIDs whose device list has been received since the login, also if it is empty
    QSet<QString> receivedDeviceLists;
    // devices a session is being built with after receiving a message without session
    QSet<Address> pendingSessionBuilds;
};

// states of all versions, indexed by the version
struct VersionStates {
    std::array<VersionState, 2> states { VersionState { .version = Version::Omemo0 }, VersionState { .version = Version::Omemo2 } };

    VersionState &at(Version version) { return states[std::size_t(version)]; }
    const VersionState &at(Version version) const { return states[std::size_t(version)]; }
};

// QBindables of const properties are read-only.
template<typename T>
QBindable<T> readOnly(const QProperty<T> &property)
{
    return &property;
}

class OmemoManagerPrivate
{
public:
    OmemoManagerPrivate(OmemoManager *q, std::shared_ptr<OmemoStorage> storage, QList<Version> enabledVersions);

    QString ownJid() const;
    QString ownFullJid() const;
    QList<uint32_t> deviceIds(Version version, const QString &jid) const;
    QXmppTask<QList<Device>> devices(QString jid);

    // device watches
    bool hasPresenceSubscription(const QString &jid) const;
    bool isKnownJid(const QString &jid) const;
    qsizetype unlistedDeviceCount(Version version, const QString &jid) const;
    QXmppTask<void> login();
    void resume();
    void disconnected();
    DeviceWatch watch(const QString &jid);
    QXmppTask<void> startWatch(QString jid);
    QXmppTask<void> refreshWatch(QString jid);
    QXmppTask<void> updateWatch(QString jid);
    void updateWatches();
    QXmppTask<void> buildSessions(QString jid);
    void releaseWatch(const QString &jid);
    void unsubscribe(const OmemoStorage::Subscription &subscription);
    bool isEnabled(Version version) const { return enabledVersions.value().contains(version); }
    QDateTime now() const { return fixedTime.isValid() ? fixedTime : QDateTime::currentDateTimeUtc(); }
    Pep::Client pubSubClient(Version version) const { return { pubSub, version }; }

    void setUpSessionLayers();
    QXmppTask<void> setOwnTrustKey(Version version, QByteArray key);
    OmemoStorage::OwnDevice storedOwnDevice() const;
    QString label() const;

    QXmppTask<void> loaded();
    QXmppTask<void> load();
    QXmppTask<void> setUp();
    QXmppTask<Result<>> createOwnDevice();
    QXmppTask<Result<>> setUpVersion(Version version);
    QXmppTask<Result<>> unpublishOwnDevice(Version version);
    void setOwnDevice(DeviceKeys &&keys);
    void reportError(const QString &context, const QXmppError &error);

    QXmppTask<Result<uint32_t>> generateDeviceId();
    QXmppTask<Result<>> publishOwnData(Version version);
    QXmppTask<Result<>> publishBundle(Version version);
    QXmppTask<Result<>> publishOwnDeviceList(Version version);

    QXmppTask<Result<>> applyDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list);
    void applyReceivedDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list);
    void clearDeviceList(Version version, const QString &jid);
    void storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, DeviceList::DeviceInfo> &infos, const DeviceList::Changes &changes);
    void removeExpiredDevices();

    // encryption, see OmemoEncryption.cpp
    static QString trustNamespace(Version version);
    void registerProviders();
    QXmppTask<TrustLevel> trustLevel(Version version, QString jid, QByteArray key);
    QXmppTask<bool> prepareSession(Version version, QString jid, uint32_t deviceId, TrustLevels acceptedTrustLevels);
    QXmppTask<Result<QList<Envelope>>> encryptKey(Version version, QList<Jid> recipientJids, SecureByteArray keyMaterial, TrustLevels acceptedTrustLevels);
    QXmppTask<Result<DecryptedEnvelope>> decryptKey(Version version, QString jid, uint32_t deviceId, QByteArray data, bool isKeyExchange, bool isMessage);
    void refreshDeviceList(Version version, const QString &jid);
    void sendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void buildSessionAndSendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void refillPreKeysAndPublish(Version version);
    OmemoStorage::Device *storedDevice(Version version, const QString &jid, uint32_t deviceId);

    QXmppTask<E2ee::EncryptResult> encryptMessage2(E2ee::EncryptRequest request);
    QXmppTask<E2ee::EncryptResult> encryptIq2(E2ee::EncryptIqRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage2(E2ee::DecryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptIq2(E2ee::DecryptIqRequest request);
    QXmppTask<E2ee::EncryptResult> encryptMessage0(E2ee::EncryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage0(E2ee::DecryptRequest request);

    OmemoManager *q;
    std::shared_ptr<OmemoStorage> storage;
    QXmppPubSubManager *pubSub = nullptr;
    QXmppRosterManager *roster = nullptr;
    QXmppTrustManager *trustManager = nullptr;
    SignalContext context;
    KeyMaintenance::Config keyConfig;
    // replaces the current time in tests
    QDateTime fixedTime;

    QProperty<QList<Version>> enabledVersions;
    // the own device exists
    bool isSetUp = false;
    bool isSettingUp = false;
    bool isLoggedIn = false;
    bool isLoadStarted = false;
    QList<QXmppPromise<void>> loadPromises;
    // deviceId is 0 until set up
    DeviceKeys device;
    // empty for the default label
    QString customLabel;

    QHash<QString, std::weak_ptr<DeviceWatch::Data>> watches;
    // Results of the device list requests in the current stream, also of released watches, since
    // their device lists are still notified about until the next login.
    QHash<QString, DeviceWatch::State> requestResults;
    QList<OmemoStorage::Subscription> subscriptions;
    QSet<QString> sessionBuildsRunning;
    QSet<QString> sessionBuildsPending;

    QProperty<OmemoManager::State> state { OmemoManager::State::Loading };
    QProperty<uint32_t> ownDeviceId { 0 };
    QProperty<QByteArray> ownKey;
    QProperty<QByteArray> ownKeyOmemo0 { [this] { return omemo0IdentityKey(ownKey.value()); } };
    VersionStates versions;
};

}  // namespace QXmpp::Omemo::Private

#endif  // OMEMOMANAGER_P_H

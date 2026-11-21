// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOMANAGER_P_H
#define OMEMOMANAGER_P_H

#include "QXmppE2eeProvider.h"
#include "QXmppOmemoManager.h"
#include "QXmppPromise.h"
#include "QXmppTrustLevel.h"

#include "Async.h"
#include "OmemoDeviceList.h"
#include "OmemoKeyMaintenance.h"
#include "OmemoPubSub.h"
#include "OmemoSession.h"

#include <array>
#include <unordered_map>

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
    QProperty<QList<Version>> usableVersions;
    QProperty<QString> verificationUri;

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
// Maximum number of devices a stanza is encrypted for. A key exchange takes about 300 bytes, so
// the stanza stays below the default size limits of ejabberd and Prosody (256 KiB). Servers
// close the stream for larger stanzas.
constexpr int MAX_DEVICES_PER_STANZA = 500;
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

// a device a message is encrypted for
struct Recipient {
    Version version;
    QString jid;
    uint32_t deviceId = 0;
    TrustLevel trustLevel = TrustLevel::Undecided;
};

// a device in a version
struct DeviceAddress {
    Version version;
    QString jid;
    uint32_t deviceId = 0;
};

struct PreparedSession {
    TrustLevel trustLevel = TrustLevel::Undecided;
    // set if the device can not be encrypted for
    std::optional<EncryptionInfo::SkipReason> skipReason;
};

struct DeviceSelection {
    QList<Recipient> recipients;
    QList<EncryptionInfo::SkippedDevice> skippedDevices;
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
    // JIDs whose device list has been requested again since the login because of an unlisted device
    QSet<QString> refreshedDeviceLists;
    // JIDs whose device list has been received since the login, also if it is empty
    QSet<QString> receivedDeviceLists;
    // devices a session is being built with after receiving a message without session
    QSet<Address> pendingSessionBuilds;
    // devices with sessions changed while storing them is deferred (see deferDeviceStores())
    std::optional<QSet<Address>> deferredDeviceStores;
    // devices to be stored in the next event loop iteration (see storeDevices())
    QSet<Address> pendingDeviceStores;
    // running requests of contacts' data, shared by the callers requesting the same data meanwhile
    QXmpp::Private::AttachableRequests<QString, Result<QList<DeviceList::Entry>>> deviceListRequests;
    QXmpp::Private::AttachableRequests<Address, Result<Pep::PublicBundle>> bundleRequests;
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
    // The device ID is the same in both versions.
    bool isSentByThisDevice(const QString &senderJid, uint32_t senderDeviceId) const { return isSetUp && senderDeviceId == device.deviceId && senderJid == ownJid(); }
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
    QXmppTask<bool> refreshWatchedDeviceList(Version version, QString jid, bool subscribe);
    QXmppTask<void> updateWatch(QString jid);
    QXmppTask<void> runWatchUpdates(QString jid);
    QXmppTask<void> updateWatchOnce(QString jid);
    void updateWatches();
    QList<Device> mergeVersions(const QList<Device> &devices) const;
    bool isUsable(const Device &device);
    QString verificationUri(const QString &jid, const QList<Device> &devices) const;
    QXmppTask<void> authenticate(QString jid, QList<QByteArray> omemo2Keys, QList<QByteArray> omemo2DistrustedKeys, QList<QByteArray> omemo0Keys);
    QXmppTask<void> buildSessions(QString jid);
    void releaseWatch(const QString &jid);
    void unsubscribe(const OmemoStorage::Subscription &subscription);
    bool isEnabled(Version version) const { return enabledVersions.value().contains(version); }
    QDateTime now() const { return fixedTime.isValid() ? fixedTime : QDateTime::currentDateTimeUtc(); }
    Pep::Client pubSubClient(Version version) const { return { pubSub, version }; }
    QXmppTask<Result<QList<DeviceList::Entry>>> requestDeviceList(Version version, const QString &jid);
    QXmppTask<Result<Pep::PublicBundle>> requestBundle(Version version, const QString &jid, uint32_t deviceId);

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
    QList<QByteArray> sameIdentityKeys(Version version, const QString &jid, const QByteArray &key) const;
    QByteArray omemo0Key(const QByteArray &omemo2Key) const;
    bool isSameDevice(const OmemoStorage::Device &omemo2Device, const OmemoStorage::Device &omemo0Device) const;
    QXmppTask<void> handleTrustLevelChanges(QHash<QString, QMultiHash<QString, QByteArray>> modifiedKeys);
    QXmppTask<void> distrustAutomaticallyTrustedKeys(QString jid, QList<QByteArray> authenticatedKeys);
    bool usesOmemo2(const QString &jid, uint32_t omemo0DeviceId) const;
    bool isSticky(const OmemoStorage::Device &omemo2Device) const;
    QXmppTask<PreparedSession> prepareSession(Version version, QString jid, uint32_t deviceId, TrustLevels acceptedTrustLevels, bool onlyKeyIfUsingOmemo2 = false);
    QXmppTask<QList<std::pair<DeviceAddress, PreparedSession>>> prepareSessions(QList<DeviceAddress> devices, TrustLevels acceptedTrustLevels, bool onlyKeyIfUsingOmemo2 = false);
    QXmppTask<void> requestMissingDeviceLists(QList<Version> usedVersions, QList<QString> jids);
    QXmppTask<Result<DeviceSelection>> selectDevices(QList<Version> versions, QList<Jid> recipientJids, TrustLevels acceptedTrustLevels);
    Device publicDevice(Version version, const QString &jid, uint32_t deviceId, TrustLevel trustLevel);
    QList<Envelope> encryptKey(Version version, const QList<Recipient> &recipients, const SecureByteArray &keyMaterial, EncryptionInfo &info);
    QXmppTask<Result<DecryptedEnvelope>> decryptKey(Version version, QString jid, uint32_t deviceId, QByteArray data, bool isKeyExchange, bool isMessage);
    void refreshDeviceList(Version version, const QString &jid);
    void sendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void buildSessionAndSendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void refillPreKeysAndPublish(Version version);
    OmemoStorage::Device *storedDevice(Version version, const QString &jid, uint32_t deviceId);
    void storeDevices(Version version, const QSet<Address> &addresses);
    void storePendingDevices();

    // Runs an operation of the session layer without storing the devices whose sessions it
    // changes. Their addresses are added to changed, so the caller can store each device once
    // with its further changes.
    template<typename Operation>
    auto deferDeviceStores(Version version, QSet<Address> &changed, Operation operation)
    {
        auto &deferred = versions.at(version).deferredDeviceStores;
        deferred.emplace();
        auto result = operation();
        changed.unite(*deferred);
        deferred.reset();
        return result;
    }

    QXmppTask<E2ee::EncryptResult> encryptMessage2(E2ee::EncryptRequest request);
    QXmppTask<E2ee::EncryptResult> encryptIq2(E2ee::EncryptIqRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage2(E2ee::DecryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptIq2(E2ee::DecryptIqRequest request);
    QXmppTask<E2ee::EncryptResult> encryptMessage0(E2ee::EncryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage0(E2ee::DecryptRequest request);
    QXmppTask<E2ee::EncryptResult> encryptMessageAutomatic(E2ee::EncryptRequest request);

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
    QPropertyNotifier enabledVersionsNotifier;
    QPropertyNotifier ownKeyNotifier;
    // the own device exists
    bool isSetUp = false;
    int maxDevicesPerStanza = MAX_DEVICES_PER_STANZA;
    bool isSettingUp = false;
    bool isLoggedIn = false;
    bool isLoadStarted = false;
    QXmpp::Private::MultiPromise<void> loadPromises;
    // deviceId is 0 until set up
    DeviceKeys device;
    // empty for the default label
    QString customLabel;

    QHash<QString, std::weak_ptr<DeviceWatch::Data>> watches;
    // Results of the device list requests in the current stream, also of released watches, since
    // their device lists are still notified about until the next login.
    QHash<QString, DeviceWatch::State> requestResults;
    QList<OmemoStorage::Subscription> subscriptions;
    // JIDs whose sessions are being built
    QXmpp::Private::CoalescedRuns<QString> sessionBuilds;
    // versions whose bundle is being published
    QXmpp::Private::CoalescedRuns<Version> bundlePublications;
    // JIDs whose watches are being updated and the callers waiting for the next update
    QXmpp::Private::CoalescedRuns<QString> watchUpdates;
    std::unordered_map<QString, QXmpp::Private::MultiPromise<void>> requestedWatchUpdates;
    // see omemo0Key()
    mutable std::unordered_map<QByteArray, QByteArray> omemo0KeyCache;
    // see storeDevices()
    QXmpp::Private::CoalescedLater deviceStore { q, [this] { storePendingDevices(); } };

    QProperty<OmemoManager::State> state { OmemoManager::State::Loading };
    QProperty<uint32_t> ownDeviceId { 0 };
    QProperty<QByteArray> ownKey;
    QProperty<QByteArray> ownKeyOmemo0 { [this] { return omemo0IdentityKey(ownKey.value()); } };
    VersionStates versions;
};

}  // namespace QXmpp::Omemo::Private

#endif  // OMEMOMANAGER_P_H

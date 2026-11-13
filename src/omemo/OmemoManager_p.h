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

// maximum number of devices per JID and version
constexpr int MAX_DEVICES_PER_JID = 200;
// Encryption for a device is stopped after this number of sent stanzas without an answer.
constexpr int UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_IS_STOPPED = 106;
// An empty message is sent after this number of received stanzas without sending one.
constexpr int UNRESPONDED_STANZAS_UNTIL_HEARTBEAT = 53;
constexpr auto DEFAULT_ACCEPTED_TRUST_LEVELS = TrustLevel::AutomaticallyTrusted | TrustLevel::ManuallyTrusted | TrustLevel::Authenticated;

struct Envelope {
    QString jid;
    uint32_t deviceId = 0;
    QByteArray data;
    bool isKeyExchange = false;
};

struct DecryptedKey {
    Private::SecureByteArray data;
    // in the format of the version
    QByteArray senderKey;
};

struct VersionState {
    Version version;
    Private::VersionKeys keys;
    // devices of other JIDs and other own devices
    QHash<QString, QHash<uint32_t, Storage::Device>> devices;
    std::unique_ptr<Private::SessionLayer> sessions;
    // a bundle publication is running / another one is needed after it
    bool isPublishingBundle = false;
    bool isBundleOutdated = false;
    // JIDs whose device list has been requested again since the login because of an unlisted device
    QSet<QString> refreshedDeviceLists;
    // devices a session is being built with after receiving a message without session
    QSet<Private::Address> pendingSessionBuilds;
};

// states of all versions, indexed by the version
struct VersionStates {
    std::array<VersionState, 2> states { VersionState { .version = Version::Omemo0 }, VersionState { .version = Version::Omemo2 } };

    VersionState &at(Version version) { return states[std::size_t(version)]; }
    const VersionState &at(Version version) const { return states[std::size_t(version)]; }
};

struct DeviceWatch::Data {
    QString jid;
    std::function<void()> release;
    QProperty<DeviceWatch::State> state { DeviceWatch::State::Loading };
    // result of the last request in the current stream, unset if not requested yet
    std::optional<DeviceWatch::State> result;
    QProperty<QList<Device>> devices;
    QProperty<QList<Version>> supportedVersions;

    ~Data()
    {
        if (release) {
            release();
        }
    }
};

class ManagerPrivate
{
public:
    ManagerPrivate(Manager *q, std::shared_ptr<Storage> storage, QList<Version> enabledVersions);

    QString ownJid() const;
    QString ownFullJid() const;
    QList<uint32_t> deviceIds(Version version, const QString &jid) const;
    void devicesChanged(const QString &jid);
    QXmppTask<QList<Device>> devices(QList<QString> jids);

    // device watches
    bool hasPresenceSubscription(const QString &jid) const;
    QXmppTask<void> login();
    void resume();
    void disconnected();
    DeviceWatch watch(const QString &jid);
    QXmppTask<void> startWatch(QString jid);
    QXmppTask<void> refreshWatch(QString jid);
    QXmppTask<void> updateWatch(QString jid);
    QXmppTask<void> buildSessions(QString jid);
    void releaseWatch(const QString &jid);
    void unsubscribe(const Storage::Subscription &subscription);
    bool isEnabled(Version version) const { return enabledVersions.contains(version); }
    QDateTime now() const { return fixedTime.isValid() ? fixedTime : QDateTime::currentDateTimeUtc(); }
    Private::Pep::Client pubSubClient(Version version) const { return { pubSub, version }; }

    void setUpSessionLayers();
    QXmppTask<void> setOwnTrustKey(Version version, QByteArray key);
    Storage::OwnDevice storedOwnDevice() const;
    QString label() const;

    QXmppTask<void> loaded();
    QXmppTask<void> load();
    QXmppTask<void> setUp();
    QXmppTask<Result<>> createOwnDevice();
    QXmppTask<Result<>> setUpVersion(Version version);
    QXmppTask<Result<>> unpublishOwnDevice(Version version);
    void setOwnDevice(Private::DeviceKeys &&keys);
    void reportError(const QString &context, const QXmppError &error);

    QXmppTask<Result<uint32_t>> generateDeviceId();
    QXmppTask<Result<>> publishOwnData(Version version);
    QXmppTask<Result<>> publishBundle(Version version);
    QXmppTask<Result<>> publishOwnDeviceList(Version version);

    QXmppTask<Result<>> applyDeviceList(Version version, const QString &jid, const QList<Private::DeviceList::Entry> &list);
    void applyReceivedDeviceList(Version version, const QString &jid, const QList<Private::DeviceList::Entry> &list);
    void clearDeviceList(Version version, const QString &jid);
    void storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, Private::DeviceList::DeviceInfo> &infos, const Private::DeviceList::Changes &changes);
    void removeExpiredDevices();

    // encryption, see OmemoEncryption.cpp
    static QString trustNamespace(Version version);
    void registerProviders();
    QXmppTask<TrustLevel> trustLevel(Version version, QString jid, QByteArray key);
    QList<QByteArray> sameIdentityKeys(Version version, const QString &jid, const QByteArray &key) const;
    QXmppTask<void> mirrorTrustLevels(QHash<QString, QMultiHash<QString, QByteArray>> modifiedKeys);
    QXmppTask<bool> prepareSession(Version version, QString jid, uint32_t deviceId, TrustLevels acceptedTrustLevels);
    QXmppTask<Result<QList<Envelope>>> encryptKey(Version version, QList<QString> jids, Private::SecureByteArray keyMaterial, TrustLevels acceptedTrustLevels);
    QXmppTask<Result<DecryptedKey>> decryptKey(Version version, QString jid, uint32_t deviceId, QByteArray data, bool isKeyExchange, bool isMessage);
    void refreshDeviceList(Version version, const QString &jid);
    void sendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void buildSessionAndSendEmptyMessage(Version version, const QString &jid, uint32_t deviceId);
    void refillPreKeysAndPublish(Version version);
    Storage::Device *storedDevice(Version version, const QString &jid, uint32_t deviceId);

    QXmppTask<E2ee::EncryptResult> encryptMessage2(E2ee::EncryptRequest request);
    QXmppTask<E2ee::EncryptResult> encryptIq2(E2ee::EncryptIqRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage2(E2ee::DecryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptIq2(E2ee::DecryptIqRequest request);
    QXmppTask<E2ee::EncryptResult> encryptMessage0(E2ee::EncryptRequest request);
    QXmppTask<E2ee::DecryptResult> decryptMessage0(E2ee::DecryptRequest request);

    Manager *q;
    std::shared_ptr<Storage> storage;
    QXmppPubSubManager *pubSub = nullptr;
    QXmppRosterManager *roster = nullptr;
    QXmppTrustManager *trustManager = nullptr;
    Private::SignalContext context;
    Private::KeyMaintenance::Config keyConfig;
    // replaces the current time in tests
    QDateTime fixedTime;

    QList<Version> enabledVersions;
    // the own device exists
    bool isSetUp = false;
    bool isSettingUp = false;
    bool isLoggedIn = false;
    bool isLoadStarted = false;
    QList<QXmppPromise<void>> loadPromises;
    // deviceId is 0 until set up
    Private::DeviceKeys device;
    // empty for the default label
    QString customLabel;

    QHash<QString, std::weak_ptr<DeviceWatch::Data>> watches;
    QList<Storage::Subscription> subscriptions;
    QSet<QString> sessionBuildsRunning;
    QSet<QString> sessionBuildsPending;

    QProperty<Manager::State> state { Manager::State::Loading };
    QProperty<uint32_t> ownDeviceId { 0 };
    QProperty<QByteArray> ownKey;
    VersionStates versions;
};

}  // namespace QXmpp::Omemo

#endif  // OMEMOMANAGER_P_H

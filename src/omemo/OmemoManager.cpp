// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppJid_p.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppRosterManager.h"
#include "QXmppTrustManager.h"

#include "Async.h"
#include "OmemoManager_p.h"
#include "StringLiterals.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QScopeGuard>
#include <QSysInfo>

using namespace QXmpp::Private;
using namespace QXmpp::Omemo::Private;

namespace QXmpp::Omemo::Private {

static constexpr std::array ALL_VERSIONS { Version::Omemo0, Version::Omemo2 };

static QStringView deviceListNodeName(Version version)
{
    return version == Version::Omemo0 ? ns_omemo_devicelist : ns_omemo_2_devices;
}

OmemoManagerPrivate::OmemoManagerPrivate(OmemoManager *q, std::shared_ptr<OmemoStorage> storage, QList<Version> enabledVersions)
    : q(q), storage(std::move(storage)), enabledVersions(std::move(enabledVersions))
{
}

QString OmemoManagerPrivate::ownJid() const
{
    return jidBare(q->client()->configuration().jid());
}

void OmemoManagerPrivate::setUpSessionLayers()
{
    for (auto version : ALL_VERSIONS) {
        auto &state = versions.at(version);
        SessionLayer::Callbacks callbacks {
            .sessionStored = [this, version](const Address &address, const QByteArray &session) {
                auto &devices = versions.at(version).devices[address.jid];
                const auto isNew = !devices.contains(address.deviceId);
                auto &stored = devices[address.deviceId];
                // Devices only known from a key exchange are not encrypted for until they are
                // listed.
                if (isNew) {
                    stored.removalFromDeviceListDate = now();
                }
                stored.version = version;
                stored.jid = address.jid;
                stored.id = address.deviceId;
                stored.session = session;
                storage->addDevice(stored); },
            .sessionDeleted = [this, version](const Address &address) {
                auto &devices = versions.at(version).devices;
                if (auto it = devices.find(address.jid); it != devices.end() && it->contains(address.deviceId)) {
                    auto &stored = (*it)[address.deviceId];
                    stored.session.clear();
                    storage->addDevice(stored);
                } },
            .preKeyRemoved = [this, version](uint32_t id) { storage->removePreKey(version, id); },
        };
        state.sessions = std::make_unique<SessionLayer>(context, version, device, state.keys, std::move(callbacks));
        if (trustManager) {
            setOwnTrustKey(version, state.sessions->ownIdentityKey());
        }
    }
}

// Without an own key, the trust data of the version is new: TOAKAFA is used by default.
QXmppTask<void> OmemoManagerPrivate::setOwnTrustKey(Version version, QByteArray key)
{
    const auto encryption = trustNamespace(version);
    if ((co_await trustManager->ownKey(encryption).withContext(q)).isEmpty()) {
        co_await trustManager->setSecurityPolicy(encryption, Toakafa).withContext(q);
    }
    co_await trustManager->setOwnKey(encryption, key).withContext(q);
}

OmemoStorage::OwnDevice OmemoManagerPrivate::storedOwnDevice() const
{
    OmemoStorage::OwnDevice stored {
        device.deviceId,
        device.identity.privateKey.toByteArray(),
        device.identity.publicKey,
        device.latestSignedPreKeyId,
        {},
    };
    for (auto version : ALL_VERSIONS) {
        const auto &state = versions.at(version);
        if (state.keys.latestPreKeyId) {
            stored.latestPreKeyIds.append({ version, state.keys.latestPreKeyId });
        }
    }
    return stored;
}

QString OmemoManagerPrivate::label() const
{
    return customLabel.isEmpty() ? OmemoManager::defaultDeviceLabel() : customLabel;
}

void OmemoManagerPrivate::reportError(const QString &context, const QXmppError &error)
{
    q->warning(context + u": "_s + error.description);
    Q_EMIT q->errorOccurred(error);
}

// Resolves when the data has been loaded from the storage.
QXmppTask<void> OmemoManagerPrivate::loaded()
{
    if (state.value() != OmemoManager::State::Loading) {
        return makeReadyTask();
    }
    QXmppPromise<void> promise;
    auto task = promise.task();
    loadPromises.append(std::move(promise));
    return task;
}

QXmppTask<void> OmemoManagerPrivate::load()
{
    auto data = co_await storage->allData().withContext(q);
    subscriptions = data.subscriptions;

    for (auto version : ALL_VERSIONS) {
        auto &state = versions.at(version);
        state.devices.clear();
    }
    for (const auto &device : std::as_const(data.devices)) {
        auto &state = versions.at(device.version);
        state.devices[device.jid].insert(device.id, device);
        if (!device.session.isEmpty()) {
            state.keys.sessions.insert({ device.jid, device.id }, device.session);
        }
    }

    if (data.ownDevice) {
        const auto &ownDevice = *data.ownDevice;
        DeviceKeys keys {
            ownDevice.id,
            { SecureByteArray(ownDevice.privateIdentityKey), ownDevice.publicIdentityKey },
            {},
            ownDevice.latestSignedPreKeyId,
        };
        for (const auto &key : std::as_const(data.signedPreKeys)) {
            keys.signedPreKeys.insert(key.id, { SecureByteArray(key.data), key.creationDate });
        }
        for (auto version : ALL_VERSIONS) {
            auto &state = versions.at(version);
            const auto latest = std::ranges::find(ownDevice.latestPreKeyIds, version, &OmemoStorage::LatestPreKeyId::version);
            state.keys.latestPreKeyId = latest == ownDevice.latestPreKeyIds.cend() ? 0 : latest->id;
        }
        for (const auto &key : std::as_const(data.preKeys)) {
            versions.at(key.version).keys.preKeys.insert(key.id, SecureByteArray(key.data));
        }
        setOwnDevice(std::move(keys));
        removeExpiredDevices();
    } else {
        state = OmemoManager::State::NotSetUp;
    }

    for (auto &promise : loadPromises) {
        promise.finish();
    }
    loadPromises.clear();
}

void OmemoManagerPrivate::setOwnDevice(DeviceKeys &&keys)
{
    device = std::move(keys);
    setUpSessionLayers();
    isSetUp = true;
    ownDeviceId = device.deviceId;
    ownKey = device.identity.publicKey;
    state = OmemoManager::State::Ready;
}

QXmppTask<Result<>> OmemoManagerPrivate::createOwnDevice()
{
    auto deviceId = co_await generateDeviceId().withContext(q);
    if (auto *error = std::get_if<QXmppError>(&deviceId)) {
        co_return std::move(*error);
    }
    auto identity = context.generateIdentityKeyPair();
    if (!identity) {
        co_return QXmppError { u"Could not generate the identity key"_s, {} };
    }
    setOwnDevice(DeviceKeys { std::get<uint32_t>(deviceId), std::move(*identity), {}, 0 });
    co_return Success();
}

// Creates the own device if needed, renews the keys and publishes the data. Called on each login.
QXmppTask<void> OmemoManagerPrivate::setUp()
{
    co_await loaded().withContext(q);
    if (isSettingUp) {
        co_return;
    }
    isSettingUp = true;
    const auto finish = qScopeGuard([this] { isSettingUp = false; });

    if (!isSetUp) {
        state = OmemoManager::State::SettingUp;
        auto result = co_await createOwnDevice().withContext(q);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            state = OmemoManager::State::NotSetUp;
            reportError(u"Could not set up OMEMO"_s, *error);
            co_return;
        }
    }

    auto signedPreKeys = KeyMaintenance::rotateSignedPreKey(context, device, now(), keyConfig);
    if (auto *error = std::get_if<QXmppError>(&signedPreKeys)) {
        reportError(u"Could not renew the signed pre key"_s, *error);
        co_return;
    }
    const auto &changes = std::get<KeyMaintenance::SignedPreKeyChanges>(signedPreKeys);
    if (changes.addedId) {
        const auto &key = device.signedPreKeys.value(*changes.addedId);
        storage->addSignedPreKey({ *changes.addedId, key.record.toByteArray(), key.creationDate });
    }
    for (auto id : changes.removedIds) {
        storage->removeSignedPreKey(id);
    }
    storage->setOwnDevice(storedOwnDevice());

    for (auto version : ALL_VERSIONS) {
        auto &state = versions.at(version);
        state.refreshedDeviceLists.clear();
        state.receivedDeviceLists.clear();
    }

    for (auto version : ALL_VERSIONS) {
        // A version may have been disabled while being logged out.
        auto result = isEnabled(version) ? co_await setUpVersion(version).withContext(q)
                                         : co_await unpublishOwnDevice(version).withContext(q);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            reportError(u"Could not publish the OMEMO data"_s, *error);
        }
    }
}

// Replaces used pre keys and publishes the data of the version.
QXmppTask<Result<>> OmemoManagerPrivate::setUpVersion(Version version)
{
    auto &keys = versions.at(version).keys;
    auto added = KeyMaintenance::refillPreKeys(context, keys, keyConfig);
    if (auto *error = std::get_if<QXmppError>(&added)) {
        return makeReadyTask<Result<>>(std::move(*error));
    }
    QList<OmemoStorage::PreKey> records;
    for (auto id : std::get<QList<uint32_t>>(added)) {
        records.append({ version, id, keys.preKeys.value(id).toByteArray() });
    }
    if (!records.isEmpty()) {
        storage->addPreKeys(records);
        storage->setOwnDevice(storedOwnDevice());
    }
    return publishOwnData(version);
}

// Removes this device from the own device list of the version and its bundle if it is listed.
QXmppTask<Result<>> OmemoManagerPrivate::unpublishOwnDevice(Version version)
{
    const auto client = pubSubClient(version);
    const auto ownId = device.deviceId;
    auto list = co_await client.requestDeviceList(ownJid()).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&list)) {
        co_return std::move(*error);
    }
    auto &entries = std::get<QList<DeviceList::Entry>>(list);
    if (entries.removeIf([&](const DeviceList::Entry &entry) { return entry.id == ownId; }) == 0) {
        co_return Success();
    }
    auto result = co_await client.publishDeviceList(entries).withContext(q);
    if (std::holds_alternative<QXmppError>(result)) {
        co_return result;
    }
    co_return co_await client.deleteBundle(ownId).withContext(q);
}

// A device ID not listed in the own device lists.
QXmppTask<Result<uint32_t>> OmemoManagerPrivate::generateDeviceId()
{
    QSet<uint32_t> usedIds;
    for (auto version : enabledVersions.value()) {
        auto result = co_await pubSubClient(version).requestDeviceList(ownJid()).withContext(q);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            co_return std::move(*error);
        }
        for (const auto &entry : std::get<QList<DeviceList::Entry>>(result)) {
            usedIds.insert(entry.id);
        }
    }

    for (int attempt = 0; attempt < 100; ++attempt) {
        if (auto id = context.generateDeviceId(); id && !usedIds.contains(*id)) {
            co_return *id;
        }
    }
    co_return QXmppError { u"Could not generate a device ID"_s, {} };
}

QXmppTask<Result<>> OmemoManagerPrivate::publishOwnData(Version version)
{
    auto result = co_await publishBundle(version).withContext(q);
    if (std::holds_alternative<QXmppError>(result)) {
        co_return result;
    }
    co_return co_await publishOwnDeviceList(version).withContext(q);
}

QXmppTask<Result<>> OmemoManagerPrivate::publishBundle(Version version)
{
    auto &state = versions.at(version);
    if (state.keys.preKeys.isEmpty()) {
        return makeReadyTask<Result<>>(QXmppError { u"There are no pre keys to publish"_s, {} });
    }

    auto bundle = state.sessions->ownBundle(device.latestSignedPreKeyId, state.keys.preKeys.constBegin().key());
    if (auto *error = std::get_if<QXmppError>(&bundle)) {
        return makeReadyTask<Result<>>(std::move(*error));
    }
    auto preKeys = state.sessions->publicPreKeys();
    if (auto *error = std::get_if<QXmppError>(&preKeys)) {
        return makeReadyTask<Result<>>(std::move(*error));
    }

    const auto &ownBundle = std::get<Bundle>(bundle);
    return pubSubClient(version).publishBundle(device.deviceId,
                                               {
                                                   ownBundle.identityKey,
                                                   ownBundle.signedPreKeyId,
                                                   ownBundle.signedPreKey,
                                                   ownBundle.signedPreKeySignature,
                                                   std::get<QHash<uint32_t, QByteArray>>(preKeys),
                                               });
}

// Fetches the own device list and republishes it if this device is missing or listed wrongly.
QXmppTask<Result<>> OmemoManagerPrivate::publishOwnDeviceList(Version version)
{
    auto result = co_await pubSubClient(version).requestDeviceList(ownJid()).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }

    co_return co_await applyDeviceList(version, ownJid(), std::get<QList<DeviceList::Entry>>(result)).withContext(q);
}

static QHash<uint32_t, DeviceList::DeviceInfo> deviceInfos(const QHash<uint32_t, OmemoStorage::Device> &devices)
{
    QHash<uint32_t, DeviceList::DeviceInfo> infos;
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        infos.insert(it.key(), { it->label, it->removalFromDeviceListDate });
    }
    return infos;
}

// Returns the publication of the own device list if it must be republished.
QXmppTask<Result<>> OmemoManagerPrivate::applyDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list)
{
    versions.at(version).receivedDeviceLists.insert(jid);
    const auto isOwnJid = jid == ownJid();
    auto infos = deviceInfos(versions.at(version).devices.value(jid));

    std::optional<DeviceList::OwnDevice> ownDevice;
    if (isOwnJid) {
        ownDevice = DeviceList::OwnDevice { device.deviceId, version == Version::Omemo2 ? std::optional(label()) : std::nullopt };
    }

    const auto changes = DeviceList::apply(infos, list, now(), MAX_DEVICES_PER_JID, ownDevice);
    if (changes.ignoredCount) {
        q->warning(u"Ignored %1 devices of %2 above the maximum of %3"_s.arg(QString::number(changes.ignoredCount), jid, QString::number(MAX_DEVICES_PER_JID)));
    }
    storeDeviceListChanges(version, jid, infos, changes);

    if (changes.republishOwnList) {
        return pubSubClient(version).publishDeviceList(DeviceList::ownList(infos, *ownDevice));
    }
    return makeReadyTask<Result<>>(Success());
}

void OmemoManagerPrivate::applyReceivedDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list)
{
    applyDeviceList(version, jid, list).then(q, [this](Result<> &&result) {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            q->warning(u"Could not publish the own device list: "_s + error->description);
        }
    });
}

void OmemoManagerPrivate::clearDeviceList(Version version, const QString &jid)
{
    if (jid == ownJid()) {
        // The own device list has been removed, e.g. by resetting all devices on another device:
        // publish the own data again.
        publishOwnData(version).then(q, [this](Result<> &&result) {
            if (auto *error = std::get_if<QXmppError>(&result)) {
                q->warning(u"Could not publish the OMEMO data: "_s + error->description);
            }
        });
        return;
    }

    auto infos = deviceInfos(versions.at(version).devices.value(jid));
    const auto changes = DeviceList::clear(infos, now());
    storeDeviceListChanges(version, jid, infos, changes);
}

void OmemoManagerPrivate::storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, DeviceList::DeviceInfo> &infos, const DeviceList::Changes &changes)
{
    if (changes.isEmpty()) {
        return;
    }

    auto &devices = versions.at(version).devices[jid];
    auto update = [&](uint32_t id) {
        auto &stored = devices[id];
        stored.version = version;
        stored.jid = jid;
        stored.id = id;
        stored.label = infos.value(id).label;
        stored.removalFromDeviceListDate = infos.value(id).removalDate;
        storage->addDevice(stored);
    };
    for (const auto ids : { &changes.added, &changes.changed, &changes.removed }) {
        for (auto id : *ids) {
            update(id);
        }
    }
    updateWatch(jid);
    if (!changes.added.isEmpty()) {
        buildSessions(jid);
    }
}

void OmemoManagerPrivate::removeExpiredDevices()
{
    const auto currentTime = now();
    for (auto version : ALL_VERSIONS) {
        auto &state = versions.at(version);
        for (auto jidIt = state.devices.begin(); jidIt != state.devices.end();) {
            const auto expired = DeviceList::expired(deviceInfos(*jidIt), currentTime);
            for (auto id : expired) {
                jidIt->remove(id);
                state.keys.sessions.remove({ jidIt.key(), id });
                storage->removeDevice(version, jidIt.key(), id);
            }
            if (!expired.isEmpty()) {
                updateWatch(jidIt.key());
            }
            jidIt = jidIt->isEmpty() ? state.devices.erase(jidIt) : std::next(jidIt);
        }
    }
}

QString OmemoManagerPrivate::ownFullJid() const
{
    return q->client()->configuration().jid();
}

// The IDs of the devices of the JID listed in its device list of the version, sorted. For the own
// JID, this device is not included.
QList<uint32_t> OmemoManagerPrivate::deviceIds(Version version, const QString &jid) const
{
    QList<uint32_t> ids;
    const auto devices = versions.at(version).devices.value(jid);
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        if (!it->removalFromDeviceListDate.isValid()) {
            ids.append(it.key());
        }
    }
    std::ranges::sort(ids);
    return ids;
}

// Applies changed trust levels to the same identity keys in the other version, so a device has to
// be authenticated only once.
QXmppTask<void> OmemoManagerPrivate::mirrorTrustLevels(QHash<QString, QMultiHash<QString, QByteArray>> modifiedKeys)
{
    for (auto version : ALL_VERSIONS) {
        const auto keys = modifiedKeys.value(trustNamespace(version));
        const auto otherEncryption = trustNamespace(version == Version::Omemo0 ? Version::Omemo2 : Version::Omemo0);
        for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
            const auto sameKeys = sameIdentityKeys(version, it.key(), it.value());
            if (sameKeys.isEmpty()) {
                continue;
            }
            const auto level = co_await trustManager->trustLevel(trustNamespace(version), it.key(), it.value()).withContext(q);
            for (const auto &sameKey : sameKeys) {
                // The trust level is equal afterwards, so the resulting change is not mirrored back.
                if (co_await trustManager->trustLevel(otherEncryption, it.key(), sameKey).withContext(q) != level) {
                    co_await trustManager->setTrustLevel(otherEncryption, { { it.key(), sameKey } }, level).withContext(q);
                }
            }
        }
    }
}

// The listed devices of the bare JID with their trust levels from the local data, sorted by version
// and ID
QXmppTask<QList<Device>> OmemoManagerPrivate::devices(QString jid)
{
    co_await loaded().withContext(q);
    if (!trustManager) {
        co_return {};
    }

    std::array<QHash<QByteArray, TrustLevel>, 2> trustLevels;
    for (auto version : ALL_VERSIONS) {
        trustLevels[std::size_t(version)] = (co_await trustManager->keys(trustNamespace(version), { jid }).withContext(q)).value(jid);
    }

    const auto deviceJid = jidOrNull(jid);
    QList<Device> devices;
    for (auto version : ALL_VERSIONS) {
        const auto stored = versions.at(version).devices.value(jid);
        const auto &keys = trustLevels[std::size_t(version)];
        for (auto id : deviceIds(version, jid)) {
            const auto &device = stored[id];
            const auto trustLevel = device.keyId.isEmpty() ? TrustLevel::Undecided : keys.value(device.keyId, TrustLevel::Undecided);
            devices.append({ version, deviceJid, id, device.label, device.keyId, trustLevel });
        }
    }
    co_return devices;
}

// Device list notifications require a presence subscription to the contact.
bool OmemoManagerPrivate::hasPresenceSubscription(const QString &jid) const
{
    if (!roster) {
        return false;
    }
    const auto type = roster->getRosterEntry(jid).subscriptionType();
    return type == QXmppRosterIq::Item::To || type == QXmppRosterIq::Item::Both;
}

// Whether stanzas may be sent to the JID and data be stored for it without the user's action: the
// own JID, contacts in the roster and JIDs whose devices are known from their device lists, e.g.
// because they are watched. Otherwise anyone could make this device send stanzas, which tell
// that it is online, and request bundles.
bool OmemoManagerPrivate::isKnownJid(const QString &jid) const
{
    if (jid == ownJid() || !watches.value(jid).expired() || (roster && !roster->getRosterEntry(jid).bareJid().isEmpty())) {
        return true;
    }
    return std::ranges::any_of(ALL_VERSIONS, [&](Version version) { return !deviceIds(version, jid).isEmpty(); });
}

// The number of stored devices of the JID that are not in its device list.
qsizetype OmemoManagerPrivate::unlistedDeviceCount(Version version, const QString &jid) const
{
    const auto devices = versions.at(version).devices.value(jid);
    return std::ranges::count_if(devices, [](const OmemoStorage::Device &device) { return device.removalFromDeviceListDate.isValid(); });
}

QXmppTask<void> OmemoManagerPrivate::login()
{
    co_await setUp().withContext(q);

    // Subscriptions of earlier sessions, e.g. if the application has been closed without logging
    // out, and of watches released in the last session.
    const auto subscriberJid = ownFullJid();
    const auto stored = subscriptions;
    for (const auto &subscription : stored) {
        if (subscription.subscriberJid != subscriberJid || watches.value(subscription.jid).expired()) {
            unsubscribe(subscription);
        }
    }

    requestResults.clear();
    const auto jids = watches.keys();
    for (const auto &jid : jids) {
        co_await refreshWatch(jid).withContext(q);
    }
}

// The stream has been resumed, so no device list notification has been missed.
void OmemoManagerPrivate::resume()
{
    const auto jids = watches.keys();
    for (const auto &jid : jids) {
        if (auto watch = watches.value(jid).lock()) {
            if (auto result = requestResults.constFind(jid); result != requestResults.cend()) {
                watch->state = *result;
            } else if (watch->state != DeviceWatch::State::Loading) {
                // not requested yet, e.g. created while being disconnected
                refreshWatch(jid);
            }
        }
    }
}

// Device list notifications may be missed until the next login.
void OmemoManagerPrivate::disconnected()
{
    for (auto it = watches.cbegin(); it != watches.cend(); ++it) {
        if (auto watch = it->lock(); watch && watch->state != DeviceWatch::State::Loading) {
            watch->state = DeviceWatch::State::Stale;
        }
    }
}

DeviceWatch OmemoManagerPrivate::watch(const QString &jid)
{
    if (auto data = watches.value(jid).lock()) {
        return DeviceWatch(data);
    }

    auto data = std::make_shared<DeviceWatch::Data>();
    data->jid = jid;
    data->release = [manager = QPointer(q), jid] {
        if (manager) {
            manager->d->releaseWatch(jid);
        }
    };
    watches.insert(jid, data);
    startWatch(jid);
    return DeviceWatch(data);
}

QXmppTask<void> OmemoManagerPrivate::startWatch(QString jid)
{
    co_await loaded().withContext(q);
    co_await updateWatch(jid).withContext(q);
    // The watch is not locked while awaiting, so that it can be released meanwhile.
    if (watches.value(jid).expired()) {
        co_return;
    }
    if (!isLoggedIn) {
        watches.value(jid).lock()->state = DeviceWatch::State::Stale;
    } else if (requestResults.value(jid, DeviceWatch::State::Error) == DeviceWatch::State::Loaded) {
        // The device lists have been notified about since they were requested by an earlier watch.
        watches.value(jid).lock()->state = DeviceWatch::State::Loaded;
        co_await buildSessions(jid).withContext(q);
    } else {
        co_await refreshWatch(jid).withContext(q);
    }
}

// Requests the device lists of a watched JID and subscribes to them if needed.
QXmppTask<void> OmemoManagerPrivate::refreshWatch(QString jid)
{
    co_await loaded().withContext(q);
    if (auto watch = watches.value(jid).lock()) {
        watch->state = DeviceWatch::State::Loading;
        requestResults.remove(jid);
    } else {
        co_return;
    }

    const auto isOwnJid = jid == ownJid();
    const auto subscribe = !isOwnJid && !hasPresenceSubscription(jid);
    auto failed = false;
    for (auto version : ALL_VERSIONS) {
        // The own device lists of disabled versions would be republished with this device.
        if (isOwnJid && (!isSetUp || !isEnabled(version))) {
            continue;
        }

        const auto client = pubSubClient(version);
        const OmemoStorage::Subscription subscription { version, jid, ownFullJid() };
        if (subscribe && isEnabled(version) && !subscriptions.contains(subscription)) {
            auto result = co_await client.subscribeToDeviceList(jid, subscription.subscriberJid).withContext(q);
            if (auto *error = std::get_if<QXmppError>(&result)) {
                q->warning(u"Could not subscribe to the device list of %1: %2"_s.arg(jid, error->description));
            } else {
                subscriptions.append(subscription);
                storage->addSubscription(subscription);
            }
        }

        auto list = co_await client.requestDeviceList(jid).withContext(q);
        if (watches.value(jid).expired()) {
            co_return;
        }
        if (auto *entries = std::get_if<QList<DeviceList::Entry>>(&list)) {
            applyReceivedDeviceList(version, jid, *entries);
        } else {
            failed = true;
        }
    }

    if (auto watch = watches.value(jid).lock()) {
        // Requests fail on disconnection, they are sent again after resuming the stream.
        if (isLoggedIn) {
            const auto result = failed ? DeviceWatch::State::Error : DeviceWatch::State::Loaded;
            requestResults.insert(jid, result);
            watch->state = result;
        } else {
            watch->state = DeviceWatch::State::Stale;
        }
    }
    co_await updateWatch(jid).withContext(q);
    co_await buildSessions(jid).withContext(q);
}

// Updates the devices of a watch with the current trust levels.
// Lists a device using both versions once, with OMEMO 2 and its OMEMO 0 key. The device is the
// same by the identity key if the one of the OMEMO 0 device is known, otherwise by the ID.
static QList<Device> mergeVersions(const QList<Device> &devices)
{
    QList<Device> omemo0Devices;
    QList<Device> omemo2Devices;
    for (const auto &device : devices) {
        (device.version == Version::Omemo0 ? omemo0Devices : omemo2Devices).append(device);
    }
    omemo0Devices.removeIf([&omemo2Devices](const Device &omemo0Device) {
        for (auto &device : omemo2Devices) {
            if (device.keyId.isEmpty() || device.jid != omemo0Device.jid) {
                continue;
            }
            if (omemo0Device.keyId.isEmpty() ? device.id == omemo0Device.id : omemo0IdentityKey(device.keyId) == omemo0Device.keyId) {
                device.omemo0KeyId = omemo0Device.keyId;
                return true;
            }
        }
        return false;
    });
    return omemo0Devices + omemo2Devices;
}

QXmppTask<void> OmemoManagerPrivate::updateWatch(QString jid)
{
    if (watches.value(jid).expired()) {
        co_return;
    }

    auto devices = co_await this->devices(jid).withContext(q);
    auto watch = watches.value(jid).lock();
    if (!watch) {
        co_return;
    }
    QList<Version> supported;
    for (const auto &device : std::as_const(devices)) {
        if (!supported.contains(device.version)) {
            supported.append(device.version);
        }
    }
    watch->devices = mergeVersions(devices);
    watch->supportedVersions = supported;
}

void OmemoManagerPrivate::updateWatches()
{
    const auto jids = watches.keys();
    for (const auto &jid : jids) {
        updateWatch(jid);
    }
}

// Builds the missing sessions with the devices of a watched JID.
QXmppTask<void> OmemoManagerPrivate::buildSessions(QString jid)
{
    if (!isSetUp || !isLoggedIn || watches.value(jid).expired()) {
        co_return;
    }
    // Devices added meanwhile are handled by another run.
    if (sessionBuildsRunning.contains(jid)) {
        sessionBuildsPending.insert(jid);
        co_return;
    }
    sessionBuildsRunning.insert(jid);
    do {
        sessionBuildsPending.remove(jid);
        const auto enabled = enabledVersions.value();
        for (auto version : enabled) {
            for (auto id : deviceIds(version, jid)) {
                if (!isSetUp || watches.value(jid).expired()) {
                    break;
                }
                co_await prepareSession(version, jid, id, DEFAULT_ACCEPTED_TRUST_LEVELS).withContext(q);
            }
        }
    } while (sessionBuildsPending.contains(jid));
    sessionBuildsRunning.remove(jid);
}

// The subscriptions are kept until the next login, so the device lists are neither subscribed to
// nor requested again if the JID is watched again in this session, e.g. when a chat is reopened.
void OmemoManagerPrivate::releaseWatch(const QString &jid)
{
    watches.remove(jid);
}

void OmemoManagerPrivate::unsubscribe(const OmemoStorage::Subscription &subscription)
{
    // removed before the request, so it is not used anymore
    subscriptions.removeAll(subscription);
    storage->removeSubscription(subscription);
    pubSubClient(subscription.version).unsubscribeFromDeviceList(subscription.jid, subscription.subscriberJid).then(q, [this, jid = subscription.jid](Result<> &&result) {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            q->warning(u"Could not unsubscribe from the device list of %1: %2"_s.arg(jid, error->description));
        }
    });
}

}  // namespace QXmpp::Omemo::Private

using namespace QXmpp::Omemo;

namespace QXmpp {

/*!
    \class QXmpp::OmemoManager
    \inmodule QXmpp

    \brief The OmemoManager class implements \xep{0384}{OMEMO Encryption} as OMEMO 0
    (\c eu.siacs.conversations.axolotl, version 0.3) and OMEMO 2 (\c urn:xmpp:omemo:2, version 0.8
    and later).

    Both versions use the same device ID and identity key, so a device has one fingerprint. Pre
    keys, sessions and the known devices are separate per version.

    The manager loads its data from the storage when it is added to a client. On the first login,
    it creates the own device and publishes it. On each further login (not after resuming the
    stream), it renews the keys and publishes the data again.

    QXmppPubSubManager and a QXmppTrustManager must be added to the client before this manager.
    New trust data uses the security policy TOAKAFA, see QXmpp::TrustSecurityPolicy. The trust
    manager can be used to change it.

    A stanza is encrypted for at most 500 devices, the other own devices first, so that it stays
    below the stanza size limit of servers. Devices above the limit cannot decrypt it.

    \since QXmpp 1.18
*/

/*!
    \enum QXmpp::OmemoManager::State

    \value Loading The data is being loaded from the storage.
    \value NotSetUp There is no own device yet. It is created on the next login.
    \value SettingUp The own device is being created.
    \value Ready The own device exists and stanzas can be encrypted and decrypted.
*/

/*!
    Constructs a manager storing its data in \a storage and using \a versions.
*/
OmemoManager::OmemoManager(std::shared_ptr<OmemoStorage> storage, QList<Version> versions)
    : d(std::make_unique<OmemoManagerPrivate>(this, std::move(storage), std::move(versions)))
{
    for (auto version : ALL_VERSIONS) {
        hooks().add<QXmpp::PubSub::EventReceived>(deviceListNodeName(version), this, &OmemoManager::handlePubSubEvent);
    }
    hooks().add<Disco::CollectInfo>([this](QList<QString> &features, auto &) {
        for (auto version : d->enabledVersions.value()) {
            features << deviceListNodeName(version) + u"+notify";
        }
    });
    hooks().add<Connection::SessionStarted>([this] {
        d->isLoggedIn = true;
        d->login();
    });
    d->registerProviders();
}

OmemoManager::~OmemoManager() = default;

/*!
    Returns the versions used for publishing and receiving. The bindable is read-only, the
    versions are changed via setEnabledVersions().
*/
QBindable<QList<Version>> OmemoManager::enabledVersions() const
{
    return readOnly(d->enabledVersions);
}

/*!
    Sets the \a versions used for publishing and receiving.

    While being logged in, the own device is published for added versions and removed from the
    device lists of removed versions immediately, otherwise on the next login. The service
    discovery features change with the next presence.

    To keep decrypting messages of a version without encrypting with it anymore, e.g. OMEMO 0,
    leave it enabled and select only the other version via QXmpp::Messages::SelectEncryption.
*/
void OmemoManager::setEnabledVersions(QList<Version> versions)
{
    const auto previous = d->enabledVersions.value();
    if (previous == versions) {
        return;
    }
    d->enabledVersions = versions;
    if (!d->isSetUp || !d->isLoggedIn) {
        return;
    }
    for (auto version : ALL_VERSIONS) {
        if (versions.contains(version) == previous.contains(version)) {
            continue;
        }
        auto task = versions.contains(version) ? d->setUpVersion(version) : d->unpublishOwnDevice(version);
        task.then(this, [this](Result<> &&result) {
            if (auto *error = std::get_if<QXmppError>(&result)) {
                d->reportError(u"Could not update the published OMEMO versions"_s, *error);
            }
        });
    }
}

/*!
    Returns the label of this device that is published in the OMEMO 2 device list.
*/
QString OmemoManager::deviceLabel() const
{
    return d->label();
}

/*!
    Sets the \a label of this device that is published in the OMEMO 2 device list. An empty label
    resets it to defaultDeviceLabel().

    The label is not stored, so a custom label must be set again after each start. It is published
    on the next login or immediately while being logged in.
*/
void OmemoManager::setDeviceLabel(const QString &label)
{
    if (std::exchange(d->customLabel, label) == label) {
        return;
    }
    if (d->isSetUp && d->isLoggedIn && d->isEnabled(Version::Omemo2)) {
        d->publishOwnDeviceList(Version::Omemo2).then(this, [this](Result<> &&result) {
            if (auto *error = std::get_if<QXmppError>(&result)) {
                d->reportError(u"Could not publish the device label"_s, *error);
            }
        });
    }
}

/*!
    Returns the label used for this device if none has been set: the application name and the name
    of the operating system without its version, e.g. "Kaidan (Debian GNU/Linux)". The application
    name is QGuiApplication::applicationDisplayName() if set, otherwise
    QCoreApplication::applicationName(). The host name is not used, because the device list can be
    read by everyone.
*/
QString OmemoManager::defaultDeviceLabel()
{
    QList<QString> osName;
    for (const auto &part : QSysInfo::prettyProductName().split(u' ', Qt::SkipEmptyParts)) {
        if (part.front().isDigit() || part.startsWith(u'(')) {
            break;
        }
        osName.append(part);
    }
    const auto os = osName.join(u' ');
    // QGuiApplication property, read without depending on Qt GUI
    auto application = qApp ? qApp->property("applicationDisplayName").toString() : QString();
    if (application.isEmpty()) {
        application = QCoreApplication::applicationName();
    }
    if (application.isEmpty() || os.isEmpty()) {
        return application.isEmpty() ? os : application;
    }
    return application + u" ("_s + os + u')';
}

/*!
    Returns the state of this device.
*/
QBindable<OmemoManager::State> OmemoManager::state() const
{
    return readOnly(d->state);
}

/*!
    Returns the ID of this device, 0 while there is none.
*/
QBindable<uint32_t> OmemoManager::ownDeviceId() const
{
    return readOnly(d->ownDeviceId);
}

/*!
    Returns the public identity key of this device in the format of \a version, empty while there
    is none.

    Both versions share the same identity key. For OMEMO 2, it is in its Ed25519 form. For OMEMO 0,
    it is in its Curve25519 form with the type prefix (0x05), as used in the bundles and shown as
    fingerprint by other OMEMO 0 clients.

    The format is the same as of Device::keyId.
*/
QBindable<QByteArray> OmemoManager::ownKey(Version version) const
{
    return readOnly(version == Version::Omemo0 ? d->ownKeyOmemo0 : d->ownKey);
}

/*!
    \struct QXmpp::Omemo::Device
    \inmodule QXmpp

    \brief A device of a JID listed in its device list of a version.

    \since QXmpp 1.18

    \var QXmpp::Omemo::Device::version
    OMEMO version of the device list
    \var QXmpp::Omemo::Device::jid
    bare JID of the device's owner
    \var QXmpp::Omemo::Device::id
    device ID
    \var QXmpp::Omemo::Device::label
    label of the device, only available with OMEMO 2
    \var QXmpp::Omemo::Device::keyId
    public identity key in the format of the version (Curve25519 for OMEMO 0, Ed25519 for OMEMO
    2), empty until a session has been built with the device
    \var QXmpp::Omemo::Device::trustLevel
    trust level of the key, \c Undecided while the key is unknown
    \var QXmpp::Omemo::Device::omemo0KeyId
    public identity key of OMEMO 0 of a device listed by DeviceWatch::devices() with OMEMO 2 that
    also uses OMEMO 0, empty otherwise and while the key is unknown, it has the same trust level as
    \c keyId
*/

/*!
    \fn bool QXmpp::Omemo::Device::hasKey(const QByteArray &key) const

    Returns whether \a key is the key of the device in one of the versions (\c keyId or
    \c omemo0KeyId), e.g. the key of the sender of a message.
*/

/*!
    \class QXmpp::Omemo::DeviceWatch
    \inmodule QXmpp

    \brief Handle to the devices of a JID, returned by OmemoManager::watchDevices().

    Cheap to copy: all copies share the same state. While a copy exists, the devices are kept up to
    date, see OmemoManager::watchDevices().

    \since QXmpp 1.18
*/

/*!
    \enum QXmpp::Omemo::DeviceWatch::State

    \value Loading The devices are being loaded from the storage or the device lists are being
    requested.
    \value Stale The known devices are available, but the device lists have not been requested in
    the current session or notifications may have been missed while being disconnected. They are
    requested on the next login.
    \value Loaded The device lists have been requested and are kept up to date.
    \value Error A device list could not be requested. The known devices are still available.
*/

/*!
    Constructs a watch without a JID.
*/
DeviceWatch::DeviceWatch()
    : d(std::make_shared<Data>())
{
}

DeviceWatch::DeviceWatch(std::shared_ptr<Data> d)
    : d(std::move(d))
{
}

/*!
    Returns the bare JID whose devices are watched.
*/
Jid DeviceWatch::jid() const
{
    return jidOrNull(d->jid);
}

/*!
    Returns whether the device lists have been requested.
*/
QBindable<DeviceWatch::State> DeviceWatch::state() const
{
    return readOnly(d->state);
}

/*!
    Returns the devices listed in the device lists of all versions, the devices only using OMEMO 0
    first, each sorted by ID. A device using both versions is listed once with OMEMO 2 and its key
    of OMEMO 0 (Device::omemo0KeyId) as soon as its key of OMEMO 2 is known. For the own JID, this
    device is not included.
*/
QBindable<QList<Device>> DeviceWatch::devices() const
{
    return readOnly(d->devices);
}

/*!
    Returns the versions with listed devices, also versions that are not enabled. It can be used to
    choose the version for a chat.
*/
QBindable<QList<Version>> DeviceWatch::supportedVersions() const
{
    return readOnly(d->supportedVersions);
}

/*!
    Returns a watch on the devices of \a jid listed in the device lists of all versions with their
    keys and trust levels, sorted by version and ID. This device is not included.

    The known devices are available without network access, also while being disconnected. While a
    copy of the watch exists, the device lists of all versions are requested once per login.
    Without presence subscription, i.e. without notifications about changed device lists, the
    device lists of the enabled versions are subscribed to explicitly. The subscriptions are kept
    until the next login, so the device lists are not requested again if \a jid is watched again
    in the same session. Sessions with the devices with accepted trust levels are built in advance,
    so the first message can be encrypted faster and the keys of the devices become known.

    The explicit subscriptions use the full JID of this client. The service of \a jid, and
    possibly \a jid itself, can therefore see the resource and that its devices have been watched,
    e.g. that its chat has been opened.

    A chat should keep a watch while it is open. A watch can also be used to show the keys of a
    contact, e.g. to authenticate them.
*/
DeviceWatch OmemoManager::watchDevices(const Jid &jid)
{
    return d->watch(jid.bare().toString());
}

/*!
    Sets the trust level of the keys of \a devices to \a trustLevel, e.g. after the user has
    verified them. Devices with unknown keys are skipped.
*/
QXmppTask<void> OmemoManager::setTrustLevel(QList<Device> devices, QXmpp::TrustLevel trustLevel)
{
    for (auto version : ALL_VERSIONS) {
        QMultiHash<QString, QByteArray> keys;
        for (const auto &device : std::as_const(devices)) {
            if (device.version == version && !device.keyId.isEmpty()) {
                keys.insert(device.jid, device.keyId);
            }
        }
        if (!keys.isEmpty()) {
            co_await d->trustManager->setTrustLevel(OmemoManagerPrivate::trustNamespace(version), keys, trustLevel).withContext(this);
        }
    }
}

/*!
    Removes the devices of \a jid, their sessions and the trust levels of their keys. This should be
    called after removing a contact. The own JID must not be passed, see resetAll().

    If the presence subscription of the contact persists, its devices are added again on the next
    device list notification.
*/
QXmppTask<void> OmemoManager::removeContactDevices(Jid contactJid)
{
    const auto jid = contactJid.bare().toString();
    if (jid == d->ownJid()) {
        warning(u"The own devices cannot be removed as a contact's devices"_s);
        co_return;
    }

    for (auto version : ALL_VERSIONS) {
        auto &state = d->versions.at(version);
        state.devices.remove(jid);
        state.keys.sessions.removeIf([&](decltype(state.keys.sessions)::iterator it) { return it.key().jid == jid; });
        state.pendingSessionBuilds.removeIf([&](const Address &address) { return address.jid == jid; });
        state.refreshedDeviceLists.remove(jid);
        state.receivedDeviceLists.remove(jid);
    }
    // requested again if watched again
    d->requestResults.remove(jid);

    co_await d->storage->removeDevices(jid).withContext(this);
    for (auto version : ALL_VERSIONS) {
        co_await d->trustManager->removeKeys(OmemoManagerPrivate::trustNamespace(version), jid).withContext(this);
    }
    d->updateWatch(jid);
}

/*!
    Removes this device from the own device lists and its bundles from the server, and resets its
    local data like resetOwnDeviceLocally(). The data is only reset if the server data has been
    removed. The user must be logged in.

    Call this when the account is removed from this device. A new device is created on the next
    login.
*/
QXmppTask<Result<>> OmemoManager::resetOwnDevice()
{
    co_await d->loaded().withContext(this);
    if (!d->isSetUp) {
        co_return Success();
    }

    // The own device list notifications would add this device again.
    d->isSetUp = false;
    const auto ownId = d->device.deviceId;
    const auto versions = d->enabledVersions.value();
    for (auto version : versions) {
        auto result = co_await d->unpublishOwnDevice(version).withContext(this);
        if (!std::holds_alternative<QXmppError>(result)) {
            // also if this device was not listed
            result = co_await d->pubSubClient(version).deleteBundle(ownId).withContext(this);
        }
        if (auto *error = std::get_if<QXmppError>(&result)) {
            d->isSetUp = true;
            co_return std::move(*error);
        }
    }

    co_await resetOwnDeviceLocally();
    co_return Success();
}

/*!
    Resets the OMEMO data of this device and the trust data of OMEMO without changing the data on
    the server. Use this after the account has been removed from the server.
*/
QXmppTask<void> OmemoManager::resetOwnDeviceLocally()
{
    for (auto version : ALL_VERSIONS) {
        auto &state = d->versions.at(version);
        state.keys = {};
        state.devices.clear();
        state.isPublishingBundle = false;
        state.isBundleOutdated = false;
        state.refreshedDeviceLists.clear();
        state.receivedDeviceLists.clear();
        state.pendingSessionBuilds.clear();
    }
    d->requestResults.clear();
    d->isSetUp = false;
    d->device = {};
    d->ownDeviceId = 0;
    d->ownKey = QByteArray();
    d->state = State::NotSetUp;

    co_await d->storage->resetAll().withContext(this);
    for (auto version : ALL_VERSIONS) {
        co_await d->trustManager->resetAll(OmemoManagerPrivate::trustNamespace(version)).withContext(this);
    }
    d->updateWatches();
}

/*!
    Deletes the device lists and bundles of all own devices of the enabled versions from the server
    and resets the data of this device like resetOwnDeviceLocally(). The other own devices publish
    their data again when they are notified about it or on their next login. The user must be
    logged in.

    Use this only if the OMEMO setup of the account cannot be recovered otherwise.
*/
QXmppTask<Result<>> OmemoManager::resetAll()
{
    co_await d->loaded().withContext(this);
    const auto wasSetUp = std::exchange(d->isSetUp, false);
    const auto versions = d->enabledVersions.value();
    for (auto version : versions) {
        const auto client = d->pubSubClient(version);
        auto list = co_await client.requestDeviceList(d->ownJid()).withContext(this);
        if (auto *error = std::get_if<QXmppError>(&list)) {
            d->isSetUp = wasSetUp;
            co_return std::move(*error);
        }

        QList<uint32_t> ids;
        if (d->device.deviceId) {
            ids.append(d->device.deviceId);
        }
        ids += d->versions.at(version).devices.value(d->ownJid()).keys();
        for (const auto &entry : std::get<QList<DeviceList::Entry>>(list)) {
            ids.append(entry.id);
        }
        std::ranges::sort(ids);
        ids.erase(std::ranges::unique(ids).begin(), ids.end());

        auto result = co_await client.deleteAll(ids).withContext(this);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            d->isSetUp = wasSetUp;
            co_return std::move(*error);
        }
    }

    co_await resetOwnDeviceLocally();
    co_return Success();
}

void OmemoManager::onRegistered(QXmppClient *client)
{
    d->pubSub = client->findExtension<QXmppPubSubManager>();
    if (!d->pubSub) {
        qFatal("QXmppPubSubManager is not available, it must be added to the client before QXmpp::OmemoManager");
    }
    d->trustManager = client->findExtension<QXmppTrustManager>();
    if (!d->trustManager) {
        qFatal("QXmppTrustManager is not available, it must be added to the client before QXmpp::OmemoManager");
    }

    d->roster = client->findExtension<QXmppRosterManager>();

    connect(d->trustManager, &QXmppTrustManager::trustLevelsChanged, this, [this](const QHash<QString, QMultiHash<QString, QByteArray>> &modifiedKeys) {
        d->mirrorTrustLevels(modifiedKeys);
        QSet<QString> jids;
        for (auto version : ALL_VERSIONS) {
            if (auto it = modifiedKeys.find(OmemoManagerPrivate::trustNamespace(version)); it != modifiedKeys.end()) {
                for (auto keyIt = it->keyBegin(); keyIt != it->keyEnd(); ++keyIt) {
                    jids.insert(*keyIt);
                }
            }
        }
        for (const auto &jid : std::as_const(jids)) {
            d->updateWatch(jid);
        }
    });

    // A resumed stream continues the session, SessionStarted is only run for new sessions.
    connect(client, &QXmppClient::connected, this, [this] {
        if (!std::exchange(d->isLoggedIn, true)) {
            d->resume();
        }
    });
    connect(client, &QXmppClient::disconnected, this, [this] {
        d->isLoggedIn = false;
        d->disconnected();
    });

    if (!std::exchange(d->isLoadStarted, true)) {
        d->load();
    }
}

void OmemoManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
    disconnect(d->trustManager, nullptr, this, nullptr);
}

HookResult OmemoManager::handlePubSubEvent(const QDomElement &element, const Jid &service, const QString &node)
{
    const auto version = node == ns_omemo_devicelist ? Version::Omemo0 : Version::Omemo2;
    const auto jid = service.bare().toString();
    // The own device list is only handled with an own device.
    if (!d->isEnabled(version) || (jid == d->ownJid() && !d->isSetUp)) {
        return HookResult::Stop;
    }

    QXmppPubSubEvent<Pep::DeviceListItem> event;
    event.parse(element);

    switch (event.eventType()) {
    case QXmppPubSubEventBase::Delete:
    case QXmppPubSubEventBase::Purge:
    case QXmppPubSubEventBase::Retract:
        d->clearDeviceList(version, jid);
        break;
    case QXmppPubSubEventBase::Items:
        if (const auto items = event.items(); !items.isEmpty()) {
            if (const auto &item = Pep::currentItem(items); item.version == version) {
                d->applyReceivedDeviceList(version, jid, item.devices);
            }
        }
        break;
    default:
        break;
    }
    return HookResult::Stop;
}

}  // namespace QXmpp

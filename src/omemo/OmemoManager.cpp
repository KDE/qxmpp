// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubManager.h"
#include "QXmppTrustManager.h"
#include "QXmppUtils.h"

#include "Async.h"
#include "OmemoManager_p.h"
#include "StringLiterals.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QScopeGuard>
#include <QSysInfo>

using namespace QXmpp::Private;
using namespace QXmpp::Omemo::Private;

namespace QXmpp::Omemo {

static constexpr std::array ALL_VERSIONS { Version::Omemo0, Version::Omemo2 };

static QStringView deviceListNodeName(Version version)
{
    return version == Version::Omemo0 ? ns_omemo_devicelist : ns_omemo_2_devices;
}

ManagerPrivate::ManagerPrivate(Manager *q, std::shared_ptr<Storage> storage, QList<Version> enabledVersions)
    : q(q), storage(std::move(storage)), enabledVersions(std::move(enabledVersions))
{
}

QString ManagerPrivate::ownJid() const
{
    return q->client()->configuration().jidBare();
}

void ManagerPrivate::setUpSessionLayers()
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
QXmppTask<void> ManagerPrivate::setOwnTrustKey(Version version, QByteArray key)
{
    const auto encryption = trustNamespace(version);
    if ((co_await trustManager->ownKey(encryption).withContext(q)).isEmpty()) {
        co_await trustManager->setSecurityPolicy(encryption, Toakafa).withContext(q);
    }
    co_await trustManager->setOwnKey(encryption, key).withContext(q);
}

Storage::OwnDevice ManagerPrivate::storedOwnDevice() const
{
    Storage::OwnDevice stored {
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

QString ManagerPrivate::label() const
{
    return customLabel.isEmpty() ? Manager::defaultDeviceLabel() : customLabel;
}

void ManagerPrivate::reportError(const QString &context, const QXmppError &error)
{
    q->warning(context + u": "_s + error.description);
    Q_EMIT q->errorOccurred(error);
}

// Resolves when the data has been loaded from the storage.
QXmppTask<void> ManagerPrivate::loaded()
{
    if (state.value() != Manager::State::Loading) {
        return makeReadyTask();
    }
    QXmppPromise<void> promise;
    auto task = promise.task();
    loadPromises.append(std::move(promise));
    return task;
}

QXmppTask<void> ManagerPrivate::load()
{
    auto data = co_await storage->allData().withContext(q);

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
            const auto latest = std::ranges::find(ownDevice.latestPreKeyIds, version, &Storage::LatestPreKeyId::version);
            state.keys.latestPreKeyId = latest == ownDevice.latestPreKeyIds.cend() ? 0 : latest->id;
        }
        for (const auto &key : std::as_const(data.preKeys)) {
            versions.at(key.version).keys.preKeys.insert(key.id, SecureByteArray(key.data));
        }
        setOwnDevice(std::move(keys));
        removeExpiredDevices();
    } else {
        state = Manager::State::NotSetUp;
    }

    for (auto &promise : loadPromises) {
        promise.finish();
    }
    loadPromises.clear();
}

void ManagerPrivate::setOwnDevice(DeviceKeys &&keys)
{
    device = std::move(keys);
    setUpSessionLayers();
    isSetUp = true;
    ownDeviceId = device.deviceId;
    ownKey = device.identity.publicKey;
    state = Manager::State::Ready;
}

QXmppTask<Result<>> ManagerPrivate::createOwnDevice()
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
QXmppTask<void> ManagerPrivate::setUp()
{
    co_await loaded().withContext(q);
    if (isSettingUp) {
        co_return;
    }
    isSettingUp = true;
    const auto finish = qScopeGuard([this] { isSettingUp = false; });

    if (!isSetUp) {
        state = Manager::State::SettingUp;
        auto result = co_await createOwnDevice().withContext(q);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            state = Manager::State::NotSetUp;
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
QXmppTask<Result<>> ManagerPrivate::setUpVersion(Version version)
{
    auto &keys = versions.at(version).keys;
    auto added = KeyMaintenance::refillPreKeys(context, keys, keyConfig);
    if (auto *error = std::get_if<QXmppError>(&added)) {
        return makeReadyTask<Result<>>(std::move(*error));
    }
    QList<Storage::PreKey> records;
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
QXmppTask<Result<>> ManagerPrivate::unpublishOwnDevice(Version version)
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
QXmppTask<Result<uint32_t>> ManagerPrivate::generateDeviceId()
{
    QSet<uint32_t> usedIds;
    for (auto version : std::as_const(enabledVersions)) {
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

QXmppTask<Result<>> ManagerPrivate::publishOwnData(Version version)
{
    auto result = co_await publishBundle(version).withContext(q);
    if (std::holds_alternative<QXmppError>(result)) {
        co_return result;
    }
    co_return co_await publishOwnDeviceList(version).withContext(q);
}

QXmppTask<Result<>> ManagerPrivate::publishBundle(Version version)
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
QXmppTask<Result<>> ManagerPrivate::publishOwnDeviceList(Version version)
{
    auto result = co_await pubSubClient(version).requestDeviceList(ownJid()).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }

    co_return co_await applyDeviceList(version, ownJid(), std::get<QList<DeviceList::Entry>>(result)).withContext(q);
}

static QHash<uint32_t, DeviceList::DeviceInfo> deviceInfos(const QHash<uint32_t, Storage::Device> &devices)
{
    QHash<uint32_t, DeviceList::DeviceInfo> infos;
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        infos.insert(it.key(), { it->label, it->removalFromDeviceListDate });
    }
    return infos;
}

// Returns the publication of the own device list if it must be republished.
QXmppTask<Result<>> ManagerPrivate::applyDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list)
{
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

void ManagerPrivate::applyReceivedDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list)
{
    applyDeviceList(version, jid, list).then(q, [this](Result<> &&result) {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            q->warning(u"Could not publish the own device list: "_s + error->description);
        }
    });
}

void ManagerPrivate::clearDeviceList(Version version, const QString &jid)
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

void ManagerPrivate::storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, DeviceList::DeviceInfo> &infos, const DeviceList::Changes &changes)
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
    Q_EMIT q->devicesChanged(version, jid);
}

void ManagerPrivate::removeExpiredDevices()
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
                Q_EMIT q->devicesChanged(version, jidIt.key());
            }
            jidIt = jidIt->isEmpty() ? state.devices.erase(jidIt) : std::next(jidIt);
        }
    }
}

/*!
    \class QXmpp::Omemo::Manager
    \inmodule QXmpp

    \brief The Manager class implements \xep{0384}{OMEMO Encryption} in version 0.3
    (\c eu.siacs.conversations.axolotl) and version 0.8 and later (\c urn:xmpp:omemo:2).

    Both versions use the same device ID and identity key, so a device has one fingerprint. Pre
    keys, sessions and the known devices are separate per version.

    The manager loads its data from the storage when it is added to a client. On the first login,
    it creates the own device and publishes it. On each further login (not after resuming the
    stream), it renews the keys and publishes the data again.

    QXmppPubSubManager and a QXmppTrustManager must be added to the client before this manager.
    New trust data uses the security policy TOAKAFA, see QXmpp::TrustSecurityPolicy. The trust
    manager can be used to change it.

    \since QXmpp 1.18
*/

/*!
    \enum QXmpp::Omemo::Manager::State

    \value Loading The data is being loaded from the storage.
    \value NotSetUp There is no own device yet. It is created on the next login.
    \value SettingUp The own device is being created.
    \value Ready The own device exists and stanzas can be encrypted and decrypted.
*/

/*!
    Constructs a manager storing its data in \a storage and using \a versions.
*/
Manager::Manager(std::shared_ptr<Storage> storage, QList<Version> versions)
    : d(std::make_unique<ManagerPrivate>(this, std::move(storage), std::move(versions)))
{
    for (auto version : ALL_VERSIONS) {
        hooks().add<QXmpp::PubSub::EventReceived>(deviceListNodeName(version), this, &Manager::handlePubSubEvent);
    }
    hooks().add<Disco::CollectInfo>([this](QList<QString> &features, auto &) {
        for (auto version : std::as_const(d->enabledVersions)) {
            features << deviceListNodeName(version) + u"+notify";
        }
    });
    d->registerProviders();
}

Manager::~Manager() = default;

/*!
    Returns the versions used for publishing and receiving.
*/
QList<Version> Manager::enabledVersions() const
{
    return d->enabledVersions;
}

/*!
    Sets the \a versions used for publishing and receiving.

    While being logged in, the own device is published for added versions and removed from the
    device lists of removed versions immediately, otherwise on the next login. The service
    discovery features change with the next presence.
*/
void Manager::setEnabledVersions(QList<Version> versions)
{
    const auto previous = std::exchange(d->enabledVersions, versions);
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
QString Manager::deviceLabel() const
{
    return d->label();
}

/*!
    Sets the \a label of this device that is published in the OMEMO 2 device list. An empty label
    resets it to defaultDeviceLabel().

    The label is not stored, so a custom label must be set again after each start. It is published
    on the next login or immediately while being logged in.
*/
void Manager::setDeviceLabel(const QString &label)
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
QString Manager::defaultDeviceLabel()
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
QBindable<Manager::State> Manager::state() const
{
    return &d->state;
}

/*!
    Returns the ID of this device, 0 while there is none.
*/
QBindable<uint32_t> Manager::ownDeviceId() const
{
    return &d->ownDeviceId;
}

/*!
    Returns the public identity key of this device in its Ed25519 form, empty while there is none.
*/
QBindable<QByteArray> Manager::ownKey() const
{
    return &d->ownKey;
}

/*!
    Returns the IDs of the devices of \a jid listed in its device list of \a version, sorted. For
    the own JID, this device is not included.
*/
QList<uint32_t> Manager::deviceIds(Version version, const QString &jid) const
{
    QList<uint32_t> ids;
    const auto devices = d->versions.at(version).devices.value(jid);
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        if (!it->removalFromDeviceListDate.isValid()) {
            ids.append(it.key());
        }
    }
    std::ranges::sort(ids);
    return ids;
}

/*!
    \struct QXmpp::Omemo::Manager::Device
    \inmodule QXmpp

    \brief A device of a JID listed in its device list of a version.

    \var QXmpp::Omemo::Manager::Device::version
    OMEMO version of the device list
    \var QXmpp::Omemo::Manager::Device::jid
    bare JID of the device's owner
    \var QXmpp::Omemo::Manager::Device::id
    device ID
    \var QXmpp::Omemo::Manager::Device::label
    label of the device, only available with OMEMO 2
    \var QXmpp::Omemo::Manager::Device::keyId
    public identity key in the format of the version (Curve25519 for OMEMO 0, Ed25519 for OMEMO
    2), empty until a message has been exchanged with the device
    \var QXmpp::Omemo::Manager::Device::trustLevel
    trust level of the key, \c Undecided while the key is unknown
*/

/*!
    \struct QXmpp::Omemo::Manager::Key
    \inmodule QXmpp

    \brief A public identity key of a JID.

    \var QXmpp::Omemo::Manager::Key::jid
    bare JID of the key's owner
    \var QXmpp::Omemo::Manager::Key::keyId
    public identity key in the format of the version
*/

/*!
    \fn void QXmpp::Omemo::Manager::trustLevelsChanged(QXmpp::Omemo::Version version, const QList<QXmpp::Omemo::Manager::Key> &modifiedKeys)

    Emitted when the trust levels of \a modifiedKeys of \a version have changed.
*/

/*!
    Returns the devices of \a jids listed in their device lists of the enabled versions. For the
    own JID, this device is not included.

    The devices of contacts are known after a device list notification, which requires a presence
    subscription, or after calling supportedVersions().
*/
QXmppTask<QList<Manager::Device>> Manager::devices(QList<QString> jids)
{
    QList<Device> result;
    const auto versions = d->enabledVersions;
    for (auto version : versions) {
        const auto keys = co_await d->trustManager->keys(ManagerPrivate::trustNamespace(version), jids).withContext(this);
        for (const auto &jid : std::as_const(jids)) {
            const auto stored = d->versions.at(version).devices.value(jid);
            for (auto id : deviceIds(version, jid)) {
                const auto &device = stored[id];
                auto trustLevel = TrustLevel::Undecided;
                if (!device.keyId.isEmpty()) {
                    trustLevel = keys.value(jid).value(device.keyId, TrustLevel::Undecided);
                }
                result.append({ version, jid, id, device.label, device.keyId, trustLevel });
            }
        }
    }
    co_return result;
}

/*!
    Sets the trust level of the keys of \a devices to \a trustLevel, e.g. after the user has
    verified them. Devices with unknown keys are skipped.
*/
QXmppTask<void> Manager::setTrustLevel(QList<Device> devices, QXmpp::TrustLevel trustLevel)
{
    for (auto version : ALL_VERSIONS) {
        QMultiHash<QString, QByteArray> keys;
        for (const auto &device : std::as_const(devices)) {
            if (device.version == version && !device.keyId.isEmpty()) {
                keys.insert(device.jid, device.keyId);
            }
        }
        if (!keys.isEmpty()) {
            co_await d->trustManager->setTrustLevel(ManagerPrivate::trustNamespace(version), keys, trustLevel).withContext(this);
        }
    }
}

/*!
    Removes the devices of \a jid, their sessions and the trust levels of their keys. This should be
    called after removing a contact. The own JID must not be passed, see resetAll().

    If the presence subscription of the contact persists, its devices are added again on the next
    device list notification.
*/
QXmppTask<void> Manager::removeContactDevices(QString jid)
{
    if (jid == d->ownJid()) {
        warning(u"The own devices cannot be removed as a contact's devices"_s);
        co_return;
    }

    QList<Version> changedVersions;
    for (auto version : ALL_VERSIONS) {
        auto &state = d->versions.at(version);
        if (state.devices.remove(jid)) {
            changedVersions.append(version);
        }
        state.keys.sessions.removeIf([&](decltype(state.keys.sessions)::iterator it) { return it.key().jid == jid; });
        state.pendingSessionBuilds.removeIf([&](const Address &address) { return address.jid == jid; });
        state.refreshedDeviceLists.remove(jid);
    }

    co_await d->storage->removeDevices(jid).withContext(this);
    for (auto version : ALL_VERSIONS) {
        co_await d->trustManager->removeKeys(ManagerPrivate::trustNamespace(version), jid).withContext(this);
    }
    for (auto version : std::as_const(changedVersions)) {
        Q_EMIT devicesChanged(version, jid);
    }
}

/*!
    Removes this device from the own device lists and its bundles from the server, and resets its
    local data like resetOwnDeviceLocally(). The data is only reset if the server data has been
    removed. The user must be logged in.

    Call this when the account is removed from this device. A new device is created on the next
    login.
*/
QXmppTask<Result<>> Manager::resetOwnDevice()
{
    co_await d->loaded().withContext(this);
    if (!d->isSetUp) {
        co_return Success();
    }

    // The own device list notifications would add this device again.
    d->isSetUp = false;
    const auto ownId = d->device.deviceId;
    const auto versions = d->enabledVersions;
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

    devicesChanged() is emitted for all JIDs with known devices.
*/
QXmppTask<void> Manager::resetOwnDeviceLocally()
{
    QList<std::pair<Version, QString>> changed;
    for (auto version : ALL_VERSIONS) {
        auto &state = d->versions.at(version);
        for (auto it = state.devices.cbegin(); it != state.devices.cend(); ++it) {
            changed.append({ version, it.key() });
        }
        state.keys = {};
        state.devices.clear();
        state.isPublishingBundle = false;
        state.isBundleOutdated = false;
        state.refreshedDeviceLists.clear();
        state.pendingSessionBuilds.clear();
    }
    d->isSetUp = false;
    d->device = {};
    d->ownDeviceId = 0;
    d->ownKey = QByteArray();
    d->state = State::NotSetUp;

    co_await d->storage->resetAll().withContext(this);
    for (auto version : ALL_VERSIONS) {
        co_await d->trustManager->resetAll(ManagerPrivate::trustNamespace(version)).withContext(this);
    }
    for (const auto &[version, jid] : std::as_const(changed)) {
        Q_EMIT devicesChanged(version, jid);
    }
}

/*!
    Deletes the device lists and bundles of all own devices of the enabled versions from the server
    and resets the data of this device like resetOwnDeviceLocally(). The other own devices publish
    their data again when they are notified about it or on their next login. The user must be
    logged in.

    Use this only if the OMEMO setup of the account cannot be recovered otherwise.
*/
QXmppTask<Result<>> Manager::resetAll()
{
    co_await d->loaded().withContext(this);
    d->isSetUp = false;
    const auto versions = d->enabledVersions;
    for (auto version : versions) {
        const auto client = d->pubSubClient(version);
        auto list = co_await client.requestDeviceList(d->ownJid()).withContext(this);
        if (auto *error = std::get_if<QXmppError>(&list)) {
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
            co_return std::move(*error);
        }
    }

    co_await resetOwnDeviceLocally();
    co_return Success();
}

/*!
    Requests the device lists of \a jid and returns the enabled versions it has devices for.

    The application can use it to choose the version for a chat.
*/
QXmppTask<Result<QList<Version>>> Manager::supportedVersions(QString jid)
{
    QList<Version> supported;
    for (auto version : std::as_const(d->enabledVersions)) {
        auto result = co_await d->pubSubClient(version).requestDeviceList(jid).withContext(this);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            co_return std::move(*error);
        }
        if (jid != d->ownJid()) {
            d->applyReceivedDeviceList(version, jid, std::get<QList<DeviceList::Entry>>(result));
        }
        if (!deviceIds(version, jid).isEmpty()) {
            supported.append(version);
        }
    }
    co_return supported;
}

void Manager::onRegistered(QXmppClient *client)
{
    d->pubSub = client->findExtension<QXmppPubSubManager>();
    if (!d->pubSub) {
        qFatal("QXmppPubSubManager is not available, it must be added to the client before QXmpp::Omemo::Manager");
    }
    d->trustManager = client->findExtension<QXmppTrustManager>();
    if (!d->trustManager) {
        qFatal("QXmppTrustManager is not available, it must be added to the client before QXmpp::Omemo::Manager");
    }

    connect(d->trustManager, &QXmppTrustManager::trustLevelsChanged, this, [this](const QHash<QString, QMultiHash<QString, QByteArray>> &modifiedKeys) {
        for (auto version : ALL_VERSIONS) {
            if (auto it = modifiedKeys.find(ManagerPrivate::trustNamespace(version)); it != modifiedKeys.end()) {
                QList<Key> keys;
                for (auto keyIt = it->cbegin(); keyIt != it->cend(); ++keyIt) {
                    keys.append({ keyIt.key(), keyIt.value() });
                }
                Q_EMIT trustLevelsChanged(version, keys);
            }
        }
    });

    connect(client, &QXmppClient::connected, this, [this, client] {
        d->isLoggedIn = true;
        if (client->streamManagementState() != QXmppClient::ResumedStream) {
            d->setUp();
        }
    });
    connect(client, &QXmppClient::disconnected, this, [this] {
        d->isLoggedIn = false;
    });

    if (!std::exchange(d->isLoadStarted, true)) {
        d->load();
    }
}

void Manager::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
    disconnect(d->trustManager, nullptr, this, nullptr);
}

HookResult Manager::handlePubSubEvent(const QDomElement &element, const QString &service, const QString &node)
{
    const auto version = node == ns_omemo_devicelist ? Version::Omemo0 : Version::Omemo2;
    if (!d->isSetUp || !d->isEnabled(version)) {
        return HookResult::Stop;
    }

    const auto jid = QXmppUtils::jidToBareJid(service);
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

}  // namespace QXmpp::Omemo

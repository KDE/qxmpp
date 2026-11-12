// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppOmemoMemoryStorage.h"

#include "Async.h"

#include <map>

using namespace QXmpp::Private;

/*!
    \class QXmppOmemoMemoryStorage
    \inmodule QXmpp

    \brief The QXmppOmemoMemoryStorage class stores data used by
    \xep{0384}{OMEMO Encryption} in the memory.

    \since QXmpp 1.5
*/

class QXmppOmemoMemoryStoragePrivate
{
public:
    bool isSetUp = false;

    std::optional<QXmppOmemoStorage::OwnDevice> ownDevice;

    // IDs of pre key pairs mapped to pre key pairs
    QHash<uint32_t, QByteArray> preKeyPairs;

    // IDs of signed pre key pairs mapped to signed pre key pairs
    QHash<uint32_t, QXmppOmemoStorage::SignedPreKeyPair> signedPreKeyPairs;

    // recipient JID mapped to device ID mapped to device
    QHash<QString, QHash<uint32_t, QXmppOmemoStorage::Device>> devices;
};

/*! Constructs an OMEMO memory storage. */
QXmppOmemoMemoryStorage::QXmppOmemoMemoryStorage()
    : d(new QXmppOmemoMemoryStoragePrivate)
{
}

QXmppOmemoMemoryStorage::~QXmppOmemoMemoryStorage() = default;

QXmppTask<QXmppOmemoStorage::OmemoData> QXmppOmemoMemoryStorage::allData()
{
    return makeReadyTask(std::move(OmemoData { d->ownDevice,
                                               d->signedPreKeyPairs,
                                               d->preKeyPairs,
                                               d->devices }));
}

QXmppTask<void> QXmppOmemoMemoryStorage::setOwnDevice(const std::optional<OwnDevice> &device)
{
    d->ownDevice = device;
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::addSignedPreKeyPair(const uint32_t keyId, const SignedPreKeyPair &keyPair)
{
    d->signedPreKeyPairs.insert(keyId, keyPair);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::removeSignedPreKeyPair(const uint32_t keyId)
{
    d->signedPreKeyPairs.remove(keyId);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::addPreKeyPairs(const QHash<uint32_t, QByteArray> &keyPairs)
{
    d->preKeyPairs.insert(keyPairs);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::removePreKeyPair(const uint32_t keyId)
{
    d->preKeyPairs.remove(keyId);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::addDevice(const QString &jid, const uint32_t deviceId, const QXmppOmemoStorage::Device &device)
{
    d->devices[jid].insert(deviceId, device);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::removeDevice(const QString &jid, const uint32_t deviceId)
{
    auto &devices = d->devices[jid];
    devices.remove(deviceId);

    // Remove the container for the passed JID if the container stores no
    // devices anymore.
    if (devices.isEmpty()) {
        d->devices.remove(jid);
    }

    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::removeDevices(const QString &jid)
{
    d->devices.remove(jid);
    co_return;
}

QXmppTask<void> QXmppOmemoMemoryStorage::resetAll()
{
    d.reset(new QXmppOmemoMemoryStoragePrivate());
    co_return;
}

namespace QXmpp::Omemo {

/*!
    \class QXmpp::Omemo::MemoryStorage
    \inmodule QXmpp

    \brief The MemoryStorage class stores the data of the \xep{0384}{OMEMO Encryption} manager in
    the memory.

    \since QXmpp 1.18
*/

struct MemoryStoragePrivate {
    struct DeviceKey {
        Version version;
        QString jid;
        uint32_t id;

        auto operator<=>(const DeviceKey &) const = default;
    };

    std::optional<Storage::OwnDevice> ownDevice;
    QMap<uint32_t, Storage::SignedPreKey> signedPreKeys;
    QMap<std::pair<Version, uint32_t>, QByteArray> preKeys;
    std::map<DeviceKey, Storage::Device> devices;
    QList<Storage::Subscription> subscriptions;
};

MemoryStorage::MemoryStorage()
    : d(std::make_unique<MemoryStoragePrivate>())
{
}

MemoryStorage::~MemoryStorage() = default;

QXmppTask<Storage::Data> MemoryStorage::allData()
{
    Data data;
    data.ownDevice = d->ownDevice;
    data.signedPreKeys = d->signedPreKeys.values();
    for (auto it = d->preKeys.cbegin(); it != d->preKeys.cend(); ++it) {
        data.preKeys.append({ it.key().first, it.key().second, it.value() });
    }
    for (const auto &[key, device] : d->devices) {
        data.devices.append(device);
    }
    data.subscriptions = d->subscriptions;
    return makeReadyTask(std::move(data));
}

QXmppTask<void> MemoryStorage::setOwnDevice(const std::optional<OwnDevice> &device)
{
    d->ownDevice = device;
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::addSignedPreKey(const SignedPreKey &key)
{
    d->signedPreKeys.insert(key.id, key);
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::removeSignedPreKey(uint32_t id)
{
    d->signedPreKeys.remove(id);
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::addPreKeys(const QList<PreKey> &keys)
{
    for (const auto &key : keys) {
        d->preKeys.insert({ key.version, key.id }, key.data);
    }
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::removePreKey(Version version, uint32_t id)
{
    d->preKeys.remove({ version, id });
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::addDevice(const Device &device)
{
    d->devices.insert_or_assign({ device.version, device.jid, device.id }, device);
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::removeDevice(Version version, const QString &jid, uint32_t id)
{
    d->devices.erase({ version, jid, id });
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::removeDevices(const QString &jid)
{
    std::erase_if(d->devices, [&](const auto &entry) { return entry.first.jid == jid; });
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::addSubscription(const Subscription &subscription)
{
    if (!d->subscriptions.contains(subscription)) {
        d->subscriptions.append(subscription);
    }
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::removeSubscription(const Subscription &subscription)
{
    d->subscriptions.removeAll(subscription);
    return makeReadyTask();
}

QXmppTask<void> MemoryStorage::resetAll()
{
    *d = {};
    return makeReadyTask();
}

}  // namespace QXmpp::Omemo

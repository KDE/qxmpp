// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppOmemoStorage.h"

#include <algorithm>
#include <tuple>

/*!
    \class QXmppOmemoStorage
    \inmodule QXmpp

    \brief The QXmppOmemoStorage class stores data used by
    \xep{0384}{OMEMO Encryption}.

    \since QXmpp 1.5

    It is used by the deprecated QXmppOmemoManager. QXmpp::Omemo::Storage::migrateFrom() copies its
    data into the storage of QXmpp::Omemo::Manager.
*/

/*!
    \fn QXmppOmemoStorage::allData()

    Returns all OMEMO data.
*/

/*!
    \fn QXmppOmemoStorage::setOwnDevice(const std::optional<OwnDevice> &device)

    Sets the own \a device (i.e., the device used by this client instance).
*/

/*!
    \fn QXmppOmemoStorage::addSignedPreKeyPair(uint32_t keyId, const SignedPreKeyPair &keyPair)

    Adds a signed pre key pair \a keyPair with ID \a keyId.
*/

/*!
    \fn QXmppOmemoStorage::removeSignedPreKeyPair(uint32_t keyId)

    Removes the signed pre key pair with ID \a keyId.
*/

/*!
    \fn QXmppOmemoStorage::addPreKeyPairs(const QHash<uint32_t, QByteArray> &keyPairs)

    Adds pre key pairs from \a keyPairs, which maps key IDs to the pre key
    pairs.
*/

/*!
    \fn QXmppOmemoStorage::removePreKeyPair(uint32_t keyId)

    Removes the pre key pair with ID \a keyId.
*/

/*!
    \fn QXmppOmemoStorage::addDevice(const QString &jid, uint32_t deviceId, const Device &device)

    Adds other devices (i.e., all devices but the own one).

    \a jid is the JID of the device owner, \a deviceId is the ID of the device
    and \a device is the device being added.
*/

/*!
    \fn QXmppOmemoStorage::removeDevice(const QString &jid, uint32_t deviceId)

    Removes a device of the other devices (i.e., all devices but the own one).

    \a jid is the JID of the device owner and \a deviceId is the ID of the
    device being removed.
*/

/*!
    \fn QXmppOmemoStorage::removeDevices(const QString &jid)

    Removes all devices belonging to \a jid from the other devices (i.e., all
    devices but the own one).
*/

/*!
    \fn QXmppOmemoStorage::resetAll()

    Resets all data.
*/

namespace QXmpp::Omemo {

/*!
    \class QXmpp::Omemo::Storage
    \inmodule QXmpp

    \brief The Storage class stores the data of the \xep{0384}{OMEMO Encryption} manager for all
    supported versions.

    The identity key and the signed pre keys are shared by all versions. Pre keys and devices are
    stored per version.

    \since QXmpp 1.18
*/

/*!
    Converts the data of a QXmppOmemoStorage to the data of this storage, so the device, its
    identity key, its sessions and the trust in it are kept when switching to the new manager. All
    pre keys and devices are assigned to OMEMO 2.
*/
Storage::Data Storage::migrate(const QXmppOmemoStorage::OmemoData &data)
{
    Data result;

    if (data.ownDevice) {
        const auto &device = *data.ownDevice;
        result.ownDevice = OwnDevice {
            device.id,
            device.privateIdentityKey,
            device.publicIdentityKey,
            device.latestSignedPreKeyId,
            { { Version::Omemo2, device.latestPreKeyId } },
        };
    }

    for (auto it = data.signedPreKeyPairs.cbegin(); it != data.signedPreKeyPairs.cend(); ++it) {
        result.signedPreKeys.append({ it.key(), it->data, it->creationDate });
    }
    for (auto it = data.preKeyPairs.cbegin(); it != data.preKeyPairs.cend(); ++it) {
        result.preKeys.append({ Version::Omemo2, it.key(), it.value() });
    }
    for (auto jidIt = data.devices.cbegin(); jidIt != data.devices.cend(); ++jidIt) {
        for (auto it = jidIt->cbegin(); it != jidIt->cend(); ++it) {
            result.devices.append({
                Version::Omemo2,
                jidIt.key(),
                it.key(),
                it->label,
                it->keyId,
                it->session,
                it->unrespondedSentStanzasCount,
                it->unrespondedReceivedStanzasCount,
                it->removalFromDeviceListDate,
            });
        }
    }

    // deterministic order
    std::ranges::sort(result.signedPreKeys, {}, &SignedPreKey::id);
    std::ranges::sort(result.preKeys, {}, &PreKey::id);
    std::ranges::sort(result.devices, [](const Device &a, const Device &b) {
        return std::tie(a.jid, a.id) < std::tie(b.jid, b.id);
    });
    return result;
}

/*!
    Copies the data of \a oldStorage into this storage using migrate().

    Nothing is changed and \c false is returned if \a oldStorage has no own device or if this
    storage already has one. Returns \c true if the data has been copied. The old storage is not
    modified, so it can be kept as backup.

    Trust is not part of the storage. The trust keys of OMEMO 2 are kept as they are, so the same
    trust storage must be used with the new manager. OMEMO 0 starts without trusted keys.
*/
QXmppTask<bool> Storage::migrateFrom(QXmppOmemoStorage *oldStorage)
{
    const auto oldData = co_await oldStorage->allData();
    if (!oldData.ownDevice || (co_await allData()).ownDevice) {
        co_return false;
    }

    const auto data = migrate(oldData);
    for (const auto &key : data.signedPreKeys) {
        co_await addSignedPreKey(key);
    }
    co_await addPreKeys(data.preKeys);
    for (const auto &device : data.devices) {
        co_await addDevice(device);
    }
    // last, so an interrupted migration is repeated
    co_await setOwnDevice(data.ownDevice);
    co_return true;
}

}  // namespace QXmpp::Omemo

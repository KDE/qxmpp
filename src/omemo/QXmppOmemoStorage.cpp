// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
    \class QXmppOmemoStorage
    \inmodule QXmpp

    \brief The QXmppOmemoStorage class stores data used by
    \xep{0384}{OMEMO Encryption}.

    \since QXmpp 1.5
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

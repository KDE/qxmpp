// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
    \class QXmppTrustStorage
    \inmodule QXmpp

    \brief The QXmppTrustStorage class stores end-to-end encryption trust data.

    The term "key" is used for a public long-term key.

    \since QXmpp 1.5
*/

/*!
    \fn QXmppTask<void> QXmppTrustStorage::setSecurityPolicy(const QString &encryption, QXmpp::TrustSecurityPolicy securityPolicy)

    Sets the security policy \a securityPolicy for the encryption protocol
    namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::resetSecurityPolicy(const QString &encryption)

    Resets the security policy for the encryption protocol namespace
    \a encryption.
*/

/*!
    \fn QXmppTrustStorage::securityPolicy(const QString &encryption)

    Returns the security policy for the encryption protocol namespace
    \a encryption.
*/

/*!
    \fn QXmppTrustStorage::setOwnKey(const QString &encryption, const QByteArray &keyId)

    Sets the own key (i.e., the key used by this client instance) for the
    encryption protocol namespace \a encryption to the key with ID \a keyId.
*/

/*!
    \fn QXmppTrustStorage::resetOwnKey(const QString &encryption)

    Resets the own key (i.e., the key used by this client instance) for the
    encryption protocol namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::ownKey(const QString &encryption)

    Returns the ID of the own key (i.e., the key used by this client instance)
    for the encryption protocol namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::addKeys(const QString &encryption, const QString &keyOwnerJid, const QList<QByteArray> &keyIds, QXmpp::TrustLevel trustLevel)

    Adds keys.

    \a encryption is the encryption protocol namespace. \a keyOwnerJid is the
    key owner's bare JID. \a keyIds are the IDs of the keys. \a trustLevel is
    the trust level of the keys.
*/

/*!
    \fn QXmppTrustStorage::removeKeys(const QString &encryption, const QList<QByteArray> &keyIds)

    Removes the keys with IDs \a keyIds for the encryption protocol namespace
    \a encryption.
*/

/*!
    \fn QXmppTrustStorage::removeKeys(const QString &encryption, const QString &keyOwnerJid)

    Removes all keys of the key owner with bare JID \a keyOwnerJid for the
    encryption protocol namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::removeKeys(const QString &encryption)

    Removes all keys for the encryption protocol namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::keys(const QString &encryption, QXmpp::TrustLevels trustLevels = {})

    Returns the JIDs of all key owners mapped to the IDs of their keys with the
    trust levels \a trustLevels for the encryption protocol namespace
    \a encryption.

    If no trust levels are passed, all keys for \a encryption are returned.
*/

/*!
    \fn QXmppTrustStorage::keys(const QString &encryption, const QList<QString> &keyOwnerJids, QXmpp::TrustLevels trustLevels = {})

    Returns the IDs of keys mapped to their trust levels for the key owners
    with bare JIDs \a keyOwnerJids and the encryption protocol namespace
    \a encryption, restricted to the trust levels \a trustLevels.

    If no trust levels are passed, all keys for \a encryption and
    \a keyOwnerJids are returned.
*/

/*!
    \fn QXmppTrustStorage::hasKey(const QString &encryption, const QString &keyOwnerJid, QXmpp::TrustLevels trustLevels)

    Returns whether at least one key of the key owner with bare JID
    \a keyOwnerJid and one of the possible trust levels \a trustLevels is
    stored for the encryption protocol namespace \a encryption.
*/

/*!
    \fn QXmppTrustStorage::setTrustLevel(const QString &encryption, const QMultiHash<QString, QByteArray> &keyIds, QXmpp::TrustLevel trustLevel)

    Sets the trust level \a trustLevel of keys for the encryption protocol
    namespace \a encryption. \a keyIds maps key owners' bare JIDs to the IDs
    of their keys.

    If a key is not stored, it is added to the storage.

    Returns the key owner JIDs mapped to their modified keys for specific
    encryption protocols.
*/

/*!
    \fn QXmppTrustStorage::setTrustLevel(const QString &encryption, const QList<QString> &keyOwnerJids, QXmpp::TrustLevel oldTrustLevel, QXmpp::TrustLevel newTrustLevel)

    Sets the trust level of keys specified by their key owner and trust level.

    \a encryption is the encryption protocol namespace. \a keyOwnerJids are the
    key owners' bare JIDs. \a oldTrustLevel is the trust level being changed.
    \a newTrustLevel is the trust level being set.

    Returns the key owner JIDs mapped to their modified keys for specific
    encryption protocols.
*/

/*!
    \fn QXmppTrustStorage::trustLevel(const QString &encryption, const QString &keyOwnerJid, const QByteArray &keyId)

    Returns the trust level of the key with ID \a keyId, owned by the JID
    \a keyOwnerJid, for the encryption protocol namespace \a encryption.

    If the key is not stored, the trust in that key is undecided.
*/

/*!
    \fn QXmppTrustStorage::resetAll(const QString &encryption)

    Resets all data for the encryption protocol namespace \a encryption.
*/

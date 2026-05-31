// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppTrustManager.h"

#include "QXmppPromise.h"
#include "QXmppTask.h"
#include "QXmppTrustStorage.h"

#include "Async.h"

using namespace QXmpp;
using namespace QXmpp::Private;

/*!
    \class QXmppTrustManager
    \inmodule QXmpp

    \brief The QXmppTrustManager manages end-to-end encryption trust decisions.

    \ingroup Managers

    \since QXmpp 1.5
*/

/*!
    Constructs a trust manager using the trust storage implementation
    \a trustStorage.
*/
QXmppTrustManager::QXmppTrustManager(QXmppTrustStorage *trustStorage)
    : m_trustStorage(trustStorage)
{
}

QXmppTrustManager::~QXmppTrustManager() = default;

/*!
    Sets the security policy \a securityPolicy for the encryption protocol
    namespace \a encryption.
*/
QXmppTask<void> QXmppTrustManager::setSecurityPolicy(const QString &encryption, TrustSecurityPolicy securityPolicy)
{
    return m_trustStorage->setSecurityPolicy(encryption, securityPolicy);
}

/*!
    Resets the security policy for the encryption protocol namespace
    \a encryption.
*/
QXmppTask<void> QXmppTrustManager::resetSecurityPolicy(const QString &encryption)
{
    return m_trustStorage->resetSecurityPolicy(encryption);
}

/*!
    Returns the security policy for the encryption protocol namespace
    \a encryption.
*/
QXmppTask<TrustSecurityPolicy> QXmppTrustManager::securityPolicy(const QString &encryption)
{
    return m_trustStorage->securityPolicy(encryption);
}

/*!
    Sets the own key (i.e., the key used by this client instance) for the
    encryption protocol namespace \a encryption to the key with ID \a keyId.
*/
QXmppTask<void> QXmppTrustManager::setOwnKey(const QString &encryption, const QByteArray &keyId)
{
    return m_trustStorage->setOwnKey(encryption, keyId);
}

/*!
    Resets the own key (i.e., the key used by this client instance) for the
    encryption protocol namespace \a encryption.
*/
QXmppTask<void> QXmppTrustManager::resetOwnKey(const QString &encryption)
{
    return m_trustStorage->resetOwnKey(encryption);
}

/*!
    Returns the ID of the own key (i.e., the key used by this client instance)
    for the encryption protocol namespace \a encryption.
*/
QXmppTask<QByteArray> QXmppTrustManager::ownKey(const QString &encryption)
{
    return m_trustStorage->ownKey(encryption);
}

/*!
    Adds keys.

    \a encryption is the encryption protocol namespace. \a keyOwnerJid is the
    key owner's bare JID. \a keyIds are the IDs of the keys. \a trustLevel is
    the trust level of the keys.
*/
QXmppTask<void> QXmppTrustManager::addKeys(const QString &encryption, const QString &keyOwnerJid, const QList<QByteArray> &keyIds, TrustLevel trustLevel)
{
    return m_trustStorage->addKeys(encryption, keyOwnerJid, keyIds, trustLevel);
}

/*!
    Removes the keys with IDs \a keyIds for the encryption protocol namespace
    \a encryption.
*/
QXmppTask<void> QXmppTrustManager::removeKeys(const QString &encryption, const QList<QByteArray> &keyIds)
{
    return m_trustStorage->removeKeys(encryption, keyIds);
}

/*!
    Removes all keys of the key owner with bare JID \a keyOwnerJid for the
    encryption protocol namespace \a encryption.
*/
QXmppTask<void> QXmppTrustManager::removeKeys(const QString &encryption, const QString &keyOwnerJid)
{
    return m_trustStorage->removeKeys(encryption, keyOwnerJid);
}

/*!
    Removes all keys for the encryption protocol namespace \a encryption.
*/
QXmppTask<void> QXmppTrustManager::removeKeys(const QString &encryption)
{
    return m_trustStorage->removeKeys(encryption);
}

/*!
    Returns the JIDs of all key owners mapped to the IDs of their keys with the
    trust levels \a trustLevels for the encryption protocol namespace
    \a encryption.

    If no trust levels are passed, all keys for \a encryption are returned.
*/
QXmppTask<QHash<QXmpp::TrustLevel, QMultiHash<QString, QByteArray>>> QXmppTrustManager::keys(const QString &encryption, QXmpp::TrustLevels trustLevels)
{
    return m_trustStorage->keys(encryption, trustLevels);
}

/*!
    Returns the IDs of keys mapped to their trust levels for the key owners
    with bare JIDs \a keyOwnerJids and the encryption protocol namespace
    \a encryption, restricted to the trust levels \a trustLevels.

    If no trust levels are passed, all keys for \a encryption and
    \a keyOwnerJids are returned.
*/
QXmppTask<QHash<QString, QHash<QByteArray, QXmpp::TrustLevel>>> QXmppTrustManager::keys(const QString &encryption, const QList<QString> &keyOwnerJids, QXmpp::TrustLevels trustLevels)
{
    return m_trustStorage->keys(encryption, keyOwnerJids, trustLevels);
}

/*!
    Returns whether at least one key of the key owner with bare JID
    \a keyOwnerJid and one of the possible trust levels \a trustLevels is
    stored for the encryption protocol namespace \a encryption.
*/
QXmppTask<bool> QXmppTrustManager::hasKey(const QString &encryption, const QString &keyOwnerJid, TrustLevels trustLevels)
{
    return m_trustStorage->hasKey(encryption, keyOwnerJid, trustLevels);
}

/*!
    Sets the trust level \a trustLevel of keys for the encryption protocol
    namespace \a encryption. \a keyIds maps key owners' bare JIDs to the IDs
    of their keys.

    If a key is not stored, it is added to the storage.
*/
QXmppTask<void> QXmppTrustManager::setTrustLevel(const QString &encryption, const QMultiHash<QString, QByteArray> &keyIds, TrustLevel trustLevel)
{
    auto modifiedKeys = co_await m_trustStorage->setTrustLevel(encryption, keyIds, trustLevel)
                            .withContext(this);
    Q_EMIT trustLevelsChanged(modifiedKeys);
}

/*!
    Sets the trust level of keys specified by their key owner and trust level.

    \a encryption is the encryption protocol namespace. \a keyOwnerJids are the
    key owners' bare JIDs. \a oldTrustLevel is the trust level being changed.
    \a newTrustLevel is the trust level being set.
*/
QXmppTask<void> QXmppTrustManager::setTrustLevel(const QString &encryption, const QList<QString> &keyOwnerJids, TrustLevel oldTrustLevel, TrustLevel newTrustLevel)
{
    auto modifiedKeys = co_await m_trustStorage->setTrustLevel(encryption, keyOwnerJids, oldTrustLevel, newTrustLevel)
                            .withContext(this);
    Q_EMIT trustLevelsChanged(modifiedKeys);
}

/*!
    Returns the trust level of the key with ID \a keyId, owned by the JID
    \a keyOwnerJid, for the encryption protocol namespace \a encryption.

    If the key is not stored, the trust in that key is undecided.
*/
QXmppTask<TrustLevel> QXmppTrustManager::trustLevel(const QString &encryption, const QString &keyOwnerJid, const QByteArray &keyId)
{
    return m_trustStorage->trustLevel(encryption, keyOwnerJid, keyId);
}

/*!
    Resets all data for the encryption protocol namespace \a encryption.
*/
QXmppTask<void> QXmppTrustManager::resetAll(const QString &encryption)
{
    return m_trustStorage->resetAll(encryption);
}

/*!
    \fn QXmppTrustManager::trustLevelsChanged(const QHash<QString, QMultiHash<QString, QByteArray>> &modifiedKeys)

    Emitted when the trust levels of keys changed because \c setTrustLevel()
    added a new key or modified an existing one.

    \a modifiedKeys maps key owners' bare JIDs to their modified keys for
    specific encryption protocol namespaces.
*/

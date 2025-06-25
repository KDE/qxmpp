// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppTrustMemoryStorage.h"

#include "Async.h"

#include <QMultiHash>

using namespace QXmpp;
using namespace QXmpp::Private;

///
/// \class QXmppTrustMemoryStorage
///
/// \brief The QXmppTrustMemoryStorage class stores trust data for end-to-end
/// encryption in the memory.
///
/// \since QXmpp 1.5
///

struct Key {
    QByteArray id;
    QString ownerJid;
    TrustLevel trustLevel;
};

class QXmppTrustMemoryStoragePrivate
{
public:
    // encryption protocols mapped to security policies
    QMap<QString, TrustSecurityPolicy> securityPolicies;

    // encryption protocols mapped to keys of this client instance
    QMap<QString, QByteArray> ownKeys;

    // encryption protocols mapped to keys with specified trust levels
    QMultiHash<QString, Key> keys;
};

///
/// Constructs a trust memory storage.
///
QXmppTrustMemoryStorage::QXmppTrustMemoryStorage()
    : d(new QXmppTrustMemoryStoragePrivate)
{
}

QXmppTrustMemoryStorage::~QXmppTrustMemoryStorage() = default;

/// \cond
QXmppTask<void> QXmppTrustMemoryStorage::setSecurityPolicy(const QString &encryption, TrustSecurityPolicy securityPolicy)
{
    d->securityPolicies.insert(encryption, securityPolicy);
    co_return;
}

QXmppTask<void> QXmppTrustMemoryStorage::resetSecurityPolicy(const QString &encryption)
{
    d->securityPolicies.remove(encryption);
    co_return;
}

QXmppTask<TrustSecurityPolicy> QXmppTrustMemoryStorage::securityPolicy(const QString &encryption)
{
    co_return d->securityPolicies.value(encryption);
}

QXmppTask<void> QXmppTrustMemoryStorage::setOwnKey(const QString &encryption, const QByteArray &keyId)
{
    d->ownKeys.insert(encryption, keyId);
    co_return;
}

QXmppTask<void> QXmppTrustMemoryStorage::resetOwnKey(const QString &encryption)
{
    d->ownKeys.remove(encryption);
    co_return;
}

QXmppTask<QByteArray> QXmppTrustMemoryStorage::ownKey(const QString &encryption)
{
    co_return d->ownKeys[encryption];
}

QXmppTask<void> QXmppTrustMemoryStorage::addKeys(const QString &encryption, const QString &keyOwnerJid, const QList<QByteArray> &keyIds, TrustLevel trustLevel)
{
    for (const auto &keyId : keyIds) {
        Key key;
        key.id = keyId;
        key.ownerJid = keyOwnerJid;
        key.trustLevel = trustLevel;
        d->keys.insert(encryption, key);
    }
    co_return;
}

QXmppTask<void> QXmppTrustMemoryStorage::removeKeys(const QString &encryption, const QList<QByteArray> &keyIds)
{
    for (auto itr = d->keys.find(encryption);
         itr != d->keys.end() && itr.key() == encryption;) {
        if (keyIds.contains(itr.value().id)) {
            itr = d->keys.erase(itr);
        } else {
            ++itr;
        }
    }
    co_return;
}

QXmppTask<void> QXmppTrustMemoryStorage::removeKeys(const QString &encryption, const QString &keyOwnerJid)
{
    for (auto itr = d->keys.find(encryption);
         itr != d->keys.end() && itr.key() == encryption;) {
        if (itr.value().ownerJid == keyOwnerJid) {
            itr = d->keys.erase(itr);
        } else {
            ++itr;
        }
    }
    co_return;
}

QXmppTask<void> QXmppTrustMemoryStorage::removeKeys(const QString &encryption)
{
    d->keys.remove(encryption);
    co_return;
}

QXmppTask<QHash<TrustLevel, QMultiHash<QString, QByteArray>>> QXmppTrustMemoryStorage::keys(const QString &encryption, TrustLevels trustLevels)
{
    QHash<TrustLevel, QMultiHash<QString, QByteArray>> keys;

    const auto storedKeys = d->keys.values(encryption);
    for (const auto &key : storedKeys) {
        const auto trustLevel = key.trustLevel;
        if (trustLevels.testFlag(trustLevel) || !trustLevels) {
            keys[trustLevel].insert(key.ownerJid, key.id);
        }
    }

    co_return keys;
}

QXmppTask<QHash<QString, QHash<QByteArray, TrustLevel>>> QXmppTrustMemoryStorage::keys(const QString &encryption, const QList<QString> &keyOwnerJids, TrustLevels trustLevels)
{
    QHash<QString, QHash<QByteArray, TrustLevel>> keys;

    const auto storedKeys = d->keys.values(encryption);
    for (const auto &key : storedKeys) {
        const auto keyOwnerJid = key.ownerJid;
        const auto trustLevel = key.trustLevel;
        if (keyOwnerJids.contains(keyOwnerJid) && (trustLevels.testFlag(trustLevel) || !trustLevels)) {
            keys[keyOwnerJid].insert(key.id, trustLevel);
        }
    }

    co_return keys;
}

QXmppTask<bool> QXmppTrustMemoryStorage::hasKey(const QString &encryption, const QString &keyOwnerJid, TrustLevels trustLevels)
{
    const auto storedKeys = d->keys.values(encryption);
    for (const auto &key : storedKeys) {
        if (key.ownerJid == keyOwnerJid && trustLevels.testFlag(key.trustLevel)) {
            co_return true;
        }
    }

    co_return false;
}

QXmppTask<QHash<QString, QMultiHash<QString, QByteArray>>> QXmppTrustMemoryStorage::setTrustLevel(const QString &encryption, const QMultiHash<QString, QByteArray> &keyIds, TrustLevel trustLevel)
{
    QHash<QString, QMultiHash<QString, QByteArray>> modifiedKeys;

    for (auto itr = keyIds.constBegin(); itr != keyIds.constEnd(); ++itr) {
        const auto keyOwnerJid = itr.key();
        const auto keyId = itr.value();

        auto isKeyFound = false;

        for (auto itrKeys = d->keys.find(encryption);
             itrKeys != d->keys.end() && itrKeys.key() == encryption;
             ++itrKeys) {
            auto &key = itrKeys.value();
            if (key.id == keyId && key.ownerJid == keyOwnerJid) {
                // Update the stored trust level if it differs from the new one.
                if (key.trustLevel != trustLevel) {
                    key.trustLevel = trustLevel;
                    modifiedKeys[encryption].insert(keyOwnerJid, keyId);
                }

                isKeyFound = true;
                break;
            }
        }

        // Create a new entry and store it if there is no such entry yet.
        if (!isKeyFound) {
            Key key;
            key.id = keyId;
            key.ownerJid = keyOwnerJid;
            key.trustLevel = trustLevel;
            d->keys.insert(encryption, key);
            modifiedKeys[encryption].insert(keyOwnerJid, keyId);
        }
    }

    co_return modifiedKeys;
}

QXmppTask<QHash<QString, QMultiHash<QString, QByteArray>>> QXmppTrustMemoryStorage::setTrustLevel(const QString &encryption, const QList<QString> &keyOwnerJids, TrustLevel oldTrustLevel, TrustLevel newTrustLevel)
{
    QHash<QString, QMultiHash<QString, QByteArray>> modifiedKeys;

    for (auto itr = d->keys.find(encryption); itr != d->keys.end() && itr.key() == encryption; ++itr) {
        auto &key = itr.value();
        auto keyOwnerJid = key.ownerJid;
        if (keyOwnerJids.contains(keyOwnerJid) && key.trustLevel == oldTrustLevel) {
            key.trustLevel = newTrustLevel;
            modifiedKeys[encryption].insert(keyOwnerJid, key.id);
        }
    }

    co_return modifiedKeys;
}

QXmppTask<TrustLevel> QXmppTrustMemoryStorage::trustLevel(const QString &encryption, const QString &keyOwnerJid, const QByteArray &keyId)
{
    const auto keys = d->keys.values(encryption);
    for (const auto &key : keys) {
        if (key.id == keyId && key.ownerJid == keyOwnerJid) {
            co_return TrustLevel(key.trustLevel);
        }
    }

    co_return TrustLevel::Undecided;
}

QXmppTask<void> QXmppTrustMemoryStorage::resetAll(const QString &encryption)
{
    d->securityPolicies.remove(encryption);
    d->ownKeys.remove(encryption);
    d->keys.remove(encryption);
    co_return;
}
/// \endcond

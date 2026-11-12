// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoKeyMaintenance.h"

#include "StringLiterals.h"

#include <algorithm>

namespace QXmpp::Omemo::Private::KeyMaintenance {

static QXmppError error(QString message)
{
    return QXmppError { std::move(message), {} };
}

// Times at which the previous signed pre keys can be removed: the creation of the next newer key
// plus the grace period.
static QHash<uint32_t, QDateTime> removalTimes(const DeviceKeys &device, const Config &config)
{
    QHash<uint32_t, QDateTime> times;
    for (auto it = device.signedPreKeys.cbegin(); it != device.signedPreKeys.cend(); ++it) {
        if (it.key() == device.latestSignedPreKeyId) {
            continue;
        }
        QDateTime successor;
        for (const auto &other : device.signedPreKeys) {
            if (other.creationDate > it->creationDate && (!successor.isValid() || other.creationDate < successor)) {
                successor = other.creationDate;
            }
        }
        // without a newer key, the latest one replaced it
        if (!successor.isValid()) {
            successor = device.signedPreKeys.value(device.latestSignedPreKeyId).creationDate;
        }
        times.insert(it.key(), successor.addSecs(config.signedPreKeyGracePeriod.count()));
    }
    return times;
}

std::variant<SignedPreKeyChanges, QXmppError> rotateSignedPreKey(const SignalContext &context, DeviceKeys &device, const QDateTime &now, const Config &config)
{
    SignedPreKeyChanges changes;

    const auto latest = device.signedPreKeys.constFind(device.latestSignedPreKeyId);
    const auto isRenewalDue = latest == device.signedPreKeys.cend() ||
        latest->creationDate.secsTo(now) >= config.signedPreKeyRenewalInterval.count();

    if (isRenewalDue) {
        const auto id = nextKeyId(device.latestSignedPreKeyId, device.signedPreKeys);
        if (!id) {
            return error(u"No free signed pre key ID"_s);
        }
        auto record = context.generateSignedPreKey(device.identity, *id, uint64_t(now.toMSecsSinceEpoch()));
        if (!record) {
            return error(u"Signed pre key could not be created"_s);
        }
        device.signedPreKeys.insert(*id, { std::move(*record), now });
        device.latestSignedPreKeyId = *id;
        changes.addedId = id;
    }

    // A key is removed when its successor has been published for the grace period.
    const auto removals = removalTimes(device, config);
    for (auto it = removals.cbegin(); it != removals.cend(); ++it) {
        if (it.value() <= now) {
            device.signedPreKeys.remove(it.key());
            changes.removedIds.append(it.key());
        }
    }
    std::ranges::sort(changes.removedIds);

    return changes;
}

QDateTime nextSignedPreKeyCheck(const DeviceKeys &device, const Config &config)
{
    const auto latest = device.signedPreKeys.constFind(device.latestSignedPreKeyId);
    if (latest == device.signedPreKeys.cend()) {
        return {};
    }

    auto next = latest->creationDate.addSecs(config.signedPreKeyRenewalInterval.count());
    for (const auto &removal : removalTimes(device, config)) {
        next = std::min(next, removal);
    }
    return next;
}

std::variant<QList<uint32_t>, QXmppError> refillPreKeys(const SignalContext &context, VersionKeys &keys, const Config &config)
{
    QList<uint32_t> added;
    while (keys.preKeys.size() < config.preKeyCount) {
        const auto id = nextKeyId(keys.latestPreKeyId, keys.preKeys, PRE_KEY_ID_MAX);
        if (!id) {
            return error(u"No free pre key ID"_s);
        }
        auto generated = context.generatePreKeys(*id, 1);
        if (!generated.contains(*id)) {
            return error(u"Pre key could not be created"_s);
        }
        keys.preKeys.insert(*id, std::move(generated[*id]));
        keys.latestPreKeyId = *id;
        added.append(*id);
    }
    return added;
}

}  // namespace QXmpp::Omemo::Private::KeyMaintenance

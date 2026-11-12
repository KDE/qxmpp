// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOKEYMAINTENANCE_H
#define OMEMOKEYMAINTENANCE_H

#include "OmemoSession.h"

#include <chrono>

// Creation, refill and rotation of the own keys. The functions modify the keys in place and return
// what changed, so it can be stored and the bundles can be republished. The current time is
// passed in, nothing here depends on the clock.
namespace QXmpp::Omemo::Private::KeyMaintenance {

using namespace std::chrono_literals;

constexpr uint32_t KEY_ID_MIN = 1;
constexpr uint32_t KEY_ID_MAX = std::numeric_limits<int32_t>::max();
// libomemo-c wraps larger pre key IDs
constexpr uint32_t PRE_KEY_ID_MAX = 0xFFFFFE;

struct Config {
    // number of pre keys published per version
    int preKeyCount = 100;
    // age after which a new signed pre key is created
    std::chrono::seconds signedPreKeyRenewalInterval = 24h * 7 * 4;
    // time the previous signed pre keys are kept after a new one has been created, so key
    // exchanges created with the previous bundle can still be decrypted
    std::chrono::seconds signedPreKeyGracePeriod = 24h * 7 * 4;
};

// The ID following latestId in [KEY_ID_MIN, maxId] that is not in use, wrapping around.
std::optional<uint32_t> nextKeyId(uint32_t latestId, const auto &usedIds, uint32_t maxId = KEY_ID_MAX)
{
    auto id = latestId;
    for (uint64_t attempts = 0; attempts < uint64_t(maxId); ++attempts) {
        id = (id >= maxId || id < KEY_ID_MIN) ? KEY_ID_MIN : id + 1;
        if (!usedIds.contains(id)) {
            return id;
        }
    }
    return {};
}

struct SignedPreKeyChanges {
    std::optional<uint32_t> addedId;
    QList<uint32_t> removedIds;

    bool isEmpty() const { return !addedId && removedIds.isEmpty(); }
};

// Creates a signed pre key if there is none or the latest one is older than the renewal interval,
// and removes the previous ones after the grace period.
std::variant<SignedPreKeyChanges, QXmppError> rotateSignedPreKey(const SignalContext &context, DeviceKeys &device, const QDateTime &now, const Config &config = {});

// The time at which rotateSignedPreKey() has to be called next.
QDateTime nextSignedPreKeyCheck(const DeviceKeys &device, const Config &config = {});

// Creates pre keys until there are preKeyCount of them. Returns the IDs of the new keys.
std::variant<QList<uint32_t>, QXmppError> refillPreKeys(const SignalContext &context, VersionKeys &keys, const Config &config = {});

}  // namespace QXmpp::Omemo::Private::KeyMaintenance

#endif  // OMEMOKEYMAINTENANCE_H

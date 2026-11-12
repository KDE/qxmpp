// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOSESSION_H
#define OMEMOSESSION_H

#include "QXmppError.h"
#include "QXmppOmemoStorage.h"

#include "Crypto.h"
#include "OmemoLibWrappers.h"

#include <functional>
#include <memory>
#include <optional>
#include <variant>

#include <QDateTime>
#include <QHash>
#include <QRecursiveMutex>

namespace QXmpp::Omemo::Private {

using SecureByteArray = QXmpp::Private::Crypto::SecureByteArray;

struct Address {
    QString jid;
    uint32_t deviceId = 0;

    bool operator==(const Address &) const = default;
};

inline size_t qHash(const Address &address, size_t seed = 0)
{
    return qHashMulti(seed, address.jid, address.deviceId);
}

struct IdentityKeyPair {
    // serialized by libomemo-c
    SecureByteArray privateKey;
    // Ed25519 form
    QByteArray publicKey;
};

// Public data of a device needed to build a session, in the form of the version's bundle.
struct Bundle {
    // Ed25519 form for OMEMO 2, Curve25519 form with type prefix (0x05) for OMEMO 0
    QByteArray identityKey;
    uint32_t signedPreKeyId = 0;
    QByteArray signedPreKey;
    QByteArray signedPreKeySignature;
    uint32_t preKeyId = 0;
    QByteArray preKey;
};

// Owns the libomemo-c context. All sessions using it must be destroyed before.
class SignalContext
{
public:
    SignalContext();
    ~SignalContext();
    Q_DISABLE_COPY_MOVE(SignalContext)

    bool isValid() const { return m_context; }
    signal_context *get() const { return m_context.get(); }

    std::optional<IdentityKeyPair> generateIdentityKeyPair() const;
    // device ID in the range [1, 2^31 - 1]
    std::optional<uint32_t> generateDeviceId() const;
    // serialized pre key records, IDs from firstId to firstId + count - 1 (at most 0xFFFFFE)
    QHash<uint32_t, SecureByteArray> generatePreKeys(uint32_t firstId, uint32_t count) const;
    // serialized signed pre key record, signed for both versions
    std::optional<SecureByteArray> generateSignedPreKey(const IdentityKeyPair &identity, uint32_t id, uint64_t timestamp) const;

private:
    OmemoContextPtr m_context;
    signal_crypto_provider m_cryptoProvider;
    QRecursiveMutex m_mutex;
};

struct SignedPreKey {
    // serialized by libomemo-c, signed for both versions
    SecureByteArray record;
    QDateTime creationDate;
};

// Key material of the own device, shared by all versions.
struct DeviceKeys {
    uint32_t deviceId = 0;
    IdentityKeyPair identity;
    QHash<uint32_t, SignedPreKey> signedPreKeys;
    uint32_t latestSignedPreKeyId = 0;
};

// Pre keys and sessions of one version.
struct VersionKeys {
    QHash<uint32_t, SecureByteArray> preKeys;
    uint32_t latestPreKeyId = 0;
    QHash<Address, QByteArray> sessions;
};

struct EncryptedKey {
    QByteArray data;
    bool isKeyExchange = false;
};

struct DecryptedKey {
    SecureByteArray data;
    // in the format of the version, only set for key exchanges
    QByteArray senderIdentityKey;
    // the pre key consumed by a key exchange, it has been removed from the key store
    std::optional<uint32_t> usedPreKeyId;
};

// Encrypts and decrypts the key material of messages with the double ratchet of libomemo-c for one
// version. The state is owned by the caller; changes of the version keys are reported via
// Callbacks so they can be persisted.
class SessionLayer
{
public:
    struct Callbacks {
        std::function<void(const Address &, const QByteArray &session)> sessionStored;
        std::function<void(const Address &)> sessionDeleted;
        std::function<void(uint32_t preKeyId)> preKeyRemoved;
    };

    SessionLayer(const SignalContext &context, Version version, const DeviceKeys &device, VersionKeys &keys, Callbacks callbacks = {});
    ~SessionLayer();
    Q_DISABLE_COPY_MOVE(SessionLayer)

    Version version() const { return m_version; }
    // own public identity key in the format of the version
    QByteArray ownIdentityKey() const;

    // The public part of the own bundle with the given keys in the format of the version.
    std::variant<Bundle, QXmppError> ownBundle(uint32_t signedPreKeyId, uint32_t preKeyId) const;
    // public pre keys in the format of the version
    std::variant<QHash<uint32_t, QByteArray>, QXmppError> publicPreKeys() const;

    std::variant<QXmpp::Success, QXmppError> buildSession(const Address &address, const Bundle &bundle);
    bool hasSession(const Address &address) const;

    std::variant<EncryptedKey, QXmppError> encrypt(const Address &address, const SecureByteArray &key);
    std::variant<DecryptedKey, QXmppError> decrypt(const Address &address, const QByteArray &data, bool isKeyExchange);

private:
    friend struct StoreCallbacks;

    const SignalContext &m_context;
    Version m_version;
    const DeviceKeys &m_device;
    VersionKeys &m_keys;
    Callbacks m_callbacks;
    StoreContextPtr m_storeContext;
    std::optional<uint32_t> m_removedPreKeyId;
};

}  // namespace QXmpp::Omemo::Private

#endif  // OMEMOSESSION_H

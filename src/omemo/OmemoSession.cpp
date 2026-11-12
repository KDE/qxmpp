// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoSession.h"

#include "OmemoCryptoProvider.h"
#include "StringLiterals.h"

#include <curve.h>
#include <protocol.h>
#include <ratchet.h>
#include <session_pre_key.h>

using namespace QXmpp;

namespace QXmpp::Omemo::Private {

namespace {

QByteArray toByteArray(const uint8_t *data, size_t size)
{
    return QByteArray(reinterpret_cast<const char *>(data), qsizetype(size));
}

SecureByteArray toSecureByteArray(const uint8_t *data, size_t size)
{
    return SecureByteArray(reinterpret_cast<const char *>(data), int(size));
}

const uint8_t *bytes(const QByteArray &data)
{
    return reinterpret_cast<const uint8_t *>(data.constData());
}

const uint8_t *bytes(const SecureByteArray &data)
{
    return reinterpret_cast<const uint8_t *>(data.constData());
}

int setBuffer(signal_buffer **buffer, const char *data, qsizetype size)
{
    *buffer = signal_buffer_create(reinterpret_cast<const uint8_t *>(data), size_t(size));
    return *buffer ? 0 : SG_ERR_NOMEM;
}

QXmppError error(QString message)
{
    return QXmppError { std::move(message), {} };
}

// libomemo-c addresses point into the UTF-8 encoded JID, which must outlive the address.
class LibAddress
{
public:
    explicit LibAddress(const Address &address)
        : m_jid(address.jid.toUtf8()),
          m_address { m_jid.constData(), size_t(m_jid.size()), int32_t(address.deviceId) }
    {
    }
    const signal_protocol_address *get() const { return &m_address; }

private:
    QByteArray m_jid;
    signal_protocol_address m_address;
};

Address fromLibAddress(const signal_protocol_address *address)
{
    return { QString::fromUtf8(address->name, qsizetype(address->name_len)), uint32_t(address->device_id) };
}

// Ed25519 form for OMEMO 2, Curve25519 form with type prefix for OMEMO 0
QByteArray serializeIdentityKey(const ec_public_key *key, Version version)
{
    if (version == Version::Omemo2) {
        return BufferPtr(ec_public_key_get_ed(key)).toByteArray();
    }
    BufferPtr buffer;
    if (ec_public_key_serialize(buffer.ptrRef(), key) < 0) {
        return {};
    }
    return buffer.toByteArray();
}

// without type prefix for OMEMO 2, with type prefix for OMEMO 0
QByteArray serializePreKey(const ec_public_key *key, Version version)
{
    if (version == Version::Omemo2) {
        return BufferPtr(ec_public_key_get_mont(key)).toByteArray();
    }
    BufferPtr buffer;
    if (ec_public_key_serialize(buffer.ptrRef(), key) < 0) {
        return {};
    }
    return buffer.toByteArray();
}

constexpr qsizetype KEY_SIZE = 32;
constexpr char CURVE25519_TYPE = 0x05;

// OMEMO 0 keys must have the type prefix; libomemo-c would read keys without it as Ed25519 keys.
bool hasValidSize(const QByteArray &key, Version version)
{
    if (version == Version::Omemo2) {
        return key.size() == KEY_SIZE;
    }
    return key.size() == KEY_SIZE + 1 && key.at(0) == CURVE25519_TYPE;
}

RefCountedPtr<ec_public_key> decodeIdentityKey(const QByteArray &key, Version version, signal_context *context)
{
    RefCountedPtr<ec_public_key> result;
    if (!hasValidSize(key, version)) {
        return result;
    }
    const auto status = version == Version::Omemo2
        ? curve_decode_point_ed(result.ptrRef(), bytes(key), size_t(key.size()), context)
        : curve_decode_point_mont(result.ptrRef(), bytes(key), size_t(key.size()), context);
    if (status < 0) {
        result.reset(nullptr);
    }
    return result;
}

RefCountedPtr<ec_public_key> decodePreKey(const QByteArray &key, Version version, signal_context *context)
{
    RefCountedPtr<ec_public_key> result;
    if (!hasValidSize(key, version) ||
        curve_decode_point_mont(result.ptrRef(), bytes(key), size_t(key.size()), context) < 0) {
        result.reset(nullptr);
    }
    return result;
}

uint32_t libVersion(Version version)
{
    return version == Version::Omemo2 ? CIPHERTEXT_OMEMO_VERSION : CIPHERTEXT_CURRENT_VERSION;
}

RefCountedPtr<ratchet_identity_key_pair> deserializeIdentityKeyPair(const IdentityKeyPair &identity, signal_context *context)
{
    RefCountedPtr<ec_private_key> privateKey;
    if (curve_decode_private_point(privateKey.ptrRef(), bytes(identity.privateKey), size_t(identity.privateKey.size()), context) < 0) {
        return {};
    }
    RefCountedPtr<ec_public_key> publicKey;
    if (curve_decode_point_ed(publicKey.ptrRef(), bytes(identity.publicKey), size_t(identity.publicKey.size()), context) < 0) {
        return {};
    }
    RefCountedPtr<ratchet_identity_key_pair> keyPair;
    if (ratchet_identity_key_pair_create(keyPair.ptrRef(), publicKey.get(), privateKey.get()) < 0) {
        return {};
    }
    return keyPair;
}

}  // namespace

SignalContext::SignalContext()
{
    if (signal_context_create(m_context.ptrRef(), this) < 0) {
        m_context.reset(nullptr);
        return;
    }

    const auto lock = [](void *userData) {
        static_cast<SignalContext *>(userData)->m_mutex.lock();
    };
    const auto unlock = [](void *userData) {
        static_cast<SignalContext *>(userData)->m_mutex.unlock();
    };

    m_cryptoProvider = createOmemoCryptoProvider(nullptr);
    if (signal_context_set_locking_functions(m_context.get(), lock, unlock) < 0 ||
        signal_context_set_crypto_provider(m_context.get(), &m_cryptoProvider) < 0) {
        m_context.reset(nullptr);
    }
}

SignalContext::~SignalContext() = default;

std::optional<IdentityKeyPair> SignalContext::generateIdentityKeyPair() const
{
    RefCountedPtr<ratchet_identity_key_pair> keyPair;
    if (signal_protocol_key_helper_generate_identity_key_pair(keyPair.ptrRef(), get()) < 0) {
        return {};
    }

    BufferSecurePtr privateKey;
    if (ec_private_key_serialize(privateKey.ptrRef(), ratchet_identity_key_pair_get_private(keyPair.get())) < 0) {
        return {};
    }
    BufferPtr publicKey(ec_public_key_get_ed(ratchet_identity_key_pair_get_public(keyPair.get())));
    if (!publicKey) {
        return {};
    }

    return IdentityKeyPair {
        toSecureByteArray(signal_buffer_data(privateKey.get()), signal_buffer_len(privateKey.get())),
        publicKey.toByteArray(),
    };
}

std::optional<uint32_t> SignalContext::generateDeviceId() const
{
    uint32_t id = 0;
    // The result is in [1, 16380]; the extended range is [1, 2^31 - 2].
    if (signal_protocol_key_helper_generate_registration_id(&id, 1, get()) < 0) {
        return {};
    }
    return id;
}

QHash<uint32_t, SecureByteArray> SignalContext::generatePreKeys(uint32_t firstId, uint32_t count) const
{
    KeyListNodePtr list;
    if (signal_protocol_key_helper_generate_pre_keys(list.ptrRef(), firstId, count, get()) < 0) {
        return {};
    }

    QHash<uint32_t, SecureByteArray> keys;
    for (auto *node = list.get(); node; node = signal_protocol_key_helper_key_list_next(node)) {
        auto *preKey = signal_protocol_key_helper_key_list_element(node);
        BufferSecurePtr record;
        if (session_pre_key_serialize(record.ptrRef(), preKey) < 0) {
            return {};
        }
        // libomemo-c wraps IDs out of its range
        if (session_pre_key_get_id(preKey) != firstId + uint32_t(keys.size())) {
            return {};
        }
        keys.insert(session_pre_key_get_id(preKey), toSecureByteArray(signal_buffer_data(record.get()), signal_buffer_len(record.get())));
    }
    return keys;
}

std::optional<SecureByteArray> SignalContext::generateSignedPreKey(const IdentityKeyPair &identity, uint32_t id, uint64_t timestamp) const
{
    const auto identityKeyPair = deserializeIdentityKeyPair(identity, get());
    if (!identityKeyPair) {
        return {};
    }

    RefCountedPtr<session_signed_pre_key> signedPreKey;
    if (signal_protocol_key_helper_generate_signed_pre_key(signedPreKey.ptrRef(), identityKeyPair.get(), id, timestamp, get()) < 0) {
        return {};
    }

    BufferSecurePtr record;
    if (session_signed_pre_key_serialize(record.ptrRef(), signedPreKey.get()) < 0) {
        return {};
    }
    return toSecureByteArray(signal_buffer_data(record.get()), signal_buffer_len(record.get()));
}

struct StoreCallbacks {
    static SessionLayer *layer(void *userData) { return static_cast<SessionLayer *>(userData); }

    static signal_protocol_identity_key_store identityKeyStore(SessionLayer *self)
    {
        signal_protocol_identity_key_store store {};
        store.get_identity_key_pair = [](signal_buffer **publicData, signal_buffer **privateData, void *userData) {
            const auto &identity = layer(userData)->m_device.identity;
            if (const auto result = setBuffer(privateData, identity.privateKey.constData(), identity.privateKey.size()); result < 0) {
                return result;
            }
            return setBuffer(publicData, identity.publicKey.constData(), identity.publicKey.size());
        };
        store.get_local_registration_id = [](void *userData, uint32_t *registrationId) {
            *registrationId = layer(userData)->m_device.deviceId;
            return 0;
        };
        // Trust is handled by the trust manager, not by libomemo-c.
        store.save_identity = [](const signal_protocol_address *, uint8_t *, size_t, void *) {
            return 0;
        };
        store.is_trusted_identity = [](const signal_protocol_address *, uint8_t *, size_t, void *) {
            return 1;
        };
        store.user_data = self;
        return store;
    }

    static signal_protocol_signed_pre_key_store signedPreKeyStore(SessionLayer *self)
    {
        signal_protocol_signed_pre_key_store store {};
        store.load_signed_pre_key = [](signal_buffer **record, uint32_t id, void *userData) {
            const auto key = layer(userData)->m_device.signedPreKeys.value(id).record;
            if (key.isEmpty()) {
                return SG_ERR_INVALID_KEY_ID;
            }
            return setBuffer(record, key.constData(), key.size());
        };
        // Signed pre keys are only created and removed by the key maintenance.
        store.store_signed_pre_key = [](uint32_t, uint8_t *, size_t, void *) {
            return 0;
        };
        store.contains_signed_pre_key = [](uint32_t id, void *userData) {
            return layer(userData)->m_device.signedPreKeys.contains(id) ? 1 : 0;
        };
        store.remove_signed_pre_key = [](uint32_t, void *) {
            return 0;
        };
        store.user_data = self;
        return store;
    }

    static signal_protocol_pre_key_store preKeyStore(SessionLayer *self)
    {
        signal_protocol_pre_key_store store {};
        store.load_pre_key = [](signal_buffer **record, uint32_t id, void *userData) {
            const auto key = layer(userData)->m_keys.preKeys.value(id);
            if (key.isEmpty()) {
                return SG_ERR_INVALID_KEY_ID;
            }
            return setBuffer(record, key.constData(), key.size());
        };
        // Pre keys are only created by the key maintenance.
        store.store_pre_key = [](uint32_t, uint8_t *, size_t, void *) {
            return 0;
        };
        store.contains_pre_key = [](uint32_t id, void *userData) {
            return layer(userData)->m_keys.preKeys.contains(id) ? 1 : 0;
        };
        store.remove_pre_key = [](uint32_t id, void *userData) {
            auto *l = layer(userData);
            if (l->m_keys.preKeys.remove(id)) {
                l->m_removedPreKeyId = id;
                if (l->m_callbacks.preKeyRemoved) {
                    l->m_callbacks.preKeyRemoved(id);
                }
            }
            return 0;
        };
        store.user_data = self;
        return store;
    }

    static signal_protocol_session_store sessionStore(SessionLayer *self)
    {
        signal_protocol_session_store store {};
        store.load_session_func = [](signal_buffer **record, signal_buffer **, const signal_protocol_address *address, void *userData) {
            const auto session = layer(userData)->m_keys.sessions.value(fromLibAddress(address));
            if (session.isEmpty()) {
                return 0;
            }
            if (const auto result = setBuffer(record, session.constData(), session.size()); result < 0) {
                return result;
            }
            return 1;
        };
        store.get_sub_device_sessions_func = [](signal_int_list **sessions, const char *name, size_t nameLength, void *userData) {
            const auto jid = QString::fromUtf8(name, qsizetype(nameLength));
            *sessions = signal_int_list_alloc();
            if (!*sessions) {
                return SG_ERR_NOMEM;
            }
            const auto &allSessions = layer(userData)->m_keys.sessions;
            for (auto it = allSessions.cbegin(); it != allSessions.cend(); ++it) {
                if (it.key().jid == jid && signal_int_list_push_back(*sessions, int(it.key().deviceId)) < 0) {
                    return SG_ERR_NOMEM;
                }
            }
            return int(signal_int_list_size(*sessions));
        };
        store.store_session_func = [](const signal_protocol_address *address, uint8_t *record, size_t recordLength, uint8_t *, size_t, void *userData) {
            auto *l = layer(userData);
            const auto key = fromLibAddress(address);
            const auto session = toByteArray(record, recordLength);
            l->m_keys.sessions.insert(key, session);
            if (l->m_callbacks.sessionStored) {
                l->m_callbacks.sessionStored(key, session);
            }
            return 0;
        };
        store.contains_session_func = [](const signal_protocol_address *address, void *userData) {
            return layer(userData)->m_keys.sessions.contains(fromLibAddress(address)) ? 1 : 0;
        };
        store.delete_session_func = [](const signal_protocol_address *address, void *userData) {
            auto *l = layer(userData);
            const auto key = fromLibAddress(address);
            if (!l->m_keys.sessions.remove(key)) {
                return 0;
            }
            if (l->m_callbacks.sessionDeleted) {
                l->m_callbacks.sessionDeleted(key);
            }
            return 1;
        };
        store.delete_all_sessions_func = [](const char *name, size_t nameLength, void *userData) {
            auto *l = layer(userData);
            const auto jid = QString::fromUtf8(name, qsizetype(nameLength));
            QList<Address> deleted;
            for (auto it = l->m_keys.sessions.begin(); it != l->m_keys.sessions.end();) {
                if (it.key().jid == jid) {
                    deleted.append(it.key());
                    it = l->m_keys.sessions.erase(it);
                } else {
                    ++it;
                }
            }
            if (l->m_callbacks.sessionDeleted) {
                for (const auto &address : std::as_const(deleted)) {
                    l->m_callbacks.sessionDeleted(address);
                }
            }
            return int(deleted.size());
        };
        store.user_data = self;
        return store;
    }
};

SessionLayer::SessionLayer(const SignalContext &context, Version version, const DeviceKeys &device, VersionKeys &keys, Callbacks callbacks)
    : m_context(context), m_version(version), m_device(device), m_keys(keys), m_callbacks(std::move(callbacks))
{
    if (signal_protocol_store_context_create(m_storeContext.ptrRef(), m_context.get()) < 0) {
        m_storeContext.reset(nullptr);
        return;
    }

    // libomemo-c copies the store structs.
    auto identityKeyStore = StoreCallbacks::identityKeyStore(this);
    auto signedPreKeyStore = StoreCallbacks::signedPreKeyStore(this);
    auto preKeyStore = StoreCallbacks::preKeyStore(this);
    auto sessionStore = StoreCallbacks::sessionStore(this);
    signal_protocol_store_context_set_identity_key_store(m_storeContext.get(), &identityKeyStore);
    signal_protocol_store_context_set_signed_pre_key_store(m_storeContext.get(), &signedPreKeyStore);
    signal_protocol_store_context_set_pre_key_store(m_storeContext.get(), &preKeyStore);
    signal_protocol_store_context_set_session_store(m_storeContext.get(), &sessionStore);
}

SessionLayer::~SessionLayer() = default;

QByteArray SessionLayer::ownIdentityKey() const
{
    const auto identityKeyPair = deserializeIdentityKeyPair(m_device.identity, m_context.get());
    return identityKeyPair ? serializeIdentityKey(ratchet_identity_key_pair_get_public(identityKeyPair.get()), m_version) : QByteArray();
}

std::variant<Bundle, QXmppError> SessionLayer::ownBundle(uint32_t signedPreKeyId, uint32_t preKeyId) const
{
    const auto identityKeyPair = deserializeIdentityKeyPair(m_device.identity, m_context.get());
    if (!identityKeyPair) {
        return error(u"Own identity key pair could not be deserialized"_s);
    }

    const auto signedRecord = m_device.signedPreKeys.value(signedPreKeyId).record;
    RefCountedPtr<session_signed_pre_key> signedPreKey;
    if (signedRecord.isEmpty() ||
        session_signed_pre_key_deserialize(signedPreKey.ptrRef(), bytes(signedRecord), size_t(signedRecord.size()), m_context.get()) < 0) {
        return error(u"Signed pre key %1 could not be loaded"_s.arg(signedPreKeyId));
    }

    const auto preKeyRecord = m_keys.preKeys.value(preKeyId);
    RefCountedPtr<session_pre_key> preKey;
    if (preKeyRecord.isEmpty() ||
        session_pre_key_deserialize(preKey.ptrRef(), bytes(preKeyRecord), size_t(preKeyRecord.size()), m_context.get()) < 0) {
        return error(u"Pre key %1 could not be loaded"_s.arg(preKeyId));
    }

    const auto *signedPublicKey = ec_key_pair_get_public(session_signed_pre_key_get_key_pair(signedPreKey.get()));
    const auto signature = m_version == Version::Omemo2
        ? toByteArray(session_signed_pre_key_get_signature_omemo(signedPreKey.get()), session_signed_pre_key_get_signature_omemo_len(signedPreKey.get()))
        : toByteArray(session_signed_pre_key_get_signature(signedPreKey.get()), session_signed_pre_key_get_signature_len(signedPreKey.get()));

    return Bundle {
        serializeIdentityKey(ratchet_identity_key_pair_get_public(identityKeyPair.get()), m_version),
        signedPreKeyId,
        serializePreKey(signedPublicKey, m_version),
        signature,
        preKeyId,
        serializePreKey(ec_key_pair_get_public(session_pre_key_get_key_pair(preKey.get())), m_version),
    };
}

std::variant<QHash<uint32_t, QByteArray>, QXmppError> SessionLayer::publicPreKeys() const
{
    QHash<uint32_t, QByteArray> keys;
    for (auto it = m_keys.preKeys.cbegin(); it != m_keys.preKeys.cend(); ++it) {
        RefCountedPtr<session_pre_key> preKey;
        if (session_pre_key_deserialize(preKey.ptrRef(), bytes(it.value()), size_t(it.value().size()), m_context.get()) < 0) {
            return error(u"Pre key %1 could not be loaded"_s.arg(it.key()));
        }
        keys.insert(it.key(), serializePreKey(ec_key_pair_get_public(session_pre_key_get_key_pair(preKey.get())), m_version));
    }
    return keys;
}

std::variant<Success, QXmppError> SessionLayer::buildSession(const Address &address, const Bundle &bundle)
{
    const auto identityKey = decodeIdentityKey(bundle.identityKey, m_version, m_context.get());
    const auto signedPreKey = decodePreKey(bundle.signedPreKey, m_version, m_context.get());
    const auto preKey = decodePreKey(bundle.preKey, m_version, m_context.get());
    if (!identityKey || !signedPreKey || !preKey) {
        return error(u"Bundle of %1/%2 contains invalid keys"_s.arg(address.jid).arg(address.deviceId));
    }

    RefCountedPtr<session_pre_key_bundle> libBundle;
    if (session_pre_key_bundle_create(libBundle.ptrRef(),
                                      address.deviceId,
                                      0,
                                      bundle.preKeyId,
                                      preKey.get(),
                                      bundle.signedPreKeyId,
                                      signedPreKey.get(),
                                      bytes(bundle.signedPreKeySignature),
                                      size_t(bundle.signedPreKeySignature.size()),
                                      identityKey.get()) < 0) {
        return error(u"Bundle could not be created"_s);
    }

    const LibAddress libAddress(address);
    SessionBuilderPtr builder;
    if (session_builder_create(builder.ptrRef(), m_storeContext.get(), libAddress.get(), m_context.get()) < 0) {
        return error(u"Session builder could not be created"_s);
    }
    session_builder_set_version(builder.get(), libVersion(m_version));

    switch (session_builder_process_pre_key_bundle(builder.get(), libBundle.get())) {
    case SG_SUCCESS:
        return Success();
    case SG_ERR_INVALID_KEY:
        return error(u"Signature of the signed pre key of %1/%2 is invalid"_s.arg(address.jid).arg(address.deviceId));
    default:
        return error(u"Bundle of %1/%2 could not be processed"_s.arg(address.jid).arg(address.deviceId));
    }
}

bool SessionLayer::hasSession(const Address &address) const
{
    return m_keys.sessions.contains(address);
}

std::variant<EncryptedKey, QXmppError> SessionLayer::encrypt(const Address &address, const SecureByteArray &key)
{
    const LibAddress libAddress(address);
    SessionCipherPtr cipher;
    if (session_cipher_create(cipher.ptrRef(), m_storeContext.get(), libAddress.get(), m_context.get()) < 0) {
        return error(u"Session cipher could not be created"_s);
    }
    session_cipher_set_version(cipher.get(), libVersion(m_version));

    RefCountedPtr<ciphertext_message> message;
    if (session_cipher_encrypt(cipher.get(), bytes(key), size_t(key.size()), message.ptrRef()) != SG_SUCCESS) {
        return error(u"Key could not be encrypted for %1/%2"_s.arg(address.jid).arg(address.deviceId));
    }

    auto *serialized = ciphertext_message_get_serialized(message.get());
    // libomemo-c keeps creating key exchanges until the other device answered.
    return EncryptedKey {
        toByteArray(signal_buffer_data(serialized), signal_buffer_len(serialized)),
        ciphertext_message_get_type(message.get()) == CIPHERTEXT_PREKEY_TYPE,
    };
}

std::variant<DecryptedKey, QXmppError> SessionLayer::decrypt(const Address &address, const QByteArray &data, bool isKeyExchange)
{
    const LibAddress libAddress(address);
    SessionCipherPtr cipher;
    if (session_cipher_create(cipher.ptrRef(), m_storeContext.get(), libAddress.get(), m_context.get()) < 0) {
        return error(u"Session cipher could not be created"_s);
    }
    session_cipher_set_version(cipher.get(), libVersion(m_version));

    BufferSecurePtr plaintext;
    DecryptedKey result;
    int status = 0;

    if (isKeyExchange) {
        RefCountedPtr<pre_key_signal_message> message;
        const auto deserialized = m_version == Version::Omemo2
            ? pre_key_signal_message_deserialize_omemo(message.ptrRef(), bytes(data), size_t(data.size()), address.deviceId, m_context.get())
            : pre_key_signal_message_deserialize(message.ptrRef(), bytes(data), size_t(data.size()), m_context.get());
        if (deserialized < 0) {
            return error(u"Key exchange of %1/%2 could not be parsed"_s.arg(address.jid).arg(address.deviceId));
        }
        result.senderIdentityKey = serializeIdentityKey(pre_key_signal_message_get_identity_key(message.get()), m_version);

        m_removedPreKeyId.reset();
        status = session_cipher_decrypt_pre_key_signal_message(cipher.get(), message.get(), nullptr, plaintext.ptrRef());
        result.usedPreKeyId = std::exchange(m_removedPreKeyId, std::nullopt);
    } else {
        RefCountedPtr<signal_message> message;
        const auto deserialized = m_version == Version::Omemo2
            ? signal_message_deserialize_omemo(message.ptrRef(), bytes(data), size_t(data.size()), m_context.get())
            : signal_message_deserialize(message.ptrRef(), bytes(data), size_t(data.size()), m_context.get());
        if (deserialized < 0) {
            return error(u"Message of %1/%2 could not be parsed"_s.arg(address.jid).arg(address.deviceId));
        }
        status = session_cipher_decrypt_signal_message(cipher.get(), message.get(), nullptr, plaintext.ptrRef());
    }

    switch (status) {
    case SG_SUCCESS:
        result.data = toSecureByteArray(signal_buffer_data(plaintext.get()), signal_buffer_len(plaintext.get()));
        return result;
    case SG_ERR_DUPLICATE_MESSAGE:
        return error(u"Message of %1/%2 has already been decrypted"_s.arg(address.jid).arg(address.deviceId));
    case SG_ERR_NO_SESSION:
        return error(u"No session with %1/%2"_s.arg(address.jid).arg(address.deviceId));
    case SG_ERR_INVALID_KEY_ID:
        return error(u"Key exchange of %1/%2 uses an unknown pre key"_s.arg(address.jid).arg(address.deviceId));
    case SG_ERR_LEGACY_MESSAGE:
        return error(u"Message of %1/%2 has an unsupported version"_s.arg(address.jid).arg(address.deviceId));
    default:
        return error(u"Message of %1/%2 could not be decrypted (%3)"_s.arg(address.jid).arg(address.deviceId).arg(status));
    }
}

}  // namespace QXmpp::Omemo::Private

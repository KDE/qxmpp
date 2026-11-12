// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoPayloadCipher.h"

#include "StringLiterals.h"

#include <memory>
#include <openssl/evp.h>

using namespace QXmpp::Private;

namespace QXmpp::Omemo::Private::PayloadCipher {

namespace {

// OMEMO 0
constexpr int GCM_KEY_SIZE = 16;
constexpr int GCM_IV_SIZE = 12;
constexpr int GCM_LEGACY_IV_SIZE = 16;
constexpr int GCM_TAG_SIZE = 16;

// OMEMO 2
constexpr auto HKDF_INFO = "OMEMO Payload";
constexpr int HKDF_KEY_SIZE = 32;
constexpr int HKDF_SALT_SIZE = 32;
constexpr int CBC_KEY_SIZE = 32;
constexpr int HMAC_KEY_SIZE = 32;
constexpr int CBC_IV_SIZE = 16;
constexpr int HKDF_OUTPUT_SIZE = CBC_KEY_SIZE + HMAC_KEY_SIZE + CBC_IV_SIZE;
constexpr int MAC_SIZE = 16;

QXmppError error(QString message)
{
    return QXmppError { std::move(message), {} };
}

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

const unsigned char *bytes(const QByteArray &data)
{
    return reinterpret_cast<const unsigned char *>(data.constData());
}

unsigned char *bytes(QByteArray &data)
{
    return reinterpret_cast<unsigned char *>(data.data());
}

const unsigned char *bytes(const SecureByteArray &data)
{
    return reinterpret_cast<const unsigned char *>(data.constData());
}

CipherContext createGcmContext(bool encrypt, const SecureByteArray &key, const QByteArray &iv)
{
    CipherContext context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_CipherInit_ex(context.get(), EVP_aes_128_gcm(), nullptr, nullptr, nullptr, encrypt) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, int(iv.size()), nullptr) != 1 ||
        EVP_CipherInit_ex(context.get(), nullptr, nullptr, bytes(key), bytes(iv), encrypt) != 1) {
        return CipherContext(nullptr, &EVP_CIPHER_CTX_free);
    }
    return context;
}

std::variant<Encrypted, QXmppError> encryptGcm(const QByteArray &plaintext, const SecureByteArray &key, const QByteArray &iv)
{
    const auto context = createGcmContext(true, key, iv);
    if (!context) {
        return error(u"AES-GCM could not be initialized"_s);
    }

    QByteArray ciphertext(plaintext.size(), Qt::Uninitialized);
    int length = 0;
    int finalLength = 0;
    QByteArray tag(GCM_TAG_SIZE, Qt::Uninitialized);
    if (EVP_CipherUpdate(context.get(), bytes(ciphertext), &length, bytes(plaintext), int(plaintext.size())) != 1 ||
        EVP_CipherFinal_ex(context.get(), bytes(ciphertext) + length, &finalLength) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag.data()) != 1) {
        return error(u"Payload could not be encrypted"_s);
    }

    auto keyMaterial = key;
    keyMaterial.append(SecureByteArray(tag));
    return Encrypted { std::move(keyMaterial), iv, std::move(ciphertext) };
}

std::variant<QByteArray, QXmppError> decryptGcm(const SecureByteArray &keyMaterial, const QByteArray &iv, QByteArray ciphertext)
{
    if (iv.size() != GCM_IV_SIZE && iv.size() != GCM_LEGACY_IV_SIZE) {
        return error(u"Invalid IV size: %1"_s.arg(iv.size()));
    }

    SecureByteArray key(keyMaterial.constData(), GCM_KEY_SIZE);
    QByteArray tag;
    if (keyMaterial.size() == GCM_KEY_SIZE + GCM_TAG_SIZE) {
        tag = QByteArray(keyMaterial.constData() + GCM_KEY_SIZE, GCM_TAG_SIZE);
    } else if (keyMaterial.size() == GCM_KEY_SIZE && ciphertext.size() >= GCM_TAG_SIZE) {
        tag = ciphertext.right(GCM_TAG_SIZE);
        ciphertext.chop(GCM_TAG_SIZE);
    } else {
        return error(u"Invalid key material size: %1"_s.arg(keyMaterial.size()));
    }

    const auto context = createGcmContext(false, key, iv);
    if (!context) {
        return error(u"AES-GCM could not be initialized"_s);
    }

    QByteArray plaintext(ciphertext.size(), Qt::Uninitialized);
    int length = 0;
    int finalLength = 0;
    if (EVP_CipherUpdate(context.get(), bytes(plaintext), &length, bytes(ciphertext), int(ciphertext.size())) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, tag.data()) != 1 ||
        EVP_CipherFinal_ex(context.get(), bytes(plaintext) + length, &finalLength) != 1) {
        return error(u"Payload could not be authenticated"_s);
    }
    return plaintext;
}

struct DerivedKeys {
    QByteArray encryptionKey;
    QByteArray authenticationKey;
    QByteArray iv;
};

std::optional<DerivedKeys> deriveKeys(const SecureByteArray &key)
{
    const auto output = Crypto::hkdfSha256(key.toByteArray(), QByteArray(HKDF_SALT_SIZE, '\0'), QByteArray(HKDF_INFO), HKDF_OUTPUT_SIZE);
    if (output.size() != HKDF_OUTPUT_SIZE) {
        return {};
    }
    return DerivedKeys {
        QByteArray(output.constData(), CBC_KEY_SIZE),
        QByteArray(output.constData() + CBC_KEY_SIZE, HMAC_KEY_SIZE),
        QByteArray(output.constData() + CBC_KEY_SIZE + HMAC_KEY_SIZE, CBC_IV_SIZE),
    };
}

std::variant<Encrypted, QXmppError> encryptCbc(const QByteArray &plaintext, const SecureByteArray &key)
{
    const auto keys = deriveKeys(key);
    if (!keys) {
        return error(u"Key derivation failed"_s);
    }

    auto ciphertext = Crypto::aesCbcEncrypt(plaintext, keys->encryptionKey, keys->iv);
    if (ciphertext.isEmpty()) {
        return error(u"Payload could not be encrypted"_s);
    }

    auto mac = Crypto::hmacSha256(keys->authenticationKey, ciphertext);
    mac.resize(MAC_SIZE);

    auto keyMaterial = key;
    keyMaterial.append(SecureByteArray(mac));
    return Encrypted { std::move(keyMaterial), {}, std::move(ciphertext) };
}

std::variant<QByteArray, QXmppError> decryptCbc(const SecureByteArray &keyMaterial, const QByteArray &ciphertext)
{
    if (keyMaterial.size() != HKDF_KEY_SIZE + MAC_SIZE) {
        return error(u"Invalid key material size: %1"_s.arg(keyMaterial.size()));
    }

    const auto keys = deriveKeys(SecureByteArray(keyMaterial.constData(), HKDF_KEY_SIZE));
    if (!keys) {
        return error(u"Key derivation failed"_s);
    }

    auto mac = Crypto::hmacSha256(keys->authenticationKey, ciphertext);
    mac.resize(MAC_SIZE);
    if (!Crypto::constTimeEqual(mac, QByteArray(keyMaterial.constData() + HKDF_KEY_SIZE, MAC_SIZE))) {
        return error(u"Payload could not be authenticated"_s);
    }

    auto plaintext = Crypto::aesCbcDecrypt(ciphertext, keys->encryptionKey, keys->iv);
    // An empty plaintext results in one block of padding.
    if (plaintext.isEmpty() && ciphertext.size() != CBC_IV_SIZE) {
        return error(u"Payload could not be decrypted"_s);
    }
    return plaintext;
}

}  // namespace

std::variant<Encrypted, QXmppError> encrypt(Version version, const QByteArray &plaintext)
{
    if (version == Version::Omemo0) {
        return encryptGcm(plaintext, SecureByteArray(Crypto::randomBytes(GCM_KEY_SIZE)), Crypto::randomBytes(GCM_IV_SIZE));
    }
    return encryptCbc(plaintext, SecureByteArray(Crypto::randomBytes(HKDF_KEY_SIZE)));
}

std::variant<Encrypted, QXmppError> encrypt(Version version, const QByteArray &plaintext, const SecureByteArray &key, const QByteArray &iv)
{
    if (version == Version::Omemo0) {
        if (key.size() != GCM_KEY_SIZE || iv.size() != GCM_IV_SIZE) {
            return error(u"Invalid key or IV size"_s);
        }
        return encryptGcm(plaintext, key, iv);
    }
    if (key.size() != HKDF_KEY_SIZE || !iv.isEmpty()) {
        return error(u"Invalid key or IV size"_s);
    }
    return encryptCbc(plaintext, key);
}

std::variant<QByteArray, QXmppError> decrypt(Version version, const SecureByteArray &keyMaterial, const QByteArray &iv, const QByteArray &ciphertext)
{
    if (version == Version::Omemo0) {
        return decryptGcm(keyMaterial, iv, ciphertext);
    }
    return decryptCbc(keyMaterial, ciphertext);
}

}  // namespace QXmpp::Omemo::Private::PayloadCipher

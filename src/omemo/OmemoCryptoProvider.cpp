// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoCryptoProvider.h"

#include "QXmppOmemoManager_p.h"
#include "QXmppUtils_p.h"

#include "Crypto.h"
#include "StringLiterals.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

using namespace QXmpp::Private;

inline QXmppOmemoManagerPrivate *managerPrivate(void *ptr)
{
    return reinterpret_cast<QXmppOmemoManagerPrivate *>(ptr);
}

static int random_func(uint8_t *data, size_t len, void *)
{
    generateRandomBytes(data, len);
    return 0;
}

int hmac_sha256_init_func(void **hmac_context, const uint8_t *key, size_t key_len, void *)
{
    auto *mac = new QMessageAuthenticationCode(QCryptographicHash::Sha256,
                                               QByteArray(reinterpret_cast<const char *>(key), key_len));
    *hmac_context = mac;
    return 0;
}

int hmac_sha256_update_func(void *hmac_context, const uint8_t *data, size_t data_len, void *)
{
    auto *mac = reinterpret_cast<QMessageAuthenticationCode *>(hmac_context);
    mac->addData(QByteArrayView(reinterpret_cast<const char *>(data), data_len));
    return 0;
}

int hmac_sha256_final_func(void *hmac_context, signal_buffer **output, void *user_data)
{
    auto *d = managerPrivate(user_data);
    auto *mac = reinterpret_cast<QMessageAuthenticationCode *>(hmac_context);

    auto result = mac->result();
    if (!(*output = signal_buffer_create(reinterpret_cast<const uint8_t *>(result.constData()), result.size()))) {
        d->warning(u"Message authentication code could not be loaded"_s);
        return -1;
    }

    return 0;
}

void hmac_sha256_cleanup_func(void *hmac_context, void *)
{
    auto *mac = reinterpret_cast<QMessageAuthenticationCode *>(hmac_context);
    delete mac;
}

int sha512_digest_init_func(void **digest_context, void *)
{
    *digest_context = new QCryptographicHash(QCryptographicHash::Sha512);
    return 0;
}

int sha512_digest_update_func(void *digest_context, const uint8_t *data, size_t data_len, void *)
{
    auto *hashGenerator = reinterpret_cast<QCryptographicHash *>(digest_context);
    hashGenerator->addData(QByteArrayView(reinterpret_cast<const char *>(data), data_len));
    return 0;
}

int sha512_digest_final_func(void *digest_context, signal_buffer **output, void *user_data)
{
    auto *d = managerPrivate(user_data);
    auto *hashGenerator = reinterpret_cast<QCryptographicHash *>(digest_context);

    auto hash = hashGenerator->result();
    if (!(*output = signal_buffer_create(reinterpret_cast<const uint8_t *>(hash.constData()), hash.size()))) {
        d->warning(u"Hash could not be loaded"_s);
        return -1;
    }

    return 0;
}

void sha512_digest_cleanup_func(void *digest_context, void *)
{
    auto *hashGenerator = reinterpret_cast<QCryptographicHash *>(digest_context);
    delete hashGenerator;
}

int encrypt_func(signal_buffer **output,
                 int cipher,
                 const uint8_t *key, size_t key_len,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *plaintext, size_t plaintext_len,
                 void *user_data)
{
    auto *d = managerPrivate(user_data);

    const auto keyData = QByteArray(reinterpret_cast<const char *>(key), key_len);
    const auto ivData = QByteArray(reinterpret_cast<const char *>(iv), iv_len);
    const auto plaintextData = QByteArray(reinterpret_cast<const char *>(plaintext), plaintext_len);

    QByteArray encryptedData;

    switch (cipher) {
    case SG_CIPHER_AES_CTR_NOPADDING:
        encryptedData = Crypto::aesCtrProcess(plaintextData, keyData, ivData);
        break;
    case SG_CIPHER_AES_CBC_PKCS5:
        encryptedData = Crypto::aesCbcEncrypt(plaintextData, keyData, ivData);
        break;
    default:
        return -2;
    }

    if (encryptedData.isEmpty()) {
        return -3;
    }

    if (!(*output = signal_buffer_create(reinterpret_cast<const uint8_t *>(encryptedData.constData()), encryptedData.size()))) {
        d->warning(u"Encrypted data could not be loaded"_s);
        return -4;
    }

    return 0;
}

int decrypt_func(signal_buffer **output,
                 int cipher,
                 const uint8_t *key, size_t key_len,
                 const uint8_t *iv, size_t iv_len,
                 const uint8_t *ciphertext, size_t ciphertext_len,
                 void *user_data)
{
    auto *d = managerPrivate(user_data);

    const auto keyData = QByteArray(reinterpret_cast<const char *>(key), key_len);
    const auto ivData = QByteArray(reinterpret_cast<const char *>(iv), iv_len);
    const auto ciphertextData = QByteArray(reinterpret_cast<const char *>(ciphertext), ciphertext_len);

    QByteArray decryptedData;

    switch (cipher) {
    case SG_CIPHER_AES_CTR_NOPADDING:
        decryptedData = Crypto::aesCtrProcess(ciphertextData, keyData, ivData);
        break;
    case SG_CIPHER_AES_CBC_PKCS5:
        decryptedData = Crypto::aesCbcDecrypt(ciphertextData, keyData, ivData);
        break;
    default:
        return -2;
    }

    if (decryptedData.isEmpty()) {
        return -3;
    }

    if (!(*output = signal_buffer_create(reinterpret_cast<const uint8_t *>(decryptedData.constData()), decryptedData.size()))) {
        d->warning(u"Decrypted data could not be loaded"_s);
        return -4;
    }

    return 0;
}

namespace QXmpp::Omemo::Private {

signal_crypto_provider createOmemoCryptoProvider(QXmppOmemoManagerPrivate *d)
{
    return {
        random_func,
        hmac_sha256_init_func,
        hmac_sha256_update_func,
        hmac_sha256_final_func,
        hmac_sha256_cleanup_func,
        sha512_digest_init_func,
        sha512_digest_update_func,
        sha512_digest_final_func,
        sha512_digest_cleanup_func,
        encrypt_func,
        decrypt_func,
        d,
    };
}

}  // namespace QXmpp::Omemo::Private

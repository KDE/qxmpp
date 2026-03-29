// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Crypto.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include <QMessageAuthenticationCode>

namespace QXmpp::Private::Crypto {

static const EVP_CIPHER *evpCipher(Cipher cipher)
{
    switch (cipher) {
    case Aes128GcmNoPad:
        return EVP_aes_128_gcm();
    case Aes256GcmNoPad:
        return EVP_aes_256_gcm();
    case Aes256CbcPkcs7:
        return EVP_aes_256_cbc();
    }
    Q_UNREACHABLE();
}

static bool isGcm(Cipher cipher)
{
    return cipher == Aes128GcmNoPad || cipher == Aes256GcmNoPad;
}

static bool isCbc(Cipher cipher)
{
    return cipher == Aes256CbcPkcs7;
}

static QByteArray aesProcess(const QByteArray &data, const QByteArray &key, const QByteArray &iv,
                             const EVP_CIPHER *cipher, bool encrypt, bool padding)
{
    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    if (EVP_CipherInit_ex(ctx, cipher, nullptr,
                          reinterpret_cast<const unsigned char *>(key.constData()),
                          reinterpret_cast<const unsigned char *>(iv.constData()),
                          encrypt ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0);

    QByteArray output(data.size() + EVP_CIPHER_CTX_block_size(ctx), '\0');
    int outLen = 0;

    if (EVP_CipherUpdate(ctx,
                         reinterpret_cast<unsigned char *>(output.data()), &outLen,
                         reinterpret_cast<const unsigned char *>(data.constData()), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int finalLen = 0;
    if (EVP_CipherFinal_ex(ctx,
                           reinterpret_cast<unsigned char *>(output.data()) + outLen, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    output.resize(outLen + finalLen);
    EVP_CIPHER_CTX_free(ctx);
    return output;
}

QByteArray aesGcmEncrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    // GCM encryption without auth tag (matching existing QCA behavior for compatibility)
    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    const auto *evp = key.size() == 16 ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    if (EVP_EncryptInit_ex(ctx, evp, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray output(data.size() + 16, '\0');
    int outLen = 0;

    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(output.data()), &outLen,
                          reinterpret_cast<const unsigned char *>(data.constData()), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Skip EVP_EncryptFinal_ex and tag - matching existing behavior where cipher.final() was
    // skipped for GCM modes. The auth tag is not appended for compatibility with existing
    // encrypted files.
    output.resize(outLen);
    EVP_CIPHER_CTX_free(ctx);
    return output;
}

QByteArray aesGcmDecrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    // GCM decryption without auth tag verification (matching existing QCA behavior)
    auto *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    const auto *evp = key.size() == 16 ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    if (EVP_DecryptInit_ex(ctx, evp, nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray output(data.size(), '\0');
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(output.data()), &outLen,
                          reinterpret_cast<const unsigned char *>(data.constData()), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Skip EVP_DecryptFinal_ex - no auth tag to verify (matching existing behavior)
    output.resize(outLen);
    EVP_CIPHER_CTX_free(ctx);
    return output;
}

static const EVP_CIPHER *aesCbc(int keySize)
{
    switch (keySize) {
    case 16:
        return EVP_aes_128_cbc();
    case 24:
        return EVP_aes_192_cbc();
    case 32:
        return EVP_aes_256_cbc();
    default:
        return nullptr;
    }
}

QByteArray aesCbcEncrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    const auto *evp = aesCbc(key.size());
    if (!evp) {
        return {};
    }
    return aesProcess(data, key, iv, evp, true, true);
}

QByteArray aesCbcDecrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    const auto *evp = aesCbc(key.size());
    if (!evp) {
        return {};
    }
    return aesProcess(data, key, iv, evp, false, true);
}

QByteArray aesCtrProcess(const QByteArray &data, const QByteArray &key, const QByteArray &iv)
{
    const EVP_CIPHER *cipher;
    switch (key.size()) {
    case 16:
        cipher = EVP_aes_128_ctr();
        break;
    case 24:
        cipher = EVP_aes_192_ctr();
        break;
    case 32:
        cipher = EVP_aes_256_ctr();
        break;
    default:
        return {};
    }
    // CTR mode: no padding, encrypt and decrypt are the same operation
    return aesProcess(data, key, iv, cipher, true, false);
}

SecureByteArray hkdfSha256(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int outputLen)
{
    auto *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        return {};
    }

    if (EVP_PKEY_derive_init(pctx) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx,
                                    reinterpret_cast<const unsigned char *>(salt.constData()),
                                    salt.size()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx,
                                   reinterpret_cast<const unsigned char *>(key.constData()),
                                   key.size()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    if (EVP_PKEY_CTX_add1_hkdf_info(pctx,
                                    reinterpret_cast<const unsigned char *>(info.constData()),
                                    info.size()) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    SecureByteArray output(outputLen);
    size_t outLen = outputLen;
    if (EVP_PKEY_derive(pctx,
                        reinterpret_cast<unsigned char *>(output.data()),
                        &outLen) != 1) {
        EVP_PKEY_CTX_free(pctx);
        return {};
    }

    output.resize(int(outLen));
    EVP_PKEY_CTX_free(pctx);
    return output;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QByteArray randomBytes(int count)
{
    QByteArray output(count, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char *>(output.data()), count) != 1) {
        return {};
    }
    return output;
}

// CipherContext

CipherContext::CipherContext(Cipher cipher, Direction direction, const QByteArray &key, const QByteArray &iv)
    : m_cipher(cipher)
{
    m_ctx = EVP_CIPHER_CTX_new();
    if (!m_ctx) {
        return;
    }

    const auto *evp = evpCipher(cipher);
    const int enc = direction == Encrypt ? 1 : 0;

    if (isGcm(cipher)) {
        if (EVP_CipherInit_ex(m_ctx, evp, nullptr, nullptr, nullptr, enc) != 1) {
            EVP_CIPHER_CTX_free(m_ctx);
            m_ctx = nullptr;
            return;
        }
        if (EVP_CIPHER_CTX_ctrl(m_ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
            EVP_CIPHER_CTX_free(m_ctx);
            m_ctx = nullptr;
            return;
        }
        if (EVP_CipherInit_ex(m_ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(iv.constData()),
                              enc) != 1) {
            EVP_CIPHER_CTX_free(m_ctx);
            m_ctx = nullptr;
            return;
        }
    } else {
        if (EVP_CipherInit_ex(m_ctx, evp, nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(iv.constData()),
                              enc) != 1) {
            EVP_CIPHER_CTX_free(m_ctx);
            m_ctx = nullptr;
            return;
        }
    }

    // enable PKCS7 padding for CBC, disable for GCM
    EVP_CIPHER_CTX_set_padding(m_ctx, isCbc(cipher) ? 1 : 0);
}

CipherContext::~CipherContext()
{
    if (m_ctx) {
        EVP_CIPHER_CTX_free(m_ctx);
    }
}

QByteArray CipherContext::update(const QByteArray &data)
{
    if (!m_ctx) {
        return {};
    }

    QByteArray output(data.size() + EVP_CIPHER_CTX_block_size(m_ctx), '\0');
    int outLen = 0;

    if (EVP_CipherUpdate(m_ctx,
                         reinterpret_cast<unsigned char *>(output.data()), &outLen,
                         reinterpret_cast<const unsigned char *>(data.constData()), data.size()) != 1) {
        return {};
    }

    output.resize(outLen);
    return output;
}

QByteArray CipherContext::finalize()
{
    if (!m_ctx) {
        return {};
    }

    // For GCM modes, skip finalization (matching existing behavior)
    if (isGcm(m_cipher)) {
        return {};
    }

    QByteArray output(EVP_CIPHER_CTX_block_size(m_ctx), '\0');
    int outLen = 0;

    if (EVP_CipherFinal_ex(m_ctx,
                           reinterpret_cast<unsigned char *>(output.data()), &outLen) != 1) {
        return {};
    }

    output.resize(outLen);
    return output;
}

bool CipherContext::validKeyLength(int length) const
{
    if (!m_ctx) {
        return false;
    }
    return length == EVP_CIPHER_CTX_key_length(m_ctx);
}

// SecureByteArray

SecureByteArray::SecureByteArray(int size)
    : m_data(size, '\0')
{
}

SecureByteArray::SecureByteArray(const QByteArray &data)
    : m_data(data)
{
}

SecureByteArray::SecureByteArray(const char *data, int size)
    : m_data(data, size)
{
}

SecureByteArray::~SecureByteArray()
{
    cleanse();
}

SecureByteArray::SecureByteArray(const SecureByteArray &other)
    : m_data(other.m_data)
{
}

SecureByteArray &SecureByteArray::operator=(const SecureByteArray &other)
{
    if (this != &other) {
        cleanse();
        m_data = other.m_data;
    }
    return *this;
}

SecureByteArray::SecureByteArray(SecureByteArray &&other) noexcept
    : m_data(std::move(other.m_data))
{
}

SecureByteArray &SecureByteArray::operator=(SecureByteArray &&other) noexcept
{
    if (this != &other) {
        cleanse();
        m_data = std::move(other.m_data);
    }
    return *this;
}

void SecureByteArray::resize(int size)
{
    if (size < m_data.size()) {
        OPENSSL_cleanse(m_data.data() + size, m_data.size() - size);
    }
    Q_ASSERT_X(size <= m_data.size(), "SecureByteArray::resize",
               "Growing a SecureByteArray may leak sensitive data due to reallocation");
    m_data.resize(size);
}

SecureByteArray &SecureByteArray::append(const SecureByteArray &other)
{
    m_data.append(other.m_data);
    return *this;
}

void SecureByteArray::cleanse()
{
    if (!m_data.isEmpty()) {
        OPENSSL_cleanse(m_data.data(), m_data.size());
    }
}

}  // namespace QXmpp::Private::Crypto

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef CRYPTO_H
#define CRYPTO_H

#include "QXmppGlobal.h"

#include <QByteArray>

struct evp_cipher_ctx_st;

namespace QXmpp::Private::Crypto {

enum Direction {
    Encrypt,
    Decrypt,
};

// secure memory wrapper (OPENSSL_cleanse on destruction)
class QXMPP_EXPORT SecureByteArray
{
public:
    SecureByteArray() = default;
    explicit SecureByteArray(int size);
    explicit SecureByteArray(const QByteArray &data);
    SecureByteArray(const char *data, int size);
    ~SecureByteArray();

    SecureByteArray(const SecureByteArray &other);
    SecureByteArray &operator=(const SecureByteArray &other);
    SecureByteArray(SecureByteArray &&other) noexcept;
    SecureByteArray &operator=(SecureByteArray &&other) noexcept;

    char *data() { return m_data.data(); }
    const char *data() const { return m_data.data(); }
    const char *constData() const { return m_data.constData(); }
    int size() const { return m_data.size(); }
    bool isEmpty() const { return m_data.isEmpty(); }

    // Only shrinking is safe. Growing may reallocate without cleansing the old buffer.
    void resize(int size);

    SecureByteArray &append(const SecureByteArray &other);
    QByteArray toByteArray() const { return m_data; }

    bool operator==(const SecureByteArray &other) const { return m_data == other.m_data; }
    bool operator!=(const SecureByteArray &other) const { return m_data != other.m_data; }

private:
    void cleanse();
    QByteArray m_data;
};

// one-shot symmetric encryption/decryption
QXMPP_EXPORT QByteArray aesGcmEncrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
QXMPP_EXPORT QByteArray aesGcmDecrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
QXMPP_EXPORT QByteArray aesCbcEncrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
QXMPP_EXPORT QByteArray aesCbcDecrypt(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
QXMPP_EXPORT QByteArray aesCtrProcess(const QByteArray &data, const QByteArray &key, const QByteArray &iv);

// HKDF-SHA256 key derivation (returns SecureByteArray to protect derived key material)
QXMPP_EXPORT SecureByteArray hkdfSha256(const QByteArray &key, const QByteArray &salt, const QByteArray &info, int outputLen);

// HMAC-SHA256 (uses Qt's QMessageAuthenticationCode)
QXMPP_EXPORT QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);

// secure random bytes
QXMPP_EXPORT QByteArray randomBytes(int count);

// constant-time comparison (prevents timing side-channel attacks)
QXMPP_EXPORT bool constTimeEqual(const QByteArray &a, const QByteArray &b);

// streaming cipher context for QIODevice wrappers
class QXMPP_EXPORT CipherContext
{
public:
    CipherContext(Cipher cipher, Direction direction, const QByteArray &key, const QByteArray &iv);
    ~CipherContext();

    CipherContext(const CipherContext &) = delete;
    CipherContext &operator=(const CipherContext &) = delete;

    QByteArray update(const QByteArray &data);
    QByteArray finalize();
    bool validKeyLength(int length) const;

private:
    evp_cipher_ctx_st *m_ctx = nullptr;
    Cipher m_cipher;
};

}  // namespace QXmpp::Private::Crypto

#endif  // CRYPTO_H

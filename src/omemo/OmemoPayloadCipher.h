// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOPAYLOADCIPHER_H
#define OMEMOPAYLOADCIPHER_H

#include "QXmppError.h"

#include "OmemoSession.h"

#include <variant>

// Symmetric encryption of the payload. The key material is encrypted for each device by the
// session layer.
namespace QXmpp::Omemo::Private::PayloadCipher {

struct Encrypted {
    // encrypted for each device
    SecureByteArray keyMaterial;
    // sent in the clear, only used by OMEMO 0 (OMEMO 2 derives it from the key)
    QByteArray iv;
    QByteArray ciphertext;
};

// OMEMO 0: AES-128-GCM with a random 12 byte IV, the key material is the key and the
// authentication tag (32 bytes).
// OMEMO 2: AES-256-CBC with key, IV and HMAC key derived via HKDF-SHA-256 from a random key, the
// key material is that key and the truncated HMAC (48 bytes).
std::variant<Encrypted, QXmppError> encrypt(Version version, const QByteArray &plaintext);

// Old OMEMO 0 clients send the authentication tag at the end of the ciphertext and only the key
// as key material, and some use 16 byte IVs; both are accepted.
std::variant<QByteArray, QXmppError> decrypt(Version version, const SecureByteArray &keyMaterial, const QByteArray &iv, const QByteArray &ciphertext);

// with given key and IV, for tests
std::variant<Encrypted, QXmppError> encrypt(Version version, const QByteArray &plaintext, const SecureByteArray &key, const QByteArray &iv);

}  // namespace QXmpp::Omemo::Private::PayloadCipher

#endif  // OMEMOPAYLOADCIPHER_H

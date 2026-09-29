// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHASHING_H
#define QXMPPHASHING_H

#include "QXmppError.h"
#include "QXmppGlobal.h"
#include "QXmppHash.h"

#include "Enums.h"

#include <memory>
#include <variant>
#include <vector>

#include <QCryptographicHash>

template<typename T>
class QFuture;
class QXmppHash;

namespace QXmpp::Private {

// XEP-0300 names, which include the IANA Hash Function Textual Names
template<>
struct Enums::Data<HashAlgorithm> {
    using enum HashAlgorithm;
    static inline constexpr auto Values = makeValues<HashAlgorithm>({
        { Unknown, {} },
        { Md2, u"md2" },
        { Md5, u"md5" },
        { Shake128, u"shake128" },
        { Shake256, u"shake256" },
        { Sha1, u"sha-1" },
        { Sha224, u"sha-224" },
        { Sha256, u"sha-256" },
        { Sha384, u"sha-384" },
        { Sha512, u"sha-512" },
        { Sha3_256, u"sha3-256" },
        { Sha3_512, u"sha3-512" },
        { Blake2b_256, u"blake2b-256" },
        { Blake2b_512, u"blake2b-512" },
    });
};

struct HashingResult {
    using Result = std::variant<std::vector<QXmppHash>, Cancelled, QXmppError>;

    HashingResult(Result result, std::unique_ptr<QIODevice> data)
        : result(std::move(result)), data(std::move(data))
    {
    }

    Result result;
    std::unique_ptr<QIODevice> data;
};

struct HashVerificationResult {
    struct NoStrongHashes { };
    struct NotMatching { };
    struct Verified { };
    using Result = std::variant<NoStrongHashes, NotMatching, Verified, Cancelled, QXmppError>;

    HashVerificationResult(Result result, std::unique_ptr<QIODevice> data)
        : result(std::move(result)), data(std::move(data))
    {
    }

    Result result;
    std::unique_ptr<QIODevice> data;
};

using HashingResultPtr = std::shared_ptr<HashingResult>;
using HashVerificationResultPtr = std::shared_ptr<HashVerificationResult>;

std::optional<QCryptographicHash::Algorithm> toQCryptographicHashAlgorithm(HashAlgorithm algorithm);
bool isHashingAlgorithmSecure(HashAlgorithm algorithm);
uint16_t hashPriority(HashAlgorithm algorithm);

// QXMPP_EXPORT for unit tests
QXMPP_EXPORT QFuture<HashingResultPtr> calculateHashes(std::unique_ptr<QIODevice> data, std::vector<HashAlgorithm> hashes);
QFuture<HashVerificationResultPtr> verifyHashes(std::unique_ptr<QIODevice> data, std::vector<QXmppHash> hashes);

}  // namespace QXmpp::Private

#endif  // QXMPPHASHING_H

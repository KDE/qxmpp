// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHASH_H
#define QXMPPHASH_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"

#include <tuple>

#include <QByteArray>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp {

/*!
    \enum QXmpp::HashAlgorithm
    \inmodule QXmpp
    \brief Cryptographic hash algorithm as specified in \xep{0300}{Use of Cryptographic Hash Functions in XMPP}.

    \since QXmpp 1.5

    \value Unknown The algorithm is unknown or not set.
    \value Md2 MD2 hash algorithm.
    \value Md5 MD5 hash algorithm.
    \value Shake128 SHAKE-128 hash algorithm.
    \value Shake256 SHAKE-256 hash algorithm.
    \value Sha1 SHA-1 hash algorithm.
    \value Sha224 SHA-224 hash algorithm.
    \value Sha256 SHA-256 hash algorithm.
    \value Sha384 SHA-384 hash algorithm.
    \value Sha512 SHA-512 hash algorithm.
    \value Sha3_256 SHA3-256 hash algorithm.
    \value Sha3_512 SHA3-512 hash algorithm.
    \value Blake2b_256 BLAKE2b-256 hash algorithm.
    \value Blake2b_512 BLAKE2b-512 hash algorithm.
*/
enum class HashAlgorithm : uint32_t {
    Unknown,
    Md2,
    Md5,
    Shake128,
    Shake256,
    Sha1,
    Sha224,
    Sha256,
    Sha384,
    Sha512,
    Sha3_256,
    Sha3_512,
    Blake2b_256,
    Blake2b_512,
};

}  // namespace QXmpp

class QXMPP_EXPORT QXmppHash
{
public:
    QXmppHash();

    static constexpr std::tuple XmlTag = { u"hash", QXmpp::Private::ns_hashes };
    bool parse(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;

    QXmpp::HashAlgorithm algorithm() const;
    void setAlgorithm(QXmpp::HashAlgorithm algorithm);

    QByteArray hash() const;
    void setHash(const QByteArray &data);

private:
    QXmpp::HashAlgorithm m_algorithm = QXmpp::HashAlgorithm::Unknown;
    QByteArray m_hash;
};

class QXMPP_EXPORT QXmppHashUsed
{
public:
    QXmppHashUsed();
    QXmppHashUsed(QXmpp::HashAlgorithm algorithm);

    static constexpr auto XmlTag = std::tuple { u"hash-used", QXmpp::Private::ns_hashes };
    bool parse(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;

    QXmpp::HashAlgorithm algorithm() const;
    void setAlgorithm(QXmpp::HashAlgorithm algorithm);

private:
    QXmpp::HashAlgorithm m_algorithm = QXmpp::HashAlgorithm::Unknown;
};

#endif  // QXMPPHASH_H

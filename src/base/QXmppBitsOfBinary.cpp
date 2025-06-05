// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryDataList.h"
#include "QXmppBitsOfBinaryIq.h"
#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QMimeDatabase>
#include <QSharedData>

using namespace QXmpp::Private;

class QXmppBitsOfBinaryDataPrivate : public QSharedData
{
public:
    QXmppBitsOfBinaryDataPrivate();

    QXmppBitsOfBinaryContentId cid;
    int maxAge;
    QMimeType contentType;
    QByteArray data;
};

QXmppBitsOfBinaryDataPrivate::QXmppBitsOfBinaryDataPrivate()
    : maxAge(-1)
{
}

///
/// \class QXmppBitsOfBinaryData
///
/// QXmppBitsOfBinaryData represents a data element for \xep{0231, Bits of
/// Binary}. It can be used as an extension in other stanzas.
///
/// \see QXmppBitsOfBinaryIq, QXmppBitsOfBinaryDataList
///
/// \since QXmpp 1.2
///

///
/// Creates bits of binary data from a QByteArray.
///
/// This hashes the data to generate a content ID. The MIME type is not set.
///
/// \note This blocks while hashing the data. You may want to run this via QtConcurrent or a
/// QThreadPool to run this on for large amounts of data.
///
/// \since QXmpp 1.5
///
QXmppBitsOfBinaryData QXmppBitsOfBinaryData::fromByteArray(QByteArray data)
{
    QXmppBitsOfBinaryContentId cid;
    cid.setHash(QCryptographicHash::hash(data, QCryptographicHash::Sha1));
    cid.setAlgorithm(QCryptographicHash::Sha1);

    QXmppBitsOfBinaryData bobData;
    bobData.d->cid = std::move(cid);
    bobData.d->data = std::move(data);

    return bobData;
}

///
/// Default constructor
///
QXmppBitsOfBinaryData::QXmppBitsOfBinaryData()
    : d(new QXmppBitsOfBinaryDataPrivate)
{
}

/// Default copy-constructor
QXmppBitsOfBinaryData::QXmppBitsOfBinaryData(const QXmppBitsOfBinaryData &) = default;
/// Default move-constructor
QXmppBitsOfBinaryData::QXmppBitsOfBinaryData(QXmppBitsOfBinaryData &&) = default;
/// Default destructor
QXmppBitsOfBinaryData::~QXmppBitsOfBinaryData() = default;
/// Default assignment operator
QXmppBitsOfBinaryData &QXmppBitsOfBinaryData::operator=(const QXmppBitsOfBinaryData &) = default;
/// Default move-assignment operator
QXmppBitsOfBinaryData &QXmppBitsOfBinaryData::operator=(QXmppBitsOfBinaryData &&) = default;

///
/// Returns the content id of the data
///
QXmppBitsOfBinaryContentId QXmppBitsOfBinaryData::cid() const
{
    return d->cid;
}

///
/// Sets the content id of the data
///
void QXmppBitsOfBinaryData::setCid(const QXmppBitsOfBinaryContentId &cid)
{
    d->cid = cid;
}

///
/// Returns the time in seconds the data should be cached
///
/// A value of 0 means that the data should not be cached, while a value of -1
/// means that nothing was set.
///
/// The default value is -1.
///
int QXmppBitsOfBinaryData::maxAge() const
{
    return d->maxAge;
}

///
/// Sets the time in seconds the data should be cached
///
/// A value of 0 means that the data should not be cached, while a value of -1
/// means that nothing was set.
///
/// The default value is -1.
///
void QXmppBitsOfBinaryData::setMaxAge(int maxAge)
{
    d->maxAge = maxAge;
}

///
/// Returns the content type of the data
///
/// \note This is the advertised content type and may differ from the actual
/// content type of the data.
///
QMimeType QXmppBitsOfBinaryData::contentType() const
{
    return d->contentType;
}

///
/// Sets the content type of the data
///
void QXmppBitsOfBinaryData::setContentType(const QMimeType &contentType)
{
    d->contentType = contentType;
}

///
/// Returns the included data in binary form
///
QByteArray QXmppBitsOfBinaryData::data() const
{
    return d->data;
}

///
/// Sets the data in binary form
///
void QXmppBitsOfBinaryData::setData(const QByteArray &data)
{
    d->data = data;
}

///
/// Returns true, if \c element is a \xep{0231, Bits of Binary} data element
///
bool QXmppBitsOfBinaryData::isBitsOfBinaryData(const QDomElement &element)
{
    return element.tagName() == u"data" && element.namespaceURI() == ns_bob;
}

/// \cond
void QXmppBitsOfBinaryData::parseElementFromChild(const QDomElement &dataElement)
{
    d->cid = QXmppBitsOfBinaryContentId::fromContentId(dataElement.attribute(u"cid"_s));
    d->maxAge = dataElement.attribute(u"max-age"_s, u"-1"_s).toInt();
    d->contentType = QMimeDatabase().mimeTypeForName(dataElement.attribute(u"type"_s));
    d->data = QByteArray::fromBase64(dataElement.text().toUtf8());
}

void QXmppBitsOfBinaryData::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    auto maxAge = d->maxAge > -1 ? std::make_optional(d->maxAge) : std::nullopt;

    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalAttribute { u"cid", d->cid.toContentId() },
        OptionalAttribute { u"max-age", maxAge },
        OptionalAttribute { u"type", d->contentType.name() },
        Characters { Base64 { d->data } },
    });
}
/// \endcond

///
/// Returns true, if cid, maxAge, contentType and data equal.
///
bool QXmppBitsOfBinaryData::operator==(const QXmppBitsOfBinaryData &other) const
{
    return d->cid == other.cid() &&
        d->maxAge == other.maxAge() &&
        d->contentType == other.contentType() &&
        d->data == other.data();
}

///
/// \class QXmppBitsOfBinaryDataList
///
/// QXmppBitsOfBinaryDataList represents a list of data elements from
/// \xep{0231, Bits of Binary}.
///
/// \since QXmpp 1.2
///

QXmppBitsOfBinaryDataList::QXmppBitsOfBinaryDataList() = default;

QXmppBitsOfBinaryDataList::~QXmppBitsOfBinaryDataList() = default;

/// \cond
void QXmppBitsOfBinaryDataList::parse(const QDomElement &element)
{
    // clear previous data elements
    clear();

    // parse all <data/> elements
    for (const auto &dataEl : iterChildElements<QXmppBitsOfBinaryData>(element)) {
        QXmppBitsOfBinaryData data;
        data.parseElementFromChild(dataEl);
        append(std::move(data));
    }
}

void QXmppBitsOfBinaryDataList::toXml(QXmlStreamWriter *writer) const
{
    for (const auto &bitsOfBinaryData : *this) {
        bitsOfBinaryData.toXmlElementFromChild(writer);
    }
}
/// \endcond

///
/// \class QXmppBitsOfBinaryIq
///
/// QXmppBitsOfBinaryIq represents a \xep{0231, Bits of Binary} IQ to request
/// and transmit Bits of Binary data elements.
///
/// \since QXmpp 1.2
///

QXmppBitsOfBinaryIq::QXmppBitsOfBinaryIq() = default;

QXmppBitsOfBinaryIq::~QXmppBitsOfBinaryIq() = default;

/// \cond
void QXmppBitsOfBinaryIq::parseElementFromChild(const QDomElement &element)
{
    auto child = firstChildElement(element, u"data", ns_bob);
    if (!child.isNull()) {
        QXmppBitsOfBinaryData::parseElementFromChild(child);
    }
}

void QXmppBitsOfBinaryIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    QXmppBitsOfBinaryData::toXmlElementFromChild(writer);
}
/// \endcond

#define CONTENTID_URL u"cid:"_s
#define CONTENTID_URL_LENGTH 4
#define CONTENTID_POSTFIX u"@bob.xmpp.org"_s
#define CONTENTID_POSTFIX_LENGTH 13
#define CONTENTID_HASH_SEPARATOR u"+"_s

static const QMap<QCryptographicHash::Algorithm, QStringView> HASH_ALGORITHMS = {
    { QCryptographicHash::Sha1, u"sha1" },
    { QCryptographicHash::Md4, u"md4" },
    { QCryptographicHash::Md5, u"md5" },
    { QCryptographicHash::Sha224, u"sha-224" },
    { QCryptographicHash::Sha256, u"sha-256" },
    { QCryptographicHash::Sha384, u"sha-384" },
    { QCryptographicHash::Sha512, u"sha-512" },
    { QCryptographicHash::Sha3_224, u"sha3-224" },
    { QCryptographicHash::Sha3_256, u"sha3-256" },
    { QCryptographicHash::Sha3_384, u"sha3-384" },
    { QCryptographicHash::Sha3_512, u"sha3-512" },
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    { QCryptographicHash::Blake2b_256, u"blake2b-256" },
    { QCryptographicHash::Blake2b_512, u"blake2b-512" },
#endif
};

class QXmppBitsOfBinaryContentIdPrivate : public QSharedData
{
public:
    QXmppBitsOfBinaryContentIdPrivate();

    QCryptographicHash::Algorithm algorithm;
    QByteArray hash;
};

QXmppBitsOfBinaryContentIdPrivate::QXmppBitsOfBinaryContentIdPrivate()
    : algorithm(QCryptographicHash::Sha1)
{
}

///
/// \class QXmppBitsOfBinaryContentId
///
/// QXmppBitsOfBinaryContentId represents a link to or an identifier of
/// \xep{0231, Bits of Binary} data.
///
/// Currently supported hash algorithms:
///  * MD4
///  * MD5
///  * SHA-1
///  * SHA-2 (SHA-224, SHA-256, SHA-384, SHA-512)
///  * SHA-3 (SHA3-224, SHA3-256, SHA3-384, SHA3-512)
///  * BLAKE2 (BLAKE2b256, BLAKE2b512) (requires Qt 6, since QXmpp 1.5)
///
/// \note Security notice: When using the content IDs to cache data between multiple entities it is
/// important to avoid hash collisions. SHA-1 cannot fulfill this requirement. You SHOULD use
/// another more secure hash algorithm if you do this.
///
/// \since QXmpp 1.2
///

///
/// Parses a \c QXmppBitsOfBinaryContentId from a \xep{0231, Bits of Binary}
/// \c cid: URL
///
/// In case parsing failed, the returned \c QXmppBitsOfBinaryContentId is
/// empty.
///
/// \see QXmppBitsOfBinaryContentId::fromContentId
///
QXmppBitsOfBinaryContentId QXmppBitsOfBinaryContentId::fromCidUrl(const QString &input)
{
    if (input.startsWith(CONTENTID_URL)) {
        return fromContentId(input.mid(CONTENTID_URL_LENGTH));
    }

    return {};
}

///
/// Parses a \c QXmppBitsOfBinaryContentId from a \xep{0231, Bits of Binary}
/// content id
///
/// In case parsing failed, the returned \c QXmppBitsOfBinaryContentId is
/// empty.
///
/// \note This does not allow \c cid: URLs to be passed. Use
/// \c QXmppBitsOfBinaryContentId::fromCidUrl for that purpose.
///
/// \see QXmppBitsOfBinaryContentId::fromCidUrl
///
QXmppBitsOfBinaryContentId QXmppBitsOfBinaryContentId::fromContentId(const QString &input)
{
    if (input.startsWith(CONTENTID_URL) || !input.endsWith(CONTENTID_POSTFIX)) {
        return {};
    }

    // remove '@bob.xmpp.org'
    QString hashAndAlgoStr = input.left(input.size() - CONTENTID_POSTFIX_LENGTH);
    // get size of hash algo id
    QStringList algoAndHash = hashAndAlgoStr.split(CONTENTID_HASH_SEPARATOR);
    if (algoAndHash.size() != 2) {
        return {};
    }

    QCryptographicHash::Algorithm algo = HASH_ALGORITHMS.key(algoAndHash.first(), QCryptographicHash::Algorithm(-1));
    if (int(algo) == -1) {
        return {};
    }

    QXmppBitsOfBinaryContentId cid;
    cid.setAlgorithm(algo);
    cid.setHash(QByteArray::fromHex(algoAndHash.last().toUtf8()));

    return cid;
}

///
/// Default contructor
///
QXmppBitsOfBinaryContentId::QXmppBitsOfBinaryContentId()
    : d(new QXmppBitsOfBinaryContentIdPrivate)
{
}

///
/// Returns true, if two \c QXmppBitsOfBinaryContentId equal
///
bool QXmppBitsOfBinaryContentId::operator==(const QXmppBitsOfBinaryContentId &other) const
{
    return d->algorithm == other.algorithm() && d->hash == other.hash();
}

/// Default destructor
QXmppBitsOfBinaryContentId::~QXmppBitsOfBinaryContentId() = default;
/// Default copy-constructor
QXmppBitsOfBinaryContentId::QXmppBitsOfBinaryContentId(const QXmppBitsOfBinaryContentId &cid) = default;
/// Default move-constructor
QXmppBitsOfBinaryContentId::QXmppBitsOfBinaryContentId(QXmppBitsOfBinaryContentId &&cid) = default;
/// Default assignment operator
QXmppBitsOfBinaryContentId &QXmppBitsOfBinaryContentId::operator=(const QXmppBitsOfBinaryContentId &other) = default;
/// Default move-assignment operator
QXmppBitsOfBinaryContentId &QXmppBitsOfBinaryContentId::operator=(QXmppBitsOfBinaryContentId &&other) = default;

///
/// Returns a \xep{0231, Bits of Binary} content id
///
QString QXmppBitsOfBinaryContentId::toContentId() const
{
    if (!isValid()) {
        return {};
    }

    return HASH_ALGORITHMS.value(d->algorithm) +
        CONTENTID_HASH_SEPARATOR +
        QString::fromUtf8(d->hash.toHex()) +
        CONTENTID_POSTFIX;
}

///
/// Returns a \xep{0231, Bits of Binary} \c cid: URL
///
QString QXmppBitsOfBinaryContentId::toCidUrl() const
{
    if (!isValid()) {
        return {};
    }

    return toContentId().prepend(CONTENTID_URL);
}

///
/// Returns the hash value in binary form
///
QByteArray QXmppBitsOfBinaryContentId::hash() const
{
    return d->hash;
}

///
/// Sets the hash value in binary form
///
void QXmppBitsOfBinaryContentId::setHash(const QByteArray &hash)
{
    d->hash = hash;
}

///
/// Returns the hash algorithm used to calculate the \c hash value
///
/// The default value is \c QCryptographicHash::Sha1.
///
QCryptographicHash::Algorithm QXmppBitsOfBinaryContentId::algorithm() const
{
    return d->algorithm;
}

///
/// Sets the hash algorithm used to calculate the \c hash value
///
/// The default value is \c QCryptographicHash::Sha1.
///
/// \note Only change this, if you know what you do. The XEP allows other
/// hashing algorithms than SHA-1 to be used, but not all clients support this.
///
void QXmppBitsOfBinaryContentId::setAlgorithm(QCryptographicHash::Algorithm algo)
{
    d->algorithm = algo;
}

///
/// Checks whether the content id is valid and can be serialized into a string.
///
/// Also checks the length of the hash.
///
/// \returns True, if the set hashing algorithm is supported, a hash value is
/// set and its length is correct, false otherwise.
///
bool QXmppBitsOfBinaryContentId::isValid() const
{
    return !d->hash.isEmpty() &&
        HASH_ALGORITHMS.contains(d->algorithm) &&
        d->hash.length() == QCryptographicHash::hashLength(d->algorithm);
}

///
/// Checks whether \c input is a Bits of Binary content id or \c cid: URL
///
/// \param input The string to be checked.
/// \param checkIsCidUrl If true, it only accepts \c cid: URLs.
///
/// \returns True, if \c input is valid.
///
bool QXmppBitsOfBinaryContentId::isBitsOfBinaryContentId(const QString &input, bool checkIsCidUrl)
{
    return input.endsWith(CONTENTID_POSTFIX) &&
        input.contains(CONTENTID_HASH_SEPARATOR) &&
        (!checkIsCidUrl || input.startsWith(CONTENTID_URL));
}

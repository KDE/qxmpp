// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppEncryptedFileSource.h"

#include "QXmppConstants_p.h"
#include "QXmppHttpFileSource.h"
#include "QXmppUtils_p.h"

#include "Enums.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <optional>

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp;
using namespace QXmpp::Private;

template<>
struct Enums::Data<Cipher> {
    static constexpr auto Values = makeValues<Cipher>({
        { Aes128GcmNoPad, u"urn:xmpp:ciphers:aes-128-gcm-nopadding:0" },
        { Aes256GcmNoPad, u"urn:xmpp:ciphers:aes-256-gcm-nopadding:0" },
        { Aes256CbcPkcs7, u"urn:xmpp:ciphers:aes-256-cbc-pkcs7:0" },
    });
};

class QXmppEncryptedFileSourcePrivate : public QSharedData
{
public:
    Cipher cipher = Aes128GcmNoPad;
    QByteArray key;
    QByteArray iv;
    QVector<QXmppHash> hashes;
    QVector<QXmppHttpFileSource> httpSources;
};

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppEncryptedFileSource)

///
/// \class QXmppEncryptedFileSource
///
/// \brief Represents an encrypted file source for file sharing.
///
/// \since QXmpp 1.5
///

QXmppEncryptedFileSource::QXmppEncryptedFileSource()
    : d(new QXmppEncryptedFileSourcePrivate())
{
}

/// Returns the cipher that was used to encrypt the data in this file source
Cipher QXmppEncryptedFileSource::cipher() const
{
    return d->cipher;
}

/// Sets the cipher that was used to encrypt the data in this file source
void QXmppEncryptedFileSource::setCipher(Cipher newCipher)
{
    d->cipher = newCipher;
}

/// Returns the key that can be used to decrypt the data in this file source
const QByteArray &QXmppEncryptedFileSource::key() const
{
    return d->key;
}

/// Sets the key that was used to encrypt the data in this file source
void QXmppEncryptedFileSource::setKey(const QByteArray &newKey)
{
    d->key = newKey;
}

/// Returns the Initialization vector that can be used to decrypt the data in this file source
const QByteArray &QXmppEncryptedFileSource::iv() const
{
    return d->iv;
}

/// Sets the initialization vector that was used to encrypt the data in this file source
void QXmppEncryptedFileSource::setIv(const QByteArray &newIv)
{
    d->iv = newIv;
}

/// Returns the hashes of the file contained in this file source
const QVector<QXmppHash> &QXmppEncryptedFileSource::hashes() const
{
    return d->hashes;
}

/// Sets the hashes of the file contained in this file source
void QXmppEncryptedFileSource::setHashes(const QVector<QXmppHash> &newHashes)
{
    d->hashes = newHashes;
}

/// Returns the http sources that can be used to retrieve the encrypted data
const QVector<QXmppHttpFileSource> &QXmppEncryptedFileSource::httpSources() const
{
    return d->httpSources;
}

/// Sets the http sources containing the encrypted data
void QXmppEncryptedFileSource::setHttpSources(const QVector<QXmppHttpFileSource> &newHttpSources)
{
    d->httpSources = newHttpSources;
}

/// \cond
bool QXmppEncryptedFileSource::parse(const QDomElement &el)
{
    if (auto parsedCipher = Enums::fromString<Cipher>(el.attribute(u"cipher"_s))) {
        d->cipher = *parsedCipher;
    } else {
        return false;
    }

    auto keyEl = el.firstChildElement(u"key"_s);
    if (keyEl.isNull()) {
        return false;
    }
    if (auto data = parseBase64(keyEl.text())) {
        d->key = std::move(*data);
    } else {
        return false;
    }

    auto ivEl = el.firstChildElement(u"iv"_s);
    if (ivEl.isNull()) {
        return false;
    }
    if (auto data = parseBase64(ivEl.text())) {
        d->iv = std::move(*data);
    } else {
        return false;
    }
    d->hashes = parseChildElements<QVector<QXmppHash>>(el);

    auto sourcesEl = el.firstChildElement(u"sources"_s);
    if (sourcesEl.isNull()) {
        return false;
    }
    d->httpSources = parseChildElements<QVector<QXmppHttpFileSource>>(sourcesEl);

    return true;
}

void QXmppEncryptedFileSource::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        Attribute { u"cipher", d->cipher },
        TextElement { u"key"_s, Base64 { d->key } },
        TextElement { u"iv"_s, Base64 { d->iv } },
        d->hashes,
        Element {
            { u"sources", ns_sfs },
            d->httpSources,
        },
    });
}
/// \endcond

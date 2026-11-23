// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppFileShare.h"

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryData.h"
#include "QXmppConstants_p.h"
#include "QXmppEncryptedFileSource.h"
#include "QXmppFileMetadata.h"
#include "QXmppFileShare_p.h"
#include "QXmppHttpFileSource.h"
#include "QXmppThumbnail.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <optional>

#include <QDomElement>
#include <QMimeDatabase>
#include <QUrl>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;
using Disposition = QXmppFileShare::Disposition;

namespace QXmpp::Private {

template<>
struct Enums::Data<Disposition> {
    using enum Disposition;
    static constexpr auto Values = makeValues<Disposition>({
        { Inline, u"inline" },
        { Attachment, u"attachment" },
    });
};

struct FileSources {
    static FileSources fromDom(const QDomElement &el);
    void toXml(XmlWriter &writer) const;

    QList<QXmppHttpFileSource> httpSources;
    QList<QXmppEncryptedFileSource> encryptedSources;
};

FileSources FileSources::fromDom(const QDomElement &el)
{
    return {
        parseChildElements<QList<QXmppHttpFileSource>>(el),
        parseChildElements<QList<QXmppEncryptedFileSource>>(el),
    };
}

// only inner (without <sources/> element)
void FileSources::toXml(XmlWriter &writer) const
{
    writer.write(httpSources);
    writer.write(encryptedSources);
}

}  // namespace QXmpp::Private

class QXmppFileSourcesAttachmentPrivate : public QSharedData
{
public:
    QString id;
    FileSources sources;
};

/*!
    \class QXmppFileSourcesAttachment
    \inmodule QXmpp

    Attachment of file sources to a previous file sharing element from \xep{0447}{Stateless file
    sharing}.

    \since QXmpp 1.7
*/

/*! Default constructor */
QXmppFileSourcesAttachment::QXmppFileSourcesAttachment()
    : d(new QXmppFileSourcesAttachmentPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppFileSourcesAttachment)

/*! Returns the ID of the referenced file-sharing element. */
const QString &QXmppFileSourcesAttachment::id() const
{
    return d->id;
}

/*!
    Sets the ID of the referenced file-sharing element.

    \a id.
*/
void QXmppFileSourcesAttachment::setId(const QString &id)
{
    d->id = id;
}

/*! Returns the HTTP sources for this file. */
const QList<QXmppHttpFileSource> &QXmppFileSourcesAttachment::httpSources() const
{
    return d->sources.httpSources;
}

/*!
    Sets the HTTP sources for this file.

    \a newHttpSources.
*/
void QXmppFileSourcesAttachment::setHttpSources(const QList<QXmppHttpFileSource> &newHttpSources)
{
    d->sources.httpSources = newHttpSources;
}

/*! Returns the encrypted sources for this file. */
const QList<QXmppEncryptedFileSource> &QXmppFileSourcesAttachment::encryptedSources() const
{
    return d->sources.encryptedSources;
}

/*!
    Sets the encrypted sources for this file.

    \a newEncryptedSources.
*/
void QXmppFileSourcesAttachment::setEncryptedSources(const QList<QXmppEncryptedFileSource> &newEncryptedSources)
{
    d->sources.encryptedSources = newEncryptedSources;
}

std::optional<QXmppFileSourcesAttachment> QXmppFileSourcesAttachment::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"sources" || el.namespaceURI() != ns_sfs) {
        return {};
    }
    QXmppFileSourcesAttachment result;
    result.d->id = el.attribute(u"id"_s);
    result.d->sources = FileSources::fromDom(el);
    return result;
}

/*!
    Serialize to XML

    \a writer.
*/
void QXmppFileSourcesAttachment::toXml(QXmpp::Private::XmlWriter &writer) const
{
    writer.write(Element {
        { u"sources", ns_sfs },
        Attribute { u"id", d->id },
        d->sources,
    });
}

class QXmppFileSharePrivate : public QSharedData
{
public:
    QXmppFileMetadata metadata;
    QString id;
    FileSources sources;
    std::optional<QXmppFileShare::Disposition> disposition;
};

/*!
    \class QXmppFileShare
    \inmodule QXmpp

    File sharing element from \xep{0447}{Stateless file sharing}. Contains
    metadata and source URLs.

    \note jinglepub references are currently missing

    \since QXmpp 1.5
*/

/*!
    \enum QXmppFileShare::Disposition

    \brief Decides whether to display the file contents (e.g. an image) inline in the chat or as
    a file.

    \value Inline The file should be displayed inline in the conversation.
    \value Attachment The file should be offered as a downloadable attachment.
*/

/*! Default constructor */
QXmppFileShare::QXmppFileShare()
    : d(new QXmppFileSharePrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppFileShare)

/*!
    Returns the disposition setting for this file.

    \deprecated Use dispositionOpt() instead, which distinguishes "no \c disposition attribute"
    from an explicit value. This getter returns \c Inline when no disposition was set.
*/
QXmppFileShare::Disposition QXmppFileShare::disposition() const
{
    return d->disposition.value_or(Disposition::Inline);
}

/*!
    Returns the disposition setting for this file.

    \c std::nullopt means no \c disposition attribute was set.

    \since QXmpp 1.16
*/
std::optional<QXmppFileShare::Disposition> QXmppFileShare::dispositionOpt() const
{
    return d->disposition;
}

/*!
    Sets the disposition setting for this file.

    \a disp.
*/
void QXmppFileShare::setDisposition(std::optional<Disposition> disp)
{
    d->disposition = disp;
}

/*!
    Returns the ID of this file element.

    This is useful for attaching sources to one of multiple files in a message.

    \since QXmpp 1.7
*/
const QString &QXmppFileShare::id() const
{
    return d->id;
}

/*!
    Sets the ID of this file element.

    This is useful for attaching sources to one of multiple files in a message.

    \since QXmpp 1.7

    \a id.
*/
void QXmppFileShare::setId(const QString &id)
{
    d->id = id;
}

/*! Returns the metadata of the shared file. */
const QXmppFileMetadata &QXmppFileShare::metadata() const
{
    return d->metadata;
}

/*! Sets the \a metadata of the shared file. */
void QXmppFileShare::setMetadata(const QXmppFileMetadata &metadata)
{
    d->metadata = metadata;
}

/*! Returns the HTTP sources for this file. */
const QList<QXmppHttpFileSource> &QXmppFileShare::httpSources() const
{
    return d->sources.httpSources;
}

/*!
    Sets the HTTP sources for this file.

    \a newHttpSources.
*/
void QXmppFileShare::setHttpSources(const QList<QXmppHttpFileSource> &newHttpSources)
{
    d->sources.httpSources = newHttpSources;
}

/*! Returns the encrypted sources for this file. */
const QList<QXmppEncryptedFileSource> &QXmppFileShare::encryptedSources() const
{
    return d->sources.encryptedSources;
}

/*!
    Sets the encrypted sources for this file.

    \a newEncryptedSources.
*/
void QXmppFileShare::setEncryptedSourecs(const QList<QXmppEncryptedFileSource> &newEncryptedSources)
{
    d->sources.encryptedSources = newEncryptedSources;
}

void QXmppFileShare::visitSources(std::function<bool(const std::any &)> &&visitor) const
{
    for (const auto &httpSource : d->sources.httpSources) {
        if (visitor(httpSource)) {
            return;
        }
    }
    for (const auto &encryptedSource : d->sources.encryptedSources) {
        if (visitor(encryptedSource)) {
            return;
        }
    }
}

void QXmppFileShare::addSource(const std::any &source)
{
    if (source.type() == typeid(QXmppHttpFileSource)) {
        d->sources.httpSources.push_back(std::any_cast<QXmppHttpFileSource>(source));
    } else if (source.type() == typeid(QXmppEncryptedFileSource)) {
        d->sources.encryptedSources.push_back(std::any_cast<QXmppEncryptedFileSource>(source));
    }
}

bool QXmppFileShare::parse(const QDomElement &el)
{
    if (elementXmlTag(el) == XmlTag) {
        // disposition
        d->disposition = Enums::fromString<Disposition>(el.attribute(u"disposition"_s));
        d->id = el.attribute(u"id"_s);

        // file metadata
        if (auto metadata = parseElement<QXmppFileMetadata>(firstChildElement(el, u"file"))) {
            d->metadata = std::move(*metadata);
        } else {
            return false;
        }

        // sources
        if (auto sourcesEl = firstChildElement(el, u"sources", ns_sfs); !sourcesEl.isNull()) {
            d->sources = FileSources::fromDom(sourcesEl);
        }
        return true;
    }
    return false;
}

void QXmppFileShare::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalAttribute { u"disposition", d->disposition },
        OptionalAttribute { u"id", d->id },
        d->metadata,
        Element { u"sources", d->sources },
    });
}

namespace QXmpp::Private::OmemoMediaSharing {

constexpr qsizetype KEY_SIZE = 32;
constexpr qsizetype IV_SIZE = 12;
// used by early implementations, still accepted by most clients
constexpr qsizetype LEGACY_IV_SIZE = 16;

constexpr QStringView AESGCM_SCHEME = u"aesgcm";
constexpr QStringView HTTPS_SCHEME = u"https";

std::optional<QString> aesGcmUrl(const QXmppEncryptedFileSource &source)
{
    if (source.cipher() != Aes256GcmNoPad || source.key().size() != KEY_SIZE ||
        (source.iv().size() != IV_SIZE && source.iv().size() != LEGACY_IV_SIZE)) {
        return {};
    }
    for (const auto &httpSource : source.httpSources()) {
        auto url = httpSource.url();
        if (!url.isValid() || url.scheme() != HTTPS_SCHEME || url.host().isEmpty()) {
            continue;
        }
        url.setScheme(AESGCM_SCHEME.toString());
        url.setFragment(QString::fromLatin1(QByteArray(source.iv() + source.key()).toHex()));
        return url.toString(QUrl::FullyEncoded);
    }
    return {};
}

static std::optional<QByteArray> parseHex(QStringView text)
{
    for (auto c : text) {
        if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F'))) {
            return {};
        }
    }
    return QByteArray::fromHex(text.toLatin1());
}

static std::optional<QXmppEncryptedFileSource> parseUrl(QStringView text)
{
    QUrl url(text.toString(), QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != AESGCM_SCHEME || url.host().isEmpty() || !url.hasFragment()) {
        return {};
    }
    // key and IV must not be decoded from percent-encodings
    auto fragment = url.fragment(QUrl::FullyEncoded);
    if (fragment.size() != 2 * (IV_SIZE + KEY_SIZE) && fragment.size() != 2 * (LEGACY_IV_SIZE + KEY_SIZE)) {
        return {};
    }
    auto ivAndKey = parseHex(fragment);
    if (!ivAndKey) {
        return {};
    }

    url.setScheme(HTTPS_SCHEME.toString());
    url.setFragment({});

    QXmppEncryptedFileSource source;
    source.setCipher(Aes256GcmNoPad);
    source.setIv(ivAndKey->left(ivAndKey->size() - KEY_SIZE));
    source.setKey(ivAndKey->right(KEY_SIZE));
    source.setHttpSources({ QXmppHttpFileSource(url) });
    return source;
}

static std::optional<QXmppBitsOfBinaryData> parseThumbnail(QStringView text)
{
    // XEP-0454 shows the base64 data without ";base64", clients write both
    QByteArray data;
    for (auto prefix : { u"data:image/jpeg;base64,", u"data:image/jpeg," }) {
        if (text.startsWith(QStringView(prefix))) {
            auto decoded = QByteArray::fromBase64Encoding(text.mid(QStringView(prefix).size()).toLatin1(),
                                                          QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded || decoded.decoded.isEmpty()) {
                return {};
            }
            data = std::move(decoded.decoded);
            break;
        }
    }
    if (data.isEmpty()) {
        return {};
    }

    auto bobData = QXmppBitsOfBinaryData::fromByteArray(data);
    bobData.setContentType(QMimeDatabase().mimeTypeForName(u"image/jpeg"_s));
    return bobData;
}

std::optional<Body> parseBody(QStringView body)
{
    body = body.trimmed();
    auto lineEnd = body.indexOf(u'\n');
    auto source = parseUrl(body.left(lineEnd));
    if (!source) {
        return {};
    }

    Body result;
    if (lineEnd >= 0) {
        result.thumbnail = parseThumbnail(body.mid(lineEnd + 1));
        if (!result.thumbnail) {
            return {};
        }
    }

    QXmppFileMetadata metadata;
    const auto fileName = source->httpSources().constFirst().url().fileName();
    if (!fileName.isEmpty()) {
        metadata.setFilename(fileName);
        if (auto mediaType = QMimeDatabase().mimeTypeForFile(fileName, QMimeDatabase::MatchExtension); !mediaType.isDefault()) {
            metadata.setMediaType(mediaType);
        }
    }
    if (result.thumbnail) {
        QXmppThumbnail thumbnail;
        thumbnail.setUri(result.thumbnail->cid().toCidUrl());
        thumbnail.setMediaType(result.thumbnail->contentType());
        metadata.setThumbnails({ thumbnail });
    }

    result.fileShare.setMetadata(metadata);
    result.fileShare.setEncryptedSourecs({ *source });
    return result;
}

}  // namespace QXmpp::Private::OmemoMediaSharing

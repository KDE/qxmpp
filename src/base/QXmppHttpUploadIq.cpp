// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppHttpUploadIq.h"

#include "QXmppConstants_p.h"
#include "QXmppHttpUploadIq_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QMimeDatabase>
#include <QSharedData>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace QXmpp::Private {

QString httpUploadPurposeFeature(HttpUpload::Purpose purpose)
{
    return ns_http_upload_purpose.toString() + u'#' + Enums::toStringView(purpose);
}

QList<std::pair<QByteArray, QByteArray>> mergeHttpUploadHeaders(const QList<HttpUpload::Header> &headers)
{
    QList<std::pair<QByteArray, QByteArray>> merged;
    for (const auto &header : headers) {
        auto name = header.name.toUtf8();
        auto value = header.value.toUtf8();
        auto itr = std::ranges::find_if(merged, [&](const auto &entry) {
            return entry.first == name;
        });
        if (itr == merged.end()) {
            merged.append({ std::move(name), std::move(value) });
        } else if (name == "Cookie") {
            // RFC 6265 allows only one Cookie header, its pairs are separated by "; ".
            itr->second += "; " + value;
        }
        // Authorization and Expires have a single value (RFC 9110), further ones are ignored.
    }
    return merged;
}

}  // namespace QXmpp::Private

static bool isAllowedHeader(QStringView name)
{
    return name == u"Authorization" || name == u"Cookie" || name == u"Expires";
}

static QString removeNewlines(QString string)
{
    string.remove(u'\n');
    string.remove(u'\r');
    return string;
}

/*!
    \namespace QXmpp::HttpUpload
    \inmodule QXmpp

    Types of \xep{0363}{HTTP File Upload}, see QXmppHttpUploadManager.

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::HttpUpload::Header::name

    The name of the header field.
*/

/*!
    \variable QXmpp::HttpUpload::Header::value

    The value of the header field. It is opaque and must not be interpreted.
*/

class QXmppHttpUploadRequestIqPrivate : public QSharedData
{
public:
    QString fileName;
    qint64 size;
    QMimeType contentType;
    std::optional<HttpUpload::Purpose> purpose;
    QDateTime expireBefore;
};

QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq()
    : d(new QXmppHttpUploadRequestIqPrivate())
{
}

/*! Default copy-constructor */
QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq(const QXmppHttpUploadRequestIq &) = default;
/*! Default move-constructor */
QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq(QXmppHttpUploadRequestIq &&) = default;
QXmppHttpUploadRequestIq::~QXmppHttpUploadRequestIq() = default;
/*! Default assignment operator */
QXmppHttpUploadRequestIq &QXmppHttpUploadRequestIq::operator=(const QXmppHttpUploadRequestIq &) = default;
/*! Default assignment operator */
QXmppHttpUploadRequestIq &QXmppHttpUploadRequestIq::operator=(QXmppHttpUploadRequestIq &&) = default;

/*! Returns the file name of the file to be uploaded. */
QString QXmppHttpUploadRequestIq::fileName() const
{
    return d->fileName;
}

/*!
    Sets the file name. The upload service will use this to create the upload/
    download URLs. This may also differ from the actual file name to get a
    different URL. It's not required to replace special characters (this is the
    server's job).

    \a fileName.
*/
void QXmppHttpUploadRequestIq::setFileName(const QString &fileName)
{
    d->fileName = fileName;
}

/*! Returns the file's size in bytes. */
qint64 QXmppHttpUploadRequestIq::size() const
{
    return d->size;
}

/*! Sets the file's \a size in bytes. */
void QXmppHttpUploadRequestIq::setSize(qint64 size)
{
    d->size = size;
}

/*! Returns the (optional) MIME-type of the file. */
QMimeType QXmppHttpUploadRequestIq::contentType() const
{
    return d->contentType;
}

/*! Sets the MIME-\a type of the file. This is optional. */
void QXmppHttpUploadRequestIq::setContentType(const QMimeType &type)
{
    d->contentType = type;
}

/*!
    Returns the purpose of the upload slot.

    If no purpose is set, the upload service assumes HttpUpload::Purpose::Message.

    \since QXmpp 1.18
*/
std::optional<HttpUpload::Purpose> QXmppHttpUploadRequestIq::purpose() const
{
    return d->purpose;
}

/*!
    Sets the \a purpose of the upload slot.

    The upload service can use it to apply a different retention period to the file. Services
    announce the purposes they support via service discovery, see
    QXmppHttpUploadService::purposes().

    \since QXmpp 1.18
*/
void QXmppHttpUploadRequestIq::setPurpose(std::optional<HttpUpload::Purpose> purpose)
{
    d->purpose = purpose;
}

/*!
    Returns the point in time after which the file must not be available anymore.

    This is only used with HttpUpload::Purpose::Ephemeral.

    \since QXmpp 1.18
*/
QDateTime QXmppHttpUploadRequestIq::expireBefore() const
{
    return d->expireBefore;
}

/*!
    Sets the point in time after which the file must not be available anymore.

    This is required for HttpUpload::Purpose::Ephemeral and ignored for other purposes.

    \a expireBefore.

    \since QXmpp 1.18
*/
void QXmppHttpUploadRequestIq::setExpireBefore(const QDateTime &expireBefore)
{
    d->expireBefore = expireBefore;
}

void QXmppHttpUploadRequestIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement request = firstChildElement(element, u"request");
    d->fileName = request.attribute(u"filename"_s);
    d->size = request.attribute(u"size"_s).toLongLong();
    if (request.hasAttribute(u"content-type"_s)) {
        QMimeDatabase mimeDb;
        QMimeType type = mimeDb.mimeTypeForName(request.attribute(u"content-type"_s));
        if (!type.isDefault() && type.isValid()) {
            d->contentType = type;
        }
    }

    d->purpose.reset();
    d->expireBefore = {};
    for (const auto &purposeEl : iterChildElements(request, {}, ns_http_upload_purpose)) {
        if (auto purpose = Enums::fromString<HttpUpload::Purpose>(purposeEl.tagName())) {
            d->purpose = purpose;
            if (*purpose == HttpUpload::Purpose::Ephemeral) {
                d->expireBefore = QXmppUtils::datetimeFromString(purposeEl.attribute(u"expire-before"_s));
            }
            break;
        }
    }
}

void QXmppHttpUploadRequestIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        PayloadXmlTag,
        Attribute { u"filename", d->fileName },
        Attribute { u"size", d->size },
        OptionalAttribute { u"content-type", d->contentType },
        [&] {
            if (d->purpose) {
                w.write(Element {
                    std::tuple { Enums::toStringView(*d->purpose), ns_http_upload_purpose },
                    OptionalAttribute {
                        u"expire-before",
                        *d->purpose == HttpUpload::Purpose::Ephemeral ? d->expireBefore : QDateTime(),
                    },
                });
            }
        },
    });
}

class QXmppHttpUploadSlotIqPrivate : public QSharedData
{
public:
    QUrl putUrl;
    QUrl getUrl;
    QList<HttpUpload::Header> headers;
};

QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq()
    : d(new QXmppHttpUploadSlotIqPrivate())
{
}

/*! Default copy-constructor */
QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq(const QXmppHttpUploadSlotIq &) = default;
/*! Default move-constructor */
QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq(QXmppHttpUploadSlotIq &&) = default;
QXmppHttpUploadSlotIq::~QXmppHttpUploadSlotIq() = default;
/*! Default assignment operator */
QXmppHttpUploadSlotIq &QXmppHttpUploadSlotIq::operator=(const QXmppHttpUploadSlotIq &) = default;
/*! Default move-assignment operator */
QXmppHttpUploadSlotIq &QXmppHttpUploadSlotIq::operator=(QXmppHttpUploadSlotIq &&) = default;

/*! Returns the URL for uploading via. HTTP PUT. */
QUrl QXmppHttpUploadSlotIq::putUrl() const
{
    return d->putUrl;
}

/*!
    Sets the URL the client should use for uploading.

    \a putUrl.
*/
void QXmppHttpUploadSlotIq::setPutUrl(const QUrl &putUrl)
{
    d->putUrl = putUrl;
}

/*! Returns the URL to where the file will be served. */
QUrl QXmppHttpUploadSlotIq::getUrl() const
{
    return d->getUrl;
}

/*!
    Sets the download URL.

    \a getUrl.
*/
void QXmppHttpUploadSlotIq::setGetUrl(const QUrl &getUrl)
{
    d->getUrl = getUrl;
}

/*!
    \deprecated

    Returns a map of header fields (header name -> value) that need to be
    included in the PUT (upload) request. This won't contain any other fields
    than: "Authorization", "Cookie" or "Expires".

    Multiple Cookie headers are combined, of Authorization and Expires only the first value is
    used.

    Use headers() instead.
*/
QMap<QString, QString> QXmppHttpUploadSlotIq::putHeaders() const
{
    QMap<QString, QString> map;
    for (const auto &[name, value] : mergeHttpUploadHeaders(d->headers)) {
        map.insert(QString::fromUtf8(name), QString::fromUtf8(value));
    }
    return map;
}

/*!
    \deprecated

    Sets the header fields the client needs to include in the PUT (upload)
    request. All fields other than "Authorization", "Cookie" or "Expires" will
    be ignored.

    \a putHeaders.

    Use setHeaders() instead.
*/
void QXmppHttpUploadSlotIq::setPutHeaders(const QMap<QString, QString> &putHeaders)
{
    QList<HttpUpload::Header> headers;
    for (const auto &[name, value] : putHeaders.asKeyValueRange()) {
        headers.append({ name, value });
    }
    setHeaders(headers);
}

/*!
    Returns the header fields that need to be included in the PUT (upload) request.

    This only contains the headers "Authorization", "Cookie" and "Expires",
    without newline characters, in the order of the slot. A header may occur multiple times:
    the values of Cookie need to be combined in this order into one header, separated by "; "
    (RFC 6265), of Authorization and Expires only the first value can be used as they have a
    single value (RFC 9110). QXmppHttpUploadManager does this.

    \since QXmpp 1.18
*/
QList<HttpUpload::Header> QXmppHttpUploadSlotIq::headers() const
{
    return d->headers;
}

/*!
    Sets the header fields the client needs to include in the PUT (upload) request.

    All fields other than "Authorization", "Cookie" or "Expires" are ignored
    and newline characters are removed.

    \a headers.

    \since QXmpp 1.18
*/
void QXmppHttpUploadSlotIq::setHeaders(const QList<HttpUpload::Header> &headers)
{
    d->headers.clear();
    for (const auto &header : headers) {
        auto name = removeNewlines(header.name);
        if (isAllowedHeader(name)) {
            d->headers.append({ std::move(name), removeNewlines(header.value) });
        }
    }
}

void QXmppHttpUploadSlotIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement slot = firstChildElement(element, u"slot");
    QDomElement put = firstChildElement(slot, u"put");
    d->getUrl = QUrl::fromEncoded(firstChildElement(slot, u"get").attribute(u"url"_s).toUtf8());
    d->putUrl = QUrl::fromEncoded(put.attribute(u"url"_s).toUtf8());
    QList<HttpUpload::Header> headers;
    for (const auto &header : iterChildElements(put, u"header")) {
        headers.append({ header.attribute(u"name"_s), header.text() });
    }
    setHeaders(headers);
}

void QXmppHttpUploadSlotIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        PayloadXmlTag,
        Element {
            u"put",
            Attribute { u"url", d->putUrl.toEncoded() },
            [&] {
                for (const auto &[name, value] : d->headers) {
                    w.write(Element {
                        u"header",
                        Attribute { u"name", name },
                        Characters { value },
                    });
                }
            },
        },
        Element {
            u"get",
            Attribute { u"url", d->getUrl.toEncoded() },
        },
    });
}

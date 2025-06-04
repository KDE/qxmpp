// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppHttpUploadIq.h"

#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QMimeDatabase>
#include <QSharedData>

using namespace QXmpp::Private;

class QXmppHttpUploadRequestIqPrivate : public QSharedData
{
public:
    QString fileName;
    qint64 size;
    QMimeType contentType;
};

QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq()
    : d(new QXmppHttpUploadRequestIqPrivate())
{
}

/// Default copy-constructor
QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq(const QXmppHttpUploadRequestIq &) = default;
/// Default move-constructor
QXmppHttpUploadRequestIq::QXmppHttpUploadRequestIq(QXmppHttpUploadRequestIq &&) = default;
QXmppHttpUploadRequestIq::~QXmppHttpUploadRequestIq() = default;
/// Default assignment operator
QXmppHttpUploadRequestIq &QXmppHttpUploadRequestIq::operator=(const QXmppHttpUploadRequestIq &) = default;
/// Default assignment operator
QXmppHttpUploadRequestIq &QXmppHttpUploadRequestIq::operator=(QXmppHttpUploadRequestIq &&) = default;

///
/// Returns the file name of the file to be uploaded.
///
QString QXmppHttpUploadRequestIq::fileName() const
{
    return d->fileName;
}

///
/// Sets the file name. The upload service will use this to create the upload/
/// download URLs. This may also differ from the actual file name to get a
/// different URL. It's not required to replace special characters (this is the
/// server's job).
///
void QXmppHttpUploadRequestIq::setFileName(const QString &fileName)
{
    d->fileName = fileName;
}

///
/// Returns the file's size in bytes.
///
qint64 QXmppHttpUploadRequestIq::size() const
{
    return d->size;
}

///
/// Sets the file's size in bytes.
///
void QXmppHttpUploadRequestIq::setSize(qint64 size)
{
    d->size = size;
}

///
/// Returns the (optional) MIME-type of the file.
///
QMimeType QXmppHttpUploadRequestIq::contentType() const
{
    return d->contentType;
}

///
/// Sets the MIME-type of the file. This is optional.
///
void QXmppHttpUploadRequestIq::setContentType(const QMimeType &type)
{
    d->contentType = type;
}

/// \cond
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
}

void QXmppHttpUploadRequestIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        Attribute { u"filename", d->fileName },
        Attribute { u"size", d->size },
        OptionalAttribute { u"content-type", d->contentType },
    });
}
/// \endcond

class QXmppHttpUploadSlotIqPrivate : public QSharedData
{
public:
    QUrl putUrl;
    QUrl getUrl;
    QMap<QString, QString> putHeaders;
};

QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq()
    : d(new QXmppHttpUploadSlotIqPrivate())
{
}

/// Default copy-constructor
QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq(const QXmppHttpUploadSlotIq &) = default;
/// Default move-constructor
QXmppHttpUploadSlotIq::QXmppHttpUploadSlotIq(QXmppHttpUploadSlotIq &&) = default;
QXmppHttpUploadSlotIq::~QXmppHttpUploadSlotIq() = default;
/// Default assignment operator
QXmppHttpUploadSlotIq &QXmppHttpUploadSlotIq::operator=(const QXmppHttpUploadSlotIq &) = default;
/// Default move-assignment operator
QXmppHttpUploadSlotIq &QXmppHttpUploadSlotIq::operator=(QXmppHttpUploadSlotIq &&) = default;

///
/// Returns the URL for uploading via. HTTP PUT.
///
QUrl QXmppHttpUploadSlotIq::putUrl() const
{
    return d->putUrl;
}

///
/// Sets the URL the client should use for uploading.
///
void QXmppHttpUploadSlotIq::setPutUrl(const QUrl &putUrl)
{
    d->putUrl = putUrl;
}

///
/// Returns the URL to where the file will be served.
///
QUrl QXmppHttpUploadSlotIq::getUrl() const
{
    return d->getUrl;
}

///
/// Sets the download URL.
///
void QXmppHttpUploadSlotIq::setGetUrl(const QUrl &getUrl)
{
    d->getUrl = getUrl;
}

///
/// Returns a map of header fields (header name -> value) that need to be
/// included in the PUT (upload) request. This won't contain any other fields
/// than: "Authorization", "Cookie" or "Expires".
///
QMap<QString, QString> QXmppHttpUploadSlotIq::putHeaders() const
{
    return d->putHeaders;
}

///
/// Sets the header fields the client needs to include in the PUT (upload)
/// request. All fields other than "Authorization", "Cookie" or "Expires" will
/// be ignored.
///
void QXmppHttpUploadSlotIq::setPutHeaders(const QMap<QString, QString> &putHeaders)
{
    d->putHeaders.clear();
    std::for_each(putHeaders.keyBegin(), putHeaders.keyEnd(),
                  [this, &putHeaders](const QString &name) {
                      if (name == u"Authorization" || name == u"Cookie" || name == u"Expires") {
                          d->putHeaders.insert(name, putHeaders[name]);
                      }
                  });
}

/// \cond
void QXmppHttpUploadSlotIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement slot = firstChildElement(element, u"slot");
    QDomElement put = firstChildElement(slot, u"put");
    d->getUrl = QUrl::fromEncoded(firstChildElement(slot, u"get").attribute(u"url"_s).toUtf8());
    d->putUrl = QUrl::fromEncoded(put.attribute(u"url"_s).toUtf8());
    if (put.hasChildNodes()) {
        QMap<QString, QString> headers;
        for (const auto &header : iterChildElements(put, u"header")) {
            headers[header.attribute(u"name"_s)] = header.text();
        }

        setPutHeaders(headers);
    }
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
                std::for_each(d->putHeaders.keyValueBegin(), d->putHeaders.keyValueEnd(), [&](const auto &pair) {
                    auto &[name, value] = pair;
                    w.write(Element {
                        u"header",
                        Attribute { u"name", name },
                        Characters { value },
                    });
                });
            },
        },
        Element {
            u"get",
            Attribute { u"url", d->getUrl.toEncoded() },
        },
    });
}
/// \endcond

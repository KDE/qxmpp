// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHTTPUPLOADIQ_H
#define QXMPPHTTPUPLOADIQ_H

#include "QXmppIq.h"

#include <optional>

#include <QDateTime>
#include <QSharedDataPointer>

class QUrl;
class QMimeType;

namespace QXmpp::HttpUpload {

/*!
    \enum QXmpp::HttpUpload::Purpose
    \inmodule QXmpp

    The purpose of an upload slot, used by the upload service to select the retention period of the
    file.

    \value Message The file is shared in a message. This is the default if no purpose is specified.
    \value Profile The file is part of a profile, e.g. an avatar, and needs a longer retention.
    \value Ephemeral The file must not be available after a given point in time.
    \value Permanent The file needs to be stored for a long time, e.g. for a blog post.

    \since QXmpp 1.18
*/
enum class Purpose {
    Message,
    Profile,
    Ephemeral,
    Permanent,
};

/*!
    \struct QXmpp::HttpUpload::Header
    \inmodule QXmpp

    \brief An HTTP header field that needs to be included in the upload (PUT) request.

    \since QXmpp 1.18
*/
struct QXMPP_EXPORT Header {
    QString name;
    QString value;

    bool operator==(const Header &) const = default;
};

}  // namespace QXmpp::HttpUpload

class QXmppHttpUploadRequestIqPrivate;
class QXmppHttpUploadSlotIqPrivate;

/*!
    \inmodule QXmpp

    \brief Represents an HTTP File Upload IQ for requesting an upload slot as
    defined by \xep{0363}{HTTP File Upload}.

    \since QXmpp 1.1

    \ingroup qxmpp-stanzas
*/
class QXMPP_EXPORT QXmppHttpUploadRequestIq : public QXmppIq
{
public:
    QXmppHttpUploadRequestIq();
    QXmppHttpUploadRequestIq(const QXmppHttpUploadRequestIq &);
    QXmppHttpUploadRequestIq(QXmppHttpUploadRequestIq &&);
    ~QXmppHttpUploadRequestIq() override;

    QXmppHttpUploadRequestIq &operator=(const QXmppHttpUploadRequestIq &);
    QXmppHttpUploadRequestIq &operator=(QXmppHttpUploadRequestIq &&);

    QString fileName() const;
    void setFileName(const QString &filename);

    qint64 size() const;
    void setSize(qint64 size);

    QMimeType contentType() const;
    void setContentType(const QMimeType &type);

    std::optional<QXmpp::HttpUpload::Purpose> purpose() const;
    void setPurpose(std::optional<QXmpp::HttpUpload::Purpose> purpose);

    QDateTime expireBefore() const;
    void setExpireBefore(const QDateTime &expireBefore);

    static constexpr std::tuple PayloadXmlTag = { u"request", QXmpp::Private::ns_http_upload };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isHttpUploadRequestIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppHttpUploadRequestIqPrivate> d;
};

/*!
    \inmodule QXmpp

    \brief Represents an HTTP File Upload IQ result for receiving an upload slot as
    defined by \xep{0363}{HTTP File Upload}.

    \since QXmpp 1.1

    \ingroup qxmpp-stanzas
*/
class QXMPP_EXPORT QXmppHttpUploadSlotIq : public QXmppIq
{
public:
    QXmppHttpUploadSlotIq();
    QXmppHttpUploadSlotIq(const QXmppHttpUploadSlotIq &);
    QXmppHttpUploadSlotIq(QXmppHttpUploadSlotIq &&);
    ~QXmppHttpUploadSlotIq() override;

    QXmppHttpUploadSlotIq &operator=(const QXmppHttpUploadSlotIq &);
    QXmppHttpUploadSlotIq &operator=(QXmppHttpUploadSlotIq &&);

    QUrl putUrl() const;
    void setPutUrl(const QUrl &putUrl);

    QUrl getUrl() const;
    void setGetUrl(const QUrl &getUrl);

#if QXMPP_DEPRECATED_SINCE(1, 18)
    [[deprecated("Use headers(), it keeps headers occurring multiple times")]]
    QMap<QString, QString> putHeaders() const;
    [[deprecated("Use setHeaders()")]]
    void setPutHeaders(const QMap<QString, QString> &putHeaders);
#endif

    QList<QXmpp::HttpUpload::Header> headers() const;
    void setHeaders(const QList<QXmpp::HttpUpload::Header> &headers);

    static constexpr std::tuple PayloadXmlTag = { u"slot", QXmpp::Private::ns_http_upload };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isHttpUploadSlotIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppHttpUploadSlotIqPrivate> d;
};

#endif  // QXMPPHTTPUPLOADIQ_H

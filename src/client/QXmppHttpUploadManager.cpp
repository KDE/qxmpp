// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2025 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppHttpUploadManager.h"

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppTask.h"
#include "QXmppUtils_p.h"

#include "Async.h"
#include "StringLiterals.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>

using namespace QXmpp;
using namespace QXmpp::Private;

///
/// \property QXmppHttpUploadManager::support
///
/// \see QXmppHttpUploadManager::support()
///
/// \since QXmpp 1.13
///

///
/// \property QXmppHttpUploadManager::services
///
/// \see QXmppHttpUploadManager::services()
///
/// \since QXmpp 1.13
///

///
/// \enum QXmppHttpUploadManager::Support
///
/// Server support for the feature.
///
/// \var QXmppHttpUploadManager::Support::Unknown
///
/// Whether the server supports the feature is not known.
///
/// That means, there is no corresponding information from the server (yet).
///
/// \var QXmppHttpUploadManager::Unsupported
///
/// The server does not support the feature.
///
/// \var QXmppHttpUploadManager::Support::Supported
///
/// The server supports the feature.
///
/// \since QXmpp 1.13
///

class QXmppHttpUploadServicePrivate : public QSharedData
{
public:
    QString jid;
    std::optional<quint64> sizeLimit;
};

///
/// \class QXmppHttpUploadService
///
/// \brief QXmppHttpUploadService represents an HTTP File Upload service.
///
/// It is used to store the JID and maximum file size for uploads.
///

QXmppHttpUploadService::QXmppHttpUploadService()
    : d(new QXmppHttpUploadServicePrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppHttpUploadService)

/// Returns the JID of the HTTP File Upload service.
QString QXmppHttpUploadService::jid() const
{
    return d->jid;
}

/// Sets the JID of the HTTP File Upload service.
void QXmppHttpUploadService::setJid(const QString &jid)
{
    d->jid = jid;
}

///
/// Returns the size limit of files that can be uploaded to this upload
/// service.
///
std::optional<quint64> QXmppHttpUploadService::sizeLimit() const
{
    return d->sizeLimit;
}

/// Sets the size limit of files that can be uploaded to this upload service.
void QXmppHttpUploadService::setSizeLimit(std::optional<quint64> sizeLimit)
{
    d->sizeLimit = sizeLimit;
}

struct QXmppHttpUploadPrivate {
    explicit QXmppHttpUploadPrivate(QXmppHttpUpload *q) : q(q) { }

    QUrl getUrl;
    std::optional<QXmppError> error;
    quint64 bytesSent = 0;
    quint64 bytesTotal = 0;
    QPointer<QNetworkReply> reply;
    bool finished = false;
    bool cancelled = false;

    void reportError(QXmppStanza::Error err)
    {
        error = QXmppError {
            err.text(),
            std::move(err)
        };
    }
    void reportError(QXmppError newError)
    {
        error.emplace(std::move(newError));
    }
    void reportFinished()
    {
        if (!finished) {
            finished = true;
            Q_EMIT q->finished(result());
        }
    }
    void reportProgress(quint64 sent, quint64 total)
    {
        if (total == 0 && bytesTotal > 0) {
            // QNetworkReply resets the progress in the end
            // ignore that so we can still see how large the file was
            return;
        }

        if (bytesSent != sent || bytesTotal != total) {
            bytesSent = sent;
            bytesTotal = total;
            Q_EMIT q->progressChanged();
        }
    }
    [[nodiscard]]
    QXmppHttpUpload::Result result() const
    {
        // assumes finished = true
        if (error) {
            return *error;
        }
        if (cancelled) {
            return Cancelled();
        }
        return getUrl;
    }

private:
    QXmppHttpUpload *q;
};

///
/// \class QXmppHttpUpload
///
/// Object that represents an ongoing or finished upload.
///
/// \since QXmpp 1.5
///

///
/// \property QXmppHttpUpload::progress
///
/// The current progress of the upload as a floating point number between 0 and 1.
///

///
/// \property QXmppHttpUpload::bytesSent
///
/// Number of bytes sent so far
///

///
/// \property QXmppHttpUpload::bytesTotal
///
/// Number of bytes that need to be sent in total to complete the upload
///

///
/// \fn QXmppHttpUpload::progressChanged
///
/// Emitted when the upload has made progress.
///

///
/// \fn QXmppHttpUpload::finished
///
/// Emitted when the upload has finished for any reason (success, cancelled, error).
///
/// \param result Result of the upload
///

QXmppHttpUpload::~QXmppHttpUpload() = default;

///
/// \class QXmppHttpUploadManager
///
/// The upload manager allows to upload a file to a server via \xep{0363, HTTP File Upload}.
/// This can be used for sending files to other users.
///
/// QXmppHttpUploadManager depends on QXmppDiscoveryManager.
///
/// \since QXmpp 1.5
///

///
/// \typedef QXmppHttpUpload::Result
///
/// Represents the result of an upload. It can either be a url to the
/// uploaded file, a QXmppHttpUpload::Cancelled unit struct, or an error as
/// QXmppError.
///

///
/// Returns the current progress of the upload as a floating point number between 0 and 1.
///
float QXmppHttpUpload::progress() const
{
    return calculateProgress(d->bytesSent, d->bytesTotal);
}

///
/// The number of bytes sent so far.
///
quint64 QXmppHttpUpload::bytesSent() const
{
    return d->bytesSent;
}

///
/// The number of bytes that need to be sent in total to complete the upload.
///
quint64 QXmppHttpUpload::bytesTotal() const
{
    return d->bytesTotal;
}

///
/// Cancels the upload.
///
void QXmppHttpUpload::cancel()
{
    d->cancelled = true;
    if (d->reply) {
        d->reply->abort();
    }
}

///
/// Returns whether the upload is already finished.
///
bool QXmppHttpUpload::isFinished() const
{
    return d->finished;
}

///
/// If the upload has already finished, returns the result of the upload as QXmppHttpUpload::Result,
/// otherwise returns an empty std::optional.
///
std::optional<QXmppHttpUpload::Result> QXmppHttpUpload::result() const
{
    if (d->finished) {
        return d->result();
    }
    return {};
}

QXmppHttpUpload::QXmppHttpUpload()
    : d(std::make_unique<QXmppHttpUploadPrivate>(this))
{
}

struct QXmppHttpUploadManagerPrivate {
    explicit QXmppHttpUploadManagerPrivate(QXmppHttpUploadManager *q, QNetworkAccessManager *netManager)
        : q(q), netManager(netManager)
    {
    }

    QXmppDiscoveryManager *discoveryManager() const
    {
        auto *disco = q->client()->findExtension<QXmppDiscoveryManager>();
        if (!disco) {
            qFatal("QXmppDiscoveryManager: Missing required QXmppDiscoveryManager.");
        }
        return disco;
    }

    QXmppHttpUploadManager *q;
    QNetworkAccessManager *netManager;
    QXmppHttpUploadManager::Support support;
    QVector<QXmppHttpUploadService> services;
};

///
/// Constructor
///
/// Creates and uses a new network access manager.
///
QXmppHttpUploadManager::QXmppHttpUploadManager()
    : d(std::make_unique<QXmppHttpUploadManagerPrivate>(this, new QNetworkAccessManager(this)))
{
}

///
/// Constructor
///
/// \param netManager shared network access manager, needs to have at least the lifetime of this
/// manager.
///
QXmppHttpUploadManager::QXmppHttpUploadManager(QNetworkAccessManager *netManager)
    : d(std::make_unique<QXmppHttpUploadManagerPrivate>(this, netManager))
{
}

///
/// Returns all discovered HTTP File Upload services.
///
/// \since QXmpp 1.13
///
QVector<QXmppHttpUploadService> QXmppHttpUploadManager::services() const
{
    return d->services;
}

///
/// \fn QXmppHttpUploadManager::servicesChanged()
///
/// \brief Emitted when services have changed.
///

///
/// Returns the server's support for upload services.
///
/// \since QXmpp 1.13
///
QXmppHttpUploadManager::Support QXmppHttpUploadManager::support() const
{
    return d->support;
}

///
/// \fn QXmppHttpUploadManager::supportChanged()
///
/// \brief Emitted when support has changed.
///

QXmppHttpUploadManager::~QXmppHttpUploadManager() = default;

///
/// Uploads the data from a QIODevice.
///
/// \param data QIODevice to read the data from. This can for example be a QFile.
///        It can be sequential or non-sequential.
/// \param filename How the file on the server should be called.
///        This is commonly used as last part of the resulting url.
/// \param fileSize The size of the file, in byte. If negative the size from the IO device is used.
/// \param mimeType The mime type of the file
/// \param uploadServiceJid optionally,
///        the jid from which an upload url can be requested (upload service)
/// \return an object representing the ongoing upload.
///         The object is passed as a shared_ptr,
///         which means it will be stored as long as there is a reference to it.
///         While this avoids errors from accessing it after it was deleted,
///         you should try not store it unneccesarily long to keep the memory usage down.
///         You can for example use std::weak_ptr to not increase the lifetime,
///         for example when capturing in long living lambdas, for example in connects.
///         You should also make sure to use the 4-arg QObject::connect,
///         so the connection and the connected lambda can be deleted after the upload finished.
///         \code
///         std::weak_ptr<QXmppHttpUpload> uploadPtr = upload;
///         connect(upload.get(), &QXmppHttpUpload::progressChanged, this, [uploadPtr]() {
///             auto upload = uploadPtr.lock();
///             if (upload) {
///                 qDebug() << upload->progress();
///             }
///         });
///         \endcode
///
std::shared_ptr<QXmppHttpUpload> QXmppHttpUploadManager::uploadFile(std::unique_ptr<QIODevice> data, const QString &filename, const QMimeType &mimeType, qint64 fileSize, const QString &uploadServiceJid)
{
    std::shared_ptr<QXmppHttpUpload> upload(new QXmppHttpUpload);

    if (!data->isOpen()) {
        upload->d->reportError({ u"Input data device MUST be open."_s, std::any() });
        upload->d->reportFinished();
        return upload;
    }

    if (fileSize < 0) {
        if (!data->isSequential()) {
            fileSize = data->size();
        } else {
            warning(u"No fileSize set and cannot determine size from IO device."_s);
            upload->d->reportError({ u"File size MUST be set for sequential devices."_s, std::any() });
            upload->d->reportFinished();
            return upload;
        }
    }

    auto future = requestSlot(filename, fileSize, mimeType, uploadServiceJid);
    // TODO: rawSourceDevice: could this lead to a memory leak if the "then lambda" is never executed?
    future.then(this, [this, upload, rawSourceDevice = data.release(), mimeType](SlotResult result) mutable {
        // first check whether upload was cancelled in the meantime
        if (upload->d->cancelled) {
            upload->d->reportFinished();
            return;
        }

        if (hasError(result)) {
            upload->d->reportError(getError(std::move(result)));
            upload->d->reportFinished();
        } else {
            auto slot = getValue(std::move(result));

            if (slot.getUrl().scheme() != u"https" || slot.putUrl().scheme() != u"https") {
                auto message = u"The server replied with an insecure non-https url. This is forbidden by XEP-0363."_s;
                upload->d->reportError(QXmppError { std::move(message), {} });
                upload->d->reportFinished();
                return;
            }

            upload->d->getUrl = slot.getUrl();

            QNetworkRequest request(slot.putUrl());
            request.setHeader(QNetworkRequest::ContentTypeHeader, mimeType.name());
            auto headers = slot.putHeaders();
            for (auto itr = headers.cbegin(); itr != headers.cend(); ++itr) {
                request.setRawHeader(itr.key().toUtf8(), itr.value().toUtf8());
            }

            auto *reply = d->netManager->put(request, rawSourceDevice);
            rawSourceDevice->setParent(reply);
            upload->d->reply = reply;

            connect(reply, &QNetworkReply::finished, this, [reply, upload]() {
                if (reply->error() == QNetworkReply::NoError) {
                    upload->d->reportFinished();
                }
                reply->deleteLater();
            });

            connect(reply, &QNetworkReply::errorOccurred, this,
                    [upload, reply](QNetworkReply::NetworkError error) {
                        upload->d->reportError({ reply->errorString(), error });
                        upload->d->reportFinished();
                        reply->deleteLater();
                    });

            connect(reply, &QNetworkReply::uploadProgress, this, [upload](qint64 sent, qint64 total) {
                quint64 sentBytes = sent < 0 ? 0 : quint64(sent);
                quint64 totalBytes = total < 0 ? 0 : quint64(total);
                upload->d->reportProgress(sentBytes, totalBytes);
            });
        }
    });

    return upload;
}

///
/// Upload data from a local file.
///
/// \param fileInfo QFileInfo about a local file
/// \param filename filename How the file on the server should be called.
///        This is commonly used as last part of the resulting url.
/// \param uploadServiceJid optionally,
///        the jid from which an upload url can be requested (upload service)
/// \return an object representing the ongoing upload.
///         The object is passed as a shared_ptr,
///         which means it will be stored as long as there is a reference to it.
///         While this avoids errors from accessing it after it was deleted,
///         you should try not store it unneccesarily long to keep the memory usage down.
///         You can for example use std::weak_ptr to not increase the lifetime,
///         for example when capturing in long living lambdas, for example in connects.
///         You should also make sure to use the 4-arg QObject::connect,
///         so the connection and the connected lambda can be deleted after the upload finished.
///         \code
///         std::weak_ptr<QXmppHttpUpload> uploadPtr = upload;
///         connect(upload.get(), &QXmppHttpUpload::progressChanged, this, [uploadPtr]() {
///             auto upload = uploadPtr.lock();
///             if (upload) {
///                 qDebug() << upload->progress();
///             }
///         });
///         \endcode
std::shared_ptr<QXmppHttpUpload> QXmppHttpUploadManager::uploadFile(const QFileInfo &fileInfo, const QString &filename, const QString &uploadServiceJid)
{
    auto file = std::make_unique<QFile>(fileInfo.absoluteFilePath());
    if (!file->open(QIODevice::ReadOnly)) {
        std::shared_ptr<QXmppHttpUpload> upload(new QXmppHttpUpload);
        upload->d->reportError({ file->errorString(), file->error() });
        upload->d->reportFinished();
        return upload;
    }

    auto upload = uploadFile(
        std::move(file),
        filename.isEmpty() ? fileInfo.fileName() : filename,
        QMimeDatabase().mimeTypeForFile(fileInfo),
        -1,
        uploadServiceJid);
    return upload;
}

void QXmppHttpUploadManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, [this, client]() {
        if (client->streamManagementState() == QXmppClient::NewStream) {
            resetCachedData();
            updateCachedData();
        }
    });
}

void QXmppHttpUploadManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, &QXmppClient::connected, this, nullptr);

    resetCachedData();
}

QXmppTask<void> QXmppHttpUploadManager::updateService(const QString &jid)
{
    QXmppPromise<void> promise;

    d->discoveryManager()->info(jid).then(this, [this, jid, promise](Result<QXmppDiscoInfo> &&result) mutable {
        if (hasError(result)) {
            warning(u"Could not retrieve discovery info for %1: %2"_s.arg(jid, getError(result).description));
            promise.finish();
        } else {
            auto info = getValue(std::move(result));
            const auto &features = info.features();

            if (!contains(features, ns_http_upload)) {
                promise.finish();
                return;
            }

            const auto &identities = info.identities();
            auto oldSupport = d->support;
            bool servicesAdded = false;

            for (const auto &identity : identities) {
                if (identity.category() == u"store" && identity.type() == u"file") {
                    QXmppHttpUploadService service;
                    service.setJid(jid);

                    if (auto form = info.dataForm(ns_http_upload)) {
                        if (auto maxSizeValue = form->fieldValue(u"max-file-size")) {
                            service.setSizeLimit(parseInt<uint64_t>(maxSizeValue->toString()));
                        }
                    }

                    d->services.append(service);
                    servicesAdded = true;
                }
            }

            if (servicesAdded) {
                Q_EMIT servicesChanged();

                d->support = Support::Supported;
            }

            if (oldSupport != d->support) {
                Q_EMIT supportChanged();
            }

            promise.finish();
        }
    });

    return promise.task();
}

void QXmppHttpUploadManager::updateServices()
{
    const auto serverJid = client()->configuration().domain();

    d->discoveryManager()->items(serverJid).then(this, [this, serverJid](Result<QList<QXmppDiscoItem>> &&result) {
        const auto maybeReportUnsupported = [this]() {
            // If no services match or errors happens, report unsupported
            if (d->support == Support::Unknown) {
                d->support = Support::Unsupported;
                Q_EMIT supportChanged();
            }
        };

        // We should have no support / services at this stage
        Q_ASSERT(d->support == Support::Unknown);
        Q_ASSERT(d->services.isEmpty());

        if (hasError(result)) {
            warning(u"Could not retrieve discovery items for %1: %2"_s.arg(serverJid, getError(result).description));
            maybeReportUnsupported();
        } else {
            const auto jids = transform<QList<QString>>(getValue(result), &QXmppDiscoItem::jid);
            auto tasks = transform<QList<QXmppTask<void>>>(jids, std::bind(&QXmppHttpUploadManager::updateService, this, std::placeholders::_1));
            auto task = joinVoidTasks(this, std::move(tasks));

            task.then(this, maybeReportUnsupported);
        }
    });
}

///
/// Resets the cached data.
///
void QXmppHttpUploadManager::resetCachedData()
{
    if (!d->services.isEmpty()) {
        d->services.clear();
        Q_EMIT servicesChanged();
    }

    if (d->support != Support::Unknown) {
        d->support = Support::Unknown;
        Q_EMIT supportChanged();
    }
}

///
/// Updates the cached data.
///
void QXmppHttpUploadManager::updateCachedData()
{
    updateServices();
}

///
/// Requests an upload slot from the server.
///
/// \param file The info of the file to be uploaded.
/// \param uploadService The HTTP File Upload service that is used to request
///                      the upload slot. If this is empty, the first
///                      discovered one is used.
///
/// \since QXmpp 1.13
///
QXmppTask<QXmppHttpUploadManager::SlotResult> QXmppHttpUploadManager::requestSlot(const QFileInfo &file,
                                                                                  const QString &uploadService)
{
    return requestSlot(file, file.fileName(), uploadService);
}

///
/// Requests an upload slot from the server.
///
/// \param file The info of the file to be uploaded.
/// \param customFileName This name is used instead of the file's actual name
///                       for requesting the upload slot.
/// \param uploadService The HTTP File Upload service that is used to request
///                      the upload slot. If this is empty, the first
///                      discovered one is used.
///
/// \since QXmpp 1.13
///
QXmppTask<QXmppHttpUploadManager::SlotResult> QXmppHttpUploadManager::requestSlot(const QFileInfo &file,
                                                                                  const QString &customFileName,
                                                                                  const QString &uploadService)
{
    return requestSlot(customFileName, file.size(),
                       QMimeDatabase().mimeTypeForFile(file),
                       uploadService);
}

///
/// Requests an upload slot from the server.
///
/// \param fileName The name of the file to be uploaded. This may be used by
///                 the server to generate the URL.
/// \param fileSize The size of the file to be uploaded. The server may reject
///                 too large files.
/// \param mimeType The content-type of the file. This can be used by the
///                 server to set the HTTP MIME-type of the URL.
/// \param uploadService The HTTP File Upload service that is used to request
///                      the upload slot. If this is empty, the first
///                      discovered one is used.
///
/// \since QXmpp 1.13
///
QXmppTask<QXmppHttpUploadManager::SlotResult> QXmppHttpUploadManager::requestSlot(const QString &fileName,
                                                                                  qint64 fileSize,
                                                                                  const QMimeType &mimeType,
                                                                                  const QString &uploadService)
{
    if (support() != Support::Supported && uploadService.isEmpty()) {
        return makeReadyTask(SlotResult(QXmppError {
            u"Couldn't request upload slot: No service found."_s, {} }));
    }

    QXmppHttpUploadRequestIq iq;
    if (uploadService.isEmpty()) {
        iq.setTo(d->services.first().jid());
    } else {
        iq.setTo(uploadService);
    }
    iq.setType(QXmppIq::Get);
    iq.setFileName(fileName);
    iq.setSize(fileSize);
    iq.setContentType(mimeType);

    return chainIq<SlotResult>(client()->sendIq(std::move(iq)), this);
}

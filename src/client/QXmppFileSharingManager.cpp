// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppFileSharingManager.h"

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryData.h"
#include "QXmppConstants_p.h"
#include "QXmppEncryptedFileSource.h"
#include "QXmppFileEncryption.h"
#include "QXmppFileMetadata.h"
#include "QXmppFileShare.h"
#include "QXmppFileShare_p.h"
#include "QXmppHashing_p.h"
#include "QXmppHttpFileSource.h"
#include "QXmppMessages.h"
#include "QXmppThumbnail.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"

#include <algorithm>
#include <any>
#include <unordered_map>
#include <utility>

#include <QFile>
#include <QFileInfo>
#include <QFutureInterface>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace QXmpp;
using namespace QXmpp::Private;

using MetadataGenerator = QXmppFileSharingManager::MetadataGenerator;
using MetadataGeneratorResult = QXmppFileSharingManager::MetadataGeneratorResult;

// The manager generates a hash with each hash algorithm
static std::vector<HashAlgorithm> hashAlgorithms()
{
    return { HashAlgorithm::Sha256, HashAlgorithm::Blake2b_256 };
}

// Writes only the first bytes to the output and discards the rest. Fails if there are far more bytes
// than expected, so e.g. a server can't cause endless downloads. Some are allowed, since files may be
// padded to hide their size.
class TruncatingDevice : public QIODevice
{
public:
    TruncatingDevice(std::unique_ptr<QIODevice> output, quint64 size)
        : m_output(std::move(output)), m_remaining(size), m_allowedExcess(std::max(size / 10, MIN_ALLOWED_EXCESS))
    {
        setOpenMode(m_output->openMode());
    }

    void close() override
    {
        QIODevice::close();
        m_output->close();
    }
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *data, qint64 len) override
    {
        const auto writtenLen = qint64(std::min(quint64(len), m_remaining));
        const auto excess = quint64(len - writtenLen);
        if (excess > m_allowedExcess) {
            setErrorString(u"The file is much larger than its specified size."_s);
            return -1;
        }
        if (writtenLen > 0 && m_output->write(data, writtenLen) != writtenLen) {
            setErrorString(m_output->errorString());
            return -1;
        }
        m_remaining -= quint64(writtenLen);
        m_allowedExcess -= excess;
        return len;
    }

private:
    static constexpr quint64 MIN_ALLOWED_EXCESS = 1024 * 1024;

    std::unique_ptr<QIODevice> m_output;
    quint64 m_remaining;
    quint64 m_allowedExcess;
};

class QXmppFileUploadPrivate
{
public:
    std::shared_ptr<QXmppFileSharingProvider::Upload> providerUpload;
    QFuture<std::shared_ptr<MetadataGeneratorResult>> metadataFuture;
    QFuture<HashingResultPtr> hashesFuture;
    std::optional<QXmppError> error;
    QXmppFileMetadata metadata;
    QXmppBitsOfBinaryDataList dataBlobs;
    std::any source;
    quint64 bytesSent = 0;
    quint64 bytesTotal = 0;
    bool finished = false;
    bool cancelled = false;
    bool success = false;
};

/*!
    \class QXmppFileUpload
    \inmodule QXmpp

    \brief Provides progress of stateless file sharing uploads.

    \since QXmpp 1.5
*/

/*!
    \class QXmppFileUpload::FileResult
    \inmodule QXmpp

    \brief Contains QXmppFileShare of the uploaded file and possible data blobs containing
    referenced thumbnails.
*/

/*!
    \variable QXmppFileUpload::FileResult::fileShare

    \brief File share with file metadata and file shares of the uploaded file.
*/

/*!
    \variable QXmppFileUpload::FileResult::dataBlobs

    \brief Data blobs of possibly in the metadata referenced thumbnails.

    The QXmppFileSharingManager may generate file thumbnails.
*/

/*!
    \typedef QXmppFileUpload::Result

    \brief Contains FileResult (successfully finished), QXmpp::Cancelled (manually cancelled)
    or QXmppError (an error occured while uploading).
*/

QXmppFileUpload::~QXmppFileUpload() = default;

/*! Returns the current progress between 0.0 and 1.0. */
float QXmppFileUpload::progress() const
{
    return calculateProgress(d->bytesSent, d->bytesTotal);
}

/*!
    \fn QXmppFileUpload::progressChanged()

    \brief Emitted when new bytes have been transferred.
*/

/*! \brief Cancels the file transfer. finished() will be emitted. */
void QXmppFileUpload::cancel()
{
    if (d->providerUpload) {
        d->providerUpload->cancel();
    }
    d->metadataFuture.cancel();
    d->hashesFuture.cancel();
}

/*! \brief Returns whether the file transfer is finished. */
bool QXmppFileUpload::isFinished() const
{
    return d->finished;
}

/*! \brief Returns the number of bytes that have been uploaded or downloaded. */
quint64 QXmppFileUpload::bytesTransferred() const
{
    return d->bytesSent;
}

/*! \brief Returns the number of bytes that totally need to be transferred. */
quint64 QXmppFileUpload::bytesTotal() const
{
    return d->bytesTotal;
}

/*!
    \brief Returns the result of the upload.

    The upload must be finished when calling this.
*/
QXmppFileUpload::Result QXmppFileUpload::result() const
{
    Q_ASSERT(d->finished);

    if (d->error) {
        return *d->error;
    }
    if (d->cancelled) {
        return Cancelled();
    }
    if (d->success) {
        QXmppFileShare fs;
        fs.setMetadata(d->metadata);
        fs.addSource(d->source);

        return FileResult { std::move(fs), d->dataBlobs };
    }

    Q_UNREACHABLE();
}

/*!
    \fn void QXmppFileUpload::finished()

    Emitted when the upload has finished.
*/

QXmppFileUpload::QXmppFileUpload()
    : d(std::make_unique<QXmppFileUploadPrivate>())
{
}

void QXmppFileUpload::reportFinished()
{
    Q_ASSERT(d->error || d->cancelled || d->success);
    if (!d->finished) {
        d->finished = true;
        Q_EMIT finished();
    }
}

class QXmppFileDownloadPrivate
{
public:
    std::shared_ptr<QXmppFileSharingProvider::Download> providerDownload;
    QFuture<HashVerificationResultPtr> hashesFuture;
    QList<QXmppHash> hashes;
    QXmppFileDownload::Result result;
    quint64 bytesReceived = 0;
    quint64 bytesTotal = 0;
    bool finished = false;
};

/*!
    \class QXmppFileDownload
    \inmodule QXmpp

    \brief Provides progress of stateless file sharing uploads.

    \since QXmpp 1.5
*/

/*!
    \struct QXmppFileDownload::Downloaded
    \inmodule QXmpp

    Indicates that the file could be downloaded.
*/

/*!
    \variable QXmppFileDownload::Downloaded::hashVerificationResult

    Describes the result of the hash verification.
*/

/*!
    \typedef QXmppFileDownload::Result

    \brief Contains QXmpp::Success (successfully finished), QXmpp::Cancelled (manually cancelled)
    or QXmppError (an error occured while downloading).
*/

QXmppFileDownload::~QXmppFileDownload() = default;

/*! Returns the current progress between 0.0 and 1.0. */
float QXmppFileDownload::progress() const
{
    return calculateProgress(d->bytesReceived, d->bytesTotal);
}

/*!
    \fn void QXmppFileDownload::progressChanged()

    \brief Emitted when new bytes have been transferred.
*/

/*! \brief Cancels the file transfer. finished() will be emitted. */
void QXmppFileDownload::cancel()
{
    if (d->providerDownload) {
        d->providerDownload->cancel();
    }
    d->hashesFuture.cancel();
}

/*! \brief Returns whether the file transfer is finished. */
bool QXmppFileDownload::isFinished() const
{
    return d->finished;
}

/*! \brief Returns the number of bytes that have been uploaded or downloaded. */
quint64 QXmppFileDownload::bytesTransferred() const
{
    return d->bytesReceived;
}

/*! \brief Returns the number of bytes that totally need to be transferred. */
quint64 QXmppFileDownload::bytesTotal() const
{
    return d->bytesTotal;
}

/*!
    \brief Returns the result of the download.

    The download must be finished when calling this.
*/
QXmppFileDownload::Result QXmppFileDownload::result() const
{
    Q_ASSERT(d->finished);
    return d->result;
}

/*!
    \fn void QXmppFileDownload::finished()

    Emitted when the download has finished.
*/

QXmppFileDownload::QXmppFileDownload()
    : d(std::make_unique<QXmppFileDownloadPrivate>())
{
}

void QXmppFileDownload::reportProgress(quint64 bytesReceived, quint64 bytesTotal)
{
    d->bytesReceived = bytesReceived;
    d->bytesTotal = bytesTotal;
    Q_EMIT progressChanged();
}

void QXmppFileDownload::reportFinished(Result result)
{
    d->finished = true;
    d->result = std::move(result);
    Q_EMIT finished();
}

class QXmppFileSharingManagerPrivate
{
public:
    MetadataGenerator metadataGenerator = [](std::unique_ptr<QIODevice>) -> QFuture<std::shared_ptr<MetadataGeneratorResult>> {
        return makeReadyFuture(std::make_shared<MetadataGeneratorResult>());
    };
    std::unordered_map<std::type_index, std::shared_ptr<QXmppFileSharingProvider>> providers;
};

/*!
    \class QXmppFileSharingProvider
    \inmodule QXmpp

    Base class for Stateless File Sharing providers

    A provider defines the way that files can be uploaded and downloaded.

    To use it, implement all the pure virtual functions, and add a using
    declaration for the type of source you want to handle.
    \code
    using SourceType = QXmppHttpFileSource;
    \endcode

    An example is the QXmppHttpFileSharingProvider, which uses HTTP File Upload.

    \since QXmpp 1.5
*/

/*!
    \class QXmppFileSharingManager
    \inmodule QXmpp
    \ingroup qxmpp-managers

    The file sharing manager allows to easily send and retrieve files in a chat.

    \section1 Encryptions protecting only the body

    Recipients of an encryption protecting only the body of messages (e.g. OMEMO 0) do not receive
    the elements of shared files (\xep{0447}{Stateless file sharing}). For them, the manager
    replaces the body by the \c aesgcm:// URL of a file (\xep{0454}{OMEMO Media Sharing}) via
    QXmpp::Messages::PrepareBodyOnly and removes all encrypted file sources, which contain the keys.
    This requires an encrypted source with QXmpp::Aes256GcmNoPad and an HTTPS URL, see
    QXmppEncryptedFileSharingProvider::setCipher().

    Such a body can only contain one file and no text, so a text (e.g. a caption) is not sent to
    those recipients. If the message contains several files, the one whose URL is the body is used,
    otherwise the first one. To share several files or files with a text, send an additional message
    for each remaining file, with its source attached (QXmppMessage::setFileSourcesAttachments()),
    as recommended by \xep{0447}{Stateless file sharing}. Applications doing so for all files of a
    message can restore its text in an own handler of QXmpp::Messages::PrepareBodyOnly:

    \code
    client->hooks(this).add<QXmpp::Messages::PrepareBodyOnly>([](QXmpp::Messages::BodyOnlyMessage &message) {
        const auto &original = message.original();
        if (!original.readFallbackRemovedText(QXmppFallback::Body, { u"urn:xmpp:sfs:0"_s }).isEmpty()) {
            message.message().setBody(original.body());
            message.message().setFallbackMarkers(original.fallbackMarkers());
        }
    });
    \endcode

    Received messages encrypted that way with a body consisting only of an \c aesgcm:// URL
    (and optionally a thumbnail) contain the file in QXmppMessage::sharedFiles(), also without
    this manager. The body is marked as fallback for \c urn:xmpp:sfs:0 (QXmppFallback).

    \since QXmpp 1.5
*/

// XEP-0454: the aesgcm:// URL replacing the body
static std::optional<QString> bodyOnlyUrl(const QXmppMessage &message)
{
    QList<QXmppEncryptedFileSource> sources;
    for (const auto &file : message.sharedFiles()) {
        sources.append(file.encryptedSources());
    }
    const auto attachments = message.fileSourcesAttachments();
    for (const auto &attachment : attachments) {
        sources.append(attachment.encryptedSources());
    }

    const auto body = message.body().trimmed();
    std::optional<QString> firstUrl;
    for (const auto &source : std::as_const(sources)) {
        auto url = OmemoMediaSharing::aesGcmUrl(source);
        if (!url) {
            continue;
        }
        const auto httpSources = source.httpSources();
        if (body == *url || std::ranges::any_of(httpSources, [&](const auto &httpSource) { return QUrl(body) == httpSource.url(); })) {
            return url;
        }
        if (!firstUrl) {
            firstUrl = std::move(url);
        }
    }
    return firstUrl;
}

static void prepareBodyOnly(Messages::BodyOnlyMessage &bodyOnly)
{
    auto &message = bodyOnly.message();

    if (auto url = bodyOnlyUrl(message)) {
        message.setBody(*url);
        // the markers of the body refer to the previous one
        auto markers = message.fallbackMarkers();
        for (auto &marker : markers) {
            auto references = marker.references();
            references.removeIf([](const QXmppFallback::Reference &reference) {
                return reference.element == QXmppFallback::Body;
            });
            marker.setReferences(references);
        }
        markers.removeIf([](const QXmppFallback &marker) { return marker.references().isEmpty(); });
        message.setFallbackMarkers(markers);
    }

    // must not be sent in the clear, even if an application allows the namespace
    auto files = message.sharedFiles();
    files.removeIf([](const auto &file) { return !file.encryptedSources().isEmpty(); });
    message.setSharedFiles(files);
    auto attachments = message.fileSourcesAttachments();
    attachments.removeIf([](const auto &attachment) { return !attachment.encryptedSources().isEmpty(); });
    message.setFileSourcesAttachments(attachments);
}

QXmppFileSharingManager::QXmppFileSharingManager()
    : d(std::make_unique<QXmppFileSharingManagerPrivate>())
{
    // before handlers of applications, so they can adapt the result
    hooks().add<Messages::PrepareBodyOnly>(&prepareBodyOnly, HookSequence::Early);
}

QXmppFileSharingManager::~QXmppFileSharingManager() = default;

/*!
    \typedef QXmppFileSharingManager::MetadataGenerator

    The function signature of a metadata \a generator function

    \a generator.
*/

/*!
    \brief Register a function that is called when metadata needs to be gererated for a file.

    The function is passed a QIODevice, so if you need the path of the file on disk,
    you can dynamically cast it to a QFile and access the fileName.
    When doing that, make sure to check the result,
    as in the future this function might be passed other QIODevices than QFile.

    \a generator.
*/
void QXmppFileSharingManager::setMetadataGenerator(MetadataGenerator &&generator)
{
    d->metadataGenerator = std::move(generator);
}

/*!
    \brief Upload a file in a way that it can be attached to a message.

    \a provider is the provider class that decides how the file is uploaded. \a filePath is the
    path to a file that should be uploaded. \a description is an optional description of the
    file.

    Returns an object that allows to track the progress of the upload.
    Once the upload is finished, the finished signal is emitted on it.
*/
std::shared_ptr<QXmppFileUpload> QXmppFileSharingManager::uploadFile(std::shared_ptr<QXmppFileSharingProvider> provider,
                                                                     const QString &filePath,
                                                                     const std::optional<QString> &description)
{
    QFileInfo fileInfo(filePath);
    auto metadata = QXmppFileMetadata::fromFileInfo(fileInfo);
    metadata.setDescription(description);

    std::shared_ptr<QXmppFileUpload> upload(new QXmppFileUpload());
    upload->d->metadata = std::move(metadata);

    auto openFile = [=]() -> std::unique_ptr<QIODevice> {
        auto device = std::make_unique<QFile>(fileInfo.absoluteFilePath());
        if (!device->open(QIODevice::ReadOnly)) {
            upload->d->error = QXmppError::fromIoDevice(*device);
            upload->reportFinished();
        }
        return device;
    };

    auto metadataIoDevice = openFile();
    auto hashesIoDevice = openFile();
    auto uploadIoDevice = openFile();

    if (upload->d->finished) {
        // error occurred while opening file
        return upload;
    }

    upload->d->metadataFuture = d->metadataGenerator(std::move(metadataIoDevice));
    upload->d->hashesFuture = calculateHashes(std::move(hashesIoDevice), hashAlgorithms());

    auto onProgress = [upload](quint64 sent, quint64 total) {
        upload->d->bytesSent = sent;
        upload->d->bytesTotal = total;
        Q_EMIT upload->progressChanged();
    };
    auto onFinished = [this, upload](QXmppFileSharingProvider::UploadResult uploadResult) {
        // free memory
        upload->d->providerUpload.reset();
        if (std::holds_alternative<std::any>(uploadResult)) {
            upload->d->source = std::get<std::any>(std::move(uploadResult));
            await(upload->d->metadataFuture, this, [this, upload](auto &&result) mutable {
                if (result->dimensions) {
                    upload->d->metadata.setWidth(result->dimensions->width());
                    upload->d->metadata.setHeight(result->dimensions->height());
                }
                if (result->length) {
                    upload->d->metadata.setLength(*result->length);
                }

                if (!result->thumbnails.empty()) {
                    QList<QXmppThumbnail> thumbnails;
                    thumbnails.reserve(result->thumbnails.size());
                    upload->d->dataBlobs.reserve(result->thumbnails.size());

                    for (const auto &metadataThumb : result->thumbnails) {
                        auto bobData = QXmppBitsOfBinaryData::fromByteArray(metadataThumb.data);
                        bobData.setContentType(metadataThumb.mimeType);

                        QXmppThumbnail thumbnail;
                        thumbnail.setHeight(metadataThumb.height);
                        thumbnail.setWidth(metadataThumb.width);
                        thumbnail.setMediaType(metadataThumb.mimeType);
                        thumbnail.setUri(bobData.cid().toCidUrl());

                        thumbnails.append(std::move(thumbnail));
                        upload->d->dataBlobs.append(std::move(bobData));
                    }
                    upload->d->metadata.setThumbnails(thumbnails);
                }

                await(upload->d->hashesFuture, this, [upload](auto hashResult) mutable {
                    auto &hashValue = hashResult->result;
                    if (std::holds_alternative<std::vector<QXmppHash>>(hashValue)) {
                        auto hashes = transform<QList<QXmppHash>>(std::get<std::vector<QXmppHash>>(std::move(hashValue)), [](auto &&hash) {
                            return hash;
                        });
                        upload->d->metadata.setHashes(hashes);
                        upload->d->success = true;
                    } else if (std::holds_alternative<Cancelled>(hashValue)) {
                        upload->d->cancelled = true;
                    } else if (std::holds_alternative<QXmppError>(hashValue)) {
                        upload->d->error = std::get<QXmppError>(std::move(hashValue));
                    }
                    upload->reportFinished();
                });
            });
        } else if (std::holds_alternative<Cancelled>(uploadResult)) {
            upload->d->cancelled = true;
            upload->reportFinished();
        } else if (std::holds_alternative<QXmppError>(uploadResult)) {
            upload->d->error = std::get<QXmppError>(std::move(uploadResult));
            upload->reportFinished();
        }
    };

    upload->d->providerUpload = provider->uploadFile(std::move(uploadIoDevice), upload->d->metadata, std::move(onProgress), std::move(onFinished));
    return upload;
}

/*!
    \brief Download a file from a QXmppFileShare

    \warning This function currently does not check the hash of the downloaded file.

    Make sure to register the provider
    that handles the sources used in this file share before calling this function.

    \a fileShare is the file share object which you want to download. \a output is an open
    QIODevice that the data should be written into, in most cases a QFile.

    Decrypted files larger than the size in the metadata of the file share are cut off
    (\xep{0448}{Encryption for stateless file sharing}). The download fails if the additional bytes
    are more than 10 % of the size and more than 1 MiB.

    Returns an object that allows to track the progress of the download.
*/
std::shared_ptr<QXmppFileDownload> QXmppFileSharingManager::downloadFile(
    const QXmppFileShare &fileShare,
    std::unique_ptr<QIODevice> output)
{
    std::shared_ptr<QXmppFileDownload> download(new QXmppFileDownload());
    download->d->hashes = fileShare.metadata().hashes();

    // currently hashing does only work with QFiles
    auto filePath = [&]() -> QString {
        if (auto *file = dynamic_cast<QFile *>(output.get())) {
            return file->fileName();
        }
        return {};
    }();

    auto onProgress = [download](quint64 received, quint64 total) {
        download->reportProgress(received, total);
    };
    auto onFinished = [this, download, filePath](QXmppFileSharingProvider::DownloadResult result) mutable {
        // reduce ref count
        download->d->providerDownload.reset();

        // pass errors directly
        if (std::holds_alternative<Cancelled>(result)) {
            download->reportFinished(Cancelled());
            return;
        }
        // Files encrypted with AES-GCM do not need to end with the authentication tag if a hash of the
        // file is known (XEP-0448).
        std::optional<QXmppError> unauthenticatedError;
        if (auto *error = std::get_if<QXmppError>(&result)) {
            if (filePath.isEmpty() || !error->holdsType<Encryption::UnauthenticatedDataError>()) {
                download->reportFinished(std::move(*error));
                return;
            }
            unauthenticatedError = std::move(*error);
        }

        // try to do hash verification
        if (filePath.isEmpty()) {
            warning(u"Can't verify hashes of other io devices than QFile!"_s);
            download->reportFinished(QXmppFileDownload::Downloaded { QXmppFileDownload::NoStrongHashes });
            return;
        }

        auto file = std::make_unique<QFile>(filePath);
        if (!file->open(QIODevice::ReadOnly)) {
            download->reportFinished(QXmppError::fromFileDevice(*file));
            return;
        }

        download->d->hashesFuture = verifyHashes(
            std::move(file),
            transform<std::vector<QXmppHash>>(download->d->hashes, [](auto hash) { return hash; }));

        await(download->d->hashesFuture, this, [download, unauthenticatedError](HashVerificationResultPtr hashResult) {
            if (unauthenticatedError && !std::holds_alternative<HashVerificationResult::Verified>(hashResult->result)) {
                download->reportFinished(*unauthenticatedError);
                return;
            }
            auto convert = overloaded {
                [](HashVerificationResult::NoStrongHashes) {
                    return QXmppFileDownload::Downloaded {
                        QXmppFileDownload::NoStrongHashes
                    };
                },
                [](HashVerificationResult::NotMatching) {
                    return QXmppError {
                        u"Checksum does not match"_s,
                        {}
                    };
                },
                [](HashVerificationResult::Verified) {
                    return QXmppFileDownload::Downloaded {
                        QXmppFileDownload::HashVerified
                    };
                }
            };
            download->reportFinished(map<QXmppFileDownload::Result>(convert, hashResult->result));
        });
    };

    fileShare.visitSources([&](const std::any &source) {
        if (auto provider = providerForSource(source)) {
            // additional bytes of decrypted files must be cut off (XEP-0448), e.g. a padding
            if (const auto size = fileShare.metadata().size(); size && source.type() == typeid(QXmppEncryptedFileSource)) {
                output = std::make_unique<TruncatingDevice>(std::move(output), *size);
            }
            download->d->providerDownload = provider->downloadFile(source, std::move(output), std::move(onProgress), std::move(onFinished));
            return true;
        }
        return false;
    });

    return download;
}

void QXmppFileSharingManager::internalRegisterProvider(std::type_index index, std::shared_ptr<QXmppFileSharingProvider> provider)
{
    d->providers.insert_or_assign(index, provider);
}

std::shared_ptr<QXmppFileSharingProvider> QXmppFileSharingManager::providerForSource(const std::any &source) const
{
    if (auto provider = d->providers.find(std::type_index(source.type()));
        provider != d->providers.cend()) {
        return provider->second;
    }
    return {};
}

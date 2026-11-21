// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2025 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppEncryptedFileSharingProvider.h"

#include "QXmppFileEncryption.h"
#include "QXmppFileMetadata.h"
#include "QXmppFileSharingManager.h"
#include "QXmppUtils.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"

#include <QMimeDatabase>

using namespace QXmpp;
using namespace QXmpp::Private;

/*!
    \class QXmppEncryptedFileSharingProvider
    \inmodule QXmpp

    Encrypts or decrypts files on the fly when uploading or downloading.

    Since QXmpp 1.18, files are encrypted with AES-256-GCM by default and the authentication tag
    is appended to files encrypted with AES-GCM, as recommended by
    \xep{0448}{Encryption for stateless file sharing}. Downloaded files encrypted with AES-GCM must
    either end with a valid tag or match a strong hash of the file share (only checked when
    downloading into a QFile). Older versions of QXmpp do not remove the tag, so the hashes of the
    file do not match there.

    \since QXmpp 1.5
*/

class QXmppEncryptedFileSharingProviderPrivate
{
public:
    QXmppFileSharingManager *manager;
    std::shared_ptr<QXmppFileSharingProvider> uploadBaseProvider;
    Cipher cipher = Aes256GcmNoPad;
};

/*!
    \brief Create a new QXmppEncryptedFileSharingProvider.

    \a manager is the QXmppFileSharingManager to be used to find other providers for
    downloading encrypted files. \a uploadBaseProvider is the provider to be used for uploading
    the encrypted files.
*/
QXmppEncryptedFileSharingProvider::QXmppEncryptedFileSharingProvider(
    QXmppFileSharingManager *manager,
    std::shared_ptr<QXmppFileSharingProvider> uploadBaseProvider)
    : d(std::make_unique<QXmppEncryptedFileSharingProviderPrivate>())
{
    d->manager = manager;
    d->uploadBaseProvider = std::move(uploadBaseProvider);
}

QXmppEncryptedFileSharingProvider::~QXmppEncryptedFileSharingProvider() = default;

/*!
    Returns the cipher used to encrypt uploaded files.

    \since QXmpp 1.18
*/
Cipher QXmppEncryptedFileSharingProvider::cipher() const
{
    return d->cipher;
}

/*!
    Sets the \a cipher used to encrypt uploaded files, by default QXmpp::Aes256GcmNoPad.

    Recipients of an encryption protecting only the body of messages (e.g. OMEMO 0) can only
    receive files encrypted with QXmpp::Aes256GcmNoPad, as an \c aesgcm:// URL in the body
    (\xep{0454}{OMEMO Media Sharing}), see QXmppFileSharingManager.

    \since QXmpp 1.18
*/
void QXmppEncryptedFileSharingProvider::setCipher(Cipher cipher)
{
    d->cipher = cipher;
}

auto QXmppEncryptedFileSharingProvider::downloadFile(const std::any &source,
                                                     std::unique_ptr<QIODevice> target,
                                                     std::function<void(quint64, quint64)> reportProgress,
                                                     std::function<void(DownloadResult)> reportFinished)
    -> std::shared_ptr<Download>
{
    QXmppEncryptedFileSource encryptedSource;
    try {
        encryptedSource = std::any_cast<QXmppEncryptedFileSource>(source);
    } catch (const std::bad_any_cast &) {
        qFatal("QXmppEncryptedFileSharingProvider::downloadFile can only handle QXmppEncryptedFileSource sources");
    }

    auto output = std::make_unique<Encryption::DecryptionDevice>(std::move(target), encryptedSource.cipher(), encryptedSource.key(), encryptedSource.iv());

    // find provider for source of encrypted file
    std::any httpSource = encryptedSource.httpSources().front();
    if (auto provider = d->manager->providerForSource(httpSource)) {
        auto onFinished = [decryptDevice = output.get(), reportFinished = std::move(reportFinished)](DownloadResult result) {
            const auto decryptResult = decryptDevice->finish();
            if (std::holds_alternative<Success>(result) && decryptResult != Encryption::DecryptionDevice::Success) {
                std::any error;
                if (decryptResult == Encryption::DecryptionDevice::Unauthenticated) {
                    // accepted by the manager if a hash matches
                    error = Encryption::UnauthenticatedDataError {};
                }
                reportFinished(QXmppError { u"Could not decrypt the downloaded file: the data is invalid or has been modified."_s, std::move(error) });
                return;
            }
            reportFinished(std::move(result));
        };

        return provider->downloadFile(httpSource, std::move(output), std::move(reportProgress), std::move(onFinished));
    }

    reportFinished(QXmppError { u"No basic file sharing provider available for encrypted file."_s, {} });
    return {};
}

auto QXmppEncryptedFileSharingProvider::uploadFile(std::unique_ptr<QIODevice> data,
                                                   const QXmppFileMetadata &,
                                                   std::function<void(quint64, quint64)> reportProgress,
                                                   std::function<void(UploadResult)> reportFinished)
    -> std::shared_ptr<Upload>
{
    auto cipher = d->cipher;
    auto key = Encryption::generateKey(cipher);
    auto iv = Encryption::generateInitializationVector(cipher);

    auto encDevice = std::make_unique<Encryption::EncryptionDevice>(std::move(data), cipher, key, iv);
    auto encryptedSize = encDevice->size();

    QXmppFileMetadata metadata;
    metadata.setFilename(QXmppUtils::generateStanzaHash(10));
    metadata.setMediaType(QMimeDatabase().mimeTypeForName(u"application/octet-stream"_s));
    metadata.setSize(encryptedSize);

    // find provider for source of encrypted file
    Q_ASSERT(d->uploadBaseProvider);
    return d->uploadBaseProvider->uploadFile(
        std::move(encDevice),
        metadata,
        std::move(reportProgress),
        [=, reportFinished = std::move(reportFinished)](UploadResult result) {
            if (std::holds_alternative<std::any>(result)) {
                auto encryptedResult = map<UploadResult>(
                    [&](std::any httpSourceAny) {
                        QXmppEncryptedFileSource encryptedSource;
                        encryptedSource.setCipher(cipher);
                        encryptedSource.setKey(key);
                        encryptedSource.setIv(iv);
                        encryptedSource.setHttpSources({ std::any_cast<QXmppHttpFileSource>(std::move(httpSourceAny)) });

                        return encryptedSource;
                    },
                    std::move(result));
                reportFinished(std::move(encryptedResult));
            } else if (std::holds_alternative<Cancelled>(result)) {
                reportFinished(Cancelled {});
            } else if (std::holds_alternative<QXmppError>(result)) {
                reportFinished(std::get<QXmppError>(std::move(result)));
            }
        });
}

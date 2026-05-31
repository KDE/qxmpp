// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPFILESHARINGPROVIDER_H
#define QXMPPFILESHARINGPROVIDER_H

#include "QXmppError.h"
#include "QXmppGlobal.h"

#include <any>
#include <functional>
#include <memory>
#include <variant>

class QIODevice;
class QXmppFileMetadata;
class QXmppUpload;
class QXmppDownload;

// The interface of a provider for the QXmppFileSharingManager.
// Documented as \class QXmppFileSharingProvider in QXmppFileSharingManager.cpp.
class QXMPP_EXPORT QXmppFileSharingProvider
{
public:
    /*!
        \typealias QXmppFileSharingProvider::DownloadResult

        Contains QXmpp::Success (successfully finished), QXmpp::Cancelled (manually cancelled) or
        QXmppError (an error occured while downloading).
    */
    using DownloadResult = std::variant<QXmpp::Success, QXmpp::Cancelled, QXmppError>;

    /*!
        \typealias QXmppFileSharingProvider::UploadResult

        Contains std::any (created file source), QXmpp::Cancelled (manually cancelled) or
        QXmppError (an error occured while uploading).
    */
    using UploadResult = std::variant<std::any /* source */, QXmpp::Cancelled, QXmppError>;

    /*!
        \inmodule QXmpp

        Used to control ongoing downloads
    */
    class Download
    {
    public:
        virtual ~Download() = default;
        /*! Cancels the download. */
        virtual void cancel() = 0;
    };

    /*!
        \inmodule QXmpp

        Used to control ongoing uploads
    */
    class Upload
    {
    public:
        virtual ~Upload() = default;
        /*! Cancels the upload. */
        virtual void cancel() = 0;
    };

    virtual ~QXmppFileSharingProvider() = default;

    /*!
        \brief Handles the download of files for this provider.

        \a source is a type-erased source object. The provider will only ever have to handle
        its own sources, so this can safely be casted to the defined source type. \a target is
        the QIODevice into which the received data should be written. \a reportProgress can be
        called to report received bytes and total bytes. \a reportFinished finalizes the
        download, no more progress must be reported after this.
    */
    virtual auto downloadFile(const std::any &source,
                              std::unique_ptr<QIODevice> target,
                              std::function<void(quint64, quint64)> reportProgress,
                              std::function<void(DownloadResult)> reportFinished) -> std::shared_ptr<Download> = 0;

    /*!
        \brief Handles the upload of a file for this provider.

        \a source is a QIODevice from which data for uploading should be read. \a info is the
        metadata of the file. \a reportProgress can be called to report sent bytes and total
        bytes. \a reportFinished finalizes the upload, no more progress must be reported after
        this.
    */
    virtual auto uploadFile(std::unique_ptr<QIODevice> source,
                            const QXmppFileMetadata &info,
                            std::function<void(quint64, quint64)> reportProgress,
                            std::function<void(UploadResult)> reportFinished) -> std::shared_ptr<Upload> = 0;
};

#endif  // QXMPPFILESHARINGPROVIDER_H

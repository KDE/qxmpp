// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2025 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHTTPUPLOADMANAGER_H
#define QXMPPHTTPUPLOADMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppError.h"

#include <optional>
#include <variant>

#include <QUrl>

class QFileInfo;
class QNetworkAccessManager;
class QXmppHttpUploadServicePrivate;
class QXmppHttpUploadSlotIq;
template<typename T>
class QXmppTask;
struct QXmppHttpUploadPrivate;
struct QXmppHttpUploadManagerPrivate;

class QXMPP_EXPORT QXmppHttpUploadService
{
public:
    QXmppHttpUploadService();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppHttpUploadService)

    QString jid() const;
    void setJid(const QString &jid);

    std::optional<quint64> sizeLimit() const;
    void setSizeLimit(std::optional<quint64> sizeLimit);

private:
    QSharedDataPointer<QXmppHttpUploadServicePrivate> d;
};

class QXMPP_EXPORT QXmppHttpUpload : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(quint64 bytesSent READ bytesSent NOTIFY progressChanged)
    Q_PROPERTY(quint64 bytesTotal READ bytesTotal NOTIFY progressChanged)

public:
    using Result = std::variant<QUrl, QXmpp::Cancelled, QXmppError>;

    ~QXmppHttpUpload();

    float progress() const;
    quint64 bytesSent() const;
    quint64 bytesTotal() const;

    void cancel();
    bool isFinished() const;
    std::optional<Result> result() const;

    Q_SIGNAL void progressChanged();
    Q_SIGNAL void finished(const QXmppHttpUpload::Result &result);

private:
    friend class QXmppHttpUploadManager;

    QXmppHttpUpload();

    std::unique_ptr<QXmppHttpUploadPrivate> d;
};

Q_DECLARE_METATYPE(QXmppHttpUpload::Result);

class QXMPP_EXPORT QXmppHttpUploadManager : public QXmppClientExtension
{
    Q_OBJECT
    Q_PROPERTY(QVector<QXmppHttpUploadService> services READ services NOTIFY servicesChanged)
    Q_PROPERTY(QXmppHttpUploadManager::Support support READ support NOTIFY supportChanged)

public:
    enum class Support {
        Unknown,
        Unsupported,
        Supported,
    };
    Q_ENUM(Support)

    QXmppHttpUploadManager();
    explicit QXmppHttpUploadManager(QNetworkAccessManager *netManager);
    ~QXmppHttpUploadManager();

    QVector<QXmppHttpUploadService> services() const;
    Q_SIGNAL void servicesChanged();

    Support support() const;
    Q_SIGNAL void supportChanged();

    std::shared_ptr<QXmppHttpUpload> uploadFile(std::unique_ptr<QIODevice> data, const QString &filename, const QMimeType &mimeType, qint64 fileSize = -1, const QString &uploadServiceJid = {});
    std::shared_ptr<QXmppHttpUpload> uploadFile(const QFileInfo &fileInfo, const QString &filename = {}, const QString &uploadServiceJid = {});

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    QXmppTask<void> updateService(const QString &jid);
    void updateServices();
    void resetCachedData();
    void updateCachedData();

    using SlotResult = std::variant<QXmppHttpUploadSlotIq, QXmppError>;
    QXmppTask<SlotResult> requestSlot(const QFileInfo &file,
                                      const QString &uploadService = {});
    QXmppTask<SlotResult> requestSlot(const QFileInfo &file,
                                      const QString &customFileName,
                                      const QString &uploadService = {});
    QXmppTask<SlotResult> requestSlot(const QString &fileName,
                                      qint64 fileSize,
                                      const QMimeType &mimeType,
                                      const QString &uploadService = {});

    friend struct QXmppHttpUploadManagerPrivate;

    std::unique_ptr<QXmppHttpUploadManagerPrivate> d;
};

#endif  // QXMPPHTTPUPLOADMANAGER_H

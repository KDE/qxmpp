// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTRANSFERMANAGER_H
#define QXMPPTRANSFERMANAGER_H

#include "QXmppClientExtension.h"

#include <QDateTime>
#include <QSharedData>
#include <QUrl>
#include <QVariant>

class QTcpSocket;
class QXmppByteStreamIq;
class QXmppIbbCloseIq;
class QXmppIbbDataIq;
class QXmppIbbOpenIq;
class QXmppIq;
class QXmppStreamInitiationIq;
class QXmppTransferFileInfoPrivate;
class QXmppTransferJobPrivate;
class QXmppTransferManager;
class QXmppTransferManagerPrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppTransferFileInfo class holds metadata about a file being transferred.

    Contains information about a file that is transferred using QXmppTransferJob.
*/
class QXMPP_EXPORT QXmppTransferFileInfo
{
public:
    QXmppTransferFileInfo();
    QXmppTransferFileInfo(const QXmppTransferFileInfo &other);
    ~QXmppTransferFileInfo();

    QDateTime date() const;
    void setDate(const QDateTime &date);

    QByteArray hash() const;
    void setHash(const QByteArray &hash);

    QString name() const;
    void setName(const QString &name);

    QString description() const;
    void setDescription(const QString &description);

    qint64 size() const;
    void setSize(qint64 size);

    bool isNull() const;
    QXmppTransferFileInfo &operator=(const QXmppTransferFileInfo &other);
    bool operator==(const QXmppTransferFileInfo &other) const;

    static constexpr std::tuple XmlTag = { u"file", QXmpp::Private::ns_stream_initiation_file_transfer };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;

private:
    QSharedDataPointer<QXmppTransferFileInfoPrivate> d;
};

/*!
    \inmodule QXmpp

    \brief The QXmppTransferJob class represents a single file transfer job,
    as defined by \xep{0096}{SI File Transfer}.

    \sa QXmppTransferManager
*/
class QXMPP_EXPORT QXmppTransferJob : public QXmppLoggable
{
    Q_OBJECT
    Q_FLAGS(Method Methods)
    /*!
        \property QXmppTransferJob::direction

        The job's transfer direction
    */
    Q_PROPERTY(Direction direction READ direction CONSTANT)
    /*!
        \property QXmppTransferJob::localFileUrl

        The local file URL
    */
    Q_PROPERTY(QUrl localFileUrl READ localFileUrl WRITE setLocalFileUrl NOTIFY localFileUrlChanged)
    /*!
        \property QXmppTransferJob::jid

        The remote party's JID
    */
    Q_PROPERTY(QString jid READ jid CONSTANT)
    /*!
        \property QXmppTransferJob::method

        The job's transfer method
    */
    Q_PROPERTY(Method method READ method CONSTANT)
    /*!
        \property QXmppTransferJob::state

        The job's state
    */
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

    /*!
        \property QXmppTransferJob::fileName

        The name of the file
    */
    Q_PROPERTY(QString fileName READ fileName CONSTANT)
    /*!
        \property QXmppTransferJob::fileSize

        The size of the file
    */
    Q_PROPERTY(qint64 fileSize READ fileSize CONSTANT)

public:
    /*!
        This enum is used to describe the direction of a transfer job.

        \value IncomingDirection The file is being received.
        \value OutgoingDirection The file is being sent.
    */
    enum Direction {
        IncomingDirection,
        OutgoingDirection
    };
    Q_ENUM(Direction)

    /*!
        This enum is used to describe the type of error encountered by a transfer job.

        \value NoError No error occurred.
        \value AbortError The file transfer was aborted.
        \value FileAccessError An error was encountered trying to access a local file.
        \value FileCorruptError The file is corrupt: the file size or hash do not match.
        \value ProtocolError An error was encountered in the file transfer protocol.
    */
    enum Error {
        NoError = 0,
        AbortError,
        FileAccessError,
        FileCorruptError,
        ProtocolError
    };
    Q_ENUM(Error)

    /*!
        This enum is used to describe a transfer method.

        \value NoMethod No transfer method.
        \value InBandMethod \xep{0047}{In-Band Bytestreams}.
        \value SocksMethod \xep{0065}{SOCKS5 Bytestreams}.
        \value AnyMethod Any supported transfer method.
    */
    enum Method {
        NoMethod = 0,
        InBandMethod = 1,
        SocksMethod = 2,
        AnyMethod = 3
    };
    Q_ENUM(Method)
    Q_DECLARE_FLAGS(Methods, Method)

    /*!
        This enum is used to describe the state of a transfer job.

        \value OfferState The transfer is being offered to the remote party.
        \value StartState The transfer is being connected.
        \value TransferState The transfer is ongoing.
        \value FinishedState The transfer is finished.
    */
    enum State {
        OfferState = 0,
        StartState = 1,
        TransferState = 2,
        FinishedState = 3
    };
    Q_ENUM(State)

    ~QXmppTransferJob() override;

    QXmppTransferJob::Direction direction() const;
    QString jid() const;
    QXmppTransferJob::Method method() const;
    QXmppTransferJob::State state() const;

    QXmppTransferJob::Error error() const;
    QString sid() const;
    qint64 speed() const;

    // XEP-0096 : File transfer
    QXmppTransferFileInfo fileInfo() const;
    QUrl localFileUrl() const;
    void setLocalFileUrl(const QUrl &localFileUrl);

    QDateTime fileDate() const;
    QByteArray fileHash() const;
    QString fileName() const;
    qint64 fileSize() const;

    /*!
        This signal is emitted when an error is encountered while
        processing the transfer job.
    */
    Q_SIGNAL void error(QXmppTransferJob::Error error);

    /*!
        This signal is emitted when the transfer job is finished.

        You can determine if the job completed successfully by testing whether
        error() returns QXmppTransferJob::NoError.

        Note: Do not delete the job in the slot connected to this signal,
        instead use deleteLater().
    */
    Q_SIGNAL void finished();

    /*!
        This signal is emitted when the local file URL changes.

        \a localFileUrl.
    */
    Q_SIGNAL void localFileUrlChanged(const QUrl &localFileUrl);

    /*!
        This signal is emitted to indicate the progress of this transfer job.

        \a done and \a total.
    */
    Q_SIGNAL void progress(qint64 done, qint64 total);

    /*! This signal is emitted when the transfer job changes \a state. */
    Q_SIGNAL void stateChanged(QXmppTransferJob::State state);

    Q_SLOT void abort();
    Q_SLOT void accept(const QString &filePath);
    Q_SLOT void accept(QIODevice *output);

private:
    QXmppTransferJob(const QString &jid, QXmppTransferJob::Direction direction, QXmppClient *client, QObject *parent);

    Q_SLOT void _q_terminated();

    void setState(QXmppTransferJob::State state);
    void terminate(QXmppTransferJob::Error error);

    const std::unique_ptr<QXmppTransferJobPrivate> d;
    friend class QXmppTransferManager;
    friend class QXmppTransferManagerPrivate;
    friend class QXmppTransferIncomingJob;
    friend class QXmppTransferOutgoingJob;
};

/*!
    \inmodule QXmpp

    \brief The QXmppTransferManager class provides support for sending and receiving files,
    as defined by \xep{0096}{SI File Transfer}.

    Stream initiation is performed as described in \xep{0095}{Stream Initiation}
    and \xep{0096}{SI File Transfer}. The actual file transfer is then performed
    using either \xep{0065}{SOCKS5 Bytestreams} or \xep{0047}{In-Band
    Bytestreams}.

    To make use of this manager, you need to instantiate it and load it into the
    QXmppClient instance as follows:

    \code
    auto *manager = new QXmppTransferManager;
    client->addExtension(manager);
    \endcode

    \ingroup Managers
*/
class QXMPP_EXPORT QXmppTransferManager : public QXmppClientExtension
{
    Q_OBJECT

    /*!
        \property QXmppTransferManager::proxy

        The JID of the bytestream proxy to use for outgoing transfers
    */
    Q_PROPERTY(QString proxy READ proxy WRITE setProxy)
    /*!
        \property QXmppTransferManager::proxyOnly

        Whether the proxy will systematically be used for outgoing SOCKS5 bytestream transfers
    */
    Q_PROPERTY(bool proxyOnly READ proxyOnly WRITE setProxyOnly)
    /*!
        \property QXmppTransferManager::supportedMethods

        The supported stream methods
    */
    Q_PROPERTY(QXmppTransferJob::Methods supportedMethods READ supportedMethods WRITE setSupportedMethods)

public:
    QXmppTransferManager();
    ~QXmppTransferManager() override;

    QString proxy() const;
    void setProxy(const QString &proxyJid);

    bool proxyOnly() const;
    void setProxyOnly(bool proxyOnly);

    QXmppTransferJob::Methods supportedMethods() const;
    void setSupportedMethods(QXmppTransferJob::Methods methods);

    QStringList discoveryFeatures() const override;
    bool handleStanza(const QDomElement &element) override;

    /*!
        This signal is emitted when a new file transfer offer is received.

        To accept the transfer job, call the job's QXmppTransferJob::accept() method.
        To refuse the transfer job, call the job's QXmppTransferJob::abort() method.

        \a job.
    */
    Q_SIGNAL void fileReceived(QXmppTransferJob *job);

    /*! This signal is emitted whenever a transfer \a job is started. */
    Q_SIGNAL void jobStarted(QXmppTransferJob *job);

    /*!
        This signal is emitted whenever a transfer job is finished.

        \sa QXmppTransferJob::finished()

        \a job.
    */
    Q_SIGNAL void jobFinished(QXmppTransferJob *job);

    Q_SLOT QXmppTransferJob *sendFile(const QString &jid, const QString &filePath, const QString &description = QString());
    Q_SLOT QXmppTransferJob *sendFile(const QString &jid, QIODevice *device, const QXmppTransferFileInfo &fileInfo, const QString &sid = QString());

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    Q_SLOT void _q_iqReceived(const QXmppIq &);
    Q_SLOT void _q_jobDestroyed(QObject *object);
    Q_SLOT void _q_jobError(QXmppTransferJob::Error error);
    Q_SLOT void _q_jobFinished();
    Q_SLOT void _q_jobStateChanged(QXmppTransferJob::State state);
    Q_SLOT void _q_socksServerConnected(QTcpSocket *socket, const QString &hostName, quint16 port);

    void byteStreamIqReceived(const QXmppByteStreamIq &);
    void byteStreamResponseReceived(const QXmppIq &);
    void byteStreamResultReceived(const QXmppByteStreamIq &);
    void byteStreamSetReceived(const QXmppByteStreamIq &);
    void ibbCloseIqReceived(const QXmppIbbCloseIq &);
    void ibbDataIqReceived(const QXmppIbbDataIq &);
    void ibbOpenIqReceived(const QXmppIbbOpenIq &);
    void ibbResponseReceived(const QXmppIq &);
    void streamInitiationIqReceived(const QXmppStreamInitiationIq &);
    void streamInitiationResultReceived(const QXmppStreamInitiationIq &);
    void streamInitiationSetReceived(const QXmppStreamInitiationIq &);
    void socksServerSendOffer(QXmppTransferJob *job);

    const std::unique_ptr<QXmppTransferManagerPrivate> d;

    friend class QXmppTransferManagerPrivate;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QXmppTransferJob::Methods)

#endif

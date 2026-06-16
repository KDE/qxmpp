// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef XMPPSOCKET_H
#define XMPPSOCKET_H

#include "QXmppLogger.h"

#include "StreamError.h"

#include <QAbstractSocket>
#include <QDomDocument>
#include <QSslError>
#include <QXmlStreamReader>

class QSslSocket;
class TestStream;
class tst_QXmppStream;

namespace QXmpp::Private {

struct StreamOpen;

struct ServerAddress {
    enum ConnectionType {
        Tcp,
        Tls,
    };

    ConnectionType type;
    QString host;
    quint16 port;
};

class SendDataInterface
{
public:
    virtual bool sendData(const QByteArray &) = 0;
};

class DomReader
{
public:
    struct Unfinished { };

    enum ErrorType {
        InvalidState,
        NotWellFormed,
        UnsupportedXmlFeature,
    };
    struct Error {
        ErrorType type;
        QString text;
    };

    using Result = std::variant<QDomElement, Unfinished, Error>;

    Result process(QXmlStreamReader &);

private:
    QDomDocument doc;
    QDomElement currentElement;
    uint depth = 0;
};

class QXMPP_EXPORT XmppSocket : public QXmppLoggable, public SendDataInterface
{
    Q_OBJECT
public:
    explicit XmppSocket(QObject *parent);
    XmppSocket(QSslSocket *socket, QObject *parent);
    ~XmppSocket() override = default;

    QSslSocket *internalSocket() const { return m_socket; }
    void resetInternalSocket();

    bool isConnected() const;
    void connectToHost(const ServerAddress &);
    void disconnectFromHost();
    bool sendData(const QByteArray &) override;
    void resetStream();
    bool isStreamReceived() const { return m_streamReceived; }

    Q_SIGNAL void started();
    Q_SIGNAL void disconnected();
    Q_SIGNAL void stanzaReceived(const QDomElement &);
    Q_SIGNAL void streamReceived(const QXmpp::Private::StreamOpen &);
    Q_SIGNAL void streamClosed();
    Q_SIGNAL void errorOccurred(const QString &text, std::variant<StreamError, QAbstractSocket::SocketError> condition);
    Q_SIGNAL void sslErrorsOccurred(const QList<QSslError> &errors);
    // TODO: replace with own connection state
    Q_SIGNAL void internalSocketStateChanged();

private:
    void setSocket(QSslSocket *socket);
    void throwError(const QString &text, StreamError condition);
    void processData(const QString &data);
    QString slice(qint64 absStart, qint64 absEnd) const;
    qint64 unitStart() const;

    friend class ::tst_QXmppStream;

    QXmlStreamReader m_reader;
    std::optional<DomReader> m_domReader;
    QString m_buffer;                // exact decoded on-wire text not yet discarded
    qint64 m_bufferStartOffset = 0;  // absolute char offset of m_buffer[0]
    qint64 m_lastUnitEnd = 0;        // absolute offset just past the last handled top-level unit (or XML declaration)
    qint64 m_stanzaStartOffset = 0;  // absolute char offset of '<' of the stanza currently being parsed
    bool m_streamReceived = false;
    bool m_directTls = false;
    bool m_acceptInput = true;

    QSslSocket *m_socket = nullptr;
};

}  // namespace QXmpp::Private

#endif  // XMPPSOCKET_H

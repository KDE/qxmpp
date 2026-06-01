// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPOUTGOINGSERVER_H
#define QXMPPOUTGOINGSERVER_H

#include "QXmppLogger.h"
#include "QXmppStreamError.h"

#include <QAbstractSocket>

class QDomElement;
class QSslError;
class QXmppDialback;
class QXmppNonza;
class QXmppOutgoingServer;
class QXmppOutgoingServerPrivate;

namespace QXmpp::Private {
struct StreamOpen;
}

/*!
    \inmodule QXmpp

    \brief The QXmppOutgoingServer class represents an outgoing XMPP stream
    to another XMPP server.
*/
class QXMPP_EXPORT QXmppOutgoingServer : public QXmppLoggable
{
    Q_OBJECT

public:
    QXmppOutgoingServer(const QString &domain, QObject *parent);
    ~QXmppOutgoingServer() override;

    bool isConnected() const;
    Q_SLOT void connectToHost(const QString &domain);
    void disconnectFromHost();
    Q_SLOT void queueData(const QByteArray &data);

    /*! This signal is emitted when the stream is connected. */
    Q_SIGNAL void connected();
    /*! This signal is emitted when the stream is disconnected. */
    Q_SIGNAL void disconnected();

    bool sendData(const QByteArray &);
    bool sendPacket(const QXmppNonza &);

    QString localStreamKey() const;
    void setLocalStreamKey(const QString &key);
    void setVerify(const QString &id, const QString &key);

    QString remoteDomain() const;

    /*! This signal is emitted when a dialback verify \a response is received. */
    Q_SIGNAL void dialbackResponseReceived(const QXmppDialback &response);

private:
    void handleStart();
    void handleStream(const QXmpp::Private::StreamOpen &streamElement);
    void handleStanza(const QDomElement &stanzaElement);

    void onDnsLookupFinished();
    void onSocketDisconnected();
    void sendDialback();
    void slotSslErrors(const QList<QSslError> &errors);
    void onSocketError(const QString &text, std::variant<QXmpp::StreamError, QAbstractSocket::SocketError> error);

    const std::unique_ptr<QXmppOutgoingServerPrivate> d;
};

#endif

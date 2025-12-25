// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPTRANSFERMANAGER_P_H
#define QXMPPTRANSFERMANAGER_P_H

#include "QXmppByteStreamIq.h"
#include "QXmppTransferManager.h"

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.  It exists for the convenience
// of the QXmppTransferManager class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

class QTimer;
class QXmppSocksClient;

class QXmppTransferIncomingJob : public QXmppTransferJob
{
    Q_OBJECT

public:
    QXmppTransferIncomingJob(const QString &jid, QXmppClient *client, QObject *parent);
    void checkData();
    void connectToHosts(const QXmppByteStreamIq &iq);
    bool writeData(const QByteArray &data);

private:
    Q_SLOT void _q_candidateDisconnected();
    Q_SLOT void _q_candidateReady();
    Q_SLOT void _q_disconnected();
    Q_SLOT void _q_receiveData();

    void connectToNextHost();

    QXmppByteStreamIq::StreamHost m_candidateHost;
    QXmppSocksClient *m_candidateClient;
    QTimer *m_candidateTimer;
    QList<QXmppByteStreamIq::StreamHost> m_streamCandidates;
    QString m_streamOfferId;
    QString m_streamOfferFrom;
};

class QXmppTransferOutgoingJob : public QXmppTransferJob
{
    Q_OBJECT

public:
    QXmppTransferOutgoingJob(const QString &jid, QXmppClient *client, QObject *parent);
    void connectToProxy();
    void startSending();

    Q_SLOT void _q_disconnected();

private:
    Q_SLOT void _q_proxyReady();
    Q_SLOT void _q_sendData();
};

#endif

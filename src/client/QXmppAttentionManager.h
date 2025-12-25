// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPATTENTIONMANAGER_H
#define QXMPPATTENTIONMANAGER_H

#include "QXmppClientExtension.h"

#include <QTime>

class QXmppAttentionManagerPrivate;
class QXmppMessage;

class QXMPP_EXPORT QXmppAttentionManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    QXmppAttentionManager(quint8 allowedAttempts = 3, QTime timeFrame = QTime(0, 15, 0));
    ~QXmppAttentionManager();

    QStringList discoveryFeatures() const override;

    quint8 allowedAttempts() const;
    void setAllowedAttempts(quint8 allowedAttempts);

    QTime allowedAttemptsTimeInterval() const;
    void setAllowedAttemptsTimeInterval(QTime interval);

    Q_SLOT QString requestAttention(const QString &jid, const QString &message = {});

    Q_SIGNAL void attentionRequested(const QXmppMessage &message, bool isTrusted);
    Q_SIGNAL void attentionRequestRateLimited(const QXmppMessage &message);

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    Q_SLOT void handleMessageReceived(const QXmppMessage &message);

    const std::unique_ptr<QXmppAttentionManagerPrivate> d;
};

#endif  // QXMPPATTENTIONMANAGER_H

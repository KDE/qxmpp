// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALLMANAGER_H
#define QXMPPCALLMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppLogger.h"

class QHostAddress;
class QXmppCall;
class QXmppCallManagerPrivate;
class QXmppIq;
class QXmppJingleIq;
class QXmppPresence;

class QXMPP_EXPORT QXmppCallManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    QXmppCallManager();
    ~QXmppCallManager() override;

    void setStunServers(const QList<QPair<QHostAddress, quint16>> &servers);
    void setStunServer(const QHostAddress &host, quint16 port = 3478);
    void setTurnServer(const QHostAddress &host, quint16 port = 3478);
    void setTurnUser(const QString &user);
    void setTurnPassword(const QString &password);
    bool dtlsRequired() const;
    void setDtlsRequired(bool);

    /// \cond
    QStringList discoveryFeatures() const override;
    bool handleStanza(const QDomElement &element) override;
    /// \endcond

    Q_SIGNAL void callReceived(std::unique_ptr<QXmppCall> &call);

    std::unique_ptr<QXmppCall> call(const QString &jid);

protected:
    /// \cond
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;
    /// \endcond

private:
    void onCallDestroyed(QObject *object);
    void onDisconnected();
    std::variant<QXmppIq, QXmppStanza::Error> handleIq(QXmppJingleIq &&iq);
    void onPresenceReceived(const QXmppPresence &presence);

    const std::unique_ptr<QXmppCallManagerPrivate> d;
    friend class QXmppCall;
    friend class QXmppCallPrivate;
    friend class QXmppCallManagerPrivate;
    friend class tst_QXmppCallManager;
};

#endif

// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMOVEDMANAGER_H
#define QXMPPMOVEDMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppSendResult.h"
#include "QXmppTask.h"

class QXmppPresence;
struct QXmppError;
class QXmppMovedManagerPrivate;

class QXMPP_EXPORT QXmppMovedManager : public QXmppClientExtension
{
    Q_OBJECT
    Q_PROPERTY(bool supportedByServer READ supportedByServer NOTIFY supportedByServerChanged)

public:
    using Result = std::variant<QXmpp::Success, QXmppError>;

    explicit QXmppMovedManager();
    ~QXmppMovedManager() override;

    QStringList discoveryFeatures() const override;

    QXmppDiscoFeatureWatch watchServerSupport() const;

#if QXMPP_DEPRECATED_SINCE(1, 17)
    [[deprecated("Use watchServerSupport()")]]
    bool supportedByServer() const;
    [[deprecated("Use watchServerSupport()")]]
    Q_SIGNAL void supportedByServerChanged();
#endif

    QXmppTask<Result> publishStatement(QString newBareJid);
    QXmppTask<Result> verifyStatement(QString oldBareJid, QString newBareJid);

    QXmppTask<QXmpp::SendResult> notifyContact(const QString &contactBareJid, const QString &oldBareJid, bool sensitive = true, const QString &reason = {});

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    QXmppTask<QXmppPresence> processSubscriptionRequest(QXmppPresence presence);

    const std::unique_ptr<QXmppMovedManagerPrivate> d;

    friend class QXmppRosterManager;
    friend class tst_QXmppMovedManager;
};

#endif  // QXMPPMOVEDMANAGER_H

// SPDX-FileCopyrightText: 2023 Tibor Csötönyi <work@taibsu.de>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPJINGLEMESSAGEINITIATIONMANAGER_H
#define QXMPPJINGLEMESSAGEINITIATIONMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppError.h"
#include "QXmppJingleIq.h"
#include "QXmppMessageHandler.h"
#include "QXmppSendResult.h"
#include "QXmppTask.h"

class QXmppJingleMessageInitiationManager;
class QXmppJingleMessageInitiationPrivate;
class QXmppJingleMessageInitiationManagerPrivate;

class QXMPP_EXPORT QXmppJingleMessageInitiation : public QObject
{
    Q_OBJECT
public:
    struct Rejected {
        std::optional<QXmppJingleReason> reason;
        bool containsTieBreak;
    };

    struct Retracted {
        std::optional<QXmppJingleReason> reason;
        bool containsTieBreak;
    };

    struct Finished {
        std::optional<QXmppJingleReason> reason;
        QString migratedTo;
    };

    /// Variant of Rejected, Retracted, Finished or Error result types
    using Result = std::variant<Rejected, Retracted, Finished, QXmppError>;

    QXmppJingleMessageInitiation(QXmppJingleMessageInitiationManager *manager);
    ~QXmppJingleMessageInitiation();

    const QString &remoteJid() const;

    QXmppTask<QXmpp::SendResult> ring();
    QXmppTask<QXmpp::SendResult> proceed();
    QXmppTask<QXmpp::SendResult> reject(std::optional<QXmppJingleReason> reason, bool containsTieBreak = false);
    QXmppTask<QXmpp::SendResult> retract(std::optional<QXmppJingleReason> reason, bool containsTieBreak = false);
    QXmppTask<QXmpp::SendResult> finish(std::optional<QXmppJingleReason> reason, const QString &migratedTo = {});

    Q_SIGNAL void ringing();
    Q_SIGNAL void proceeded(const QString &id, const QString &remoteResource);
    Q_SIGNAL void closed(const Result &result);

private:
    QString id() const;
    void setId(const QString &id);
    void setRemoteJid(const QString &remoteJid);
    bool isProceeded() const;
    void setIsProceeded(bool isProceeded);

    std::unique_ptr<QXmppJingleMessageInitiationPrivate> d;

    friend class QXmppJingleMessageInitiationManager;
    friend class tst_QXmppJingleMessageInitiationManager;
};

class QXMPP_EXPORT QXmppJingleMessageInitiationManager : public QXmppClientExtension, public QXmppMessageHandler
{
    Q_OBJECT
public:
    using ProposeResult = std::variant<std::shared_ptr<QXmppJingleMessageInitiation>, QXmppError>;

    QXmppJingleMessageInitiationManager();
    ~QXmppJingleMessageInitiationManager();

    /// \cond
    QStringList discoveryFeatures() const override;
    /// \endcond

    QXmppTask<ProposeResult> propose(
        const QString &remoteJid,
        const QXmppJingleRtpDescription &description);

    Q_SIGNAL void proposed(
        const std::shared_ptr<QXmppJingleMessageInitiation> &jmi,
        const QString &id,
        const std::optional<QXmppJingleRtpDescription> &description);

protected:
    /// \cond
    bool handleMessage(const QXmppMessage &) override;
    /// \endcond

private:
    QXmppTask<QXmpp::SendResult> sendMessage(
        const QXmppJingleMessageInitiationElement &jmiElement,
        const QString &remoteJid);

    void clear(const std::shared_ptr<QXmppJingleMessageInitiation> &jmi);
    void clearAll();

    bool handleJmiElement(QXmppJingleMessageInitiationElement &&jmiElement, const QString &senderJid);
    bool handleExistingJmi(const std::shared_ptr<QXmppJingleMessageInitiation> &existingJmi, const QXmppJingleMessageInitiationElement &jmiElement, const QString &remoteResource);
    bool handleProposeJmiElement(const QXmppJingleMessageInitiationElement &jmiElement, const QString &remoteJid);
    bool handleTieBreak(const std::shared_ptr<QXmppJingleMessageInitiation> &existingJmi, const QXmppJingleMessageInitiationElement &jmiElement, const QString &remoteResource);
    bool handleExistingSession(const std::shared_ptr<QXmppJingleMessageInitiation> &existingJmi, const QString &jmiElementId);
    bool handleNonExistingSession(const std::shared_ptr<QXmppJingleMessageInitiation> &existingJmi, const QString &jmiElementId, const QString &remoteResource);
    std::shared_ptr<QXmppJingleMessageInitiation> addJmi(const QString &remoteJid);
    const QVector<std::shared_ptr<QXmppJingleMessageInitiation>> &jmis() const;

private:
    std::unique_ptr<QXmppJingleMessageInitiationManagerPrivate> d;

    friend class QXmppJingleMessageInitiationPrivate;
    friend class tst_QXmppJingleMessageInitiationManager;
};

Q_DECLARE_METATYPE(QXmppJingleMessageInitiation::Result)

#endif  // QXMPPJINGLEMESSAGEINITIATIONMANAGER_H

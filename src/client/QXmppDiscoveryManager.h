// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_H
#define QXMPPDISCOVERYMANAGER_H

#include "QXmppClientExtension.h"

#include <variant>

#include <QDateTime>

template<typename T>
class QXmppTask;
class QXmppDataForm;
class QXmppDiscoveryIq;
class QXmppDiscoveryManagerPrivate;
struct QXmppError;

class QXMPP_EXPORT QXmppDiscoveryManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    /// Policies for how cached service discovery information is used.
    enum class CachePolicy {
        /// Always ensure the data is up-to-date. Cached data may be used only if it is guaranteed
        /// to be current (e.g. via entity capabilities).
        Strict,
        /// Cached data may be used even if it is not guaranteed to be current, within the
        /// configured limits.
        Relaxed,
    };

    QXmppDiscoveryManager();
    ~QXmppDiscoveryManager() override;

    QXmppTask<QXmpp::Result<QXmppDiscoInfo>> info(const QString &jid, const QString &node = {}, CachePolicy fetchPolicy = CachePolicy::Relaxed);
    QXmppTask<QXmpp::Result<QList<QXmppDiscoItem>>> items(const QString &jid, const QString &node = {}, CachePolicy fetchPolicy = CachePolicy::Relaxed);

    const QList<QXmppDiscoIdentity> &identities() const;
    void setIdentities(const QList<QXmppDiscoIdentity> &identities);

    const QList<QXmppDataForm> &infoForms() const;
    void setInfoForms(const QList<QXmppDataForm> &dataForms);

    QString clientCapabilitiesNode() const;
    void setClientCapabilitiesNode(const QString &);

    QXmppDiscoInfo buildClientInfo() const;

    /// \cond
    QStringList discoveryFeatures() const override;
    bool handleStanza(const QDomElement &element) override;
    /// \endcond

    /// This signal is emitted when an information response is received.
    Q_SIGNAL void infoReceived(const QXmppDiscoveryIq &);

    /// This signal is emitted when an items response is received.
    Q_SIGNAL void itemsReceived(const QXmppDiscoveryIq &);

#if QXMPP_DEPRECATED_SINCE(1, 12)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    using InfoResult = std::variant<QXmppDiscoveryIq, QXmppError>;
    using ItemsResult = std::variant<QList<QXmppDiscoveryIq::Item>, QXmppError>;
    [[deprecated("Use info()")]]
    QXmppTask<InfoResult> requestDiscoInfo(const QString &jid, const QString &node = {});
    [[deprecated("Use items()")]]
    QXmppTask<ItemsResult> requestDiscoItems(const QString &jid, const QString &node = {});
    QT_WARNING_POP

    [[deprecated("Use buildClientInfo()")]]
    QXmppDiscoveryIq capabilities();

    [[deprecated("Use identities()")]]
    QString clientCategory() const;
    [[deprecated("Use setIdentities()")]]
    void setClientCategory(const QString &);

    [[deprecated("Use identities()")]]
    void setClientName(const QString &);
    [[deprecated("Use setIdentities()")]]
    QString clientApplicationName() const;

    [[deprecated("Use identities()")]]
    QString clientType() const;
    [[deprecated("Use setIdentities()")]]
    void setClientType(const QString &);

    [[deprecated("Use infoForms()")]]
    QXmppDataForm clientInfoForm() const;
    [[deprecated("Use setInfoForms()()")]]
    void setClientInfoForm(const QXmppDataForm &form);

    [[deprecated("Use info()")]]
    QString requestInfo(const QString &jid, const QString &node = QString());
    [[deprecated("Use items()")]]
    QString requestItems(const QString &jid, const QString &node = QString());
#endif

protected:
    void onRegistered(QXmppClient *client);
    void onUnregistered(QXmppClient *client);

private:
    friend class QXmppDiscoveryManagerPrivate;
    const std::unique_ptr<QXmppDiscoveryManagerPrivate> d;
};

#endif  // QXMPPDISCOVERYMANAGER_H

// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_H
#define QXMPPDISCOVERYMANAGER_H

#include "QXmppClientExtension.h"

#include <variant>

template<typename T>
class QXmppTask;
class QXmppDataForm;
class QXmppDiscoveryIq;
class QXmppDiscoveryManagerPrivate;
struct QXmppError;

///
/// \brief The QXmppDiscoveryManager class makes it possible to discover information about other
/// entities as defined by \xep{0030, Service Discovery}.
///
/// \ingroup Managers
///
class QXMPP_EXPORT QXmppDiscoveryManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    QXmppDiscoveryManager();
    ~QXmppDiscoveryManager() override;

    QXmppTask<QXmpp::Result<QXmppDiscoInfo>> info(const QString &jid, const QString &node = {});
    QXmppTask<QXmpp::Result<QList<QXmppDiscoItem>>> items(const QString &jid, const QString &node = {});

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
    using InfoResult [[deprecated]] = std::variant<QXmppDiscoveryIq, QXmppError>;
    using ItemsResult [[deprecated]] = std::variant<QList<QXmppDiscoveryIq::Item>, QXmppError>;
    [[deprecated("Use info()")]]
    QXmppTask<InfoResult> requestDiscoInfo(const QString &jid, const QString &node = {});
    [[deprecated("Use items()")]]
    QXmppTask<ItemsResult> requestDiscoItems(const QString &jid, const QString &node = {});
    QT_WARNING_POP

    [[deprecated("Use buildDiscoInfo()")]]
    QXmppDiscoveryIq capabilities();

    [[deprecated("Use ownIdentities()")]]
    QString clientCategory() const;
    [[deprecated("Use setOwnIdentities()")]]
    void setClientCategory(const QString &);

    [[deprecated("Use ownIdentities()")]]
    void setClientName(const QString &);
    [[deprecated("Use setOwnIdentities()")]]
    QString clientApplicationName() const;

    [[deprecated("Use ownIdentities()")]]
    QString clientType() const;
    [[deprecated("Use setOwnIdentities()")]]
    void setClientType(const QString &);

    [[deprecated("Use ownDataForms()")]]
    QXmppDataForm clientInfoForm() const;
    [[deprecated("Use setOwnDataForms()")]]
    void setClientInfoForm(const QXmppDataForm &form);

    [[deprecated("Use requestDiscoInfo")]]
    QString requestInfo(const QString &jid, const QString &node = QString());
    [[deprecated("Use requestDiscoItems")]]
    QString requestItems(const QString &jid, const QString &node = QString());
#endif

private:
    friend class QXmppDiscoveryManagerPrivate;
    const std::unique_ptr<QXmppDiscoveryManagerPrivate> d;
};

#endif  // QXMPPDISCOVERYMANAGER_H

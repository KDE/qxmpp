// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPRPCMANAGER_H
#define QXMPPRPCMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppInvokable.h"
#include "QXmppRemoteMethod.h"

#include <QMap>
#include <QVariant>

class QXmppRpcErrorIq;
class QXmppRpcInvokeIq;
class QXmppRpcResponseIq;

/// \cond
#if QXMPP_DEPRECATED_SINCE(1, 12)
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

///
/// \brief The QXmppRpcManager class make it possible to invoke remote methods
/// and to expose local interfaces for remote procedure calls, as specified by
/// \xep{0009, Jabber-RPC}.
///
/// To make use of this manager, you need to instantiate it and load it into
/// the QXmppClient instance as follows:
///
/// \code
/// QXmppRpcManager *manager = new QXmppRpcManager;
/// client->addExtension(manager);
/// \endcode
///
/// \ingroup Managers
///
/// \deprecated
///
class QXMPP_EXPORT Q_DECL_DEPRECATED_X("Removed from public API (unmaintained)") QXmppRpcManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    QXmppRpcManager();

    void addInvokableInterface(QXmppInvokable *interface);
    QXmppRemoteMethodResult callRemoteMethod(const QString &jid,
                                             const QString &interface,
                                             const QVariant &arg1 = QVariant(),
                                             const QVariant &arg2 = QVariant(),
                                             const QVariant &arg3 = QVariant(),
                                             const QVariant &arg4 = QVariant(),
                                             const QVariant &arg5 = QVariant(),
                                             const QVariant &arg6 = QVariant(),
                                             const QVariant &arg7 = QVariant(),
                                             const QVariant &arg8 = QVariant(),
                                             const QVariant &arg9 = QVariant(),
                                             const QVariant &arg10 = QVariant());

    QStringList discoveryFeatures() const override;
    QList<QXmppDiscoIdentity> discoveryIdentities() const override;
    bool handleStanza(const QDomElement &element) override;

    Q_SIGNAL void rpcCallResponse(const QXmppRpcResponseIq &result);
    Q_SIGNAL void rpcCallError(const QXmppRpcErrorIq &err);

private:
    void invokeInterfaceMethod(const QXmppRpcInvokeIq &iq);

    QMap<QString, QXmppInvokable *> m_interfaces;
};

QT_WARNING_POP
#endif
/// \endcond

#endif

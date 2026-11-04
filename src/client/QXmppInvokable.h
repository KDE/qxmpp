// SPDX-FileCopyrightText: 2009 Ian Reinhart Geiser <geiseri@kde.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPINVOKABLE_H
#define QXMPPINVOKABLE_H

#include "QXmppGlobal.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QWriteLocker>

class QXMPP_EXPORT QXmppInvokable : public QObject
{
    Q_OBJECT
public:
    QXmppInvokable(QObject *parent = nullptr);

    ~QXmppInvokable() override;

    QVariant dispatch(const QByteArray &method, const QList<QVariant> &args = QList<QVariant>());

    static QList<QByteArray> paramTypes(const QList<QVariant> &params);

    virtual bool isAuthorized(const QString &jid) const = 0;

    Q_SLOT QStringList interfaces() const;

private:
    void buildMethodHash();
    QHash<QByteArray, int> m_methodHash;
    QReadWriteLock m_lock;
};

#endif

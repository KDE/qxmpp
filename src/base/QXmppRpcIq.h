// SPDX-FileCopyrightText: 2009 Ian Reinhart Geiser <geiseri@kde.org>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPRPCIQ_H
#define QXMPPRPCIQ_H

#include "QXmppIq.h"

#include <QVariant>

class QXMPP_EXPORT QXmppRpcMarshaller
{
public:
    static void marshall(QXmlStreamWriter *writer, const QVariant &value);
    static QVariant demarshall(const QDomElement &elem, QStringList &errors);
};

/*!
    \inmodule QXmpp

    \brief The QXmppRpcResponseIq class represents an IQ used to carry
    an RPC response as specified by \xep{0009}{Jabber-RPC}.

    \ingroup Stanzas
*/

class QXMPP_EXPORT QXmppRpcResponseIq : public QXmppIq
{
public:
    QXmppRpcResponseIq();

    int faultCode() const;
    void setFaultCode(int faultCode);

    QString faultString() const;
    void setFaultString(const QString &faultString);

    QVariantList values() const;
    void setValues(const QVariantList &values);

    static bool isRpcResponseIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    int m_faultCode;
    QString m_faultString;
    QVariantList m_values;
};

/*!
    \inmodule QXmpp

    \brief The QXmppRpcInvokeIq class represents an IQ used to carry
    an RPC invocation as specified by \xep{0009}{Jabber-RPC}.

    \ingroup Stanzas
*/

class QXMPP_EXPORT QXmppRpcInvokeIq : public QXmppIq
{
public:
    QXmppRpcInvokeIq();

    QString method() const;
    void setMethod(const QString &method);

    QVariantList arguments() const;
    void setArguments(const QVariantList &arguments);

    static bool isRpcInvokeIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QVariantList m_arguments;
    QString m_method;

    friend class QXmppRpcErrorIq;
};

class QXMPP_EXPORT QXmppRpcErrorIq : public QXmppIq
{
public:
    QXmppRpcErrorIq();

    QXmppRpcInvokeIq query() const;
    void setQuery(const QXmppRpcInvokeIq &query);

    static bool isRpcErrorIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QXmppRpcInvokeIq m_query;
};

#endif  // QXMPPRPCIQ_H

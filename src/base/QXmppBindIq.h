// SPDX-FileCopyrightText: 2011 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPBINDIQ_H
#define QXMPPBINDIQ_H

#include "QXmppIq.h"

/// \cond
#if QXMPP_DEPRECATED_SINCE(1, 12)
class QXMPP_EXPORT Q_DECL_DEPRECATED_X("Removed from public API") QXmppBindIq : public QXmppIq
{
public:
    static QXmppBindIq bindAddressIq(const QString &resource);

    QString jid() const;
    void setJid(const QString &);

    QString resource() const;
    void setResource(const QString &);

    static constexpr std::tuple PayloadXmlTag = { u"bind", QXmpp::Private::ns_bind };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isBindIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QString m_jid;
    QString m_resource;
};
#endif
/// \endcond

#endif  // QXMPPBIND_H

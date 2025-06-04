// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPINGIQ_H
#define QXMPPPINGIQ_H

#include "QXmppIq.h"

class QXMPP_EXPORT QXmppPingIq : public QXmppIq
{
public:
    QXmppPingIq();

    /// \cond
    static constexpr std::tuple PayloadXmlTag = { u"ping", QXmpp::Private::ns_ping };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isPingIq(const QDomElement &element);

    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;
    /// \endcond
};

#endif

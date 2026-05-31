// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPingIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

/*!
    \class QXmppPingIq
    \inmodule QXmpp

    QXmppPingIq represents a Ping IQ as defined by \xep{0199}{XMPP Ping}.

    \ingroup Stanzas
*/

QXmppPingIq::QXmppPingIq()
    : QXmppIq(QXmppIq::Get)
{
}

void QXmppPingIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { PayloadXmlTag });
}

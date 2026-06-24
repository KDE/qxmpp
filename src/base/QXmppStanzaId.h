// SPDX-FileCopyrightText: 2018 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTANZAID_H
#define QXMPPSTANZAID_H

#include "QXmppConstants_p.h"

#include <optional>
#include <tuple>

#include <QString>

class QDomElement;
class QXmlStreamWriter;

/*!
    \struct QXmppStanzaId
    \inmodule QXmpp

    \brief Stanza ID element as defined in \xep{0359}{Unique and Stable Stanza IDs}.

    \since QXmpp 1.8
*/
struct QXmppStanzaId {
    /*! Identifier of the stanza element */
    QString id;
    /*! JID of the generating entity */
    QString by;

    static constexpr std::tuple XmlTag = { u"stanza-id", QXmpp::Private::ns_sid };
    static std::optional<QXmppStanzaId> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

#endif  // QXMPPSTANZAID_H

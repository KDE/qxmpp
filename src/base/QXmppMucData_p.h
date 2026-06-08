// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCDATA_P_H
#define QXMPPMUCDATA_P_H

#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppMucData.h"

#include <optional>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp::Private {

// IQ payload for muc#admin requests (XEP-0045 §9).
// Used to get or modify role/affiliation lists via GetIq or SetIq.
struct MucAdminQuery {
    QList<QXmpp::Muc::Item> items;

    static constexpr std::tuple XmlTag = { u"query", ns_muc_admin };
    static std::optional<MucAdminQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

// IQ payload for muc#owner requests (XEP-0045 §10).
// Used to fetch, submit, or cancel a room configuration form, or to destroy a room.
struct MucOwnerQuery {
    std::optional<QXmppDataForm> form;
    QString destroyAlternateJid;
    QString destroyReason;

    static constexpr std::tuple XmlTag = { u"query", ns_muc_owner };
    static std::optional<MucOwnerQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

// IQ payload for jabber:iq:register queries to a MUC room (XEP-0045 §7.10).
struct MucRegisterQuery {
    std::optional<QXmppDataForm> form;
    bool isRegistered = false;

    static constexpr std::tuple XmlTag = { u"query", ns_register };
    static std::optional<MucRegisterQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

struct MucUniqueQuery {
    QString name;

    static constexpr std::tuple XmlTag = { u"unique", ns_muc_unique };
    static std::optional<MucUniqueQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

}  // namespace QXmpp::Private

#endif  // QXMPPMUCDATA_P_H

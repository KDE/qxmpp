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

///
/// \brief IQ payload for muc\#admin requests (XEP-0045 §9).
///
/// Used to get or modify role/affiliation lists via \c GetIq or \c SetIq.
/// Each item carries a JID or nickname, a role or affiliation, and optionally a reason.
///
struct MucAdminQuery {
    QList<QXmpp::Muc::Item> items;

    static constexpr std::tuple XmlTag = { u"query", ns_muc_admin };
    static std::optional<MucAdminQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

///
/// \brief IQ payload for muc\#owner requests (XEP-0045 §10).
///
/// Used to fetch, submit, or cancel a room configuration form, or to destroy a room.
/// Exactly one of \e form or the destroy fields should be set at a time.
///
struct MucOwnerQuery {
    /// The room configuration data form (Submit or Cancel type for SET; populated on GET result).
    std::optional<QXmppDataForm> form;
    /// JID of an alternate room when destroying (may be empty).
    QString destroyAlternateJid;
    /// Human-readable reason when destroying (may be empty).
    QString destroyReason;

    static constexpr std::tuple XmlTag = { u"query", ns_muc_owner };
    static std::optional<MucOwnerQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

}  // namespace QXmpp::Private

#endif  // QXMPPMUCDATA_P_H

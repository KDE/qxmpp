// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucData.h"

#include "QXmppConstants_p.h"
#include "QXmppMucData_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmpp::Muc::Affiliation> {
    using enum QXmpp::Muc::Affiliation;
    static constexpr auto Values = makeValues<QXmpp::Muc::Affiliation>({
        { Outcast, u"outcast" },
        { None, u"none" },
        { Member, u"member" },
        { Admin, u"admin" },
        { Owner, u"owner" },
    });
};

template<>
struct Enums::Data<QXmpp::Muc::Role> {
    using enum QXmpp::Muc::Role;
    static constexpr auto Values = makeValues<QXmpp::Muc::Role>({
        { None, u"none" },
        { Visitor, u"visitor" },
        { Participant, u"participant" },
        { Moderator, u"moderator" },
    });
};

namespace QXmpp::Muc {

/// \cond
std::optional<HistoryOptions> HistoryOptions::fromDom(const QDomElement &el)
{
    HistoryOptions opts;
    auto parseInt = [&](const QString &name) -> std::optional<int> {
        if (!el.hasAttribute(name)) {
            return std::nullopt;
        }
        return el.attribute(name).toInt();
    };
    opts.m_maxChars = parseInt(u"maxchars"_s);
    opts.m_maxStanzas = parseInt(u"maxstanzas"_s);
    opts.m_seconds = parseInt(u"seconds"_s);
    if (el.hasAttribute(u"since"_s)) {
        opts.m_since = QXmppUtils::datetimeFromString(el.attribute(u"since"_s));
    }
    return opts;
}

void HistoryOptions::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"history",
        OptionalAttribute { u"maxchars", m_maxChars },
        OptionalAttribute { u"maxstanzas", m_maxStanzas },
        OptionalAttribute { u"seconds", m_seconds },
        OptionalAttribute { u"since", m_since },
    });
}
/// \endcond

/// \cond
std::optional<Destroy> Destroy::fromDom(const QDomElement &el)
{
    Destroy d;
    d.m_alternateRoom = el.attribute(u"jid"_s);
    d.m_reason = el.firstChildElement(u"reason"_s).text();
    return d;
}

void Destroy::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"destroy",
        OptionalAttribute { u"jid", m_alternateRoom },
        OptionalTextElement { u"reason", m_reason },
    });
}
/// \endcond

/// \cond
std::optional<Item> Item::fromDom(const QDomElement &el)
{
    Item item;
    item.m_jid = el.attribute(u"jid"_s);
    item.m_nick = el.attribute(u"nick"_s);
    item.m_affiliation = Enums::fromString<Affiliation>(el.attribute(u"affiliation"_s));
    item.m_role = Enums::fromString<Role>(el.attribute(u"role"_s));
    item.m_reason = el.firstChildElement(u"reason"_s).text();
    item.m_actor = el.firstChildElement(u"actor"_s).attribute(u"jid"_s);
    return item;
}

void Item::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        u"item",
        OptionalAttribute { u"affiliation", m_affiliation },
        OptionalAttribute { u"jid", m_jid },
        OptionalAttribute { u"nick", m_nick },
        OptionalAttribute { u"role", m_role },
        OptionalContent { !m_actor.isEmpty(), Element { u"actor", Attribute { u"jid", m_actor } } },
        OptionalTextElement { u"reason", m_reason },
    });
}
/// \endcond

/// \cond
std::optional<Invite> Invite::fromDom(const QDomElement &el)
{
    Invite invite;
    invite.m_from = el.attribute(u"from"_s);
    invite.m_to = el.attribute(u"to"_s);
    invite.m_reason = el.firstChildElement(u"reason"_s).text();
    return invite;
}

void Invite::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"invite",
        OptionalAttribute { u"to", m_to },
        OptionalAttribute { u"from", m_from },
        OptionalTextElement { u"reason", m_reason },
    });
}
/// \endcond

/// \cond
std::optional<Decline> Decline::fromDom(const QDomElement &el)
{
    Decline decline;
    decline.m_from = el.attribute(u"from"_s);
    decline.m_to = el.attribute(u"to"_s);
    decline.m_reason = el.firstChildElement(u"reason"_s).text();
    return decline;
}

void Decline::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"decline",
        OptionalAttribute { u"to", m_to },
        OptionalAttribute { u"from", m_from },
        OptionalTextElement { u"reason", m_reason },
    });
}
/// \endcond

/// \cond
std::optional<UserQuery> UserQuery::fromDom(const QDomElement &el)
{
    UserQuery ue;
    ue.m_statusCodes = transformFilter<QList<uint32_t>>(parseSingleAttributeElements<QList<QString>>(el, u"status", ns_muc_user, u"code"_s), [](const auto &string) {
        return parseInt<uint32_t>(string);
    });
    ue.m_invite = parseOptionalChildElement<Invite>(el);
    ue.m_password = firstChildElement(el, u"password").text();
    ue.m_decline = parseOptionalChildElement<Decline>(el);
    return ue;
}

void UserQuery::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        Tag { u"x", ns_muc_user },
        SingleAttributeElements { u"status", u"code", m_statusCodes },
        m_invite,
        OptionalTextElement { u"password", m_password },
        m_decline,
    });
}
/// \endcond

}  // namespace QXmpp::Muc

namespace QXmpp::Private {

/// \cond
std::optional<MucAdminQuery> MucAdminQuery::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return {};
    }
    return MucAdminQuery { parseChildElements<QList<QXmpp::Muc::Item>>(el) };
}

void MucAdminQuery::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { Tag { u"query", ns_muc_admin }, items });
}
/// \endcond

/// \cond
std::optional<MucOwnerQuery> MucOwnerQuery::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return {};
    }
    MucOwnerQuery q;
    q.form = parseOptionalChildElement<QXmppDataForm>(el);
    const auto destroyEl = el.firstChildElement(u"destroy"_s);
    if (!destroyEl.isNull()) {
        q.destroyAlternateJid = destroyEl.attribute(u"jid"_s);
        q.destroyReason = destroyEl.firstChildElement(u"reason"_s).text();
    }
    return q;
}

void MucOwnerQuery::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        Tag { u"query", ns_muc_owner },
        form,
        OptionalContent {
            !destroyAlternateJid.isEmpty() || !destroyReason.isEmpty(),
            Element {
                u"destroy",
                OptionalAttribute { u"jid", destroyAlternateJid },
                OptionalTextElement { u"reason", destroyReason },
            },
        },
    });
}
/// \endcond

}  // namespace QXmpp::Private

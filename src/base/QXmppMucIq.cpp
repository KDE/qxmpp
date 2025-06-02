// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppMucItem::Affiliation> {
    using enum QXmppMucItem::Affiliation;
    static constexpr auto NullValue = UnspecifiedAffiliation;
    static constexpr auto Values = makeValues<QXmppMucItem::Affiliation>({
        { UnspecifiedAffiliation, {} },
        { OutcastAffiliation, u"outcast" },
        { NoAffiliation, u"none" },
        { MemberAffiliation, u"member" },
        { AdminAffiliation, u"admin" },
        { OwnerAffiliation, u"owner" },
    });
};

template<>
struct Enums::Data<QXmppMucItem::Role> {
    using enum QXmppMucItem::Role;
    static constexpr auto NullValue = UnspecifiedRole;
    static constexpr auto Values = makeValues<QXmppMucItem::Role>({
        { UnspecifiedRole, {} },
        { NoRole, u"none" },
        { VisitorRole, u"visitor" },
        { ParticipantRole, u"participant" },
        { ModeratorRole, u"moderator" },
    });
};

QXmppMucItem::QXmppMucItem()
    : m_affiliation(QXmppMucItem::UnspecifiedAffiliation),
      m_role(QXmppMucItem::UnspecifiedRole)
{
}

/// Returns true if the current item is null.
bool QXmppMucItem::isNull() const
{
    return m_actor.isEmpty() &&
        m_affiliation == UnspecifiedAffiliation &&
        m_jid.isEmpty() &&
        m_nick.isEmpty() &&
        m_reason.isEmpty() &&
        m_role == UnspecifiedRole;
}

/// Returns the actor for this item, for instance the admin who kicked
/// a user out of a room.
QString QXmppMucItem::actor() const
{
    return m_actor;
}

/// Sets the \a actor for this item, for instance the admin who kicked
/// a user out of a room.
void QXmppMucItem::setActor(const QString &actor)
{
    m_actor = actor;
}

/// Returns the user's affiliation, i.e. long-lived permissions.
QXmppMucItem::Affiliation QXmppMucItem::affiliation() const
{
    return m_affiliation;
}

/// Sets the user's affiliation, i.e. long-lived permissions.
void QXmppMucItem::setAffiliation(Affiliation affiliation)
{
    m_affiliation = affiliation;
}

/// Returns the user's real JID.
QString QXmppMucItem::jid() const
{
    return m_jid;
}

/// Sets the user's real JID.
void QXmppMucItem::setJid(const QString &jid)
{
    m_jid = jid;
}

/// Returns the user's nickname.
QString QXmppMucItem::nick() const
{
    return m_nick;
}

/// Sets the user's nickname.
void QXmppMucItem::setNick(const QString &nick)
{
    m_nick = nick;
}

/// Returns the reason for this item, for example the reason for kicking
/// a user out of a room.
QString QXmppMucItem::reason() const
{
    return m_reason;
}

/// Sets the \a reason for this item, for example the reason for kicking
/// a user out of a room.
void QXmppMucItem::setReason(const QString &reason)
{
    m_reason = reason;
}

/// Returns the user's role, i.e. short-lived permissions.
QXmppMucItem::Role QXmppMucItem::role() const
{
    return m_role;
}

/// Sets the user's role, i.e. short-lived permissions.
void QXmppMucItem::setRole(Role role)
{
    m_role = role;
}

/// \cond
void QXmppMucItem::parse(const QDomElement &element)
{
    m_affiliation = Enums::fromString<Affiliation>(element.attribute(u"affiliation"_s).toLower()).value_or(UnspecifiedAffiliation);
    m_jid = element.attribute(u"jid"_s);
    m_nick = element.attribute(u"nick"_s);
    m_role = Enums::fromString<Role>(element.attribute(u"role"_s).toLower()).value_or(UnspecifiedRole);
    m_actor = element.firstChildElement(u"actor"_s).attribute(u"jid"_s);
    m_reason = element.firstChildElement(u"reason"_s).text();
}

void QXmppMucItem::toXml(QXmlStreamWriter *writer) const
{
    if (isNull()) {
        return;
    }

    XmlWriter(writer).write(Element {
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

/// Returns the IQ's items.
QList<QXmppMucItem> QXmppMucAdminIq::items() const
{
    return m_items;
}

/// Sets the IQ's items.
void QXmppMucAdminIq::setItems(const QList<QXmppMucItem> &items)
{
    m_items = items;
}

/// \cond
bool QXmppMucAdminIq::isMucAdminIq(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    return (queryElement.namespaceURI() == ns_muc_admin);
}

void QXmppMucAdminIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    m_items = parseChildElements<QList<QXmppMucItem>>(queryElement);
}

void QXmppMucAdminIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { { u"query", ns_muc_admin }, m_items });
}
/// \endcond

/// Returns the IQ's data form.
QXmppDataForm QXmppMucOwnerIq::form() const
{
    return m_form;
}

/// Sets the IQ's data form.
void QXmppMucOwnerIq::setForm(const QXmppDataForm &form)
{
    m_form = form;
}

/// \cond
bool QXmppMucOwnerIq::isMucOwnerIq(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    return (queryElement.namespaceURI() == ns_muc_owner);
}

void QXmppMucOwnerIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    m_form.parse(queryElement.firstChildElement(u"x"_s));
}

void QXmppMucOwnerIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { { u"query", ns_muc_owner }, m_form });
}
/// \endcond

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucData.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"

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
        Tag { u"item", ns_muc_admin },
        OptionalAttribute { u"jid", m_jid },
        OptionalAttribute { u"nick", m_nick },
        OptionalAttribute { u"affiliation", m_affiliation },
        OptionalAttribute { u"role", m_role },
        OptionalTextElement { u"reason", m_reason },
        OptionalContent { !m_actor.isEmpty(), Element { u"actor", Attribute { u"jid", m_actor } } },
    });
}
/// \endcond

}  // namespace QXmpp::Muc

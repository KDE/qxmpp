// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMixIq.h"

#include "QXmppConstants_p.h"
#include "QXmppMixIq_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QSharedData>
#include <QStringBuilder>

using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppMixIq::Type> {
    using enum QXmppMixIq::Type;

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    static constexpr auto Values = makeValues<QXmppMixIq::Type>({
        { None, {} },
        { ClientJoin, u"client-join" },
        { ClientLeave, u"client-leave" },
        { Join, u"join" },
        { Leave, u"leave" },
        { UpdateSubscription, u"update-subscription" },
        { SetNick, u"setnick" },
        { Create, u"create" },
        { Destroy, u"destroy" },
    });
    QT_WARNING_POP
};

//
// \class QXmppMixSubscriptionUpdateIq
//
// This class represents an IQ used to subscribe to nodes and unsubcribe from nodes of a MIX
// channel as defined by \xep{0369, Mediated Information eXchange (MIX)}.
//
// \since QXmpp 1.7
//
// \ingroup Stanzas
//

///
/// Constructs a MIX subscription update IQ.
///
QXmppMixSubscriptionUpdateIq::QXmppMixSubscriptionUpdateIq()
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMixSubscriptionUpdateIq)

///
/// Returns the nodes to subscribe to.
///
/// \return the nodes being subscribed to
///
QXmppMixConfigItem::Nodes QXmppMixSubscriptionUpdateIq::additions() const
{
    return m_additions;
}

///
/// Sets the nodes to subscribe to.
///
/// \param additions nodes being subscribed to
///
void QXmppMixSubscriptionUpdateIq::setAdditions(QXmppMixConfigItem::Nodes additions)
{
    m_additions = additions;
}

///
/// Returns the nodes to unsubscribe from.
///
/// \return the nodes being unsubscribed from
///
QXmppMixConfigItem::Nodes QXmppMixSubscriptionUpdateIq::removals() const
{
    return m_removals;
}

///
/// Sets the nodes to unsubscribe from.
///
/// \param removals nodes being unsubscribed from
///
void QXmppMixSubscriptionUpdateIq::setRemovals(QXmppMixConfigItem::Nodes removals)
{
    m_removals = removals;
}

bool QXmppMixSubscriptionUpdateIq::isMixSubscriptionUpdateIq(const QDomElement &element)
{
    const QDomElement &child = element.firstChildElement(u"update-subscription"_s);
    return !child.isNull() && (child.namespaceURI() == ns_mix);
}

void QXmppMixSubscriptionUpdateIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement child = element.firstChildElement();
    m_additions = Enums::fromStringList<QXmppMixConfigItem::Node>(parseSingleAttributeElements(child, u"subscribe", ns_mix, u"node"_s));
    m_removals = Enums::fromStringList<QXmppMixConfigItem::Node>(parseSingleAttributeElements(child, u"unsubscribe", ns_mix, u"node"_s));
}

void QXmppMixSubscriptionUpdateIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("update-subscription"));
    writer->writeDefaultNamespace(toString65(ns_mix));
    writeSingleAttributeElements(writer, u"subscribe", u"node", Enums::toStringList(m_additions));
    writeSingleAttributeElements(writer, u"unsubscribe", u"node", Enums::toStringList(m_removals));
    writer->writeEndElement();
}

//
// \class QXmppMixInvitationRequestIq
//
// This class represents an IQ used to request an invitation to a MIX channel as defined by
// \xep{0407, Mediated Information eXchange (MIX): Miscellaneous Capabilities}.
//
// \since QXmpp 1.7
//
// \ingroup Stanzas
//

///
/// Constructs a MIX invitation request IQ.
///
QXmppMixInvitationRequestIq::QXmppMixInvitationRequestIq()
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMixInvitationRequestIq)

///
/// Returns the JID of the invitee for whom an invitation is requested from a channel.
///
/// \return the invitee's JID
///
QString QXmppMixInvitationRequestIq::inviteeJid() const
{
    return m_inviteeJid;
}

///
/// Sets the JID of the invitee for whom an invitation is requested from a channel.
///
/// \param inviteeJid invitee's JID
///
void QXmppMixInvitationRequestIq::setInviteeJid(const QString &inviteeJid)
{
    m_inviteeJid = inviteeJid;
}

bool QXmppMixInvitationRequestIq::isMixInvitationRequestIq(const QDomElement &element)
{
    const QDomElement &child = element.firstChildElement(u"invite"_s);
    return !child.isNull() && (child.namespaceURI() == ns_mix_misc);
}

void QXmppMixInvitationRequestIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement child = element.firstChildElement();
    const auto subChild = child.firstChildElement(u"invitee"_s);
    m_inviteeJid = subChild.text();
}

void QXmppMixInvitationRequestIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("invite"));
    writer->writeDefaultNamespace(toString65(ns_mix_misc));
    writeXmlTextElement(writer, u"invitee", m_inviteeJid);
    writer->writeEndElement();
}

//
// \class QXmppMixInvitationResponseIq
//
// This class represents an IQ that contains a requested invitation to a MIX channel as defined by
// \xep{0407, Mediated Information eXchange (MIX): Miscellaneous Capabilities}.
//
// \since QXmpp 1.7
//
// \ingroup Stanzas
//

///
/// Constructs a MIX invitation response IQ.
///
QXmppMixInvitationResponseIq::QXmppMixInvitationResponseIq()
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMixInvitationResponseIq)

///
/// Returns the invitation to a channel.
///
/// \return the channel invitation
///
QXmppMixInvitation QXmppMixInvitationResponseIq::invitation() const
{
    return m_invitation;
}

///
/// Sets the invitation to a channel.
///
/// \param invitation channel invitation
///
void QXmppMixInvitationResponseIq::setInvitation(const QXmppMixInvitation &invitation)
{
    m_invitation = invitation;
}

bool QXmppMixInvitationResponseIq::isMixInvitationResponseIq(const QDomElement &element)
{
    const QDomElement &child = element.firstChildElement(u"invite"_s);
    return !child.isNull() && (child.namespaceURI() == ns_mix_misc);
}

void QXmppMixInvitationResponseIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement child = element.firstChildElement();
    const auto subChild = child.firstChildElement(u"invitation"_s);
    m_invitation = QXmppMixInvitation();
    m_invitation.parse(subChild);
}

void QXmppMixInvitationResponseIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("invite"));
    writer->writeDefaultNamespace(toString65(ns_mix_misc));
    m_invitation.toXml(writer);
    writer->writeEndElement();
}

class QXmppMixIqPrivate : public QSharedData
{
public:
    QString participantId;
    QString channelId;
    QString channelJid;
    QXmppMixConfigItem::Nodes subscriptions;
    QString nick;
    std::optional<QXmppMixInvitation> invitation;
    QXmppMixIq::Type actionType = QXmppMixIq::None;
};

///
/// \class QXmppMixIq
///
/// This class represents an IQ used to do actions on a MIX channel as defined by
/// \xep{0369, Mediated Information eXchange (MIX)},
/// \xep{0405, Mediated Information eXchange (MIX): Participant Server Requirements} and
/// \xep{0407, Mediated Information eXchange (MIX): Miscellaneous Capabilities}.
///
/// \since QXmpp 1.1
///
/// \ingroup Stanzas
///

///
/// \enum QXmppMixIq::Type
///
/// Action type of the MIX IQ stanza.
///
/// \var QXmppMixIq::None
///
/// Nothing is done.
///
/// \var QXmppMixIq::ClientJoin
///
/// The client sends a request to join a MIX channel to the user's server.
///
/// \var QXmppMixIq::ClientLeave
///
/// The client sends a request to leave a MIX channel to the user's server.
///
/// \var QXmppMixIq::Join
///
/// The user's server forwards a join request from the client to the MIX channel.
///
/// \var QXmppMixIq::Leave
///
/// The user's server forwards a leave request from the client to the MIX channel.
///
/// \var QXmppMixIq::UpdateSubscription
///
/// The client subscribes to MIX nodes or unsubscribes from MIX nodes.
///
/// \deprecated This is deprecated since QXmpp 1.7. Use QXmppMixManager instead.
///
/// \var QXmppMixIq::SetNick
///
/// The client changes the user's nickname within the MIX channel.
///
/// \var QXmppMixIq::Create
///
/// The client creates a MIX channel.
///
/// \var QXmppMixIq::Destroy
///
/// The client destroys a MIX channel.
///

QXmppMixIq::QXmppMixIq()
    : d(new QXmppMixIqPrivate)
{
}

/// Default copy-constructor
QXmppMixIq::QXmppMixIq(const QXmppMixIq &) = default;
/// Default move-constructor
QXmppMixIq::QXmppMixIq(QXmppMixIq &&) = default;
QXmppMixIq::~QXmppMixIq() = default;
/// Default assignment operator
QXmppMixIq &QXmppMixIq::operator=(const QXmppMixIq &) = default;
/// Default move-assignment operator
QXmppMixIq &QXmppMixIq::operator=(QXmppMixIq &&) = default;
///
/// Returns the channel JID, in case of a Join/ClientJoin query result, containing the participant
/// ID.
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::participantId() and
/// QXmppMixIq::channelJid() instead.
///
QString QXmppMixIq::jid() const
{
    if (d->participantId.isEmpty()) {
        return d->channelJid;
    }

    if (d->channelJid.isEmpty()) {
        return {};
    }

    return d->participantId % u'#' % d->channelJid;
}

///
/// Sets the channel JID, in case of a Join/ClientJoin query result, containing the participant ID.
///
/// \param jid channel JID including a possible participant ID
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::setParticipantId() and
/// QXmppMixIq::setChannelJid() instead.
///
void QXmppMixIq::setJid(const QString &jid)
{
    const auto jidParts = jid.split(u'#');

    if (jidParts.size() == 1) {
        d->channelJid = jid;
    } else if (jidParts.size() == 2) {
        d->participantId = jidParts.at(0);
        d->channelJid = jidParts.at(1);
    }
}

///
/// Returns the participant ID for a Join/ClientJoin result.
///
/// \return the participant ID
///
/// \since QXmpp 1.7
///
QString QXmppMixIq::participantId() const
{
    return d->participantId;
}

///
/// Sets the participant ID for a Join/ClientJoin result.
///
/// @param participantId ID of the user in the channel
///
/// \since QXmpp 1.7
///
void QXmppMixIq::setParticipantId(const QString &participantId)
{
    d->participantId = participantId;
}

///
/// Returns the channel's ID (the local part of the channel JID).
///
/// It can be empty if a JID was set.
///
/// \return the ID of the channel
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::channelId() instead.
///
QString QXmppMixIq::channelName() const
{
    return d->channelId;
}

///
/// Sets the channel's ID (the local part of the channel JID) for creating or destroying a channel.
///
/// If you create a new channel, the channel ID can be left empty to let the server generate an ID.
///
/// \param channelName ID of the channel
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::setChannelId()
/// instead.
///
void QXmppMixIq::setChannelName(const QString &channelName)
{
    d->channelId = channelName;
}

///
/// Returns the channel's ID (the local part of the channel JID).
///
/// It can be empty if a JID was set.
///
/// \return the ID of the channel
///
/// \since QXmpp 1.7
///
QString QXmppMixIq::channelId() const
{
    return d->channelId;
}

///
/// Sets the channel's ID (the local part of the channel JID) for creating or destroying a channel.
///
/// If you create a new channel, the channel ID can be left empty to let the server generate an ID.
///
/// @param channelId channel ID to be set
///
/// \since QXmpp 1.7
///
void QXmppMixIq::setChannelId(const QString &channelId)
{
    d->channelId = channelId;
}

///
/// Returns the channel's JID.
///
/// \return the JID of the channel
///
/// \since QXmpp 1.7
///
QString QXmppMixIq::channelJid() const
{
    return d->channelJid;
}

///
/// Sets the channel's JID.
///
/// @param channelJid JID to be set
///
/// \since QXmpp 1.7
///
void QXmppMixIq::setChannelJid(const QString &channelJid)
{
    d->channelJid = channelJid;
}

///
/// Returns the nodes being subscribed to.
///
/// \return the nodes being subscribed to
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::subscriptions() instead.
///
QStringList QXmppMixIq::nodes() const
{
    return transform<QStringList>(Enums::toStringList(d->subscriptions), &QStringView::toString);
}

///
/// Sets the nodes being subscribed to.
///
/// \param nodes nodes being subscribed to
///
/// \deprecated This method is deprecated since QXmpp 1.7. Use QXmppMixIq::setSubscriptions()
/// instead.
///
void QXmppMixIq::setNodes(const QStringList &nodes)
{
    d->subscriptions = Enums::fromStringList<QXmppMixConfigItem::Node>(nodes);
}

///
/// Returns the nodes to subscribe to.
///
/// \return the nodes being subscribed to
///
/// \since QXmpp 1.7
///
QXmppMixConfigItem::Nodes QXmppMixIq::subscriptions() const
{
    return d->subscriptions;
}

///
/// Sets the nodes to subscribe to.
///
/// \param subscriptions nodes being subscribed to
///
/// \since QXmpp 1.7
///
void QXmppMixIq::setSubscriptions(QXmppMixConfigItem::Nodes subscriptions)
{
    d->subscriptions = subscriptions;
}

///
/// Returns the user's nickname in the channel.
///
/// \return the nickname of the user
///
QString QXmppMixIq::nick() const
{
    return d->nick;
}

///
/// Sets the user's nickname used for the channel.
///
/// \param nick nick of the user to be set
///
void QXmppMixIq::setNick(const QString &nick)
{
    d->nick = nick;
}

///
/// Returns the invitation to the channel being joined via Type::ClientJoin or Type::Join.
///
/// \return the channel invitation
///
/// \since QXmpp 1.7
///
std::optional<QXmppMixInvitation> QXmppMixIq::invitation() const
{
    return d->invitation;
}

///
/// Sets the invitation to the channel being joined via Type::ClientJoin or Type::Join.
///
/// \param invitation channel invitation
///
/// \since QXmpp 1.7
///
void QXmppMixIq::setInvitation(const std::optional<QXmppMixInvitation> &invitation)
{
    d->invitation = invitation;
}

/// Returns the MIX channel's action type.
///
/// \return the action type of the channel
///
QXmppMixIq::Type QXmppMixIq::actionType() const
{
    return d->actionType;
}

///
/// Sets the MIX channel's action type.
///
/// \param type action type of the channel
///
void QXmppMixIq::setActionType(QXmppMixIq::Type type)
{
    d->actionType = type;
}

/// \cond
bool QXmppMixIq::isMixIq(const QDomElement &element)
{
    const QDomElement &child = element.firstChildElement();
    return !child.isNull() && (child.namespaceURI() == ns_mix || child.namespaceURI() == ns_mix_pam);
}

void QXmppMixIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement child = element.firstChildElement();
    d->actionType = Enums::fromString<Type>(child.tagName()).value_or(None);

    if (child.namespaceURI() == ns_mix_pam) {
        if (child.hasAttribute(u"channel"_s)) {
            d->channelJid = child.attribute(u"channel"_s);
        }

        child = child.firstChildElement();
    }

    if (!child.isNull() && child.namespaceURI() == ns_mix) {
        if (child.hasAttribute(u"id"_s)) {
            d->participantId = child.attribute(u"id"_s);
        }
        if (child.hasAttribute(u"jid"_s)) {
            d->channelJid = (child.attribute(u"jid"_s)).split(u'#').last();
        }
        if (child.hasAttribute(u"channel"_s)) {
            d->channelId = child.attribute(u"channel"_s);
        }

        d->nick = firstChildElement(child, u"nick").text();
        d->invitation = parseOptionalChildElement<QXmppMixInvitation>(child);
        d->subscriptions = Enums::fromStringList<QXmppMixConfigItem::Node>(parseSingleAttributeElements(child, u"subscribe", ns_mix, u"node"_s));
    }
}

void QXmppMixIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    if (d->actionType == None) {
        return;
    }

    writer->writeStartElement(toString65(Enums::toString(d->actionType)));

    if (d->actionType == ClientJoin || d->actionType == ClientLeave) {
        writer->writeDefaultNamespace(toString65(ns_mix_pam));
        if (type() == Set) {
            writeOptionalXmlAttribute(writer, u"channel", d->channelJid);
        }
        if (d->actionType == ClientJoin) {
            writer->writeStartElement(QSL65("join"));
        } else if (d->actionType == ClientLeave) {
            writer->writeStartElement(QSL65("leave"));
        }
    }

    writer->writeDefaultNamespace(toString65(ns_mix));
    writeOptionalXmlAttribute(writer, u"channel", d->channelId);
    if (type() == Result) {
        writeOptionalXmlAttribute(writer, u"id", d->participantId);
    }

    writeSingleAttributeElements(writer, u"subscribe", u"node", Enums::toStringList(d->subscriptions));
    writeOptionalXmlTextElement(writer, u"nick", d->nick);
    writeOptional(writer, d->invitation);

    writer->writeEndElement();

    if (d->actionType == ClientJoin || d->actionType == ClientLeave) {
        writer->writeEndElement();
    }
}
/// \endcond

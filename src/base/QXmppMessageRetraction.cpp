// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMessageRetraction.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

/*!
    \class QXmppMessageModeration
    \inmodule QXmpp

    \brief The QXmppMessageModeration class represents the moderation information of a moderated
    message retraction or tombstone as defined by \xep{0425}{Moderated Message Retraction}.

    This is the \c {<moderated/>} element (and its accompanying \c {<reason/>}) that a
    \xep{0045}{Multi-User Chat} service adds when a moderator retracts another occupant's message.

    \ingroup Stanzas

    \since QXmpp 1.17
*/

/*!
    Returns the full JID (room@service/nick) of the moderator that performed the retraction.
*/
QString QXmppMessageModeration::moderatorJid() const
{
    return m_moderatorJid;
}

/*!
    Sets the full JID (room@service/nick) of the moderator that performed the retraction to \a jid.
*/
void QXmppMessageModeration::setModeratorJid(const QString &jid)
{
    m_moderatorJid = jid;
}

/*!
    Returns the \xep{0421}{Occupant identifier} of the moderator (semi-anonymous rooms).
*/
QString QXmppMessageModeration::moderatorOccupantId() const
{
    return m_moderatorOccupantId;
}

/*!
    Sets the \xep{0421}{Occupant identifier} of the moderator to \a id.
*/
void QXmppMessageModeration::setModeratorOccupantId(const QString &id)
{
    m_moderatorOccupantId = id;
}

/*!
    Returns the optional human-readable reason for the moderation.
*/
QString QXmppMessageModeration::reason() const
{
    return m_reason;
}

/*!
    Sets the optional human-readable \a reason for the moderation.
*/
void QXmppMessageModeration::setReason(const QString &reason)
{
    m_reason = reason;
}

void QXmppMessageModeration::parse(const QDomElement &parentElement)
{
    auto moderatedEl = firstChildElement<QXmppMessageModeration>(parentElement);
    m_moderatorJid = moderatedEl.attribute(u"by"_s);

    if (auto occupantEl = firstChildElement(moderatedEl, u"occupant-id", ns_muc_occupant_id); !occupantEl.isNull()) {
        m_moderatorOccupantId = occupantEl.attribute(u"id"_s);
    }

    // <reason/> is a sibling of <moderated/>, inheriting the parent's namespace.
    m_reason = firstChildElement(parentElement, u"reason").text();
}

void QXmppMessageModeration::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"moderated", ns_message_moderate },
        OptionalAttribute { u"by", m_moderatorJid },
        OptionalContent {
            !m_moderatorOccupantId.isEmpty(),
            Element {
                { u"occupant-id", ns_muc_occupant_id },
                Attribute { u"id", m_moderatorOccupantId },
            },
        },
    });
    // <reason/> sits next to <moderated/>, inheriting the parent's namespace.
    w.write(OptionalTextElement { u"reason", m_reason });
}

/*!
    \class QXmppMessageRetraction
    \inmodule QXmpp

    \brief The QXmppMessageRetraction class represents a message retraction as defined by
    \xep{0424}{Message Retraction}.

    Indicates that a previously sent message should be removed. The retracted message is referenced
    by its id: the \xep{0359}{Unique and Stable Stanza IDs} origin id in 1:1 chats, or the stanza id
    assigned by the room in group chats.

    If the retraction was performed by a \xep{0045}{Multi-User Chat} moderator
    (\xep{0425}{Moderated Message Retraction}), moderation() carries information about the moderator
    and the reason.

    When sending a retraction, you are responsible for adding a \xep{0428}{Fallback Indication}
    marker, a human-readable fallback body and the \xep{0334}{Message Processing Hints} \c store
    hint (see QXmppMessage::setFallbackMarkers() and QXmppMessage::Store).

    \ingroup Stanzas

    \since QXmpp 1.17
*/

/*!
    Constructs a message retraction referencing the message with the given \a retractedId.
*/
QXmppMessageRetraction::QXmppMessageRetraction(QString retractedId)
    : m_retractedId(std::move(retractedId))
{
}

/*!
    Returns the id of the message being retracted (origin id in 1:1 chats, stanza id in MUCs).
*/
QString QXmppMessageRetraction::retractedId() const
{
    return m_retractedId;
}

/*!
    Sets the \a id of the message being retracted (origin id in 1:1 chats, stanza id in MUCs).
*/
void QXmppMessageRetraction::setRetractedId(const QString &id)
{
    m_retractedId = id;
}

/*!
    Returns the moderation information, set only for \xep{0425}{Moderated Message Retraction}
    retractions.
*/
const std::optional<QXmppMessageModeration> &QXmppMessageRetraction::moderation() const
{
    return m_moderation;
}

/*!
    Sets the \a moderation information for a \xep{0425}{Moderated Message Retraction} retraction.
*/
void QXmppMessageRetraction::setModeration(const std::optional<QXmppMessageModeration> &moderation)
{
    m_moderation = moderation;
}

void QXmppMessageRetraction::parse(const QDomElement &element)
{
    m_retractedId = element.attribute(u"id"_s);

    if (!firstChildElement<QXmppMessageModeration>(element).isNull()) {
        QXmppMessageModeration moderation;
        moderation.parse(element);
        m_moderation = std::move(moderation);
    } else {
        m_moderation.reset();
    }
}

void QXmppMessageRetraction::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"retract", ns_message_retract },
        Attribute { u"id", m_retractedId },
        [&] {
            if (m_moderation) {
                m_moderation->toXml(writer);
            }
        },
    });
}

/*!
    \class QXmppMessageRetracted
    \inmodule QXmpp

    \brief The QXmppMessageRetracted class represents a retracted-message tombstone as defined by
    \xep{0424}{Message Retraction}.

    Archiving services (\xep{0313}{Message Archive Management}) replace the body of a retracted
    message with a \c {<retracted/>} tombstone. It records when the message was retracted and, for
    \xep{0425}{Moderated Message Retraction}, by whom.

    \ingroup Stanzas

    \since QXmpp 1.17
*/

/*!
    Returns the timestamp at which the message was retracted.
*/
QDateTime QXmppMessageRetracted::stamp() const
{
    return m_stamp;
}

/*!
    Sets the timestamp \a stamp at which the message was retracted.
*/
void QXmppMessageRetracted::setStamp(const QDateTime &stamp)
{
    m_stamp = stamp;
}

/*!
    Returns the optional JID of the entity that retracted the message.
*/
QString QXmppMessageRetracted::by() const
{
    return m_by;
}

/*!
    Sets the optional JID \a by of the entity that retracted the message.
*/
void QXmppMessageRetracted::setBy(const QString &by)
{
    m_by = by;
}

/*!
    Returns the moderation information, set only for \xep{0425}{Moderated Message Retraction}
    tombstones.
*/
const std::optional<QXmppMessageModeration> &QXmppMessageRetracted::moderation() const
{
    return m_moderation;
}

/*!
    Sets the \a moderation information for a \xep{0425}{Moderated Message Retraction} tombstone.
*/
void QXmppMessageRetracted::setModeration(const std::optional<QXmppMessageModeration> &moderation)
{
    m_moderation = moderation;
}

void QXmppMessageRetracted::parse(const QDomElement &element)
{
    m_stamp = QXmppUtils::datetimeFromString(element.attribute(u"stamp"_s));
    m_by = element.attribute(u"by"_s);

    if (!firstChildElement<QXmppMessageModeration>(element).isNull()) {
        QXmppMessageModeration moderation;
        moderation.parse(element);
        m_moderation = std::move(moderation);
    } else {
        m_moderation.reset();
    }
}

void QXmppMessageRetracted::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"retracted", ns_message_retract },
        OptionalAttribute { u"stamp", m_stamp },
        OptionalAttribute { u"by", m_by },
        [&] {
            if (m_moderation) {
                m_moderation->toXml(writer);
            }
        },
    });
}

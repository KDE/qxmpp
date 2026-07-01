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
    \class QXmppMessageRetraction
    \inmodule QXmpp

    \brief The QXmppMessageRetraction class represents a message retraction as defined by
    \xep{0424}{Message Retraction}.

    Indicates that a previously sent message should be removed. The retracted message is referenced
    by its id: the \xep{0359}{Unique and Stable Stanza IDs} origin id in 1:1 chats, or the stanza id
    assigned by the room in group chats.

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

void QXmppMessageRetraction::parse(const QDomElement &element)
{
    m_retractedId = element.attribute(u"id"_s);
}

void QXmppMessageRetraction::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"retract", ns_message_retract },
        Attribute { u"id", m_retractedId },
    });
}

/*!
    \class QXmppMessageRetracted
    \inmodule QXmpp

    \brief The QXmppMessageRetracted class represents a retracted-message tombstone as defined by
    \xep{0424}{Message Retraction}.

    Archiving services (\xep{0313}{Message Archive Management}) replace the body of a retracted
    message with a \c {<retracted/>} tombstone. It records when the message was retracted.

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

void QXmppMessageRetracted::parse(const QDomElement &element)
{
    m_stamp = QXmppUtils::datetimeFromString(element.attribute(u"stamp"_s));
    m_by = element.attribute(u"by"_s);
}

void QXmppMessageRetracted::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"retracted", ns_message_retract },
        OptionalAttribute { u"stamp", m_stamp },
        OptionalAttribute { u"by", m_by },
    });
}

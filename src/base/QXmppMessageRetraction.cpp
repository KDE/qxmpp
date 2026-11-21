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

    This is the \c {<moderated/>} element that a \xep{0045}{Multi-User Chat} service adds when a
    moderator retracts another occupant's message. The reason of the moderation is available via
    QXmppMessageRetraction::reason() and QXmppMessageRetracted::reason().

    \ingroup qxmpp-stanzas

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

    \deprecated since QXmpp 1.18, the reason is part of the retraction, use
    QXmppMessageRetraction::reason() or QXmppMessageRetracted::reason().
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
QString QXmppMessageModeration::reason() const
{
    return m_reason;
}
#endif

/*!
    Sets the optional human-readable \a reason for the moderation.

    \deprecated since QXmpp 1.18, the reason is part of the retraction, use
    QXmppMessageRetraction::setReason() or QXmppMessageRetracted::setReason().
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
void QXmppMessageModeration::setReason(const QString &reason)
{
    m_reason = reason;
}
#endif

void QXmppMessageModeration::parse(const QDomElement &parentElement)
{
    auto moderatedEl = firstChildElement<QXmppMessageModeration>(parentElement);
    m_moderatorJid = moderatedEl.attribute(u"by"_s);

    if (auto occupantEl = firstChildElement(moderatedEl, u"occupant-id", ns_muc_occupant_id); !occupantEl.isNull()) {
        m_moderatorOccupantId = occupantEl.attribute(u"id"_s);
    }

    // <reason/> is a sibling of <moderated/>
    m_reason = firstChildElement(parentElement, u"reason", ns_message_retract).text();
}

void QXmppMessageModeration::toXml(QXmlStreamWriter *writer) const
{
    writeModerated(writer);
    // <reason/> is a sibling of <moderated/>, inheriting the parent's namespace
    XmlWriter(writer).write(OptionalTextElement { u"reason", m_reason });
}

void QXmppMessageModeration::writeModerated(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
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
}

/*!
    \class QXmppMessageRetraction
    \inmodule QXmpp

    \brief The QXmppMessageRetraction class represents a message retraction as defined by
    \xep{0424}{Message Retraction}.

    Indicates that a previously sent message should be removed, together with all of its
    \xep{0308}{Last Message Correction} corrections. The retracted message is referenced by its id
    (see retractedId()).

    To make messages retractable, applications need to set a globally unique 'id' on them, e.g. a
    UUID (see QXmppUtils::generateStanzaUuid()).

    If the retraction was performed by a \xep{0045}{Multi-User Chat} moderator
    (\xep{0425}{Moderated Message Retraction}), moderation() carries information about the moderator.

    When sending a retraction, you should add the \xep{0334}{Message Processing Hints} \c store hint
    (QXmppMessage::Store). A human-readable fallback body is optional; if you add one, mark it with a
    \xep{0428}{Fallback Indication} marker (see QXmppMessage::setFallbackMarkers()).

    Clients must not display the body of a received retraction, even if no message with the
    retracted id is known. Received retractions containing a \xep{0308}{Last Message Correction}
    \c {<replace/>} element must only be processed as retractions.

    \ingroup qxmpp-stanzas

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
    Returns the id of the message being retracted.

    This is the 'id' attribute of the original message in 1:1 chats, or the
    \xep{0359}{Unique and Stable Stanza IDs} stanza id assigned by the room in group chats. It
    always refers to the original message, not to one of its corrections.

    The id is empty if the retraction is embedded into another element that identifies the message
    differently, e.g. a \xep{0425}{Moderated Message Retraction} request.
*/
QString QXmppMessageRetraction::retractedId() const
{
    return m_retractedId;
}

/*!
    Sets the \a id of the message being retracted.

    \sa retractedId()
*/
void QXmppMessageRetraction::setRetractedId(const QString &id)
{
    m_retractedId = id;
}

/*!
    Returns the optional human-readable reason for the retraction.

    The reason must only be displayed together with the retracted message, after the retraction has
    been processed, i.e. it refers to a known message and has been sent by its author or a
    moderator.

    \since QXmpp 1.18
*/
QString QXmppMessageRetraction::reason() const
{
    return m_reason;
}

/*!
    Sets the optional human-readable \a reason for the retraction.

    The reason should not repeat the content of the retracted message.

    \since QXmpp 1.18
*/
void QXmppMessageRetraction::setReason(const QString &reason)
{
    m_reason = reason;
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
    m_reason = firstChildElement(element, u"reason", ns_message_retract).text();

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
        OptionalAttribute { u"id", m_retractedId },
        [&] {
            if (m_moderation) {
                m_moderation->writeModerated(writer);
            }
        },
        // fall back to the reason set via the deprecated QXmppMessageModeration::setReason()
        OptionalTextElement { u"reason", m_reason.isEmpty() && m_moderation ? m_moderation->m_reason : m_reason },
    });
}

/*!
    \class QXmppMessageRetracted
    \inmodule QXmpp

    \brief The QXmppMessageRetracted class represents a retracted-message tombstone as defined by
    \xep{0424}{Message Retraction}.

    Archiving services (\xep{0313}{Message Archive Management}) replace the body of a retracted
    message with a \c {<retracted/>} tombstone. It references the retraction message and records
    when the message was retracted, optionally why and, for \xep{0425}{Moderated Message Retraction},
    by whom.

    If the retracted message has been corrected (\xep{0308}{Last Message Correction}), the original
    message and each correction are replaced by a tombstone. The tombstones of the corrections keep
    their \c {<replace/>} element (QXmppMessage::replaceId()), so that they can be shown as a single
    retracted message.

    \ingroup qxmpp-stanzas

    \since QXmpp 1.17
*/

/*!
    Returns the id of the retraction message, which is stored in the archive as well.

    \since QXmpp 1.18
*/
QString QXmppMessageRetracted::retractionId() const
{
    return m_retractionId;
}

/*!
    Sets the \a id of the retraction message.

    \since QXmpp 1.18
*/
void QXmppMessageRetracted::setRetractionId(const QString &id)
{
    m_retractionId = id;
}

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
    Returns the optional human-readable reason of the retraction, copied from the retraction message.

    \since QXmpp 1.18
*/
QString QXmppMessageRetracted::reason() const
{
    return m_reason;
}

/*!
    Sets the optional human-readable \a reason of the retraction.

    \since QXmpp 1.18
*/
void QXmppMessageRetracted::setReason(const QString &reason)
{
    m_reason = reason;
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
    m_retractionId = element.attribute(u"id"_s);
    m_stamp = QXmppUtils::datetimeFromString(element.attribute(u"stamp"_s));
    m_by = element.attribute(u"by"_s);
    m_reason = firstChildElement(element, u"reason", ns_message_retract).text();

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
        OptionalAttribute { u"id", m_retractionId },
        OptionalAttribute { u"stamp", m_stamp },
        OptionalAttribute { u"by", m_by },
        [&] {
            if (m_moderation) {
                m_moderation->writeModerated(writer);
            }
        },
        // fall back to the reason set via the deprecated QXmppMessageModeration::setReason()
        OptionalTextElement { u"reason", m_reason.isEmpty() && m_moderation ? m_moderation->m_reason : m_reason },
    });
}

struct QXmppMessageRetractionLimitsPrivate : QSharedData {
    std::optional<std::chrono::seconds> maxAge;
};

/*!
    \class QXmppMessageRetractionLimits
    \inmodule QXmpp

    Data form used in service discovery information for advertising the limits an entity enforces on
    message retractions. See \xep{0424}{Message Retraction} for details.

    ```
    QXmppDiscoInfo info;
    if (auto limits = info.dataForm<QXmppMessageRetractionLimits>()) {
        auto maxAge = limits->maxAge();
    }
    ```

    \since QXmpp 1.17
*/

/*! Tries to parse \a form into QXmppMessageRetractionLimits. */
std::optional<QXmppMessageRetractionLimits> QXmppMessageRetractionLimits::fromDataForm(const QXmppDataForm &form)
{
    if (auto parsed = QXmppMessageRetractionLimits();
        QXmppDataFormBase::fromDataForm(form, parsed)) {
        return parsed;
    }
    return std::nullopt;
}

QXmppMessageRetractionLimits::QXmppMessageRetractionLimits()
    : d(new QXmppMessageRetractionLimitsPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMessageRetractionLimits)

/*!
    Returns the maximum age of a message for which a retraction request is honoured.

    An entity that does not advertise a limit honours retraction requests regardless of the age of
    the retracted message.
*/
std::optional<std::chrono::seconds> QXmppMessageRetractionLimits::maxAge() const
{
    return d->maxAge;
}

/*!
    Sets the maximum age of a message for which a retraction request is honoured.

    \a maxAge
*/
void QXmppMessageRetractionLimits::setMaxAge(std::optional<std::chrono::seconds> maxAge)
{
    d->maxAge = maxAge;
}

/// \cond
QString QXmppMessageRetractionLimits::formType() const
{
    return ns_message_retract.toString();
}

bool QXmppMessageRetractionLimits::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    if (field.type() == QXmppDataForm::Field::HiddenField) {
        return false;
    }

    if (field.key() == u"max-age") {
        if (const auto seconds = parseUInt(field.value())) {
            d->maxAge = std::chrono::seconds(*seconds);
        }
        return true;
    }
    return false;
}

void QXmppMessageRetractionLimits::serializeForm(QXmppDataForm &form) const
{
    std::optional<quint32> maxAge;

    if (d->maxAge) {
        maxAge = quint32(d->maxAge->count());
    }

    serializeOptionalNumber(form, QXmppDataForm::Field::TextSingleField, u"max-age", maxAge);
}
/// \endcond

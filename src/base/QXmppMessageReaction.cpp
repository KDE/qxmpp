// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMessageReaction.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

class QXmppMessageReactionPrivate : public QSharedData
{
public:
    QString messageId;
    QVector<QString> emojis;
};

/*!
    \class QXmppMessageReaction
    \inmodule QXmpp

    \brief The QXmppMessageReaction class represents a reaction to a message in the form of emojis
    as specified by \xep{0444}{Message Reactions}.

    \since QXmpp 1.5
*/

/*! Constructs a message reaction. */
QXmppMessageReaction::QXmppMessageReaction()
    : d(new QXmppMessageReactionPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMessageReaction)

/*!
    Returns the ID of the message for that the reaction is sent.

    For a group chat message, \c {QXmppMessage::stanzaId()} is used.

    For other message types, \c {QXmppMessage::originId()} is used.
    If that is not available, \c {QXmppMessage::id()} is used.
*/
QString QXmppMessageReaction::messageId() const
{
    return d->messageId;
}

/*!
    Sets the ID of the message for that the reaction is sent to \a messageId.

    For a group chat message, \c {QXmppMessage::stanzaId()} must be used.
    If there is no such ID, a message reaction must not be sent.

    For other message types, \c {QXmppMessage::originId()} should be used.
    If that is not available, \c {QXmppMessage::id()} should be used.
*/
void QXmppMessageReaction::setMessageId(const QString &messageId)
{
    d->messageId = messageId;
}

/*!
    Returns the emojis in reaction to a message.
*/
QVector<QString> QXmppMessageReaction::emojis() const
{
    return d->emojis;
}

/*!
    Sets the \a emojis in reaction to a message.

    Each reaction should only consist of unicode codepoints that can be displayed as a single emoji.
    Duplicates are not allowed.
*/
void QXmppMessageReaction::setEmojis(const QVector<QString> &emojis)
{
    d->emojis = emojis;
}

void QXmppMessageReaction::parse(const QDomElement &element)
{
    d->messageId = element.attribute(u"id"_s);
    d->emojis = parseTextElements<QVector<QString>>(element, u"reaction", ns_reactions);

    // Remove duplicate emojis.
    std::sort(d->emojis.begin(), d->emojis.end());
    d->emojis.erase(std::unique(d->emojis.begin(), d->emojis.end()), d->emojis.end());
}

void QXmppMessageReaction::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        { u"reactions", ns_reactions },
        Attribute { u"id", d->messageId },
        TextElements { u"reaction", d->emojis },
    });
}

/*!
    Determines whether the given DOM \a element is a message reaction element.

    Returns true if \a element is a message reaction element, otherwise false.
*/
bool QXmppMessageReaction::isMessageReaction(const QDomElement &element)
{
    return element.tagName() == u"reactions" &&
        element.namespaceURI() == ns_reactions;
}

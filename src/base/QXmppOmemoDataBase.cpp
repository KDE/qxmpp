// SPDX-FileCopyrightText: 2021 Germán Márquez Mejía <mancho@olomono.de>
// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppGlobal_p.h"
#include "QXmppOmemoElement_p.h"
#include "QXmppOmemoEnvelope_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

/*!
    \class QXmppOmemoEnvelope
    \inmodule QXmpp

    \brief The QXmppOmemoEnvelope class represents an OMEMO envelope as
    defined by \xep{0384}{OMEMO Encryption}.
*/

/*!
    Returns the ID of the recipient's device.

    The ID is 0 if it is unset.
*/
uint32_t QXmppOmemoEnvelope::recipientDeviceId() const
{
    return m_recipientDeviceId;
}

/*!
    Sets the \a id of the recipient's device.

    The ID must be at least 1 and at most \c std::numeric_limits<int32_t>::max().
*/
void QXmppOmemoEnvelope::setRecipientDeviceId(uint32_t id)
{
    m_recipientDeviceId = id;
}

/*!
    Returns true if a pre-key was used to prepare this envelope, otherwise false.

    The default is false.
*/
bool QXmppOmemoEnvelope::isUsedForKeyExchange() const
{
    return m_isUsedForKeyExchange;
}

/*!
    Sets via \a isUsed whether a pre-key was used to prepare this envelope.
*/
void QXmppOmemoEnvelope::setUsedForKeyExchange(bool isUsed)
{
    m_isUsedForKeyExchange = isUsed;
}

/*!
    Returns the BLOB containing the binary data for the underlying double
    ratchet library.

    It should be treated like an obscure BLOB being passed as is to the ratchet
    library for further processing.
*/
QByteArray QXmppOmemoEnvelope::data() const
{
    return m_data;
}

/*!
    Sets the BLOB containing the binary \a data from the underlying double
    ratchet library.

    It should be treated like an obscure BLOB produced by the ratchet library.
*/
void QXmppOmemoEnvelope::setData(const QByteArray &data)
{
    m_data = data;
}

void QXmppOmemoEnvelope::parse(const QDomElement &element)
{
    m_recipientDeviceId = element.attribute(u"rid"_s).toUInt();

    const auto isUsedForKeyExchange = element.attribute(u"kex"_s);
    if (isUsedForKeyExchange == u"true" || isUsedForKeyExchange == u"1") {
        m_isUsedForKeyExchange = true;
    }

    m_data = QByteArray::fromBase64(element.text().toLatin1());
}

void QXmppOmemoEnvelope::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"key",
        Attribute { u"rid", m_recipientDeviceId },
        OptionalAttribute { u"kex", DefaultedBool { m_isUsedForKeyExchange, false } },
        Characters { Base64 { m_data } },
    });
}

/*!
    Determines whether the given DOM \a element is an OMEMO envelope.

    Returns true if \a element is an OMEMO envelope, otherwise false.
*/
bool QXmppOmemoEnvelope::isOmemoEnvelope(const QDomElement &element)
{
    return element.tagName() == u"key" && element.namespaceURI() == ns_omemo_2;
}

/*!
    \class QXmppOmemoElement
    \inmodule QXmpp

    \brief The QXmppOmemoElement class represents an OMEMO element as
    defined by \xep{0384}{OMEMO Encryption}.
*/

/*!
    Returns the ID of the sender's device.

    The ID is 0 if it is unset.
*/
uint32_t QXmppOmemoElement::senderDeviceId() const
{
    return m_senderDeviceId;
}

/*!
    Sets the \a id of the sender's device.

    The ID must be at least 1 and at most
    \c std::numeric_limits<int32_t>::max().
*/
void QXmppOmemoElement::setSenderDeviceId(uint32_t id)
{
    m_senderDeviceId = id;
}

/*!
    Returns the encrypted payload which consists of the encrypted SCE envelope.
*/
QByteArray QXmppOmemoElement::payload() const
{
    return m_payload;
}

/*!
    Sets the encrypted \a payload which consists of the encrypted SCE envelope.
*/
void QXmppOmemoElement::setPayload(const QByteArray &payload)
{
    m_payload = payload;
}

/*!
    Searches for an OMEMO envelope by its \a recipientJid (bare JID of the
    recipient) and \a recipientDeviceId (ID of the recipient's device).

    Returns the found OMEMO envelope.
*/
std::optional<QXmppOmemoEnvelope> QXmppOmemoElement::searchEnvelope(const QString &recipientJid, uint32_t recipientDeviceId) const
{
    for (auto itr = m_envelopes.constFind(recipientJid);
         itr != m_envelopes.constEnd() && itr.key() == recipientJid;
         ++itr) {
        const auto &envelope = itr.value();
        if (envelope.recipientDeviceId() == recipientDeviceId) {
            return envelope;
        }
    }

    return std::nullopt;
}

/*!
    Adds an OMEMO \a envelope for the recipient with bare JID \a recipientJid.

    If a full JID is passed as \a recipientJid, it is converted into a bare JID.

    \sa QXmppOmemoEnvelope
*/
void QXmppOmemoElement::addEnvelope(const QString &recipientJid, const QXmppOmemoEnvelope &envelope)
{
    m_envelopes.insert(QXmppUtils::jidToBareJid(recipientJid), envelope);
}

void QXmppOmemoElement::parse(const QDomElement &element)
{
    const auto header = element.firstChildElement(u"header"_s);

    m_senderDeviceId = header.attribute(u"sid"_s).toUInt();

    for (const auto &recipient : iterChildElements(header, u"keys")) {
        const auto recipientJid = recipient.attribute(u"jid"_s);

        for (const auto &envelope : iterChildElements(recipient, u"key")) {
            QXmppOmemoEnvelope omemoEnvelope;
            omemoEnvelope.parse(envelope);
            addEnvelope(recipientJid, omemoEnvelope);
        }
    }

    m_payload = QByteArray::fromBase64(element.firstChildElement(u"payload"_s).text().toLatin1());
}

void QXmppOmemoElement::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"encrypted", ns_omemo_2 },
        Element {
            u"header",
            Attribute { u"sid", m_senderDeviceId },
            [&] {
                const auto recipientJids = m_envelopes.uniqueKeys();
                for (const auto &recipientJid : recipientJids) {
                    w.write(Element {
                        u"keys",
                        Attribute { u"jid", recipientJid },
                        [&] {
                            for (auto itr = m_envelopes.constFind(recipientJid);
                                 itr != m_envelopes.constEnd() && itr.key() == recipientJid;
                                 ++itr) {
                                w.write(itr.value());
                            }
                        },
                    });
                }
            },
        },
        OptionalTextElement { u"payload", Base64 { m_payload } },
    });
}

/*!
    Determines whether the given DOM \a element is an OMEMO element.

    Returns true if \a element is an OMEMO element, otherwise false.
*/
bool QXmppOmemoElement::isOmemoElement(const QDomElement &element)
{
    return element.tagName() == u"encrypted" && element.namespaceURI() == ns_omemo_2;
}

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPE2EEPROVIDER_H
#define QXMPPE2EEPROVIDER_H

#include "QXmppE2eeMetadata.h"
#include "QXmppError.h"
#include "QXmppHooks.h"
#include "QXmppMessage.h"
#include "QXmppSendStanzaParams.h"
#include "QXmppTask.h"

#include <functional>
#include <variant>

#include <QDomElement>

class QXmlStreamWriter;

namespace QXmpp::E2ee {

// Classes with getters and setters, so fields can be added in a source compatible way.

class DecryptRequest
{
public:
    const QXmppMessage &message() const { return m_message; }
    void setMessage(QXmppMessage message) { m_message = std::move(message); }

    // real sender resolved by the pipeline (not the room JID in MUCs)
    const QString &senderJid() const { return m_senderJid; }
    void setSenderJid(QString jid) { m_senderJid = std::move(jid); }

private:
    QXmppMessage m_message;
    QString m_senderJid;
};

class Decrypted
{
public:
    // the SCE <content/> element, merged into the message by the pipeline; a null element for
    // messages only used by the encryption (e.g. OMEMO key exchange), which are not delivered
    const QDomElement &content() const { return m_content; }
    void setContent(QDomElement content) { m_content = std::move(content); }

    const QXmppE2eeMetadata &metadata() const { return m_metadata; }
    void setMetadata(QXmppE2eeMetadata metadata) { m_metadata = std::move(metadata); }

    // SCE affixes, checked by the pipeline against the sender and the recipient;
    // empty if the method has no SCE (OMEMO 0)
    const QString &sceFrom() const { return m_sceFrom; }
    void setSceFrom(QString jid) { m_sceFrom = std::move(jid); }
    const QString &sceTo() const { return m_sceTo; }
    void setSceTo(QString jid) { m_sceTo = std::move(jid); }

private:
    QDomElement m_content;
    QXmppE2eeMetadata m_metadata;
    QString m_sceFrom;
    QString m_sceTo;
};

// The message is encrypted, but not for this device.
struct NoEnvelope { };

using DecryptResult = std::variant<Decrypted, NoEnvelope, QXmppError>;

class EncryptRequest
{
public:
    // The provider encrypts the sensitive elements (SceSensitive). The own devices are not in
    // recipientJids(), the provider encrypts for them too.
    const QXmppMessage &message() const { return m_message; }
    void setMessage(QXmppMessage message) { m_message = std::move(message); }

    // bare JIDs, for MUCs the members supplied by the ResolveRecipients hook
    const QList<QString> &recipientJids() const { return m_recipientJids; }
    void setRecipientJids(QList<QString> jids) { m_recipientJids = std::move(jids); }

    const QXmppSendStanzaParams &params() const { return m_params; }
    void setParams(QXmppSendStanzaParams params) { m_params = std::move(params); }

private:
    QXmppMessage m_message;
    QList<QString> m_recipientJids;
    QXmppSendStanzaParams m_params;
};

class Encrypted
{
public:
    // The encrypted element(s) added to the public message, e.g. QXmppOmemoElement. They must be
    // registered as encrypted container (Xml::Registry::registerEncryptedContainer()), so they are
    // serialized in the public part. The pipeline adds the EME element and the fallback body.
    const QXmpp::Xml::Extensions &elements() const { return m_elements; }
    void setElements(QXmpp::Xml::Extensions elements) { m_elements = std::move(elements); }

private:
    QXmpp::Xml::Extensions m_elements;
};

using EncryptResult = std::variant<Encrypted, QXmppError>;

class EncryptIqRequest
{
public:
    // JID of the recipient of the IQ, the own bare JID for IQs to the own account
    const QString &to() const { return m_to; }
    void setTo(QString to) { m_to = std::move(to); }

    // Writes the payload of the IQ: at most one element, exactly one in requests. The payload is
    // only serialized when it is written.
    void writePayload(QXmlStreamWriter *writer) const
    {
        if (m_payload) {
            m_payload(writer);
        }
    }
    void setPayload(std::function<void(QXmlStreamWriter *)> payload) { m_payload = std::move(payload); }

    // The error of an error response.
    const std::optional<QXmppStanza::Error> &error() const { return m_error; }
    void setError(std::optional<QXmppStanza::Error> error) { m_error = std::move(error); }

    // Writes the content to be encrypted: the payload and the error. The error declares the
    // namespace 'jabber:client', so the content can be written outside of the IQ, e.g. into an
    // SCE envelope.
    QXMPP_EXPORT void writeContent(QXmlStreamWriter *writer) const;

    // bare JIDs, the addressee or QXmppSendStanzaParams::encryptionJids()
    const QList<QString> &recipientJids() const { return m_recipientJids; }
    void setRecipientJids(QList<QString> jids) { m_recipientJids = std::move(jids); }

    const QXmppSendStanzaParams &params() const { return m_params; }
    void setParams(QXmppSendStanzaParams params) { m_params = std::move(params); }

private:
    QString m_to;
    std::function<void(QXmlStreamWriter *)> m_payload;
    std::optional<QXmppStanza::Error> m_error;
    QList<QString> m_recipientJids;
    QXmppSendStanzaParams m_params;
};

class DecryptIqRequest
{
public:
    // the encrypted IQ
    const QDomElement &iq() const { return m_iq; }
    void setIq(QDomElement iq) { m_iq = std::move(iq); }

    // bare JID of the sender
    const QString &senderJid() const { return m_senderJid; }
    void setSenderJid(const QString &jid) { m_senderJid = jid; }

private:
    QDomElement m_iq;
    QString m_senderJid;
};

// What an encryption encrypts of a message.
enum class ContentEncryption {
    // Stanza Content Encryption (XEP-0420): all sensitive elements, e.g. OMEMO 2
    Sce,
    // only the body, all other elements are sent in the clear, e.g. OMEMO 0.3
    BodyOnly,
};

// Registered per encryption. Function members instead of virtuals, so it can be extended without
// breaking the ABI.
struct Provider {
    // Namespace of the encryption, e.g. QXmpp::E2ee::Omemo2. It must reference static storage,
    // like the namespace of the encrypted container (see Xml::Registry::registerEncryptedContainer()).
    QStringView encryption;
    // With BodyOnly, the content of Decrypted contains only the body and encryptMessage() only
    // encrypts the body, the pipeline sends the other elements in the clear.
    ContentEncryption contentEncryption = ContentEncryption::Sce;
    std::function<QXmppTask<DecryptResult>(DecryptRequest)> decryptMessage;
    std::function<QXmppTask<EncryptResult>(EncryptRequest)> encryptMessage;
    // Optional. The content of Decrypted replaces the child elements of the IQ.
    std::function<QXmppTask<EncryptResult>(EncryptIqRequest)> encryptIq;
    std::function<QXmppTask<DecryptResult>(DecryptIqRequest)> decryptIq;
};

// A view on the hooks of a client, e.g. Providers(hooks()) in an extension. A provider is removed
// when its owner is destroyed or the extension is removed from the client.
class QXMPP_EXPORT Providers
{
public:
    explicit Providers(QXmpp::Hooks hooks);

    void add(Provider provider) const;
    std::optional<Provider> find(QStringView encryption) const;
    bool contains(QStringView encryption) const { return find(encryption).has_value(); }

private:
    QXmpp::Hooks m_hooks;
};

}  // namespace QXmpp::E2ee

#endif  // QXMPPE2EEPROVIDER_H

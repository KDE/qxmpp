// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGES_H
#define QXMPPMESSAGES_H

#include "QXmppError.h"
#include "QXmppHooks.h"
#include "QXmppMessage.h"
#include "QXmppSendResult.h"
#include "QXmppSendStanzaParams.h"
#include "QXmppStanzaId.h"

#include <any>
#include <memory>
#include <optional>

#include <QString>

namespace QXmpp::Private {
class OutgoingMessages;
}

namespace QXmpp::E2ee {

// What an encryption encrypts of a message. Defined here instead of in QXmppE2eeProvider.h, which
// includes this header, because OutgoingMessage and Sent report it.
enum class ContentEncryption {
    // Stanza Content Encryption (XEP-0420): all sensitive elements, e.g. OMEMO 2
    Sce,
    // only the body, other sensitive elements are sent in the clear if allowed by PrepareBodyOnly
    // or dropped, e.g. OMEMO 0.3
    BodyOnly,
};

}  // namespace QXmpp::E2ee

namespace QXmpp::Messages {

class Context;

enum class Origin {
    Stream,
    CarbonReceived,
    CarbonSent,
    Mam,
    Injected,
};

enum class DecryptionState {
    NotEncrypted,
    Decrypted,
    NoEnvelope,
    Failed,
    Unsupported,
};

// Attached to the Context of messages with Origin::Mam.
class ArchiveInfo
{
public:
    // Bare JID of the archive, e.g. the own account or a MUC room.
    const QString &archiveJid() const { return m_archiveJid; }
    void setArchiveJid(const QString &jid) { m_archiveJid = jid; }

    // ID of the message in the archive.
    const QString &id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }

    // ID of the archive query the message is a result of.
    const QString &queryId() const { return m_queryId; }
    void setQueryId(const QString &id) { m_queryId = id; }

    // Real JID of the sender as reported by a MUC archive (XEP-0313), empty if there is none
    // or more than one. Not validated, see QXmppMucManagerV2.
    const QString &occupantJid() const { return m_occupantJid; }
    void setOccupantJid(const QString &jid) { m_occupantJid = jid; }

private:
    QString m_archiveJid;
    QString m_id;
    QString m_queryId;
    QString m_occupantJid;
};

struct ResolveSender {
    static constexpr QStringView Name = u"org.qxmpp.message.resolve-sender";
    using Signature = QXmppTask<QXmpp::HookResult>(Context &context);
};

struct Received {
    static constexpr QStringView Name = u"org.qxmpp.message.received";
    using Signature = QXmpp::HookResult(Context &context);
};

// Input and result of SelectEncryption.
class EncryptionSelection
{
public:
    EncryptionSelection(const QXmppMessage &message, bool required, QString encryption = {})
        : m_message(&message), m_encryption(std::move(encryption)), m_required(required)
    {
    }

    // The message to be sent.
    const QXmppMessage &message() const { return *m_message; }
    // JID of the recipient of the message.
    QString to() const { return m_message->to(); }

    // see QXmppSendStanzaParams::isEncryptionRequired(), also set for sensitive stanzas of managers
    bool isRequired() const { return m_required; }

    // Namespace of the encryption (see QXmpp::E2ee), empty for sending unencrypted (default).
    const QString &encryption() const { return m_encryption; }
    void setEncryption(QStringView encryption) { m_encryption = encryption.toString(); }

private:
    const QXmppMessage *m_message;
    QString m_encryption;
    bool m_required = false;
};

// Input and result of PrepareBodyOnly.
class BodyOnlyMessage
{
public:
    explicit BodyOnlyMessage(const QXmppMessage &original)
        : m_original(&original), m_message(original)
    {
    }

    // The message as sent to the recipients of encryptions protecting more than the body.
    const QXmppMessage &original() const { return *m_original; }

    // The message for the recipients of an encryption protecting only the body: its body is
    // encrypted, its other sensitive elements are sent in the clear if their namespace is in
    // unprotectedNamespaces(), else they are dropped. Changes of public elements have no effect.
    QXmppMessage &message() { return m_message; }
    const QXmppMessage &message() const { return m_message; }

    QList<QString> &unprotectedNamespaces() { return m_unprotectedNamespaces; }
    const QList<QString> &unprotectedNamespaces() const { return m_unprotectedNamespaces; }
    QXMPP_EXPORT void allowLowRiskElements();

    // The message is not sent to the recipients of an encryption protecting only the body.
    bool isUnsupported() const { return m_unsupported; }
    void setUnsupported() { m_unsupported = true; }

private:
    const QXmppMessage *m_original;
    QXmppMessage m_message;
    QList<QString> m_unprotectedNamespaces;
    bool m_unsupported = false;
};

// State of an outgoing message, created by the pipeline.
class OutgoingMessage
{
public:
    OutgoingMessage(QXmppMessage &&message, QXmppSendStanzaParams params, QString encryption)
        : m_message(std::move(message)), m_params(std::move(params)), m_encryption(std::move(encryption))
    {
    }

    // The message without end-to-end encryption.
    QXmppMessage &message() { return m_message; }
    const QXmppMessage &message() const { return m_message; }
    const QXmppSendStanzaParams &params() const { return m_params; }

    // Namespace of the encryption, empty if the message is sent unencrypted. After encrypting,
    // the one used, e.g. OMEMO 2 or OMEMO 0 for QXmpp::Omemo::Automatic.
    const QString &encryption() const { return m_encryption; }

    // Bare JIDs the message is encrypted for, see ResolveRecipients.
    QList<QString> &recipientJids() { return m_recipientJids; }
    const QList<QString> &recipientJids() const { return m_recipientJids; }

    // Set after encrypting: what has been encrypted of the message.
    std::optional<E2ee::ContentEncryption> contentEncryption() const { return m_contentEncryption; }

    // Set after encrypting: information of the provider, e.g. QXmpp::Omemo::EncryptionInfo.
    const std::any &encryptionInfo() const { return m_encryptionInfo; }

    // Error making sending fail, set by handlers of Prepare and ResolveRecipients.
    const std::optional<QXmppError> &error() const { return m_error; }
    void setError(QXmppError error) { m_error = std::move(error); }

private:
    friend class QXmpp::Private::OutgoingMessages;

    QXmppMessage m_message;
    QXmppSendStanzaParams m_params;
    QString m_encryption;
    QList<QString> m_recipientJids;
    std::optional<E2ee::ContentEncryption> m_contentEncryption;
    std::any m_encryptionInfo;
    std::optional<QXmppError> m_error;
};

// How far the delivery of a sent message has been confirmed.
enum class DeliveryState {
    // Written to the stream. Without Stream Management, it is unknown whether the server has
    // received it.
    Written,
    // The own server has acknowledged it via Stream Management (XEP-0198). It may not have
    // reached the recipient yet.
    ServerAcknowledged,
    // A handler of ConfirmDelivery has confirmed that the recipient has accepted it, e.g. a MUC
    // room by reflecting it.
    Confirmed,
};

// Confirmation of the delivery by a handler of ConfirmDelivery.
struct Confirmation {
    // ID of the message in the archive of the recipient, e.g. of a MUC room (XEP-0359)
    std::optional<QXmppStanzaId> archiveId = {};
};

// Result of a message sent via QXmppClient::sendMessage().
class Sent
{
public:
    Sent(std::shared_ptr<const OutgoingMessage> message, DeliveryState deliveryState, std::optional<QXmppStanzaId> archiveId = {})
        : m_message(std::move(message)), m_deliveryState(deliveryState), m_archiveId(std::move(archiveId))
    {
    }

    DeliveryState deliveryState() const { return m_deliveryState; }
    // see Confirmation
    const std::optional<QXmppStanzaId> &archiveId() const { return m_archiveId; }

    // The message as prepared by the pipeline, without end-to-end encryption.
    const QXmppMessage &message() const { return m_message->message(); }
    const QXmppSendStanzaParams &params() const { return m_message->params(); }

    // see OutgoingMessage
    const QString &encryption() const { return m_message->encryption(); }
    const QList<QString> &recipientJids() const { return m_message->recipientJids(); }
    std::optional<E2ee::ContentEncryption> contentEncryption() const { return m_message->contentEncryption(); }
    const std::any &encryptionInfo() const { return m_message->encryptionInfo(); }

private:
    std::shared_ptr<const OutgoingMessage> m_message;
    DeliveryState m_deliveryState;
    std::optional<QXmppStanzaId> m_archiveId;
};

struct SelectEncryption {
    static constexpr QStringView Name = u"org.qxmpp.message.select-encryption";
    using Signature = QXmppTask<void>(EncryptionSelection &selection);
};

struct Prepare {
    static constexpr QStringView Name = u"org.qxmpp.message.prepare";
    using Signature = QXmpp::HookResult(OutgoingMessage &message);
};

struct ResolveRecipients {
    static constexpr QStringView Name = u"org.qxmpp.message.resolve-recipients";
    using Signature = QXmppTask<void>(OutgoingMessage &message);
};

struct PrepareBodyOnly {
    static constexpr QStringView Name = u"org.qxmpp.message.prepare-body-only";
    using Signature = void(BodyOnlyMessage &message);
};

struct ConfirmDelivery {
    static constexpr QStringView Name = u"org.qxmpp.message.confirm-delivery";
    // confirmation: set if the delivery has been confirmed, an error makes sending fail
    using Signature = QXmppTask<void>(const OutgoingMessage &message, std::optional<QXmpp::Result<Confirmation>> &confirmation);
};

}  // namespace QXmpp::Messages

#endif  // QXMPPMESSAGES_H

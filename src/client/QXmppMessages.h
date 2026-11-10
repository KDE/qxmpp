// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGES_H
#define QXMPPMESSAGES_H

#include "QXmppError.h"
#include "QXmppHooks.h"
#include "QXmppMessage.h"
#include "QXmppSendStanzaParams.h"

#include <optional>

#include <QString>

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

    // Real JID of the sender as reported by a MUC archive (XEP-0313), empty if there is none
    // or more than one. Not validated, see QXmppMucManagerV2.
    const QString &occupantJid() const { return m_occupantJid; }
    void setOccupantJid(const QString &jid) { m_occupantJid = jid; }

private:
    QString m_archiveJid;
    QString m_id;
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

    const QXmppMessage &message() const { return *m_message; }

    // see QXmppSendStanzaParams::isEncryptionRequired() and QXmppClient::sendSensitive()
    bool isRequired() const { return m_required; }

    // Namespace of the encryption (see QXmpp::E2ee), empty for sending unencrypted (default).
    const QString &encryption() const { return m_encryption; }
    void setEncryption(QStringView encryption) { m_encryption = encryption.toString(); }

private:
    const QXmppMessage *m_message = nullptr;
    QString m_encryption;
    bool m_required = false;
};

// State of an outgoing message, created by the pipeline.
class OutgoingMessage
{
public:
    OutgoingMessage(QXmppMessage &&message, QXmppSendStanzaParams params, QString encryption)
        : m_message(std::move(message)), m_params(std::move(params)), m_encryption(std::move(encryption))
    {
    }

    QXmppMessage &message() { return m_message; }
    const QXmppMessage &message() const { return m_message; }
    const QXmppSendStanzaParams &params() const { return m_params; }

    // Namespace of the selected encryption, empty if the message is sent unencrypted.
    const QString &encryption() const { return m_encryption; }

    // Bare JIDs the message is encrypted for, see ResolveRecipients.
    QList<QString> &recipientJids() { return m_recipientJids; }
    const QList<QString> &recipientJids() const { return m_recipientJids; }

    // Error making sending fail, set by handlers of Prepare and ResolveRecipients.
    const std::optional<QXmppError> &error() const { return m_error; }
    void setError(QXmppError error) { m_error = std::move(error); }

private:
    QXmppMessage m_message;
    QXmppSendStanzaParams m_params;
    QString m_encryption;
    QList<QString> m_recipientJids;
    std::optional<QXmppError> m_error;
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

}  // namespace QXmpp::Messages

#endif  // QXMPPMESSAGES_H

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMessageContext.h"

#include "QXmppMessageContext_p.h"
#include "QXmppUtils.h"

namespace QXmpp::Messages {

/*!
    \namespace QXmpp::Messages
    \inmodule QXmpp

    Processing of incoming and outgoing messages.

    Incoming messages run through these phases:
    \list 1
    \li Unwrap: the stream, carbons and MAM create a Context with their Origin.
    \li ResolveSender: asynchronous, e.g. MUC sets the real sender.
    \li Decrypt: asynchronous, via the provider of the encryption method
        (QXmpp::E2ee::Provider).
    \li Received: synchronous, handlers consume the message.
    \li Deliver: QXmppClient::messageReceived() if no handler consumed the message.
    \endlist

    Messages of the same conversation are processed in the order in which they were received,
    also across the asynchronous decryption.

    Messages sent via QXmppClient::send() (and the deprecated QXmppClient::sendSensitive() and
    QXmppClient::reply()) run through these phases:
    \list 1
    \li SelectEncryption: asynchronous, usually the application selects the encryption with its
        policy per contact.
    \li Prepare: synchronous, handlers add elements or cancel sending.
    \li ResolveRecipients: asynchronous, only for encrypted messages, e.g. MUC sets the
        members of the room.
    \li Encrypt: asynchronous, via the provider of the selected encryption
        (QXmpp::E2ee::Provider). The public part of Explicit Message Encryption
        (\xep{0380}{Explicit Message Encryption}) and a fallback body are added for messages
        with a body.
    \li Send.
    \li ConfirmDelivery: asynchronous, e.g. MUC waits for the reflection of the room. The task
        returned by QXmppClient::send() finishes afterwards.
    \endlist

    Outgoing messages to the same JID are sent in the order in which they were passed, also
    across the asynchronous phases. Waiting for the confirmation of the delivery does not block
    the next message.

    IQs sent via QXmppClient::sendSensitiveIq() are encrypted with the encryption selected by
    the SelectEncryption hook too. Encrypted IQ requests are decrypted by the e2ee provider and
    passed to the extensions with their QXmppE2eeMetadata. Replies of handlers added via
    QXmpp::Iq::Received are encrypted like the request.

    \since QXmpp 1.18
*/

/*!
    \class QXmpp::Messages::EncryptionSelection
    \inmodule QXmpp

    Input and result of the SelectEncryption hook.

    The hook runs for messages and for IQs sent via QXmppClient::sendSensitiveIq() and as
    responses to IQ requests. For IQs, only the attributes of the stanza are available, not the
    payload.

    The encryption is identified by its namespace (see QXmpp::E2ee), it is empty for sending the
    message unencrypted. Replies of QXmpp's managers (e.g. delivery receipts) and responses to
    IQ requests are preset with the encryption of the message or request replied to, so they
    stay encrypted without a handler.

    isRequired() is set for sensitive stanzas sent by QXmpp's managers (e.g. roster subscription
    requests) and for messages with QXmppSendStanzaParams::isEncryptionRequired().
*/

/*!
    \class QXmpp::Messages::OutgoingMessage
    \inmodule QXmpp

    State of an outgoing message in the Prepare and ResolveRecipients hooks.
*/

/*!
    \struct QXmpp::Messages::SelectEncryption
    \inmodule QXmpp

    Asynchronous hook selecting the end-to-end encryption of an outgoing message.

    It runs for every message, also for messages sent by managers on their own (e.g. receipts),
    except for messages that already contain encrypted elements, e.g. a key exchange created by
    a provider. They are sent as they are.
    Handlers run in order and may override the encryption set by earlier ones. Handlers that need
    to wait, e.g. for a database, return a QXmppTask, others can return \c void:

    \code
    client->hooks(this).add<QXmpp::Messages::SelectEncryption>([this](QXmpp::Messages::EncryptionSelection &selection) {
        if (isEncryptionEnabled(selection.stanza().to())) {
            selection.setEncryption(QXmpp::E2ee::Omemo2);
        }
    });
    \endcode

    If no encryption is selected, sensitive stanzas sent by QXmpp's managers (and via the
    deprecated QXmppClient::sendSensitive()) are encrypted with the deprecated encryption
    extension (QXmppClient::setEncryptionExtension()) as in earlier versions.
*/

/*!
    \struct QXmpp::Messages::Prepare
    \inmodule QXmpp

    Hook run synchronously for each outgoing message after the encryption has been selected.
    Handlers can modify the message. Handlers returning QXmpp::HookResult::Stop cancel sending,
    which then fails with the error set via QXmpp::Messages::OutgoingMessage::setError() or else
    with QXmpp::SendError::Cancelled.
*/

/*!
    \struct QXmpp::Messages::ConfirmDelivery
    \inmodule QXmpp

    Asynchronous hook run for each outgoing message right after it has been written to the
    stream, also before the server has acknowledged it, as a confirmation may arrive earlier.

    Handlers wait for a confirmation of the delivery and set the result: an error makes sending
    fail, QXmpp::SendSuccess with \c acknowledged set confirms the delivery. Without confirmation,
    the result is the one of the stream (see QXmppClient::send()). If the stream reports an error,
    sending fails without waiting for the handlers.

    QXmppMucManagerV2 waits for the reflection of messages sent to joined rooms.
*/

/*!
    \struct QXmpp::Messages::ResolveRecipients
    \inmodule QXmpp

    Asynchronous hook run for each outgoing message that is encrypted. The recipients are
    QXmppSendStanzaParams::encryptionJids() or else the bare JID of the addressee. Handlers can
    add recipients, e.g. the members of a MUC, and may wait for them to be retrieved. Handlers
    can make sending fail with QXmpp::Messages::OutgoingMessage::setError().

    QXmppMucManagerV2 encrypts messages to joined rooms for the real JIDs of the occupants. In
    members-only rooms, all members, admins and owners are added, also if they are offline
    (\xep{0384}{OMEMO Encryption}). The lists are retrieved before the first encrypted message
    after joining and are kept up to date afterwards. Messages to anonymous rooms can not be
    encrypted and fail.
*/

/*!
    \enum QXmpp::Messages::Origin

    Where an incoming message came from.

    \value Stream Received directly from the server.
    \value CarbonReceived Copy of a message received by another own device.
    \value CarbonSent Copy of a message sent by another own device.
    \value Mam Result of an archive query, not delivered as a live message. The context has a
    QXmpp::Messages::ArchiveInfo attachment.
    \value Injected Injected by an extension via QXmppClientExtension::injectMessage().
*/

/*!
    \class QXmpp::Messages::ArchiveInfo
    \inmodule QXmpp

    Attachment of messages retrieved from an archive (\xep{0313}{Message Archive Management}),
    see Context::attachment().

    occupantJid() is the real JID of the sender reported by a MUC archive. It is only set if the
    archive reports exactly one, as a sender could add one to its message. It must only be trusted
    for messages from the archive of a non-anonymous room.

    \since QXmpp 1.18
*/

/*!
    \struct QXmpp::Messages::ResolveSender
    \inmodule QXmpp

    Asynchronous hook run for each incoming message before it is decrypted. MUC sets the real
    sender here. Handlers returning QXmpp::HookResult::Stop drop the message.

    It is started as soon as the message has been received, not after earlier messages of the
    conversation have been processed, so handlers returning without waiting see the state at
    the time the message was received if no earlier handler waits (e.g. the occupants of a MUC). Handlers that need to wait,
    e.g. for a database, return a QXmppTask. Later messages of the conversation wait for them.
*/

/*!
    \struct QXmpp::Messages::Received
    \inmodule QXmpp

    Hook run for each incoming message after the decryption. Handlers returning
    QXmpp::HookResult::Stop consume the message, it is then not delivered via
    QXmppClient::messageReceived(). Messages of archive queries (Origin::Mam) are never delivered
    via QXmppClient::messageReceived(), consumed ones are not returned by
    QXmppMamManager::retrieveMessages().
*/

/*!
    \enum QXmpp::Messages::DecryptionState

    \value NotEncrypted The message was not end-to-end encrypted.
    \value Decrypted The message was decrypted.
    \value NoEnvelope The message is encrypted, but not for this device, e.g. an own message
        reflected by a MUC.
    \value Failed The decryption failed, see Context::decryptionError().
    \value Unsupported There is no provider for the encryption method.
*/

/*!
    \class QXmpp::Messages::Context
    \inmodule QXmpp

    State of an incoming message while it is processed.

    \since QXmpp 1.18
*/

/*!
    Creates a context for \a message from \a origin.

    The sender is the bare JID of 'from'. The conversation is the bare JID of 'to' for
    Origin::CarbonSent and the one of 'from' otherwise.
*/
Context::Context(QXmppMessage &&message, Origin origin)
    : d(std::make_unique<ContextPrivate>())
{
    d->origin = origin;
    d->senderJid = QXmppUtils::jidToBareJid(message.from());
    d->conversationJid = QXmppUtils::jidToBareJid(origin == Origin::CarbonSent ? message.to() : message.from());
    d->message = std::move(message);
}

Context::Context(Context &&) noexcept = default;
Context &Context::operator=(Context &&) noexcept = default;
Context::~Context() = default;

/*!
    Returns the message.
*/
QXmppMessage &Context::message()
{
    return d->message;
}

/*!
    \overload
*/
const QXmppMessage &Context::message() const
{
    return d->message;
}

/*!
    Returns where the message came from.
*/
Origin Context::origin() const
{
    return d->origin;
}

/*!
    Returns the bare JID of the real sender.

    For messages from a MUC room, it is set by the QXmpp::Messages::ResolveSender handler of
    the MUC manager if the real JID is known. The decryption uses it to look up the keys.
*/
const QString &Context::senderJid() const
{
    return d->senderJid;
}

/*!
    Sets the bare JID of the real sender to \a jid.
*/
void Context::setSenderJid(const QString &jid)
{
    d->senderJid = jid;
}

/*!
    Returns the bare JID of the conversation, i.e. of the contact or the room.
*/
const QString &Context::conversationJid() const
{
    return d->conversationJid;
}

/*!
    Sets the bare JID of the conversation to \a jid.
*/
void Context::setConversationJid(const QString &jid)
{
    d->conversationJid = jid;
}

/*!
    Returns the result of the decryption.
*/
DecryptionState Context::decryptionState() const
{
    return d->decryptionState;
}

/*!
    Returns the metadata of the end-to-end encryption if the message was decrypted.
*/
const std::optional<QXmppE2eeMetadata> &Context::e2eeMetadata() const
{
    return d->e2eeMetadata;
}

/*!
    Returns the error if the decryption failed.
*/
const std::optional<QXmppError> &Context::decryptionError() const
{
    return d->decryptionError;
}

/*!
    \fn template<typename T> T *QXmpp::Messages::Context::attachment()

    Returns the attached value of type \c T or \c nullptr.
*/

/*!
    \fn template<typename T> const T *QXmpp::Messages::Context::attachment() const

    \overload
*/

/*!
    \fn template<typename T> T &QXmpp::Messages::Context::attach(T value)

    Attaches \a value, replacing an attached value of the same type, and returns it.
*/

std::any *Context::attachmentInternal(std::type_index type)
{
    auto it = d->attachments.find(type);
    return it != d->attachments.end() ? &it->second : nullptr;
}

const std::any *Context::attachmentInternal(std::type_index type) const
{
    auto it = d->attachments.find(type);
    return it != d->attachments.end() ? &it->second : nullptr;
}

std::any *Context::attachInternal(std::type_index type, std::any &&value)
{
    return &(d->attachments.insert_or_assign(type, std::move(value)).first->second);
}

}  // namespace QXmpp::Messages

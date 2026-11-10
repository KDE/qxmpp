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

    \since QXmpp 1.18
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
    the time the message was received if no earlier handler waits (e.g. the occupants of a MUC).
    Handlers that need to wait, e.g. for a database, return a QXmppTask. Later messages of the
    conversation wait for them.
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

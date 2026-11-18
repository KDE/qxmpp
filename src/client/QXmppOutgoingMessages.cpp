// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeProvider.h"
#include "QXmppFallback.h"
#include "QXmppHooks_p.h"
#include "QXmppMessages.h"
#include "QXmppOutgoingMessages_p.h"
#include "QXmppPacket_p.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppTrustMessageElement.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"
#include "QXmppXmlElement.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"

#include <algorithm>

#include <QDomDocument>
#include <QXmlStreamWriter>

using namespace QXmpp;
using namespace QXmpp::Messages;

namespace QXmpp::Private {

void setReflectionIds(QXmppMessage &message)
{
    if (message.originId().isEmpty()) {
        message.setOriginId(message.id().isEmpty() ? QXmppUtils::generateStanzaUuid() : message.id());
    }
    if (message.id().isEmpty()) {
        message.setId(message.originId());
    }
}

// The message for the recipients of an encryption protecting only the body.
struct BodyOnly {
    QXmppMessage message;
    // its sensitive elements allowed in the clear
    QList<Xml::Element> clearElements;
};

static std::optional<BodyOnly> createBodyOnly(Hooks hooks, const QXmppMessage &original)
{
    BodyOnlyMessage bodyOnly(original);
    hooks.run<PrepareBodyOnly>(bodyOnly);
    const auto &message = bodyOnly.message();
    const auto &namespaces = bodyOnly.unprotectedNamespaces();
    if (bodyOnly.isUnsupported()) {
        return {};
    }
    // would appear as a new message
    if (!message.replaceId().isEmpty() && !namespaces.contains(ns_message_correct)) {
        return {};
    }

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(u"message"_s);
    writer.writeDefaultNamespace(ns_client.toString());
    message.serializeExtensions(&writer, SceSensitive);
    writer.writeEndElement();
    QDomDocument document;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    document.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing);
#else
    document.setContent(xml, true);
#endif

    QList<Xml::Element> clearElements;
    for (const auto &element : iterChildElements(document.documentElement())) {
        // the body is encrypted, the namespace of the stanza can not be allowed
        if (element.namespaceURI() != ns_client && namespaces.contains(element.namespaceURI())) {
            clearElements.append(Xml::Element::fromDom(element));
        }
    }
    if (message.body().isEmpty() && clearElements.isEmpty()) {
        return {};
    }
    return BodyOnly { message, clearElements };
}

// Adds the encrypted elements and the public parts of Explicit Message Encryption (XEP-0380).
// bodyOnly is set if the message has been encrypted for recipients of an encryption protecting
// only the body.
static void applyEncrypted(QXmppMessage &message, const QString &selectedEncryption, const E2ee::Encrypted &encrypted, const BodyOnly *bodyOnly)
{
    const auto &encryption = encrypted.encryption().isEmpty() ? selectedEncryption : encrypted.encryption();

    // encryptions unknown to QXmpp have no name
    auto fallbackBody = [&] {
        const auto name = message.encryptionName().isEmpty() ? encryption : message.encryptionName();
        return u"This message is encrypted with %1 but could not be decrypted"_s.arg(name);
    };

    // Fallback markers are serialized in the public part too. The ones of the sensitive part are
    // encrypted, so they must not be sent publicly.
    message.setFallbackMarkers({});

    // A message with a body gets a fallback body, so clients not supporting the encryption can
    // show something and servers store it. Trust messages look the same, so they can not be
    // told apart from messages with a body.
    if (!message.body().isEmpty() || message.trustMessageElement() || (bodyOnly && !bodyOnly->message.body().isEmpty())) {
        message.setEncryptionMethodNs(encryption);
        message.setE2eeFallbackBody(fallbackBody());
        message.setFallbackMarkers({ QXmppFallback { encryption, { { QXmppFallback::Body, {} } } } });
    }

    // Fallback markers among the elements in the clear (e.g. of a reply) refer to the decrypted body.
    if (bodyOnly) {
        for (const auto &element : bodyOnly->clearElements) {
            message.publicExtensions().add(element);
        }
    }
    message.extensions().append(encrypted.elements());
}

// State of a sent message until its delivery has been confirmed.
struct OutgoingMessages::Delivery {
    std::shared_ptr<OutgoingMessage> message;
    QXmppPromise<Result<Sent>> promise;
    // set by handlers of ConfirmDelivery
    std::optional<Result<Confirmation>> confirmation;
    // result of the stream
    std::optional<SendResult> sent;
    bool confirmed = false;
    bool finished = false;

    void finish(Result<Sent> &&value)
    {
        if (!finished) {
            finished = true;
            promise.finish(std::move(value));
        }
    }

    // The delivery has failed if the stream or a handler reports an error. It is confirmed by a
    // handler (e.g. the reflection of a MUC) or else by the stream.
    void update()
    {
        if (sent && std::holds_alternative<QXmppError>(*sent)) {
            finish(std::get<QXmppError>(std::move(*sent)));
        } else if (confirmed && confirmation) {
            if (auto *error = std::get_if<QXmppError>(&*confirmation)) {
                finish(std::move(*error));
            } else {
                finish(Sent(message, DeliveryState::Confirmed, std::get<Confirmation>(std::move(*confirmation)).archiveId));
            }
        } else if (confirmed && sent) {
            finish(Sent(message, std::get<SendSuccess>(*sent).acknowledged ? DeliveryState::ServerAcknowledged : DeliveryState::Written));
        }
    }
};

OutgoingMessages::OutgoingMessages(QXmppClient *client, QXmppClientPrivate *clientPrivate, Hooks hooks)
    : m_client(client), m_clientPrivate(clientPrivate), m_hooks(hooks)
{
}

bool OutgoingMessages::isPipelineStanza(const QXmppStanza &stanza)
{
    return typeid(stanza) == typeid(QXmppMessage);
}

QXmppTask<Result<Sent>> OutgoingMessages::send(QXmppMessage &&message, QXmppSendStanzaParams params, Mode mode, QString encryption)
{
    const auto conversationJid = QXmppUtils::jidToBareJid(message.to());
    auto &queue = m_queues[conversationJid];
    queue.push_back(Pending { std::move(message), std::move(params), mode, std::move(encryption), {} });
    auto task = queue.back().result.task();
    if (queue.size() == 1) {
        processFront(conversationJid);
    }
    return task;
}

QXmppTask<SendResult> OutgoingMessages::sendCompat(QXmppMessage &&message, QXmppSendStanzaParams params, Mode mode, QString encryption)
{
    return send(std::move(message), std::move(params), mode, std::move(encryption)).then(m_client, [](Result<Sent> &&result) -> SendResult {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            return std::move(*error);
        }
        return SendSuccess { std::get<Sent>(result).deliveryState() != DeliveryState::Written };
    });
}

// Processes the first message of a conversation. The next message is processed after it has been
// written to the stream, so the order is kept across asynchronous steps.
QXmppTask<void> OutgoingMessages::processFront(QString conversationJid)
{
    // stays valid: only the front is removed from the queue, by this function
    auto &pending = m_queues.at(conversationJid).front();

    // Messages already containing encrypted elements (e.g. a key exchange created by a provider)
    // are sent as they are.
    QString encryptedWith;
    for (const auto &item : pending.message.extensions()) {
        if (const auto encryption = Xml::Registry::encryption(item); !encryption.isNull()) {
            encryptedWith = encryption.toString();
            break;
        }
    }
    const bool encrypted = !encryptedWith.isEmpty();

    const bool fixed = pending.mode == Mode::Fixed;
    const bool required = pending.mode != Mode::Default || pending.params.isEncryptionRequired();
    EncryptionSelection selection(pending.message, required, pending.encryption);
    if (!encrypted && !fixed) {
        co_await m_hooks.run<SelectEncryption>(selection).withContext(m_client);
    }

    if (fixed && !encrypted && selection.encryption().isEmpty()) {
        finishFront(conversationJid, QXmppError { u"No end-to-end encryption has been given for a message that must be encrypted"_s, SendError::EncryptionError });
        co_return;
    }

    std::optional<E2ee::Provider> provider;
    if (!selection.encryption().isEmpty()) {
        provider = E2ee::Providers(m_hooks).find(selection.encryption());
        if (!provider || !provider->encryptMessage) {
            finishFront(conversationJid, QXmppError { u"No end-to-end encryption provider for "_s + selection.encryption(), SendError::EncryptionError });
            co_return;
        }
        // The whole content of messages with a fixed encryption must be encrypted.
        if (fixed && provider->contentEncryption == E2ee::ContentEncryption::BodyOnly) {
            finishFront(conversationJid, QXmppError { selection.encryption() + u" only encrypts the body, the message can not be encrypted with it"_s, SendError::EncryptionError });
            co_return;
        }
    }

    auto outgoingPtr = std::make_shared<OutgoingMessage>(std::move(pending.message), pending.params, encrypted ? encryptedWith : selection.encryption());
    auto &outgoing = *outgoingPtr;
    if (m_hooks.run<Prepare>(outgoing) == HookResult::Stop || outgoing.error()) {
        auto error = outgoing.error().value_or(QXmppError { u"Sending the message has been cancelled"_s, SendError::Cancelled });
        finishFront(conversationJid, std::move(error));
        co_return;
    }

    if (!provider) {
        auto *legacyEncryption = m_clientPrivate->encryptionExtension;
        if (pending.mode == Mode::Sensitive && legacyEncryption && !encrypted) {
            // like before the pipeline existed; a copy is encrypted, the message is still used by
            // the handlers of ConfirmDelivery and returned with the result
            auto result = co_await legacyEncryption->encryptMessage(QXmppMessage(outgoing.message()), pending.params).withContext(m_client);
            if (auto *error = std::get_if<QXmppError>(&result)) {
                finishFront(conversationJid, std::move(*error));
            } else {
                QByteArray xml;
                QXmlStreamWriter writer(&xml);
                const auto &encrypted = *std::get<std::unique_ptr<QXmppMessage>>(result);
                encrypted.toXml(&writer, ScePublic);
                outgoing.m_encryption = encrypted.encryptionMethodNs();
                finishFront(conversationJid, m_clientPrivate->stream->streamAckManager().send(QXmppPacket(xml, true)), outgoingPtr);
            }
            co_return;
        }
        if (pending.params.isEncryptionRequired() && !encrypted) {
            finishFront(conversationJid, QXmppError { u"No end-to-end encryption has been selected for a message that must be encrypted"_s, SendError::EncryptionError });
            co_return;
        }
        finishFront(conversationJid, m_clientPrivate->stream->streamAckManager().send(QXmppPacket(outgoing.message())), outgoingPtr);
        co_return;
    }

    outgoing.recipientJids() = pending.params.encryptionJids();
    if (outgoing.recipientJids().isEmpty()) {
        outgoing.recipientJids().append(QXmppUtils::jidToBareJid(outgoing.message().to()));
    }
    co_await m_hooks.run<ResolveRecipients>(outgoing).withContext(m_client);
    if (outgoing.error()) {
        finishFront(conversationJid, QXmppError(*outgoing.error()));
        co_return;
    }

    E2ee::EncryptRequest request;
    request.setMessage(outgoing.message());
    request.setRecipientJids(outgoing.recipientJids());
    request.setParams(pending.params);
    // created once, also if the provider requests it several times
    auto bodyOnly = std::make_shared<std::optional<std::optional<BodyOnly>>>();
    // The whole content of messages with a fixed encryption must be encrypted, so there is no
    // variant for recipients of an encryption protecting only the body.
    auto getBodyOnly = [hooks = m_hooks, bodyOnly, original = outgoing.message(), fixed]() -> const std::optional<BodyOnly> & {
        if (!bodyOnly->has_value()) {
            *bodyOnly = fixed ? std::nullopt : createBodyOnly(hooks, original);
        }
        return **bodyOnly;
    };
    request.setBodyOnlyMessage([getBodyOnly]() -> std::optional<QXmppMessage> {
        if (const auto &bodyOnly = getBodyOnly()) {
            return bodyOnly->message;
        }
        return {};
    });

    auto result = co_await provider->encryptMessage(std::move(request)).withContext(m_client);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        finishFront(conversationJid, std::move(*error));
        co_return;
    }
    const auto &encryptedResult = std::get<E2ee::Encrypted>(result);

    const BodyOnly *usedBodyOnly = nullptr;
    if (encryptedResult.contentEncryption().value_or(provider->contentEncryption) == E2ee::ContentEncryption::BodyOnly) {
        usedBodyOnly = getBodyOnly() ? &*getBodyOnly() : nullptr;
        if (!usedBodyOnly) {
            finishFront(conversationJid, QXmppError { u"The message can not be sent with an encryption protecting only the body"_s, SendError::EncryptionError });
            co_return;
        }
    }

    auto message = outgoing.message();
    applyEncrypted(message, outgoing.encryption(), encryptedResult, usedBodyOnly);
    if (!encryptedResult.encryption().isEmpty()) {
        outgoing.m_encryption = encryptedResult.encryption();
    }
    outgoing.m_contentEncryption = encryptedResult.contentEncryption().value_or(provider->contentEncryption);
    outgoing.m_encryptionInfo = encryptedResult.info();

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    message.toXml(&writer, ScePublic);
    finishFront(conversationJid, m_clientPrivate->stream->streamAckManager().send(QXmppPacket(xml, true)), outgoingPtr);
}

void OutgoingMessages::finishFront(const QString &conversationJid, QXmppTask<SendResult> &&sendTask, std::shared_ptr<OutgoingMessage> message)
{
    auto queue = m_queues.find(conversationJid);
    auto delivery = std::make_shared<Delivery>(std::move(message), std::move(queue->second.front().result));
    queue->second.pop_front();

    // The message has been written, the next one can be processed. The confirmation of the
    // delivery does not block it. This is done before reporting the result, which may send
    // further messages.
    if (queue->second.empty()) {
        m_queues.erase(queue);
    } else {
        processFront(conversationJid);
    }

    confirmDelivery(std::move(delivery), std::move(sendTask));
}

// Reports the result of a written message after its delivery has been confirmed.
void OutgoingMessages::confirmDelivery(std::shared_ptr<Delivery> delivery, QXmppTask<SendResult> sendTask)
{
    if (delivery->message) {
        // Started immediately, not after the acknowledgement of the server: a confirmation, e.g.
        // the reflection of a MUC, may arrive earlier.
        m_hooks.run<ConfirmDelivery>(std::as_const(*delivery->message), delivery->confirmation).then(m_client, [delivery] {
            delivery->confirmed = true;
            delivery->update();
        });
    } else {
        // not written
        delivery->confirmed = true;
    }

    sendTask.then(m_client, [delivery](SendResult &&result) {
        delivery->sent = std::move(result);
        delivery->update();
    });
}

void OutgoingMessages::finishFront(const QString &conversationJid, QXmppError &&error)
{
    QXmppPromise<SendResult> promise;
    auto task = promise.task();
    promise.finish(std::move(error));
    finishFront(conversationJid, std::move(task));
}

}  // namespace QXmpp::Private

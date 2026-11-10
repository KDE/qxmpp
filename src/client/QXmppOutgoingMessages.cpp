// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClient_p.h"
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
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"

#include <algorithm>

#include <QXmlStreamWriter>

using namespace QXmpp;
using namespace QXmpp::Messages;

namespace QXmpp::Private {

// Adds the encrypted elements and the public parts of Explicit Message Encryption (XEP-0380).
static void applyEncrypted(QXmppMessage &message, const QString &encryption, const E2ee::Encrypted &encrypted)
{
    // Fallback markers are serialized in the public part too. The ones of the sensitive part are
    // encrypted, so they must not be sent publicly.
    message.setFallbackMarkers({});

    // A message with a body gets a fallback body, so clients not supporting the encryption can
    // show something and servers store it. Trust messages look the same, so they can not be
    // told apart from messages with a body.
    if (!message.body().isEmpty() || message.trustMessageElement()) {
        message.setEncryptionMethodNs(encryption);
        // encryptions unknown to QXmpp have no name
        const auto name = message.encryptionName().isEmpty() ? encryption : message.encryptionName();
        message.setE2eeFallbackBody(u"This message is encrypted with %1 but could not be decrypted"_s.arg(name));
        message.setFallbackMarkers({ QXmppFallback { encryption, { { QXmppFallback::Body, {} } } } });
    }

    message.extensions().append(encrypted.elements());
}

// State of a sent message until its delivery has been confirmed.
struct OutgoingMessages::Delivery {
    std::shared_ptr<OutgoingMessage> message;
    QXmppPromise<SendResult> promise;
    // set by handlers of ConfirmDelivery
    SendResult result = SendSuccess();
    // result of the stream
    std::optional<SendResult> sent;
    bool confirmed = false;
    bool finished = false;

    void finish(SendResult &&value)
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
            finish(std::move(*sent));
        } else if (confirmed && (std::holds_alternative<QXmppError>(result) || std::get<SendSuccess>(result).acknowledged)) {
            finish(std::move(result));
        } else if (confirmed && sent) {
            finish(std::move(*sent));
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

QXmppTask<SendResult> OutgoingMessages::send(QXmppMessage &&message, QXmppSendStanzaParams params, Mode mode, QString encryption)
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

// Processes the first message of a conversation. The next message is processed after it has been
// written to the stream, so the order is kept across asynchronous steps.
QXmppTask<void> OutgoingMessages::processFront(QString conversationJid)
{
    // stays valid: only the front is removed from the queue, by this function
    auto &pending = m_queues.at(conversationJid).front();

    // Messages already containing encrypted elements (e.g. a key exchange created by a provider)
    // are sent as they are.
    const bool encrypted = std::ranges::any_of(pending.message.extensions(), [](const std::any &item) {
        return !Xml::Registry::encryption(item).isNull();
    });

    const bool required = pending.mode == Mode::Sensitive || pending.params.isEncryptionRequired();
    EncryptionSelection selection(pending.message, required, pending.encryption);
    if (!encrypted) {
        co_await m_hooks.run<SelectEncryption>(selection).withContext(m_client);
    }

    std::optional<E2ee::Provider> provider;
    if (!selection.encryption().isEmpty()) {
        provider = E2ee::Providers(m_hooks).find(selection.encryption());
        if (!provider || !provider->encryptMessage) {
            finishFront(conversationJid, QXmppError { u"No end-to-end encryption provider for "_s + selection.encryption(), SendError::EncryptionError });
            co_return;
        }
    }

    auto outgoingPtr = std::make_shared<OutgoingMessage>(std::move(pending.message), pending.params, selection.encryption());
    auto &outgoing = *outgoingPtr;
    if (m_hooks.run<Prepare>(outgoing) == HookResult::Stop || outgoing.error()) {
        auto error = outgoing.error().value_or(QXmppError { u"Sending the message has been cancelled"_s, SendError::Cancelled });
        finishFront(conversationJid, std::move(error));
        co_return;
    }

    if (!provider) {
        auto *legacyEncryption = m_clientPrivate->encryptionExtension;
        if (pending.mode == Mode::Sensitive && legacyEncryption && !encrypted) {
            // like before the pipeline existed
            auto result = co_await legacyEncryption->encryptMessage(std::move(outgoing.message()), pending.params).withContext(m_client);
            if (auto *error = std::get_if<QXmppError>(&result)) {
                finishFront(conversationJid, std::move(*error));
            } else {
                QByteArray xml;
                QXmlStreamWriter writer(&xml);
                const auto &encrypted = *std::get<std::unique_ptr<QXmppMessage>>(result);
                encrypted.toXml(&writer, ScePublic);
                outgoing.message() = encrypted;
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
        finishFront(conversationJid, *outgoing.error());
        co_return;
    }

    E2ee::EncryptRequest request;
    request.setMessage(outgoing.message());
    request.setRecipientJids(outgoing.recipientJids());
    request.setParams(pending.params);

    auto result = co_await provider->encryptMessage(std::move(request)).withContext(m_client);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        finishFront(conversationJid, std::move(*error));
        co_return;
    }

    auto &message = outgoing.message();
    applyEncrypted(message, outgoing.encryption(), std::get<E2ee::Encrypted>(result));

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
        m_hooks.run<ConfirmDelivery>(std::as_const(*delivery->message), delivery->result).then(m_client, [delivery] {
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

void OutgoingMessages::finishFront(const QString &conversationJid, SendResult &&result)
{
    QXmppPromise<SendResult> promise;
    auto task = promise.task();
    promise.finish(std::move(result));
    finishFront(conversationJid, std::move(task));
}

}  // namespace QXmpp::Private

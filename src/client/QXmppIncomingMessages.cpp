// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClientExtension.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeProvider.h"
#include "QXmppHooks_p.h"
#include "QXmppIncomingMessages_p.h"
#include "QXmppMessage.h"
#include "QXmppMessageContext_p.h"
#include "QXmppMessageHandler.h"
#include "QXmppMessages.h"
#include "QXmppUtils.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Messages;

namespace QXmpp::Private {

static QStringView encryption(const QXmppMessage &message)
{
    for (const auto &item : message.extensions()) {
        if (auto encryption = Xml::Registry::encryption(item); !encryption.isNull()) {
            return encryption;
        }
    }
    return {};
}

static void fail(Context &context, DecryptionState state, QString errorText = {})
{
    auto &d = MessageContextAccess::d(context);
    d.decryptionState = state;
    if (!errorText.isEmpty()) {
        d.decryptionError = QXmppError { std::move(errorText), {} };
    }
}

// Merges the decrypted content into the message of the context.
static void applyDecrypted(Context &context, E2ee::Decrypted &&decrypted)
{
    auto &d = MessageContextAccess::d(context);
    auto &message = d.message;

    // The affixes of XEP-0420 bind the content to the sender and the recipient. In MUCs, the
    // sender is the real JID resolved by ResolveSender and the recipient is the room. Otherwise
    // the recipient is the addressee, i.e. this account or for sent carbons the contact.
    if (!decrypted.sceFrom().isEmpty() && decrypted.sceFrom() != d.senderJid) {
        fail(context, DecryptionState::Failed, u"The sender of the encrypted content does not match the sender of the message"_s);
        return;
    }
    const auto expectedRecipient = message.type() == QXmppMessage::GroupChat ? d.conversationJid : QXmppUtils::jidToBareJid(message.to());
    if (!decrypted.sceTo().isEmpty() && decrypted.sceTo() != expectedRecipient) {
        fail(context, DecryptionState::Failed, u"The recipient of the encrypted content does not match the recipient of the message"_s);
        return;
    }

    // The public part was parsed with ScePublic, so it contains no sensitive elements apart from
    // the fallback body and publicExtensions(). Fallback markers of the public part refer to
    // the fallback body and must not be applied to the decrypted body.
    if (decrypted.content().isNull()) {
        d.withoutContent = true;
    }
    message.extensions().removeIf([](const std::any &item) {
        return !Xml::Registry::encryption(item).isNull();
    });
    message.setFallbackMarkers({});
    message.parseExtensions(decrypted.content(), SceSensitive);
    message.setE2eeMetadata(decrypted.metadata());

    d.e2eeMetadata = decrypted.metadata();
    d.decryptionState = DecryptionState::Decrypted;
}

IncomingMessages::IncomingMessages(QXmppClient *client, Hooks hooks, const QList<QXmppClientExtension *> *extensions)
    : m_client(client), m_hooks(hooks), m_extensions(extensions)
{
}

// Processes a message received directly from the stream.
void IncomingMessages::processElement(const QDomElement &element, QXmppE2eeExtension *legacyEncryption)
{
    const auto encryption = Xml::Registry::encryption(element);
    const bool hasProvider = !encryption.isNull() && E2ee::Providers(m_hooks).contains(encryption);
    const bool legacyEncrypted = legacyEncryption && legacyEncryption->isEncrypted(element);

    // Without a way to decrypt the message, it is parsed completely like before, so the
    // fallback body is the body.
    QXmppMessage message;
    message.parse(element, hasProvider || legacyEncrypted ? ScePublic : SceAll);

    process(Context(std::move(message), Origin::Stream), encryption);
}

// Processes a message unwrapped or injected by an extension.
void IncomingMessages::process(Context &&context)
{
    auto encryption = Private::encryption(context.message());
    process(std::move(context), encryption);
}

void IncomingMessages::process(Context &&context, QStringView encryption)
{
    if (!encryption.isNull() && !E2ee::Providers(m_hooks).contains(encryption)) {
        fail(context, DecryptionState::Unsupported);
        encryption = {};
    }

    // The queue is chosen before ResolveSender. Its handlers may set the conversation, but the
    // order is kept per JID of the unwrapped message (e.g. the room).
    const auto conversationJid = context.conversationJid();
    auto &queue = m_queues[conversationJid];
    queue.push_back(Pending { std::move(context), encryption });
    auto *pending = &queue.back();

    // Started immediately, not when the message is at the front of the queue, so handlers see
    // the state at the time the message was received (e.g. the occupants of a MUC).
    m_hooks.run<ResolveSender>(pending->context).then(m_client, [this, conversationJid, pending](HookResult &&hookResult) {
        pending->resolved = true;
        pending->dropped = hookResult == HookResult::Stop;
        // removed only after it has been resolved
        if (&m_queues.at(conversationJid).front() == pending) {
            runNext(conversationJid);
        }
    });
}

void IncomingMessages::runNext(const QString &conversationJid)
{
    auto &pending = m_queues.at(conversationJid).front();
    if (!pending.resolved) {
        // continued when it has been resolved
        return;
    }
    if (pending.dropped) {
        finishFront(conversationJid);
        return;
    }
    if (pending.encryption.isNull()) {
        finish(pending.context);
        finishFront(conversationJid);
        return;
    }

    decrypt(std::move(pending.context), pending.encryption).then(m_client, [this, conversationJid](Context &&context) {
        finish(context);
        finishFront(conversationJid);
    });
}

void IncomingMessages::finishFront(const QString &conversationJid)
{
    auto queue = m_queues.find(conversationJid);
    queue->second.pop_front();
    if (queue->second.empty()) {
        m_queues.erase(queue);
    } else {
        runNext(conversationJid);
    }
}

QXmppTask<Context> IncomingMessages::decrypt(Context context, QStringView encryption)
{
    auto provider = E2ee::Providers(m_hooks).find(encryption);
    if (!provider || !provider->decryptMessage) {
        fail(context, DecryptionState::Unsupported);
        co_return context;
    }

    E2ee::DecryptRequest request;
    request.setMessage(context.message());
    request.setSenderJid(context.senderJid());

    auto result = co_await provider->decryptMessage(std::move(request)).withContext(m_client);
    if (auto *decrypted = std::get_if<E2ee::Decrypted>(&result)) {
        if (decrypted->metadata().encryption() == NoEncryption) {
            // also for providers not setting it
            auto metadata = decrypted->metadata();
            metadata.setEncryptionNamespace(encryption.toString());
            decrypted->setMetadata(std::move(metadata));
        }
        applyDecrypted(context, std::move(*decrypted));
    } else if (std::holds_alternative<E2ee::NoEnvelope>(result)) {
        fail(context, DecryptionState::NoEnvelope);
    } else {
        auto &d = MessageContextAccess::d(context);
        d.decryptionState = DecryptionState::Failed;
        d.decryptionError = std::get<QXmppError>(std::move(result));
    }
    co_return context;
}

void IncomingMessages::finish(Context &context)
{
    if (MessageContextAccess::d(context).withoutContent) {
        return;
    }
    if (m_hooks.run<Received>(context) == HookResult::Stop) {
        return;
    }

    // extensions still implementing the old interface
    const auto extensions = *m_extensions;
    for (auto *extension : extensions) {
        if (auto *handler = dynamic_cast<QXmppMessageHandler *>(extension)) {
            if (handler->handleMessage(context.message())) {
                return;
            }
        }
    }

    if (context.origin() != Origin::Mam) {
        Q_EMIT m_client->messageReceived(context.message());
    }
}

}  // namespace QXmpp::Private

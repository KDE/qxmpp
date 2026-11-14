// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClientExtension.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeProvider.h"
#include "QXmppFallback.h"
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

// The provider decrypting received messages of the encryption.
static std::optional<E2ee::Provider> findProvider(Hooks hooks, QStringView encryption)
{
    auto provider = E2ee::Providers(std::move(hooks)).find(encryption);
    if (provider && provider->isEnabled && !provider->isEnabled()) {
        return {};
    }
    return provider;
}

// The encryption of the provider with the highest priority, the first one if there is none.
static QStringView preferredEncryption(const QList<QStringView> &encryptions, Hooks hooks)
{
    QStringView preferred = encryptions.value(0);
    std::optional<int> priority;
    for (auto encryption : encryptions) {
        if (auto provider = findProvider(hooks, encryption); provider && (!priority || provider->priority > *priority)) {
            preferred = encryption;
            priority = provider->priority;
        }
    }
    return preferred;
}

static QStringView encryption(const QXmppMessage &message, Hooks hooks)
{
    QList<QStringView> encryptions;
    for (const auto &item : message.extensions()) {
        if (auto encryption = Xml::Registry::encryption(item); !encryption.isNull()) {
            encryptions.append(encryption);
        }
    }
    return preferredEncryption(encryptions, std::move(hooks));
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
static void applyDecrypted(Context &context, E2ee::Decrypted &&decrypted, const E2ee::Provider &provider)
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

    if (decrypted.content().isNull()) {
        d.withoutContent = true;
    }
    message.extensions().removeIf([](const std::any &item) {
        return !Xml::Registry::encryption(item).isNull();
    });
    if (provider.contentEncryption == E2ee::ContentEncryption::BodyOnly) {
        // The other elements are sent in the clear and have been parsed completely. Only the
        // body is replaced, the one of the public part is the fallback body. Fallback markers
        // of other elements (e.g. replies) refer to the decrypted body.
        if (message.e2eeFallbackBody().isEmpty()) {
            message.setE2eeFallbackBody(message.body());
        }
        auto markers = message.fallbackMarkers();
        markers.removeIf([&](const QXmppFallback &marker) { return marker.forNamespace() == provider.encryption; });
        message.setFallbackMarkers(markers);
    } else {
        // The public part was parsed with ScePublic, so it contains no sensitive elements apart
        // from the fallback body and publicExtensions(). Fallback markers of the public part
        // refer to the fallback body and must not be applied to the decrypted body.
        message.setFallbackMarkers({});
    }
    message.parseExtensions(decrypted.content(), SceSensitive);
    message.setE2eeMetadata(decrypted.metadata());

    d.e2eeMetadata = decrypted.metadata();
    d.decryptionState = DecryptionState::Decrypted;
}

IncomingMessages::IncomingMessages(QXmppClient *client, Hooks hooks, const QList<QXmppClientExtension *> *extensions)
    : m_client(client), m_hooks(hooks), m_extensions(extensions)
{
}

// Parses a received message. Without a way to decrypt it, it is parsed completely like before,
// so the fallback body is the body.
IncomingMessages::ParsedMessage IncomingMessages::parse(const QDomElement &element, QXmppE2eeExtension *legacyEncryption) const
{
    ParsedMessage parsed;
    parsed.encryption = preferredEncryption(Xml::Registry::encryptions(element), m_hooks);
    const auto provider = parsed.encryption.isNull() ? std::nullopt : findProvider(m_hooks, parsed.encryption);
    if (provider && provider->contentEncryption == E2ee::ContentEncryption::BodyOnly) {
        // only the body is encrypted, the public one is its fallback
        parsed.message.parse(element, SceAll);
        parsed.message.setE2eeFallbackBody(parsed.message.body());
        parsed.message.setBody({});
        return parsed;
    }
    parsed.legacyEncrypted = !provider && legacyEncryption && legacyEncryption->isEncrypted(element);
    parsed.message.parse(element, provider || parsed.legacyEncrypted ? ScePublic : SceAll);
    return parsed;
}

// Processes a message received directly from the stream.
void IncomingMessages::processElement(const QDomElement &element, QXmppE2eeExtension *legacyEncryption)
{
    auto parsed = parse(element, legacyEncryption);
    Context context(std::move(parsed.message), Origin::Stream);
    // Stanzas without 'from' are sent by the server on behalf of the account (RFC 6120 §8.1.2.1).
    if (context.senderJid().isEmpty()) {
        context.setSenderJid(m_client->configuration().jidBare());
    }
    process(std::move(context), parsed.encryption, {});
}

// Processes a message unwrapped or injected by an extension.
void IncomingMessages::process(Context &&context)
{
    auto encryption = Private::encryption(context.message(), m_hooks);
    process(std::move(context), encryption, {});
}

QXmppTask<IncomingMessages::CollectResult> IncomingMessages::collect(Context &&context, QStringView encryption)
{
    QXmppPromise<CollectResult> promise;
    auto task = promise.task();
    process(std::move(context), encryption, std::move(promise));
    return task;
}

void IncomingMessages::process(Context &&context, QStringView encryption, std::optional<QXmppPromise<CollectResult>> result)
{
    if (!encryption.isNull() && !findProvider(m_hooks, encryption)) {
        fail(context, DecryptionState::Unsupported);
        encryption = {};
    }

    // The queue is chosen before ResolveSender. Its handlers may set the conversation, but the
    // order is kept per JID of the unwrapped message (e.g. the room).
    const auto conversationJid = context.conversationJid();
    auto &queue = m_queues[conversationJid];
    queue.push_back(Pending { std::move(context), encryption, std::move(result) });
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
        if (pending.result) {
            pending.result->finish(CollectResult());
        }
        finishFront(conversationJid);
        return;
    }
    if (pending.encryption.isNull()) {
        finish(std::move(pending.context), pending.result);
        finishFront(conversationJid);
        return;
    }

    decrypt(std::move(pending.context), pending.encryption).then(m_client, [this, conversationJid](Context &&context) {
        finish(std::move(context), m_queues.at(conversationJid).front().result);
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
    auto provider = findProvider(m_hooks, encryption);
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
        applyDecrypted(context, std::move(*decrypted), *provider);
    } else if (std::holds_alternative<E2ee::NoEnvelope>(result)) {
        fail(context, DecryptionState::NoEnvelope);
    } else {
        auto &d = MessageContextAccess::d(context);
        d.decryptionState = DecryptionState::Failed;
        d.decryptionError = std::get<QXmppError>(std::move(result));
    }
    co_return context;
}

void IncomingMessages::finish(Context &&context, std::optional<QXmppPromise<CollectResult>> &result)
{
    const bool delivered = !MessageContextAccess::d(context).withoutContent &&
        m_hooks.run<Received>(context) == HookResult::Continue;

    if (result) {
        result->finish(delivered ? CollectResult(std::move(context)) : CollectResult());
        return;
    }
    if (!delivered) {
        return;
    }

#if QXMPP_DEPRECATED_SINCE(1, 18)
    // extensions still implementing the deprecated interface
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const auto extensions = *m_extensions;
    for (auto *extension : extensions) {
        if (auto *handler = dynamic_cast<QXmppMessageHandler *>(extension)) {
            if (handler->handleMessage(context.message())) {
                return;
            }
        }
    }
    QT_WARNING_POP
#endif

    if (context.origin() != Origin::Mam) {
        Q_EMIT m_client->messageReceived(context.message());
    }
}

}  // namespace QXmpp::Private

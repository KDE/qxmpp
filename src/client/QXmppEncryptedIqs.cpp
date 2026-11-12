// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppE2eeExtension.h"
#include "QXmppE2eeProvider.h"
#include "QXmppEncryptedIqs_p.h"
#include "QXmppHooks_p.h"
#include "QXmppIqHandling.h"
#include "QXmppLogger.h"
#include "QXmppOutgoingClient_p.h"
#include "QXmppPacket_p.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppUtils.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"

#include <QDomDocument>
#include <QXmlStreamWriter>

using namespace QXmpp;

namespace QXmpp::Private {

struct DecryptedIq {
    QDomElement iq;
    QXmppE2eeMetadata metadata;
};

// The IQ with its child elements replaced by \a children.
static QByteArray toXml(const OutgoingIq &iq, const Xml::Extensions &children)
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writeIqStart(&writer, iq.type, iq.id, iq.from, iq.to, iq.lang);
    children.toXml(&writer);
    writeIqEnd(&writer);
    return xml;
}

// Maps an IQ response like QXmppClient::sendIq() does.
static EncryptedIqs::IqResult mapResponse(QDomElement &&response)
{
    if (response.attribute(u"type"_s) == u"error") {
        return iqResponseError(response);
    }
    return std::move(response);
}

static QXmppError notEncryptedError()
{
    return QXmppError { u"No end-to-end encryption has been selected for an IQ that must be encrypted"_s, SendError::EncryptionError };
}

static Iq::Type iqType(QXmppIq::Type type)
{
    switch (type) {
    case QXmppIq::Get:
        return Iq::Type::Get;
    case QXmppIq::Set:
        return Iq::Type::Set;
    case QXmppIq::Result:
        return Iq::Type::Result;
    case QXmppIq::Error:
        return Iq::Type::Error;
    }
    Q_UNREACHABLE();
}

EncryptedIqs::EncryptedIqs(QXmppClient *client, QXmppClientPrivate *clientPrivate, Hooks hooks)
    : m_client(client), m_clientPrivate(clientPrivate), m_hooks(hooks)
{
}

OutgoingIq EncryptedIqs::serialize(const QXmppIq &iq)
{
    QByteArray payload;
    QXmlStreamWriter writer(&payload);
    iq.toXmlElementFromChild(&writer);
    return OutgoingIq { iqType(iq.type()), iq.id(), iq.from(), iq.to(), {}, makeRawXmlWriter(std::move(payload)), iq.errorOptional() };
}

QXmppIq EncryptedIqs::deserialize(const OutgoingIq &iq)
{
    // the wrapper sets the namespace of elements without own namespace, e.g. <error/>
    QDomDocument document;
    const auto xml = "<wrapper xmlns='jabber:client'>" + iq.toXml() + "</wrapper>";
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    document.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing);
#else
    document.setContent(xml, true);
#endif
    QXmppIq result;
    result.parse(document.documentElement().firstChildElement());
    return result;
}

QXmppTask<EncryptedIqs::Selection> EncryptedIqs::select(const OutgoingIq &iq, QString encryption, bool required)
{
    // an IQ without 'to' is addressed to the own account (RFC 6120, section 10.3.3)
    Iq::EncryptionSelection selection(iq.type, iq.to.isEmpty() ? m_client->configuration().jidBare() : iq.to, required, std::move(encryption));
    co_await m_hooks.run<Iq::SelectEncryption>(selection).withContext(m_client);

    if (selection.encryption().isEmpty()) {
        co_return QString();
    }
    auto provider = E2ee::Providers(m_hooks).find(selection.encryption());
    if (!provider || !provider->encryptIq) {
        co_return QXmppError { u"No end-to-end encryption provider for IQs encrypted with "_s + selection.encryption(), SendError::EncryptionError };
    }
    co_return selection.encryption();
}

QXmppTask<EncryptedIqs::Encryption> EncryptedIqs::encrypt(const OutgoingIq &iq, QString encryption, QXmppSendStanzaParams params)
{
    auto provider = E2ee::Providers(m_hooks).find(encryption);
    if (!provider || !provider->encryptIq) {
        co_return QXmppError { u"No end-to-end encryption provider for IQs encrypted with "_s + encryption, SendError::EncryptionError };
    }

    // an IQ without 'to' is addressed to the own account (RFC 6120, section 10.3.3)
    auto to = iq.to.isEmpty() ? m_client->configuration().jidBare() : iq.to;
    auto recipients = params.encryptionJids();
    if (recipients.isEmpty()) {
        recipients.append(QXmppUtils::jidToBareJid(to));
    }

    E2ee::EncryptIqRequest request;
    request.setTo(std::move(to));
    request.setPayload(iq.payload);
    request.setError(iq.error);
    request.setRecipientJids(std::move(recipients));
    request.setParams(std::move(params));

    auto result = co_await provider->encryptIq(std::move(request)).withContext(m_client);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    co_return toXml(iq, std::get<E2ee::Encrypted>(result).elements());
}

// Decrypts an IQ, the decrypted content replaces its child elements.
static QXmppTask<std::variant<DecryptedIq, QXmppError>> decryptIq(QXmppClient *client, const Hooks &hooks, QDomElement iq, QStringView encryption)
{
    auto provider = E2ee::Providers(hooks).find(encryption);
    if (!provider || !provider->decryptIq) {
        co_return QXmppError { u"No end-to-end encryption provider for IQs encrypted with "_s + encryption, SendError::EncryptionError };
    }

    const auto ownJid = client->configuration().jidBare();
    // IQs from the server on behalf of the account have no 'from'
    const auto senderJid = iq.hasAttribute(u"from"_s) ? QXmppUtils::jidToBareJid(iq.attribute(u"from"_s)) : ownJid;

    E2ee::DecryptIqRequest request;
    request.setIq(iq);
    request.setSenderJid(senderJid);
    auto result = co_await provider->decryptIq(std::move(request)).withContext(client);

    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    if (std::holds_alternative<E2ee::NoEnvelope>(result)) {
        co_return QXmppError { u"The IQ is not encrypted for this device"_s, SendError::EncryptionError };
    }

    auto &decrypted = std::get<E2ee::Decrypted>(result);
    // the affixes of XEP-0420 bind the content to the sender and the recipient
    if (!decrypted.sceFrom().isEmpty() && decrypted.sceFrom() != senderJid) {
        co_return QXmppError { u"The sender of the encrypted content does not match the sender of the IQ"_s, SendError::EncryptionError };
    }
    if (!decrypted.sceTo().isEmpty() && decrypted.sceTo() != ownJid) {
        co_return QXmppError { u"The recipient of the encrypted content does not match the recipient of the IQ"_s, SendError::EncryptionError };
    }

    auto decryptedIq = iq.cloneNode(false).toElement();
    for (auto child = decrypted.content().firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        decryptedIq.appendChild(decryptedIq.ownerDocument().importNode(child, true));
    }

    auto metadata = decrypted.metadata();
    if (metadata.encryption() == NoEncryption) {
        // also for providers not setting it
        metadata.setEncryptionNamespace(encryption.toString());
    }
    co_return DecryptedIq { decryptedIq, metadata };
}

QXmppTask<EncryptedIqs::IqResult> EncryptedIqs::sendRequest(OutgoingIq iq, QXmppSendStanzaParams params)
{
    auto selection = co_await select(iq, {}, params.isEncryptionRequired());
    if (auto *error = std::get_if<QXmppError>(&selection)) {
        co_return std::move(*error);
    }

    auto *stream = m_clientPrivate->stream;
    const auto to = iq.to.isEmpty() ? m_client->configuration().jidBare() : iq.to;

    const auto &encryption = std::get<QString>(selection);
    if (encryption.isEmpty()) {
        if (m_clientPrivate->encryptionExtension) {
            // deprecated encryption extension
            co_return co_await m_clientPrivate->sendSensitiveIq(deserialize(iq), params);
        }
        if (params.isEncryptionRequired()) {
            co_return notEncryptedError();
        }
        co_return co_await stream->iqManager().sendIq(QXmppPacket(iq.toXml(), true), iq.id, to).withContext(m_client);
    }

    auto encrypted = co_await encrypt(iq, encryption, params);
    if (auto *error = std::get_if<QXmppError>(&encrypted)) {
        co_return std::move(*error);
    }

    // error responses are encrypted too, so they are mapped after the decryption
    auto response = co_await stream->iqManager().sendIq(QXmppPacket(std::get<QByteArray>(encrypted), true), iq.id, to, true).withContext(m_client);
    if (auto *error = std::get_if<QXmppError>(&response)) {
        co_return std::move(*error);
    }

    auto responseIq = std::get<QDomElement>(std::move(response));
    if (const auto responseEncryption = Xml::Registry::encryption(responseIq); !responseEncryption.isNull()) {
        auto decrypted = co_await decryptIq(m_client, m_hooks, responseIq, responseEncryption);
        if (auto *error = std::get_if<QXmppError>(&decrypted)) {
            co_return std::move(*error);
        }
        responseIq = std::get<DecryptedIq>(std::move(decrypted)).iq;
    }
    co_return mapResponse(std::move(responseIq));
}

QXmppTask<SendResult> EncryptedIqs::send(OutgoingIq iq, QXmppSendStanzaParams params, QString encryption)
{
    auto selection = co_await select(iq, std::move(encryption), params.isEncryptionRequired());
    if (auto *error = std::get_if<QXmppError>(&selection)) {
        co_return std::move(*error);
    }

    auto &streamAckManager = m_clientPrivate->stream->streamAckManager();
    const auto &selected = std::get<QString>(selection);
    if (selected.isEmpty()) {
        if (auto *legacyEncryption = m_clientPrivate->encryptionExtension) {
            // deprecated encryption extension
            co_return co_await m_clientPrivate->sendEncryptedIq(legacyEncryption->encryptIq(deserialize(iq), params));
        }
        if (params.isEncryptionRequired()) {
            co_return notEncryptedError();
        }
        co_return co_await streamAckManager.send(QXmppPacket(iq.toXml(), true)).withContext(m_client);
    }

    auto encrypted = co_await encrypt(iq, selected, params);
    if (auto *error = std::get_if<QXmppError>(&encrypted)) {
        co_return std::move(*error);
    }
    co_return co_await streamAckManager.send(QXmppPacket(std::get<QByteArray>(encrypted), true)).withContext(m_client);
}

QXmppTask<SendResult> EncryptedIqs::sendResponse(OutgoingIq iq, const std::optional<QXmppE2eeMetadata> &requestMetadata)
{
    if (!requestMetadata) {
        return m_clientPrivate->stream->streamAckManager().send(QXmppPacket(iq.toXml(), true));
    }

    // the encryption of the request is the default of SelectEncryption
    auto encryption = requestMetadata->encryptionNamespace();
    if (auto provider = E2ee::Providers(m_hooks).find(encryption); !provider || !provider->encryptIq) {
        encryption.clear();
    }
    return send(std::move(iq), {}, std::move(encryption));
}

bool EncryptedIqs::processIncoming(const QDomElement &element)
{
    const auto type = element.attribute(u"type"_s);
    if (element.tagName() != u"iq" || (type != u"get" && type != u"set")) {
        return false;
    }
    const auto encryption = Xml::Registry::encryption(element);
    if (encryption.isNull()) {
        return false;
    }
    auto provider = E2ee::Providers(m_hooks).find(encryption);
    if (!provider || !provider->decryptIq) {
        return false;
    }
    processIncomingAsync(element, encryption);
    return true;
}

QXmppTask<void> EncryptedIqs::processIncomingAsync(QDomElement element, QStringView encryption)
{
    auto result = co_await decryptIq(m_client, m_hooks, element, encryption);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        Q_EMIT m_client->logMessage(QXmppLogger::WarningMessage, u"Could not decrypt incoming IQ: "_s + error->description);

        // The requester would wait for a response otherwise. The error is sent unencrypted: the
        // encryption may not work with the requester and the error contains nothing sensitive.
        using Error = QXmppStanza::Error;
        QXmppIq response(QXmppIq::Error);
        response.setTo(element.attribute(u"from"_s));
        response.setId(element.attribute(u"id"_s));
        response.setError(Error(Error::Modify, Error::NotAcceptable, u"The end-to-end encrypted request could not be decrypted."_s));
        m_client->send(std::move(response));
        co_return;
    }
    auto &decrypted = std::get<DecryptedIq>(result);
    m_client->injectIq(decrypted.iq, decrypted.metadata);
}

}  // namespace QXmpp::Private

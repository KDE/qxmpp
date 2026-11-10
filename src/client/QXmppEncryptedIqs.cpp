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
#include "QXmppLogger.h"
#include "QXmppMessages.h"
#include "QXmppOutgoingClient_p.h"
#include "QXmppPacket_p.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppUtils.h"
#include "QXmppXmlElement.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomDocument>
#include <QXmlStreamWriter>

using namespace QXmpp;
using namespace QXmpp::Messages;

namespace QXmpp::Private {

struct DecryptedIq {
    QDomElement iq;
    QXmppE2eeMetadata metadata;
};

static QByteArray toXml(const QDomElement &element)
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    XmlWriter w(&writer);
    Xml::Element::fromDom(element).toXml(w);
    return xml;
}

// The IQ with its child elements replaced by \a children.
static QByteArray toXml(const QDomElement &iq, const Xml::Extensions &children)
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(u"iq"_s);
    const auto attributes = iq.attributes();
    for (int i = 0; i < attributes.size(); i++) {
        const auto attribute = attributes.item(i).toAttr();
        writer.writeAttribute(attribute.nodeName(), attribute.value());
    }
    children.toXml(&writer);
    writer.writeEndElement();
    return xml;
}

static QString recipient(const QDomElement &iq)
{
    return QXmppUtils::jidToBareJid(iq.attribute(u"to"_s));
}

// Maps an IQ response like QXmppClient::sendIq() does.
static EncryptedIqs::IqResult mapResponse(QDomElement &&response)
{
    if (response.attribute(u"type"_s) == u"error") {
        QXmppIq iq;
        iq.parse(response);
        if (auto error = iq.errorOptional()) {
            return QXmppError { error->text(), *error };
        }
        using Error = QXmppStanza::Error;
        return QXmppError { u"IQ error"_s, Error(Error::Cancel, Error::UndefinedCondition) };
    }
    return std::move(response);
}

EncryptedIqs::EncryptedIqs(QXmppClient *client, QXmppClientPrivate *clientPrivate, Hooks hooks)
    : m_client(client), m_clientPrivate(clientPrivate), m_hooks(hooks)
{
}

QDomElement EncryptedIqs::serialize(const QXmppIq &iq)
{
    // the wrapper sets the namespace of elements without own namespace, e.g. <error/>
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(u"wrapper"_s);
    writer.writeDefaultNamespace(ns_client.toString());
    iq.toXml(&writer);
    writer.writeEndElement();

    QDomDocument document;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    document.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing);
#else
    document.setContent(xml, true);
#endif
    return document.documentElement().firstChildElement();
}

QXmppTask<EncryptedIqs::Selection> EncryptedIqs::select(const QXmppIq &header, QString encryption)
{
    EncryptionSelection selection(header, true, std::move(encryption));
    co_await m_hooks.run<SelectEncryption>(selection).withContext(m_client);

    if (selection.encryption().isEmpty()) {
        co_return QString();
    }
    auto provider = E2ee::Providers(m_hooks).find(selection.encryption());
    if (!provider || !provider->encryptIq) {
        co_return QXmppError { u"No end-to-end encryption provider for IQs encrypted with "_s + selection.encryption(), SendError::EncryptionError };
    }
    co_return selection.encryption();
}

QXmppTask<EncryptedIqs::Encryption> EncryptedIqs::encrypt(QDomElement iq, QString encryption, QXmppSendStanzaParams params)
{
    auto provider = E2ee::Providers(m_hooks).find(encryption);
    if (!provider || !provider->encryptIq) {
        co_return QXmppError { u"No end-to-end encryption provider for IQs encrypted with "_s + encryption, SendError::EncryptionError };
    }

    auto recipients = params.encryptionJids();
    if (recipients.isEmpty()) {
        recipients.append(recipient(iq));
    }

    E2ee::EncryptIqRequest request;
    request.setIq(iq);
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

QXmppTask<EncryptedIqs::IqResult> EncryptedIqs::sendRequest(QXmppIq header, QDomElement iq, QXmppSendStanzaParams params)
{
    auto selection = co_await select(header, {});
    if (auto *error = std::get_if<QXmppError>(&selection)) {
        co_return std::move(*error);
    }

    auto *stream = m_clientPrivate->stream;
    const auto id = iq.attribute(u"id"_s);
    const auto to = iq.attribute(u"to"_s).isEmpty() ? m_client->configuration().jidBare() : iq.attribute(u"to"_s);

    const auto &encryption = std::get<QString>(selection);
    if (encryption.isEmpty()) {
        if (m_clientPrivate->encryptionExtension) {
            // like before
            co_return co_await m_clientPrivate->sendSensitiveIq(std::move(header), params);
        }
        co_return co_await stream->iqManager().sendIq(QXmppPacket(toXml(iq), true), id, to).withContext(m_client);
    }

    auto encrypted = co_await encrypt(iq, encryption, params);
    if (auto *error = std::get_if<QXmppError>(&encrypted)) {
        co_return std::move(*error);
    }

    // error responses are encrypted too, so they are mapped after the decryption
    auto response = co_await stream->iqManager().sendIq(QXmppPacket(std::get<QByteArray>(encrypted), true), id, to, true).withContext(m_client);
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

QXmppTask<SendResult> EncryptedIqs::send(QXmppIq header, QDomElement iq, QXmppSendStanzaParams params, QString encryption)
{
    auto selection = co_await select(header, std::move(encryption));
    if (auto *error = std::get_if<QXmppError>(&selection)) {
        co_return std::move(*error);
    }

    auto &streamAckManager = m_clientPrivate->stream->streamAckManager();
    const auto &selected = std::get<QString>(selection);
    if (selected.isEmpty()) {
        if (auto *legacyEncryption = m_clientPrivate->encryptionExtension) {
            // like before
            co_return co_await m_clientPrivate->sendEncryptedIq(legacyEncryption->encryptIq(std::move(header), params));
        }
        co_return co_await streamAckManager.send(QXmppPacket(toXml(iq), true)).withContext(m_client);
    }

    auto encrypted = co_await encrypt(iq, selected, params);
    if (auto *error = std::get_if<QXmppError>(&encrypted)) {
        co_return std::move(*error);
    }
    co_return co_await streamAckManager.send(QXmppPacket(std::get<QByteArray>(encrypted), true)).withContext(m_client);
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

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPENCRYPTEDIQS_P_H
#define QXMPPENCRYPTEDIQS_P_H

#include "QXmppError.h"
#include "QXmppHooks.h"
#include "QXmppIq.h"
#include "QXmppIqStanza.h"
#include "QXmppSendResult.h"
#include "QXmppSendStanzaParams.h"
#include "QXmppTask.h"

#include <variant>

#include <QDomElement>

class QXmppClient;
class QXmppClientPrivate;

namespace QXmpp::Private {

// End-to-end encryption of IQs via the e2ee providers (QXmpp::E2ee::Provider::encryptIq() and
// decryptIq()). The encryption is selected by the QXmpp::Iq::SelectEncryption hook, without selection the
// encryption extension is used like before.
class EncryptedIqs
{
public:
    using IqResult = std::variant<QDomElement, QXmppError>;

    EncryptedIqs(QXmppClient *client, QXmppClientPrivate *clientPrivate, Hooks hooks);

    // Serializes the IQ including its payload, so it can be processed after the object passed
    // by the user is gone.
    static OutgoingIq serialize(const QXmppIq &iq);
    // Creates an IQ containing the payload as extensions for the deprecated encryption extension.
    // Unlike the IQ passed by the user, it can be copied without losing the payload of subclasses.
    static QXmppIq deserialize(const OutgoingIq &iq);

    // Sends a request and decrypts the response.
    QXmppTask<IqResult> sendRequest(OutgoingIq iq, QXmppSendStanzaParams params);
    // Sends an IQ without waiting for a response, e.g. a response. \a encryption is the default
    // selection, e.g. the encryption of the request.
    QXmppTask<SendResult> send(OutgoingIq iq, QXmppSendStanzaParams params, QString encryption);
    // Sends the response to a request, encrypted like the request if \a requestMetadata is set.
    QXmppTask<SendResult> sendResponse(OutgoingIq iq, const std::optional<QXmppE2eeMetadata> &requestMetadata);

    // Decrypts an incoming encrypted request and processes it like an injected IQ. Returns false
    // if there is no provider for it.
    bool processIncoming(const QDomElement &element);

private:
    using Selection = std::variant<QString, QXmppError>;
    using Encryption = std::variant<QByteArray, QXmppError>;

    QXmppTask<Selection> select(const OutgoingIq &iq, QString encryption, bool required);
    QXmppTask<Encryption> encrypt(const OutgoingIq &iq, QString encryption, QXmppSendStanzaParams params);
    QXmppTask<IqResult> decrypt(QDomElement iq, QStringView encryption);
    QXmppTask<void> processIncomingAsync(QDomElement element, QStringView encryption);

    QXmppClient *m_client;
    QXmppClientPrivate *m_clientPrivate;
    Hooks m_hooks;
};

}  // namespace QXmpp::Private

#endif  // QXMPPENCRYPTEDIQS_P_H

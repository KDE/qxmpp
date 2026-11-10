// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGECONTEXT_P_H
#define QXMPPMESSAGECONTEXT_P_H

#include "QXmppMessage.h"
#include "QXmppMessageContext.h"

#include <unordered_map>

namespace QXmpp::Messages {

class ContextPrivate
{
public:
    QXmppMessage message;
    Origin origin;
    QString senderJid;
    QString conversationJid;
    DecryptionState decryptionState = DecryptionState::NotEncrypted;
    std::optional<QXmppE2eeMetadata> e2eeMetadata;
    std::optional<QXmppError> decryptionError;
    std::unordered_map<std::type_index, std::any> attachments;
    // decrypted without content, e.g. an OMEMO key exchange, not processed further
    bool withoutContent = false;
};

}  // namespace QXmpp::Messages

namespace QXmpp::Private {

// Used by the pipeline to set the results of the decryption.
struct MessageContextAccess {
    static Messages::ContextPrivate &d(Messages::Context &context) { return *context.d; }
};

}  // namespace QXmpp::Private

#endif  // QXMPPMESSAGECONTEXT_P_H

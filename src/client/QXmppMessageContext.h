// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGECONTEXT_H
#define QXMPPMESSAGECONTEXT_H

#include "QXmppE2eeMetadata.h"
#include "QXmppError.h"
#include "QXmppMessages.h"

#include <any>
#include <memory>
#include <optional>
#include <typeindex>

class QXmppMessage;

namespace QXmpp::Private {
struct MessageContextAccess;
}

namespace QXmpp::Messages {

class ContextPrivate;

class QXMPP_EXPORT Context
{
public:
    Context(QXmppMessage &&message, Origin origin);
    Context(Context &&) noexcept;
    Context &operator=(Context &&) noexcept;
    ~Context();

    QXmppMessage &message();
    const QXmppMessage &message() const;

    Origin origin() const;

    // Bare JID of the real sender. Defaults to the bare 'from' (own JID for CarbonSent).
    // Set by the ResolveSender hook, e.g. by MUC from the occupant ID. Decryption uses it.
    const QString &senderJid() const;
    void setSenderJid(const QString &jid);

    // Bare JID of the conversation (contact or room). Messages of one conversation are
    // processed in order, also across the async decryption.
    const QString &conversationJid() const;
    void setConversationJid(const QString &jid);

    // Set by the pipeline.
    DecryptionState decryptionState() const;
    const std::optional<QXmppE2eeMetadata> &e2eeMetadata() const;
    const std::optional<QXmppError> &decryptionError() const;

    // Typed data of extensions, e.g. MUC attaches its occupant info, MAM its archive ID.
    template<typename T>
    T *attachment()
    {
        return std::any_cast<T>(attachmentInternal(std::type_index(typeid(T))));
    }
    template<typename T>
    const T *attachment() const
    {
        return std::any_cast<T>(attachmentInternal(std::type_index(typeid(T))));
    }
    template<typename T>
    T &attach(T value)
    {
        return *std::any_cast<T>(attachInternal(std::type_index(typeid(T)), std::any(std::move(value))));
    }

private:
    std::any *attachmentInternal(std::type_index type);
    const std::any *attachmentInternal(std::type_index type) const;
    std::any *attachInternal(std::type_index type, std::any &&value);

    friend struct QXmpp::Private::MessageContextAccess;
    std::unique_ptr<ContextPrivate> d;
};

}  // namespace QXmpp::Messages

#endif  // QXMPPMESSAGECONTEXT_H

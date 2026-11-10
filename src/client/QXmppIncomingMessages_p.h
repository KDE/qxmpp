// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPINCOMINGMESSAGES_P_H
#define QXMPPINCOMINGMESSAGES_P_H

#include "QXmppGlobal.h"
#include "QXmppHooks.h"
#include "QXmppMessageContext.h"
#include "QXmppTask.h"

#include <deque>
#include <unordered_map>

class QDomElement;
class QXmppClient;
class QXmppClientExtension;
class QXmppE2eeExtension;

namespace QXmpp::Private {

// Pipeline of incoming messages, see QXmpp::Messages.
class IncomingMessages
{
public:
    IncomingMessages(QXmppClient *client, Hooks hooks, const QList<QXmppClientExtension *> *extensions);

    void processElement(const QDomElement &element, QXmppE2eeExtension *legacyEncryption);
    void process(Messages::Context &&context);

private:
    struct Pending {
        Messages::Context context;
        // namespace of the encryption, null if not encrypted
        QStringView encryption;
        // ResolveSender has finished
        bool resolved = false;
        // ResolveSender has dropped the message
        bool dropped = false;
    };

    void process(Messages::Context &&context, QStringView encryption);
    void runNext(const QString &conversationJid);
    void finishFront(const QString &conversationJid);
    QXmppTask<Messages::Context> decrypt(Messages::Context context, QStringView encryption);
    void finish(Messages::Context &context);

    QXmppClient *m_client;
    Hooks m_hooks;
    const QList<QXmppClientExtension *> *m_extensions;
    // Messages of a conversation in the order of their arrival, waiting for ResolveSender or the
    // processing of an earlier message. References to the elements stay valid until they are
    // removed from the front.
    std::unordered_map<QString, std::deque<Pending>> m_queues;
};

}  // namespace QXmpp::Private

#endif  // QXMPPINCOMINGMESSAGES_P_H

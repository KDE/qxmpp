// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPOUTGOINGMESSAGES_P_H
#define QXMPPOUTGOINGMESSAGES_P_H

#include "QXmppHooks.h"
#include "QXmppMessage.h"
#include "QXmppPromise.h"
#include "QXmppSendResult.h"
#include "QXmppSendStanzaParams.h"
#include "QXmppTask.h"

#include <deque>
#include <unordered_map>

class QXmppClient;
class QXmppClientPrivate;
class QXmppStanza;

namespace QXmpp::Private {

// Pipeline of outgoing messages, see QXmpp::Messages.
class OutgoingMessages
{
public:
    enum class Mode {
        // QXmppClient::send()
        Default,
        // QXmppClient::sendSensitive() and reply(): falls back to the encryption extension
        Sensitive,
    };

    OutgoingMessages(QXmppClient *client, QXmppClientPrivate *clientPrivate, Hooks hooks);

    // Whether the stanza is sent via the pipeline. Subclasses of QXmppMessage are not, as they
    // can not be copied without losing their content.
    static bool isPipelineStanza(const QXmppStanza &stanza);

    QXmppTask<SendResult> send(QXmppMessage &&message, QXmppSendStanzaParams params, Mode mode, QString encryption = {});

private:
    struct Pending {
        QXmppMessage message;
        QXmppSendStanzaParams params;
        Mode mode;
        // preset encryption, e.g. of the message replied to
        QString encryption;
        QXmppPromise<SendResult> result;
    };

    QXmppTask<void> processFront(QString conversationJid);
    QXmppTask<SendResult> sendEncrypted(QXmppMessage &&message, const QString &encryption, QXmppSendStanzaParams params);
    void finishFront(const QString &conversationJid, QXmppTask<SendResult> &&sendTask);
    void finishFront(const QString &conversationJid, SendResult &&result);

    QXmppClient *m_client;
    QXmppClientPrivate *m_clientPrivate;
    Hooks m_hooks;
    // messages of a conversation waiting for an earlier message
    std::unordered_map<QString, std::deque<Pending>> m_queues;
};

}  // namespace QXmpp::Private

#endif  // QXMPPOUTGOINGMESSAGES_P_H

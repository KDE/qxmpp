// SPDX-FileCopyrightText: 2016 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMamManager.h"

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppE2eeExtension.h"
#include "QXmppIncomingMessages_p.h"
#include "QXmppMamIq.h"
#include "QXmppMessage.h"
#include "QXmppMessageContext.h"
#include "QXmppMessages.h"
#include "QXmppPromise.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "StringLiterals.h"

#include <unordered_map>

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;
using namespace QXmpp::Messages;

template<typename T>
auto sum(const T &c)
{
    return std::accumulate(c.begin(), c.end(), 0);
}

struct MamMessage {
    QDomElement element;
    std::optional<QDateTime> delay;
    // ID in the archive
    QString id;
};

enum EncryptedType { Unencrypted,
                     Encrypted };

QXmppMessage parseMamMessage(const MamMessage &mamMessage, EncryptedType encrypted)
{
    QXmppMessage m;
    m.parse(mamMessage.element, encrypted == Encrypted ? ScePublic : SceAll);
    if (mamMessage.delay) {
        m.setStamp(*mamMessage.delay);
    }
    return m;
}

std::optional<std::tuple<MamMessage, QString>> parseMamMessageResult(const QDomElement &messageEl)
{
    auto resultElement = firstChildElement(messageEl, u"result", ns_mam);
    if (resultElement.isNull()) {
        return {};
    }

    auto forwardedElement = firstChildElement(resultElement, u"forwarded", ns_forwarding);
    if (forwardedElement.isNull()) {
        return {};
    }

    auto queryId = resultElement.attribute(u"queryid"_s);

    auto messageElement = firstChildElement(forwardedElement, u"message", ns_client);
    if (messageElement.isNull()) {
        return {};
    }

    auto parseDelay = [](const auto &forwardedEl) -> std::optional<QDateTime> {
        auto delayEl = firstChildElement(forwardedEl, u"delay", ns_delayed_delivery);
        if (!delayEl.isNull()) {
            return QXmppUtils::datetimeFromString(delayEl.attribute(u"stamp"_s));
        }
        return {};
    };

    return { { MamMessage { messageElement, parseDelay(forwardedElement), resultElement.attribute(u"id"_s) }, queryId } };
}

// Real JID of the sender reported by a MUC archive (XEP-0313). Only used if there is exactly one,
// as a sender could add such an element to its message.
static QString occupantJid(const QDomElement &messageElement)
{
    QString jid;
    int count = 0;
    for (const auto &x : iterChildElements(messageElement, u"x", ns_muc_user)) {
        for (const auto &item : iterChildElements(x, u"item")) {
            if (auto itemJid = item.attribute(u"jid"_s); !itemJid.isEmpty()) {
                jid = itemJid;
                count++;
            }
        }
    }
    return count == 1 ? jid : QString();
}

struct RetrieveRequestState {
    QXmppPromise<QXmppMamManager::RetrieveResult> promise;
    QXmppMamResultIq iq;
    // bare JID of the queried archive
    QString archiveJid;
    QList<MamMessage> messages;
    // empty for messages consumed by the message pipeline
    QList<std::optional<QXmppMessage>> processedMessages;
    uint runningDecryptionJobs = 0;

    void finish()
    {
        Q_ASSERT(messages.count() == processedMessages.count());
        QList<QXmppMessage> results;
        results.reserve(processedMessages.size());
        for (auto &message : processedMessages) {
            if (message) {
                results.append(std::move(*message));
            }
        }
        promise.finish(
            QXmppMamManager::RetrievedMessages {
                std::move(iq),
                std::move(results) });
    }
};

class QXmppMamManagerPrivate
{
public:
    // std::string because older Qt 5 versions don't add std::hash support for QString
    std::unordered_map<std::string, RetrieveRequestState> ongoingRequests;
};

/*!
    \struct QXmppMamManager::RetrievedMessages
    \inmodule QXmpp

    \brief Contains all retrieved messages and the result IQ that can be used for pagination.

    \since QXmpp 1.5
*/

/*!
    \variable QXmppMamManager::RetrievedMessages::result

    The returned result IQ from the MAM server.
*/

/*!
    \variable QXmppMamManager::RetrievedMessages::messages

    A vector of retrieved QXmppMessages.
*/

/*!
    \typedef QXmppMamManager::RetrieveResult

    Contains RetrievedMessages or an QXmppError.

    \since QXmpp 1.5
*/

QXmppMamManager::QXmppMamManager()
    : d(std::make_unique<QXmppMamManagerPrivate>())
{
    hooks().add<Disco::CollectInfo>([](QList<QString> &features, auto &) {
        // XEP-0313: Message Archive Management
        features << staticString(ns_mam);
    });
}

QXmppMamManager::~QXmppMamManager() = default;

/*!
    Returns a watch on whether the own account supports \xep{0313}{Message Archive Management}.

    The information is requested as long as a copy of the watch exists and shared with all other
    watches on the account information. This requires the QXmppDiscoveryManager to be registered
    with the client. If the manager is not registered with a client, a watch that never loads is
    returned.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppMamManager::watchAccountSupport() const
{
    return watchAccountFeature(client(), QXmpp::Namespace::Mam2);
}

bool QXmppMamManager::handleStanza(const QDomElement &element)
{
    if (element.tagName() == u"message") {
        if (auto result = parseMamMessageResult(element)) {
            auto &[message, queryId] = *result;

            auto itr = d->ongoingRequests.find(queryId.toStdString());
            if (itr != d->ongoingRequests.end()) {
                // future-based API
                itr->second.messages.append(std::move(message));
            } else {
                // signal-based API
                Q_EMIT archivedMessageReceived(queryId, parseMamMessage(message, Unencrypted));
            }
            return true;
        }
    } else if (isIqElement<QXmppMamResultIq>(element)) {
        QXmppMamResultIq result;
        result.parse(element);
        Q_EMIT resultsRecieved(result.id(), result.resultSetReply(), result.complete());
        return true;
    }

    return false;
}

static QXmppMamQueryIq buildRequest(const QString &to,
                                    const QString &node,
                                    const QString &jid,
                                    const QDateTime &start,
                                    const QDateTime &end,
                                    const QXmppResultSetQuery &resultSetQuery)
{
    QList<QXmppDataForm::Field> fields;

    QXmppDataForm::Field hiddenField(QXmppDataForm::Field::HiddenField);
    hiddenField.setKey(u"FORM_TYPE"_s);
    hiddenField.setValue(staticString(ns_mam));
    fields << hiddenField;

    if (!jid.isEmpty()) {
        QXmppDataForm::Field jidField;
        jidField.setKey(u"with"_s);
        jidField.setValue(jid);
        fields << jidField;
    }

    if (start.isValid()) {
        QXmppDataForm::Field startField;
        startField.setKey(u"start"_s);
        startField.setValue(QXmppUtils::datetimeToString(start));
        fields << startField;
    }

    if (end.isValid()) {
        QXmppDataForm::Field endField;
        endField.setKey(u"end"_s);
        endField.setValue(QXmppUtils::datetimeToString(end));
        fields << endField;
    }

    QXmppDataForm form;
    form.setType(QXmppDataForm::Submit);
    form.setFields(fields);

    QXmppMamQueryIq queryIq;
    QString queryId = queryIq.id(); /* reuse the IQ id as query id */
    queryIq.setTo(to);
    queryIq.setNode(node);
    queryIq.setQueryId(queryId);
    queryIq.setForm(form);
    queryIq.setResultSetQuery(resultSetQuery);
    return queryIq;
}

/*!
    Retrieves archived messages. For each received message, the
    archiveMessageReceived() signal is emitted. Once all messages are received,
    the resultsRecieved() signal is emitted. It returns a result set that can
    be used to page through the results.
    The number of results may be limited by the server.

    \warning This API does not work with end-to-end encrypted messages. You can
    use the new QFuture-based API (retrieveMessages()) for that.

    \a to is an optional entity that should be queried. Leave this empty to query the local
    archive. \a node is an optional node that should be queried. This is used when querying a
    pubsub node. \a jid is an optional JID to filter the results. \a start is an optional start
    time to filter the results, \a end an optional end time to filter the results.
    \a resultSetQuery is an optional Result Set Management query. This can be used to limit the
    number of results and to page through the archive.

    Returns the query id of the request. This can be used to associate the corresponding
    resultsRecieved signal.
*/
QString QXmppMamManager::retrieveArchivedMessages(const QString &to,
                                                  const QString &node,
                                                  const QString &jid,
                                                  const QDateTime &start,
                                                  const QDateTime &end,
                                                  const QXmppResultSetQuery &resultSetQuery)
{
    auto queryIq = buildRequest(to, node, jid, start, end, resultSetQuery);
    auto id = queryIq.id();
    client()->send(std::move(queryIq));
    return id;
}

/*!
    Retrieves archived messages and reports all messages at once via a QFuture.

    This function tries to decrypt encrypted messages.

    The number of results may be limited by the server.

    \a to is an optional entity that should be queried. Leave this empty to query the local
    archive. \a node is an optional node that should be queried. This is used when querying a
    pubsub node. \a jid is an optional JID to filter the results. \a start is an optional start
    time to filter the results, \a end an optional end time to filter the results.
    \a resultSetQuery is an optional Result Set Management query. This can be used to limit the
    number of results and to page through the archive.

    Returns the result of the query.

    \since QXmpp 1.5
*/
QXmppTask<QXmppMamManager::RetrieveResult> QXmppMamManager::retrieveMessages(const QString &to, const QString &node, const QString &jid, const QDateTime &start, const QDateTime &end, const QXmppResultSetQuery &resultSetQuery)
{
    auto queryIq = buildRequest(to, node, jid, start, end, resultSetQuery);
    auto queryId = queryIq.queryId();

    auto [itr, inserted] = d->ongoingRequests.insert({ queryIq.queryId().toStdString(), RetrieveRequestState() });
    Q_ASSERT(inserted);
    itr->second.archiveJid = QXmppUtils::jidToBareJid(to.isEmpty() ? client()->configuration().jidBare() : to);

    // create task here; promise could finish immediately after client()->sendIq()
    auto task = itr->second.promise.task();

    // retrieve messages
    client()->sendIq(std::move(queryIq)).then(this, [this, queryId](QXmppClient::IqResult result) {
        auto itr = d->ongoingRequests.find(queryId.toStdString());
        if (itr == d->ongoingRequests.end()) {
            return;
        }
        auto &state = itr->second;

        // handle IQ sending errors
        if (hasError(result)) {
            state.promise.finish(getError(result));
            d->ongoingRequests.erase(itr);
            return;
        }

        // parse IQ
        auto &iq = state.iq;
        iq.parse(getValue(result));

        // process the messages in the message pipeline, they may be decrypted asynchronously
        state.processedMessages.resize(state.messages.size());
        state.runningDecryptionJobs = state.messages.size();
        if (state.messages.isEmpty()) {
            state.finish();
            d->ongoingRequests.erase(itr);
            return;
        }

        auto finishJob = [this, queryId](qsizetype i, std::optional<QXmppMessage> message) {
            auto itr = d->ongoingRequests.find(queryId.toStdString());
            Q_ASSERT(itr != d->ongoingRequests.end());
            auto &state = itr->second;

            state.processedMessages[i] = std::move(message);

            // finish promise on last job
            state.runningDecryptionJobs--;
            if (state.runningDecryptionJobs == 0) {
                state.finish();
                d->ongoingRequests.erase(itr);
            }
        };

        auto *e2eeExt = LegacyE2ee::encryptionExtension(client());
        auto &incomingMessages = client()->d->incomingMessages;
        const auto size = state.messages.size();
        for (qsizetype i = 0; i < size; i++) {
            const auto mamMessage = state.messages.at(i);
            auto parsed = incomingMessages.parse(mamMessage.element, e2eeExt);
            if (mamMessage.delay) {
                parsed.message.setStamp(*mamMessage.delay);
            }

            if (parsed.legacyEncrypted) {
                // encryption without provider
                e2eeExt->decryptMessage(std::move(parsed.message)).then(this, [this, i, mamMessage, finishJob](auto result) {
                    if (std::holds_alternative<QXmppMessage>(result)) {
                        finishJob(i, std::get<QXmppMessage>(std::move(result)));
                    } else {
                        warning(u"Error decrypting message."_s);
                        finishJob(i, parseMamMessage(mamMessage, Unencrypted));
                    }
                });
                continue;
            }

            Context context(std::move(parsed.message), Origin::Mam);
            if (context.senderJid() == client()->configuration().jidBare()) {
                // sent by this account
                context.setConversationJid(QXmppUtils::jidToBareJid(context.message().to()));
            }
            ArchiveInfo info;
            info.setArchiveJid(state.archiveJid);
            info.setId(mamMessage.id);
            info.setOccupantJid(occupantJid(mamMessage.element));
            context.attach(std::move(info));

            incomingMessages.collect(std::move(context), parsed.encryption).then(this, [i, finishJob](std::optional<Context> &&context) {
                finishJob(i, context ? std::optional(std::move(context->message())) : std::nullopt);
            });
        }
    });

    return task;
}

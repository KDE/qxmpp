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
#include "QXmppMessageContext_p.h"
#include "QXmppMessages.h"
#include "QXmppPromise.h"
#include "QXmppStanzaHandling_p.h"
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
    QXmppPromise<Mam::RetrieveResult> promise;
    QXmppMamResultIq iq;
    // bare JID of the queried archive
    QString archiveJid;
    // processed messages in the order of the archive, empty for dropped or consumed messages and
    // messages still being processed
    std::vector<std::optional<Context>> messages;
    // messages still being processed by the message pipeline
    uint runningJobs = 0;
    // the result IQ has been received
    bool resultReceived = false;

    void finish()
    {
        std::vector<Context> results;
        results.reserve(messages.size());
        for (auto &context : messages) {
            if (context) {
                results.push_back(std::move(*context));
            }
        }
        promise.finish(Mam::Page { std::move(iq), std::move(results) });
    }
};

class QXmppMamManagerPrivate
{
public:
    explicit QXmppMamManagerPrivate(QXmppMamManager *q) : q(q) { }

    void processMessage(const QString &queryId, RetrieveRequestState &state, MamMessage &&message);
    QXmppTask<std::optional<Context>> decryptLegacy(QXmppE2eeExtension *e2eeExt, Context context, MamMessage message);
    void finishMessage(const QString &queryId, size_t index, std::optional<Context> &&context);

    QXmppMamManager *q;
    // std::string because older Qt 5 versions don't add std::hash support for QString
    std::unordered_map<std::string, RetrieveRequestState> ongoingRequests;
};

/*!
    \namespace QXmpp::Mam
    \inmodule QXmpp

    Types of \xep{0313}{Message Archive Management}, see QXmppMamManager.

    \since QXmpp 1.18
*/

/*!
    \struct QXmpp::Mam::Query
    \inmodule QXmpp

    Archive query of QXmppMamManager::retrieve().

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::Query::to

    The archive to query, empty for the archive of the own account.
*/

/*!
    \variable QXmpp::Mam::Query::node

    The PubSub node to query, usually empty.
*/

/*!
    \variable QXmpp::Mam::Query::with

    Only returns messages exchanged with this JID if set.
*/

/*!
    \variable QXmpp::Mam::Query::start

    Only returns messages after this time if valid.
*/

/*!
    \variable QXmpp::Mam::Query::end

    Only returns messages before this time if valid.
*/

/*!
    \variable QXmpp::Mam::Query::resultSetQuery

    Limits the number of results and pages through the archive (\xep{0059}{Result Set Management}).
*/

/*!
    \struct QXmpp::Mam::Page
    \inmodule QXmpp

    Result of QXmppMamManager::retrieve().

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::Page::result

    The result IQ of the query, it contains the result set reply for paging.
*/

/*!
    \variable QXmpp::Mam::Page::messages

    The processed messages in the order of the archive. Messages consumed by a handler of
    QXmpp::Mam::MessageReceived are not contained.
*/

/*!
    \typedef QXmpp::Mam::RetrieveResult

    Page or QXmppError.

    \since QXmpp 1.18
*/

/*!
    \struct QXmpp::Mam::MessageReceived
    \inmodule QXmpp

    Hook run for each message of a query of QXmppMamManager::retrieve() as soon as it has been
    processed by the message pipeline (see QXmpp::Messages), i.e. before the query has finished.
    This way, the messages can be shown while further ones are still retrieved. The
    QXmpp::Messages::ArchiveInfo attachment of the context contains the ID of the query.

    Messages of a conversation are passed in the order of the archive. Handlers returning
    QXmpp::HookResult::Stop consume the message, it is then not contained in the result of the
    query. If the query fails, the hook is still run for the messages received before.

    The QXmpp::Messages::Received hook is not run for archived messages.

    \since QXmpp 1.18
*/

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
    : d(std::make_unique<QXmppMamManagerPrivate>(this))
{
    hooks().add<Connection::StanzaReceived>(Private::unencryptedStanzaHandler(this, &QXmppMamManager::handleIncomingStanza));
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

bool QXmppMamManager::handleIncomingStanza(const QDomElement &element)
{
    if (element.tagName() == u"message") {
        if (auto result = parseMamMessageResult(element)) {
            auto &[message, queryId] = *result;

            auto itr = d->ongoingRequests.find(queryId.toStdString());
            if (itr != d->ongoingRequests.end()) {
                // task-based API
                d->processMessage(queryId, itr->second, std::move(message));
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

    This function tries to decrypt encrypted messages. Use retrieve() to get the result of the
    decryption and the resolved sender of the messages.

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
    return retrieve(Mam::Query { to, node, jid, start, end, resultSetQuery }).then(this, [](Mam::RetrieveResult &&result) -> RetrieveResult {
        if (auto *error = std::get_if<QXmppError>(&result)) {
            return std::move(*error);
        }
        auto &page = std::get<Mam::Page>(result);
        QList<QXmppMessage> messages;
        messages.reserve(page.messages.size());
        for (auto &context : page.messages) {
            messages.append(std::move(context.message()));
        }
        return RetrievedMessages { std::move(page.result), std::move(messages) };
    });
}

/*!
    Retrieves archived messages and returns them with their context.

    The messages are processed by the message pipeline (see QXmpp::Messages): The real sender is
    resolved (QXmpp::Messages::ResolveSender) and encrypted messages are decrypted. The
    QXmpp::Messages::Received hook is not run for them and they are not delivered via
    QXmppClient::messageReceived(). Each context has the origin QXmpp::Messages::Origin::Mam
    and a QXmpp::Messages::ArchiveInfo attachment. Messages without content after the decryption
    (e.g. key exchanges) are not returned. To process the messages while the query is still
    running, use the QXmpp::Mam::MessageReceived hook.

    The number of results may be limited by the server, see \a query's result set query for
    paging through the archive.

    \since QXmpp 1.18
*/
QXmppTask<Mam::RetrieveResult> QXmppMamManager::retrieve(const Mam::Query &query)
{
    auto queryIq = buildRequest(query.to, query.node, query.with, query.start, query.end, query.resultSetQuery);
    auto queryId = queryIq.queryId();

    auto [itr, inserted] = d->ongoingRequests.insert({ queryId.toStdString(), RetrieveRequestState() });
    Q_ASSERT(inserted);
    itr->second.archiveJid = QXmppUtils::jidToBareJid(query.to.isEmpty() ? client()->configuration().jidBare() : query.to);

    // create task here; promise could finish immediately after client()->sendIq()
    auto task = itr->second.promise.task();

    // the messages are processed as they arrive, see handleIncomingStanza()
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

        state.iq.parse(getValue(result));
        state.resultReceived = true;
        if (state.runningJobs == 0) {
            state.finish();
            d->ongoingRequests.erase(itr);
        }
    });

    return task;
}

// Processes a message of a task-based query in the message pipeline, it may be decrypted
// asynchronously.
void QXmppMamManagerPrivate::processMessage(const QString &queryId, RetrieveRequestState &state, MamMessage &&message)
{
    const auto index = state.messages.size();
    state.messages.emplace_back();
    state.runningJobs++;

    auto *client = q->client();
    auto *e2eeExt = LegacyE2ee::encryptionExtension(client);
    auto &incomingMessages = client->d->incomingMessages;
    auto parsed = incomingMessages.parse(message.element, e2eeExt);
    if (message.delay) {
        parsed.message.setStamp(*message.delay);
    }

    Context context(std::move(parsed.message), Origin::Mam);
    if (context.senderJid() == client->configuration().jidBare()) {
        // sent by this account
        context.setConversationJid(QXmppUtils::jidToBareJid(context.message().to()));
    }
    ArchiveInfo info;
    info.setArchiveJid(state.archiveJid);
    info.setId(message.id);
    info.setQueryId(queryId);
    info.setOccupantJid(occupantJid(message.element));
    context.attach(std::move(info));

    auto task = parsed.legacyEncrypted
        ? decryptLegacy(e2eeExt, std::move(context), std::move(message))
        : incomingMessages.collect(std::move(context), parsed.encryption);
    task.then(q, [this, queryId, index](std::optional<Context> &&context) {
        finishMessage(queryId, index, std::move(context));
    });
}

// Decrypts a message via the deprecated encryption extension, the hooks of the pipeline are not
// run.
QXmppTask<std::optional<Context>> QXmppMamManagerPrivate::decryptLegacy(QXmppE2eeExtension *e2eeExt, Context context, MamMessage message)
{
    auto result = co_await e2eeExt->decryptMessage(std::move(context.message())).withContext(q);

    auto &d = MessageContextAccess::d(context);
    if (auto *decrypted = std::get_if<QXmppMessage>(&result)) {
        d.message = std::move(*decrypted);
        d.e2eeMetadata = d.message.e2eeMetadata();
        d.decryptionState = DecryptionState::Decrypted;
    } else if (auto *error = std::get_if<QXmppError>(&result)) {
        q->warning(u"Error decrypting message."_s);
        d.message = parseMamMessage(message, Unencrypted);
        d.decryptionState = DecryptionState::Failed;
        d.decryptionError = std::move(*error);
    } else {
        d.message = parseMamMessage(message, Unencrypted);
    }
    co_return std::move(context);
}

// Passes a processed message to the hook and finishes the query after its last message.
void QXmppMamManagerPrivate::finishMessage(const QString &queryId, size_t index, std::optional<Context> &&context)
{
    if (context && q->hooks().run<Mam::MessageReceived>(*context) == HookResult::Stop) {
        context.reset();
    }

    auto itr = ongoingRequests.find(queryId.toStdString());
    if (itr == ongoingRequests.end()) {
        // the query failed
        return;
    }
    auto &state = itr->second;
    state.messages[index] = std::move(context);
    state.runningJobs--;
    if (state.resultReceived && state.runningJobs == 0) {
        state.finish();
        ongoingRequests.erase(itr);
    }
}

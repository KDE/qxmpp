// SPDX-FileCopyrightText: 2016 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMAMMANAGER_H
#define QXMPPMAMMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppError.h"
#include "QXmppHooks.h"
#include "QXmppMamIq.h"
#include "QXmppMessageContext.h"
#include "QXmppResultSet.h"

#include <optional>
#include <variant>
#include <vector>

#include <QDateTime>

template<typename T>
class QXmppTask;
class QXmppMessage;
class QXmppMamManagerPrivate;

namespace QXmpp::Mam {

struct Query {
    QString to {};
    QString node {};
    QString with {};
    QDateTime start {};
    QDateTime end {};
    QString beforeId {};
    QString afterId {};
    QList<QString> ids {};
    std::optional<bool> includeGroupchat {};
    QList<QXmppDataForm::Field> additionalFields {};
    QXmppResultSetQuery resultSetQuery {};
    bool flipPage = false;
};

struct Page {
    QueryResult result {};
    std::vector<QXmpp::Messages::Context> messages {};
};

using RetrieveResult = std::variant<Page, QXmppError>;

struct MessageReceived {
    static constexpr QStringView Name = u"org.qxmpp.mam.message-received";
    using Signature = QXmpp::HookResult(QXmpp::Messages::Context &context);
};

}  // namespace QXmpp::Mam

/*!
    \inmodule QXmpp

    \brief The QXmppMamManager class makes it possible to access message
    archives as defined by \xep{0313}{Message Archive Management}.

    To make use of this manager, you need to instantiate it and load it into
    the QXmppClient instance as follows:

    \code
    QXmppMamManager *manager = new QXmppMamManager;
    client->addExtension(manager);
    \endcode

    Checking whether the account supports message archiving using watchAccountSupport()
    requires the QXmppDiscoveryManager to be registered with the client.

    \ingroup qxmpp-managers

    \since QXmpp 1.0
*/
class QXMPP_EXPORT QXmppMamManager : public QXmppClientExtension
{
    Q_OBJECT

public:
#if QXMPP_DEPRECATED_SINCE(1, 18)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    struct [[deprecated("Use retrieve() and QXmpp::Mam::Page")]]
    QXMPP_EXPORT RetrievedMessages {
        QXmppMamResultIq result;
        QList<QXmppMessage> messages;
    };

    using RetrieveResult [[deprecated("Use retrieve() and QXmpp::Mam::RetrieveResult")]] = std::variant<RetrievedMessages, QXmppError>;
    QT_WARNING_POP
#endif

    QXmppMamManager();
    ~QXmppMamManager();

    QString retrieveArchivedMessages(const QString &to = QString(),
                                     const QString &node = QString(),
                                     const QString &jid = QString(),
                                     const QDateTime &start = QDateTime(),
                                     const QDateTime &end = QDateTime(),
                                     const QXmppResultSetQuery &resultSetQuery = QXmppResultSetQuery());
#if QXMPP_DEPRECATED_SINCE(1, 18)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    [[deprecated("Use retrieve()")]]
    QXmppTask<RetrieveResult> retrieveMessages(const QString &to = QString(),
                                               const QString &node = QString(),
                                               const QString &jid = QString(),
                                               const QDateTime &start = QDateTime(),
                                               const QDateTime &end = QDateTime(),
                                               const QXmppResultSetQuery &resultSetQuery = QXmppResultSetQuery());
    QT_WARNING_POP
#endif
    QXmppTask<QXmpp::Mam::RetrieveResult> retrieve(const QXmpp::Mam::Query &query);
    QXmppTask<QXmpp::Result<QXmppDataForm>> requestQueryForm(const QString &to = {}, const QString &node = {});

    QXmppDiscoFeatureWatch watchAccountSupport() const;

    /*!
        This signal is emitted when an archived \a message is received

        \a queryId.

        For queries sent via retrieveArchivedMessages(), only messages sent by the queried archive
        are emitted (since QXmpp 1.18).
    */
    Q_SIGNAL void archivedMessageReceived(const QString &queryId, const QXmppMessage &message);

    /*!
        This signal is emitted when all results for a request have been received

        \a complete, \a queryId, and \a resultSetReply.
    */
    Q_SIGNAL void resultsRecieved(const QString &queryId, const QXmppResultSetReply &resultSetReply, bool complete);

private:
    bool handleIncomingStanza(const QDomElement &stanza);

    friend class QXmppMamManagerPrivate;
    std::unique_ptr<QXmppMamManagerPrivate> d;
};

#endif

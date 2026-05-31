// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2020 Germán Márquez Mejía <mancho@olomono.de>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPubSubManager.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppPubSubAffiliation.h"
#include "QXmppPubSubBaseItem.h"
#include "QXmppPubSubEventHandler.h"
#include "QXmppPubSubSubscribeOptions.h"
#include "QXmppPubSubSubscription.h"
#include "QXmppStanza.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "Async.h"
#include "StringLiterals.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

/*!
    \class QXmppPubSubEventHandler
    \inmodule QXmpp

    Interface for handling \xep{0060}{Publish-Subscribe} (PubSub) events.

    \since QXmpp 1.5
*/

/*!
    \fn QXmppPubSubEventHandler::handlePubSubEvent()

    Handles the PubSub event.

    \a element is the QDomElement of the &lt;message/&gt; stanza.
    \a pubSubService is the JID of the PubSub service (if the message's 'from'
    attribute is empty then the user's bare JID is used here since
    QXmpp 1.13). \a nodeName is the name of the PubSub node on the service.

    Returns whether the event has been handled and should not be handled by
    other event handlers.
*/

/*!
    \class QXmppPubSubManager
    \inmodule QXmpp

    \brief The QXmppPubSubManager aims to provide publish-subscribe
    functionality as specified in \xep{0060}{Publish-Subscribe} (PubSub).

    However, it currently only supports a few PubSub use cases but all of the
    \xep{0060}{Personal Eventing Protocol} (PEP) ones. PEP allows
    a standard XMPP user account to function as a virtual PubSub service.

    To make use of this manager, you need to instantiate it and load it into
    the QXmppClient instance as follows:

    \code
    QXmppPubSubManager *manager = new QXmppPubSubManager;
    client->addExtension(manager);
    \endcode

    \note To subscribe to PEP event notifications use the
    QXmppClientExtension::discoveryFeatures method of your client extension
    according to section 9.2 of \xep{0060}{Publish-Subscribe}. For example:
    \code
    QStringList YourExtension::discoveryFeatures() const
    {
    return { "http://jabber.org/protocol/tune+notify" };
    }
    \endcode

    \list
        \li Item pagination: requesting a continuation
        \li Requesting most recent items (max_items=x):
            https://xmpp.org/extensions/xep-0060.html#subscriber-retrieve-requestrecent
        \li subscribe()/unsubscribe():
        \li return subscription on success
        \li correctly handle configuration required (and other) cases
    \endlist

    \ingroup Managers

    \since QXmpp 1.5
*/

/*!
    \struct QXmppPubSubManager::Items
    \inmodule QXmpp

    Struct containing a list of items and a continuation if the results were
    incomplete.
*/

/*!
    \typedef QXmppPubSubManager::Result

    Result of a generic request without a return value. Contains Success in case
    everything went well. If the returned IQ contained an error a
    QXmppStanza::Error is reported.
*/

/*!
    \typedef QXmppPubSubManager::FeaturesResult

    Type containing service discovery features, InvalidServiceType if the service is not of the
    desired type or the returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::NodesResult

    Type containing a list of node names or the returned IQ error
    (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::InstantNodeResult

    Contains the name of the new node (QString) or the returned IQ error
    (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::ItemResult

    Contains the item if it has been found (std::optional<T>) or the returned IQ
    error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::ItemsResult

    Contains all items that have been found (QVector<T>) or the returned IQ
    error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::ItemIdsResult

    Contains all item IDs that have been found (QVector<QString>) or the
    returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::PublishItemResult

    Contains the ID of the item, if no ID was set in the request (QString) or
    the returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::PublishItemsResult

    Contains the IDs of the items, if no IDs were set in the request
    (QVector<QString>) or the returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::SubscriptionsResult

    Contains a list of active subscriptions (QVector<QXmppPubSubSubscription>)
    or the returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::AffiliationsResult

    Contains the list of affiliations with the node(s)
    (QVector<QXmppPubSubAffiliation>) or the returned IQ error
    (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::OptionsResult

    Contains the current subscribe options (QXmppPubSubSubscribeOptions) or the
    returned IQ error (QXmppStanza::Error).
*/

/*!
    \typedef QXmppPubSubManager::NodeConfigResult

    Contains a node configuration (QXmppPubSubNodeConfig) or the returned IQ
    error (QXmppStanza::Error).
*/

/*! Default constructor. */
QXmppPubSubManager::QXmppPubSubManager()
{
}

/*! Default destructor. */
QXmppPubSubManager::~QXmppPubSubManager()
{
}

/*!
    Requests all features of a pubsub service and checks the identities via service discovery.

    This uses a \xep{0030}{Service Discovery} info request to get the service
    identities and features.

    The features are only returned if the service is of type \a serviceType,
    otherwise InvalidServiceType is returned.

    \a serviceJid is the JID of the entity hosting the pubsub service.
    \a serviceType is the type of service to retrieve features for.
*/
QXmppTask<QXmppPubSubManager::FeaturesResult> QXmppPubSubManager::requestFeatures(const QString &serviceJid, ServiceType serviceType)
{
    auto *discoManager = client()->findExtension<QXmppDiscoveryManager>();
    Q_ASSERT(discoManager);

    auto infoResult = co_await discoManager->info(serviceJid);
    if (hasError(infoResult)) {
        co_return getError(std::move(infoResult));
    }

    auto &info = getValue(infoResult);
    const auto &identities = info.identities();
    const auto isPubSubServiceFound = std::any_of(identities.cbegin(), identities.cend(), [=](const QXmppDiscoIdentity &identity) {
        if (identity.category() == u"pubsub") {
            const auto identityType = identity.type();

            switch (serviceType) {
            case PubSubOrPep:
                return identityType == u"service" || identityType == u"pep";
            case PubSub:
                return identityType == u"service";
            case Pep:
                return identityType == u"pep";
            }
        }
        return false;
    });

    if (isPubSubServiceFound) {
        co_return info.features();
    }

    co_return InvalidServiceType();
}

/*!
    Requests all listed nodes of the pubsub service with JID \a jid via
    service discovery. \a jid is the Jabber ID of the entity hosting the
    pubsub service.

    This uses a \xep{0030}{Service Discovery} items request to get a list of
    nodes.
*/
QXmppTask<QXmppPubSubManager::NodesResult> QXmppPubSubManager::requestNodes(const QString &jid)
{
    auto *discoManager = client()->findExtension<QXmppDiscoveryManager>();
    Q_ASSERT(discoManager);

    auto itemsResult = co_await discoManager->items(jid);
    if (hasError(itemsResult)) {
        co_return getError(std::move(itemsResult));
    }

    QVector<QString> nodes;
    for (const auto &item : std::as_const(getValue(itemsResult))) {
        // only accept non-empty nodes
        if (const auto node = item.node(); !node.isEmpty()) {
            nodes << node;
        }
    }

    // make unique
    std::sort(nodes.begin(), nodes.end());
    nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
    co_return nodes;
}

/*!
    Creates an empty pubsub node \a nodeName on the pubsub service with JID
    \a jid using the default configuration. \a jid is the Jabber ID of the
    entity hosting the pubsub service.

    Calling this before QXmppPubSubManager::publishItems is usually not
    necessary when publishing to a node for the first time if the service
    suppports the auto-create feature (Section 7.1.4 of \xep{0060}{Publish-Subscribe}).
*/
auto QXmppPubSubManager::createNode(const QString &jid, const QString &nodeName) -> QXmppTask<Result>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Create);
    request.setQueryNode(nodeName);
    request.setTo(jid);

    return client()->sendGenericIq(std::move(request));
}

/*!
    Creates an empty pubsub node \a nodeName on the pubsub service with JID
    \a jid using the custom configuration \a config. \a jid is the Jabber ID
    of the entity hosting the pubsub service.

    Calling this before QXmppPubSubManager::publishItems is usually not
    necessary when publishing to a node for the first time if the service
    suppports the auto-create feature (Section 7.1.4 of \xep{0060}{Publish-Subscribe}).
*/
auto QXmppPubSubManager::createNode(const QString &jid, const QString &nodeName, const QXmppPubSubNodeConfig &config) -> QXmppTask<Result>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Create);
    request.setQueryNode(nodeName);
    request.setTo(jid);
    request.setDataForm(config);

    return client()->sendGenericIq(std::move(request));
}

/*!
    Creates an instant pubsub node on the pubsub service with JID \a jid using
    the default configuration. \a jid is the Jabber ID of the entity hosting
    the pubsub service.

    The pubsub service automatically generates a random node name. On success
    it is returned via the QFuture.
*/
auto QXmppPubSubManager::createInstantNode(const QString &jid) -> QXmppTask<InstantNodeResult>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Create);
    request.setTo(jid);

    co_return mapSuccess(
        parseIq<PubSubIq<>>(co_await client()->sendIq(std::move(request))),
        [](const PubSubIq<> &iq) { return iq.queryNode(); });
}

/*!
    Creates an instant pubsub node on the pubsub service with JID \a jid using
    the custom configuration \a config. \a jid is the Jabber ID of the entity
    hosting the pubsub service.

    The pubsub service automatically generates a random node name. On success
    it is returned via the QFuture.
*/
auto QXmppPubSubManager::createInstantNode(const QString &jid, const QXmppPubSubNodeConfig &config) -> QXmppTask<InstantNodeResult>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Create);
    request.setTo(jid);
    request.setDataForm(config);

    co_return mapSuccess(
        parseIq<PubSubIq<>>(co_await client()->sendIq(std::move(request))),
        [](const PubSubIq<> &iq) {
            return iq.queryNode();
        });
}

/*!
    Deletes the pubsub node \a nodeName, along with all of its items, on the
    pubsub service with JID \a jid. \a jid is the Jabber ID of the entity
    hosting the pubsub service.
*/
auto QXmppPubSubManager::deleteNode(const QString &jid, const QString &nodeName) -> QXmppTask<Result>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Delete);
    request.setQueryNode(nodeName);
    request.setTo(jid);

    return client()->sendGenericIq(std::move(request));
}

/*!
    Requests the IDs of all items of the pubsub node \a nodeName on the
    service with JID \a serviceJid via service discovery. \a serviceJid is
    the JID of the entity hosting the pubsub service.

    This uses a \xep{0030}{Service Discovery} items request to get a list of
    items.
*/
QXmppTask<QXmppPubSubManager::ItemIdsResult> QXmppPubSubManager::requestItemIds(const QString &serviceJid, const QString &nodeName)
{
    auto *discoManager = client()->findExtension<QXmppDiscoveryManager>();
    Q_ASSERT(discoManager);

    auto items = co_await discoManager->items(serviceJid, nodeName);
    if (hasError(items)) {
        co_return getError(std::move(items));
    }

    co_return transform<QVector<QString>>(getValue(std::move(items)), [](const auto &item) {
        return item.name();
    });
}

/*!
    Deletes the item with ID \a itemId from the pubsub node \a nodeName on
    the service with JID \a jid. \a jid is the Jabber ID of the entity
    hosting the pubsub service. \a notify specifies whether to generate
    retraction notifications for subscribers (since QXmpp 1.11, default:
    false).
*/
auto QXmppPubSubManager::retractItem(const QString &jid, const QString &nodeName, const QString &itemId, bool notify)
    -> QXmppTask<Result>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Retract);
    request.setQueryNode(nodeName);
    request.setItems({ QXmppPubSubBaseItem(itemId) });
    request.setRetractNotify(notify);
    request.setTo(jid);

    return client()->sendGenericIq(std::move(request));
}

/*!
    Deletes the item with standard ID \a itemId from the pubsub node
    \a nodeName on the service with JID \a jid. \a jid is the Jabber ID of
    the entity hosting the pubsub service. \a notify specifies whether to
    generate retraction notifications for subscribers (since QXmpp 1.11,
    default: false).
*/
auto QXmppPubSubManager::retractItem(const QString &jid, const QString &nodeName, StandardItemId itemId, bool notify)
    -> QXmppTask<Result>
{
    return retractItem(jid, nodeName, standardItemIdToString(itemId), notify);
}

/*!
    Purges all items from the node \a nodeName on the pubsub service with JID
    \a jid. \a jid is the Jabber ID of the entity hosting the pubsub service.
*/
auto QXmppPubSubManager::purgeItems(const QString &jid, const QString &nodeName) -> QXmppTask<Result>
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIq<>::Purge);
    request.setQueryNode(nodeName);
    request.setTo(jid);

    return client()->sendGenericIq(std::move(request));
}

/*!
    Requests all subscriptions with the PubSub service with JID \a jid.
*/
QXmppTask<QXmppPubSubManager::SubscriptionsResult> QXmppPubSubManager::requestSubscriptions(const QString &jid)
{
    return requestSubscriptions(jid, {});
}

/*!
    Requests the subscription(s) with the PubSub node \a nodeName on the
    service with JID \a jid.
*/
QXmppTask<QXmppPubSubManager::SubscriptionsResult> QXmppPubSubManager::requestSubscriptions(const QString &jid, const QString &nodeName)
{
    PubSubIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);
    request.setQueryType(PubSubIq<>::Subscriptions);
    request.setQueryNode(nodeName);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](auto &&iq) -> SubscriptionsResult { return iq.subscriptions(); });
}

/*!
    Requests the affiliations of all users on the PubSub node \a nodeName of
    the service with JID \a jid.

    This can be used to view and manage affiliations of other users with a node.
    Owner privileges are required.
*/
QXmppTask<QXmppPubSubManager::AffiliationsResult> QXmppPubSubManager::requestNodeAffiliations(const QString &jid, const QString &nodeName)
{
    PubSubIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);
    request.setQueryType(PubSubIq<>::OwnerAffiliations);
    request.setQueryNode(nodeName);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](auto &&iq) -> AffiliationsResult { return iq.affiliations(); });
}

/*!
    Requests the user's affiliations with all PubSub nodes on the PubSub
    service with JID \a jid.
*/
QXmppTask<QXmppPubSubManager::AffiliationsResult> QXmppPubSubManager::requestAffiliations(const QString &jid)
{
    return requestAffiliations(jid, {});
}

/*!
    Requests the user's affiliations with the PubSub node \a nodeName on the
    service with JID \a jid.
*/
QXmppTask<QXmppPubSubManager::AffiliationsResult> QXmppPubSubManager::requestAffiliations(const QString &jid, const QString &nodeName)
{
    PubSubIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);
    request.setQueryType(PubSubIq<>::Affiliations);
    request.setQueryNode(nodeName);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](auto &&iq) -> AffiliationsResult { return iq.affiliations(); });
}

/*!
    Requests the subscribe options form of the own subscription to the node
    \a nodeName on the pubsub service with JID \a service.
*/
QXmppTask<QXmppPubSubManager::OptionsResult> QXmppPubSubManager::requestSubscribeOptions(const QString &service, const QString &nodeName)
{
    return requestSubscribeOptions(service, nodeName, client()->configuration().jidBare());
}

/*!
    Requests the subscribe options form of the subscription of the user with
    JID \a subscriberJid to the node \a nodeName on the pubsub service with
    JID \a service.
*/
QXmppTask<QXmppPubSubManager::OptionsResult> QXmppPubSubManager::requestSubscribeOptions(const QString &service, const QString &nodeName, const QString &subscriberJid)
{
    PubSubIq request;
    request.setType(QXmppIq::Get);
    request.setTo(service);
    request.setQueryType(PubSubIq<>::Options);
    request.setQueryNode(nodeName);
    request.setQueryJid(subscriberJid);

    co_return parseIq<PubSubIq<>>(co_await client()->sendIq(std::move(request)), [](PubSubIq<> &&iq) -> OptionsResult {
        if (const auto form = iq.dataForm()) {
            if (const auto options = QXmppPubSubSubscribeOptions::fromDataForm(*form)) {
                return *options;
            }
        }
        return QXmppError { u"Server returned invalid data form."_s, {} };
    });
}

/*!
    Sets the subscription options \a options for our own account on the node
    \a nodeName of the pubsub service with JID \a service.
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::setSubscribeOptions(const QString &service, const QString &nodeName, const QXmppPubSubSubscribeOptions &options)
{
    return setSubscribeOptions(service, nodeName, options, client()->configuration().jidBare());
}

/*!
    Sets the subscription options \a options for another user's account
    identified by \a subscriberJid on the node \a nodeName of the pubsub
    service with JID \a service.
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::setSubscribeOptions(const QString &service, const QString &nodeName, const QXmppPubSubSubscribeOptions &options, const QString &subscriberJid)
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setTo(service);
    request.setQueryType(PubSubIq<>::Options);
    request.setDataForm(options);
    request.setQueryNode(nodeName);
    request.setQueryJid(subscriberJid);
    return client()->sendGenericIq(std::move(request));
}

/*!
    Requests the node configuration and starts the configuration process.

    Requires owner privileges. If the result is successful (a node config form
    has been returned) this starts the configuration process. The next step is
    to call configureNode() or cancelNodeConfiguration().

    \a service is the JID of the pubsub service. \a nodeName is the name of
    the pubsub node on the service.

    \sa configureNode()
    \sa cancelNodeConfiguration()
*/
QXmppTask<QXmppPubSubManager::NodeConfigResult> QXmppPubSubManager::requestNodeConfiguration(const QString &service, const QString &nodeName)
{
    PubSubIq request;
    request.setType(QXmppIq::Get);
    request.setTo(service);
    request.setQueryNode(nodeName);
    request.setQueryType(PubSubIq<>::Configure);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](PubSubIq<> &&iq) -> NodeConfigResult {
            if (const auto dataForm = iq.dataForm()) {
                if (const auto config = QXmppPubSubNodeConfig::fromDataForm(*dataForm)) {
                    return *config;
                }
                return QXmppError { u"Server returned invalid data form."_s, {} };
            }
            return QXmppError { u"Server returned no data form."_s, {} };
        });
}

/*!
    Sets the node configuration \a config for the pubsub node \a nodeName on
    the service with JID \a service.

    Requires owner privileges. You can use requestNodeConfiguration() to receive
    a data form with all valid options and default values.

    \sa requestNodeConfiguration()
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::configureNode(const QString &service, const QString &nodeName, const QXmppPubSubNodeConfig &config)
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setTo(service);
    request.setQueryNode(nodeName);
    request.setQueryType(PubSubIq<>::Configure);
    request.setDataForm(config);
    return client()->sendGenericIq(std::move(request));
}

/*!
    Cancels the configuration process for the pubsub node \a nodeName on the
    service with JID \a service and uses the default or existing
    configuration.

    \sa requestNodeConfiguration()
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::cancelNodeConfiguration(const QString &service, const QString &nodeName)
{
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setTo(service);
    request.setQueryNode(nodeName);
    request.setQueryType(PubSubIq<>::Configure);
    request.setDataForm(QXmppDataForm(QXmppDataForm::Cancel));
    return client()->sendGenericIq(std::move(request));
}

/*!
    Subscribes the user with JID \a subscriberJid to the pubsub node
    \a nodeName on the service with JID \a serviceJid. \a subscriberJid is
    the bare or full JID of the subscriber.
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::subscribeToNode(const QString &serviceJid, const QString &nodeName, const QString &subscriberJid)
{
    // TODO: Return subscription
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setTo(serviceJid);
    request.setQueryNode(nodeName);
    request.setQueryType(PubSubIq<>::Subscribe);
    request.setQueryJid(subscriberJid);
    return client()->sendGenericIq(std::move(request));
}

/*!
    Unsubscribes the user with JID \a subscriberJid from the pubsub node
    \a nodeName on the service with JID \a serviceJid. \a subscriberJid is
    the bare or full JID of the subscriber.
*/
QXmppTask<QXmppPubSubManager::Result> QXmppPubSubManager::unsubscribeFromNode(const QString &serviceJid, const QString &nodeName, const QString &subscriberJid)
{
    // TODO: Return subscription
    PubSubIq request;
    request.setType(QXmppIq::Set);
    request.setTo(serviceJid);
    request.setQueryNode(nodeName);
    request.setQueryType(PubSubIq<>::Unsubscribe);
    request.setQueryJid(subscriberJid);
    return client()->sendGenericIq(std::move(request));
}

/*!
    \fn QXmppPubSubManager::requestOwnPepFeatures()

    Requests all features of the own PEP service via service discovery.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::requestFeatures on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepNodes()

    Requests all listed nodes of the own PEP service via service discovery.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::fetchNodes on the current account's bare JID.
*/

/*!
    \fn QXmppTask<Result> QXmppPubSubManager::createOwnPepNode(const QString &nodeName)

    Creates an empty PEP node \a nodeName with the default configuration.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::createNode on the current account's bare JID.

    Calling this before QXmppPubSubManager::publishOwnPepItems is usually not
    necessary when publishing to a node for the first time if the service
    suppports the auto-create feature (Section 7.1.4 of \xep{0060}{Publish-Subscribe}).
*/

/*!
    \fn QXmppTask<Result> QXmppPubSubManager::createOwnPepNode(const QString &nodeName, const QXmppPubSubNodeConfig &config)

    Creates an empty PEP node \a nodeName with the custom configuration
    \a config.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::createNode on the current account's bare JID.

    Calling this before QXmppPubSubManager::publishOwnPepItems is usually not
    necessary when publishing to a node for the first time if the service
    suppports the auto-create feature (Section 7.1.4 of \xep{0060}{Publish-Subscribe}).
*/

/*!
    \fn QXmppPubSubManager::deleteOwnPepNode

    Deletes the PEP node \a nodeName, along with all of its items.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::deleteNode on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepItem(const QString &nodeName, const QString &itemId)

    Requests the item with ID \a itemId from the PEP node \a nodeName.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::requestItem on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepItem(const QString &nodeName, StandardItemId itemId)

    Requests the item with standard ID \a itemId from the PEP node
    \a nodeName.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::requestItem on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepItems(const QString &nodeName)

    Requests all items of the PEP node \a nodeName.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::requestItems on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepItemIds(const QString &nodeName)

    Requests the IDs of all items of the PEP node \a nodeName via service
    discovery.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::requestItemIds on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::retractOwnPepItem(const QString &nodeName, const QString &itemId, bool notify)

    Deletes the item with ID \a itemId from the PEP node \a nodeName.
    \a notify specifies whether to generate retraction notifications for
    subscribers (since QXmpp 1.13, default: false).

    This is a convenience method equivalent to calling
    QXmppPubSubManager::retractItem on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::retractOwnPepItem(const QString &nodeName, StandardItemId itemId, bool notify)

    Deletes the item with standard ID \a itemId from the PEP node
    \a nodeName. \a notify specifies whether to generate retraction
    notifications for subscribers (since QXmpp 1.13, default: false).

    This is a convenience method equivalent to calling
    QXmppPubSubManager::retractItem on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::purgeOwnPepItems

    Purges all items from the PEP node \a nodeName.

    This is a convenience method equivalent to calling
    QXmppPubSubManager::purgeItems on the current account's bare JID.
*/

/*!
    \fn QXmppPubSubManager::requestOwnPepNodeConfiguration

    Requests the node configuration of the PEP node \a nodeName and starts
    the configuration process.

    This is a convenience method equivalent to calling
    requestNodeConfiguration() the current account's bare JID.

    \sa configureOwnPepNode()
    \sa cancelOwnPepNodeConfiguration()
*/

/*!
    \fn QXmppPubSubManager::configureOwnPepNode

    Sets the node configuration \a config for the PEP node \a nodeName.

    This is a convenience method equivalent to calling configureNode() the
    current account's bare JID.

    \sa requestOwnPepNodeConfiguration()
*/

/*!
    \fn QXmppPubSubManager::cancelOwnPepNodeConfiguration

    Cancels the configuration process for the PEP node \a nodeName.

    This is a convenience method equivalent to calling cancelNodeConfiguration()
    the current account's bare JID.

    \sa requestOwnPepNodeConfiguration()
*/

/*!
    Returns the item ID string for the standard item ID \a itemId, or a
    default-constructed string if there is no corresponding one.
*/
QString QXmppPubSubManager::standardItemIdToString(StandardItemId itemId)
{
    switch (itemId) {
    case Current:
        return u"current"_s;
    }
    return {};
}

QStringList QXmppPubSubManager::discoveryFeatures() const
{
    return {
        staticString(ns_pubsub_rsm)
    };
}

bool QXmppPubSubManager::handleStanza(const QDomElement &element)
{
    if (element.tagName() != u"message") {
        return false;
    }

    auto event = firstChildElement(element, u"event", ns_pubsub_event);
    if (!event.isNull()) {
        auto service = element.attribute(u"from"_s);
        if (service.isEmpty()) {
            service = client()->configuration().jidBare();
        }
        const auto node = event.firstChildElement().attribute(u"node"_s);

        const auto extensions = client()->extensions();
        for (auto *extension : extensions) {
            if (auto *eventHandler = dynamic_cast<QXmppPubSubEventHandler *>(extension)) {
                if (eventHandler->handlePubSubEvent(element, service, node)) {
                    return true;
                }
            }
        }
    }
    return false;
}

PubSubIq<> QXmppPubSubManager::requestItemsIq(const QString &jid, const QString &nodeName, const QStringList &itemIds)
{
    PubSubIq request;
    request.setTo(jid);
    request.setType(QXmppIq::Get);
    request.setQueryType(PubSubIqBase::Items);
    request.setQueryNode(nodeName);

    if (!itemIds.isEmpty()) {
        QVector<QXmppPubSubBaseItem> items;
        items.reserve(itemIds.size());
        for (const auto &id : itemIds) {
            items << QXmppPubSubBaseItem(id);
        }
        request.setItems(items);
    }
    return request;
}

auto QXmppPubSubManager::publishItem(PubSubIqBase &&request) -> QXmppTask<PublishItemResult>
{
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIqBase::Publish);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](const PubSubIq<> &iq) -> PublishItemResult {
            if (!iq.items().isEmpty()) {
                return iq.items().constFirst().id();
            } else {
                return QString();
            }
        });
}

auto QXmppPubSubManager::publishItems(PubSubIqBase &&request) -> QXmppTask<PublishItemsResult>
{
    request.setType(QXmppIq::Set);
    request.setQueryType(PubSubIqBase::Publish);

    co_return parseIq<PubSubIq<>>(
        co_await client()->sendIq(std::move(request)),
        [](auto &&iq) -> PublishItemsResult {
            return transform<QVector<QString>>(iq.items(), &QXmppPubSubBaseItem::id);
        });
}

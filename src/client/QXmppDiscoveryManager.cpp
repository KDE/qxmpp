// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppDiscoveryIq_p.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppIqHandling.h"
#include "QXmppPresence.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
#include "Async.h"
#include "Iq.h"
#include "StringLiterals.h"

#include <QCoreApplication>

using namespace QXmpp;

using DiscoveryState = QXmppDiscoveryManagerPrivate::DiscoveryState;

template<typename... Ts>
inline uint qHash(const std::tuple<Ts...> &t, uint seed = 0) noexcept
{
    return std::apply([&](auto const &...args) {
        ((seed = qHash(args, seed ^ 0x9e3779b9u + (seed << 6) + (seed >> 2))), ...);
        return seed;
    },
                      t);
}

template<typename Response, typename Payload>
static QXmppTask<std::variant<Response, QXmppError>> get(QXmppClient *client, const QString &to, Payload &&payload)
{
    return chain<std::variant<Response, QXmppError>>(
        client->sendIq(CompatIq { GetIq<Payload> { generateSequentialStanzaId(), {}, to, {}, std::move(payload) } }),
        client,
        parseIqResponseFlat<Response>);
}

QXmppDiscoveryManager::QXmppDiscoveryManager()
    : d(new QXmppDiscoveryManagerPrivate(this))
{
    d->recentInfoEntries.setMaxCost(50);
    d->itemsCache.setMaxCost(50);
    d->capsCache.setMaxCost(200);
    d->clientCapabilitiesNode = u"org.qxmpp.caps"_s;
    d->identities = { d->defaultIdentity() };
}

QXmppDiscoveryManager::~QXmppDiscoveryManager()
{
    for (const auto &entry : d->lockInfoEntries()) {
        entry->manager = nullptr;
    }
    const auto infoWatches = d->lockInfoWatches();
    for (const auto &data : infoWatches) {
        data->manager = nullptr;
    }
    // the information will never be known, cancel the tasks waiting for it
    for (const auto &data : infoWatches) {
        auto promises = std::move(data->knownPromises);
        data->knownPromises.clear();
    }
}

/*!
    Fetches discovery info from the specified XMPP entity.

    \since QXmpp 1.12

    \a cachePolicy, \a node, and \a jid.

    For available entities that announce \xep{0115}{Entity Capabilities} in their presence, the
    info is requested using the caps and cached for all entities with the same caps. Such cached
    info is always up to date and is also used with CachePolicy::Strict.
*/
QXmppTask<Result<QXmppDiscoInfo>> QXmppDiscoveryManager::info(const QString &jid, const QString &node, CachePolicy cachePolicy)
{
    // XEP-0115: Entity Capabilities, the info is up to date with both policies
    if (node.isEmpty()) {
        if (auto itr = d->availableJids.constFind(jid); itr != d->availableJids.cend() && itr->has_value()) {
            // also covers info that has not been verified or is no longer in the caps cache
            if (auto entry = d->findEntry(jid, {}); entry && entry->state.value() == QXmppDiscoInfoWatch::State::Loaded) {
                return makeReadyTask<Result<QXmppDiscoInfo>>(*entry->info.value());
            }
            return d->capsInfo(jid, **itr);
        }
    }

    if (cachePolicy == CachePolicy::Relaxed) {
        if (auto entry = d->findEntry(jid, node); entry && entry->state.value() == QXmppDiscoInfoWatch::State::Loaded) {
            return makeReadyTask<Result<QXmppDiscoInfo>>(*entry->info.value());
        }
    }

    return d->infoRequests.produce(
        { jid, node },
        [this](const auto &key) {
            auto &[jid, node] = key;
            return chain<Result<QXmppDiscoInfo>>(
                get<QXmppDiscoInfo>(client(), jid, QXmppDiscoInfo { node }),
                this,
                [this, jid, node](auto &&result) -> Result<QXmppDiscoInfo> {
                    // only cache successful responses for now (permanent errors could also be cached)
                    if (hasValue(result)) {
                        d->storeInfo(jid, node, getValue(result));
                    }
                    return result;
                });
        },
        this);
}

/*!
    Fetches discovery items from the specified XMPP entity.

    \since QXmpp 1.12

    \a cachePolicy, \a node, and \a jid.
*/
QXmppTask<Result<QList<QXmppDiscoItem>>> QXmppDiscoveryManager::items(const QString &jid, const QString &node, CachePolicy cachePolicy)
{
    if (cachePolicy == CachePolicy::Relaxed) {
        if (auto *cachedItems = d->itemsCache[{ jid, node }]) {
            return makeReadyTask<Result<QList<QXmppDiscoItem>>>(*cachedItems);
        }
    }

    return d->itemsRequests.produce(
        { jid, node },
        [this](const auto &key) {
            auto &[jid, node] = key;
            return chain<Result<QList<QXmppDiscoItem>>>(
                get<QXmppDiscoItems>(client(), jid, QXmppDiscoItems { node }),
                this,
                [this, jid, node](auto &&result) -> Result<QList<QXmppDiscoItem>> {
                    if (auto *itemsPayload = std::get_if<QXmppDiscoItems>(&result)) {
                        d->itemsCache.insert({ jid, node }, new QList<QXmppDiscoItem> { itemsPayload->items() });
                        return itemsPayload->items();
                    } else {
                        return getError(std::move(result));
                    }
                });
        },
        this);
}

/*!
    Returns the base identities of this client.

    The identities are added to the service discovery information other entities can request.

    \note Additionally also all identities reported via QXmppClientExtension::discoveryIdentities() are added.

    \note The default identity is type=client, category=pc/phone (OS dependent) and name="{application name} {application version}".

    \since QXmpp 1.12
*/
const QList<QXmppDiscoIdentity> &QXmppDiscoveryManager::identities() const
{
    return d->identities;
}

/*!
    Sets the base identities of this client.

    The identities are added to the service discovery information other entities can request.

    \note Additionally also all identities reported via QXmppClientExtension::discoveryIdentities() are added.

    \note The default identity is type=client, category=pc/phone (OS dependent) and name="{application name} {application version}".

    \since QXmpp 1.12

    \a identities.
*/
void QXmppDiscoveryManager::setIdentities(const QList<QXmppDiscoIdentity> &identities)
{
    d->identities = identities;
}

/*!
    Returns the data forms for this client as defined in \xep{0128}{Service Discovery Extensions}.

    The data forms are added to the service discovery information other entities can request.

    \since QXmpp 1.12
*/
const QList<QXmppDataForm> &QXmppDiscoveryManager::infoForms() const
{
    return d->dataForms;
}

/*!
    Sets the data forms for this client as defined in \xep{0128}{Service Discovery Extensions}.

    The data forms are added to the service discovery information other entities can request.

    \since QXmpp 1.12

    \a dataForms.
*/
void QXmppDiscoveryManager::setInfoForms(const QList<QXmppDataForm> &dataForms)
{
    d->dataForms = dataForms;
}

/*!
    Builds a full disco info element for this client.

    Contains features and identities from all extensions and identities and data forms configured
    in this manager.

    \since QXmpp 1.12
*/
QXmppDiscoInfo QXmppDiscoveryManager::buildClientInfo() const
{
    const auto extensions = client()->extensions();

    // collect features and identities
    auto allFeatures = QXmppClientPrivate::discoveryFeatures();
    auto allIdentities = d->identities;
    for (auto *extension : extensions) {
        if (extension) {
            allFeatures << extension->discoveryFeatures();
            allIdentities << extension->discoveryIdentities();
        }
    }

    std::sort(allFeatures.begin(), allFeatures.end());

    return QXmppDiscoInfo { {}, allIdentities, allFeatures, d->dataForms };
}

// QXmppDiscoInfoWatch

static bool isKnown(QXmppDiscoInfoWatch::State state)
{
    using enum QXmppDiscoInfoWatch::State;
    return state == Loaded || state == Stale || state == Error;
}

/*!
    \class QXmppDiscoInfoWatch
    \inmodule QXmpp

    \brief Lightweight handle to a watch on the service discovery information of an entity.

    Returned by QXmppDiscoveryManager::watchInfo(), QXmppDiscoveryManager::watchServerInfo()
    and QXmppDiscoveryManager::watchAccountInfo(). Cheap to copy — all copies and all other
    watches on the same entity share the same underlying state. When the last copy goes out
    of scope, the watch is automatically unregistered from the discovery manager.

    \since QXmpp 1.17
*/

/*!
    \enum QXmppDiscoInfoWatch::State

    \value Unknown No information is available and no request is running, e.g. because the
    client is not connected.
    \value Loading The information is being requested for the first time.
    \value Loaded The information is up to date.
    \value Stale The information is from an earlier point in time and may be outdated, e.g.
    after a new stream has been started. A new request is running or will be started once the
    client is connected.
    \value Error The entity responded with an error, e.g. because it does not exist.

    The information is available exactly in the states Loaded and Stale. A watch that has
    information never goes back to Loading, so user interfaces can keep showing it while it is
    refreshed.
*/

/*!
    Constructs a watch that is not connected to any entity.

    Its state stays Unknown and it never contains information.
*/
QXmppDiscoInfoWatch::QXmppDiscoInfoWatch()
    : d(std::make_shared<Data>())
{
}

QXmppDiscoInfoWatch::QXmppDiscoInfoWatch(std::shared_ptr<Data> d)
    : d(std::move(d))
{
}

QXmppTask<void> QXmppDiscoInfoWatch::Data::waitUntilKnown()
{
    if (!knownNotifier) {
        knownNotifier = state.addNotifier([this] {
            if (!isKnown(state.value())) {
                return;
            }
            // finishing resumes user code, which may destroy this
            auto promises = std::move(knownPromises);
            knownPromises.clear();
            for (auto &promise : promises) {
                promise.finish();
            }
        });
    }

    QXmppPromise<void> promise;
    auto task = promise.task();
    knownPromises.push_back(std::move(promise));
    return task;
}

void QXmppDiscoInfoWatch::Data::setEntry(std::shared_ptr<DiscoInfoEntry> newEntry)
{
    if (newEntry == entry) {
        return;
    }
    if (newEntry) {
        newEntry->watchCount++;
        state.setBinding([entry = newEntry.get()] { return entry->state.value(); });
        info.setBinding([entry = newEntry.get()] { return entry->info.value(); });
        changesTracked.setBinding([entry = newEntry.get()] { return entry->changesTracked.value(); });
    } else {
        state = State::Unknown;
        info = std::nullopt;
        changesTracked = false;
    }
    if (entry) {
        entry->watchCount--;
    }
    entry = std::move(newEntry);
}

QXmppDiscoInfoWatch::Data::~Data()
{
    if (entry) {
        entry->watchCount--;
    }
    if (manager) {
        // the key may already belong to a new watch
        auto &watches = manager->d->infoWatches;
        if (auto itr = watches.find(key); itr != watches.end() && itr->second.expired()) {
            watches.erase(itr);
        }
    }
}

DiscoInfoEntry::~DiscoInfoEntry()
{
    if (manager) {
        manager->infoEntries.remove({ jid, node });
    }
}

/*! Returns the state of the watched information. */
QBindable<QXmppDiscoInfoWatch::State> QXmppDiscoInfoWatch::state() const
{
    return &d->state;
}

/*!
    Returns the watched information.

    Contains a value exactly if the state is Loaded or Stale.
*/
QBindable<std::optional<QXmppDiscoInfo>> QXmppDiscoInfoWatch::info() const
{
    return &d->info;
}

/*!
    Returns whether changes of the information are tracked.

    If changes are tracked, the information is requested again whenever it changes, so
    information in the state Loaded stays up to date. This is the case for MUC rooms that have
    been joined using QXmppMucManagerV2 and for available entities that announce
    \xep{0115}{Entity Capabilities} in their presence.

    The information of full JIDs is dropped when the entity becomes unavailable or available
    again, as the JID may refer to another entity then, e.g. a MUC occupant.

    Otherwise the information is a snapshot from the time it has been requested. Use refresh()
    to request it again.
*/
QBindable<bool> QXmppDiscoInfoWatch::changesTracked() const
{
    return &d->changesTracked;
}

/*!
    Requests the information again, even if it is up to date.

    Existing information stays available with the state Stale until the response arrives.
*/
void QXmppDiscoInfoWatch::refresh()
{
    if (d->manager) {
        d->manager->d->updateEntry(*d);
        if (d->entry) {
            d->manager->d->fetchInfo(d->entry, QXmppDiscoveryManager::CachePolicy::Strict);
        }
    }
}

/*!
    Returns a watch on whether the entity supports \a feature.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppDiscoInfoWatch::watchFeature(const QString &feature) const
{
    return watchFeatures({ feature });
}

/*!
    Returns a watch on whether the entity supports all of \a features.

    The feature watch shares the information of this watch and does not cause any requests
    on its own.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppDiscoInfoWatch::watchFeatures(const QStringList &features) const
{
    auto data = std::make_shared<QXmppDiscoFeatureWatch::Data>(*this, features);
    data->supported.setBinding([data = data.get()] {
        const auto info = data->infoWatch.info().value();
        return info && std::ranges::all_of(data->features, [&](const auto &feature) {
                   return info->features().contains(feature);
               });
    });
    return QXmppDiscoFeatureWatch(std::move(data));
}

/*!
    \overload

    Returns a watch on whether the entity supports \a feature.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppDiscoInfoWatch::watchFeature(QXmpp::Namespace feature) const
{
    return watchFeatures({ namespaceUri(feature) });
}

/*!
    \overload

    Returns a watch on whether the entity supports all of \a features.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppDiscoInfoWatch::watchFeatures(const QList<QXmpp::Namespace> &features) const
{
    return watchFeatures(transform<QStringList>(features, namespaceUri));
}

// QXmppDiscoFeatureWatch

/*!
    \class QXmppDiscoFeatureWatch
    \inmodule QXmpp

    \brief Lightweight handle to a watch on whether an entity supports features.

    Returned by QXmppDiscoInfoWatch::watchFeature() and QXmppDiscoInfoWatch::watchFeatures().
    Cheap to copy — all copies share the same underlying state. The feature watch keeps the
    info watch it has been created from alive.

    \since QXmpp 1.17
*/

/*!
    Constructs a watch that is not connected to any entity.

    Its state stays Unknown and the features are never supported.
*/
QXmppDiscoFeatureWatch::QXmppDiscoFeatureWatch()
    : d(std::make_shared<Data>())
{
}

QXmppDiscoFeatureWatch::QXmppDiscoFeatureWatch(std::shared_ptr<Data> d)
    : d(std::move(d))
{
}

/*!
    Returns whether the entity supports all watched features.

    As long as the information has not been received, this is \c false. Check state() to
    distinguish missing support from missing information, e.g. before telling the user that a
    feature is unsupported.

    The information may be outdated in the state QXmppDiscoInfoWatch::State::Stale. If the
    entity responded with an error, no feature is supported.
*/
QBindable<bool> QXmppDiscoFeatureWatch::supported() const
{
    return &d->supported;
}

/*! Returns the state of the information the support is based on. */
QBindable<QXmppDiscoInfoWatch::State> QXmppDiscoFeatureWatch::state() const
{
    return d->infoWatch.state();
}

/*!
    Returns whether the entity supports all watched features, as soon as that is known.

    If the information is Loaded or Stale, the task finishes immediately with the current value.
    Otherwise it finishes once the information has been received, with \c false if the entity
    responded with an error. Until then the information is requested, also after reconnections,
    even if no other copy of the watch exists anymore.

    If the watch is not connected to a QXmppDiscoveryManager, the task finishes immediately with
    \c false. If the manager is destroyed before the information is known, the task is cancelled.

    \code
    if (co_await manager->watchServerSupport().resolve()) {
        // use the feature
    }
    \endcode

    \since QXmpp 1.17
*/
QXmppTask<bool> QXmppDiscoFeatureWatch::resolve() const
{
    // keeps the information requested until the task has finished
    auto watch = *this;
    auto &info = *watch.d->infoWatch.d;
    if (info.manager && !isKnown(info.state.value())) {
        co_await info.waitUntilKnown();
    }
    co_return watch.d->supported.value();
}

QXmppDiscoFeatureWatch QXmpp::Private::watchServerFeature(QXmppClient *client, QXmpp::Namespace feature)
{
    if (auto *disco = client ? client->findExtension<QXmppDiscoveryManager>() : nullptr) {
        return disco->watchServerInfo().watchFeature(feature);
    }
    return {};
}

QXmppDiscoFeatureWatch QXmpp::Private::watchAccountFeature(QXmppClient *client, QXmpp::Namespace feature)
{
    if (auto *disco = client ? client->findExtension<QXmppDiscoveryManager>() : nullptr) {
        return disco->watchAccountInfo().watchFeature(feature);
    }
    return {};
}

void QXmpp::Private::DiscoInfoTracking::setTracked(QXmppClient *client, const QString &jid, bool tracked)
{
    if (auto *disco = client ? client->findExtension<QXmppDiscoveryManager>() : nullptr) {
        disco->d->setTracked(jid, tracked);
    }
}

void QXmpp::Private::DiscoInfoTracking::invalidate(QXmppClient *client, const QString &jid)
{
    if (auto *disco = client ? client->findExtension<QXmppDiscoveryManager>() : nullptr) {
        disco->d->invalidate(jid);
    }
}

void QXmpp::Private::DiscoInfoTracking::reset(QXmppClient *client, const QString &jid)
{
    if (auto *disco = client ? client->findExtension<QXmppDiscoveryManager>() : nullptr) {
        disco->d->reset(jid);
    }
}

// QXmppDiscoServicesWatch

/*! Returns whether all discovery queries have completed. */
QBindable<bool> QXmppDiscoServicesWatch::loaded() const
{
    return &d->loaded;
}

/*! Returns the list of discovered services matching the filter. */
QBindable<QList<QXmppDiscoService>> QXmppDiscoServicesWatch::services() const
{
    return &d->services;
}

/*!
    \brief Watches for server services matching the given identity \a category and optional \a type.

    Returns a lightweight handle that provides reactive access to discovered
    services via QBindable properties. The watch is automatically started and
    will be populated as discovery results arrive.

    \a category is the identity category to filter by. \a type is the optional identity type
    to filter by. If not set, matches any \a type. \a requiredFeatures specifies features that
    discovered services must support.

    Returns a watch handle. Keep it alive as long as you need updates.

    \since QXmpp 1.16
*/
QXmppDiscoServicesWatch QXmppDiscoveryManager::discoverServices(Disco::Category category, std::optional<Disco::Type> type, QStringList requiredFeatures)
{
    std::optional<QString> typeStr;
    if (type) {
        typeStr = Enums::toString(*type);
    }
    return discoverServices(Enums::toString(category), std::move(typeStr), std::move(requiredFeatures));
}

/*!
    \brief Watches for server services matching the given identity category and optional type strings.

    This overload accepts raw strings for non-standard or unregistered categories and types.

    \since QXmpp 1.16

    \a requiredFeatures, \a category, and \a type.
*/
QXmppDiscoServicesWatch QXmppDiscoveryManager::discoverServices(QString category, std::optional<QString> type, QStringList requiredFeatures)
{
    auto data = std::make_shared<QXmppDiscoServicesWatch::Data>();

    QXmppDiscoveryManagerPrivate::WatchEntry entry;
    entry.data = data;
    entry.category = std::move(category);
    entry.type = std::move(type);
    entry.requiredFeatures = std::move(requiredFeatures);

    // If discovery already completed, populate from cached results
    if (d->discoveryState == DiscoveryState::Complete) {
        QList<QXmppDiscoService> matching;
        for (const auto &service : std::as_const(d->discoveredServices)) {
            if (d->matchesFilter(entry, service.info)) {
                matching.append(service);
            }
        }
        data->services = std::move(matching);
        data->loaded = true;
    }

    d->watches.append(std::move(entry));

    QXmppDiscoServicesWatch watch;
    watch.d = std::move(data);

    // Discovery runs on QXmppClient::connected and is skipped while no watch exists, so a
    // watch added after that point would stay empty until the next reconnect.
    if (d->clientConnected && d->discoveryState == DiscoveryState::NotStarted) {
        d->discoverServices();
    }

    return watch;
}

/*!
    \brief Watches the service discovery information of \a jid and \a node.

    Returns a lightweight handle that provides reactive access to the information via
    QBindable properties. All watches on the same entity share their state and the
    information is only requested once.

    The information is requested when the first watch is created and again on every new
    stream. Until the new response arrives, the previous information stays available with
    the state QXmppDiscoInfoWatch::State::Stale. If no other watch on the entity exists, but
    the information has been requested before, e.g. using info(), it is used with the state
    Stale while it is requested again, unless changes of the entity are tracked (see
    QXmppDiscoInfoWatch::changesTracked()).

    Keep the handle alive as long as you need updates.

    \since QXmpp 1.17
*/
QXmppDiscoInfoWatch QXmppDiscoveryManager::watchInfo(const QString &jid, const QString &node)
{
    return d->watchInfo({ QXmppDiscoInfoWatch::Data::Target::Jid, jid, node });
}

/*!
    \brief Watches the service discovery information of the own server.

    Unlike watchInfo() with the server's domain, this watch follows the configured domain,
    so it stays valid after the account has been changed.

    \since QXmpp 1.17
*/
QXmppDiscoInfoWatch QXmppDiscoveryManager::watchServerInfo()
{
    return d->watchInfo({ QXmppDiscoInfoWatch::Data::Target::Server, {}, {} });
}

/*!
    \brief Watches the service discovery information of the own bare JID.

    The account's information contains the features the server provides for the account,
    e.g. \xep{0163}{Personal Eventing Protocol} or \xep{0313}{Message Archive Management}.

    This watch follows the configured JID, so it stays valid after the account has been
    changed.

    \since QXmpp 1.17
*/
QXmppDiscoInfoWatch QXmppDiscoveryManager::watchAccountInfo()
{
    return d->watchInfo({ QXmppDiscoInfoWatch::Data::Target::Account, {}, {} });
}

/*!
    Returns the capabilities node of the local XMPP client.

    By default this is "org.qxmpp.caps".
*/
QString QXmppDiscoveryManager::clientCapabilitiesNode() const
{
    return d->clientCapabilitiesNode;
}

/*!
    Sets the capabilities node of the local XMPP client.

    By default this is "org.qxmpp.caps".

    \a node.
*/
void QXmppDiscoveryManager::setClientCapabilitiesNode(const QString &node)
{
    d->clientCapabilitiesNode = node;
}

QStringList QXmppDiscoveryManager::discoveryFeatures() const
{
    return { ns_disco_info.toString() };
}

bool QXmppDiscoveryManager::handleStanza(const QDomElement &element)
{
    if (handleIqRequests<GetIq<QXmppDiscoInfo>, GetIq<QXmppDiscoItems>>(element, client(), d.get())) {
        return true;
    }

    if (isIqElement<QXmppDiscoveryIq>(element)) {
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QXmppDiscoveryIq receivedIq;
        receivedIq.parse(element);

        switch (receivedIq.type()) {
        case QXmppIq::Get:
            break;
        case QXmppIq::Result:
        case QXmppIq::Error:
            // handle all replies
            if (receivedIq.queryType() == QXmppDiscoveryIq::InfoQuery) {
                Q_EMIT infoReceived(receivedIq);
            } else if (receivedIq.queryType() == QXmppDiscoveryIq::ItemsQuery) {
                Q_EMIT itemsReceived(receivedIq);
            }
            return true;

        case QXmppIq::Set:
            // let other manager handle "set" IQs
            return false;
        }
        QT_WARNING_POP
    }
    return false;
}

void QXmppDiscoveryManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::presenceReceived, this, [this](const QXmppPresence &presence) {
        d->handlePresence(presence);
    });
    connect(client, &QXmppClient::connected, this, [this, client]() {
        d->clientConnected = true;
        const auto newStream = client->streamManagementState() != QXmppClient::ResumedStream;
        if (newStream) {
            d->itemsCache.clear();
            // only watched info is kept, as stale info
            d->recentInfoEntries.clear();
            d->clearTracked();
            d->discoverServices();
        }
        d->refreshInfoWatches(newStream);
    });
    connect(client, &QXmppClient::disconnected, this, [this, client]() {
        d->clientConnected = false;
        // without stream management no changes are received until the next stream
        if (client->streamManagementState() == QXmppClient::NoStreamManagement) {
            d->clearTracked();
        }
        // Queries still in flight will never complete; let a later watch restart discovery.
        if (d->discoveryState == DiscoveryState::Running) {
            d->discoveryState = DiscoveryState::NotStarted;
        }
    });
}

void QXmppDiscoveryManager::onUnregistered(QXmppClient *client)
{
    // Drop the results with the client they belong to; a later registration rediscovers.
    d->clientConnected = false;
    d->discoveryState = DiscoveryState::NotStarted;
    d->discoveredServices.clear();
    d->clearTracked();
    disconnect(client, nullptr, this, nullptr);
}

QString QXmppDiscoveryManagerPrivate::defaultApplicationName()
{
    if (!qApp->applicationName().isEmpty()) {
        if (!qApp->applicationVersion().isEmpty()) {
            return qApp->applicationName() + u' ' + qApp->applicationVersion();
        } else {
            return qApp->applicationName();
        }
    } else {
        return u"QXmpp " + QXmppVersion();
    }
}

QXmppDiscoIdentity QXmppDiscoveryManagerPrivate::defaultIdentity()
{
    return QXmppDiscoIdentity {
        u"client"_s,
#if defined Q_OS_ANDROID || defined Q_OS_BLACKBERRY || defined Q_OS_IOS || defined Q_OS_WP
        u"phone"_s,
#else
        u"pc"_s,
#endif
        defaultApplicationName(),
    };
}

std::variant<CompatIq<QXmppDiscoInfo>, QXmppStanza::Error> QXmppDiscoveryManagerPrivate::handleIq(GetIq<QXmppDiscoInfo> &&iq)
{
    if (iq.payload.node().isEmpty() || iq.payload.node().startsWith(clientCapabilitiesNode)) {
        return CompatIq { QXmppIq::Result, q->buildClientInfo() };
    }
    return StanzaError(StanzaError::Cancel, StanzaError::ItemNotFound, u"Unknown node."_s);
}

std::variant<CompatIq<QXmppDiscoItems>, QXmppStanza::Error> QXmppDiscoveryManagerPrivate::handleIq(GetIq<QXmppDiscoItems> &&iq)
{
    if (iq.payload.node().isEmpty() || iq.payload.node().startsWith(clientCapabilitiesNode)) {
        return CompatIq { QXmppIq::Result, QXmppDiscoItems() };
    }
    return StanzaError(StanzaError::Cancel, StanzaError::ItemNotFound, u"Unknown node."_s);
}

QXmppDiscoInfoWatch QXmppDiscoveryManagerPrivate::watchInfo(QXmppDiscoInfoWatch::Data::Key &&key)
{
    if (auto itr = infoWatches.find(key); itr != infoWatches.end()) {
        // an expired watch may still be in the map while it is being destroyed
        if (auto data = itr->second.lock()) {
            return QXmppDiscoInfoWatch(std::move(data));
        }
    }

    auto data = std::make_shared<QXmppDiscoInfoWatch::Data>();
    data->manager = q;
    data->key = std::move(key);
    infoWatches.insert_or_assign(data->key, data);

    if (data->key.target == QXmppDiscoInfoWatch::Data::Target::Jid) {
        auto entry = this->entry(data->key.jid, data->key.node);
        const auto firstWatch = entry->watchCount == 0;
        data->setEntry(entry);

        // Cached info of untracked entities may be outdated. Server and account info are only
        // requested on new streams.
        if (firstWatch && !entry->changesTracked.value()) {
            fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Strict);
        } else {
            fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Relaxed);
        }
    } else {
        updateEntry(*data);
        if (data->entry) {
            fetchInfo(data->entry, QXmppDiscoveryManager::CachePolicy::Relaxed);
        }
    }
    return QXmppDiscoInfoWatch(std::move(data));
}

std::shared_ptr<DiscoInfoEntry> QXmppDiscoveryManagerPrivate::findEntry(const QString &jid, const QString &node) const
{
    return infoEntries.value({ jid, node }).lock();
}

std::shared_ptr<DiscoInfoEntry> QXmppDiscoveryManagerPrivate::entry(const QString &jid, const QString &node)
{
    if (auto entry = findEntry(jid, node)) {
        return entry;
    }

    auto entry = std::make_shared<DiscoInfoEntry>();
    entry->manager = this;
    entry->jid = jid;
    entry->node = node;
    entry->changesTracked = node.isEmpty() && isTracked(jid);
    infoEntries.insert({ jid, node }, entry);
    return entry;
}

void QXmppDiscoveryManagerPrivate::storeInfo(const QString &jid, const QString &node, const QXmppDiscoInfo &info)
{
    auto entry = this->entry(jid, node);
    recentInfoEntries.insert({ jid, node }, new std::shared_ptr { entry });

    Qt::beginPropertyUpdateGroup();
    entry->info = info;
    entry->state = QXmppDiscoInfoWatch::State::Loaded;
    Qt::endPropertyUpdateGroup();
}

bool QXmppDiscoveryManagerPrivate::isTracked(const QString &jid) const
{
    if (trackedJids.contains(jid)) {
        return true;
    }
    // changes of the caps are announced by presence
    auto itr = availableJids.constFind(jid);
    return itr != availableJids.cend() && itr->has_value();
}

void QXmppDiscoveryManagerPrivate::setTracked(const QString &jid, bool tracked)
{
    if (tracked) {
        trackedJids.insert(jid);
    } else {
        trackedJids.remove(jid);
    }
    if (auto entry = findEntry(jid, {})) {
        entry->changesTracked = isTracked(jid);
    }
}

void QXmppDiscoveryManagerPrivate::clearTracked()
{
    trackedJids.clear();
    availableJids.clear();

    Qt::beginPropertyUpdateGroup();
    for (const auto &entry : lockInfoEntries()) {
        entry->changesTracked = false;
    }
    Qt::endPropertyUpdateGroup();
}

void QXmppDiscoveryManagerPrivate::invalidate(const QString &jid)
{
    if (auto entry = findEntry(jid, {})) {
        entry->changesTracked = isTracked(jid);
        if (entry->watchCount > 0) {
            fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Strict);
        } else {
            recentInfoEntries.remove({ jid, {} });
        }
    }
}

void QXmppDiscoveryManagerPrivate::reset(const QString &jid)
{
    trackedJids.remove(jid);
    availableJids.remove(jid);
    dropInfo(jid);
}

void QXmppDiscoveryManagerPrivate::dropInfo(const QString &jid)
{
    if (auto entry = findEntry(jid, {})) {
        if (entry->watchCount == 0) {
            recentInfoEntries.remove({ jid, {} });
            return;
        }

        Qt::beginPropertyUpdateGroup();
        entry->changesTracked = isTracked(jid);
        entry->info = std::nullopt;
        entry->state = QXmppDiscoInfoWatch::State::Unknown;
        fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Strict);
        Qt::endPropertyUpdateGroup();
    }
}

void QXmppDiscoveryManagerPrivate::handlePresence(const QXmppPresence &presence)
{
    const auto jid = presence.from();
    if (QXmppUtils::jidToResource(jid).isEmpty()) {
        return;
    }

    if (presence.type() == QXmppPresence::Unavailable) {
        reset(jid);
        return;
    }
    if (presence.type() != QXmppPresence::Available) {
        return;
    }

    std::optional<Caps> caps;
    if (!presence.capabilityHash().isEmpty() && !presence.capabilityNode().isEmpty() && !presence.capabilityVer().isEmpty()) {
        caps = Caps { presence.capabilityHash(), presence.capabilityNode(), presence.capabilityVer() };
    }

    Qt::beginPropertyUpdateGroup();
    if (auto itr = availableJids.find(jid); itr == availableJids.end()) {
        // The JID may refer to another entity than before, e.g. a MUC occupant.
        availableJids.insert(jid, caps);
        dropInfo(jid);
    } else if (caps && caps != *itr) {
        // Presences without caps do not mean that the caps have been removed, servers may
        // strip unchanged caps (caps optimization).
        *itr = caps;
        invalidate(jid);
    }
    Qt::endPropertyUpdateGroup();
}

QXmppTask<Result<QXmppDiscoInfo>> QXmppDiscoveryManagerPrivate::capsInfo(const QString &jid, const Caps &caps)
{
    if (capsHashAlgorithm(caps.hash)) {
        if (auto *cachedInfo = capsCache[{ caps.hash, caps.ver }]) {
            applyCapsInfo(jid, caps, *cachedInfo);
            return makeReadyTask<Result<QXmppDiscoInfo>>(QXmppDiscoInfo { *cachedInfo });
        }
    }

    const auto node = caps.node + u'#' + QString::fromLatin1(caps.ver.toBase64());
    return infoRequests.produce(
        { jid, node },
        [this, caps](const auto &key) { return startCapsRequest(std::get<0>(key), caps); },
        q);
}

QXmppTask<Result<QXmppDiscoInfo>> QXmppDiscoveryManagerPrivate::startCapsRequest(const QString &jid, const Caps &caps)
{
    // wait for another entity with the same caps
    if (capsHashAlgorithm(caps.hash)) {
        auto [itr, inserted] = capsRequests.try_emplace(CapsKey { caps.hash, caps.ver });
        if (!inserted) {
            QXmppPromise<Result<QXmppDiscoInfo>> promise;
            auto task = promise.task();
            itr->second.push_back({ jid, caps, std::move(promise) });
            return task;
        }
    }
    return requestCapsInfo(jid, caps);
}

QXmppTask<Result<QXmppDiscoInfo>> QXmppDiscoveryManagerPrivate::requestCapsInfo(const QString &jid, const Caps &caps)
{
    const auto node = caps.node + u'#' + QString::fromLatin1(caps.ver.toBase64());
    return chain<Result<QXmppDiscoInfo>>(
        q->client()->sendIq(CompatIq { GetIq<QXmppDiscoInfo> { generateSequentialStanzaId(), {}, jid, {}, QXmppDiscoInfo { node } } }),
        q,
        [this, jid, caps](QXmppClient::IqResult &&response) -> Result<QXmppDiscoInfo> {
            const auto algorithm = capsHashAlgorithm(caps.hash);
            const auto iqElement = std::holds_alternative<QDomElement>(response) ? std::get<QDomElement>(response) : QDomElement();

            auto result = parseIqResponseFlat<QXmppDiscoInfo>(std::move(response));
            if (!hasValue(result)) {
                if (algorithm) {
                    finishCapsRequest({ caps.hash, caps.ver }, {});
                }
                return result;
            }

            const auto &info = getValue(result);
            if (algorithm) {
                // Unverified info is only used for this entity. Other entities with the same
                // caps are requested themselves.
                const auto query = firstChildElement(iqElement, u"query", ns_disco_info);
                if (capsVerificationString(query, *algorithm) == caps.ver) {
                    capsCache.insert({ caps.hash, caps.ver }, new QXmppDiscoInfo { info });
                    applyCapsInfo(jid, caps, info);
                    finishCapsRequest({ caps.hash, caps.ver }, info);
                    return result;
                }
                q->warning(u"Received service discovery information of %1 does not match its entity capabilities."_s.arg(jid));
                finishCapsRequest({ caps.hash, caps.ver }, {});
            }
            applyCapsInfo(jid, caps, info);
            return result;
        });
}

void QXmppDiscoveryManagerPrivate::finishCapsRequest(const CapsKey &key, const std::optional<QXmppDiscoInfo> &info)
{
    auto node = capsRequests.extract(key);
    if (node.empty()) {
        return;
    }

    for (auto &waiter : node.mapped()) {
        if (info) {
            applyCapsInfo(waiter.jid, waiter.caps, *info);
            waiter.promise.finish(*info);
        } else {
            startCapsRequest(waiter.jid, waiter.caps).then(q, [promise = std::move(waiter.promise)](auto &&result) mutable {
                promise.finish(std::move(result));
            });
        }
    }
}

void QXmppDiscoveryManagerPrivate::applyCapsInfo(const QString &jid, const Caps &caps, const QXmppDiscoInfo &info)
{
    // the caps may have changed in the meantime
    if (availableJids.value(jid) == caps) {
        storeInfo(jid, {}, info);
    }
}

// Watches can be destroyed or created by user code, which modifies the map.
std::vector<std::shared_ptr<QXmppDiscoInfoWatch::Data>> QXmppDiscoveryManagerPrivate::lockInfoWatches() const
{
    std::vector<std::shared_ptr<QXmppDiscoInfoWatch::Data>> watches;
    watches.reserve(infoWatches.size());
    for (const auto &[key, watch] : infoWatches) {
        if (auto data = watch.lock()) {
            watches.push_back(std::move(data));
        }
    }
    return watches;
}

std::vector<std::shared_ptr<DiscoInfoEntry>> QXmppDiscoveryManagerPrivate::lockInfoEntries() const
{
    std::vector<std::shared_ptr<DiscoInfoEntry>> entries;
    entries.reserve(infoEntries.size());
    for (const auto &entry : infoEntries) {
        if (auto locked = entry.lock()) {
            entries.push_back(std::move(locked));
        }
    }
    return entries;
}

QString QXmppDiscoveryManagerPrivate::resolveJid(const QXmppDiscoInfoWatch::Data &data) const
{
    using enum QXmppDiscoInfoWatch::Data::Target;
    if (data.key.target == Jid) {
        return data.key.jid;
    }
    if (auto *client = q->client()) {
        return data.key.target == Server ? client->configuration().domain() : client->configuration().jidBare();
    }
    return {};
}

// Watches on the server or account info follow the configured account.
void QXmppDiscoveryManagerPrivate::updateEntry(QXmppDiscoInfoWatch::Data &data)
{
    if (data.key.target == QXmppDiscoInfoWatch::Data::Target::Jid) {
        return;
    }
    const auto jid = resolveJid(data);
    if (!data.entry || data.entry->jid != jid) {
        data.setEntry(jid.isEmpty() ? nullptr : entry(jid, {}));
    }
}

void QXmppDiscoveryManagerPrivate::fetchInfo(const std::shared_ptr<DiscoInfoEntry> &entry, QXmppDiscoveryManager::CachePolicy cachePolicy)
{
    using State = QXmppDiscoInfoWatch::State;

    if (cachePolicy == QXmppDiscoveryManager::CachePolicy::Relaxed && entry->state.value() == State::Loaded) {
        return;
    }

    if (entry->state.value() == State::Loaded) {
        entry->state = State::Stale;
    }
    if (!clientConnected) {
        return;
    }
    if (entry->state.value() == State::Unknown || entry->state.value() == State::Error) {
        entry->state = State::Loading;
    }

    // successful responses are stored by info()
    q->info(entry->jid, entry->node, QXmppDiscoveryManager::CachePolicy::Strict).then(q, [weakEntry = std::weak_ptr(entry)](auto &&result) {
        auto entry = weakEntry.lock();
        if (!entry || hasValue(result)) {
            return;
        }

        if (getError(result).isStanzaError()) {
            Qt::beginPropertyUpdateGroup();
            entry->info = std::nullopt;
            entry->state = State::Error;
            Qt::endPropertyUpdateGroup();
        } else if (entry->state.value() == State::Loading) {
            // not an answer from the entity, e.g. the connection has been lost
            entry->state = State::Unknown;
        }
    });
}

void QXmppDiscoveryManagerPrivate::refreshInfoWatches(bool newStream)
{
    using State = QXmppDiscoInfoWatch::State;

    // Sending requests emits signals (e.g. for logging), which are not deferred by the update
    // group, so this iterates over a snapshot.
    Qt::beginPropertyUpdateGroup();

    if (newStream) {
        // info of the previous account is released and does not show up as stale info
        for (const auto &data : lockInfoWatches()) {
            updateEntry(*data);
        }
    }
    for (const auto &entry : lockInfoEntries()) {
        if (entry->watchCount == 0) {
            continue;
        }
        if (newStream) {
            fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Strict);
        } else if (entry->state.value() == State::Unknown || entry->state.value() == State::Stale) {
            // requests of the resumed stream may have failed while disconnected
            fetchInfo(entry, QXmppDiscoveryManager::CachePolicy::Strict);
        }
    }

    Qt::endPropertyUpdateGroup();
}

void QXmppDiscoveryManagerPrivate::discoverServices()
{
    pruneExpiredWatches();

    if (watches.isEmpty()) {
        return;
    }

    discoveryState = DiscoveryState::Running;
    discoveredServices.clear();

    // Reset all live watches
    for (auto &watch : watches) {
        if (auto data = watch.data.lock()) {
            data->loaded = false;
            data->services = QList<QXmppDiscoService>();
        }
    }

    auto serverDomain = q->client()->configuration().domain();
    q->items(serverDomain).then(q, [this](auto &&result) {
        if (!hasValue(result)) {
            finalizeDiscovery();
            return;
        }

        const auto &itemList = getValue(result);
        if (itemList.isEmpty()) {
            finalizeDiscovery();
            return;
        }

        auto remaining = std::make_shared<int>(itemList.size());

        for (const auto &item : itemList) {
            q->info(item.jid()).then(q, [this, jid = item.jid(), remaining](auto &&infoResult) {
                if (hasValue(infoResult)) {
                    processServiceInfo(jid, getValue(infoResult));
                }

                if (--(*remaining) == 0) {
                    finalizeDiscovery();
                }
            });
        }
    });
}

void QXmppDiscoveryManagerPrivate::processServiceInfo(const QString &jid, const QXmppDiscoInfo &info)
{
    discoveredServices.append(QXmppDiscoService { jid, info });

    for (auto &watch : watches) {
        if (auto data = watch.data.lock()) {
            if (matchesFilter(watch, info)) {
                auto list = data->services.value();
                list.append(QXmppDiscoService { jid, info });
                data->services = std::move(list);
            }
        }
    }
}

void QXmppDiscoveryManagerPrivate::finalizeDiscovery()
{
    discoveryState = DiscoveryState::Complete;
    for (auto &watch : watches) {
        if (auto data = watch.data.lock()) {
            data->loaded = true;
        }
    }
}

void QXmppDiscoveryManagerPrivate::pruneExpiredWatches()
{
    watches.removeIf([](const WatchEntry &entry) {
        return entry.data.expired();
    });
}

bool QXmppDiscoveryManagerPrivate::matchesFilter(const WatchEntry &watch, const QXmppDiscoInfo &info) const
{
    // Check that at least one identity matches category (and optionally type)
    bool identityMatch = false;
    for (const auto &identity : info.identities()) {
        if (identity.category() != watch.category) {
            continue;
        }
        if (watch.type && identity.type() != *watch.type) {
            continue;
        }
        identityMatch = true;
        break;
    }

    if (!identityMatch) {
        return false;
    }

    // Check that all required features are present
    const auto &features = info.features();
    for (const auto &required : watch.requiredFeatures) {
        if (!features.contains(required)) {
            return false;
        }
    }

    return true;
}

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
#include "QXmppUtils.h"

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
    d->infoCache.setMaxCost(50);
    d->itemsCache.setMaxCost(50);
    d->clientCapabilitiesNode = u"org.qxmpp.caps"_s;
    d->identities = { d->defaultIdentity() };
}

QXmppDiscoveryManager::~QXmppDiscoveryManager()
{
    for (const auto &data : d->lockInfoWatches()) {
        data->manager = nullptr;
    }
}

/*!
    Fetches discovery info from the specified XMPP entity.

    \since QXmpp 1.12

    \a cachePolicy, \a node, and \a jid.
*/
QXmppTask<Result<QXmppDiscoInfo>> QXmppDiscoveryManager::info(const QString &jid, const QString &node, CachePolicy cachePolicy)
{
    if (cachePolicy == CachePolicy::Relaxed) {
        if (auto *cachedInfo = d->infoCache[{ jid, node }]) {
            return makeReadyTask<Result<QXmppDiscoInfo>>(*cachedInfo);
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
                        d->infoCache.insert({ jid, node }, new QXmppDiscoInfo { getValue(result) });
                        d->updateInfoWatches(jid, node, getValue(result));
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

QXmppDiscoInfoWatch::Data::~Data()
{
    if (manager) {
        // the key may already belong to a new watch
        auto &watches = manager->d->infoWatches;
        if (auto itr = watches.find(key); itr != watches.end() && itr->second.expired()) {
            watches.erase(itr);
        }
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
    Requests the information again, even if it is up to date.

    Existing information stays available with the state Stale until the response arrives.
*/
void QXmppDiscoInfoWatch::refresh()
{
    if (d->manager) {
        d->manager->d->fetchInfo(d, QXmppDiscoveryManager::CachePolicy::Strict);
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
    the state QXmppDiscoInfoWatch::State::Stale.

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
    connect(client, &QXmppClient::connected, this, [this, client]() {
        d->clientConnected = true;
        const auto newStream = client->streamManagementState() != QXmppClient::ResumedStream;
        if (newStream) {
            d->itemsCache.clear();
            d->infoCache.clear();
            d->discoverServices();
        }
        d->refreshInfoWatches(newStream);
    });
    connect(client, &QXmppClient::disconnected, this, [this]() {
        d->clientConnected = false;
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

    fetchInfo(data, QXmppDiscoveryManager::CachePolicy::Relaxed);
    return QXmppDiscoInfoWatch(std::move(data));
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

void QXmppDiscoveryManagerPrivate::fetchInfo(const std::shared_ptr<QXmppDiscoInfoWatch::Data> &data, QXmppDiscoveryManager::CachePolicy cachePolicy)
{
    using State = QXmppDiscoInfoWatch::State;

    const auto jid = resolveJid(*data);
    if (jid.isEmpty()) {
        return;
    }

    if (cachePolicy == QXmppDiscoveryManager::CachePolicy::Relaxed) {
        if (auto *cachedInfo = infoCache[{ jid, data->key.node }]) {
            updateInfoWatches(jid, data->key.node, *cachedInfo);
            return;
        }
    }

    if (data->state == State::Loaded) {
        data->state = State::Stale;
    }
    if (!clientConnected) {
        return;
    }
    if (data->state == State::Unknown || data->state == State::Error) {
        data->state = State::Loading;
    }

    // successful responses are applied to all watches by info()
    q->info(jid, data->key.node, QXmppDiscoveryManager::CachePolicy::Strict).then(q, [this, weakData = std::weak_ptr(data), jid](auto &&result) {
        auto data = weakData.lock();
        if (!data || hasValue(result) || resolveJid(*data) != jid) {
            return;
        }

        if (getError(result).isStanzaError()) {
            Qt::beginPropertyUpdateGroup();
            data->infoJid = jid;
            data->info = std::nullopt;
            data->state = State::Error;
            Qt::endPropertyUpdateGroup();
        } else if (data->state == State::Loading) {
            // not an answer from the entity, e.g. the connection has been lost
            data->state = State::Unknown;
        }
    });
}

void QXmppDiscoveryManagerPrivate::updateInfoWatches(const QString &jid, const QString &node, const QXmppDiscoInfo &info)
{
    using enum QXmppDiscoInfoWatch::Data::Target;

    // no user code can modify the map while the notifications are deferred
    Qt::beginPropertyUpdateGroup();

    auto update = [&](QXmppDiscoInfoWatch::Data::Key &&key) {
        if (auto itr = infoWatches.find(key); itr != infoWatches.end()) {
            if (auto data = itr->second.lock(); data && resolveJid(*data) == jid) {
                data->infoJid = jid;
                data->info = info;
                data->state = QXmppDiscoInfoWatch::State::Loaded;
            }
        }
    };
    update({ Jid, jid, node });
    if (node.isEmpty()) {
        update({ Server, {}, {} });
        update({ Account, {}, {} });
    }

    Qt::endPropertyUpdateGroup();
}

void QXmppDiscoveryManagerPrivate::refreshInfoWatches(bool newStream)
{
    using State = QXmppDiscoInfoWatch::State;

    // Sending requests emits signals (e.g. for logging), which are not deferred by the update
    // group, so this iterates over a snapshot.
    Qt::beginPropertyUpdateGroup();

    for (const auto &data : lockInfoWatches()) {
        if (newStream) {
            // information of the previous account must not show up as stale information
            if (data->info.value() && data->infoJid != resolveJid(*data)) {
                data->info = std::nullopt;
                data->state = State::Unknown;
            }
            fetchInfo(data, QXmppDiscoveryManager::CachePolicy::Strict);
        } else if (data->state == State::Unknown || data->state == State::Stale) {
            // requests of the resumed stream may have failed while disconnected
            fetchInfo(data, QXmppDiscoveryManager::CachePolicy::Strict);
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

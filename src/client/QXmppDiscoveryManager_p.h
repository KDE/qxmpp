// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_P_H
#define QXMPPDISCOVERYMANAGER_P_H

#include "QXmppDiscoveryManager.h"
#include "QXmppPromise.h"

#include "Async.h"
#include "Iq.h"

#include <unordered_map>

#include <QCache>

using namespace QXmpp::Private;

struct QXmppDiscoInfoWatch::Data {
    enum class Target {
        Jid,
        Server,
        Account,
    };

    struct Key {
        Target target;
        QString jid;
        QString node;

        bool operator==(const Key &) const = default;
    };

    struct KeyHash {
        size_t operator()(const Key &key) const noexcept
        {
            return qHashMulti(0, key.target, key.jid, key.node);
        }
    };

    ~Data();

    QXmppTask<void> waitUntilKnown();

    // reset by the manager on destruction
    QXmppDiscoveryManager *manager = nullptr;
    Key key;
    // JID the current info belongs to; differs from the resolved JID after an account change
    QString infoJid;

    QProperty<State> state { State::Unknown };
    QProperty<std::optional<QXmppDiscoInfo>> info;
    QProperty<bool> changesTracked;

    // finished once the state is Loaded, Stale or Error
    std::vector<QXmppPromise<void>> knownPromises;
    std::optional<QPropertyNotifier> knownNotifier;
};

struct QXmppDiscoFeatureWatch::Data {
    QXmppDiscoInfoWatch infoWatch;
    QStringList features;
    QProperty<bool> supported;
};

class QXmppDiscoveryManagerPrivate
{
public:
    using StanzaError = QXmppStanza::Error;

    struct WatchEntry {
        std::weak_ptr<QXmppDiscoServicesWatch::Data> data;
        QString category;
        std::optional<QString> type;
        QStringList requiredFeatures;
    };

    QXmppDiscoveryManager *q = nullptr;
    QString clientCapabilitiesNode;
    QList<QXmppDiscoIdentity> identities;
    QList<QXmppDataForm> dataForms;

    // cached data
    QCache<std::tuple<QString, QString>, QXmppDiscoInfo> infoCache;
    QCache<std::tuple<QString, QString>, QList<QXmppDiscoItem>> itemsCache;

    // outgoing requests
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QXmppDiscoInfo>> infoRequests;
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QList<QXmppDiscoItem>>> itemsRequests;

    // info watches
    std::unordered_map<QXmppDiscoInfoWatch::Data::Key, std::weak_ptr<QXmppDiscoInfoWatch::Data>, QXmppDiscoInfoWatch::Data::KeyHash> infoWatches;
    // JIDs whose changes are reported by other managers, e.g. joined MUC rooms
    QSet<QString> trackedJids;

    // service watches
    QList<WatchEntry> watches;
    QList<QXmppDiscoService> discoveredServices;

    // Service discovery is a per-connection state machine: it starts on connect, or when the
    // first watch appears on an already connected client, and ends once every info query has
    // been answered.
    enum class DiscoveryState {
        NotStarted,
        Running,
        Complete,
    };
    DiscoveryState discoveryState = DiscoveryState::NotStarted;
    bool clientConnected = false;

    explicit QXmppDiscoveryManagerPrivate(QXmppDiscoveryManager *q) : q(q) { }

    static QString defaultApplicationName();
    static QXmppDiscoIdentity defaultIdentity();

    std::variant<CompatIq<QXmppDiscoInfo>, StanzaError> handleIq(GetIq<QXmppDiscoInfo> &&iq);
    std::variant<CompatIq<QXmppDiscoItems>, StanzaError> handleIq(GetIq<QXmppDiscoItems> &&iq);

    QXmppDiscoInfoWatch watchInfo(QXmppDiscoInfoWatch::Data::Key &&key);
    std::shared_ptr<QXmppDiscoInfoWatch::Data> findJidWatch(const QString &jid) const;
    void setTracked(const QString &jid, bool tracked);
    void clearTracked();
    void invalidate(const QString &jid);
    void reset(const QString &jid);
    std::vector<std::shared_ptr<QXmppDiscoInfoWatch::Data>> lockInfoWatches() const;
    QString resolveJid(const QXmppDiscoInfoWatch::Data &data) const;
    void fetchInfo(const std::shared_ptr<QXmppDiscoInfoWatch::Data> &data, QXmppDiscoveryManager::CachePolicy cachePolicy);
    void updateInfoWatches(const QString &jid, const QString &node, const QXmppDiscoInfo &info);
    void refreshInfoWatches(bool newStream);

    void discoverServices();
    void processServiceInfo(const QString &jid, const QXmppDiscoInfo &info);
    void finalizeDiscovery();
    void pruneExpiredWatches();
    bool matchesFilter(const WatchEntry &watch, const QXmppDiscoInfo &info) const;
};

namespace QXmpp::Private {

// Feature watches for managers. Return a watch that never loads if the client is null or has no
// QXmppDiscoveryManager.
QXmppDiscoFeatureWatch watchServerFeature(QXmppClient *client, QXmpp::Namespace feature);
QXmppDiscoFeatureWatch watchAccountFeature(QXmppClient *client, QXmpp::Namespace feature);

// Used by managers that know when the info of an entity changes.
struct DiscoInfoTracking {
    // Marks the info of \a jid as tracked, i.e. the caller reports all changes, e.g. using
    // invalidate(). This does not request the info, the caller is responsible for fetching it
    // when tracking starts. Tracking ends automatically on new streams.
    static void setTracked(QXmppClient *client, const QString &jid, bool tracked);
    // Drops the cached info of \a jid and requests it again if it is watched.
    static void invalidate(QXmppClient *client, const QString &jid);
    // Like invalidate(), but for JIDs that may now refer to another entity, e.g. MUC occupants.
    // Watches do not keep the old info and changes are no longer tracked.
    static void reset(QXmppClient *client, const QString &jid);
};

}  // namespace QXmpp::Private

#endif  // QXMPPDISCOVERYMANAGER_P_H

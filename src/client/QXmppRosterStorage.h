// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPROSTERSTORAGE_H
#define QXMPPROSTERSTORAGE_H

#include "QXmppGlobal.h"
#include "QXmppRosterIq.h"

#include <vector>

#include <QString>

template<typename T>
class QXmppTask;

///
/// \brief Storage backend used by QXmppRosterManager to cache the roster between
/// sessions (RFC 6121 §2.6 Roster Versioning).
///
/// Implementations are responsible for persisting the version string returned by
/// the server together with the current set of roster items. QXmppRosterManager
/// owns a default in-memory implementation (QXmppRosterMemoryStorage); callers
/// that want real persistence implement this interface and pass an instance via
/// QXmppRosterManager::setStorage() before connecting to the server.
///
/// All operations are asynchronous so that disk- or network-backed
/// implementations can run off the GUI thread. Each mutating operation carries
/// the new roster version so that the backend can persist the item change and
/// the version atomically.
///
/// \par Ordering
/// Implementations MUST apply mutations to persistent state in the order they
/// are invoked on a given instance. The returned QXmppTask values may complete
/// in any order — only the visible side effect needs to be ordered. This is
/// because consecutive operations are not commutative: a later upsert for the
/// same JID, or a later version, must not be overwritten by an earlier one
/// landing afterwards. A typical implementation funnels all writes through a
/// single worker / serialized queue.
///
/// The storage instance is scoped to a single account. Multi-account
/// applications should either instantiate one storage object per
/// QXmppRosterManager or implement their own JID-scoped backend.
///
/// \ingroup Managers
///
/// \since QXmpp 1.16
///
class QXMPP_EXPORT QXmppRosterStorage
{
public:
    /// Snapshot of the persisted roster: server version + items.
    struct RosterCache {
        /// Last roster version reported by the server, or an empty string if no
        /// roster has been received yet.
        QString version;
        /// Roster items, keyed intrinsically by \c item.bareJid().
        std::vector<QXmppRosterIq::Item> items;
    };

    virtual ~QXmppRosterStorage();

    /// Loads the persisted roster.
    virtual QXmppTask<RosterCache> load() = 0;

    /// Replaces every persisted item with the given list and stores the new version.
    /// Called when the server returns a full roster snapshot.
    virtual QXmppTask<void> replaceAll(const QString &version,
                                       const std::vector<QXmppRosterIq::Item> &items) = 0;

    /// Inserts or replaces a single item and stores the new version.
    /// Called for roster pushes with subscription != Remove.
    virtual QXmppTask<void> upsertItem(const QString &version,
                                       const QXmppRosterIq::Item &item) = 0;

    /// Removes the item with the given bare JID (if present) and stores the new version.
    /// Called for roster pushes with subscription == Remove.
    virtual QXmppTask<void> removeItem(const QString &version,
                                       const QString &bareJid) = 0;

    /// Wipes all persisted data (items and version). Used on account switch or
    /// explicit logout.
    virtual QXmppTask<void> clear() = 0;
};

#endif  // QXMPPROSTERSTORAGE_H

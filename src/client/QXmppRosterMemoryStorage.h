// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPROSTERMEMORYSTORAGE_H
#define QXMPPROSTERMEMORYSTORAGE_H

#include "QXmppRosterStorage.h"

#include <memory>

class QXmppRosterMemoryStoragePrivate;

///
/// \brief In-memory default implementation of QXmppRosterStorage.
///
/// Used internally by QXmppRosterManager when no external storage has been
/// configured. Data is kept only for the lifetime of the instance — pass a
/// custom QXmppRosterStorage to QXmppRosterManager::setStorage() to persist the
/// roster across sessions.
///
/// \since QXmpp 1.16
///
class QXMPP_EXPORT QXmppRosterMemoryStorage : public QXmppRosterStorage
{
public:
    QXmppRosterMemoryStorage();
    ~QXmppRosterMemoryStorage() override;

    /// \cond
    QXmppTask<RosterCache> load() override;
    QXmppTask<void> replaceAll(const QString &version,
                               const std::vector<QXmppRosterIq::Item> &items) override;
    QXmppTask<void> upsertItem(const QString &version,
                               const QXmppRosterIq::Item &item) override;
    QXmppTask<void> removeItem(const QString &version,
                               const QString &bareJid) override;
    QXmppTask<void> clear() override;
    /// \endcond

private:
    std::unique_ptr<QXmppRosterMemoryStoragePrivate> d;
};

#endif  // QXMPPROSTERMEMORYSTORAGE_H

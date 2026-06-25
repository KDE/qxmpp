// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPROSTERMEMORYSTORAGE_H
#define QXMPPROSTERMEMORYSTORAGE_H

#include "QXmppRosterStorage.h"

#include <memory>

class QXmppRosterMemoryStoragePrivate;

/*!
    \class QXmppRosterMemoryStorage
    \inmodule QXmpp
    \brief In-memory default implementation of QXmppRosterStorage.

    Used internally by QXmppRosterManager when no external storage has been
    configured. Data is kept only for the lifetime of the instance — pass a
    custom QXmppRosterStorage to QXmppRosterManager::setStorage() to persist the
    roster across sessions.

    \ingroup Managers
    \since QXmpp 1.16
*/
class QXMPP_EXPORT QXmppRosterMemoryStorage : public QXmppRosterStorage
{
public:
    QXmppRosterMemoryStorage();
    ~QXmppRosterMemoryStorage() override;

    QXmppTask<RosterCache> load() override;
    QXmppTask<void> replaceAll(const QString &version,
                               const std::vector<QXmppRosterIq::Item> &items) override;
    QXmppTask<void> upsertItem(const QString &version,
                               const QXmppRosterIq::Item &item) override;
    QXmppTask<void> removeItem(const QString &version,
                               const QString &bareJid) override;
    QXmppTask<void> clear() override;

private:
    std::unique_ptr<QXmppRosterMemoryStoragePrivate> d;
};

#endif  // QXMPPROSTERMEMORYSTORAGE_H

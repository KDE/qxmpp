// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHOOKS_P_H
#define QXMPPHOOKS_P_H

#include "QXmppHooks.h"

#include <vector>

#include <QHash>
#include <QObject>

namespace QXmpp::Private {

// Handlers of all hooks of one client (or server).
//
// The names of hooks have static storage duration (Hook::Name), they are used as keys without a
// copy.
//
// Running a hook works on a snapshot of its handler list, so handlers can add and remove handlers
// while the hook is running. Removed handlers are not run anymore, added handlers are first run
// with the next run of the hook.
class QXMPP_PRIVATE_EXPORT HookRegistry : public QObject
{
public:
    // \a key is null for hooks without key. For single responder hooks, a warning is logged if
    // \a warnDuplicate is set and there already is a handler.
    void add(QStringView name, const QString &key, const std::type_info &signature, int sequence, QObject *owner, std::shared_ptr<void> function, bool singleResponder, bool warnDuplicate);
    void addAll(const HookRegistry &other);
    bool run(QStringView name, const QString &key, const std::type_info &signature, const std::function<bool(void *)> &invoke) const;
    // For single responder hooks, only the first handler is run.
    QXmppTask<bool> runAsync(QStringView name, const QString &key, const std::type_info &signature, std::function<QXmppTask<bool>(void *)> invoke, bool singleResponder) const;
    // \a key std::nullopt checks all keys
    bool hasHandlers(QStringView name, const std::optional<QString> &key) const;
    void removeAll(QObject *owner);

    struct Entry {
        int sequence;
        QObject *owner;
        // name of the signature's type_info, compared by value to work across shared libraries
        const char *signature;
        std::shared_ptr<void> function;
        bool singleResponder = false;
        bool removed = false;
    };
    using EntryList = std::vector<std::shared_ptr<Entry>>;

private:
    struct Hook {
        // name of the signature's type_info, the same for all handlers of the hook
        const char *signature = nullptr;
        // handlers per key, the key is null for hooks without key
        QHash<QString, std::shared_ptr<const EntryList>> handlers;
    };

    void add(QStringView name, const QString &key, const char *signature, int sequence, QObject *owner, std::shared_ptr<void> function, bool singleResponder, bool warnDuplicate);
    std::shared_ptr<const EntryList> entries(QStringView name, const QString &key, const char *signature) const;

    QHash<QString, Hook> m_hooks;
    QHash<QObject *, QMetaObject::Connection> m_owners;
};

}  // namespace QXmpp::Private

#endif  // QXMPPHOOKS_P_H

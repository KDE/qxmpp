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
    void add(QStringView name, const std::type_info &signature, int sequence, QObject *owner, std::shared_ptr<void> function);
    void addAll(const HookRegistry &other);
    bool run(QStringView name, const std::type_info &signature, const std::function<bool(void *)> &invoke) const;
    bool hasHandlers(QStringView name) const;
    void removeAll(QObject *owner);

private:
    void add(QStringView name, const char *signature, int sequence, QObject *owner, std::shared_ptr<void> function);

    struct Entry {
        int sequence;
        QObject *owner;
        // name of the signature's type_info, compared by value to work across shared libraries
        const char *signature;
        std::shared_ptr<void> function;
        bool removed = false;
    };
    using EntryList = std::vector<std::shared_ptr<Entry>>;

    QHash<QString, std::shared_ptr<const EntryList>> m_hooks;
    QHash<QObject *, QMetaObject::Connection> m_owners;
};

}  // namespace QXmpp::Private

#endif  // QXMPPHOOKS_P_H

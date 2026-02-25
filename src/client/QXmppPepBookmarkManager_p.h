// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPEPBOOKMARKMANAGER_P_H
#define QXMPPPEPBOOKMARKMANAGER_P_H

#include "QXmppPepBookmarkManager.h"
#include "QXmppTask.h"

#include <optional>

class QXmppPubSubManager;

namespace QXmpp::Private {
struct Bookmarks2ConferenceItem;
}  // namespace QXmpp::Private

struct QXmppPepBookmarkManagerPrivate {
    QXmppPepBookmarkManager *q = nullptr;
    std::optional<QList<QXmppMucBookmark>> bookmarks;

    QXmppPubSubManager *pubsub();

    void setBookmarks(QVector<QXmpp::Private::Bookmarks2ConferenceItem> &&items);
    void resetBookmarks();
    QXmppTask<QXmpp::Result<>> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<QXmpp::Result<>> removeBookmark(const QString &jid);
};

#endif  // QXMPPPEPBOOKMARKMANAGER_P_H

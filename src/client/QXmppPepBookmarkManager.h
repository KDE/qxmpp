// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPEPBOOKMARKMANAGER_H
#define QXMPPPEPBOOKMARKMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppPubSubEventHandler.h"
#include "QXmppTask.h"

#include <optional>

class QXmppError;
class QXmppMucBookmarkPrivate;

namespace QXmpp::Private {
struct Bookmarks2Conference;
}  // namespace QXmpp::Private

/*!
    \class QXmppMucBookmark
    \inmodule QXmpp

    \brief Bookmark data for a MUC room stored via \xep{0402}{PEP Native Bookmarks}.

    A bookmark records the JID of a room together with the user's preferred nickname, an optional
    password, a human-readable display name, and an autojoin flag. Bookmarks are managed through
    QXmppPepBookmarkManager::setBookmark() and QXmppPepBookmarkManager::removeBookmark().

    \since QXmpp 1.13
*/
class QXMPP_EXPORT QXmppMucBookmark
{
public:
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucBookmark)

    QXmppMucBookmark();
    QXmppMucBookmark(const QString &jid, const QString &name, bool autojoin, const QString &nick, const QString &password);

    const QString &jid() const;
    void setJid(const QString &jid);
    const QString &name() const;
    void setName(const QString &name);
    const QString &nick() const;
    void setNick(const QString &nick);
    const QString &password() const;
    void setPassword(const QString &password);
    bool autojoin() const;
    void setAutojoin(bool autojoin);

private:
    friend class QXmppPepBookmarkManager;
    friend struct QXmppPepBookmarkManagerPrivate;
    QXmppMucBookmark(const QString &jid, QXmpp::Private::Bookmarks2Conference conference);
    QSharedDataPointer<QXmppMucBookmarkPrivate> d;
};

struct QXmppPepBookmarkManagerPrivate;

/*!
    \class QXmppPepBookmarkManager
    \inmodule QXmpp

    \brief Manages \xep{0402}{PEP Native Bookmarks}.

    Bookmarks are automatically fetched when the session is established (unless the stream has
    been resumed, in which case the cached bookmarks are still up to date). Changes made from
    other clients arrive as PubSub event notifications and trigger the corresponding signals.

    If the fetch fails, the bookmarks are unknown: bookmarks() returns \c std::nullopt. Bookmarks
    fetched on an earlier connection are discarded in that case, because they may be outdated by
    now. bookmarksReset() is emitted in both cases, so it also signals a failed fetch.

    \section1 Setup

    QXmppPepBookmarkManager requires QXmppPubSubManager to be registered with the client:

    \code
    auto *pubsub = client.addNewExtension<QXmppPubSubManager>();
    auto *bookmarks = client.addNewExtension<QXmppPepBookmarkManager>();
    \endcode

    \section1 Usage

    \code
    connect(bm, &QXmppPepBookmarkManager::bookmarksReset, this, [bm]() {
        const auto &bookmarks = bm->bookmarks();
        if (!bookmarks) {
            // the bookmarks could not be fetched and are unknown now
            return;
        }
        for (const auto &bookmark : *bookmarks) {
            qDebug() << bookmark.jid() << bookmark.name();
        }
    });

    QXmppMucBookmark bookmark;
    bookmark.setJid(u"room@conference.example.org"_s);
    bookmark.setName(u"My Room"_s);
    bookmark.setNick(u"alice"_s);
    bookmark.setAutojoin(true);
    bm->setBookmark(std::move(bookmark)).then(this, [](auto result) {
        if (std::holds_alternative<QXmppError>(result)) { return; }
    });
\endcode

    \ingroup Managers
    \since QXmpp 1.16
*/
class QXMPP_EXPORT QXmppPepBookmarkManager : public QXmppClientExtension,
                                             public QXmppPubSubEventHandler
{
    Q_OBJECT

public:
    struct BookmarkChange {
        QXmppMucBookmark oldBookmark;
        QXmppMucBookmark newBookmark;
    };

    QXmppPepBookmarkManager();
    ~QXmppPepBookmarkManager();

    QStringList discoveryFeatures() const override;

    const std::optional<QList<QXmppMucBookmark>> &bookmarks() const;
    Q_SIGNAL void bookmarksReset();
    Q_SIGNAL void bookmarksAdded(const QList<QXmppMucBookmark> &newBookmarks);
    Q_SIGNAL void bookmarksChanged(const QList<QXmppPepBookmarkManager::BookmarkChange> &bookmarkUpdates);
    Q_SIGNAL void bookmarksRemoved(const QList<QString> &removedBookmarkJids);

    QXmppTask<QXmpp::Result<>> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<QXmpp::Result<>> removeBookmark(const QString &jid);

    bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) override;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onConnected();

    friend struct QXmppPepBookmarkManagerPrivate;
    friend class tst_QXmppMuc;
    const std::unique_ptr<QXmppPepBookmarkManagerPrivate> d;
};

#endif  // QXMPPPEPBOOKMARKMANAGER_H

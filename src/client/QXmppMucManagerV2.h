// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_H
#define QXMPPMUCMANAGERV2_H

#include "QXmppClientExtension.h"
#include "QXmppPubSubEventHandler.h"
#include "QXmppTask.h"

#include <variant>

namespace QXmpp::Private {
struct Bookmarks2Conference;
struct Bookmarks2ConferenceItem;
}  // namespace QXmpp::Private

class QXmppError;
class QXmppMucBookmarkPrivate;

class QXMPP_EXPORT QXmppMucBookmark
{
public:
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucBookmark)

    QXmppMucBookmark();
    QXmppMucBookmark(const QString &jid, const QString &name, bool autojoin, const QString &nick, const QString &password);
    QXmppMucBookmark(const QString &jid, QXmpp::Private::Bookmarks2Conference conference);

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
    friend class QXmppMucManagerV2Private;
    QSharedDataPointer<QXmppMucBookmarkPrivate> d;
};

Q_DECLARE_METATYPE(QXmppMucBookmark);

struct QXmppMucManagerV2Private;

class QXMPP_EXPORT QXmppMucManagerV2 : public QXmppClientExtension, public QXmppPubSubEventHandler
{
    Q_OBJECT

public:
    struct BookmarkChange {
        QXmppMucBookmark oldBookmark;
        QXmppMucBookmark newBookmark;
    };

    struct Avatar {
        QString contentType;
        QByteArray data;
    };

    QXmppMucManagerV2();
    ~QXmppMucManagerV2();

    QStringList discoveryFeatures() const override;

    const std::optional<QList<QXmppMucBookmark>> &bookmarks() const;
    Q_SIGNAL void bookmarksReset();
    Q_SIGNAL void bookmarksAdded(const QList<QXmppMucBookmark> &newBookmarks);
    Q_SIGNAL void bookmarksChanged(const QList<QXmppMucManagerV2::BookmarkChange> &bookmarkUpdates);
    Q_SIGNAL void bookmarksRemoved(const QList<QString> &removedBookmarkJids);

    QXmppTask<QXmpp::Result<>> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<QXmpp::Result<>> removeBookmark(const QString &jid);

    QXmppTask<QXmpp::Result<>> setRoomAvatar(const QString &jid, const Avatar &avatar);
    QXmppTask<QXmpp::Result<>> removeRoomAvatar(const QString &jid);
    QXmppTask<QXmpp::Result<std::optional<Avatar>>> fetchRoomAvatar(const QString &jid);

    bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) override;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onConnected();

    friend class QXmppMucManagerV2Private;
    friend class tst_QXmppMuc;
    const std::unique_ptr<QXmppMucManagerV2Private> d;
};

Q_DECLARE_METATYPE(QXmppMucManagerV2::BookmarkChange);

#endif  // QXMPPMUCMANAGERV2_H

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCMANAGERV2_H
#define QXMPPMUCMANAGERV2_H

#include "QXmppClientExtension.h"
#include "QXmppTask.h"

#include <variant>

namespace QXmpp::Private {
struct Bookmarks2Conference;
struct Bookmarks2ConferenceItem;
}  // namespace QXmpp::Private

class QXmppError;
class QXmppMucBookmarkPrivate;

class QXmppMucBookmark
{
public:
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucBookmark)

    QXmppMucBookmark();
    QXmppMucBookmark(QString &&jid, QXmpp::Private::Bookmarks2Conference &&conference);

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

struct QXmppMucManagerV2Private;

class QXmppMucManagerV2 : public QXmppClientExtension
{
    Q_OBJECT

public:
    using EmptyResult = std::variant<QXmpp::Success, QXmppError>;

    QXmppMucManagerV2();
    ~QXmppMucManagerV2();

    QStringList discoveryFeatures() const override;

    const std::optional<QList<QXmppMucBookmark>> &bookmarks() const;
    Q_SIGNAL void bookmarksReset();

    QXmppTask<EmptyResult> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<EmptyResult> removeBookmark(const QString &jid);

protected:
    void onRegistered(QXmppClient *client);
    void onUnregistered(QXmppClient *client);

private:
    void onConnected();
    void onDisconnected();

    friend class QXmppMucManagerV2Private;
    const std::unique_ptr<QXmppMucManagerV2Private> d;
};

#endif  // QXMPPMUCMANAGERV2_H

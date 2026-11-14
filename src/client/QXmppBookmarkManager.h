// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPBOOKMARKMANAGER_H
#define QXMPPBOOKMARKMANAGER_H

#include "QXmppClientExtension.h"

#include <QUrl>

class QXmppBookmarkManagerPrivate;
class QXmppBookmarkSet;

/*!
    \inmodule QXmpp
    \ingroup qxmpp-managers

    \brief The QXmppBookmarkManager class allows you to store and retrieve
    bookmarks as defined by \xep{0048}{Bookmarks}.
*/

class QXMPP_EXPORT QXmppBookmarkManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    QXmppBookmarkManager();
    ~QXmppBookmarkManager() override;

    bool areBookmarksReceived() const;
    QXmppBookmarkSet bookmarks() const;
    bool setBookmarks(const QXmppBookmarkSet &bookmarks);

    /*! This signal is emitted when \a bookmarks are received. */
    Q_SIGNAL void bookmarksReceived(const QXmppBookmarkSet &bookmarks);

private:
    bool handleIncomingStanza(const QDomElement &stanza);
    void requestBookmarks();
    void resetBookmarks();

    const std::unique_ptr<QXmppBookmarkManagerPrivate> d;
};

#endif

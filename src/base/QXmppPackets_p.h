// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPACKETS_P_H
#define QXMPPPACKETS_P_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"

#include <QUrl>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

//
// This file contains all structs needed in public headers.
//
// Other XML elements are in packets/*.h
//

namespace QXmpp::Private {

struct Bind {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind };
    // XML schema actually says variant<Jid, Resource>
    QString jid;
    QString resource;
};

struct BookmarkConference {
    static constexpr std::tuple XmlTag = { u"conference", ns_bookmarks };
    bool autojoin = false;
    QString jid;
    QString name;
    QString nick;
    QString password;
};

struct BookmarkUrl {
    static constexpr std::tuple XmlTag = { u"url", ns_bookmarks };

    QString name;
    QUrl url;
};

struct BookmarkStorage {
    static constexpr std::tuple XmlTag = { u"storage", ns_bookmarks };
    QList<BookmarkConference> conferences;
    QList<BookmarkUrl> urls;
};

}  // namespace QXmpp::Private

#endif  // QXMPPPACKETS_P_H

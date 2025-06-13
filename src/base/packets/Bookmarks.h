// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include "QXmppConstants_p.h"

#include "Xml.h"

namespace QXmpp::Private {

struct BookmarkConference {
    static constexpr std::tuple XmlTag = { u"conference", ns_bookmarks };
    bool autojoin;
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

template<>
struct XmlSpec<BookmarkConference> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &BookmarkConference::autojoin, u"autojoin", BoolDefaultSerializer(false) },
        XmlAttribute { &BookmarkConference::jid, u"jid" },
        XmlAttribute { &BookmarkConference::name, u"name" },
        XmlOptionalTextElement { &BookmarkConference::nick, u"nick" },
        XmlOptionalTextElement { &BookmarkConference::password, u"password" },
    };
};

template<>
struct XmlSpec<BookmarkUrl> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &BookmarkUrl::name, u"name" },
        XmlAttribute { &BookmarkUrl::url, u"url" },
    };
};

template<>
struct XmlSpec<BookmarkStorage> {
    static constexpr std::tuple Spec = {
        XmlReference { &BookmarkStorage::conferences },
        XmlReference { &BookmarkStorage::urls },
    };
};

}  // namespace QXmpp::Private

#endif  // BOOKMARKS_H

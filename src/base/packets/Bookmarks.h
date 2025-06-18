// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include "QXmppConstants_p.h"
#include "QXmppPackets_p.h"

#include "Xml.h"

namespace QXmpp::Private {

// actual structs in QXmppPackets_p.h

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

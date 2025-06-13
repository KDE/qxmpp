// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBookmarkSet.h"

#include "packets/Bookmarks.h"

using namespace QXmpp::Private;

/// Constructs a new conference room bookmark.
QXmppBookmarkConference::QXmppBookmarkConference()
    : m_autoJoin(false)
{
}

/// Returns whether the client should automatically join the conference room
/// on login.
bool QXmppBookmarkConference::autoJoin() const
{
    return m_autoJoin;
}

/// Sets whether the client should automatically join the conference room
/// on login.
void QXmppBookmarkConference::setAutoJoin(bool autoJoin)
{
    m_autoJoin = autoJoin;
}

/// Returns the JID of the conference room.
QString QXmppBookmarkConference::jid() const
{
    return m_jid;
}

/// Sets the JID of the conference room.
void QXmppBookmarkConference::setJid(const QString &jid)
{
    m_jid = jid;
}

/// Returns the friendly name for the bookmark.
QString QXmppBookmarkConference::name() const
{
    return m_name;
}

/// Sets the friendly name for the bookmark.
void QXmppBookmarkConference::setName(const QString &name)
{
    m_name = name;
}

/// Returns the preferred nickname for the conference room.
QString QXmppBookmarkConference::nickName() const
{
    return m_nickName;
}

/// Sets the preferred nickname for the conference room.
void QXmppBookmarkConference::setNickName(const QString &nickName)
{
    m_nickName = nickName;
}

/// Returns the friendly name for the bookmark.
QString QXmppBookmarkUrl::name() const
{
    return m_name;
}

/// Sets the friendly name for the bookmark.
void QXmppBookmarkUrl::setName(const QString &name)
{
    m_name = name;
}

/// Returns the URL for the web page.
QUrl QXmppBookmarkUrl::url() const
{
    return m_url;
}

/// Sets the URL for the web page.
void QXmppBookmarkUrl::setUrl(const QUrl &url)
{
    m_url = url;
}

/// Returns the conference rooms bookmarks in this bookmark set.
QList<QXmppBookmarkConference> QXmppBookmarkSet::conferences() const
{
    return m_conferences;
}

/// Sets the conference rooms bookmarks in this bookmark set.
void QXmppBookmarkSet::setConferences(const QList<QXmppBookmarkConference> &conferences)
{
    m_conferences = conferences;
}

/// Returns the web page bookmarks in this bookmark set.
QList<QXmppBookmarkUrl> QXmppBookmarkSet::urls() const
{
    return m_urls;
}

/// Sets the web page bookmarks in this bookmark set.
void QXmppBookmarkSet::setUrls(const QList<QXmppBookmarkUrl> &urls)
{
    m_urls = urls;
}

/// \cond
bool QXmppBookmarkSet::isBookmarkSet(const QDomElement &element)
{
    return element.tagName() == u"storage" &&
        element.namespaceURI() == ns_bookmarks;
}

void QXmppBookmarkSet::parse(const QDomElement &element)
{
    try {
        auto storage = XmlSpecParser::parse<BookmarkStorage>(element);
        m_conferences = transform<QList<QXmppBookmarkConference>>(storage.conferences, [](const auto &c) {
            QXmppBookmarkConference conference;
            conference.setAutoJoin(c.autojoin);
            conference.setJid(c.jid);
            conference.setName(c.name);
            conference.setNickName(c.nick);
            return conference;
        });
        m_urls = transform<QList<QXmppBookmarkUrl>>(storage.urls, [](const auto &u) {
            QXmppBookmarkUrl url;
            url.setName(u.name);
            url.setUrl(u.url);
            return url;
        });
    } catch (ParsingError) {
    }
}

void QXmppBookmarkSet::toXml(QXmlStreamWriter *writer) const
{
    BookmarkStorage s {
        transform<QList<BookmarkConference>>(m_conferences, [](const auto &c) {
            return BookmarkConference { c.autoJoin(), c.jid(), c.name(), c.nickName(), {} };
        }),
        transform<QList<BookmarkUrl>>(m_urls, [](const auto &u) {
            return BookmarkUrl { u.name(), u.url() };
        }),
    };
    XmlSpecSerializer::serialize(writer, s);
}
/// \endcond

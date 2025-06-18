// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBookmarkSet.h"

#include "packets/Bookmarks.h"

using namespace QXmpp::Private;

/*! Constructs a new conference room bookmark. */
QXmppBookmarkConference::QXmppBookmarkConference()
{
}

/*!
    Returns whether the client should automatically join the conference room
    on login.
*/
bool QXmppBookmarkConference::autoJoin() const
{
    return m_data.autojoin;
}

/*!
    Sets whether the client should automatically join the conference room
    on login.

    \a autoJoin.
*/
void QXmppBookmarkConference::setAutoJoin(bool autoJoin)
{
    m_data.autojoin = autoJoin;
}

/*! Returns the JID of the conference room. */
QString QXmppBookmarkConference::jid() const
{
    return m_data.jid;
}

/*!
    Sets the JID of the conference room.

    \a jid.
*/
void QXmppBookmarkConference::setJid(const QString &jid)
{
    m_data.jid = jid;
}

/*! Returns the friendly name for the bookmark. */
QString QXmppBookmarkConference::name() const
{
    return m_data.name;
}

/*! Sets the friendly \a name for the bookmark. */
void QXmppBookmarkConference::setName(const QString &name)
{
    m_data.name = name;
}

/*! Returns the preferred nickname for the conference room. */
QString QXmppBookmarkConference::nickName() const
{
    return m_data.nick;
}

/*!
    Sets the preferred nickname for the conference room.

    \a nickName.
*/
void QXmppBookmarkConference::setNickName(const QString &nickName)
{
    m_data.nick = nickName;
}

/*! Returns the friendly name for the bookmark. */
QString QXmppBookmarkUrl::name() const
{
    return m_data.name;
}

/*! Sets the friendly \a name for the bookmark. */
void QXmppBookmarkUrl::setName(const QString &name)
{
    m_data.name = name;
}

/*! Returns the URL for the web page. */
QUrl QXmppBookmarkUrl::url() const
{
    return m_data.url;
}

/*!
    Sets the URL for the web page.

    \a url.
*/
void QXmppBookmarkUrl::setUrl(const QUrl &url)
{
    m_data.url = url;
}

/*! Returns the conference rooms bookmarks in this bookmark set. */
QList<QXmppBookmarkConference> QXmppBookmarkSet::conferences() const
{
    return m_conferences;
}

/*!
    Sets the conference rooms bookmarks in this bookmark set.

    \a conferences.
*/
void QXmppBookmarkSet::setConferences(const QList<QXmppBookmarkConference> &conferences)
{
    m_conferences = conferences;
}

/*! Returns the web page bookmarks in this bookmark set. */
QList<QXmppBookmarkUrl> QXmppBookmarkSet::urls() const
{
    return m_urls;
}

/*!
    Sets the web page bookmarks in this bookmark set.

    \a urls.
*/
void QXmppBookmarkSet::setUrls(const QList<QXmppBookmarkUrl> &urls)
{
    m_urls = urls;
}

bool QXmppBookmarkSet::isBookmarkSet(const QDomElement &element)
{
    return elementXmlTag(element) == XmlTag;
}

void QXmppBookmarkSet::parse(const QDomElement &element)
{
    try {
        auto storage = XmlSpecParser::parse<BookmarkStorage>(element);
        m_conferences = transform<QList<QXmppBookmarkConference>>(std::move(storage.conferences), [](BookmarkConference &&c) {
            return QXmppBookmarkConference(std::move(c));
        });
        m_urls = transform<QList<QXmppBookmarkUrl>>(std::move(storage.urls), [](BookmarkUrl &&u) {
            return QXmppBookmarkUrl(std::move(u));
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

// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBookmarkSet.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

/*! Constructs a new conference room bookmark. */
QXmppBookmarkConference::QXmppBookmarkConference()
    : m_autoJoin(false)
{
}

/*!
    Returns whether the client should automatically join the conference room
    on login.
*/
bool QXmppBookmarkConference::autoJoin() const
{
    return m_autoJoin;
}

/*!
    Sets whether the client should automatically join the conference room
    on login.

    \a autoJoin.
*/
void QXmppBookmarkConference::setAutoJoin(bool autoJoin)
{
    m_autoJoin = autoJoin;
}

/*! Returns the JID of the conference room. */
QString QXmppBookmarkConference::jid() const
{
    return m_jid;
}

/*!
    Sets the JID of the conference room.

    \a jid.
*/
void QXmppBookmarkConference::setJid(const QString &jid)
{
    m_jid = jid;
}

/*! Returns the friendly name for the bookmark. */
QString QXmppBookmarkConference::name() const
{
    return m_name;
}

/*! Sets the friendly \a name for the bookmark. */
void QXmppBookmarkConference::setName(const QString &name)
{
    m_name = name;
}

/*! Returns the preferred nickname for the conference room. */
QString QXmppBookmarkConference::nickName() const
{
    return m_nickName;
}

/*!
    Sets the preferred nickname for the conference room.

    \a nickName.
*/
void QXmppBookmarkConference::setNickName(const QString &nickName)
{
    m_nickName = nickName;
}

std::optional<QXmppBookmarkConference> QXmppBookmarkConference::fromDom(const QDomElement &el)
{
    QXmppBookmarkConference conference;
    conference.m_autoJoin = parseBoolean(el.attribute(u"autojoin"_s)).value_or(false);
    conference.m_jid = el.attribute(u"jid"_s);
    conference.m_name = el.attribute(u"name"_s);
    conference.m_nickName = firstChildElement(el, u"nick").text();
    return conference;
}

void QXmppBookmarkConference::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"conference",
        OptionalAttribute { u"autojoin", DefaultedBool { m_autoJoin, false } },
        OptionalAttribute { u"jid", m_jid },
        OptionalAttribute { u"name", m_name },
        OptionalAttribute { u"nick", m_nickName },
    });
}

/*! Returns the friendly name for the bookmark. */
QString QXmppBookmarkUrl::name() const
{
    return m_name;
}

/*! Sets the friendly \a name for the bookmark. */
void QXmppBookmarkUrl::setName(const QString &name)
{
    m_name = name;
}

/*! Returns the URL for the web page. */
QUrl QXmppBookmarkUrl::url() const
{
    return m_url;
}

/*!
    Sets the URL for the web page.

    \a url.
*/
void QXmppBookmarkUrl::setUrl(const QUrl &url)
{
    m_url = url;
}

std::optional<QXmppBookmarkUrl> QXmppBookmarkUrl::fromDom(const QDomElement &el)
{
    QXmppBookmarkUrl url;
    url.setName(el.attribute(u"name"_s));
    url.setUrl(QUrl(el.attribute(u"url"_s)));
    return url;
}

void QXmppBookmarkUrl::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"url",
        OptionalAttribute { u"name", m_name },
        OptionalAttribute { u"url", m_url },
    });
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
    m_conferences = parseChildElements<QList<QXmppBookmarkConference>>(element);
    m_urls = parseChildElements<QList<QXmppBookmarkUrl>>(element);
}

void QXmppBookmarkSet::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { XmlTag, m_conferences, m_urls });
}

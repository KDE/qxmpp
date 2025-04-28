// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBookmarkSet.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"

#include <QDomElement>

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

/// \cond
std::optional<QXmppBookmarkConference> QXmppBookmarkConference::fromDom(const QDomElement &el)
{
    QXmppBookmarkConference conference;
    auto autojoinAttribute = el.attribute(u"autojoin"_s);
    conference.setAutoJoin(autojoinAttribute == u"true" || autojoinAttribute == u"1");
    conference.setJid(el.attribute(u"jid"_s));
    conference.setName(el.attribute(u"name"_s));
    conference.setNickName(firstChildElement(el, u"nick").text());
    return conference;
}

void QXmppBookmarkConference::toXml(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("conference"));
    if (m_autoJoin) {
        writeOptionalXmlAttribute(writer, u"autojoin", u"true"_s);
    }
    writeOptionalXmlAttribute(writer, u"jid", m_jid);
    writeOptionalXmlAttribute(writer, u"name", m_name);
    writeOptionalXmlTextElement(writer, u"nick", m_nickName);
    writer->writeEndElement();
}
/// \endcond

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

/// \cond
std::optional<QXmppBookmarkUrl> QXmppBookmarkUrl::fromDom(const QDomElement &el)
{
    QXmppBookmarkUrl url;
    url.setName(el.attribute(u"name"_s));
    url.setUrl(QUrl(el.attribute(u"url"_s)));
    return url;
}

void QXmppBookmarkUrl::toXml(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("url"));
    writeOptionalXmlAttribute(writer, u"name", m_name);
    writeOptionalXmlAttribute(writer, u"url", m_url.toString());
    writer->writeEndElement();
}
/// \endcond

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
    m_conferences = parseChildElements<QList<QXmppBookmarkConference>>(element);
    m_urls = parseChildElements<QList<QXmppBookmarkUrl>>(element);
}

void QXmppBookmarkSet::toXml(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(QSL65("storage"));
    writer->writeDefaultNamespace(toString65(ns_bookmarks));
    writeElements(writer, m_conferences);
    writeElements(writer, m_urls);
    writer->writeEndElement();
}
/// \endcond

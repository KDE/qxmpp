// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPBOOKMARKSET_H
#define QXMPPBOOKMARKSET_H

#include "QXmppPackets_p.h"
#include "QXmppStanza.h"

///
/// \brief The QXmppBookmarkConference class represents a bookmark for a conference room,
/// as defined by \xep{0048, Bookmarks}.
///
class QXMPP_EXPORT QXmppBookmarkConference
{
public:
    QXmppBookmarkConference();
    QXmppBookmarkConference(QXmpp::Private::BookmarkConference &&data)
        : m_data(std::move(data)) { }

    bool autoJoin() const;
    void setAutoJoin(bool autoJoin);

    QString jid() const;
    void setJid(const QString &jid);

    QString name() const;
    void setName(const QString &name);

    QString nickName() const;
    void setNickName(const QString &nickName);

private:
    QXmpp::Private::BookmarkConference m_data;
};

///
/// \brief The QXmppBookmarkUrl class represents a bookmark for a web page,
/// as defined by \xep{0048, Bookmarks}.
///
class QXMPP_EXPORT QXmppBookmarkUrl
{
public:
    QXmppBookmarkUrl() { }
    QXmppBookmarkUrl(QXmpp::Private::BookmarkUrl &&data)
        : m_data(std::move(data)) { }

    QString name() const;
    void setName(const QString &name);

    QUrl url() const;
    void setUrl(const QUrl &url);

private:
    QXmpp::Private::BookmarkUrl m_data;
};

///
/// \brief The QXmppbookmarkSets class represents a set of bookmarks, as defined
/// by \xep{0048, Bookmarks}.
///
class QXMPP_EXPORT QXmppBookmarkSet
{
public:
    QList<QXmppBookmarkConference> conferences() const;
    void setConferences(const QList<QXmppBookmarkConference> &conferences);

    QList<QXmppBookmarkUrl> urls() const;
    void setUrls(const QList<QXmppBookmarkUrl> &urls);

    /// \cond
    static bool isBookmarkSet(const QDomElement &element);
    static constexpr std::tuple XmlTag = { u"storage", QXmpp::Private::ns_bookmarks };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QList<QXmppBookmarkConference> m_conferences;
    QList<QXmppBookmarkUrl> m_urls;
};

#endif

// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPBOOKMARKSET_H
#define QXMPPBOOKMARKSET_H

#include "QXmppConstants_p.h"
#include "QXmppStanza.h"

#include <QList>
#include <QUrl>

///
/// \brief The QXmppBookmarkConference class represents a bookmark for a conference room,
/// as defined by \xep{0048, Bookmarks}.
///
class QXMPP_EXPORT QXmppBookmarkConference
{
public:
    QXmppBookmarkConference();

    bool autoJoin() const;
    void setAutoJoin(bool autoJoin);

    QString jid() const;
    void setJid(const QString &jid);

    QString name() const;
    void setName(const QString &name);

    QString nickName() const;
    void setNickName(const QString &nickName);

    /// \cond
    static constexpr std::tuple XmlTag = { u"conference", QXmpp::Private::ns_bookmarks };
    static std::optional<QXmppBookmarkConference> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    bool m_autoJoin;
    QString m_jid;
    QString m_name;
    QString m_nickName;
};

///
/// \brief The QXmppBookmarkUrl class represents a bookmark for a web page,
/// as defined by \xep{0048, Bookmarks}.
///
class QXMPP_EXPORT QXmppBookmarkUrl
{
public:
    QString name() const;
    void setName(const QString &name);

    QUrl url() const;
    void setUrl(const QUrl &url);

    /// \cond
    static constexpr std::tuple XmlTag = { u"url", QXmpp::Private::ns_bookmarks };
    static std::optional<QXmppBookmarkUrl> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_name;
    QUrl m_url;
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

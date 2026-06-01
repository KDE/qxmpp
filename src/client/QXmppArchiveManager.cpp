// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppArchiveManager.h"

#include "QXmppArchiveIq.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"

#include "StringLiterals.h"

#include <QDomElement>

using namespace QXmpp::Private;

QStringList QXmppArchiveManager::discoveryFeatures() const
{
    // XEP-0036: Message Archiving
    return { staticString(ns_archive) };
}

bool QXmppArchiveManager::handleStanza(const QDomElement &element)
{
    auto tag = iqPayloadXmlTag(element);

    if (tag == PayloadXmlTag<QXmppArchiveChatIq>) {
        QXmppArchiveChatIq archiveIq;
        archiveIq.parse(element);
        Q_EMIT archiveChatReceived(archiveIq.chat(), archiveIq.resultSetReply());
        return true;
    } else if (tag == PayloadXmlTag<QXmppArchiveListIq>) {
        QXmppArchiveListIq archiveIq;
        archiveIq.parse(element);
        Q_EMIT archiveListReceived(archiveIq.chats(), archiveIq.resultSetReply());
        return true;
    } else if (tag == PayloadXmlTag<QXmppArchivePrefIq>) {
        // TODO: handle preference iq
        QXmppArchivePrefIq archiveIq;
        archiveIq.parse(element);
        return true;
    }

    return false;
}

/*!
    Retrieves the list of available collections. Once the results are
    received, the archiveListReceived() signal will be emitted.

    \a jid is the JID you want conversations with. \a start is an optional start time, \a end
    an optional end time. \a rsm is an optional Result Set Management query.
*/
void QXmppArchiveManager::listCollections(const QString &jid, const QDateTime &start,
                                          const QDateTime &end, const QXmppResultSetQuery &rsm)
{
    QXmppArchiveListIq packet;
    packet.setResultSetQuery(rsm);
    packet.setWith(jid);
    packet.setStart(start);
    packet.setEnd(end);
    client()->send(std::move(packet));
}

/*!
    \overload

    Retrieves the list of available collections. Once the results are
    received, the archiveListReceived() signal will be emitted.

    \a jid is the JID you want conversations with. \a start is the start time, \a end the end
    time. \a max is the maximum number of collections to list.
*/
void QXmppArchiveManager::listCollections(const QString &jid, const QDateTime &start, const QDateTime &end, int max)
{
    QXmppResultSetQuery rsm;
    rsm.setMax(max);
    listCollections(jid, start, end, rsm);
}

/*!
    Removes the specified collection(s).

    \a jid is the JID of the collection. \a start is an optional start time, \a end an optional
    end time.
*/
void QXmppArchiveManager::removeCollections(const QString &jid, const QDateTime &start, const QDateTime &end)
{
    QXmppArchiveRemoveIq packet;
    packet.setType(QXmppIq::Set);
    packet.setWith(jid);
    packet.setStart(start);
    packet.setEnd(end);
    client()->send(std::move(packet));
}

/*!
    Retrieves the specified collection. Once the results are received,
    the archiveChatReceived() will be emitted.

    \a jid is the JID of the collection. \a start is the start time of the collection. \a rsm
    is an optional Result Set Management query.
*/
void QXmppArchiveManager::retrieveCollection(const QString &jid, const QDateTime &start, const QXmppResultSetQuery &rsm)
{
    QXmppArchiveRetrieveIq packet;
    packet.setResultSetQuery(rsm);
    packet.setStart(start);
    packet.setWith(jid);
    client()->send(std::move(packet));
}

/*!
    \overload

    Retrieves the specified collection. Once the results are received,
    the archiveChatReceived() will be emitted.

    \a jid is the JID of the collection. \a start is the start time of the collection. \a max
    is the maximum number of messages to retrieve.
*/
void QXmppArchiveManager::retrieveCollection(const QString &jid, const QDateTime &start, int max)
{
    QXmppResultSetQuery rsm;
    rsm.setMax(max);
    retrieveCollection(jid, start, rsm);
}

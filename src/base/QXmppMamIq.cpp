// SPDX-FileCopyrightText: 2016 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMamIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

/*!
    \struct QXmpp::Mam::QueryRequest
    \inmodule QXmpp

    \brief IQ payload of an archive query (\xep{0313}{Message Archive Management}).

    It is sent in an IQ of type set and answered with QXmpp::Mam::QueryResult:
    \code
    client->sendIq(QXmpp::Iq::Set<QXmpp::Mam::QueryRequest> { .payload = { .queryId = queryId, .form = form } })
        .then(this, [](QXmpp::Result<QXmpp::Mam::QueryResult> &&result) { ... });
    \endcode

    Usually QXmppMamManager::retrieve() is used instead, it also processes the retrieved messages.

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::QueryRequest::queryId

    ID of the query, the archive includes it in the messages of the results.
*/

/*!
    \variable QXmpp::Mam::QueryRequest::node

    The PubSub node to query, usually empty.
*/

/*!
    \variable QXmpp::Mam::QueryRequest::form

    Data form with the filters of the query (\xep{0313}{Message Archive Management} §4.1).
*/

/*!
    \variable QXmpp::Mam::QueryRequest::resultSetQuery

    Limits the number of results and pages through the archive (\xep{0059}{Result Set Management}).
*/

/*!
    \variable QXmpp::Mam::QueryRequest::flipPage

    Whether the archive should send the messages of the page in reverse order, i.e. the newest
    message first. This does not change which messages are returned.

    This requires the archive to support QXmpp::Namespace::Mam2Extended.
*/

/*!
    \struct QXmpp::Mam::QueryResult
    \inmodule QXmpp

    \brief IQ payload of the response to an archive query (\xep{0313}{Message Archive Management}).

    It is sent after all messages of the results and is used for paging through the archive.

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::QueryResult::complete

    Whether the last page of the results has been reached, i.e. there are no further messages
    matching the query.
*/

/*!
    \variable QXmpp::Mam::QueryResult::stable

    Whether the results are stable. Unstable results may still change in order or content, e.g.
    because the storage of a clustered server has not converged yet. They should not be cached
    long-term without confirming them with a later query.
*/

/*!
    \variable QXmpp::Mam::QueryResult::resultSetReply

    The result set reply (\xep{0059}{Result Set Management}) for paging through the archive.
*/

std::optional<Mam::QueryRequest> Mam::QueryRequest::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return {};
    }

    QueryRequest query;
    query.queryId = el.attribute(u"queryid"_s);
    query.node = el.attribute(u"node"_s);
    if (auto formEl = firstChildElement(el, u"x", ns_data); !formEl.isNull()) {
        query.form.parse(formEl);
    }
    if (auto setEl = firstChildElement(el, u"set", ns_rsm); !setEl.isNull()) {
        query.resultSetQuery.parse(setEl);
    }
    query.flipPage = !firstChildElement(el, u"flip-page", ns_mam).isNull();
    return query;
}

void Mam::QueryRequest::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalAttribute { u"node", node },
        OptionalAttribute { u"queryid", queryId },
        form,
        resultSetQuery,
        OptionalContent { flipPage, Element { u"flip-page" } },
    });
}

std::optional<Mam::QueryResult> Mam::QueryResult::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return {};
    }

    QueryResult result;
    result.complete = parseBoolean(el.attribute(u"complete"_s)).value_or(false);
    result.stable = parseBoolean(el.attribute(u"stable"_s)).value_or(true);
    if (auto setEl = firstChildElement(el, u"set", ns_rsm); !setEl.isNull()) {
        result.resultSetReply.parse(setEl);
    }
    return result;
}

void Mam::QueryResult::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalAttribute { u"complete", DefaultedBool { complete, false } },
        OptionalAttribute { u"stable", DefaultedBool { stable, true } },
        resultSetReply,
    });
}

/*!
    \struct QXmpp::Mam::ArchiveEntry
    \inmodule QXmpp

    \brief First or last message of an archive, see QXmpp::Mam::ArchiveMetadata.

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::ArchiveEntry::id

    ID of the message in the archive.
*/

/*!
    \variable QXmpp::Mam::ArchiveEntry::timestamp

    Time at which the message was archived.
*/

/*!
    \struct QXmpp::Mam::ArchiveMetadata
    \inmodule QXmpp

    \brief IQ payload with the metadata of an archive (\xep{0313}{Message Archive Management}).

    An empty payload is sent in an IQ of type get to request the metadata. The archive responds
    with the first and the last message it contains:
    \code
    client->sendIq(QXmpp::Iq::Get<QXmpp::Mam::ArchiveMetadata> {})
        .then(this, [](QXmpp::Result<QXmpp::Mam::ArchiveMetadata> &&result) { ... });
    \endcode

    Usually QXmppMamManager::requestMetadata() is used instead.

    This requires the archive to support QXmpp::Namespace::Mam2Extended.

    \since QXmpp 1.18
*/

/*!
    \variable QXmpp::Mam::ArchiveMetadata::start

    The first message of the archive, empty if the archive is empty.
*/

/*!
    \variable QXmpp::Mam::ArchiveMetadata::end

    The last message of the archive, empty if the archive is empty.
*/

static std::optional<Mam::ArchiveEntry> parseArchiveEntry(const QDomElement &el)
{
    if (el.isNull()) {
        return {};
    }
    return Mam::ArchiveEntry {
        .id = el.attribute(u"id"_s),
        .timestamp = QXmppUtils::datetimeFromString(el.attribute(u"timestamp"_s)),
    };
}

std::optional<Mam::ArchiveMetadata> Mam::ArchiveMetadata::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return {};
    }

    return ArchiveMetadata {
        .start = parseArchiveEntry(firstChildElement(el, u"start", ns_mam)),
        .end = parseArchiveEntry(firstChildElement(el, u"end", ns_mam)),
    };
}

void Mam::ArchiveMetadata::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    auto writeEntry = [&](QStringView tag, const std::optional<ArchiveEntry> &entry) {
        if (entry) {
            w.write(Element {
                tag,
                Attribute { u"id", entry->id },
                Attribute { u"timestamp", entry->timestamp },
            });
        }
    };

    w.write(Element {
        XmlTag,
        [&] { writeEntry(u"start", start); },
        [&] { writeEntry(u"end", end); },
    });
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

class QXmppMamQueryIqPrivate : public QSharedData
{
public:
    QXmppDataForm form;
    QXmppResultSetQuery resultSetQuery;
    QString node;
    QString queryId;
};

/*!
    \class QXmppMamQueryIq
    \inmodule QXmpp

    The QXmppMamQueryIq class represents the query IQ for \xep{0313}{Message
    Archive Management}.

    \ingroup qxmpp-stanzas

    \since QXmpp 1.0

    \deprecated since QXmpp 1.18, use QXmpp::Mam::QueryRequest or QXmppMamManager::retrieve().
*/

QXmppMamQueryIq::QXmppMamQueryIq()
    : QXmppIq(QXmppIq::Set),
      d(new QXmppMamQueryIqPrivate)
{
}

/*! Default copy constructor */
QXmppMamQueryIq::QXmppMamQueryIq(const QXmppMamQueryIq &) = default;
/*! Default move constructor */
QXmppMamQueryIq::QXmppMamQueryIq(QXmppMamQueryIq &&) = default;
QXmppMamQueryIq::~QXmppMamQueryIq() = default;
/*! Default assignemnt operator */
QXmppMamQueryIq &QXmppMamQueryIq::operator=(const QXmppMamQueryIq &) = default;
/*! Default move-assignemnt operator */
QXmppMamQueryIq &QXmppMamQueryIq::operator=(QXmppMamQueryIq &&) = default;

/*! Returns the form that specifies the query. */
QXmppDataForm QXmppMamQueryIq::form() const
{
    return d->form;
}

/*!
    Sets the data \a form that specifies the query.
*/
void QXmppMamQueryIq::setForm(const QXmppDataForm &form)
{
    d->form = form;
}

/*! Returns the result set query for result set management. */
QXmppResultSetQuery QXmppMamQueryIq::resultSetQuery() const
{
    return d->resultSetQuery;
}

/*!
    Sets \a resultSetQuery for result set management.
*/
void QXmppMamQueryIq::setResultSetQuery(const QXmppResultSetQuery &resultSetQuery)
{
    d->resultSetQuery = resultSetQuery;
}

/*! Returns the node to query. */
QString QXmppMamQueryIq::node() const
{
    return d->node;
}

/*!
    Sets the \a node to query.
*/
void QXmppMamQueryIq::setNode(const QString &node)
{
    d->node = node;
}

/*! Returns the queryid that will be included in the results. */
QString QXmppMamQueryIq::queryId() const
{
    return d->queryId;
}

/*!
    Sets the query \a id that will be included in the results.
*/
void QXmppMamQueryIq::setQueryId(const QString &id)
{
    d->queryId = id;
}

void QXmppMamQueryIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    d->node = queryElement.attribute(u"node"_s);
    d->queryId = queryElement.attribute(u"queryId"_s);
    QDomElement resultSetElement = queryElement.firstChildElement(u"set"_s);
    if (!resultSetElement.isNull()) {
        d->resultSetQuery.parse(resultSetElement);
    }
    QDomElement formElement = queryElement.firstChildElement(u"x"_s);
    if (!formElement.isNull()) {
        d->form.parse(formElement);
    }
}

void QXmppMamQueryIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        OptionalAttribute { u"node", d->node },
        OptionalAttribute { u"queryid", d->queryId },
        d->form,
        d->resultSetQuery,
    });
}

class QXmppMamResultIqPrivate : public QSharedData
{
public:
    QXmppResultSetReply resultSetReply;
    bool complete;
};

/*!
    \class QXmppMamResultIq
    \inmodule QXmpp

    The QXmppMamResultIq class represents the result IQ for \xep{0313}{Message
    Archive Management}.

    \ingroup qxmpp-stanzas

    \since QXmpp 1.0

    \deprecated since QXmpp 1.18, use QXmpp::Mam::QueryResult or QXmppMamManager::retrieve().
*/

QXmppMamResultIq::QXmppMamResultIq()
    : d(new QXmppMamResultIqPrivate)
{
    d->complete = false;
}

/*! Default move constructor */
QXmppMamResultIq::QXmppMamResultIq(QXmppMamResultIq &&) = default;
/*! Default copy constructor */
QXmppMamResultIq::QXmppMamResultIq(const QXmppMamResultIq &) = default;
QXmppMamResultIq::~QXmppMamResultIq() = default;
/*! Default assignemnt operator */
QXmppMamResultIq &QXmppMamResultIq::operator=(const QXmppMamResultIq &) = default;
/*! Default move-assignemnt operator */
QXmppMamResultIq &QXmppMamResultIq::operator=(QXmppMamResultIq &&) = default;

/*! Returns the result set reply for result set management. */
QXmppResultSetReply QXmppMamResultIq::resultSetReply() const
{
    return d->resultSetReply;
}

/*! Sets the \a resultSetReply for result set management. */
void QXmppMamResultIq::setResultSetReply(const QXmppResultSetReply &resultSetReply)
{
    d->resultSetReply = resultSetReply;
}

/*!
    Returns true if the results returned by the server are complete (not
    limited by the server).
*/
bool QXmppMamResultIq::complete() const
{
    return d->complete;
}

/*!
    Sets whether the results returned by the server are complete (\a complete),
    i.e. not limited by the server.
*/
void QXmppMamResultIq::setComplete(bool complete)
{
    d->complete = complete;
}

void QXmppMamResultIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement finElement = element.firstChildElement(u"fin"_s);
    d->complete = finElement.attribute(u"complete"_s) == u"true";
    QDomElement resultSetElement = finElement.firstChildElement(u"set"_s);
    if (!resultSetElement.isNull()) {
        d->resultSetReply.parse(resultSetElement);
    }
}

void QXmppMamResultIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        OptionalAttribute { u"complete", DefaultedBool { d->complete, false } },
        d->resultSetReply,
    });
}
QT_WARNING_POP

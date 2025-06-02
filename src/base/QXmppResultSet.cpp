// SPDX-FileCopyrightText: 2012 Oliver Goffart <ogoffart@woboq.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppResultSet.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDebug>
#include <QDomElement>

using namespace QXmpp::Private;

template<std::integral Int>
static auto toOptional(Int n)
{
    return n >= 0 ? std::make_optional(n) : std::nullopt;
}

static auto toOptional(QStringView s)
{
    return !s.isNull() ? std::make_optional(s) : std::nullopt;
}

QXmppResultSetQuery::QXmppResultSetQuery()
    : m_index(-1), m_max(-1)
{
}

///
/// Returns the maximum number of results.
///
/// \note -1 means no limit, 0 means no results are wanted.
///
int QXmppResultSetQuery::max() const
{
    return m_max;
}

///
/// Sets the maximum number of results.
///
/// \note -1 means no limit, 0 means no results are wanted.
///
void QXmppResultSetQuery::setMax(int max)
{
    m_max = max;
}

///
/// Returns the index for the first element in the page.
///
/// This is used for retrieving pages out of order.
///
int QXmppResultSetQuery::index() const
{
    return m_index;
}

///
/// Sets the index for the first element in the page.
///
/// This is used for retrieving pages out of order.
///
void QXmppResultSetQuery::setIndex(int index)
{
    m_index = index;
}

///
/// Returns the UID of the first result in the next page.
///
/// This is used for for paging backwards through results.
///
QString QXmppResultSetQuery::before() const
{
    return m_before;
}

///
/// Sets the UID of the first result in the next page.
///
/// This is used for for paging backwards through results.
///
void QXmppResultSetQuery::setBefore(const QString &before)
{
    m_before = before;
}

///
/// Returns the UID of the last result in the previous page.
///
/// This is used for for paging forwards through results.
///
QString QXmppResultSetQuery::after() const
{
    return m_after;
}

///
/// Sets the UID of the last result in the previous page.
///
/// This is used for for paging forwards through results.
///
void QXmppResultSetQuery::setAfter(const QString &after)
{
    m_after = after;
}

/// Returns true if no result set information is present.
bool QXmppResultSetQuery::isNull() const
{
    return m_max == -1 && m_index == -1 && m_after.isNull() && m_before.isNull();
}

/// \cond
void QXmppResultSetQuery::parse(const QDomElement &element)
{
    QDomElement setElement = (element.tagName() == u"set") ? element : element.firstChildElement(u"set"_s);
    if (setElement.namespaceURI() == ns_rsm) {
        m_max = parseInt(setElement.firstChildElement(u"max"_s).text()).value_or(-1);
        m_after = setElement.firstChildElement(u"after"_s).text();
        m_before = setElement.firstChildElement(u"before"_s).text();
        m_index = parseInt(setElement.firstChildElement(u"index"_s).text()).value_or(-1);
    }
}

void QXmppResultSetQuery::toXml(QXmlStreamWriter *writer) const
{
    if (isNull()) {
        return;
    }

    XmlWriter(writer).write(Element {
        { u"set", ns_rsm },
        OptionalTextElement { u"max", toOptional(m_max) },
        OptionalTextElement { u"after", toOptional(m_after) },
        OptionalTextElement { u"before", toOptional(m_before) },
        OptionalTextElement { u"index", toOptional(m_index) },
    });
}
/// \endcond

QXmppResultSetReply::QXmppResultSetReply()
    : m_count(-1), m_index(-1)
{
}

/// Returns the UID of the first result in the page.
QString QXmppResultSetReply::first() const
{
    return m_first;
}

/// Sets the UID of the first result in the page.
void QXmppResultSetReply::setFirst(const QString &first)
{
    m_first = first;
}

/// Returns the UID of the last result in the page.
QString QXmppResultSetReply::last() const
{
    return m_last;
}

/// Sets the UID of the last result in the page.
void QXmppResultSetReply::setLast(const QString &last)
{
    m_last = last;
}

///
/// Returns the total number of items in the set.
///
/// \note This may be an approximate count.
///
int QXmppResultSetReply::count() const
{
    return m_count;
}

///
/// Sets the total number of items in the set.
///
/// \note This may be an approximate count.
///
void QXmppResultSetReply::setCount(int count)
{
    m_count = count;
}

///
/// Returns the index for the first result in the page.
///
/// This is used for retrieving pages out of order.
///
/// \note This may be an approximate index.
///
int QXmppResultSetReply::index() const
{
    return m_index;
}

///
/// Sets the index for the first result in the page.
///
/// This is used for retrieving pages out of order.
///
/// \note This may be an approximate index.
///
void QXmppResultSetReply::setIndex(int index)
{
    m_index = index;
}

/// Returns true if no result set information is present.
bool QXmppResultSetReply::isNull() const
{
    return m_count == -1 && m_index == -1 && m_first.isNull() && m_last.isNull();
}

/// \cond
void QXmppResultSetReply::parse(const QDomElement &element)
{
    QDomElement setElement = (element.tagName() == u"set") ? element : firstChildElement(element, u"set");
    if (setElement.namespaceURI() == ns_rsm) {
        m_count = parseInt(firstChildElement(setElement, u"count").text()).value_or(-1);
        QDomElement firstElem = firstChildElement(setElement, u"first");
        m_first = firstElem.text();
        m_index = parseInt(firstElem.attribute(u"index"_s)).value_or(-1);
        m_last = firstChildElement(setElement, u"last").text();
    }
}

void QXmppResultSetReply::toXml(QXmlStreamWriter *writer) const
{
    if (isNull()) {
        return;
    }

    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalContent {
            toOptional(m_first) || toOptional(m_index),
            Element {
                u"first",
                OptionalAttribute { u"index", toOptional(m_index) },
                Characters { m_first },
            },
        },
        OptionalTextElement { u"last", toOptional(m_last) },
        OptionalTextElement { u"count", toOptional(m_count) },
    });
}
/// \endcond

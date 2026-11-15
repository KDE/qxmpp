// SPDX-FileCopyrightText: 2016 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMAMIQ_H
#define QXMPPMAMIQ_H

#include "QXmppDataForm.h"
#include "QXmppIq.h"
#include "QXmppIqStanza.h"
#include "QXmppResultSet.h"

#include <optional>

#include <QSharedDataPointer>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp::Mam {

struct QXMPP_EXPORT QueryRequest {
    QString queryId {};
    QString node {};
    QXmppDataForm form {};
    QXmppResultSetQuery resultSetQuery {};

    static constexpr std::tuple XmlTag = { u"query", QXmpp::Private::ns_mam };
    static std::optional<QueryRequest> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

struct QXMPP_EXPORT QueryResult {
    bool complete = false;
    QXmppResultSetReply resultSetReply {};

    static constexpr std::tuple XmlTag = { u"fin", QXmpp::Private::ns_mam };
    static std::optional<QueryResult> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
};

}  // namespace QXmpp::Mam

// the response to a query contains the end of the results
template<>
struct QXmpp::Iq::ResponseTraits<QXmpp::Iq::Set<QXmpp::Mam::QueryRequest>> {
    using Payload = QXmpp::Mam::QueryResult;
};

#if QXMPP_DEPRECATED_SINCE(1, 18)
class QXmppMamQueryIqPrivate;
class QXmppMamResultIqPrivate;

class [[deprecated("Use QXmpp::Mam::QueryRequest")]]
QXMPP_EXPORT QXmppMamQueryIq : public QXmppIq
{
public:
    QXmppMamQueryIq();
    QXmppMamQueryIq(const QXmppMamQueryIq &);
    QXmppMamQueryIq(QXmppMamQueryIq &&);
    ~QXmppMamQueryIq() override;

    QXmppMamQueryIq &operator=(const QXmppMamQueryIq &);
    QXmppMamQueryIq &operator=(QXmppMamQueryIq &&);

    QXmppDataForm form() const;
    void setForm(const QXmppDataForm &form);
    QXmppResultSetQuery resultSetQuery() const;
    void setResultSetQuery(const QXmppResultSetQuery &resultSetQuery);
    QString node() const;
    void setNode(const QString &node);
    QString queryId() const;
    void setQueryId(const QString &id);

    static constexpr std::tuple PayloadXmlTag = { u"query", QXmpp::Private::ns_mam };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isMamQueryIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppMamQueryIqPrivate> d;
};

class [[deprecated("Use QXmpp::Mam::QueryResult")]]
QXMPP_EXPORT QXmppMamResultIq : public QXmppIq
{
public:
    QXmppMamResultIq();
    QXmppMamResultIq(const QXmppMamResultIq &);
    QXmppMamResultIq(QXmppMamResultIq &&);
    ~QXmppMamResultIq() override;

    QXmppMamResultIq &operator=(const QXmppMamResultIq &);
    QXmppMamResultIq &operator=(QXmppMamResultIq &&);

    QXmppResultSetReply resultSetReply() const;
    void setResultSetReply(const QXmppResultSetReply &resultSetReply);
    bool complete() const;
    void setComplete(bool complete);

    static constexpr std::tuple PayloadXmlTag = { u"fin", QXmpp::Private::ns_mam };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isMamResultIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppMamResultIqPrivate> d;
};
#endif

#endif

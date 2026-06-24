// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppSpamReport.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"

#include "Enums.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppSpamReport::Reason> {
    using enum QXmppSpamReport::Reason;
    static constexpr auto Values = makeValues<QXmppSpamReport::Reason>({
        { Spam, u"urn:xmpp:reporting:spam" },
        { Abuse, u"urn:xmpp:reporting:abuse" },
    });
};

/*!
    \class QXmppSpamReport
    \inmodule QXmpp

    \brief Describes a spam/abuse report attached to a block request, as defined in
    \xep{0377}{Blocking Command Reports}.

    A report carries a reason() (spam or abuse) and may optionally include explanatory text(),
    references to the offending messages (messageReferences()) and opt-in flags allowing the
    server to forward the report to the reported JID's origin domain (forwardToOrigin()) or to
    third-party abuse services (forwardToThirdParty()).

    Reports are sent together with a block request via
    QXmppBlockingManager::reportAndBlock().

    \since QXmpp 1.17
    \sa QXmppBlockingManager
*/

/*!
    \enum QXmppSpamReport::Reason

    The reason a JID is being reported.

    \value Spam
        Unwanted or unsolicited messages (\c urn:xmpp:reporting:spam).
    \value Abuse
        Abusive behaviour not covered by another category (\c urn:xmpp:reporting:abuse).
*/

/*!
    \fn QXmppSpamReport::textLanguage() const

    Returns the language (\c xml:lang) of the explanatory text(). Empty if no language is set.
*/

/*!
    \fn QXmppSpamReport::setTextLanguage(const QString &language)

    Sets the \a language (\c xml:lang) of the explanatory text(). When empty, no \c xml:lang
    attribute is serialized.
*/

/*!
    \fn QXmppSpamReport::forwardToOrigin() const

    Returns whether the server is allowed to forward this report to the domain where the
    reported message originated.
*/

/*!
    \fn QXmppSpamReport::forwardToThirdParty() const

    Returns whether the server is allowed to forward this report to third-party services that
    process reports (e.g. for statistics, analysis or block lists).
*/

/*!
    \fn QXmppSpamReport::QXmppSpamReport()

    Constructs a report with the reason QXmppSpamReport::Reason::Spam.
*/

/*!
    \fn QXmppSpamReport::QXmppSpamReport(Reason reason)

    Constructs a report with the given \a reason.
*/

/// \cond
std::optional<QXmppSpamReport> QXmppSpamReport::fromDom(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return std::nullopt;
    }

    auto reason = Enums::fromString<Reason>(el.attribute(u"reason"_s));
    if (!reason) {
        return std::nullopt;
    }

    QXmppSpamReport report(*reason);
    if (auto text = firstChildElement(el, u"text", ns_reporting); !text.isNull()) {
        report.m_text = text.text();
        report.m_textLanguage = text.attributeNS(staticString(ns_xml), u"lang"_s);
    }
    report.m_messageReferences = parseChildElements<QList<QXmppStanzaId>>(el);
    report.m_forwardToOrigin = hasChild(el, u"report-origin", ns_reporting);
    report.m_forwardToThirdParty = hasChild(el, u"third-party", ns_reporting);
    return report;
}

void QXmppSpamReport::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        XmlTag,
        Attribute { u"reason", m_reason },
        m_messageReferences,
        OptionalContent { !m_text.isEmpty(),
                          Element { u"text", OptionalAttribute { u"xml:lang", m_textLanguage }, Characters { m_text } } },
        OptionalContent { m_forwardToOrigin, Element { u"report-origin" } },
        OptionalContent { m_forwardToThirdParty, Element { u"third-party" } },
    });
}
/// \endcond

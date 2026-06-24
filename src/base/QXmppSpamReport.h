// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSPAMREPORT_H
#define QXMPPSPAMREPORT_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"
#include "QXmppStanzaId.h"

#include <optional>
#include <tuple>

#include <QList>
#include <QString>

class QDomElement;
class QXmlStreamWriter;

class QXMPP_EXPORT QXmppSpamReport
{
public:
    enum class Reason {
        Spam,
        Abuse,
    };

    QXmppSpamReport() = default;
    explicit QXmppSpamReport(Reason reason) : m_reason(reason) { }

    Reason reason() const { return m_reason; }
    void setReason(Reason reason) { m_reason = reason; }

    const QString &text() const { return m_text; }
    void setText(const QString &text) { m_text = text; }

    const QString &textLanguage() const { return m_textLanguage; }
    void setTextLanguage(const QString &language) { m_textLanguage = language; }

    const QList<QXmppStanzaId> &messageReferences() const { return m_messageReferences; }
    void setMessageReferences(const QList<QXmppStanzaId> &references) { m_messageReferences = references; }

    bool forwardToOrigin() const { return m_forwardToOrigin; }
    void setForwardToOrigin(bool enabled) { m_forwardToOrigin = enabled; }

    bool forwardToThirdParty() const { return m_forwardToThirdParty; }
    void setForwardToThirdParty(bool enabled) { m_forwardToThirdParty = enabled; }

    static constexpr std::tuple XmlTag = { u"report", QXmpp::Private::ns_reporting };
    static std::optional<QXmppSpamReport> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;

private:
    Reason m_reason = Reason::Spam;
    QString m_text;
    QString m_textLanguage;
    QList<QXmppStanzaId> m_messageReferences;
    bool m_forwardToOrigin = false;
    bool m_forwardToThirdParty = false;
};

#endif  // QXMPPSPAMREPORT_H

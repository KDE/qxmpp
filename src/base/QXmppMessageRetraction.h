// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGERETRACTION_H
#define QXMPPMESSAGERETRACTION_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"

#include <tuple>

#include <QDateTime>
#include <QString>

class QDomElement;
class QXmlStreamWriter;

class QXMPP_EXPORT QXmppMessageRetraction
{
public:
    QXmppMessageRetraction() = default;
    explicit QXmppMessageRetraction(QString retractedId);

    QString retractedId() const;
    void setRetractedId(const QString &id);

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
    static constexpr std::tuple XmlTag = { u"retract", QXmpp::Private::ns_message_retract };

private:
    QString m_retractedId;
};

class QXMPP_EXPORT QXmppMessageRetracted
{
public:
    QXmppMessageRetracted() = default;

    QDateTime stamp() const;
    void setStamp(const QDateTime &stamp);

    QString by() const;
    void setBy(const QString &by);

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
    static constexpr std::tuple XmlTag = { u"retracted", QXmpp::Private::ns_message_retract };

private:
    QDateTime m_stamp;
    QString m_by;
};

#endif  // QXMPPMESSAGERETRACTION_H

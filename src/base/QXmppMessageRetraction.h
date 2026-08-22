// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGERETRACTION_H
#define QXMPPMESSAGERETRACTION_H

#include "QXmppConstants_p.h"
#include "QXmppDataFormBase.h"
#include "QXmppGlobal.h"

#include <chrono>
#include <optional>
#include <tuple>

#include <QDateTime>
#include <QString>

class QDomElement;
class QXmlStreamWriter;

class QXMPP_EXPORT QXmppMessageModeration
{
public:
    QXmppMessageModeration() = default;

    QString moderatorJid() const;
    void setModeratorJid(const QString &jid);

    QString moderatorOccupantId() const;
    void setModeratorOccupantId(const QString &id);

    QString reason() const;
    void setReason(const QString &reason);

    void parse(const QDomElement &parentElement);
    void toXml(QXmlStreamWriter *writer) const;
    static constexpr std::tuple XmlTag = { u"moderated", QXmpp::Private::ns_message_moderate };

private:
    QString m_moderatorJid;
    QString m_moderatorOccupantId;
    QString m_reason;
};

class QXMPP_EXPORT QXmppMessageRetraction
{
public:
    QXmppMessageRetraction() = default;
    explicit QXmppMessageRetraction(QString retractedId);

    QString retractedId() const;
    void setRetractedId(const QString &id);

    const std::optional<QXmppMessageModeration> &moderation() const;
    void setModeration(const std::optional<QXmppMessageModeration> &moderation);

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
    static constexpr std::tuple XmlTag = { u"retract", QXmpp::Private::ns_message_retract };

private:
    QString m_retractedId;
    std::optional<QXmppMessageModeration> m_moderation;
};

class QXMPP_EXPORT QXmppMessageRetracted
{
public:
    QXmppMessageRetracted() = default;

    QDateTime stamp() const;
    void setStamp(const QDateTime &stamp);

    QString by() const;
    void setBy(const QString &by);

    const std::optional<QXmppMessageModeration> &moderation() const;
    void setModeration(const std::optional<QXmppMessageModeration> &moderation);

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
    static constexpr std::tuple XmlTag = { u"retracted", QXmpp::Private::ns_message_retract };

private:
    QDateTime m_stamp;
    QString m_by;
    std::optional<QXmppMessageModeration> m_moderation;
};

struct QXmppMessageRetractionLimitsPrivate;

class QXMPP_EXPORT QXmppMessageRetractionLimits : public QXmppExtensibleDataFormBase
{
public:
    /*! FORM_TYPE of this data form */
    static constexpr auto DataFormType = QXmpp::Private::ns_message_retract;
    static std::optional<QXmppMessageRetractionLimits> fromDataForm(const QXmppDataForm &form);

    QXmppMessageRetractionLimits();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMessageRetractionLimits)

    std::optional<std::chrono::seconds> maxAge() const;
    void setMaxAge(std::optional<std::chrono::seconds> maxAge);

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMessageRetractionLimitsPrivate> d;
};

#endif  // QXMPPMESSAGERETRACTION_H

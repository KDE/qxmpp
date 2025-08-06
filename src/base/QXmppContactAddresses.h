// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCONTACTADDRESSES_H
#define QXMPPCONTACTADDRESSES_H

#include "QXmppDataFormBase.h"

struct QXmppContactAddressesPrivate;

class QXMPP_EXPORT QXmppContactAddresses : public QXmppExtensibleDataFormBase
{
public:
    static std::optional<QXmppContactAddresses> fromDataForm(const QXmppDataForm &form);

    QXmppContactAddresses();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppContactAddresses)

    QList<QString> abuseAddresses() const;
    void setAbuseAddresses(const QList<QString> &);

    QList<QString> adminAddresses() const;
    void setAdminAddresses(const QList<QString> &newAdminAddresses);

    QList<QString> feedbackAddresses() const;
    void setFeedbackAddresses(const QList<QString> &newFeedbackAddresses);

    QList<QString> salesAddresses() const;
    void setSalesAddresses(const QList<QString> &newSalesAddresses);

    QList<QString> securityAddresses() const;
    void setSecurityAddresses(const QList<QString> &newSecurityAddresses);

    QList<QString> statusAddresses() const;
    void setStatusAddresses(const QList<QString> &newStatusAddresses);

    QList<QString> supportAddresses() const;
    void setSupportAddresses(const QList<QString> &newSupportAddresses);

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppContactAddressesPrivate> d;
};

#endif  // QXMPPCONTACTADDRESSES_H

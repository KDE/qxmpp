// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppContactAddresses.h"

#include "StringLiterals.h"

using namespace QXmpp::Private;

struct QXmppContactAddressesPrivate : QSharedData {
    QList<QString> abuseAddresses;
    QList<QString> adminAddresses;
    QList<QString> feedbackAddresses;
    QList<QString> salesAddresses;
    QList<QString> securityAddresses;
    QList<QString> statusAddresses;
    QList<QString> supportAddresses;
};

///
/// \class QXmppContactAddresses
///
/// Data form used in service discovery information for publishing service contact addresses. See
/// \xep{0157, Contact Addresses for XMPP Services} for details.
///
/// ```
/// QXmppDiscoveryIq iq;
/// if (auto contactAddresses = iq.dataForm<QXmppContactAddresses>()) {
///     auto abuseAddresses = contactAddresses->abuseAddresses();
/// }
/// ```
///
/// \since QXmpp 1.12
///

/// Parses QXmppDataForm into contact addresses and returns it if successful.
std::optional<QXmppContactAddresses> QXmppContactAddresses::fromDataForm(const QXmppDataForm &form)
{
    if (form.formType() != DataFormType) {
        return std::nullopt;
    }

    QXmppContactAddresses contactAddresses;
    if (QXmppDataFormBase::fromDataForm(form, contactAddresses)) {
        return contactAddresses;
    }
    return std::nullopt;
}

QXmppContactAddresses::QXmppContactAddresses()
    : d(new QXmppContactAddressesPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppContactAddresses)

/// Returns addresses for communication related to abusive traffic.
QList<QString> QXmppContactAddresses::abuseAddresses() const
{
    return d->abuseAddresses;
}

/// Sets addresses for communication related to abusive traffic.
void QXmppContactAddresses::setAbuseAddresses(const QList<QString> &abuseAddresses)
{
    d->abuseAddresses = abuseAddresses;
}

/// Returns addresses for communication with the service administrators.
QList<QString> QXmppContactAddresses::adminAddresses() const
{
    return d->adminAddresses;
}

/// Sets addresses for communication with the service administrators.
void QXmppContactAddresses::setAdminAddresses(const QList<QString> &newAdminAddresses)
{
    d->adminAddresses = newAdminAddresses;
}

/// Returns addresses for customer feedback.
QList<QString> QXmppContactAddresses::feedbackAddresses() const
{
    return d->feedbackAddresses;
}

/// Sets addresses for customer feedback.
void QXmppContactAddresses::setFeedbackAddresses(const QList<QString> &newFeedbackAddresses)
{
    d->feedbackAddresses = newFeedbackAddresses;
}

/// Returns addresses for communication related to sales and marketing.
QList<QString> QXmppContactAddresses::salesAddresses() const
{
    return d->salesAddresses;
}

/// Sets addresses for communication related to sales and marketing.
void QXmppContactAddresses::setSalesAddresses(const QList<QString> &newSalesAddresses)
{
    d->salesAddresses = newSalesAddresses;
}

/// Returns addresses for communication related to security concerns.
QList<QString> QXmppContactAddresses::securityAddresses() const
{
    return d->securityAddresses;
}

/// Sets addresses for communication related to security concerns.
void QXmppContactAddresses::setSecurityAddresses(const QList<QString> &newSecurityAddresses)
{
    d->securityAddresses = newSecurityAddresses;
}

/// Returns addresses for service status.
QList<QString> QXmppContactAddresses::statusAddresses() const
{
    return d->statusAddresses;
}

/// Sets addresses for service status.
void QXmppContactAddresses::setStatusAddresses(const QList<QString> &newStatusAddresses)
{
    d->statusAddresses = newStatusAddresses;
}

/// Returns addresses for customer support.
QList<QString> QXmppContactAddresses::supportAddresses() const
{
    return d->supportAddresses;
}

/// Sets addresses for customer support.
void QXmppContactAddresses::setSupportAddresses(const QList<QString> &newSupportAddresses)
{
    d->supportAddresses = newSupportAddresses;
}

QString QXmppContactAddresses::formType() const
{
    return ns_contact_addresses.toString();
}

bool QXmppContactAddresses::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (field.type() == Type::ListMultiField) {
        if (key == u"abuse-addresses") {
            d->abuseAddresses = value.toStringList();
        } else if (key == u"admin-addresses") {
            d->adminAddresses = value.toStringList();
        } else if (key == u"feedback-addresses") {
            d->feedbackAddresses = value.toStringList();
        } else if (key == u"sales-addresses") {
            d->salesAddresses = value.toStringList();
        } else if (key == u"security-addresses") {
            d->securityAddresses = value.toStringList();
        } else if (key == u"status-addresses") {
            d->statusAddresses = value.toStringList();
        } else if (key == u"support-addresses") {
            d->supportAddresses = value.toStringList();
        } else {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

void QXmppContactAddresses::serializeForm(QXmppDataForm &form) const
{
    using Type = QXmppDataForm::Field::Type;

    serializeEmptyable(form, Type::ListMultiField, u"abuse-addresses", d->abuseAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"admin-addresses", d->adminAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"feedback-addresses", d->feedbackAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"sales-addresses", d->salesAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"security-addresses", d->securityAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"status-addresses", d->statusAddresses);
    serializeEmptyable(form, Type::ListMultiField, u"support-addresses", d->supportAddresses);
}

// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppSendStanzaParams.h"

#include <QList>

using namespace QXmpp;

/*!
    \class QXmppSendStanzaParams
    \inmodule QXmpp

    Contains additional parameters for sending stanzas.

    \since QXmpp 1.5
*/

class QXmppSendStanzaParamsPrivate : public QSharedData
{
public:
    TrustLevels acceptedTrustLevels;
    QList<QString> encryptionJids;
    bool encryptionRequired = false;
    std::optional<QString> forcedEncryption;
};

QXmppSendStanzaParams::QXmppSendStanzaParams()
    : d(new QXmppSendStanzaParamsPrivate)
{
}

/*! Copy-constructor */
QXmppSendStanzaParams::QXmppSendStanzaParams(const QXmppSendStanzaParams &other) = default;
/*! Move-constructor */
QXmppSendStanzaParams::QXmppSendStanzaParams(QXmppSendStanzaParams &&) = default;
QXmppSendStanzaParams::~QXmppSendStanzaParams() = default;
/*! Assignment operator */
QXmppSendStanzaParams &QXmppSendStanzaParams::operator=(const QXmppSendStanzaParams &) = default;
/*! Move-assignment operator */
QXmppSendStanzaParams &QXmppSendStanzaParams::operator=(QXmppSendStanzaParams &&) = default;

/*!
    Returns the list of JIDs that the stanza should be encrypted for.

    If this is empty, the stanza should be encrypted for the recipient.
    This option is useful for groupchats.
*/
QList<QString> QXmppSendStanzaParams::encryptionJids() const
{
    return d->encryptionJids;
}

/*!
    Sets the list of JIDs that the stanza should be encrypted for.

    If this is empty, the stanza should be encrypted for the recipient.
    This option is useful for groupchats.

    \a encryptionJids.
*/
void QXmppSendStanzaParams::setEncryptionJids(QList<QString> encryptionJids)
{
    d->encryptionJids = std::move(encryptionJids);
}

/*!
    Returns the possible trust levels a key must have to be used for encryption (the trust
    levels of the keys used for encryption).

    If no trust levels are set, the encryption manager uses an own default.
*/
std::optional<TrustLevels> QXmppSendStanzaParams::acceptedTrustLevels() const
{
    if (d->acceptedTrustLevels) {
        return d->acceptedTrustLevels;
    }
    return {};
}

/*!
    Sets the possible \a trustLevels a key must have to be used for encryption.

    If no trust levels are set, the encryption manager uses an own default.
*/
void QXmppSendStanzaParams::setAcceptedTrustLevels(std::optional<TrustLevels> trustLevels)
{
    d->acceptedTrustLevels = trustLevels.value_or(QXmpp::TrustLevels());
}

/*!
    Returns whether sending a message or IQ fails if it can not be end-to-end encrypted.

    \since QXmpp 1.18
*/
bool QXmppSendStanzaParams::isEncryptionRequired() const
{
    return d->encryptionRequired;
}

/*!
    Sets whether sending a message or IQ fails if it can not be end-to-end encrypted.

    If set, sending fails with QXmpp::SendError::EncryptionError if no encryption is selected by
    the QXmpp::Messages::SelectEncryption hook (or QXmpp::Iq::SelectEncryption for IQs), instead
    of sending the stanza unencrypted.
    Sensitive stanzas sent by QXmpp's managers (and via the deprecated
    QXmppClient::sendSensitive()) are still encrypted with the deprecated encryption extension
    (QXmppClient::setEncryptionExtension()) if no encryption is selected.

    \a required.

    \since QXmpp 1.18
*/
void QXmppSendStanzaParams::setEncryptionRequired(bool required)
{
    d->encryptionRequired = required;
}

/*!
    Returns the end-to-end encryption forced for a message or IQ instead of selecting it via the
    QXmpp::Messages::SelectEncryption hook (or QXmpp::Iq::SelectEncryption for IQs): the namespace
    of the encryption (see QXmpp::E2ee) or an empty string for sending it unencrypted.

    By default, it is not set and the encryption is selected via the hook.

    \since QXmpp 1.18
*/
std::optional<QString> QXmppSendStanzaParams::forcedEncryption() const
{
    return d->forcedEncryption;
}

/*!
    Forces the end-to-end \a encryption of a message or IQ instead of selecting it via the
    QXmpp::Messages::SelectEncryption hook (or QXmpp::Iq::SelectEncryption for IQs): the namespace
    of the encryption (see QXmpp::E2ee) or an empty string for sending it unencrypted, e.g. after
    the user has confirmed that.

    Sending fails if no provider of the encryption is registered or if the stanza must be encrypted
    (isEncryptionRequired()) and an empty string is given.

    \since QXmpp 1.18
*/
void QXmppSendStanzaParams::setForcedEncryption(std::optional<QString> encryption)
{
    d->forcedEncryption = std::move(encryption);
}

// SPDX-FileCopyrightText: 2020 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppRegistrationManager.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppOutgoingClient.h"
#include "QXmppRegisterIq.h"
#include "QXmppStreamFeatures.h"
#include "QXmppUtils.h"

#include "Algorithms.h"
#include "StringLiterals.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

class QXmppRegistrationManagerPrivate
{
public:
    QXmppRegistrationManagerPrivate();

    // whether to block login and request the registration form on connect
    bool registerOnConnectEnabled;
    // Created on the first connection, since the QXmppDiscoveryManager may be registered after
    // this manager. Kept afterwards, so the support stays known on reconnection.
    std::optional<QXmppDiscoFeatureWatch> serverSupport;
    QPropertyNotifier serverSupportNotifier;

    // caching
    QString changePasswordIqId;
    QString newPassword;

    QString deleteAccountIqId;

    QString registrationIqId;
    QXmppRegisterIq registrationFormToSend;
};

QXmppRegistrationManagerPrivate::QXmppRegistrationManagerPrivate()
    : registerOnConnectEnabled(false)
{
}

/*! Default constructor. */
QXmppRegistrationManager::QXmppRegistrationManager()
    : d(std::make_unique<QXmppRegistrationManagerPrivate>())
{
}

QXmppRegistrationManager::~QXmppRegistrationManager() = default;

/*! This adds the \c jabber:iq:register namespace to the features. */
QStringList QXmppRegistrationManager::discoveryFeatures() const
{
    return QStringList {
        ns_register.toString()
    };
}

/*!
    Changes the password of the user's account to \a newPassword. \a newPassword must not be
    empty.

    \note Be sure to only call this when any previous requests have finished.
*/
void QXmppRegistrationManager::changePassword(const QString &newPassword)
{
    auto iq = QXmppRegisterIq::createChangePasswordRequest(client()->configuration().user(), newPassword);

    d->changePasswordIqId = iq.id();
    d->newPassword = newPassword;

    client()->send(std::move(iq));
}

/*!
    Cancels an existing registration on the server.

    \sa accountDeleted()
    \sa accountDeletionFailed()
*/
void QXmppRegistrationManager::deleteAccount()
{
    auto iq = QXmppRegisterIq::createUnregistrationRequest();
    d->deleteAccountIqId = iq.id();

    client()->setIgnoredStreamErrors({ QXmpp::StreamError::Conflict, QXmpp::StreamError::NotAuthorized });
    client()->send(std::move(iq));
}

/*!
    Returns a watch on whether the own server supports \xep{0077}{In-Band Registration}.

    This is only relevant to actions that happen after authentication.

    The information is requested as long as a copy of the watch exists and shared with all other
    watches on the server information.

    This requires the QXmppDiscoveryManager to be registered with the client. If the manager is
    not registered with a client, a watch that never loads is returned.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppRegistrationManager::watchServerSupport() const
{
    return watchServerFeature(client(), QXmpp::Namespace::Register);
}

/*!
    Returns whether the server supports registration.

    This is only relevant to actions that happen after authentication. This requires the
    QXmppDiscoveryManager to be registered with the client. The information is requested once
    the client is connected and kept while it is updated on reconnection.

    \deprecated since QXmpp 1.17, use watchServerSupport() instead.

    \sa QXmppRegistrationManager::supportedByServerChanged()
*/
bool QXmppRegistrationManager::supportedByServer() const
{
    return d->serverSupport && d->serverSupport->supported().value();
}

/*!
    Requests the registration form for registering.

    \a service is the service from which the registration form should be requested. If left
    empty, this will default to the local server.
*/
void QXmppRegistrationManager::requestRegistrationForm(const QString &service)
{
    QXmppRegisterIq iq;
    iq.setType(QXmppIq::Get);
    iq.setTo(service);
    client()->send(std::move(iq));
}

/*!
    Sets the completed registration form \a iq to be sent on the next connect with the server.
*/
void QXmppRegistrationManager::setRegistrationFormToSend(const QXmppRegisterIq &iq)
{
    d->registrationFormToSend = iq;
}

/*!
    Sets the completed data form \a dataForm for registration to be sent on the next connect
    with the server.
*/
void QXmppRegistrationManager::setRegistrationFormToSend(const QXmppDataForm &dataForm)
{
    d->registrationFormToSend = QXmppRegisterIq();
    d->registrationFormToSend.setForm(dataForm);
}

/*!
    Sends a completed registration form that was previously set using
    setRegistrationFormToSend().

    You usually only need to set the form and the manager will automatically
    send it when connected. More details can be found in the documentation of
    the QXmppRegistrationManager.
*/
void QXmppRegistrationManager::sendCachedRegistrationForm()
{
    if (auto form = d->registrationFormToSend.form(); !form.isNull()) {
        form.setType(QXmppDataForm::Submit);
        d->registrationFormToSend.setForm(form);
    }

    d->registrationFormToSend.setType(QXmppIq::Set);

    client()->send(std::move(d->registrationFormToSend));
    d->registrationIqId = d->registrationFormToSend.id();

    // clear cache
    d->registrationFormToSend = QXmppRegisterIq();
}

/*!
    Returns whether to only request the registration form and not to connect
    with username/password.
*/
bool QXmppRegistrationManager::registerOnConnectEnabled() const
{
    return d->registerOnConnectEnabled;
}

/*!
    Sets whether to only request the registration form and not to connect with
    username/password. Set \a enabled to true to register, false to connect normally.
*/
void QXmppRegistrationManager::setRegisterOnConnectEnabled(bool enabled)
{
    d->registerOnConnectEnabled = enabled;

    if (enabled) {
        client()->setIgnoredStreamErrors({ QXmpp::StreamError::ConnectionTimeout });
    } else {
        client()->setIgnoredStreamErrors({});
    }
}

bool QXmppRegistrationManager::handleStanza(const QDomElement &stanza)
{
    if (d->registerOnConnectEnabled && QXmppStreamFeatures::isStreamFeatures(stanza)) {
        QXmppStreamFeatures features;
        features.parse(stanza);

        // handle STARTTLS first (this is a workaround, registration management should better be
        // integrated into the OutgoingClient)
        if (client()->stream()->handleStarttls(features)) {
            return true;
        }

        if (features.registerMode() == QXmppStreamFeatures::Disabled) {
            warning(u"Could not request the registration form, because the server does not advertise the register stream feature."_s);
            client()->disconnectFromServer();
            Q_EMIT registrationFailed({ QXmppStanza::Error::Cancel,
                                        QXmppStanza::Error::FeatureNotImplemented,
                                        u"The server does not advertise the register stream feature."_s });
            return true;
        }

        if (!d->registrationFormToSend.form().isNull() || !d->registrationFormToSend.username().isNull()) {
            info(u"Sending completed form."_s);
            sendCachedRegistrationForm();
            return true;
        }

        info(u"Requesting registration form from server."_s);
        requestRegistrationForm();
        return true;
    }

    if (stanza.tagName() == u"iq") {
        const QString &id = stanza.attribute(u"id"_s);

        if (!id.isEmpty() && id == d->registrationIqId) {
            QXmppIq iq;
            iq.parse(stanza);

            switch (iq.type()) {
            case QXmppIq::Result:
                info(u"Successfully registered with the service."_s);
                Q_EMIT registrationSucceeded();
                break;
            case QXmppIq::Error:
                warning(u"Registering with the service failed: "_s.append(iq.error().text()));
                Q_EMIT registrationFailed(iq.error());
                break;
            default:
                break;  // should never occur
            }

            d->registrationIqId.clear();
            return true;
        } else if (!id.isEmpty() && id == d->changePasswordIqId) {
            QXmppIq iq;
            iq.parse(stanza);

            switch (iq.type()) {
            case QXmppIq::Result:
                info(u"Changed password successfully."_s);
                client()->configuration().setPassword(d->newPassword);
                Q_EMIT passwordChanged(d->newPassword);
                break;
            case QXmppIq::Error:
                warning(u"Failed to change password: "_s.append(iq.error().text()));
                Q_EMIT passwordChangeFailed(iq.error());
                break;
            default:
                break;  // should never occur
            }

            d->changePasswordIqId.clear();
            d->newPassword.clear();
            return true;
        } else if (!id.isEmpty() && id == d->deleteAccountIqId) {
            QXmppIq iq;
            iq.parse(stanza);

            switch (iq.type()) {
            case QXmppIq::Result:
                handleAccountDeleted();
                client()->disconnectFromServer();
                break;
            case QXmppIq::Error:
                warning(u"Failed to delete account: "_s.append(iq.error().text()));
                Q_EMIT accountDeletionFailed(iq.error());
                break;
            default:
                break;  // should never occur
            }

            d->deleteAccountIqId.clear();
            return true;
        } else if (isIqElement<QXmppRegisterIq>(stanza)) {
            QXmppRegisterIq iq;
            iq.parse(stanza);

            switch (iq.type()) {
            case QXmppIq::Result:
                info(u"Received registration form."_s);
                Q_EMIT registrationFormReceived(iq);
                break;
            case QXmppIq::Error:
                warning(u"Registration form could not be received: "_s.append(iq.error().text()));
                Q_EMIT registrationFailed(iq.error());
                break;
            default:
                break;  // should never occur
            }

            return true;
        }
    }
    return false;
}

void QXmppRegistrationManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, [this] {
        if (d->serverSupport || !this->client()->findExtension<QXmppDiscoveryManager>()) {
            return;
        }
        d->serverSupport = watchServerSupport();
        d->serverSupportNotifier = d->serverSupport->supported().addNotifier([this] {
            QT_WARNING_PUSH
            QT_WARNING_DISABLE_DEPRECATED
            Q_EMIT supportedByServerChanged();
            QT_WARNING_POP
        });
    });

    connect(client, &QXmppClient::disconnected, this, [this, client]() {
        client->setIgnoredStreamErrors({});

        if (!d->deleteAccountIqId.isEmpty()) {
            handleAccountDeleted();
            d->deleteAccountIqId.clear();
        }
    });
}

void QXmppRegistrationManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);

    const auto wasSupported = d->serverSupport && d->serverSupport->supported().value();
    d->serverSupportNotifier = {};
    d->serverSupport = {};
    if (wasSupported) {
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        Q_EMIT supportedByServerChanged();
        QT_WARNING_POP
    }
}

void QXmppRegistrationManager::handleAccountDeleted()
{
    info(u"Account deleted successfully."_s);
    Q_EMIT accountDeleted();
}

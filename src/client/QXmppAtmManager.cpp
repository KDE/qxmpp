// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppAtmManager.h"

#include "QXmppCarbonManager.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMessage.h"
#include "QXmppTrustMessageElement.h"
#include "QXmppTrustMessageKeyOwner.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Async.h"

using namespace QXmpp;
using namespace QXmpp::Private;

///
/// \class QXmppAtmManager
///
/// \brief The QXmppAtmManager class represents a manager for
/// \xep{0450, Automatic Trust Management (ATM)}.
///
/// For interacting with the storage, a corresponding implementation of the
/// storage interface must be added.
/// That implementation has to be adapted to your storage such as a database.
/// In case you only need memory and no peristent storage, you can use the
/// existing implementation and add the storage with it:
///
///\code
/// QXmppAtmTrustStorage *trustStorage = new QXmppAtmTrustMemoryStorage;
/// QXmppAtmManager *manager = new QXmppAtmManager(trustStorage);
/// client->addExtension(manager);
/// \endcode
///
/// It is strongly recommended to enable \xep{0280, Message Carbons} with
/// \code
/// QXmppCarbonManager *carbonManager = new QXmppCarbonManager;
/// carbonManager->setCarbonsEnabled(true);
/// client->addExtension(carbonManager);
/// \endcode
/// and \xep{0313, Message Archive Management} with
/// \code
/// QXmppMamManager *mamManager = new QXmppMamManager;
/// client->addExtension(mamManager);
/// \endcode
/// for delivering trust messages to all online and offline endpoints.
///
/// In addition, archiving via MAM must be enabled on the server.
///
/// \ingroup Managers
///
/// \since QXmpp 1.5
///

///
/// Constructs an ATM manager.
///
/// \param trustStorage trust storage implementation
///
QXmppAtmManager::QXmppAtmManager(QXmppAtmTrustStorage *trustStorage)
    : QXmppTrustManager(trustStorage)
{
}

///
/// Authenticates or distrusts keys manually (e.g., by the Trust Message URI of
/// a scanned QR code or after entering key IDs by hand) and sends corresponding
/// trust messages.
///
/// \param encryption encryption protocol namespace
/// \param keyOwnerJid JID of the key owner
/// \param keyIdsForAuthentication IDs of the keys being authenticated
/// \param keyIdsForDistrusting IDs of the keys being distrusted
///
QXmppTask<void> QXmppAtmManager::makeTrustDecisions(const QString &encryption, const QString &keyOwnerJid, const QList<QByteArray> &keyIdsForAuthentication, const QList<QByteArray> &keyIdsForDistrusting)
{
    auto retrievedKeys = co_await keys(encryption, TrustLevel::Authenticated | TrustLevel::ManuallyDistrusted).withContext(this);

    const auto authenticatedKeys = retrievedKeys.value(TrustLevel::Authenticated);
    const auto manuallyDistrustedKeys = retrievedKeys.value(TrustLevel::ManuallyDistrusted);
    const auto ownJid = client()->configuration().jidBare();
    const auto ownAuthenticatedKeys = authenticatedKeys.values(ownJid);

    // Create a key owner for the keys being authenticated or
    // distrusted.
    QXmppTrustMessageKeyOwner keyOwner;
    keyOwner.setJid(keyOwnerJid);

    QList<QByteArray> modifiedAuthenticatedKeys;
    QList<QByteArray> modifiedManuallyDistrustedKeys;

    for (const auto &keyId : keyIdsForAuthentication) {
        if (!authenticatedKeys.contains(keyOwnerJid, keyId)) {
            modifiedAuthenticatedKeys.append(keyId);
        }
    }

    for (const auto &keyId : keyIdsForDistrusting) {
        if (!manuallyDistrustedKeys.contains(keyOwnerJid, keyId)) {
            modifiedManuallyDistrustedKeys.append(keyId);
        }
    }

    if (modifiedAuthenticatedKeys.isEmpty() && modifiedManuallyDistrustedKeys.isEmpty()) {
        // Skip further processing if there are no changes.
        co_return;
    }
    keyOwner.setTrustedKeys(modifiedAuthenticatedKeys);
    keyOwner.setDistrustedKeys(modifiedManuallyDistrustedKeys);

    QMultiHash<QString, QByteArray> keysBeingAuthenticated;
    QMultiHash<QString, QByteArray> keysBeingDistrusted;

    for (const auto &key : std::as_const(modifiedAuthenticatedKeys)) {
        keysBeingAuthenticated.insert(keyOwnerJid, key);
    }

    for (const auto &key : std::as_const(modifiedManuallyDistrustedKeys)) {
        keysBeingDistrusted.insert(keyOwnerJid, key);
    }

    // Create a key owner for authenticated and distrusted keys of own
    // endpoints.
    QXmppTrustMessageKeyOwner ownKeyOwner;
    ownKeyOwner.setJid(ownJid);

    if (!ownAuthenticatedKeys.isEmpty()) {
        ownKeyOwner.setTrustedKeys(ownAuthenticatedKeys);
    }

    const auto ownManuallyDistrustedKeys = manuallyDistrustedKeys.values(ownJid);
    if (!ownManuallyDistrustedKeys.isEmpty()) {
        ownKeyOwner.setDistrustedKeys(ownManuallyDistrustedKeys);
    }

    const auto areOwnKeysProcessed = keyOwnerJid == ownJid;
    if (areOwnKeysProcessed) {
        auto contactsAuthenticatedKeys = authenticatedKeys;
        contactsAuthenticatedKeys.remove(ownJid);

        const auto contactsWithAuthenticatedKeys = contactsAuthenticatedKeys.uniqueKeys();

        // Send trust messages for the keys of the own endpoints being
        // authenticated or distrusted to endpoints of contacts with
        // authenticated keys.
        // Own endpoints with authenticated keys can receive the trust
        // messages via Message Carbons.
        for (const auto &contactJid : contactsWithAuthenticatedKeys) {
            sendTrustMessage(encryption, { keyOwner }, contactJid);
        }

        // Send a trust message for the keys of the own endpoints being
        // authenticated or distrusted to other own endpoints with
        // authenticated keys.
        // It is skipped if a trust message is already delivered via
        // Message Carbons or there are no other own endpoints with
        // authenticated keys.
        // FIXME: QXmppCarbonManager has been replaced
        const auto *carbonManager = client()->findExtension<QXmppCarbonManager>();
        const auto isMessageCarbonsDisabled = !carbonManager || !carbonManager->carbonsEnabled();
        if (isMessageCarbonsDisabled || (contactsAuthenticatedKeys.isEmpty() && !ownAuthenticatedKeys.isEmpty())) {
            sendTrustMessage(encryption, { keyOwner }, ownJid);
        }

        co_await makeTrustDecisions(encryption, keysBeingAuthenticated, keysBeingDistrusted)
            .withContext(this);

        // Send a trust message for all authenticated or distrusted
        // keys to the own endpoints whose keys have been
        // authenticated.
        // It is skipped if no keys of own endpoints have been
        // authenticated.
        if (!keyOwner.trustedKeys().isEmpty()) {
            auto contactsManuallyDistrustedKeys = manuallyDistrustedKeys;
            contactsManuallyDistrustedKeys.remove(ownJid);

            auto contactJids = contactsManuallyDistrustedKeys.uniqueKeys() + contactsWithAuthenticatedKeys;

            // Remove duplicates from contactJids.
            std::sort(contactJids.begin(), contactJids.end());
            contactJids.erase(std::unique(contactJids.begin(), contactJids.end()), contactJids.end());

            QList<QXmppTrustMessageKeyOwner> allKeyOwners;

            for (const auto &contactJid : std::as_const(contactJids)) {
                QXmppTrustMessageKeyOwner contactKeyOwner;
                contactKeyOwner.setJid(contactJid);
                contactKeyOwner.setTrustedKeys(contactsAuthenticatedKeys.values(contactJid));

                if (const auto contactManuallyDistrustedKeys = contactsManuallyDistrustedKeys.values(contactJid); !contactsManuallyDistrustedKeys.isEmpty()) {
                    contactKeyOwner.setDistrustedKeys(contactManuallyDistrustedKeys);
                }

                allKeyOwners.append(contactKeyOwner);
            }

            if (!(ownKeyOwner.trustedKeys().isEmpty() && ownKeyOwner.distrustedKeys().isEmpty())) {
                allKeyOwners.append(ownKeyOwner);
            }

            if (!allKeyOwners.isEmpty()) {
                sendTrustMessage(encryption, allKeyOwners, ownJid);
            }
        }
    } else {
        // Send a trust message for the keys of the contact's endpoints
        // being authenticated or distrusted to own endpoints
        // with authenticated keys.
        if (!ownAuthenticatedKeys.isEmpty()) {
            sendTrustMessage(encryption, { keyOwner }, ownJid);
        }

        co_await makeTrustDecisions(encryption, keysBeingAuthenticated, keysBeingDistrusted).withContext(this);

        // Send a trust message for own authenticated or distrusted
        // keys to the contact's endpoints whose keys have been
        // authenticated.
        // It is skipped if no keys of contacts have been
        // authenticated or there are no keys for the trust message.
        if (!keyOwner.trustedKeys().isEmpty() && !(ownKeyOwner.trustedKeys().isEmpty() && ownKeyOwner.distrustedKeys().isEmpty())) {
            sendTrustMessage(encryption, { ownKeyOwner }, keyOwnerJid);
        }
    }
}

/// \cond
void QXmppAtmManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::messageReceived, this, &QXmppAtmManager::handleMessage);
}

void QXmppAtmManager::onUnregistered(QXmppClient *client)
{
    disconnect(client, &QXmppClient::messageReceived, this, &QXmppAtmManager::handleMessage);
}
/// \endcond

///
/// Authenticates or distrusts keys.
///
/// \param encryption encryption protocol namespace
/// \param keyIdsForAuthentication key owners' bare JIDs mapped to the IDs of
///        their keys being authenticated
/// \param keyIdsForDistrusting key owners' bare JIDs mapped to the IDs of their
///        keys being distrusted
///
QXmppTask<void> QXmppAtmManager::makeTrustDecisions(const QString &encryption, const QMultiHash<QString, QByteArray> &keyIdsForAuthentication, const QMultiHash<QString, QByteArray> &keyIdsForDistrusting)
{
    // authenticate
    if (!keyIdsForAuthentication.isEmpty()) {
        co_await setTrustLevel(encryption, keyIdsForAuthentication, TrustLevel::Authenticated).withContext(this);
        if (co_await securityPolicy(encryption).withContext(this) == Toakafa) {
            co_await distrustAutomaticallyTrustedKeys(encryption, keyIdsForAuthentication.uniqueKeys()).withContext(this);
        }
        co_await makePostponedTrustDecisions(encryption, keyIdsForAuthentication.values()).withContext(this);
    }
    // distrust
    if (!keyIdsForDistrusting.isEmpty()) {
        co_await setTrustLevel(encryption, keyIdsForDistrusting, TrustLevel::ManuallyDistrusted).withContext(this);
        co_await trustStorage()->removeKeysForPostponedTrustDecisions(encryption, keyIdsForDistrusting.values());
    }
}

///
/// Handles incoming messages and uses included trust message elements for
/// making automatic trust decisions.
///
/// \param message message that can contain a trust message element
///
QXmppTask<void> QXmppAtmManager::handleMessage(const QXmppMessage &message)
{
    const auto trustMessageElement = message.trustMessageElement();

    // Skip further processing in the following cases:
    // 1. The message does not contain an ATM trust message element.
    // 2. The trust message is sent by this endpoint and reflected via
    //    Message Carbons.
    if (!trustMessageElement ||
        trustMessageElement->usage() != ns_atm ||
        message.from() == client()->configuration().jid()) {
        co_return;
    }

    const auto senderJid = QXmppUtils::jidToBareJid(message.from());
    const auto e2eeMetadata = message.e2eeMetadata();
    const auto senderKey = e2eeMetadata ? e2eeMetadata->senderKey() : QByteArray();
    const auto encryption = trustMessageElement->encryption();

    auto senderKeyTrustLevel = co_await trustLevel(encryption, senderJid, senderKey).withContext(this);

    // key owner JIDs mapped to key IDs
    QMultiHash<QString, QByteArray> keysBeingAuthenticated;
    QMultiHash<QString, QByteArray> keysBeingDistrusted;

    QList<QXmppTrustMessageKeyOwner> keyOwnersForPostponedTrustDecisions;

    const auto ownJid = client()->configuration().jidBare();
    const auto isOwnTrustMessage = senderJid == ownJid;
    const auto keyOwners = trustMessageElement->keyOwners();

    for (const auto &keyOwner : keyOwners) {
        const auto keyOwnerJid = keyOwner.jid();

        // A trust message from an own endpoint is allowed to
        // authenticate or distrust the keys of own endpoints and
        // endpoints of contacts.
        // Whereas a trust message from an endpoint of a contact is
        // only allowed to authenticate or distrust the keys of that
        // contact's own endpoints.
        const auto isSenderQualifiedForTrustDecisions = isOwnTrustMessage || senderJid == keyOwnerJid;
        if (isSenderQualifiedForTrustDecisions) {
            // Make trust decisions if the key of the sender is
            // authenticated.
            // Othwerwise, store the keys of the trust message for
            // making the trust decisions as soon as the key of the
            // sender is authenticated.
            if (senderKeyTrustLevel == TrustLevel::Authenticated) {
                const auto trustedKeys = keyOwner.trustedKeys();
                for (const auto &key : trustedKeys) {
                    keysBeingAuthenticated.insert(keyOwnerJid, key);
                }

                const auto distrustedKeys = keyOwner.distrustedKeys();
                for (const auto &key : distrustedKeys) {
                    keysBeingDistrusted.insert(keyOwnerJid, key);
                }
            } else {
                keyOwnersForPostponedTrustDecisions.append(keyOwner);
            }
        }
    }

    auto addPostponedTask = trustStorage()->addKeysForPostponedTrustDecisions(encryption, senderKey, keyOwnersForPostponedTrustDecisions);
    auto trustDecisionTask = makeTrustDecisions(encryption, keysBeingAuthenticated, keysBeingDistrusted);
    co_await addPostponedTask;
    co_await trustDecisionTask;
}

///
/// Distrusts all formerly automatically trusted keys (as specifed by the
/// security policy TOAKAFA).
///
/// \param encryption encryption protocol namespace
/// \param keyOwnerJids bare JIDs of the key owners
///
QXmppTask<void> QXmppAtmManager::distrustAutomaticallyTrustedKeys(const QString &encryption, const QList<QString> &keyOwnerJids)
{
    return setTrustLevel(encryption, keyOwnerJids, TrustLevel::AutomaticallyTrusted, TrustLevel::AutomaticallyDistrusted);
}

///
/// Authenticates or distrusts keys for whom earlier trust messages were
/// received but not used for authenticating or distrusting at that time.
///
/// As soon as the senders' keys have been authenticated, all postponed trust
/// decisions can be performed by this method.
///
/// \param encryption encryption protocol namespace
/// \param senderKeyIds IDs of the keys that were used by the senders
///
QXmppTask<void> QXmppAtmManager::makePostponedTrustDecisions(const QString &encryption, const QList<QByteArray> &senderKeyIds)
{
    auto postponedTrustDecisionKeys =
        co_await trustStorage()
            ->keysForPostponedTrustDecisions(encryption, senderKeyIds)
            .withContext(this);

    // JIDs of key owners mapped to the IDs of their keys
    auto keysBeingAuthenticated = postponedTrustDecisionKeys.value(true);
    auto keysBeingDistrusted = postponedTrustDecisionKeys.value(false);

    auto cleanupTask = trustStorage()->removeKeysForPostponedTrustDecisions(encryption, keysBeingAuthenticated.values(), keysBeingDistrusted.values());
    auto trustDecisionTask = makeTrustDecisions(encryption, keysBeingAuthenticated, keysBeingDistrusted);
    co_await cleanupTask;
    co_await trustDecisionTask;
}

///
/// Sends a trust message.
///
/// \param encryption namespace of the encryption
/// \param keyOwners key owners containing the data for authentication or distrusting
/// \param recipientJid JID of the recipient
///
QXmppTask<QXmpp::SendResult> QXmppAtmManager::sendTrustMessage(const QString &encryption, const QList<QXmppTrustMessageKeyOwner> &keyOwners, const QString &recipientJid)
{
    QXmppTrustMessageElement trustMessageElement;
    trustMessageElement.setUsage(ns_atm.toString());
    trustMessageElement.setEncryption(encryption);
    trustMessageElement.setKeyOwners(keyOwners);

    QXmppMessage message;
    message.setTo(recipientJid);
    message.setTrustMessageElement(trustMessageElement);

    QXmppSendStanzaParams params;
    params.setAcceptedTrustLevels(TrustLevel::Authenticated);

    return client()->sendSensitive(std::move(message), params);
}

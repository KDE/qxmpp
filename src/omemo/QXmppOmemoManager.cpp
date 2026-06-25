// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppOmemoElement_p.h"
#include "QXmppOmemoIq_p.h"
#include "QXmppOmemoItems_p.h"
#include "QXmppOmemoManager_p.h"
#include "QXmppPubSubEvent.h"
#include "QXmppTrustManager.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Async.h"
#include "StringLiterals.h"

#include <QStringBuilder>

#undef max
#undef interface

using namespace QXmpp;
using namespace QXmpp::Private;
using namespace QXmpp::Omemo::Private;

using Manager = QXmppOmemoManager;
using ManagerPrivate = QXmppOmemoManagerPrivate;

// default label used for the own device
const auto DEVICE_LABEL = u"QXmpp"_s;

class QXmppOmemoOwnDevicePrivate : public QSharedData
{
public:
    QString label;
    QByteArray keyId;
};

/*!
    \class QXmppOmemoOwnDevice
    \inmodule QXmpp

    \brief The QXmppOmemoOwnDevice class represents the \xep{0384}{OMEMO Encryption} device of this
    client instance.
*/

/*! Constructs an OMEMO device for this client instance. */
QXmppOmemoOwnDevice::QXmppOmemoOwnDevice()
    : d(new QXmppOmemoOwnDevicePrivate)
{
}

/*! Copy-constructor. */
QXmppOmemoOwnDevice::QXmppOmemoOwnDevice(const QXmppOmemoOwnDevice &other) = default;
/*! Move-constructor. */
QXmppOmemoOwnDevice::QXmppOmemoOwnDevice(QXmppOmemoOwnDevice &&) noexcept = default;
QXmppOmemoOwnDevice::~QXmppOmemoOwnDevice() = default;
/*! Assignment operator. */
QXmppOmemoOwnDevice &QXmppOmemoOwnDevice::operator=(const QXmppOmemoOwnDevice &) = default;
/*! Move-assignment operator. */
QXmppOmemoOwnDevice &QXmppOmemoOwnDevice::operator=(QXmppOmemoOwnDevice &&) = default;

/*!
    Returns the human-readable string used to identify the device by users.

    If no label is set, a default-constructed QString is returned.
*/
QString QXmppOmemoOwnDevice::label() const
{
    return d->label;
}

/*!
    Sets an optional human-readable string \a label used to identify the device
    by users.

    The label should not contain more than 53 characters.
*/
void QXmppOmemoOwnDevice::setLabel(const QString &label)
{
    d->label = label;
}

/*!
    Returns the ID of the public long-term key which never changes.
*/
QByteArray QXmppOmemoOwnDevice::keyId() const
{
    return d->keyId;
}

/*!
    Sets the ID of the public long-term key (which never changes) to \a keyId.
*/
void QXmppOmemoOwnDevice::setKeyId(const QByteArray &keyId)
{
    d->keyId = keyId;
}

class QXmppOmemoDevicePrivate : public QSharedData
{
public:
    QString jid;
    TrustLevel trustLevel = TrustLevel::Undecided;
    QString label;
    QByteArray keyId;
};

/*!
    \class QXmppOmemoDevice
    \inmodule QXmpp

    \brief The QXmppOmemoDevice class represents a \xep{0384}{OMEMO Encryption} device.
*/

/*! Constructs an OMEMO device. */
QXmppOmemoDevice::QXmppOmemoDevice()
    : d(new QXmppOmemoDevicePrivate)
{
}

/*! Copy-constructor. */
QXmppOmemoDevice::QXmppOmemoDevice(const QXmppOmemoDevice &other) = default;
/*! Move-constructor. */
QXmppOmemoDevice::QXmppOmemoDevice(QXmppOmemoDevice &&) noexcept = default;
QXmppOmemoDevice::~QXmppOmemoDevice() = default;
/*! Assignment operator. */
QXmppOmemoDevice &QXmppOmemoDevice::operator=(const QXmppOmemoDevice &) = default;
/*! Move-assignment operator. */
QXmppOmemoDevice &QXmppOmemoDevice::operator=(QXmppOmemoDevice &&) = default;

/*!
    Returns the device owner's bare JID.
*/
QString QXmppOmemoDevice::jid() const
{
    return d->jid;
}

/*!
    Sets the device owner's bare JID to \a jid.
*/
void QXmppOmemoDevice::setJid(const QString &jid)
{
    d->jid = jid;
}

/*!
    Returns the human-readable string used to identify the device by users.

    If no label is set, a default-constructed QString is returned.
*/
QString QXmppOmemoDevice::label() const
{
    return d->label;
}

/*!
    Sets an optional human-readable string \a label used to identify the device
    by users.

    The label should not contain more than 53 characters.
*/
void QXmppOmemoDevice::setLabel(const QString &label)
{
    d->label = label;
}

/*!
    Returns the ID of the public long-term key which never changes.
*/
QByteArray QXmppOmemoDevice::keyId() const
{
    return d->keyId;
}

/*!
    Sets the ID of the public long-term key (which never changes) to \a keyId.
*/
void QXmppOmemoDevice::setKeyId(const QByteArray &keyId)
{
    d->keyId = keyId;
}

/*!
    Returns the trust level of the key.
*/
TrustLevel QXmppOmemoDevice::trustLevel() const
{
    return d->trustLevel;
}

/*!
    Sets the trust level of the key to \a trustLevel.
*/
void QXmppOmemoDevice::setTrustLevel(TrustLevel trustLevel)
{
    d->trustLevel = trustLevel;
}

/*!
    \class QXmppOmemoManager
    \inmodule QXmpp

    The QXmppOmemoManager class manages OMEMO encryption as defined in \xep{0384}{OMEMO Encryption}.

    OMEMO uses \xep{0060}{Publish-Subscribe} (PubSub) and \xep{0163}{Personal Eventing Protocol}
    (PEP).
    Thus, they must be supported by the server and the corresponding PubSub manager must be added to
    the client:
    \code
    QXmppPubSubManager *pubSubManager = new QXmppPubSubManager;
    client->addExtension(pubSubManager);
    \endcode

    For interacting with the storage, corresponding implementations of the storage interfaces must
    be instantiated.
    Those implementations have to be adapted to your storage such as a database.
    In case you only need memory and no persistent storage, you can use the existing
    implementations:
    \code
    QXmppOmemoStorage *omemoStorage = new QXmppOmemoMemoryStorage;
    QXmppTrustStorage *trustStorage = new QXmppTrustMemoryStorage;
    \endcode

    A trust manager using its storage must be added to the client:
    \code
    client->addNewExtension<QXmppAtmManager>(trustStorage);
    \endcode

    Afterwards, the OMEMO manager using its storage must be added to the client and activated to be
    used for encryption:
    \code
    auto *manager = client->addNewExtension<QXmppOmemoManager>(omemoStorage);
    client->setEncryptionExtension(manager);
    \endcode

    You can set a security policy used by OMEMO.
    Is is recommended to apply TOAKAFA for good security and usability when using
    \xep{0450}{Automatic Trust Management (ATM)}:
    \code
    manager->setSecurityPolicy(QXmpp::Toakafa);
    \endcode

    \xep{0280}{Message Carbons} should be used for delivering messages to all endpoints of a user:
    \code
    client->addNewExtension<QXmppCarbonManagerV2>();
    \endcode

    The old QXmppCarbonManager cannot be used with OMEMO.

    The OMEMO data must be loaded before connecting to the server:
    \code
    manager->load();
    });
    \endcode

    If no OMEMO data could be loaded (i.e., the result of \c load() is "false"), it must be set up
    first.
    That can be done as soon as the user is logged in to the server:
    \code
    connect(client, &QXmppClient::connected, this, [=]() {
    auto future = manager->start();
    });
    \endcode

    Once the future is finished and the result is "true", the manager is ready for use.
    Otherwise, check the logging output for details.

    The OMEMO manager is initialized after a successful call (i.e., the result is "true") of load()
    or setUp().

    By default, stanzas are only sent to devices having keys with the following trust levels:
    \code
    QXmpp::TrustLevel::AutomaticallyTrusted | QXmpp::TrustLevel::ManuallyTrusted
    | QXmpp::TrustLevel::Authenticated
    \endcode
    That behavior can be changed for each message being sent by specifying the
    accepted trust levels:
    \code
    QXmppSendStanzaParams params;
    params.setAcceptedTrustLevels(QXmpp::TrustLevel::Authenticated)
    client->send(stanza, params);
    \endcode

    Stanzas can be encrypted for multiple JIDs which is needed in group chats:
    \code
    QXmppSendStanzaParams params;
    params.setEncryptionJids({ "alice@example.org", "bob@example.com" })
    client->send(stanza, params);
    \endcode

    \ingroup Managers

    \since QXmpp 1.5
*/

/*!
    \typedef QXmppOmemoManager::Result

    Contains QXmpp::Success for success or an QXmppStanza::Error for an error.
*/

/*!
    Constructs an OMEMO manager using \a omemoStorage to store all OMEMO data.
*/
QXmppOmemoManager::QXmppOmemoManager(QXmppOmemoStorage *omemoStorage)
    : d(new ManagerPrivate(this, omemoStorage))
{
    d->ownDevice.label = DEVICE_LABEL;
    d->initOmemoLibrary();
    d->schedulePeriodicTasks();
}

QXmppOmemoManager::~QXmppOmemoManager() = default;

/*!
    Loads all locally stored OMEMO data.

    This should be called after starting the client and before the login.
    It must only be called after \c setUp() has been called once for the user
    during one of the past login sessions.
    It does not need to be called if setUp() has been called during the current
    login session.

    Returns whether everything is loaded successfully.

    \sa QXmppOmemoManager::setUp()
*/
QXmppTask<bool> Manager::load()
{
    if (d->initialized) {
        co_return true;
    }

    auto omemoData = co_await d->omemoStorage->allData().withContext(this);
    const auto &optionalOwnDevice = omemoData.ownDevice;
    if (optionalOwnDevice) {
        d->ownDevice = *optionalOwnDevice;
    } else {
        debug(u"Device could not be loaded because it is not stored"_s);
        co_return false;
    }

    const auto &signedPreKeyPairs = omemoData.signedPreKeyPairs;
    if (signedPreKeyPairs.isEmpty()) {
        warning(u"Signed pre keys could not be loaded because none is stored"_s);
        co_return false;
    } else {
        d->signedPreKeyPairs = signedPreKeyPairs;
    }

    const auto &preKeyPairs = omemoData.preKeyPairs;
    if (preKeyPairs.isEmpty()) {
        warning(u"Pre keys could not be loaded because none is stored"_s);
        co_return false;
    } else {
        d->preKeyPairs = preKeyPairs;
    }

    d->devices = omemoData.devices;
    d->removeDevicesRemovedFromServer();

    d->initialized = true;
    co_return true;
}

/*!
    Sets up all OMEMO data locally and on the server.

    The user must be logged in while calling this. \a deviceLabel is a
    human-readable string used to identify the own device.

    Returns whether everything is set up successfully.
*/
QXmppTask<bool> Manager::setUp(QString deviceLabel)
{
    if (!co_await d->setUpDeviceId()) {
        co_return false;
    }

    // The identity key pair in its deserialized form is not stored as a
    // member variable because it is only needed by
    // updateSignedPreKeyPair().
    RefCountedPtr<ratchet_identity_key_pair> identityKeyPair;

    if (d->setUpIdentityKeyPair(identityKeyPair.ptrRef()) &&
        d->updateSignedPreKeyPair(identityKeyPair.get()) &&
        d->updatePreKeyPairs(PRE_KEY_INITIAL_CREATION_COUNT)) {
        d->ownDevice.label = deviceLabel;

        co_await d->omemoStorage->setOwnDevice(d->ownDevice);
        d->initialized = co_await d->publishOmemoData();

        co_return d->initialized;
    }
    co_return false;
}

/*!
    Returns the key of this client instance.
*/
QXmppTask<QByteArray> Manager::ownKey()
{
    return d->trustManager->ownKey(staticString(ns_omemo_2));
}

/*!
    Returns the JIDs of all key owners mapped to the IDs of their keys with
    the specified \a trustLevels except for the own key.

    If no trust levels are passed, all keys are returned.

    This should be called in order to get all stored keys which can be more than
    the stored devices because of trust decisions made without a published or
    received device.
*/
QXmppTask<QHash<QXmpp::TrustLevel, QMultiHash<QString, QByteArray>>> Manager::keys(QXmpp::TrustLevels trustLevels)
{
    return d->trustManager->keys(staticString(ns_omemo_2), trustLevels);
}

/*!
    Returns the IDs of keys mapped to their trust levels for the key owners
    with the bare JIDs in \a jids and matching the specified \a trustLevels,
    except for the own key.

    If no trust levels are passed, all keys for the JIDs are returned.

    This should be called in order to get the stored keys which can be more than
    the stored devices because of trust decisions made without a published or
    received device.
*/
QXmppTask<QHash<QString, QHash<QByteArray, QXmpp::TrustLevel>>> Manager::keys(const QList<QString> &jids, QXmpp::TrustLevels trustLevels)
{
    return d->trustManager->keys(staticString(ns_omemo_2), jids, trustLevels);
}

/*!
    Changes the label of the own (this client instance's current user's) device
    to \a deviceLabel.

    The label is a human-readable string used to identify the device by users.

    Returns whether the action was successful.
*/
QXmppTask<bool> Manager::changeDeviceLabel(const QString &deviceLabel)
{
    return d->changeDeviceLabel(deviceLabel);
}

/*!
    Returns the maximum count of devices stored per JID.

    If more devices than that maximum are received for one JID from a server,
    they will not be stored locally and thus not used for encryption.
*/
int Manager::maximumDevicesPerJid() const
{
    return d->maximumDevicesPerJid;
}

/*!
    Sets the \a maximum count of devices stored per JID.

    If more devices than that maximum are received for one JID from a server,
    they will not be stored locally and thus not used for encryption.
*/
void Manager::setMaximumDevicesPerJid(int maximum)
{
    d->maximumDevicesPerJid = maximum;
}

/*!
    Returns the maximum count of devices for whom a stanza is encrypted.

    If more devices than that maximum are stored for all addressed recipients of
    a stanza, the stanza will only be encrypted for first devices until the
    maximum is reached.
*/
int Manager::maximumDevicesPerStanza() const
{
    return d->maximumDevicesPerStanza;
}

/*!
    Sets the \a maximum count of devices for whom a stanza is encrypted.

    If more devices than that maximum are stored for all addressed recipients of
    a stanza, the stanza will only be encrypted for first devices until the
    maximum is reached.
*/
void Manager::setMaximumDevicesPerStanza(int maximum)
{
    d->maximumDevicesPerStanza = maximum;
}

/*!
    Requests device lists from contacts with the JIDs in \a jids and stores
    them locally.

    The user must be logged in while calling this.
    The JID of the current user must not be passed.

    Returns the results of the requests for each JID.
*/
QXmppTask<QList<Manager::DevicesResult>> Manager::requestDeviceLists(QList<QString> jids)
{
    Q_ASSERT_X(!jids.contains(d->ownBareJid()), "Requesting contact's device list", "Own JID passed");
    QList<Manager::DevicesResult> devicesResults;

    // Process each JID sequentially to avoid MSVC coroutine ICE with complex template types
    for (const auto &jid : jids) {
        auto result = co_await d->requestDeviceList(jid);
        devicesResults << DevicesResult {
            jid,
            mapSuccess(std::move(result), [](QXmppOmemoDeviceListItem) { return Success(); })
        };
    }

    co_return devicesResults;
}

/*!
    Subscribes the current user's resource to device lists manually for the
    contacts with the JIDs in \a jids.

    This should be called after each login and only for contacts without
    presence subscription because their device lists are not automatically
    subscribed.
    The user must be logged in while calling this.

    Call \c QXmppOmemoManager::unsubscribeFromDeviceLists() before logout.

    Returns the results of the subscription for each JID.
*/
QXmppTask<QList<Manager::DevicesResult>> Manager::subscribeToDeviceLists(QList<QString> jids)
{
    QList<DevicesResult> devicesResults;
    devicesResults.reserve(jids.size());

    // Process each JID sequentially to avoid MSVC coroutine ICE with complex template types
    for (const auto &jid : jids) {
        auto result = co_await d->subscribeToDeviceList(jid);
        devicesResults.push_back(DevicesResult { jid, std::move(result) });
    }

    co_return devicesResults;
}

/*!
    Unsubscribes the current user's resource from all device lists that were
    manually subscribed by \c QXmppOmemoManager::subscribeToDeviceList().

    This should be called before each logout.
    The user must be logged in while calling this.

    Returns the results of the unsubscription for each JID.
*/
QXmppTask<QList<Manager::DevicesResult>> Manager::unsubscribeFromDeviceLists()
{
    return d->unsubscribeFromDeviceLists(d->jidsOfManuallySubscribedDevices);
}

/*!
    Returns the device of this client instance's current user.
*/
QXmppOmemoOwnDevice Manager::ownDevice()
{
    QXmppOmemoOwnDevice device;
    device.setLabel(d->ownDevice.label);
    device.setKeyId(d->ownDevice.publicIdentityKey);

    return device;
}

/*!
    Returns all locally stored devices except the own device.

    Only devices that have been received after subscribing the corresponding device lists on the
    server are stored locally.
    Thus, only those are returned.
    Call \c QXmppOmemoManager::subscribeToDeviceLists() for contacts without presence subscription
    before.

    You must build sessions before you can get devices with corresponding keys.
*/
QXmppTask<QList<QXmppOmemoDevice>> Manager::devices()
{
    return devices(d->devices.keys());
}

/*!
    Returns the locally stored devices for the JIDs in \a jids, except the own
    device.

    Only devices that have been received after subscribing the corresponding device lists on the
    server are stored locally.
    Thus, only those are returned.
    Call \c QXmppOmemoManager::subscribeToDeviceLists() for contacts without presence subscription
    before.

    You must build sessions before you can get devices with corresponding keys.
*/
QXmppTask<QList<QXmppOmemoDevice>> Manager::devices(QList<QString> jids)
{
    auto keys = co_await this->keys(jids).withContext(this);

    QList<QXmppOmemoDevice> devices;
    for (const auto &jid : jids) {
        const auto &storedDevices = d->devices.value(jid);
        const auto &storedKeys = keys.value(jid);

        for (const auto &storedDevice : storedDevices) {
            const auto &keyId = storedDevice.keyId;

            QXmppOmemoDevice device;
            device.setJid(jid);
            device.setLabel(storedDevice.label);

            if (!keyId.isEmpty()) {
                device.setKeyId(keyId);
                device.setTrustLevel(storedKeys.value(keyId));
            }

            devices.append(device);
        }
    }

    co_return devices;
}

/*!
    Removes all devices of the contact with JID \a jid and the subscription to
    the contact's device list.

    This should be called after removing a contact.
    The JID of the current user must not be passed.
    Use \c QXmppOmemoManager::resetAll() in order to remove all devices of the
    user.

    Returns the result of the contact device removals.
*/
QXmppTask<QXmppPubSubManager::Result> Manager::removeContactDevices(QString jid)
{
    Q_ASSERT_X(jid != d->ownBareJid(), "Removing contact device", "Own JID passed");

    auto result = co_await d->unsubscribeFromDeviceList(jid).withContext(this);
    if (std::holds_alternative<QXmppError>(result)) {
        warning(u"Contact '" + jid + u"' could not be removed because the device list subscription could not be removed");
        co_return result;
    }

    d->devices.remove(jid);

    auto storageTask = d->omemoStorage->removeDevices(jid);
    auto trustTask = d->trustManager->removeKeys(staticString(ns_omemo_2), jid);
    co_await storageTask.withContext(this);
    co_await trustTask.withContext(this);

    Q_EMIT devicesRemoved(jid);
    co_return result;
}

/*!
    Sets the \a trustLevels keys must have in order to build sessions for their
    devices.
*/
void Manager::setAcceptedSessionBuildingTrustLevels(QXmpp::TrustLevels trustLevels)
{
    d->acceptedSessionBuildingTrustLevels = trustLevels;
}

/*!
    Returns the trust levels keys must have in order to build sessions for their
    devices.
*/
TrustLevels Manager::acceptedSessionBuildingTrustLevels()
{
    return d->acceptedSessionBuildingTrustLevels;
}

/*!
    Sets whether sessions are built when new devices are received from the
    server, controlled by \a isNewDeviceAutoSessionBuildingEnabled.

    This can be used to not call \c QXmppOmemoManager::buildMissingSessions
    manually.
    But it should not be used before the initial setup and storing lots of
    devices locally.
    Otherwise, it could lead to a massive computation and network load when
    there are many devices for whom sessions are built.

    \sa QXmppOmemoManager::buildMissingSessions
*/
void Manager::setNewDeviceAutoSessionBuildingEnabled(bool isNewDeviceAutoSessionBuildingEnabled)
{
    d->isNewDeviceAutoSessionBuildingEnabled = isNewDeviceAutoSessionBuildingEnabled;
}

/*!
    Returns whether sessions are built when new devices are received from the
    server.

    \sa QXmppOmemoManager::setNewDeviceAutoSessionBuildingEnabled
*/
bool Manager::isNewDeviceAutoSessionBuildingEnabled()
{
    return d->isNewDeviceAutoSessionBuildingEnabled;
}

/*!
    Builds sessions manually with devices for whom no sessions are available.

    Usually, sessions are built during sending a first message to a device or after a first message
    is received from a device.
    This can be called in order to speed up the sending of a message.
    If this method is called before sending the first message, all sessions can be built and when
    the first message is being sent, the message only needs to be encrypted.
    Especially for chats with multiple devices, that can decrease the noticeable time a user has to
    wait for sending a message.
    Additionally, the keys are automatically retrieved from the server which is helpful in order to
    get them when calling \c QXmppOmemoManager::devices().

    The user must be logged in while calling this. \a jids contains the JIDs
    of the device owners for whom the sessions are built.
*/
QXmppTask<void> Manager::buildMissingSessions(QList<QString> jids)
{
    auto &devices = d->devices;
    auto devicesCount = 0;

    for (const auto &jid : jids) {
        // Do not exceed the maximum of manageable devices.
        if (devicesCount > d->maximumDevicesPerStanza - devicesCount) {
            warning(u"Sessions could not be built for all JIDs because their devices are "
                    "altogether more than the maximum of manageable devices " +
                    QString::number(d->maximumDevicesPerStanza) +
                    u" - Use QXmppOmemoManager::setMaximumDevicesPerStanza() to increase the maximum");
            break;
        } else {
            devicesCount += devices.value(jid).size();
        }
    }

    if (devicesCount) {
        // Build sessions sequentially to avoid MSVC coroutine ICE
        for (const auto &jid : jids) {
            auto &processedDevices = devices[jid];

            for (auto itr = processedDevices.begin(); itr != processedDevices.end(); ++itr) {
                const auto &deviceId = itr.key();
                auto &device = itr.value();

                if (device.session.isEmpty()) {
                    co_await d->buildSessionWithDeviceBundle(jid, deviceId, device);
                }
            }
        }
    }
}

/*!
    Resets all local OMEMO data for this device and the trust data used by OMEMO.

    ATTENTION: This should only be called after an account is removed from the server since the data
    on the server is not reset.

    Call \c setUp() once this method is finished and you are logged in to a new account if you want
    to set up everything for it.

    \since QXmpp 1.9
*/
QXmppTask<void> QXmppOmemoManager::resetOwnDeviceLocally()
{
    return d->resetOwnDeviceLocally();
}

/*!
    Resets all OMEMO data for this device and the trust data used by OMEMO.

    ATTENTION: This should only be called when an account is removed locally or
    if there are unrecoverable problems with the OMEMO setup of this device.

    The data on the server for other own devices is not removed.
    Call \c resetAll() for that purpose.

    The user must be logged in while calling this.

    Call \c setUp() once this method is finished if you want to set up
    everything again for this device.
    Existing sessions are reset, which might lead to undecryptable incoming
    stanzas until everything is set up again.

    Returns whether the action was successful.
*/
QXmppTask<bool> Manager::resetOwnDevice()
{
    return d->resetOwnDevice();
}

/*!
    Resets all OMEMO data for all own devices and the trust data used by OMEMO.

    ATTENTION: This should only be called if there is a certain reason for it
    since it deletes the data for this device and for other own devices from the
    server.

    Call \c resetOwnDevice() if you only want to delete the OMEMO data for this
    device.

    The user must be logged in while calling this.

    Call \c setUp() once this method is finished if you want to set up
    everything again.
    Existing sessions are reset, which might lead to undecryptable incoming
    stanzas until everything is set up again.

    Returns whether the action was successful.
*/
QXmppTask<bool> Manager::resetAll()
{
    return d->resetAll();
}

/*!
    Sets the security policy used by this E2EE extension to \a securityPolicy.
*/
QXmppTask<void> Manager::setSecurityPolicy(QXmpp::TrustSecurityPolicy securityPolicy)
{
    return d->trustManager->setSecurityPolicy(staticString(ns_omemo_2), securityPolicy);
}

/*!
    Returns the security policy used by this E2EE extension.
*/
QXmppTask<QXmpp::TrustSecurityPolicy> Manager::securityPolicy()
{
    return d->trustManager->securityPolicy(staticString(ns_omemo_2));
}

/*!
    Sets the \a trustLevel of keys identified by \a keyIds, which maps key
    owners' bare JIDs to the IDs of their keys.

    If a key is not stored, it is added to the storage.
*/
QXmppTask<void> Manager::setTrustLevel(const QMultiHash<QString, QByteArray> &keyIds, QXmpp::TrustLevel trustLevel)
{
    return d->trustManager->setTrustLevel(staticString(ns_omemo_2), keyIds, trustLevel);
}

/*!
    Returns the trust level of the key identified by \a keyOwnerJid (the key
    owner's bare JID) and \a keyId (the ID of the key).

    If the key is not stored, the trust in that key is undecided.
*/
QXmppTask<QXmpp::TrustLevel> Manager::trustLevel(const QString &keyOwnerJid, const QByteArray &keyId)
{
    return d->trustManager->trustLevel(staticString(ns_omemo_2), keyOwnerJid, keyId);
}

QXmppTask<QXmppE2eeExtension::MessageEncryptResult> Manager::encryptMessage(QXmppMessage &&message, const std::optional<QXmppSendStanzaParams> &params)
{
    QList<QString> recipientJids;
    std::optional<TrustLevels> acceptedTrustLevels;

    if (params) {
        recipientJids = params->encryptionJids();
        acceptedTrustLevels = params->acceptedTrustLevels();
    }

    if (recipientJids.isEmpty()) {
        recipientJids.append(QXmppUtils::jidToBareJid(message.to()));
    }

    if (!acceptedTrustLevels) {
        acceptedTrustLevels = ACCEPTED_TRUST_LEVELS;
    }

    return d->encryptMessageForRecipients(std::move(message), recipientJids, *acceptedTrustLevels);
}

QXmppTask<QXmppE2eeExtension::MessageDecryptResult> QXmppOmemoManager::decryptMessage(QXmppMessage &&messageRef)
{
    // Move into local to prevent use-after-free across co_await.
    auto message = std::move(messageRef);

    if (!d->initialized) {
        co_return QXmppError {
            u"OMEMO manager must be initialized before decrypting"_s,
            SendError::EncryptionError
        };
    }

    if (!message.omemoElement()) {
        co_return NotEncrypted();
    }

    if (auto decrypted = co_await d->decryptMessage(message)) {
        co_return std::move(*decrypted);
    }
    co_return QXmppError { u"Couldn't decrypt message"_s, {} };
}

QXmppTask<QXmppE2eeExtension::IqEncryptResult> Manager::encryptIq(QXmppIq &&iqRef, const std::optional<QXmppSendStanzaParams> &params)
{
    // Move into local to prevent use-after-free across co_await.
    auto iq = std::move(iqRef);

    if (!d->initialized) {
        co_return QXmppError {
            u"OMEMO manager must be initialized before encrypting"_s,
            SendError::EncryptionError
        };
    }

    std::optional<TrustLevels> acceptedTrustLevels;
    if (params) {
        acceptedTrustLevels = params->acceptedTrustLevels();
    }
    if (!acceptedTrustLevels) {
        acceptedTrustLevels = ACCEPTED_TRUST_LEVELS;
    }

    auto omemoElement = co_await d->encryptStanza(iq, { QXmppUtils::jidToBareJid(iq.to()) }, *acceptedTrustLevels);
    if (!omemoElement) {
        co_return QXmppError {
            u"OMEMO element could not be created"_s,
            SendError::EncryptionError
        };
    }

    auto omemoIq = std::make_unique<QXmppOmemoIq>();
    omemoIq->setId(iq.id());
    omemoIq->setType(iq.type());
    omemoIq->setLang(iq.lang());
    omemoIq->setFrom(iq.from());
    omemoIq->setTo(iq.to());
    omemoIq->setOmemoElement(*omemoElement);

    co_return omemoIq;
}

QXmppTask<QXmppE2eeExtension::IqDecryptResult> Manager::decryptIq(const QDomElement &element)
{
    if (!d->initialized) {
        // TODO: Add decryption queue to avoid this error
        co_return QXmppError {
            u"OMEMO manager must be initialized before decrypting"_s,
            SendError::EncryptionError
        };
    }

    if (!QXmppOmemoIq::isOmemoIq(element)) {
        co_return NotEncrypted();
    }

    // Tag name and iq type are already checked in QXmppClient.
    if (auto result = co_await d->decryptIq(element)) {
        co_return result->iq;
    }
    co_return QXmppError {
        u"OMEMO message could not be decrypted"_s,
        SendError::EncryptionError
    };
}

bool QXmppOmemoManager::isEncrypted(const QDomElement &el)
{
    return !firstChildElement(el, u"encrypted", ns_omemo_2).isNull();
}

bool QXmppOmemoManager::isEncrypted(const QXmppMessage &message)
{
    return message.omemoElement().has_value();
}

QStringList Manager::discoveryFeatures() const
{
    return {
        ns_omemo_2_devices + u"+notify"
    };
}

bool Manager::handleStanza(const QDomElement &stanza)
{
    if (stanza.tagName() != u"iq" || !QXmppOmemoIq::isOmemoIq(stanza)) {
        return false;
    }

    // TODO: Queue incoming IQs until OMEMO is initialized
    if (!d->initialized) {
        warning(u"Couldn't decrypt incoming IQ because the manager isn't initialized yet."_s);
        return false;
    }

    auto type = stanza.attribute(u"type"_s);
    if (type != u"get" && type != u"set") {
        // ignore incoming result and error IQs (they are handled via Client::sendIq())
        return false;
    }

    d->decryptIq(stanza).then(this, [=, this](auto result) {
        if (result) {
            injectIq(result->iq, result->e2eeMetadata);
        } else {
            warning(u"Could not decrypt incoming OMEMO IQ."_s);
        }
    });
    return true;
}

bool Manager::handleMessage(const QXmppMessage &message)
{
    if (d->initialized && message.omemoElement()) {
        d->decryptMessage(message).then(this, [this, message](std::optional<QXmppMessage> optionalDecryptedMessage) {
            if (optionalDecryptedMessage) {
                injectMessage(std::move(*optionalDecryptedMessage));
            } else {
                Q_EMIT client()->messageReceived(message);
            }
        });

        return true;
    }

    return false;
}

/*!
    \fn QXmppOmemoManager::trustLevelsChanged(const QMultiHash<QString, QByteArray> &modifiedKeys)

    Emitted when the trust levels of keys changed. \a modifiedKeys maps key
    owners' bare JIDs to their modified keys.
*/

/*!
    \fn QXmppOmemoManager::deviceAdded(const QString &jid, uint32_t deviceId)

    Emitted when a device is added. \a jid is the device owner's bare JID and
    \a deviceId is the ID of the device.
*/

/*!
    \fn QXmppOmemoManager::deviceChanged(const QString &jid, uint32_t deviceId)

    Emitted when a device changed. \a jid is the device owner's bare JID and
    \a deviceId is the ID of the device.
*/

/*!
    \fn QXmppOmemoManager::deviceRemoved(const QString &jid, uint32_t deviceId)

    Emitted when a device is removed. \a jid is the device owner's bare JID
    and \a deviceId is the ID of the device.
*/

/*!
    \fn QXmppOmemoManager::devicesRemoved(const QString &jid)

    Emitted when all devices of the owner with bare JID \a jid are removed.
*/

/*!
    \fn QXmppOmemoManager::allDevicesRemoved()

    Emitted when all devices are removed.
*/

void Manager::onRegistered(QXmppClient *client)
{
    d->trustManager = client->findExtension<QXmppTrustManager>();
    if (!d->trustManager) {
        qFatal("QXmppTrustManager is not available, it must be added to the client before adding QXmppOmemoManager");
    }

    d->pubSubManager = client->findExtension<QXmppPubSubManager>();
    if (!d->pubSubManager) {
        qFatal("QXmppPubSubManager is not available, it must be added to the client before adding QXmppOmemoManager");
    }

    connect(d->trustManager, &QXmppTrustManager::trustLevelsChanged, this, [=, this](const QHash<QString, QMultiHash<QString, QByteArray>> &modifiedKeys) {
        const auto &modifiedOmemoKeys = modifiedKeys.value(staticString(ns_omemo_2));

        if (!modifiedOmemoKeys.isEmpty()) {
            Q_EMIT trustLevelsChanged(modifiedOmemoKeys);
        }

        QMultiHash<QString, uint32_t> modifiedDevices;

        for (auto itr = modifiedOmemoKeys.cbegin(); itr != modifiedOmemoKeys.cend(); ++itr) {
            const auto &keyOwnerJid = itr.key();
            const auto &keyId = itr.value();

            // Ensure to emit 'deviceChanged()' later only if there is a device with the key.
            const auto &devices = d->devices.value(keyOwnerJid);
            for (auto devicesItr = devices.cbegin(); devicesItr != devices.cend(); ++devicesItr) {
                if (devicesItr->keyId == keyId) {
                    modifiedDevices.insert(keyOwnerJid, devicesItr.key());
                    break;
                }
            }
        }

        for (auto modifiedDevicesItr = modifiedDevices.cbegin(); modifiedDevicesItr != modifiedDevices.cend(); ++modifiedDevicesItr) {
            Q_EMIT deviceChanged(modifiedDevicesItr.key(), modifiedDevicesItr.value());
        }
    });
}

void Manager::onUnregistered(QXmppClient *client)
{
    // TODO: Proper clean up of connections (currently no issue because extensions are deleted
    // on removal)
}

bool Manager::handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName)
{
    if (nodeName == ns_omemo_2_devices && QXmppPubSubEvent<QXmppOmemoDeviceListItem>::isPubSubEvent(element)) {
        QXmppPubSubEvent<QXmppOmemoDeviceListItem> event;
        event.parse(element);

        switch (event.eventType()) {
        // Items have been published.
        case QXmppPubSubEventBase::Items: {
            // Only process items if the event notification contains one.
            // That is necessary because PubSub allows publishing without items leading to
            // notification-only events.
            if (const auto &items = event.items(); !items.isEmpty()) {
                // Since the usage of the item ID \c QXmppPubSubManager::Current is only RECOMMENDED
                // by \xep{0060}{Publish-Subscribe} (PubSub) but not obligatory, an appropriate
                // contact device list is determined.
                // In case of the own device list node, it is sctrictly processed as a recommended
                // singleton item and changed to fit that if needed.
                const auto isOwnDeviceListNode = d->ownBareJid() == pubSubService;
                if (isOwnDeviceListNode) {
                    const auto &deviceListItem = items.constFirst();
                    if (deviceListItem.id() == QXmppPubSubManager::standardItemIdToString(QXmppPubSubManager::Current)) {
                        d->updateDevices(pubSubService, event.items().constFirst());
                    } else {
                        d->handleIrregularDeviceListChanges(pubSubService);
                    }
                } else {
                    d->updateContactDevices(pubSubService, items);
                }
            }

            break;
        }
        // Specific items are deleted.
        case QXmppPubSubEventBase::Retract: {
            d->handleIrregularDeviceListChanges(pubSubService);
        }
        // All items are deleted.
        case QXmppPubSubEventBase::Purge:
        // The whole node is deleted.
        case QXmppPubSubEventBase::Delete:
            d->handleIrregularDeviceListChanges(pubSubService);
            break;
        case QXmppPubSubEventBase::Configuration:
        case QXmppPubSubEventBase::Subscription:
            break;
        }

        return true;
    }

    return false;
}

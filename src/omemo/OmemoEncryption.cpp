// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppJid_p.h"
#include "QXmppOmemoElement_p.h"
#include "QXmppOmemoEnvelope_p.h"
#include "QXmppSceEnvelope_p.h"
#include "QXmppTrustManager.h"
#include "QXmppUtils.h"
#include "QXmppXmlRegistry.h"

#include "Async.h"
#include "Omemo0Data.h"
#include "OmemoManager_p.h"
#include "OmemoPayloadCipher.h"
#include "StringLiterals.h"

#include <QDomDocument>
#include <QRandomGenerator>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;
using namespace QXmpp::Omemo::Private;

namespace QXmpp::Omemo::Private {

// OMEMO 2 empty messages encrypt this number of zero bytes
constexpr qsizetype EMPTY_MESSAGE_KEY_SIZE = 32;
constexpr uint32_t SCE_RPAD_SIZE_MAX = 200;

QString OmemoManagerPrivate::trustNamespace(Version version)
{
    return version == Version::Omemo0 ? staticString(ns_omemo) : staticString(ns_omemo_2);
}

// A coroutine taking all arguments by value: the providers are copied and may be destroyed while it
// is suspended.
template<typename Result, typename Request>
static QXmppTask<Result> afterLoading(OmemoManagerPrivate *d, QXmppTask<Result> (OmemoManagerPrivate::*function)(Request), Request request)
{
    co_await d->loaded().withContext(d->q);
    // called before co_await, GCC 15 crashes otherwise
    auto task = (d->*function)(std::move(request));
    co_return co_await task.withContext(d->q);
}

void OmemoManagerPrivate::registerProviders()
{
    // The OMEMO 0 element is part of this library, so it is registered here.
    [[maybe_unused]]
    static const bool registered = [] {
        Xml::Registry::registerEncryptedContainer<Omemo0::Element>(Xml::Scope::Message);
        return true;
    }();

    // The stanzas are processed after the data has been loaded.
    // Messages with elements of both versions are decrypted with OMEMO 2.
    E2ee::Providers(q->hooks()).add(E2ee::Provider {
        .encryption = E2ee::Omemo2,
        .decryptMessage = [this](E2ee::DecryptRequest request) { return afterLoading(this, &OmemoManagerPrivate::decryptMessage2, std::move(request)); },
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &OmemoManagerPrivate::encryptMessage2, std::move(request)); },
        .encryptIq = [this](E2ee::EncryptIqRequest request) { return afterLoading(this, &OmemoManagerPrivate::encryptIq2, std::move(request)); },
        .decryptIq = [this](E2ee::DecryptIqRequest request) { return afterLoading(this, &OmemoManagerPrivate::decryptIq2, std::move(request)); },
        .priority = 1,
        .isEnabled = [this] { return isEnabled(Version::Omemo2); },
    });
    // OMEMO 0 encrypts only the body and no IQs.
    E2ee::Providers(q->hooks()).add(E2ee::Provider {
        .encryption = E2ee::Omemo0,
        .contentEncryption = E2ee::ContentEncryption::BodyOnly,
        .decryptMessage = [this](E2ee::DecryptRequest request) { return afterLoading(this, &OmemoManagerPrivate::decryptMessage0, std::move(request)); },
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &OmemoManagerPrivate::encryptMessage0, std::move(request)); },
        .isEnabled = [this] { return isEnabled(Version::Omemo0); },
    });
    // The automatic version only sends elements of the other providers.
    E2ee::Providers(q->hooks()).add(E2ee::Provider {
        .encryption = Automatic,
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &OmemoManagerPrivate::encryptMessageAutomatic, std::move(request)); },
        .encryptIq = [this](E2ee::EncryptIqRequest request) { return afterLoading(this, &OmemoManagerPrivate::encryptIq2, std::move(request)); },
    });
}

OmemoStorage::Device *OmemoManagerPrivate::storedDevice(Version version, const QString &jid, uint32_t deviceId)
{
    auto &devices = versions.at(version).devices;
    auto jidIt = devices.find(jid);
    if (jidIt == devices.end()) {
        return nullptr;
    }
    auto it = jidIt->find(deviceId);
    return it == jidIt->end() ? nullptr : &*it;
}

static Version otherVersion(Version version)
{
    return version == Version::Omemo0 ? Version::Omemo2 : Version::Omemo0;
}

// The trust level of a key. A new key gets the trust level of the same identity key in the other
// version, otherwise the one of the security policy.
QXmppTask<TrustLevel> OmemoManagerPrivate::trustLevel(Version version, QString jid, QByteArray key)
{
    const auto encryption = trustNamespace(version);
    auto level = co_await trustManager->trustLevel(encryption, jid, key).withContext(q);
    if (level != TrustLevel::Undecided) {
        co_return level;
    }

    const auto otherEncryption = trustNamespace(otherVersion(version));
    for (const auto &sameKey : sameIdentityKeys(version, jid, key)) {
        level = co_await trustManager->trustLevel(otherEncryption, jid, sameKey).withContext(q);
        if (level != TrustLevel::Undecided) {
            co_await trustManager->addKeys(encryption, jid, { key }, level).withContext(q);
            co_return level;
        }
    }

    // Trust on first use until a key of the JID has been authenticated in any version (TOAKAFA).
    level = TrustLevel::AutomaticallyDistrusted;
    if (co_await trustManager->securityPolicy(encryption).withContext(q) == Toakafa &&
        !co_await trustManager->hasKey(encryption, jid, TrustLevel::Authenticated).withContext(q) &&
        !co_await trustManager->hasKey(otherEncryption, jid, TrustLevel::Authenticated).withContext(q)) {
        level = TrustLevel::AutomaticallyTrusted;
    }
    co_await trustManager->addKeys(encryption, jid, { key }, level).withContext(q);
    co_return level;
}

// The known keys of the JID's devices in the other version with the same identity key. The device
// IDs are not compared, they are not authenticated.
QList<QByteArray> OmemoManagerPrivate::sameIdentityKeys(Version version, const QString &jid, const QByteArray &key) const
{
    const auto curveKey = version == Version::Omemo2 ? omemo0IdentityKey(key) : key;
    if (curveKey.isEmpty()) {
        return {};
    }
    const auto other = otherVersion(version);
    QList<QByteArray> keys;
    for (const auto &device : versions.at(other).devices.value(jid)) {
        if (!device.keyId.isEmpty() && !keys.contains(device.keyId) &&
            (other == Version::Omemo0 ? device.keyId : omemo0IdentityKey(device.keyId)) == curveKey) {
            keys.append(device.keyId);
        }
    }
    return keys;
}

// Whether the device is the same as one using OMEMO 2: by the identity key if the one of the
// OMEMO 0 device is known, otherwise by the ID.
static bool isSameDevice(const OmemoStorage::Device &omemo2Device, const OmemoStorage::Device &omemo0Device)
{
    if (omemo2Device.keyId.isEmpty()) {
        return false;
    }
    return omemo0Device.keyId.isEmpty() ? omemo2Device.id == omemo0Device.id : omemo0IdentityKey(omemo2Device.keyId) == omemo0Device.keyId;
}

// Whether the OMEMO 0 device of the JID uses OMEMO 2. It is encrypted for with OMEMO 2 only then.
bool OmemoManagerPrivate::usesOmemo2(const QString &jid, uint32_t omemo0DeviceId) const
{
    const auto omemo0Devices = versions.at(Version::Omemo0).devices.value(jid);
    const auto omemo0Device = omemo0Devices.value(omemo0DeviceId, OmemoStorage::Device { Version::Omemo0, jid, omemo0DeviceId });
    const auto omemo2Devices = versions.at(Version::Omemo2).devices.value(jid);
    return std::ranges::any_of(omemo2Devices, [&](const OmemoStorage::Device &device) { return isSameDevice(device, omemo0Device); });
}

// Whether the device has been removed from the OMEMO 2 device list, but is still listed in the
// OMEMO 0 one. It stays encrypted for with OMEMO 2, so a server can not make the clients fall back
// to OMEMO 0 by hiding the OMEMO 2 device list.
bool OmemoManagerPrivate::isSticky(const OmemoStorage::Device &omemo2Device) const
{
    if (!omemo2Device.removalFromDeviceListDate.isValid()) {
        return false;
    }
    const auto omemo0Devices = versions.at(Version::Omemo0).devices.value(omemo2Device.jid);
    return std::ranges::any_of(omemo0Devices, [&](const OmemoStorage::Device &device) {
        return !device.removalFromDeviceListDate.isValid() && isSameDevice(omemo2Device, device);
    });
}

using SkipReason = EncryptionInfo::SkipReason;

// Builds a session with the device if there is none. Returns its trust level and why it can not be
// encrypted for, if so.
QXmppTask<PreparedSession> OmemoManagerPrivate::prepareSession(Version version, QString jid, uint32_t deviceId, TrustLevels acceptedTrustLevels)
{
    const Address address { jid, deviceId };
    auto *device = storedDevice(version, jid, deviceId);
    if (!device) {
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::NoSession };
    }
    if (device->unrespondedSentStanzasCount >= UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_STOPS) {
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::Unresponsive };
    }

    auto checkTrust = [&](TrustLevel trust) {
        return PreparedSession { trust, acceptedTrustLevels.testFlag(trust) ? std::nullopt : std::optional(SkipReason::Untrusted) };
    };
    if (versions.at(version).sessions->hasSession(address) && !device->keyId.isEmpty()) {
        co_return checkTrust(co_await trustLevel(version, jid, device->keyId).withContext(q));
    }

    auto result = co_await pubSubClient(version).requestBundle(jid, deviceId).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        q->warning(u"Could not fetch the bundle of %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::NoSession };
    }
    const auto &bundle = std::get<Pep::PublicBundle>(result);
    // Checked before the key is stored, so the device has no key without a usable bundle.
    if (!versions.at(version).sessions->isSignedPreKeyValid(bundle.identityKey, bundle.signedPreKey, bundle.signedPreKeySignature)) {
        q->warning(u"The bundle of %1/%2 has an invalid signature"_s.arg(jid, QString::number(deviceId)));
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::NoSession };
    }

    // The key of a device does not change. A changed key could be used to take over a device.
    device = storedDevice(version, jid, deviceId);
    if (!device) {
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::NoSession };
    }
    if (!device->keyId.isEmpty() && device->keyId != bundle.identityKey) {
        q->warning(u"The bundle of %1/%2 has another identity key than before"_s.arg(jid, QString::number(deviceId)));
        co_return PreparedSession { TrustLevel::Undecided, SkipReason::NoSession };
    }
    const auto trust = co_await trustLevel(version, jid, bundle.identityKey).withContext(q);
    device = storedDevice(version, jid, deviceId);
    if (!device) {
        co_return PreparedSession { trust, SkipReason::NoSession };
    }
    // also stored if the device is not trusted, so the key can be authenticated
    if (device->keyId.isEmpty()) {
        device->keyId = bundle.identityKey;
        storage->addDevice(*device);
        updateWatch(jid);
    }
    if (!acceptedTrustLevels.testFlag(trust)) {
        co_return PreparedSession { trust, SkipReason::Untrusted };
    }

    auto &sessions = *versions.at(version).sessions;
    if (sessions.hasSession(address)) {
        co_return PreparedSession { trust };
    }
    if (bundle.preKeys.isEmpty()) {
        q->warning(u"The bundle of %1/%2 contains no pre keys"_s.arg(jid, QString::number(deviceId)));
        co_return PreparedSession { trust, SkipReason::NoSession };
    }
    const auto preKeyIds = bundle.preKeys.keys();
    const auto preKeyId = preKeyIds.at(QRandomGenerator::system()->bounded(preKeyIds.size()));
    auto built = sessions.buildSession(address,
                                       { bundle.identityKey,
                                         bundle.signedPreKeyId,
                                         bundle.signedPreKey,
                                         bundle.signedPreKeySignature,
                                         preKeyId,
                                         bundle.preKeys.value(preKeyId) });
    if (auto *error = std::get_if<QXmppError>(&built)) {
        q->warning(u"Could not build a session with %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
        co_return PreparedSession { trust, SkipReason::NoSession };
    }
    co_return PreparedSession { trust };
}

// Selects the listed devices of the JIDs and the other own devices with an accepted trust level,
// but at most maxDevicesPerStanza devices, the own ones first. With both versions (OMEMO 2 first,
// so its devices are known when selecting the OMEMO 0 ones), each device gets the best version it
// supports: devices using OMEMO 2 only get OMEMO 2, also if they have been removed from its device
// list (see isSticky()).
QXmppTask<Result<DeviceSelection>> OmemoManagerPrivate::selectDevices(QList<Version> usedVersions, QList<Jid> recipientJids, TrustLevels acceptedTrustLevels)
{
    const auto own = ownJid();
    auto jids = transform<QList<QString>>(recipientJids, [](const Jid &jid) { return jid.toString(); });
    const auto hasRecipients = std::ranges::any_of(jids, [&](const QString &jid) { return jid != own; });
    jids.removeAll(own);
    jids.prepend(own);
    const auto perDevice = usedVersions.contains(Version::Omemo0) && usedVersions.contains(Version::Omemo2);

    DeviceSelection selection;
    auto &recipients = selection.recipients;
    auto skip = [&](Version version, const QString &jid, uint32_t deviceId, TrustLevel trustLevel, SkipReason reason) {
        selection.skippedDevices.append({ publicDevice(version, jid, deviceId, trustLevel), reason });
    };
    for (const auto &jid : std::as_const(jids)) {
        auto candidates = [&](Version version) {
            auto ids = deviceIds(version, jid);
            if (perDevice && version == Version::Omemo2) {
                for (const auto &device : versions.at(version).devices.value(jid)) {
                    if (isSticky(device)) {
                        ids.append(device.id);
                    }
                }
            }
            if (perDevice && version == Version::Omemo0) {
                ids.removeIf([&](uint32_t id) { return usesOmemo2(jid, id); });
            }
            return ids;
        };

        if (recipients.size() >= maxDevicesPerStanza) {
            for (auto version : std::as_const(usedVersions)) {
                for (auto deviceId : candidates(version)) {
                    skip(version, jid, deviceId, TrustLevel::Undecided, SkipReason::DeviceLimit);
                }
            }
            continue;
        }

        for (auto version : std::as_const(usedVersions)) {
            // Device lists of contacts are subscribed, but they are unknown before the first
            // message or if only unlisted devices are known from their messages. Device lists
            // without devices are requested once per login, e.g. the OMEMO 2 one of contacts using
            // only OMEMO 0.
            if (jid != own && deviceIds(version, jid).isEmpty() && !versions.at(version).receivedDeviceLists.contains(jid)) {
                auto result = co_await pubSubClient(version).requestDeviceList(jid).withContext(q);
                if (auto *list = std::get_if<QList<DeviceList::Entry>>(&result)) {
                    applyReceivedDeviceList(version, jid, *list);
                }
            }
        }

        for (auto version : std::as_const(usedVersions)) {
            for (auto deviceId : candidates(version)) {
                if (recipients.size() >= maxDevicesPerStanza) {
                    skip(version, jid, deviceId, TrustLevel::Undecided, SkipReason::DeviceLimit);
                    continue;
                }
                const auto prepared = co_await prepareSession(version, jid, deviceId, acceptedTrustLevels).withContext(q);
                if (prepared.skipReason) {
                    skip(version, jid, deviceId, prepared.trustLevel, *prepared.skipReason);
                } else {
                    recipients.append({ version, jid, deviceId, prepared.trustLevel });
                }
            }
        }
    }

    if (const auto skippedCount = std::ranges::count(selection.skippedDevices, SkipReason::DeviceLimit, &EncryptionInfo::SkippedDevice::reason)) {
        q->warning(u"Not encrypted for %1 devices above the maximum of %2 devices per stanza"_s.arg(QString::number(skippedCount), QString::number(maxDevicesPerStanza)));
    }
    if (hasRecipients && std::ranges::all_of(recipients, [&](const Recipient &recipient) { return recipient.jid == own; })) {
        co_return QXmppError { u"There is no device of the recipients with an accepted trust level"_s, SendError::NoEncryptionRecipients };
    }
    co_return selection;
}

Device OmemoManagerPrivate::publicDevice(Version version, const QString &jid, uint32_t deviceId, TrustLevel trustLevel)
{
    Device device { .version = version, .jid = jidOrNull(jid), .id = deviceId, .trustLevel = trustLevel };
    if (const auto *stored = storedDevice(version, jid, deviceId)) {
        device.label = stored->label;
        device.keyId = stored->keyId;
    }
    return device;
}

// Encrypts the key material for the recipients using the version.
QList<Envelope> OmemoManagerPrivate::encryptKey(Version version, const QList<Recipient> &recipients, const SecureByteArray &keyMaterial, EncryptionInfo &info)
{
    QList<Envelope> envelopes;
    for (const auto &[recipientVersion, jid, deviceId, trustLevel] : recipients) {
        if (recipientVersion != version) {
            continue;
        }
        auto encrypted = versions.at(version).sessions->encrypt({ jid, deviceId }, keyMaterial);
        if (auto *error = std::get_if<QXmppError>(&encrypted)) {
            q->warning(u"Could not encrypt for %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
            info.skippedDevices.append({ publicDevice(version, jid, deviceId, trustLevel), SkipReason::NoSession });
            continue;
        }
        const auto &key = std::get<EncryptedKey>(encrypted);
        envelopes.append({ jid, deviceId, key.data, key.isKeyExchange });
        info.devices.append(publicDevice(version, jid, deviceId, trustLevel));

        if (auto *device = storedDevice(version, jid, deviceId)) {
            device->unrespondedSentStanzasCount = std::min(device->unrespondedSentStanzasCount + 1, UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_STOPS);
            device->unrespondedReceivedStanzasCount = 0;
            storage->addDevice(*device);
        }
    }
    return envelopes;
}

QXmppTask<Result<DecryptedEnvelope>> OmemoManagerPrivate::decryptKey(Version version, QString jid, uint32_t deviceId, QByteArray data, bool isKeyExchange, bool isMessage)
{
    const Address address { jid, deviceId };
    auto &state = versions.at(version);
    auto &sessions = *state.sessions;
    // Each unknown device is stored, so their number is limited.
    if (!storedDevice(version, jid, deviceId) && unlistedDeviceCount(version, jid) >= MAX_UNLISTED_DEVICES_PER_JID) {
        co_return QXmppError { u"Too many devices of %1 are not in its device list"_s.arg(jid), {} };
    }
    if (!isKeyExchange && !sessions.hasSession(address)) {
        // The other device sends key exchanges until it receives a message from this device.
        if (!isKnownJid(jid)) {
            co_return QXmppError { u"There is no session with %1/%2"_s.arg(jid, QString::number(deviceId)), {} };
        }
        buildSessionAndSendEmptyMessage(version, jid, deviceId);
        co_return QXmppError { u"There is no session with %1/%2, a new one is built"_s.arg(jid, QString::number(deviceId)), {} };
    }

    // The key of a device does not change, like in prepareSession().
    const auto *knownDevice = storedDevice(version, jid, deviceId);
    auto result = sessions.decrypt(address, data, isKeyExchange, knownDevice ? knownDevice->keyId : QByteArray());
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    auto &decrypted = std::get<DecryptedKey>(result);

    // the session callback has created the device if it was unknown
    auto *device = storedDevice(version, jid, deviceId);
    Q_ASSERT(device);
    // Strangers get no automatic stanzas until the user interacts with them, e.g. by watching
    // their devices or by sending a message.
    const auto isKnown = isKnownJid(jid);
    // The device list may be outdated. It is requested once per login, later changes are notified.
    if (isKnown && device->removalFromDeviceListDate.isValid() && !state.refreshedDeviceLists.contains(jid)) {
        state.refreshedDeviceLists.insert(jid);
        refreshDeviceList(version, jid);
    }
    if (isKeyExchange) {
        if (device->keyId.isEmpty()) {
            device->keyId = decrypted.senderIdentityKey;
            updateWatch(jid);
        }
        if (decrypted.usedPreKeyId) {
            refillPreKeysAndPublish(version);
        }
    }

    device->unrespondedSentStanzasCount = 0;
    auto sendEmpty = isKeyExchange && isMessage;
    if (++device->unrespondedReceivedStanzasCount >= UNRESPONDED_STANZAS_UNTIL_HEARTBEAT) {
        sendEmpty = true;
    }
    sendEmpty = sendEmpty && isKnown;
    if (sendEmpty) {
        device->unrespondedReceivedStanzasCount = 0;
    }
    storage->addDevice(*device);
    const auto senderKey = device->keyId;

    if (sendEmpty) {
        sendEmptyMessage(version, jid, deviceId);
    }
    if (isKeyExchange) {
        // stores the key with the trust level of the security policy if it is new
        co_await trustLevel(version, jid, senderKey).withContext(q);
    }
    co_return DecryptedEnvelope { std::move(decrypted.data), senderKey };
}

void OmemoManagerPrivate::refreshDeviceList(Version version, const QString &jid)
{
    pubSubClient(version).requestDeviceList(jid).then(q, [this, version, jid](Result<QList<DeviceList::Entry>> &&result) {
        if (auto *list = std::get_if<QList<DeviceList::Entry>>(&result)) {
            applyReceivedDeviceList(version, jid, *list);
        }
    });
}

// Sends a message without payload, e.g. to complete a key exchange or as heartbeat.
void OmemoManagerPrivate::sendEmptyMessage(Version version, const QString &jid, uint32_t deviceId)
{
    // OMEMO 0 sends a key transport element: key and IV of an empty payload.
    SecureByteArray keyMaterial;
    QByteArray iv;
    if (version == Version::Omemo2) {
        keyMaterial = SecureByteArray(QByteArray(EMPTY_MESSAGE_KEY_SIZE, '\0'));
    } else {
        auto encrypted = PayloadCipher::encrypt(version, {});
        if (auto *error = std::get_if<QXmppError>(&encrypted)) {
            q->warning(u"Could not create an empty message for %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
            return;
        }
        keyMaterial = std::move(std::get<PayloadCipher::Encrypted>(encrypted).keyMaterial);
        iv = std::get<PayloadCipher::Encrypted>(encrypted).iv;
    }

    auto key = versions.at(version).sessions->encrypt({ jid, deviceId }, keyMaterial);
    if (auto *error = std::get_if<QXmppError>(&key)) {
        q->warning(u"Could not create an empty message for %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
        return;
    }
    const auto &envelope = std::get<EncryptedKey>(key);

    QXmppMessage message;
    message.setTo(jid);
    message.setType(QXmppMessage::Chat);
    message.addHint(QXmppMessage::Store);
    if (version == Version::Omemo2) {
        QXmppOmemoEnvelope omemoEnvelope;
        omemoEnvelope.setRecipientDeviceId(deviceId);
        omemoEnvelope.setUsedForKeyExchange(envelope.isKeyExchange);
        omemoEnvelope.setData(envelope.data);
        QXmppOmemoElement element;
        element.setSenderDeviceId(device.deviceId);
        element.addEnvelope(jid, omemoEnvelope);
        message.extensions().add(std::move(element));
    } else {
        Omemo0::Element element;
        element.senderDeviceId = device.deviceId;
        element.iv = iv;
        element.envelopes.append({ deviceId, envelope.isKeyExchange, envelope.data });
        message.extensions().add(std::move(element));
    }
    q->client()->send(std::move(message));
}

void OmemoManagerPrivate::buildSessionAndSendEmptyMessage(Version version, const QString &jid, uint32_t deviceId)
{
    // Messages arriving before the session is built do not start another one.
    const Address address { jid, deviceId };
    if (versions.at(version).pendingSessionBuilds.contains(address)) {
        return;
    }

    // Only listed devices are encrypted for. The device is added as unlisted if it is unknown.
    if (!storedDevice(version, jid, deviceId)) {
        auto &device = versions.at(version).devices[jid][deviceId];
        device = OmemoStorage::Device { version, jid, deviceId };
        device.removalFromDeviceListDate = now();
        storage->addDevice(device);
    }
    versions.at(version).pendingSessionBuilds.insert(address);
    prepareSession(version, jid, deviceId, DEFAULT_ACCEPTED_TRUST_LEVELS | TrustLevel::AutomaticallyDistrusted | TrustLevel::ManuallyDistrusted)
        .then(q, [this, version, jid, deviceId, address](PreparedSession &&prepared) {
            versions.at(version).pendingSessionBuilds.remove(address);
            if (!prepared.skipReason) {
                sendEmptyMessage(version, jid, deviceId);
            }
        });
}

// Replaces used pre keys. Bundle publications are not run in parallel.
void OmemoManagerPrivate::refillPreKeysAndPublish(Version version)
{
    auto &state = versions.at(version);
    auto added = KeyMaintenance::refillPreKeys(context, state.keys, keyConfig);
    if (auto *error = std::get_if<QXmppError>(&added)) {
        q->warning(u"Could not create pre keys: "_s + error->description);
        return;
    }
    QList<OmemoStorage::PreKey> records;
    for (auto id : std::get<QList<uint32_t>>(added)) {
        records.append({ version, id, state.keys.preKeys.value(id).toByteArray() });
    }
    storage->addPreKeys(records);
    storage->setOwnDevice(storedOwnDevice());

    if (state.isPublishingBundle) {
        state.isBundleOutdated = true;
        return;
    }
    state.isPublishingBundle = true;
    publishBundle(version).then(q, [this, version](Result<> &&result) {
        auto &state = versions.at(version);
        state.isPublishingBundle = false;
        if (auto *error = std::get_if<QXmppError>(&result)) {
            q->warning(u"Could not publish the bundle: "_s + error->description);
        }
        if (std::exchange(state.isBundleOutdated, false)) {
            refillPreKeysAndPublish(version);
        }
    });
}

static QByteArray createSceEnvelope(const QString &from, const QString &to, auto writeContent)
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QXmppSceEnvelopeWriter envelope(writer);
    envelope.start();
    envelope.writeTimestamp(QDateTime::currentDateTimeUtc());
    envelope.writeTo(to);
    envelope.writeFrom(from);
    const auto rpadSize = QRandomGenerator::system()->bounded(SCE_RPAD_SIZE_MAX);
    envelope.writeRpad(QString::fromUtf8(QXmppUtils::generateRandomBytes(rpadSize).toBase64()));
    envelope.writeContent([&] { writeContent(writer); });
    envelope.end();
    return xml;
}

static QDomElement parseSceEnvelope(const QByteArray &xml)
{
    QDomDocument document;
    if (!document.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing)) {
        return {};
    }
    auto envelope = document.documentElement();
    return envelope.tagName() == u"envelope" && envelope.namespaceURI() == ns_sce ? envelope : QDomElement();
}

static Result<QXmppOmemoElement> encryptOmemo2(OmemoManagerPrivate *d, const QByteArray &sceEnvelope, const QList<Recipient> &recipients, EncryptionInfo &info)
{
    auto payload = PayloadCipher::encrypt(Version::Omemo2, sceEnvelope);
    if (auto *error = std::get_if<QXmppError>(&payload)) {
        return std::move(*error);
    }
    const auto &encrypted = std::get<PayloadCipher::Encrypted>(payload);

    QXmppOmemoElement element;
    element.setSenderDeviceId(d->device.deviceId);
    element.setPayload(encrypted.ciphertext);
    for (const auto &envelope : d->encryptKey(Version::Omemo2, recipients, encrypted.keyMaterial, info)) {
        QXmppOmemoEnvelope omemoEnvelope;
        omemoEnvelope.setRecipientDeviceId(envelope.deviceId);
        omemoEnvelope.setUsedForKeyExchange(envelope.isKeyExchange);
        omemoEnvelope.setData(envelope.data);
        element.addEnvelope(envelope.jid, omemoEnvelope);
    }
    return element;
}

static Result<Omemo0::Element> encryptOmemo0(OmemoManagerPrivate *d, const QString &body, const QList<Recipient> &recipients, EncryptionInfo &info)
{
    auto payload = PayloadCipher::encrypt(Version::Omemo0, body.toUtf8());
    if (auto *error = std::get_if<QXmppError>(&payload)) {
        return std::move(*error);
    }
    const auto &encrypted = std::get<PayloadCipher::Encrypted>(payload);

    Omemo0::Element element;
    element.senderDeviceId = d->device.deviceId;
    element.iv = encrypted.iv;
    element.payload = encrypted.ciphertext;
    for (const auto &envelope : d->encryptKey(Version::Omemo0, recipients, encrypted.keyMaterial, info)) {
        element.envelopes.append({ envelope.deviceId, envelope.isKeyExchange, envelope.data });
    }
    return element;
}

static QXmppTask<E2ee::EncryptResult> encryptSce(OmemoManagerPrivate *d, QByteArray sceEnvelope, QList<Jid> recipientJids, TrustLevels acceptedTrustLevels)
{
    if (!d->isSetUp || !d->isEnabled(Version::Omemo2)) {
        co_return QXmppError { u"OMEMO 2 is not set up"_s, SendError::EncryptionError };
    }
    auto selected = co_await d->selectDevices({ Version::Omemo2 }, recipientJids, acceptedTrustLevels).withContext(d->q);
    if (auto *error = std::get_if<QXmppError>(&selected)) {
        co_return std::move(*error);
    }
    auto &selection = std::get<DeviceSelection>(selected);
    EncryptionInfo info { .skippedDevices = std::move(selection.skippedDevices) };
    auto element = encryptOmemo2(d, sceEnvelope, selection.recipients, info);
    if (auto *error = std::get_if<QXmppError>(&element)) {
        co_return std::move(*error);
    }

    Xml::Extensions elements;
    elements.add(std::get<QXmppOmemoElement>(std::move(element)));
    E2ee::Encrypted result;
    result.setElements(std::move(elements));
    result.setInfo(std::move(info));
    co_return result;
}

static QByteArray messageSceEnvelope(const QString &from, const QXmppMessage &message)
{
    return createSceEnvelope(from, jidBare(message.to()), [&](QXmlStreamWriter &writer) {
        message.serializeExtensions(&writer, SceSensitive, staticString(ns_client));
    });
}

QXmppTask<E2ee::EncryptResult> OmemoManagerPrivate::encryptMessage2(E2ee::EncryptRequest request)
{
    return encryptSce(this, messageSceEnvelope(ownJid(), request.message()), request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS));
}

QXmppTask<E2ee::EncryptResult> OmemoManagerPrivate::encryptIq2(E2ee::EncryptIqRequest request)
{
    const auto sce = createSceEnvelope(ownJid(), request.to().bare().toString(), [&](QXmlStreamWriter &writer) {
        request.writeContent(&writer);
    });
    return encryptSce(this, sce, request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS));
}

static QXmppTask<E2ee::DecryptResult> decryptSce(OmemoManagerPrivate *d, QXmppOmemoElement element, QString senderJid, bool isMessage)
{
    if (!d->isSetUp || !d->isEnabled(Version::Omemo2)) {
        co_return QXmppError { u"OMEMO 2 is not set up"_s, {} };
    }
    const auto envelope = element.searchEnvelope(d->ownJid(), d->device.deviceId);
    if (!envelope) {
        co_return E2ee::NoEnvelope { .sentByThisDevice = d->isSentByThisDevice(senderJid, element.senderDeviceId()) };
    }

    auto key = co_await d->decryptKey(Version::Omemo2, senderJid, element.senderDeviceId(), envelope->data(), envelope->isUsedForKeyExchange(), isMessage).withContext(d->q);
    if (auto *error = std::get_if<QXmppError>(&key)) {
        co_return std::move(*error);
    }
    const auto &decryptedKey = std::get<DecryptedEnvelope>(key);

    QXmppE2eeMetadata metadata;
    metadata.setEncryption(QXmpp::Omemo2);
    metadata.setSenderKey(decryptedKey.senderKey);

    E2ee::Decrypted result;
    if (element.payload().isEmpty()) {
        // empty message, e.g. completing a key exchange
        result.setMetadata(metadata);
        co_return result;
    }

    auto plaintext = PayloadCipher::decrypt(Version::Omemo2, decryptedKey.data, {}, element.payload());
    if (auto *error = std::get_if<QXmppError>(&plaintext)) {
        co_return std::move(*error);
    }
    auto sceEnvelope = parseSceEnvelope(std::get<QByteArray>(plaintext));
    if (sceEnvelope.isNull()) {
        co_return QXmppError { u"The decrypted payload is no SCE envelope"_s, {} };
    }

    QXmppSceEnvelopeReader reader(std::move(sceEnvelope));
    metadata.setSceTimestamp(reader.timestamp());
    result.setContent(reader.contentElement());
    result.setMetadata(metadata);
    auto sceFrom = QXmpp::Private::jidOrNull(reader.from());
    auto sceTo = QXmpp::Private::jidOrNull(reader.to());
    if ((!reader.from().isEmpty() && sceFrom.isNull()) || (!reader.to().isEmpty() && sceTo.isNull())) {
        co_return QXmppError { u"The SCE envelope contains an invalid JID"_s, {} };
    }
    result.setSceFrom(std::move(sceFrom));
    result.setSceTo(std::move(sceTo));
    co_return result;
}

QXmppTask<E2ee::DecryptResult> OmemoManagerPrivate::decryptMessage2(E2ee::DecryptRequest request)
{
    auto element = request.message().extensions().get<QXmppOmemoElement>();
    if (!element) {
        return makeReadyTask<E2ee::DecryptResult>(QXmppError { u"The message contains no OMEMO element"_s, {} });
    }
    // Also encrypted with OMEMO 0, e.g. for this device if the sender does not know yet that it
    // uses OMEMO 2. Elements sent in the clear for OMEMO 0 are not used.
    if (request.message().extensions().get<Omemo0::Element>() && isEnabled(Version::Omemo0) &&
        !element->searchEnvelope(ownJid(), device.deviceId)) {
        return decryptMessage0(std::move(request));
    }
    return decryptSce(this, std::move(*element), request.senderJid(), true);
}

QXmppTask<E2ee::DecryptResult> OmemoManagerPrivate::decryptIq2(E2ee::DecryptIqRequest request)
{
    const auto encrypted = firstChildElement(request.iq(), u"encrypted", ns_omemo_2);
    if (encrypted.isNull()) {
        return makeReadyTask<E2ee::DecryptResult>(QXmppError { u"The IQ contains no OMEMO element"_s, {} });
    }
    QXmppOmemoElement element;
    element.parse(encrypted);
    return decryptSce(this, std::move(element), request.senderJid(), false);
}

QXmppTask<E2ee::EncryptResult> OmemoManagerPrivate::encryptMessage0(E2ee::EncryptRequest request)
{
    if (!isSetUp || !isEnabled(Version::Omemo0)) {
        co_return QXmppError { u"OMEMO 0 is not set up"_s, SendError::EncryptionError };
    }
    const auto message = request.bodyOnlyMessage();
    if (!message) {
        co_return QXmppError { u"The message can not be encrypted with OMEMO 0"_s, SendError::EncryptionError };
    }
    // only the body is encrypted
    if (message->body().isEmpty()) {
        E2ee::Encrypted result;
        result.setInfo(EncryptionInfo());
        co_return result;
    }

    auto selected = co_await selectDevices({ Version::Omemo0 }, request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS)).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&selected)) {
        co_return std::move(*error);
    }
    auto &selection = std::get<DeviceSelection>(selected);
    EncryptionInfo info { .skippedDevices = std::move(selection.skippedDevices) };
    auto element = encryptOmemo0(this, message->body(), selection.recipients, info);
    if (auto *error = std::get_if<QXmppError>(&element)) {
        co_return std::move(*error);
    }

    Xml::Extensions elements;
    elements.add(std::get<Omemo0::Element>(std::move(element)));
    E2ee::Encrypted result;
    result.setElements(std::move(elements));
    result.setInfo(std::move(info));
    co_return result;
}

// Encrypts for each device with the best version it supports. The OMEMO 2 element contains all
// sensitive elements, the OMEMO 0 one only the body of the body-only message.
QXmppTask<E2ee::EncryptResult> OmemoManagerPrivate::encryptMessageAutomatic(E2ee::EncryptRequest request)
{
    if (!isSetUp) {
        co_return QXmppError { u"OMEMO is not set up"_s, SendError::EncryptionError };
    }
    const auto &message = request.message();
    const auto acceptedTrustLevels = request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS);

    QList<Version> usedVersions;
    if (isEnabled(Version::Omemo2)) {
        usedVersions.append(Version::Omemo2);
    }
    if (isEnabled(Version::Omemo0)) {
        usedVersions.append(Version::Omemo0);
    }
    if (usedVersions.isEmpty()) {
        co_return QXmppError { u"No OMEMO version is enabled"_s, SendError::EncryptionError };
    }

    auto selected = co_await selectDevices(usedVersions, request.recipientJids(), acceptedTrustLevels).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&selected)) {
        co_return std::move(*error);
    }
    auto usesVersion = [&](Version version) {
        return std::ranges::any_of(std::get<DeviceSelection>(selected).recipients, [&](const Recipient &recipient) { return recipient.version == version; });
    };

    std::optional<QXmppMessage> bodyOnly;
    QList<EncryptionInfo::SkippedDevice> omemo0Skipped;
    if (usesVersion(Version::Omemo0)) {
        bodyOnly = request.bodyOnlyMessage();
        if (!bodyOnly) {
            if (!isEnabled(Version::Omemo2)) {
                co_return QXmppError { u"The message can not be encrypted with OMEMO 0"_s, SendError::EncryptionError };
            }
            const auto &first = std::get<DeviceSelection>(selected);
            for (const auto &recipient : first.recipients) {
                if (recipient.version == Version::Omemo0) {
                    omemo0Skipped.append({ publicDevice(Version::Omemo0, recipient.jid, recipient.deviceId, recipient.trustLevel), SkipReason::BodyOnlyUnsupported });
                }
            }
            for (const auto &skipped : first.skippedDevices) {
                if (skipped.device.version == Version::Omemo0) {
                    omemo0Skipped.append(skipped);
                }
            }
            // selected again, so the devices of OMEMO 0 do not count for the limit
            selected = co_await selectDevices({ Version::Omemo2 }, request.recipientJids(), acceptedTrustLevels).withContext(q);
            if (auto *error = std::get_if<QXmppError>(&selected)) {
                co_return std::move(*error);
            }
        }
    }
    auto &selection = std::get<DeviceSelection>(selected);
    const auto &recipients = selection.recipients;
    EncryptionInfo info { .skippedDevices = std::move(selection.skippedDevices) };
    info.skippedDevices.append(omemo0Skipped);

    Xml::Extensions elements;
    E2ee::Encrypted result;
    if (usesVersion(Version::Omemo2)) {
        auto element = encryptOmemo2(this, messageSceEnvelope(ownJid(), message), recipients, info);
        if (auto *error = std::get_if<QXmppError>(&element)) {
            co_return std::move(*error);
        }
        elements.add(std::get<QXmppOmemoElement>(std::move(element)));
    }
    if (bodyOnly && !bodyOnly->body().isEmpty()) {
        auto element = encryptOmemo0(this, bodyOnly->body(), recipients, info);
        if (auto *error = std::get_if<QXmppError>(&element)) {
            co_return std::move(*error);
        }
        elements.add(std::get<Omemo0::Element>(std::move(element)));
    }
    result.setElements(std::move(elements));
    result.setEncryption(usesVersion(Version::Omemo2) ? E2ee::Omemo2.toString() : E2ee::Omemo0.toString());
    result.setContentEncryption(bodyOnly ? E2ee::ContentEncryption::BodyOnly : E2ee::ContentEncryption::Sce);
    result.setInfo(std::move(info));
    co_return result;
}

QXmppTask<E2ee::DecryptResult> OmemoManagerPrivate::decryptMessage0(E2ee::DecryptRequest request)
{
    if (!isSetUp || !isEnabled(Version::Omemo0)) {
        co_return QXmppError { u"OMEMO 0 is not set up"_s, {} };
    }
    const auto element = request.message().extensions().get<Omemo0::Element>();
    if (!element) {
        co_return QXmppError { u"The message contains no OMEMO element"_s, {} };
    }

    if (isEnabled(Version::Omemo2) && !request.message().extensions().get<QXmppOmemoElement>() && usesOmemo2(request.senderJid(), element->senderDeviceId)) {
        q->warning(u"Received a message encrypted only with OMEMO 0 from %1/%2, which uses OMEMO 2"_s.arg(request.senderJid(), QString::number(element->senderDeviceId)));
    }

    // The envelopes are not grouped by JID, the device ID identifies the recipient.
    const auto envelope = std::ranges::find(element->envelopes, device.deviceId, &Omemo0::Envelope::recipientDeviceId);
    if (envelope == element->envelopes.end()) {
        co_return E2ee::NoEnvelope { .sentByThisDevice = isSentByThisDevice(request.senderJid(), element->senderDeviceId) };
    }

    auto key = co_await decryptKey(Version::Omemo0, request.senderJid(), element->senderDeviceId, envelope->data, envelope->isUsedForKeyExchange, true).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&key)) {
        co_return std::move(*error);
    }
    const auto &decryptedKey = std::get<DecryptedEnvelope>(key);

    QXmppE2eeMetadata metadata;
    metadata.setEncryption(QXmpp::Omemo0);
    // also if decrypted by the provider of OMEMO 2 because this device only got an OMEMO 0 key
    metadata.setContentEncryption(E2ee::ContentEncryption::BodyOnly);
    metadata.setSenderKey(decryptedKey.senderKey);

    E2ee::Decrypted result;
    result.setMetadata(metadata);
    if (!element->payload) {
        // key transport, e.g. completing a key exchange
        co_return result;
    }

    auto plaintext = PayloadCipher::decrypt(Version::Omemo0, decryptedKey.data, element->iv, *element->payload);
    if (auto *error = std::get_if<QXmppError>(&plaintext)) {
        co_return std::move(*error);
    }

    QDomDocument document;
    auto content = document.createElementNS(staticString(ns_sce), u"content"_s);
    auto body = document.createElementNS(staticString(ns_client), u"body"_s);
    body.appendChild(document.createTextNode(QString::fromUtf8(std::get<QByteArray>(plaintext))));
    content.appendChild(body);
    document.appendChild(content);
    result.setContent(content);
    co_return result;
}

}  // namespace QXmpp::Omemo::Private

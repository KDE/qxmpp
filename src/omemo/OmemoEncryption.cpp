// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
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

namespace QXmpp::Omemo {

// OMEMO 2 empty messages encrypt this number of zero bytes
constexpr qsizetype EMPTY_MESSAGE_KEY_SIZE = 32;
constexpr uint32_t SCE_RPAD_SIZE_MAX = 200;

QString ManagerPrivate::trustNamespace(Version version)
{
    return version == Version::Omemo0 ? staticString(ns_omemo) : staticString(ns_omemo_2);
}

// A coroutine taking all arguments by value: the providers are copied and may be destroyed while it
// is suspended.
template<typename Result, typename Request>
static QXmppTask<Result> afterLoading(ManagerPrivate *d, QXmppTask<Result> (ManagerPrivate::*function)(Request), Request request)
{
    co_await d->loaded().withContext(d->q);
    co_return co_await (d->*function)(std::move(request)).withContext(d->q);
}

void ManagerPrivate::registerProviders()
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
        .decryptMessage = [this](E2ee::DecryptRequest request) { return afterLoading(this, &ManagerPrivate::decryptMessage2, std::move(request)); },
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &ManagerPrivate::encryptMessage2, std::move(request)); },
        .encryptIq = [this](E2ee::EncryptIqRequest request) { return afterLoading(this, &ManagerPrivate::encryptIq2, std::move(request)); },
        .decryptIq = [this](E2ee::DecryptIqRequest request) { return afterLoading(this, &ManagerPrivate::decryptIq2, std::move(request)); },
        .priority = 1,
        .isEnabled = [this] { return isEnabled(Version::Omemo2); },
    });
    // OMEMO 0 encrypts only the body and no IQs.
    E2ee::Providers(q->hooks()).add(E2ee::Provider {
        .encryption = E2ee::Omemo0,
        .contentEncryption = E2ee::ContentEncryption::BodyOnly,
        .decryptMessage = [this](E2ee::DecryptRequest request) { return afterLoading(this, &ManagerPrivate::decryptMessage0, std::move(request)); },
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &ManagerPrivate::encryptMessage0, std::move(request)); },
        .isEnabled = [this] { return isEnabled(Version::Omemo0); },
    });
    // The automatic version only sends elements of the other providers.
    E2ee::Providers(q->hooks()).add(E2ee::Provider {
        .encryption = Automatic,
        .encryptMessage = [this](E2ee::EncryptRequest request) { return afterLoading(this, &ManagerPrivate::encryptMessageAutomatic, std::move(request)); },
        .encryptIq = [this](E2ee::EncryptIqRequest request) { return afterLoading(this, &ManagerPrivate::encryptIq2, std::move(request)); },
    });
}

Storage::Device *ManagerPrivate::storedDevice(Version version, const QString &jid, uint32_t deviceId)
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
QXmppTask<TrustLevel> ManagerPrivate::trustLevel(Version version, QString jid, QByteArray key)
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
QList<QByteArray> ManagerPrivate::sameIdentityKeys(Version version, const QString &jid, const QByteArray &key) const
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
static bool isSameDevice(const Storage::Device &omemo2Device, const Storage::Device &omemo0Device)
{
    if (omemo2Device.keyId.isEmpty()) {
        return false;
    }
    return omemo0Device.keyId.isEmpty() ? omemo2Device.id == omemo0Device.id : omemo0IdentityKey(omemo2Device.keyId) == omemo0Device.keyId;
}

// Whether the OMEMO 0 device of the JID uses OMEMO 2. It is encrypted for with OMEMO 2 only then.
bool ManagerPrivate::usesOmemo2(const QString &jid, uint32_t omemo0DeviceId) const
{
    const auto omemo0Devices = versions.at(Version::Omemo0).devices.value(jid);
    const auto omemo0Device = omemo0Devices.value(omemo0DeviceId, Storage::Device { Version::Omemo0, jid, omemo0DeviceId });
    const auto omemo2Devices = versions.at(Version::Omemo2).devices.value(jid);
    return std::ranges::any_of(omemo2Devices, [&](const Storage::Device &device) { return isSameDevice(device, omemo0Device); });
}

// Whether the device has been removed from the OMEMO 2 device list, but is still listed in the
// OMEMO 0 one. It stays encrypted for with OMEMO 2, so a server can not make the clients fall back
// to OMEMO 0 by hiding the OMEMO 2 device list.
bool ManagerPrivate::isSticky(const Storage::Device &omemo2Device) const
{
    if (!omemo2Device.removalFromDeviceListDate.isValid()) {
        return false;
    }
    const auto omemo0Devices = versions.at(Version::Omemo0).devices.value(omemo2Device.jid);
    return std::ranges::any_of(omemo0Devices, [&](const Storage::Device &device) {
        return !device.removalFromDeviceListDate.isValid() && isSameDevice(omemo2Device, device);
    });
}

// Builds a session with the device if there is none. Returns whether the device can be encrypted
// for.
QXmppTask<bool> ManagerPrivate::prepareSession(Version version, QString jid, uint32_t deviceId, TrustLevels acceptedTrustLevels)
{
    const Address address { jid, deviceId };
    auto *device = storedDevice(version, jid, deviceId);
    if (!device || device->unrespondedSentStanzasCount >= UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_IS_STOPPED) {
        co_return false;
    }

    if (versions.at(version).sessions->hasSession(address) && !device->keyId.isEmpty()) {
        co_return acceptedTrustLevels.testFlag(co_await trustLevel(version, jid, device->keyId).withContext(q));
    }

    auto result = co_await pubSubClient(version).requestBundle(jid, deviceId).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        q->warning(u"Could not fetch the bundle of %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
        co_return false;
    }
    const auto &bundle = std::get<Pep::PublicBundle>(result);

    // The key of a device does not change. A changed key could be used to take over a device.
    device = storedDevice(version, jid, deviceId);
    if (!device) {
        co_return false;
    }
    if (!device->keyId.isEmpty() && device->keyId != bundle.identityKey) {
        q->warning(u"The bundle of %1/%2 has another identity key than before"_s.arg(jid, QString::number(deviceId)));
        co_return false;
    }
    const auto trust = co_await trustLevel(version, jid, bundle.identityKey).withContext(q);
    device = storedDevice(version, jid, deviceId);
    if (!device) {
        co_return false;
    }
    // also stored if the device is not trusted, so the key can be authenticated
    if (device->keyId.isEmpty()) {
        device->keyId = bundle.identityKey;
        storage->addDevice(*device);
        devicesChanged(jid);
    }
    if (!acceptedTrustLevels.testFlag(trust)) {
        co_return false;
    }

    auto &sessions = *versions.at(version).sessions;
    if (sessions.hasSession(address)) {
        co_return true;
    }
    if (bundle.preKeys.isEmpty()) {
        q->warning(u"The bundle of %1/%2 contains no pre keys"_s.arg(jid, QString::number(deviceId)));
        co_return false;
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
        co_return false;
    }
    co_return true;
}

// Selects the listed devices of the JIDs and the other own devices with an accepted trust level,
// but at most maxDevicesPerStanza devices, the own ones first. With both versions (OMEMO 2 first,
// so its devices are known when selecting the OMEMO 0 ones), each device gets the best version it
// supports: devices using OMEMO 2 only get OMEMO 2, also if they have been removed from its device
// list (see isSticky()).
QXmppTask<Result<QList<Recipient>>> ManagerPrivate::selectDevices(QList<Version> usedVersions, QList<QString> jids, TrustLevels acceptedTrustLevels)
{
    const auto own = ownJid();
    const auto hasRecipients = std::ranges::any_of(jids, [&](const QString &jid) { return jid != own; });
    jids.removeAll(own);
    jids.prepend(own);
    const auto perDevice = usedVersions.contains(Version::Omemo0) && usedVersions.contains(Version::Omemo2);

    QList<Recipient> recipients;
    qsizetype skippedCount = 0;
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
            return ids;
        };

        if (recipients.size() >= maxDevicesPerStanza) {
            for (auto version : std::as_const(usedVersions)) {
                skippedCount += candidates(version).size();
            }
            continue;
        }

        for (auto version : std::as_const(usedVersions)) {
            // Device lists of contacts are subscribed, but they are unknown before the first
            // message or if only unlisted devices are known from their messages.
            if (jid != own && deviceIds(version, jid).isEmpty()) {
                auto result = co_await pubSubClient(version).requestDeviceList(jid).withContext(q);
                if (auto *list = std::get_if<QList<DeviceList::Entry>>(&result)) {
                    applyReceivedDeviceList(version, jid, *list);
                }
            }
        }

        for (auto version : std::as_const(usedVersions)) {
            for (auto deviceId : candidates(version)) {
                if (perDevice && version == Version::Omemo0 && usesOmemo2(jid, deviceId)) {
                    continue;
                }
                if (recipients.size() >= maxDevicesPerStanza) {
                    skippedCount++;
                    continue;
                }
                if (co_await prepareSession(version, jid, deviceId, acceptedTrustLevels).withContext(q)) {
                    recipients.append({ version, jid, deviceId });
                }
            }
        }
    }

    if (skippedCount) {
        q->warning(u"Not encrypted for %1 devices above the maximum of %2 devices per stanza"_s.arg(QString::number(skippedCount), QString::number(maxDevicesPerStanza)));
    }
    if (hasRecipients && std::ranges::all_of(recipients, [&](const Recipient &recipient) { return recipient.jid == own; })) {
        co_return QXmppError { u"There is no device of the recipients with an accepted trust level"_s, SendError::EncryptionError };
    }
    co_return recipients;
}

// Encrypts the key material for the recipients using the version.
QList<Envelope> ManagerPrivate::encryptKey(Version version, const QList<Recipient> &recipients, const SecureByteArray &keyMaterial)
{
    QList<Envelope> envelopes;
    for (const auto &[recipientVersion, jid, deviceId] : recipients) {
        if (recipientVersion != version) {
            continue;
        }
        auto encrypted = versions.at(version).sessions->encrypt({ jid, deviceId }, keyMaterial);
        if (auto *error = std::get_if<QXmppError>(&encrypted)) {
            q->warning(u"Could not encrypt for %1/%2: %3"_s.arg(jid, QString::number(deviceId), error->description));
            continue;
        }
        const auto &key = std::get<EncryptedKey>(encrypted);
        envelopes.append({ jid, deviceId, key.data, key.isKeyExchange });

        if (auto *device = storedDevice(version, jid, deviceId)) {
            device->unrespondedSentStanzasCount = std::min(device->unrespondedSentStanzasCount + 1, UNRESPONDED_STANZAS_UNTIL_ENCRYPTION_IS_STOPPED);
            device->unrespondedReceivedStanzasCount = 0;
            storage->addDevice(*device);
        }
    }
    return envelopes;
}

QXmppTask<Result<DecryptedKey>> ManagerPrivate::decryptKey(Version version, QString jid, uint32_t deviceId, QByteArray data, bool isKeyExchange, bool isMessage)
{
    const Address address { jid, deviceId };
    auto &state = versions.at(version);
    auto &sessions = *state.sessions;
    if (!isKeyExchange && !sessions.hasSession(address)) {
        // The other device sends key exchanges until it receives a message from this device.
        buildSessionAndSendEmptyMessage(version, jid, deviceId);
        co_return QXmppError { u"There is no session with %1/%2, a new one is built"_s.arg(jid, QString::number(deviceId)), {} };
    }

    // The key of a device does not change, like in prepareSession().
    const auto *knownDevice = storedDevice(version, jid, deviceId);
    auto result = sessions.decrypt(address, data, isKeyExchange, knownDevice ? knownDevice->keyId : QByteArray());
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    auto &decrypted = std::get<Private::DecryptedKey>(result);

    // the session callback has created the device if it was unknown
    auto *device = storedDevice(version, jid, deviceId);
    Q_ASSERT(device);
    // The device list may be outdated. It is requested once per login, later changes are notified.
    if (device->removalFromDeviceListDate.isValid() && !state.refreshedDeviceLists.contains(jid)) {
        state.refreshedDeviceLists.insert(jid);
        refreshDeviceList(version, jid);
    }
    if (isKeyExchange) {
        if (device->keyId.isEmpty()) {
            device->keyId = decrypted.senderIdentityKey;
            devicesChanged(jid);
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
    co_return DecryptedKey { std::move(decrypted.data), senderKey };
}

void ManagerPrivate::refreshDeviceList(Version version, const QString &jid)
{
    pubSubClient(version).requestDeviceList(jid).then(q, [this, version, jid](Result<QList<DeviceList::Entry>> &&result) {
        if (auto *list = std::get_if<QList<DeviceList::Entry>>(&result)) {
            applyReceivedDeviceList(version, jid, *list);
        }
    });
}

// Sends a message without payload, e.g. to complete a key exchange or as heartbeat.
void ManagerPrivate::sendEmptyMessage(Version version, const QString &jid, uint32_t deviceId)
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

void ManagerPrivate::buildSessionAndSendEmptyMessage(Version version, const QString &jid, uint32_t deviceId)
{
    // Messages arriving before the session is built do not start another one.
    const Address address { jid, deviceId };
    if (versions.at(version).pendingSessionBuilds.contains(address)) {
        return;
    }

    // Only listed devices are encrypted for. The device is added as unlisted if it is unknown.
    if (!storedDevice(version, jid, deviceId)) {
        auto &device = versions.at(version).devices[jid][deviceId];
        device = Storage::Device { version, jid, deviceId };
        device.removalFromDeviceListDate = now();
        storage->addDevice(device);
    }
    versions.at(version).pendingSessionBuilds.insert(address);
    prepareSession(version, jid, deviceId, DEFAULT_ACCEPTED_TRUST_LEVELS | TrustLevel::AutomaticallyDistrusted | TrustLevel::ManuallyDistrusted)
        .then(q, [this, version, jid, deviceId, address](bool prepared) {
            versions.at(version).pendingSessionBuilds.remove(address);
            if (prepared) {
                sendEmptyMessage(version, jid, deviceId);
            }
        });
}

// Replaces used pre keys. Bundle publications are not run in parallel.
void ManagerPrivate::refillPreKeysAndPublish(Version version)
{
    auto &state = versions.at(version);
    auto added = KeyMaintenance::refillPreKeys(context, state.keys, keyConfig);
    if (auto *error = std::get_if<QXmppError>(&added)) {
        q->warning(u"Could not create pre keys: "_s + error->description);
        return;
    }
    QList<Storage::PreKey> records;
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

static Result<QXmppOmemoElement> encryptOmemo2(ManagerPrivate *d, const QByteArray &sceEnvelope, const QList<Recipient> &recipients)
{
    auto payload = PayloadCipher::encrypt(Version::Omemo2, sceEnvelope);
    if (auto *error = std::get_if<QXmppError>(&payload)) {
        return std::move(*error);
    }
    const auto &encrypted = std::get<PayloadCipher::Encrypted>(payload);

    QXmppOmemoElement element;
    element.setSenderDeviceId(d->device.deviceId);
    element.setPayload(encrypted.ciphertext);
    for (const auto &envelope : d->encryptKey(Version::Omemo2, recipients, encrypted.keyMaterial)) {
        QXmppOmemoEnvelope omemoEnvelope;
        omemoEnvelope.setRecipientDeviceId(envelope.deviceId);
        omemoEnvelope.setUsedForKeyExchange(envelope.isKeyExchange);
        omemoEnvelope.setData(envelope.data);
        element.addEnvelope(envelope.jid, omemoEnvelope);
    }
    return element;
}

static Result<Omemo0::Element> encryptOmemo0(ManagerPrivate *d, const QString &body, const QList<Recipient> &recipients)
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
    for (const auto &envelope : d->encryptKey(Version::Omemo0, recipients, encrypted.keyMaterial)) {
        element.envelopes.append({ envelope.deviceId, envelope.isKeyExchange, envelope.data });
    }
    return element;
}

static QXmppTask<E2ee::EncryptResult> encryptSce(ManagerPrivate *d, QByteArray sceEnvelope, QList<QString> recipientJids, TrustLevels acceptedTrustLevels)
{
    if (!d->isSetUp || !d->isEnabled(Version::Omemo2)) {
        co_return QXmppError { u"OMEMO 2 is not set up"_s, SendError::EncryptionError };
    }
    auto recipients = co_await d->selectDevices({ Version::Omemo2 }, recipientJids, acceptedTrustLevels).withContext(d->q);
    if (auto *error = std::get_if<QXmppError>(&recipients)) {
        co_return std::move(*error);
    }
    auto element = encryptOmemo2(d, sceEnvelope, std::get<QList<Recipient>>(recipients));
    if (auto *error = std::get_if<QXmppError>(&element)) {
        co_return std::move(*error);
    }

    Xml::Extensions elements;
    elements.add(std::get<QXmppOmemoElement>(std::move(element)));
    E2ee::Encrypted result;
    result.setElements(std::move(elements));
    co_return result;
}

static QByteArray messageSceEnvelope(const QString &from, const QXmppMessage &message)
{
    return createSceEnvelope(from, QXmppUtils::jidToBareJid(message.to()), [&](QXmlStreamWriter &writer) {
        message.serializeExtensions(&writer, SceSensitive, staticString(ns_client));
    });
}

// Whether the message has elements that are only sent if encrypted, apart from the body.
static bool hasSensitiveContent(const QXmppMessage &message)
{
    auto withoutBody = message;
    withoutBody.setBody({});
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    withoutBody.serializeExtensions(&writer, SceSensitive, staticString(ns_client));
    return !xml.isEmpty();
}

// Whether the body alone has the meaning of the message, for devices only receiving the body
// (UnprotectedContent::Drop). A correction would appear as a new message.
static bool isBodyEnough(const QXmppMessage &message)
{
    return !message.body().isEmpty() && message.replaceId().isEmpty();
}

QXmppTask<E2ee::EncryptResult> ManagerPrivate::encryptMessage2(E2ee::EncryptRequest request)
{
    return encryptSce(this, messageSceEnvelope(ownJid(), request.message()), request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS));
}

QXmppTask<E2ee::EncryptResult> ManagerPrivate::encryptIq2(E2ee::EncryptIqRequest request)
{
    const auto sce = createSceEnvelope(ownJid(), QXmppUtils::jidToBareJid(request.to()), [&](QXmlStreamWriter &writer) {
        request.writeContent(&writer);
    });
    return encryptSce(this, sce, request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS));
}

static QXmppTask<E2ee::DecryptResult> decryptSce(ManagerPrivate *d, QXmppOmemoElement element, QString senderJid, bool isMessage)
{
    if (!d->isSetUp || !d->isEnabled(Version::Omemo2)) {
        co_return QXmppError { u"OMEMO 2 is not set up"_s, {} };
    }
    const auto envelope = element.searchEnvelope(d->ownJid(), d->device.deviceId);
    if (!envelope) {
        co_return E2ee::NoEnvelope {};
    }

    auto key = co_await d->decryptKey(Version::Omemo2, senderJid, element.senderDeviceId(), envelope->data(), envelope->isUsedForKeyExchange(), isMessage).withContext(d->q);
    if (auto *error = std::get_if<QXmppError>(&key)) {
        co_return std::move(*error);
    }
    const auto &decryptedKey = std::get<DecryptedKey>(key);

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
    result.setSceFrom(reader.from());
    result.setSceTo(reader.to());
    co_return result;
}

QXmppTask<E2ee::DecryptResult> ManagerPrivate::decryptMessage2(E2ee::DecryptRequest request)
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

QXmppTask<E2ee::DecryptResult> ManagerPrivate::decryptIq2(E2ee::DecryptIqRequest request)
{
    const auto encrypted = firstChildElement(request.iq(), u"encrypted", ns_omemo_2);
    if (encrypted.isNull()) {
        return makeReadyTask<E2ee::DecryptResult>(QXmppError { u"The IQ contains no OMEMO element"_s, {} });
    }
    QXmppOmemoElement element;
    element.parse(encrypted);
    return decryptSce(this, std::move(element), request.senderJid(), false);
}

QXmppTask<E2ee::EncryptResult> ManagerPrivate::encryptMessage0(E2ee::EncryptRequest request)
{
    if (!isSetUp || !isEnabled(Version::Omemo0)) {
        co_return QXmppError { u"OMEMO 0 is not set up"_s, SendError::EncryptionError };
    }
    const auto &message = request.message();
    const auto drop = request.unprotectedContent().value_or(unprotectedContent) == Manager::UnprotectedContent::Drop;
    if (drop && !isBodyEnough(message) && hasSensitiveContent(message)) {
        co_return QXmppError { u"The message can not be encrypted with OMEMO 0 without sending unprotected content"_s, SendError::EncryptionError };
    }
    // only the body is encrypted
    if (message.body().isEmpty()) {
        co_return E2ee::Encrypted();
    }

    auto recipients = co_await selectDevices({ Version::Omemo0 }, request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS)).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&recipients)) {
        co_return std::move(*error);
    }
    auto element = encryptOmemo0(this, message.body(), std::get<QList<Recipient>>(recipients));
    if (auto *error = std::get_if<QXmppError>(&element)) {
        co_return std::move(*error);
    }

    Xml::Extensions elements;
    elements.add(std::get<Omemo0::Element>(std::move(element)));
    E2ee::Encrypted result;
    result.setElements(std::move(elements));
    if (drop) {
        // only the public elements are sent
        result.setContentEncryption(E2ee::ContentEncryption::Sce);
    }
    co_return result;
}

// Encrypts for each device with the best version it supports. The OMEMO 2 element contains all
// sensitive elements, also if the other ones are sent in the clear for OMEMO 0.
QXmppTask<E2ee::EncryptResult> ManagerPrivate::encryptMessageAutomatic(E2ee::EncryptRequest request)
{
    if (!isSetUp) {
        co_return QXmppError { u"OMEMO is not set up"_s, SendError::EncryptionError };
    }
    const auto &message = request.message();
    const auto plaintext = request.unprotectedContent().value_or(unprotectedContent) == Manager::UnprotectedContent::Plaintext;

    QList<Version> usedVersions;
    if (isEnabled(Version::Omemo2)) {
        usedVersions.append(Version::Omemo2);
    }
    if (isEnabled(Version::Omemo0) && (plaintext || isBodyEnough(message))) {
        usedVersions.append(Version::Omemo0);
    }
    if (usedVersions.isEmpty()) {
        co_return QXmppError { u"The message can not be encrypted with the enabled OMEMO versions without sending unprotected content"_s, SendError::EncryptionError };
    }

    auto selected = co_await selectDevices(usedVersions, request.recipientJids(), request.params().acceptedTrustLevels().value_or(DEFAULT_ACCEPTED_TRUST_LEVELS)).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&selected)) {
        co_return std::move(*error);
    }
    const auto &recipients = std::get<QList<Recipient>>(selected);
    auto usesVersion = [&](Version version) {
        return std::ranges::any_of(recipients, [&](const Recipient &recipient) { return recipient.version == version; });
    };

    Xml::Extensions elements;
    E2ee::Encrypted result;
    if (usesVersion(Version::Omemo2)) {
        auto element = encryptOmemo2(this, messageSceEnvelope(ownJid(), message), recipients);
        if (auto *error = std::get_if<QXmppError>(&element)) {
            co_return std::move(*error);
        }
        elements.add(std::get<QXmppOmemoElement>(std::move(element)));
    }
    const auto usesOmemo0 = usesVersion(Version::Omemo0);
    if (usesOmemo0 && !message.body().isEmpty()) {
        auto element = encryptOmemo0(this, message.body(), recipients);
        if (auto *error = std::get_if<QXmppError>(&element)) {
            co_return std::move(*error);
        }
        elements.add(std::get<Omemo0::Element>(std::move(element)));
    }
    result.setElements(std::move(elements));
    result.setEncryption(usesVersion(Version::Omemo2) ? E2ee::Omemo2.toString() : E2ee::Omemo0.toString());
    result.setContentEncryption(usesOmemo0 && plaintext ? E2ee::ContentEncryption::BodyOnly : E2ee::ContentEncryption::Sce);
    co_return result;
}

QXmppTask<E2ee::DecryptResult> ManagerPrivate::decryptMessage0(E2ee::DecryptRequest request)
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
        co_return E2ee::NoEnvelope {};
    }

    auto key = co_await decryptKey(Version::Omemo0, request.senderJid(), element->senderDeviceId, envelope->data, envelope->isUsedForKeyExchange, true).withContext(q);
    if (auto *error = std::get_if<QXmppError>(&key)) {
        co_return std::move(*error);
    }
    const auto &decryptedKey = std::get<DecryptedKey>(key);

    QXmppE2eeMetadata metadata;
    metadata.setEncryption(QXmpp::Omemo0);
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

}  // namespace QXmpp::Omemo

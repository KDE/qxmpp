// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOMANAGER_P_H
#define OMEMOMANAGER_P_H

#include "QXmppOmemoManager.h"
#include "QXmppPromise.h"

#include "OmemoDeviceList.h"
#include "OmemoKeyMaintenance.h"
#include "OmemoPubSub.h"
#include "OmemoSession.h"

#include <array>

#include <QProperty>

class QXmppPubSubManager;

namespace QXmpp::Omemo::Private {

// maximum number of devices per JID and version
constexpr int MAX_DEVICES_PER_JID = 200;

struct VersionState {
    Version version;
    VersionKeys keys;
    // devices of other JIDs and other own devices
    QHash<QString, QHash<uint32_t, OmemoStorage::Device>> devices;
    std::unique_ptr<SessionLayer> sessions;
};

// states of all versions, indexed by the version
struct VersionStates {
    std::array<VersionState, 2> states { VersionState { .version = Version::Omemo0 }, VersionState { .version = Version::Omemo2 } };

    VersionState &at(Version version) { return states[std::size_t(version)]; }
    const VersionState &at(Version version) const { return states[std::size_t(version)]; }
};

// QBindables of const properties are read-only.
template<typename T>
QBindable<T> readOnly(const QProperty<T> &property)
{
    return &property;
}

class OmemoManagerPrivate
{
public:
    OmemoManagerPrivate(OmemoManager *q, std::shared_ptr<OmemoStorage> storage, QList<Version> enabledVersions);

    QString ownJid() const;
    QList<uint32_t> deviceIds(Version version, const QString &jid) const;
    bool isEnabled(Version version) const { return enabledVersions.value().contains(version); }
    QDateTime now() const { return fixedTime.isValid() ? fixedTime : QDateTime::currentDateTimeUtc(); }
    Pep::Client pubSubClient(Version version) const { return { pubSub, version }; }

    void setUpSessionLayers();
    OmemoStorage::OwnDevice storedOwnDevice() const;
    QString label() const;

    QXmppTask<void> loaded();
    QXmppTask<void> load();
    QXmppTask<void> setUp();
    QXmppTask<Result<>> createOwnDevice();
    QXmppTask<Result<>> setUpVersion(Version version);
    QXmppTask<Result<>> unpublishOwnDevice(Version version);
    void setOwnDevice(DeviceKeys &&keys);
    void reportError(const QString &context, const QXmppError &error);

    QXmppTask<Result<uint32_t>> generateDeviceId();
    QXmppTask<Result<>> publishOwnData(Version version);
    QXmppTask<Result<>> publishBundle(Version version);
    QXmppTask<Result<>> publishOwnDeviceList(Version version);

    void applyDeviceList(Version version, const QString &jid, const QList<DeviceList::Entry> &list);
    void clearDeviceList(Version version, const QString &jid);
    void storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, DeviceList::DeviceInfo> &infos, const DeviceList::Changes &changes);
    void removeExpiredDevices();

    OmemoManager *q;
    std::shared_ptr<OmemoStorage> storage;
    QXmppPubSubManager *pubSub = nullptr;
    SignalContext context;
    KeyMaintenance::Config keyConfig;
    // replaces the current time in tests
    QDateTime fixedTime;

    QProperty<QList<Version>> enabledVersions;
    // the own device exists
    bool isSetUp = false;
    bool isSettingUp = false;
    bool isLoggedIn = false;
    bool isLoadStarted = false;
    QList<QXmppPromise<void>> loadPromises;
    // deviceId is 0 until set up
    DeviceKeys device;
    // empty for the default label
    QString customLabel;

    QProperty<OmemoManager::State> state { OmemoManager::State::Loading };
    QProperty<uint32_t> ownDeviceId { 0 };
    QProperty<QByteArray> ownKey;
    QProperty<QByteArray> ownKeyOmemo0 { [this] { return omemo0IdentityKey(ownKey.value()); } };
    VersionStates versions;
};

}  // namespace QXmpp::Omemo::Private

#endif  // OMEMOMANAGER_P_H

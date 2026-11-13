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

namespace QXmpp::Omemo {

// maximum number of devices per JID and version
constexpr int MAX_DEVICES_PER_JID = 200;

struct VersionState {
    Version version;
    Private::VersionKeys keys;
    // devices of other JIDs and other own devices
    QHash<QString, QHash<uint32_t, Storage::Device>> devices;
    std::unique_ptr<Private::SessionLayer> sessions;
};

// states of all versions, indexed by the version
struct VersionStates {
    std::array<VersionState, 2> states { VersionState { .version = Version::Omemo0 }, VersionState { .version = Version::Omemo2 } };

    VersionState &at(Version version) { return states[std::size_t(version)]; }
    const VersionState &at(Version version) const { return states[std::size_t(version)]; }
};

class ManagerPrivate
{
public:
    ManagerPrivate(Manager *q, std::shared_ptr<Storage> storage, QList<Version> enabledVersions);

    QString ownJid() const;
    bool isEnabled(Version version) const { return enabledVersions.contains(version); }
    QDateTime now() const { return fixedTime.isValid() ? fixedTime : QDateTime::currentDateTimeUtc(); }
    Private::Pep::Client pubSubClient(Version version) const { return { pubSub, version }; }

    void setUpSessionLayers();
    Storage::OwnDevice storedOwnDevice() const;
    QString label() const;

    QXmppTask<void> loaded();
    QXmppTask<void> load();
    QXmppTask<void> setUp();
    QXmppTask<Result<>> createOwnDevice();
    QXmppTask<Result<>> setUpVersion(Version version);
    QXmppTask<Result<>> unpublishOwnDevice(Version version);
    void setOwnDevice(Private::DeviceKeys &&keys);
    void reportError(const QString &context, const QXmppError &error);

    QXmppTask<Result<uint32_t>> generateDeviceId();
    QXmppTask<Result<>> publishOwnData(Version version);
    QXmppTask<Result<>> publishBundle(Version version);
    QXmppTask<Result<>> publishOwnDeviceList(Version version);

    void applyDeviceList(Version version, const QString &jid, const QList<Private::DeviceList::Entry> &list);
    void clearDeviceList(Version version, const QString &jid);
    void storeDeviceListChanges(Version version, const QString &jid, const QHash<uint32_t, Private::DeviceList::DeviceInfo> &infos, const Private::DeviceList::Changes &changes);
    void removeExpiredDevices();

    Manager *q;
    std::shared_ptr<Storage> storage;
    QXmppPubSubManager *pubSub = nullptr;
    Private::SignalContext context;
    Private::KeyMaintenance::Config keyConfig;
    // replaces the current time in tests
    QDateTime fixedTime;

    QList<Version> enabledVersions;
    // the own device exists
    bool isSetUp = false;
    bool isSettingUp = false;
    bool isLoggedIn = false;
    bool isLoadStarted = false;
    QList<QXmppPromise<void>> loadPromises;
    // deviceId is 0 until set up
    Private::DeviceKeys device;
    // empty for the default label
    QString customLabel;

    QProperty<Manager::State> state { Manager::State::Loading };
    QProperty<uint32_t> ownDeviceId { 0 };
    QProperty<QByteArray> ownKey;
    VersionStates versions;
};

}  // namespace QXmpp::Omemo

#endif  // OMEMOMANAGER_P_H

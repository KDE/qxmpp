// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMODEVICELIST_H
#define OMEMODEVICELIST_H

#include <chrono>
#include <optional>

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

// Merging of received device lists into the known devices of a JID (for one version). The
// functions modify the devices in place and return what changed.
namespace QXmpp::Omemo::Private::DeviceList {

using namespace std::chrono_literals;

// devices that are not in the device list anymore are removed after this time
constexpr std::chrono::seconds REMOVAL_INTERVAL = 24h * 7 * 12;

// A device of a received device list. OMEMO 0 has no labels.
struct Entry {
    uint32_t id = 0;
    QString label;

    bool operator==(const Entry &) const = default;
};

// What the device list contains about a known device.
struct DeviceInfo {
    QString label;
    // set while the device is not in the device list
    QDateTime removalDate;

    bool operator==(const DeviceInfo &) const = default;
};

struct OwnDevice {
    uint32_t id = 0;
    // not compared for OMEMO 0, which has no labels
    std::optional<QString> label;
};

struct Changes {
    QList<uint32_t> added;
    // label changed or listed again after its removal
    QList<uint32_t> changed;
    // not in the list anymore, the removal date has been set
    QList<uint32_t> removed;
    // devices beyond the maximum that were ignored
    int ignoredCount = 0;
    // Own device list only: the list must be republished because the own device is missing,
    // has a wrong label or IDs are listed multiple times.
    bool republishOwnList = false;

    bool isEmpty() const { return added.isEmpty() && changed.isEmpty() && removed.isEmpty(); }
};

// Applies a received device list. For the own device list, ownDevice must be set; the own device
// is not added to the devices.
Changes apply(QHash<uint32_t, DeviceInfo> &devices,
              const QList<Entry> &list,
              const QDateTime &now,
              int maxDevices,
              const std::optional<OwnDevice> &ownDevice = {});

// The device list node or its item has been removed: all devices are marked as removed.
Changes clear(QHash<uint32_t, DeviceInfo> &devices, const QDateTime &now);

// Devices whose removal date is older than the removal interval, to be deleted with their keys and
// sessions.
QList<uint32_t> expired(const QHash<uint32_t, DeviceInfo> &devices, const QDateTime &now, std::chrono::seconds removalInterval = REMOVAL_INTERVAL);

// The time at which expired() returns devices next, invalid if no device is marked as removed.
QDateTime nextExpiry(const QHash<uint32_t, DeviceInfo> &devices, std::chrono::seconds removalInterval = REMOVAL_INTERVAL);

// The device list to publish for the own device (the own device first).
QList<Entry> ownList(const QHash<uint32_t, DeviceInfo> &otherOwnDevices, const OwnDevice &ownDevice);

}  // namespace QXmpp::Omemo::Private::DeviceList

#endif  // OMEMODEVICELIST_H

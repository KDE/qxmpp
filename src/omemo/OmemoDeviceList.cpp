// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoDeviceList.h"

#include <algorithm>

#include <QSet>

namespace QXmpp::Omemo::Private::DeviceList {

Changes apply(QHash<uint32_t, DeviceInfo> &devices, const QList<Entry> &list, const QDateTime &now, int maxDevices, const std::optional<OwnDevice> &ownDevice)
{
    Changes changes;

    // valid entries in their order, without duplicates and without the own device
    QList<Entry> entries;
    QSet<uint32_t> listedIds;
    auto isOwnDeviceListed = false;
    for (const auto &entry : list) {
        if (entry.id == 0 || listedIds.contains(entry.id)) {
            if (ownDevice) {
                changes.republishOwnList = true;
            }
            continue;
        }
        listedIds.insert(entry.id);

        if (ownDevice && entry.id == ownDevice->id) {
            isOwnDeviceListed = true;
            if (ownDevice->label && entry.label != *ownDevice->label) {
                changes.republishOwnList = true;
            }
            continue;
        }
        entries.append(entry);
    }
    if (ownDevice && !isOwnDeviceListed) {
        changes.republishOwnList = true;
    }

    if (entries.size() > maxDevices) {
        changes.ignoredCount = int(entries.size() - maxDevices);
        entries.resize(maxDevices);
    }

    QSet<uint32_t> keptIds;
    for (const auto &entry : std::as_const(entries)) {
        keptIds.insert(entry.id);

        auto it = devices.find(entry.id);
        if (it == devices.end()) {
            devices.insert(entry.id, { entry.label, {} });
            changes.added.append(entry.id);
        } else if (it->label != entry.label || it->removalDate.isValid()) {
            it->label = entry.label;
            it->removalDate = {};
            changes.changed.append(entry.id);
        }
    }

    // The removal date is only set once, so the removal interval is not restarted with every
    // update of the list.
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if (!keptIds.contains(it.key()) && !it->removalDate.isValid()) {
            it->removalDate = now;
            changes.removed.append(it.key());
        }
    }

    std::ranges::sort(changes.added);
    std::ranges::sort(changes.changed);
    std::ranges::sort(changes.removed);
    return changes;
}

Changes clear(QHash<uint32_t, DeviceInfo> &devices, const QDateTime &now)
{
    return apply(devices, {}, now, 0);
}

QList<uint32_t> expired(const QHash<uint32_t, DeviceInfo> &devices, const QDateTime &now, std::chrono::seconds removalInterval)
{
    QList<uint32_t> ids;
    for (auto it = devices.cbegin(); it != devices.cend(); ++it) {
        if (it->removalDate.isValid() && it->removalDate.secsTo(now) >= removalInterval.count()) {
            ids.append(it.key());
        }
    }
    std::ranges::sort(ids);
    return ids;
}

QDateTime nextExpiry(const QHash<uint32_t, DeviceInfo> &devices, std::chrono::seconds removalInterval)
{
    QDateTime next;
    for (const auto &device : devices) {
        if (device.removalDate.isValid()) {
            const auto expiry = device.removalDate.addSecs(removalInterval.count());
            if (!next.isValid() || expiry < next) {
                next = expiry;
            }
        }
    }
    return next;
}

QList<Entry> ownList(const QHash<uint32_t, DeviceInfo> &otherOwnDevices, const OwnDevice &ownDevice)
{
    QList<Entry> list { { ownDevice.id, ownDevice.label.value_or(QString()) } };

    QList<uint32_t> ids;
    for (auto it = otherOwnDevices.cbegin(); it != otherOwnDevices.cend(); ++it) {
        // Removed devices are not listed again, so other clients can remove them.
        if (!it->removalDate.isValid() && it.key() != ownDevice.id) {
            ids.append(it.key());
        }
    }
    std::ranges::sort(ids);
    for (const auto id : std::as_const(ids)) {
        list.append({ id, otherOwnDevices.value(id).label });
    }
    return list;
}

}  // namespace QXmpp::Omemo::Private::DeviceList

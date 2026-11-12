// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMOPUBSUB_H
#define OMEMOPUBSUB_H

#include "QXmppError.h"
#include "QXmppGlobal.h"
#include "QXmppOmemoStorage.h"
#include "QXmppPubSubBaseItem.h"
#include "QXmppTask.h"

#include "OmemoDeviceList.h"

#include <algorithm>
#include <variant>

class QDomElement;
class QXmppPubSubManager;

// Access to the device list and bundle nodes of both OMEMO versions.
//
// OMEMO 2 uses one bundle node with an item per device, OMEMO 0 uses one bundle node per device
// ('eu.siacs.conversations.axolotl.bundles:<id>') with a single item.
namespace QXmpp::Omemo::Private::Pep {

// Public part of a bundle with all pre keys; keys in the wire format of the version.
struct PublicBundle {
    QByteArray identityKey;
    uint32_t signedPreKeyId = 0;
    QByteArray signedPreKey;
    QByteArray signedPreKeySignature;
    QHash<uint32_t, QByteArray> preKeys;

    bool operator==(const PublicBundle &) const = default;
};

QString deviceListNode(Version version);
QString bundleNode(Version version, uint32_t deviceId);

// PubSub item with a device list of either version
class DeviceListItem : public QXmppPubSubBaseItem
{
public:
    // unset if the item contains no device list
    std::optional<Version> version;
    QList<DeviceList::Entry> devices;

protected:
    void parsePayload(const QDomElement &payloadElement) override;
};

// The item ID 'current' is only recommended. Other items are probably outdated, so the last one is
// used if there is no 'current' item. The items must not be empty.
template<typename Item>
const Item &currentItem(const QList<Item> &items)
{
    const auto current = std::ranges::find_if(items, [](const Item &item) { return item.id() == u"current"; });
    return current != items.end() ? *current : items.constLast();
}

using QXmpp::Result;

class Client
{
public:
    Client(QXmppPubSubManager *pubSub, Version version);

    Version version() const { return m_version; }

    // An empty list if the JID has no device list.
    QXmppTask<Result<QList<DeviceList::Entry>>> requestDeviceList(const QString &jid) const;
    QXmppTask<Result<PublicBundle>> requestBundle(const QString &jid, uint32_t deviceId) const;

    // Publish with publish options; if the node exists with another configuration, it is
    // reconfigured and the item published again.
    QXmppTask<Result<Success>> publishDeviceList(const QList<DeviceList::Entry> &devices) const;
    QXmppTask<Result<Success>> publishBundle(uint32_t deviceId, const PublicBundle &bundle) const;
    QXmppTask<Result<Success>> deleteBundle(uint32_t deviceId) const;

private:
    template<typename Item>
    QXmppTask<Result<Success>> publish(const QString &node, const Item &item) const;

    QXmppPubSubManager *m_pubSub;
    Version m_version;
};

}  // namespace QXmpp::Omemo::Private::Pep

#endif  // OMEMOPUBSUB_H

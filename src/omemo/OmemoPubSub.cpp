// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "OmemoPubSub.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppOmemoDeviceList_p.h"
#include "QXmppOmemoItems_p.h"
#include "QXmppPubSubManager.h"
#include "QXmppPubSubNodeConfig.h"
#include "QXmppPubSubPublishOptions.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Omemo0Data.h"
#include "StringLiterals.h"

#include <QDomElement>

using namespace QXmpp::Private;

namespace QXmpp::Omemo::Private::Pep {

// QXmppPubSubManager::Current
constexpr QStringView CURRENT_ITEM = u"current";

QString deviceListNode(Version version)
{
    switch (version) {
    case Version::Omemo0:
        return staticString(ns_omemo_devicelist);
    case Version::Omemo2:
        return staticString(ns_omemo_2_devices);
    }
    Q_UNREACHABLE();
}

QString bundleNode(Version version, uint32_t deviceId)
{
    switch (version) {
    case Version::Omemo0:
        return ns_omemo.toString() + u".bundles:"_s + QString::number(deviceId);
    case Version::Omemo2:
        return staticString(ns_omemo_2_bundles);
    }
    Q_UNREACHABLE();
}

static QString bundleItemId(Version version, uint32_t deviceId)
{
    return version == Version::Omemo2 ? QString::number(deviceId) : staticString(CURRENT_ITEM);
}

static QList<DeviceList::Entry> toEntries(const QXmppOmemoDeviceList &list)
{
    QList<DeviceList::Entry> entries;
    for (const auto &device : list) {
        entries.append({ device.id(), device.label() });
    }
    return entries;
}

static QList<DeviceList::Entry> toEntries(const Omemo0::DeviceList &list)
{
    QList<DeviceList::Entry> entries;
    for (const auto &device : list.devices) {
        entries.append(DeviceList::Entry { device.id, {} });
    }
    return entries;
}

static PublicBundle toPublicBundle(const QXmppOmemoDeviceBundleItem &item)
{
    const auto bundle = item.deviceBundle();
    return {
        bundle.publicIdentityKey(),
        bundle.signedPublicPreKeyId(),
        bundle.signedPublicPreKey(),
        bundle.signedPublicPreKeySignature(),
        bundle.publicPreKeys(),
    };
}

static PublicBundle toPublicBundle(const Omemo0::DeviceBundleItem &item)
{
    const auto &bundle = item.deviceBundle;
    return {
        bundle.publicIdentityKey,
        bundle.signedPublicPreKeyId,
        bundle.signedPublicPreKey,
        bundle.signedPublicPreKeySignature,
        bundle.publicPreKeys,
    };
}

void DeviceListItem::parsePayload(const QDomElement &payloadElement)
{
    if (auto list = parseElement<Omemo0::DeviceList>(payloadElement)) {
        version = Version::Omemo0;
        devices = toEntries(*list);
    } else if (isElement<QXmppOmemoDeviceList>(payloadElement)) {
        version = Version::Omemo2;
        devices = toEntries(*parseElement<QXmppOmemoDeviceList>(payloadElement));
    }
}

static bool isItemNotFound(const QXmppError &error)
{
    const auto stanzaError = error.value<QXmppStanza::Error>();
    return stanzaError && stanzaError->condition() == QXmppStanza::Error::ItemNotFound;
}

// The node exists with a configuration that does not match the publish options (XEP-0060 §7.1.5,
// <conflict/> with <precondition-not-met/>).
static bool isPreconditionNotMet(const QXmppError &error)
{
    const auto stanzaError = error.value<QXmppStanza::Error>();
    return stanzaError && stanzaError->condition() == QXmppStanza::Error::Conflict;
}

static QXmppPubSubPublishOptions publishOptions(bool isBundleNode, Version version)
{
    QXmppPubSubPublishOptions options;
    options.setAccessModel(QXmppPubSubPublishOptions::Open);
    // one item per device
    if (isBundleNode && version == Version::Omemo2) {
        options.setMaxItems(QXmppPubSubNodeConfig::Max());
    }
    return options;
}

static QXmppPubSubNodeConfig nodeConfig(bool isBundleNode, Version version)
{
    QXmppPubSubNodeConfig config;
    config.setAccessModel(QXmppPubSubNodeConfig::Open);
    if (isBundleNode && version == Version::Omemo2) {
        config.setMaxItems(QXmppPubSubNodeConfig::Max());
    }
    return config;
}

Client::Client(QXmppPubSubManager *pubSub, Version version)
    : m_pubSub(pubSub), m_version(version)
{
}

static QXmppTask<Result<QList<DeviceList::Entry>>> requestDeviceListItems(QXmppPubSubManager *pubSub, Version version, QString jid)
{
    auto result = co_await pubSub->requestItems<DeviceListItem>(jid, deviceListNode(version)).withContext(pubSub);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        if (isItemNotFound(*error)) {
            co_return QList<DeviceList::Entry>();
        }
        co_return std::move(*error);
    }

    const auto &items = std::get<QXmppPubSubManager::Items<DeviceListItem>>(result).items;
    if (items.isEmpty()) {
        co_return QList<DeviceList::Entry>();
    }
    const auto &item = currentItem(items);
    co_return item.version == version ? item.devices : QList<DeviceList::Entry>();
}

QXmppTask<Result<QList<DeviceList::Entry>>> Client::requestDeviceList(const QString &jid) const
{
    return requestDeviceListItems(m_pubSub, m_version, jid);
}

// OMEMO 0 does not specify the item ID of a bundle and Dino uses random ones, so all items are
// requested like by Conversations.
template<typename Item>
static QXmppTask<Result<PublicBundle>> requestBundleItems(QXmppPubSubManager *pubSub, QString jid, QString node)
{
    auto result = co_await pubSub->requestItems<Item>(jid, node).withContext(pubSub);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    const auto &items = std::get<QXmppPubSubManager::Items<Item>>(result).items;
    if (items.isEmpty()) {
        co_return QXmppError { u"The bundle node contains no item"_s, {} };
    }
    co_return toPublicBundle(currentItem(items));
}

template<typename Item>
static QXmppTask<Result<PublicBundle>> requestBundleItem(QXmppPubSubManager *pubSub, QString jid, QString node, QString itemId)
{
    auto result = co_await pubSub->requestItem<Item>(jid, node, itemId).withContext(pubSub);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        co_return std::move(*error);
    }
    co_return toPublicBundle(std::get<Item>(result));
}

QXmppTask<Result<PublicBundle>> Client::requestBundle(const QString &jid, uint32_t deviceId) const
{
    const auto node = bundleNode(m_version, deviceId);
    if (m_version == Version::Omemo0) {
        return requestBundleItems<Omemo0::DeviceBundleItem>(m_pubSub, jid, node);
    }
    return requestBundleItem<QXmppOmemoDeviceBundleItem>(m_pubSub, jid, node, bundleItemId(m_version, deviceId));
}

template<typename Item>
static QXmppTask<Result<Success>> publishItem(QXmppPubSubManager *pubSub, QString node, Item item, QXmppPubSubPublishOptions options, QXmppPubSubNodeConfig config)
{
    auto result = co_await pubSub->publishOwnPepItem(node, item, options).withContext(pubSub);
    if (auto *error = std::get_if<QXmppError>(&result)) {
        if (!isPreconditionNotMet(*error)) {
            co_return std::move(*error);
        }

        auto configResult = co_await pubSub->configureOwnPepNode(node, config).withContext(pubSub);
        if (auto *configError = std::get_if<QXmppError>(&configResult)) {
            co_return std::move(*configError);
        }

        result = co_await pubSub->publishOwnPepItem(node, item, options).withContext(pubSub);
        if (auto *error = std::get_if<QXmppError>(&result)) {
            co_return std::move(*error);
        }
    }
    co_return Success();
}

QXmppTask<Result<Success>> Client::publishDeviceList(const QList<DeviceList::Entry> &devices) const
{
    const auto node = deviceListNode(m_version);
    const auto options = publishOptions(false, m_version);
    const auto config = nodeConfig(false, m_version);

    if (m_version == Version::Omemo0) {
        Omemo0::DeviceListItem item;
        item.setId(staticString(CURRENT_ITEM));
        for (const auto &device : devices) {
            item.deviceList.devices.append(Omemo0::DeviceElement { device.id });
        }
        return publishItem(m_pubSub, node, item, options, config);
    }

    QXmppOmemoDeviceList list;
    for (const auto &device : devices) {
        QXmppOmemoDeviceElement element;
        element.setId(device.id);
        element.setLabel(device.label);
        list.append(element);
    }
    QXmppOmemoDeviceListItem item;
    item.setId(staticString(CURRENT_ITEM));
    item.setDeviceList(list);
    return publishItem(m_pubSub, node, item, options, config);
}

QXmppTask<Result<Success>> Client::publishBundle(uint32_t deviceId, const PublicBundle &bundle) const
{
    const auto node = bundleNode(m_version, deviceId);
    const auto itemId = bundleItemId(m_version, deviceId);
    const auto options = publishOptions(true, m_version);
    const auto config = nodeConfig(true, m_version);

    if (m_version == Version::Omemo0) {
        Omemo0::DeviceBundleItem item;
        item.setId(itemId);
        item.deviceBundle = {
            bundle.identityKey,
            bundle.signedPreKey,
            bundle.signedPreKeyId,
            bundle.signedPreKeySignature,
            bundle.preKeys,
        };
        return publishItem(m_pubSub, node, item, options, config);
    }

    QXmppOmemoDeviceBundle deviceBundle;
    deviceBundle.setPublicIdentityKey(bundle.identityKey);
    deviceBundle.setSignedPublicPreKeyId(bundle.signedPreKeyId);
    deviceBundle.setSignedPublicPreKey(bundle.signedPreKey);
    deviceBundle.setSignedPublicPreKeySignature(bundle.signedPreKeySignature);
    for (auto it = bundle.preKeys.cbegin(); it != bundle.preKeys.cend(); ++it) {
        deviceBundle.addPublicPreKey(it.key(), it.value());
    }
    QXmppOmemoDeviceBundleItem item;
    item.setId(itemId);
    item.setDeviceBundle(deviceBundle);
    return publishItem(m_pubSub, node, item, options, config);
}

QXmppTask<Result<Success>> Client::deleteBundle(uint32_t deviceId) const
{
    if (m_version == Version::Omemo0) {
        return m_pubSub->deleteOwnPepNode(bundleNode(m_version, deviceId));
    }
    return m_pubSub->retractOwnPepItem(bundleNode(m_version, deviceId), bundleItemId(m_version, deviceId));
}

}  // namespace QXmpp::Omemo::Private::Pep

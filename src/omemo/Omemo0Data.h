// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef OMEMO0DATA_H
#define OMEMO0DATA_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"
#include "QXmppPubSubBaseItem.h"

#include <optional>

#include <QHash>
#include <QList>

class QDomElement;
class QXmlStreamWriter;

//
// Legacy OMEMO (XEP-0384 v0.3.0, namespace eu.siacs.conversations.axolotl) wire
// format. These types only parse and serialize the XML; the OMEMO 2 equivalents
// live in QXmppOmemoData.cpp and the QXmppOmemo*_p.h headers.
//
namespace QXmpp::Private::Omemo0 {

// Envelope, i.e. a single <key/> element.
struct Envelope {
    static constexpr std::tuple XmlTag = { u"key", ns_omemo };
    static std::optional<Envelope> fromDom(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

    uint32_t recipientDeviceId = 0;
    bool isUsedForKeyExchange = false;
    QByteArray data;
};

// Encrypted message element.
//
// Unlike OMEMO 2 the keys are a flat list (no per-JID grouping), the IV is a
// separate child of the header and the payload is optional (omitted for
// KeyTransportElements).
struct Element {
    static constexpr std::tuple XmlTag = { u"encrypted", ns_omemo };
    static std::optional<Element> fromDom(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

    uint32_t senderDeviceId = 0;
    QByteArray iv;
    std::optional<QByteArray> payload;
    QList<Envelope> envelopes;
};

// Device list entry, i.e. a single <device/> element. Unlike OMEMO 2 it has no
// label.
struct DeviceElement {
    static constexpr std::tuple XmlTag = { u"device", ns_omemo };
    static std::optional<DeviceElement> fromDom(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

    bool operator==(const DeviceElement &other) const = default;

    uint32_t id = 0;
};

// Device list. The element is tagged <list/> (not <devices/> as in OMEMO 2).
struct DeviceList {
    static constexpr std::tuple XmlTag = { u"list", ns_omemo };
    static std::optional<DeviceList> fromDom(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

    QList<DeviceElement> devices;
};

// Device bundle. The child element/attribute names differ from OMEMO 2
// (identityKey/signedPreKeyPublic/signedPreKeySignature/prekeys>preKeyPublic).
struct DeviceBundle {
    static constexpr std::tuple XmlTag = { u"bundle", ns_omemo };
    static std::optional<DeviceBundle> fromDom(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

    QByteArray publicIdentityKey;
    QByteArray signedPublicPreKey;
    uint32_t signedPublicPreKeyId = 0;
    QByteArray signedPublicPreKeySignature;
    QHash<uint32_t, QByteArray> publicPreKeys;
};

//
// PubSub item wrappers. Device bundles are published on node
// 'eu.siacs.conversations.axolotl.bundles:<id>', device lists on node
// 'eu.siacs.conversations.axolotl.devicelist'.
//
class DeviceBundleItem : public QXmppPubSubBaseItem
{
public:
    static bool isItem(const QDomElement &itemElement);

    DeviceBundle deviceBundle;

protected:
    void parsePayload(const QDomElement &payloadElement) override;
    void serializePayload(QXmlStreamWriter *writer) const override;
};

class DeviceListItem : public QXmppPubSubBaseItem
{
public:
    static bool isItem(const QDomElement &itemElement);

    DeviceList deviceList;

protected:
    void parsePayload(const QDomElement &payloadElement) override;
    void serializePayload(QXmlStreamWriter *writer) const override;
};

}  // namespace QXmpp::Private::Omemo0

Q_DECLARE_TYPEINFO(QXmpp::Private::Omemo0::Envelope, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QXmpp::Private::Omemo0::Element, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QXmpp::Private::Omemo0::DeviceElement, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QXmpp::Private::Omemo0::DeviceList, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QXmpp::Private::Omemo0::DeviceBundle, Q_MOVABLE_TYPE);

#endif  // OMEMO0DATA_H

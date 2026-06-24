// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Omemo0Data.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

namespace QXmpp::Private::Omemo0 {

//
// Envelope
//

std::optional<Envelope> Envelope::fromDom(const QDomElement &el)
{
    if (!isElement<Envelope>(el)) {
        return {};
    }

    auto data = parseBase64(el.text());
    if (!data) {
        return {};
    }

    const auto prekey = el.attribute(u"prekey"_s);
    return Envelope {
        .recipientDeviceId = el.attribute(u"rid"_s).toUInt(),
        .isUsedForKeyExchange = prekey == u"true" || prekey == u"1",
        .data = std::move(*data),
    };
}

void Envelope::toXml(QXmlStreamWriter *writer) const
{
    // The namespace is inherited from the parent <header/>, so it is not
    // written here (XmlTag is only used for parsing/registry matching).
    XmlWriter(writer).write(Private::Element {
        u"key",
        Attribute { u"rid", recipientDeviceId },
        OptionalAttribute { u"prekey", DefaultedBool { isUsedForKeyExchange, false } },
        Characters { Base64 { data } },
    });
}

//
// Element
//

std::optional<Element> Element::fromDom(const QDomElement &el)
{
    if (!isElement<Element>(el)) {
        return {};
    }

    const auto header = firstChildElement(el, u"header");

    auto iv = parseBase64(firstChildElement(header, u"iv").text());
    if (!iv) {
        return {};
    }

    Element result {
        .senderDeviceId = header.attribute(u"sid"_s).toUInt(),
        .iv = std::move(*iv),
    };

    for (const auto &keyElement : iterChildElements(header, u"key")) {
        if (auto envelope = Envelope::fromDom(keyElement)) {
            result.envelopes.push_back(std::move(*envelope));
        }
    }

    if (const auto payloadElement = firstChildElement(el, u"payload"); !payloadElement.isNull()) {
        auto payload = parseBase64(payloadElement.text());
        if (!payload) {
            return {};
        }
        result.payload = std::move(*payload);
    }

    return result;
}

void Element::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Private::Element {
        XmlTag,
        Private::Element {
            u"header",
            Attribute { u"sid", senderDeviceId },
            envelopes,
            TextElement { u"iv", Base64 { iv } },
        },
        [&] {
            if (payload) {
                w.write(TextElement { u"payload", Base64 { *payload } });
            }
        },
    });
}

//
// DeviceElement
//

std::optional<DeviceElement> DeviceElement::fromDom(const QDomElement &el)
{
    if (!isElement<DeviceElement>(el)) {
        return {};
    }
    return DeviceElement { .id = el.attribute(u"id"_s).toUInt() };
}

void DeviceElement::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Private::Element {
        u"device",
        Attribute { u"id", id },
    });
}

//
// DeviceList
//

std::optional<DeviceList> DeviceList::fromDom(const QDomElement &el)
{
    if (!isElement<DeviceList>(el)) {
        return {};
    }
    return DeviceList {
        .devices = parseChildElements<QList<DeviceElement>>(el),
    };
}

void DeviceList::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Private::Element { XmlTag, devices });
}

//
// DeviceBundle
//

std::optional<DeviceBundle> DeviceBundle::fromDom(const QDomElement &el)
{
    if (!isElement<DeviceBundle>(el)) {
        return {};
    }

    auto signedPreKeySignature = parseBase64(firstChildElement(el, u"signedPreKeySignature").text());
    auto identityKey = parseBase64(firstChildElement(el, u"identityKey").text());
    if (!signedPreKeySignature || !identityKey) {
        return {};
    }

    DeviceBundle bundle {
        .publicIdentityKey = std::move(*identityKey),
        .signedPublicPreKeySignature = std::move(*signedPreKeySignature),
    };

    if (const auto signedPreKeyElement = firstChildElement(el, u"signedPreKeyPublic"); !signedPreKeyElement.isNull()) {
        auto signedPreKey = parseBase64(signedPreKeyElement.text());
        if (!signedPreKey) {
            return {};
        }
        bundle.signedPublicPreKeyId = signedPreKeyElement.attribute(u"signedPreKeyId"_s).toUInt();
        bundle.signedPublicPreKey = std::move(*signedPreKey);
    }

    if (const auto preKeysElement = firstChildElement(el, u"prekeys"); !preKeysElement.isNull()) {
        for (const auto &preKeyElement : iterChildElements(preKeysElement, u"preKeyPublic")) {
            auto preKey = parseBase64(preKeyElement.text());
            if (!preKey) {
                return {};
            }
            bundle.publicPreKeys.insert(preKeyElement.attribute(u"preKeyId"_s).toUInt(), std::move(*preKey));
        }
    }

    return bundle;
}

void DeviceBundle::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Private::Element {
        XmlTag,
        Private::Element {
            u"signedPreKeyPublic",
            Attribute { u"signedPreKeyId", signedPublicPreKeyId },
            Characters { Base64 { signedPublicPreKey } },
        },
        TextElement { u"signedPreKeySignature", Base64 { signedPublicPreKeySignature } },
        TextElement { u"identityKey", Base64 { publicIdentityKey } },
        Private::Element {
            u"prekeys",
            [&] {
                for (auto it = publicPreKeys.cbegin(); it != publicPreKeys.cend(); ++it) {
                    w.write(Private::Element {
                        u"preKeyPublic",
                        Attribute { u"preKeyId", it.key() },
                        Characters { Base64 { it.value() } },
                    });
                }
            },
        },
    });
}

//
// DeviceBundleItem
//

bool DeviceBundleItem::isItem(const QDomElement &itemElement)
{
    return QXmppPubSubBaseItem::isItem(itemElement, isElement<DeviceBundle>);
}

void DeviceBundleItem::parsePayload(const QDomElement &payloadElement)
{
    if (auto bundle = DeviceBundle::fromDom(payloadElement)) {
        deviceBundle = std::move(*bundle);
    }
}

void DeviceBundleItem::serializePayload(QXmlStreamWriter *writer) const
{
    deviceBundle.toXml(writer);
}

//
// DeviceListItem
//

bool DeviceListItem::isItem(const QDomElement &itemElement)
{
    return QXmppPubSubBaseItem::isItem(itemElement, isElement<DeviceList>);
}

void DeviceListItem::parsePayload(const QDomElement &payloadElement)
{
    if (auto list = DeviceList::fromDom(payloadElement)) {
        deviceList = std::move(*list);
    }
}

void DeviceListItem::serializePayload(QXmlStreamWriter *writer) const
{
    deviceList.toXml(writer);
}

}  // namespace QXmpp::Private::Omemo0

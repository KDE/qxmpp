// SPDX-FileCopyrightText: 2021 Germán Márquez Mejía <mancho@olomono.de>
// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppOmemoDeviceBundle_p.h"
#include "QXmppOmemoDeviceElement_p.h"
#include "QXmppOmemoDeviceList_p.h"
#include "QXmppOmemoElement_p.h"
#include "QXmppOmemoIq_p.h"
#include "QXmppOmemoItems_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QHash>

using namespace QXmpp::Private;

/*!
    \class QXmppOmemoDeviceElement
    \inmodule QXmpp

    \brief The QXmppOmemoDeviceElement class represents an element of the
    OMEMO device list as defined by \xep{0384}{OMEMO Encryption}.
*/

/*! Returns true if the IDs of this element and \a other match. */
bool QXmppOmemoDeviceElement::operator==(const QXmppOmemoDeviceElement &other) const
{
    return m_id == other.id();
}

/*!
    Returns the ID of this device element.

    The ID is used to identify a device and fetch its bundle.
    The ID is 0 if it is unset.

    \sa QXmppOmemoDeviceBundle
*/
uint32_t QXmppOmemoDeviceElement::id() const
{
    return m_id;
}

/*!
    Sets this device element's ID to \a id.

    The ID must be at least 1 and at most
    \c std::numeric_limits<int32_t>::max().
*/
void QXmppOmemoDeviceElement::setId(uint32_t id)
{
    m_id = id;
}

/*!
    Returns the label of this device element.

    The label is a human-readable string used to identify the device by users.
    If no label is set, a default-constructed QString is returned.
*/
QString QXmppOmemoDeviceElement::label() const
{
    return m_label;
}

/*!
    Sets the optional \a label of this device element.

    The label should not contain more than 53 characters.
*/
void QXmppOmemoDeviceElement::setLabel(const QString &label)
{
    m_label = label;
}

void QXmppOmemoDeviceElement::parse(const QDomElement &element)
{
    m_id = element.attribute(u"id"_s).toInt();
    m_label = element.attribute(u"label"_s);
}

void QXmppOmemoDeviceElement::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"device",
        Attribute { u"id", m_id },
        OptionalAttribute { u"label", m_label },
    });
}

/*!
    Determines whether the given DOM \a element is an OMEMO device element.

    Returns true if \a element is an OMEMO device element, otherwise false.
*/
bool QXmppOmemoDeviceElement::isOmemoDeviceElement(const QDomElement &element)
{
    return elementXmlTag(element) == XmlTag;
}

/*!
    \class QXmppOmemoDeviceList
    \inmodule QXmpp

    \brief The QXmppOmemoDeviceList class represents an OMEMO device list
    as defined by \xep{0384}{OMEMO Encryption}.
*/

void QXmppOmemoDeviceList::parse(const QDomElement &element)
{
    *this = parseChildElements<QXmppOmemoDeviceList>(element);
}

void QXmppOmemoDeviceList::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        [&] {
            for (const auto &item : *this) {
                item.toXml(writer);
            }
        },
    });
}

/*!
    Determines whether the given DOM \a element is an OMEMO device list.

    Returns true if \a element is an OMEMO device list, otherwise false.
*/
bool QXmppOmemoDeviceList::isOmemoDeviceList(const QDomElement &element)
{
    return elementXmlTag(element) == XmlTag;
}

/*!
    \class QXmppOmemoDeviceBundle
    \inmodule QXmpp

    \brief The QXmppOmemoDeviceBundle class represents an OMEMO bundle as
    defined by \xep{0384}{OMEMO Encryption}.

    It is a collection of publicly accessible data used by the X3DH key exchange.
    The data is used to build an encrypted session with an OMEMO device.
*/

/*!
    Returns the public identity key.

    The public identity key is the public long-term key which never changes.
*/
QByteArray QXmppOmemoDeviceBundle::publicIdentityKey() const
{
    return m_publicIdentityKey;
}

/*!
    Sets the public identity key to \a key.
*/
void QXmppOmemoDeviceBundle::setPublicIdentityKey(const QByteArray &key)
{
    m_publicIdentityKey = key;
}

/*!
    Returns the public pre key that is signed.
*/
QByteArray QXmppOmemoDeviceBundle::signedPublicPreKey() const
{
    return m_signedPublicPreKey;
}

/*!
    Sets the public pre key that is signed to \a key.
*/
void QXmppOmemoDeviceBundle::setSignedPublicPreKey(const QByteArray &key)
{
    m_signedPublicPreKey = key;
}

/*!
    Returns the ID of the public pre key that is signed.

    The ID is 0 if it is unset.
*/
uint32_t QXmppOmemoDeviceBundle::signedPublicPreKeyId() const
{
    return m_signedPublicPreKeyId;
}

/*!
    Sets the ID of the public pre key that is signed to \a id.

    The ID must be at least 1 and at most
    \c std::numeric_limits<int32_t>::max().
*/
void QXmppOmemoDeviceBundle::setSignedPublicPreKeyId(uint32_t id)
{
    m_signedPublicPreKeyId = id;
}

/*!
    Returns the signature of the public pre key that is signed.
*/
QByteArray QXmppOmemoDeviceBundle::signedPublicPreKeySignature() const
{
    return m_signedPublicPreKeySignature;
}

/*!
    Sets the \a signature of the public pre key that is signed.
*/
void QXmppOmemoDeviceBundle::setSignedPublicPreKeySignature(const QByteArray &signature)
{
    m_signedPublicPreKeySignature = signature;
}

/*!
    Returns the public pre keys.

    The key of a key-value pair represents the ID of the corresponding public
    pre key.
    The value of a key-value pair represents the public pre key.
*/
QHash<uint32_t, QByteArray> QXmppOmemoDeviceBundle::publicPreKeys() const
{
    return m_publicPreKeys;
}

/*!
    Adds a public pre key \a key with ID \a id.

    The ID must be at least 1 and at most
    \c std::numeric_limits<int32_t>::max().
*/
void QXmppOmemoDeviceBundle::addPublicPreKey(uint32_t id, const QByteArray &key)
{
    m_publicPreKeys.insert(id, key);
}

/*!
    Removes the public pre key with ID \a id.
*/
void QXmppOmemoDeviceBundle::removePublicPreKey(uint32_t id)
{
    m_publicPreKeys.remove(id);
}

void QXmppOmemoDeviceBundle::parse(const QDomElement &element)
{
    m_publicIdentityKey = QByteArray::fromBase64(element.firstChildElement(u"ik"_s).text().toLatin1());

    const auto signedPublicPreKeyElement = element.firstChildElement(u"spk"_s);
    if (!signedPublicPreKeyElement.isNull()) {
        m_signedPublicPreKeyId = signedPublicPreKeyElement.attribute(u"id"_s).toInt();
        m_signedPublicPreKey = QByteArray::fromBase64(signedPublicPreKeyElement.text().toLatin1());
    }
    m_signedPublicPreKeySignature = QByteArray::fromBase64(element.firstChildElement(u"spks"_s).text().toLatin1());

    const auto publicPreKeysElement = element.firstChildElement(u"prekeys"_s);
    if (!publicPreKeysElement.isNull()) {
        for (QDomElement publicPreKeyElement = publicPreKeysElement.firstChildElement(u"pk"_s);
             !publicPreKeyElement.isNull();
             publicPreKeyElement = publicPreKeyElement.nextSiblingElement(u"pk"_s)) {
            m_publicPreKeys.insert(publicPreKeyElement.attribute(u"id"_s).toInt(), QByteArray::fromBase64(publicPreKeyElement.text().toLatin1()));
        }
    }
}

void QXmppOmemoDeviceBundle::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);
    w.write(Element {
        { u"bundle", ns_omemo_2 },
        TextElement { u"ik", Base64 { m_publicIdentityKey } },
        Element {
            u"spk",
            Attribute { u"id", m_signedPublicPreKeyId },
            Characters { Base64 { m_signedPublicPreKey } },
        },
        TextElement { u"spks", Base64 { m_signedPublicPreKeySignature } },
        Element {
            u"prekeys",
            [&] {
                for (auto it = m_publicPreKeys.cbegin(); it != m_publicPreKeys.cend(); it++) {
                    w.write(Element {
                        u"pk",
                        Attribute { u"id", it.key() },
                        Characters { Base64 { it.value() } },
                    });
                }
            },
        },
    });
}

/*!
    Determines whether the given DOM \a element is an OMEMO device bundle.

    Returns true if \a element is an OMEMO device bundle, otherwise false.
*/
bool QXmppOmemoDeviceBundle::isOmemoDeviceBundle(const QDomElement &element)
{
    return element.tagName() == u"bundle" && element.namespaceURI() == ns_omemo_2;
}

/*!
    \class QXmppOmemoIq
    \inmodule QXmpp

    \brief The QXmppOmemoIq class represents an encrypted IQ stanza as defined
    by \xep{0384}{OMEMO Encryption} and \xep{0420}{Stanza Content Encryption}
    (SCE).

    \ingroup Stanzas
*/

/*!
    Returns the OMEMO element which contains the data used by OMEMO.
*/
QXmppOmemoElement QXmppOmemoIq::omemoElement()
{
    return m_omemoElement;
}

/*!
    Sets the OMEMO element \a omemoElement which contains the data used by
    OMEMO.
*/
void QXmppOmemoIq::setOmemoElement(const QXmppOmemoElement &omemoElement)
{
    m_omemoElement = omemoElement;
}

void QXmppOmemoIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement child = element.firstChildElement();
    m_omemoElement.parse(child);
}

void QXmppOmemoIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    m_omemoElement.toXml(writer);
}

/*!
    Determines whether the given DOM \a element is an OMEMO IQ stanza.

    Returns true if \a element is an OMEMO IQ stanza, otherwise false.
*/
bool QXmppOmemoIq::isOmemoIq(const QDomElement &element)
{
    auto child = element.firstChildElement();
    return !child.isNull() && QXmppOmemoElement::isOmemoElement(child);
}

QXmppOmemoDeviceBundle QXmppOmemoDeviceBundleItem::deviceBundle() const
{
    return m_deviceBundle;
}

void QXmppOmemoDeviceBundleItem::setDeviceBundle(const QXmppOmemoDeviceBundle &deviceBundle)
{
    m_deviceBundle = deviceBundle;
}

bool QXmppOmemoDeviceBundleItem::isItem(const QDomElement &itemElement)
{
    return QXmppPubSubBaseItem::isItem(itemElement, QXmppOmemoDeviceBundle::isOmemoDeviceBundle);
}

void QXmppOmemoDeviceBundleItem::parsePayload(const QDomElement &payloadElement)
{
    m_deviceBundle.parse(payloadElement);
}

void QXmppOmemoDeviceBundleItem::serializePayload(QXmlStreamWriter *writer) const
{
    m_deviceBundle.toXml(writer);
}

QXmppOmemoDeviceList QXmppOmemoDeviceListItem::deviceList() const
{
    return m_deviceList;
}

void QXmppOmemoDeviceListItem::setDeviceList(const QXmppOmemoDeviceList &deviceList)
{
    m_deviceList = deviceList;
}

bool QXmppOmemoDeviceListItem::isItem(const QDomElement &itemElement)
{
    return QXmppPubSubBaseItem::isItem(itemElement, QXmppOmemoDeviceList::isOmemoDeviceList);
}

void QXmppOmemoDeviceListItem::parsePayload(const QDomElement &payloadElement)
{
    m_deviceList.parse(payloadElement);
}

void QXmppOmemoDeviceListItem::serializePayload(QXmlStreamWriter *writer) const
{
    m_deviceList.toXml(writer);
}

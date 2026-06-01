// SPDX-FileCopyrightText: 2021 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppTrustMessages.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

/*!
    \class QXmppTrustMessageElement
    \inmodule QXmpp

    \brief The QXmppTrustMessageElement class represents a trust message element
    as defined by \xep{0434}{Trust Messages (TM)}.

    \since QXmpp 1.5
*/

class QXmppTrustMessageElementPrivate : public QSharedData
{
public:
    QString usage;
    QString encryption;
    QList<QXmppTrustMessageKeyOwner> keyOwners;
};

/*! Constructs a trust message element. */
QXmppTrustMessageElement::QXmppTrustMessageElement()
    : d(new QXmppTrustMessageElementPrivate)
{
}

/*! Copy-constructor. */
QXmppTrustMessageElement::QXmppTrustMessageElement(const QXmppTrustMessageElement &other) = default;
/*! Move-constructor. */
QXmppTrustMessageElement::QXmppTrustMessageElement(QXmppTrustMessageElement &&) = default;
QXmppTrustMessageElement::~QXmppTrustMessageElement() = default;
/*! Assignment operator. */
QXmppTrustMessageElement &QXmppTrustMessageElement::operator=(const QXmppTrustMessageElement &other) = default;
/*! Move-assignment operator. */
QXmppTrustMessageElement &QXmppTrustMessageElement::operator=(QXmppTrustMessageElement &&) = default;

/*!
    Returns the namespace of the trust management protocol.
*/
QString QXmppTrustMessageElement::usage() const
{
    return d->usage;
}

/*!
    Sets the trust management protocol namespace via \a usage.
*/
void QXmppTrustMessageElement::setUsage(const QString &usage)
{
    d->usage = usage;
}

/*!
    Returns the namespace of the keys' encryption protocol.
*/
QString QXmppTrustMessageElement::encryption() const
{
    return d->encryption;
}

/*!
    Sets the namespace of the keys' \a encryption protocol.
*/
void QXmppTrustMessageElement::setEncryption(const QString &encryption)
{
    d->encryption = encryption;
}

/*!
    Returns the key owners containing the corresponding information for
    trusting or distrusting their keys.
*/
QList<QXmppTrustMessageKeyOwner> QXmppTrustMessageElement::keyOwners() const
{
    return d->keyOwners;
}

/*!
    Sets the \a keyOwners containing the corresponding information for trusting
    or distrusting their keys.
*/
void QXmppTrustMessageElement::setKeyOwners(const QList<QXmppTrustMessageKeyOwner> &keyOwners)
{
    d->keyOwners = keyOwners;
}

/*!
    Adds a \a keyOwner containing the corresponding information for trusting or
    distrusting the owner's keys.
*/
void QXmppTrustMessageElement::addKeyOwner(const QXmppTrustMessageKeyOwner &keyOwner)
{
    d->keyOwners.append(keyOwner);
}

void QXmppTrustMessageElement::parse(const QDomElement &element)
{
    d->usage = element.attribute(u"usage"_s);
    d->encryption = element.attribute(u"encryption"_s);
    d->keyOwners = parseChildElements<QList<QXmppTrustMessageKeyOwner>>(element);
}

void QXmppTrustMessageElement::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        Attribute { u"usage", d->usage },
        Attribute { u"encryption", d->encryption },
        d->keyOwners,
    });
}

/*!
    Determines whether the given DOM \a element is a trust message element.

    Returns true if \a element is a trust message element, otherwise false.
*/
bool QXmppTrustMessageElement::isTrustMessageElement(const QDomElement &element)
{
    return element.tagName() == u"trust-message" &&
        element.namespaceURI() == ns_tm;
}

/*!
    \class QXmppTrustMessageKeyOwner
    \inmodule QXmpp

    \brief The QXmppTrustMessageKeyOwner class represents a key owner of the
    trust message as defined by \xep{0434}{Trust Messages (TM)}.

    \since QXmpp 1.5
*/

class QXmppTrustMessageKeyOwnerPrivate : public QSharedData
{
public:
    QString jid;
    QList<QByteArray> trustedKeys;
    QList<QByteArray> distrustedKeys;
};

/*! Constructs a trust message key owner. */
QXmppTrustMessageKeyOwner::QXmppTrustMessageKeyOwner()
    : d(new QXmppTrustMessageKeyOwnerPrivate)
{
}

/*! Copy constructor. */
QXmppTrustMessageKeyOwner::QXmppTrustMessageKeyOwner(const QXmppTrustMessageKeyOwner &other) = default;
/*! Copy constructor. */
QXmppTrustMessageKeyOwner::QXmppTrustMessageKeyOwner(QXmppTrustMessageKeyOwner &&) = default;
QXmppTrustMessageKeyOwner::~QXmppTrustMessageKeyOwner() = default;
/*! Assignment operator. */
QXmppTrustMessageKeyOwner &QXmppTrustMessageKeyOwner::operator=(const QXmppTrustMessageKeyOwner &other) = default;
/*! Assignment operator. */
QXmppTrustMessageKeyOwner &QXmppTrustMessageKeyOwner::operator=(QXmppTrustMessageKeyOwner &&) = default;

/*!
    Returns the bare JID of the key owner.
*/
QString QXmppTrustMessageKeyOwner::jid() const
{
    return d->jid;
}

/*!
    Sets the bare \a jid of the key owner.

    If a full JID is passed, it is converted into a bare JID.
*/
void QXmppTrustMessageKeyOwner::setJid(const QString &jid)
{
    d->jid = QXmppUtils::jidToBareJid(jid);
}

/*!
    Returns the IDs of the keys that are trusted.
*/
QList<QByteArray> QXmppTrustMessageKeyOwner::trustedKeys() const
{
    return d->trustedKeys;
}

/*!
    Sets the \a keyIds of keys that are trusted.
*/
void QXmppTrustMessageKeyOwner::setTrustedKeys(const QList<QByteArray> &keyIds)
{
    d->trustedKeys = keyIds;
}

/*!
    Returns the IDs of the keys that are distrusted.
*/
QList<QByteArray> QXmppTrustMessageKeyOwner::distrustedKeys() const
{
    return d->distrustedKeys;
}

/*!
    Sets the \a keyIds of keys that are distrusted.
*/
void QXmppTrustMessageKeyOwner::setDistrustedKeys(const QList<QByteArray> &keyIds)
{
    d->distrustedKeys = keyIds;
}

void QXmppTrustMessageKeyOwner::parse(const QDomElement &element)
{
    d->jid = element.attribute(u"jid"_s);

    for (const auto &childElement : iterChildElements(element, u"trust")) {
        d->trustedKeys.append(QByteArray::fromBase64(childElement.text().toLatin1()));
    }
    for (const auto &childElement : iterChildElements(element, u"distrust")) {
        d->distrustedKeys.append(QByteArray::fromBase64(childElement.text().toLatin1()));
    }
}

void QXmppTrustMessageKeyOwner::toXml(QXmlStreamWriter *writer) const
{
    using std::views::transform;

    XmlWriter(writer).write(Element {
        u"key-owner",
        Attribute { u"jid", d->jid },
        TextElements { u"trust", transform(d->trustedKeys, &Base64::fromByteArray) },
        TextElements { u"distrust", transform(d->distrustedKeys, &Base64::fromByteArray) },
    });
}

/*!
    Determines whether the given DOM \a element is a trust message key owner.

    Returns true if \a element is a trust message key owner, otherwise false.
*/
bool QXmppTrustMessageKeyOwner::isTrustMessageKeyOwner(const QDomElement &element)
{
    return element.tagName() == u"key-owner" &&
        element.namespaceURI() == ns_tm;
}

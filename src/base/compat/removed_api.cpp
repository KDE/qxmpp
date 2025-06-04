// SPDX-FileCopyrightText: 2011 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppArchiveIq.h"
#include "QXmppBindIq.h"
#include "QXmppBitsOfBinaryIq.h"
#include "QXmppByteStreamIq.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppElement.h"
#include "QXmppEntityTimeIq.h"
#include "QXmppExternalServiceDiscoveryIq.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppIbbIq.h"
#include "QXmppMamIq.h"
#include "QXmppNonSASLAuth.h"
#include "QXmppPingIq.h"
#include "QXmppPubSubIq.h"
#include "QXmppPubSubItem.h"
#include "QXmppRegisterIq.h"
#include "QXmppRosterIq.h"
#include "QXmppSessionIq.h"
#include "QXmppStartTlsPacket.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"
#include "QXmppVersionIq.h"

#include "StringLiterals.h"

#include <QDomElement>
#include <QSharedData>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

template<typename Enum, std::size_t N>
std::optional<Enum> enumFromString(const std::array<QStringView, N> &values, QStringView str)
{
    if (auto itr = std::ranges::find(values, str); itr != values.end()) {
        return Enum(std::distance(values.begin(), itr));
    }
    return {};
}

static void writeOptionalXmlAttribute(QXmlStreamWriter *stream, QStringView name, QStringView value)
{
    if (!value.isEmpty()) {
        stream->writeAttribute(name.toString(), value.toString());
    }
}

static bool isIqType(const QDomElement &element, QStringView tagName, QStringView xmlns)
{
    // IQs must have only one child element, so we do not need to iterate over the child elements.
    auto child = element.firstChildElement();
    return child.tagName() == tagName && child.namespaceURI() == xmlns;
}

/// \cond

// ArchiveIq

bool QXmppArchiveChatIq::isArchiveChatIq(const QDomElement &element)
{
    auto chatEl = firstChildElement(element, u"chat", ns_archive);
    return !chatEl.isNull() && !chatEl.attribute(u"with"_s).isEmpty();
}

bool QXmppArchiveListIq::isArchiveListIq(const QDomElement &element)
{
    return isIqType(element, u"list", ns_archive);
}

bool QXmppArchiveRemoveIq::isArchiveRemoveIq(const QDomElement &element)
{
    return isIqType(element, u"remove", ns_archive);
}

bool QXmppArchiveRetrieveIq::isArchiveRetrieveIq(const QDomElement &element)
{
    return isIqType(element, u"retrieve", ns_archive);
}

bool QXmppArchivePrefIq::isArchivePrefIq(const QDomElement &element)
{
    return isIqType(element, u"pref", ns_archive);
}

// BindIq

bool QXmppBindIq::isBindIq(const QDomElement &element)
{
    return isIqType(element, u"bind", ns_bind);
}

// Bob

bool QXmppBitsOfBinaryIq::isBitsOfBinaryIq(const QDomElement &element)
{
    return isIqType(element, u"data", ns_bob);
}

// ByteStreamIq

bool QXmppByteStreamIq::isByteStreamIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_bytestreams);
}

// DiscoveryIq

bool QXmppDiscoveryIq::isDiscoveryIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_disco_info) || isIqType(element, u"query", ns_disco_items);
}

// EntityTimeIq

bool QXmppEntityTimeIq::isEntityTimeIq(const QDomElement &element)
{
    return isIqType(element, u"time", ns_entity_time);
}

bool QXmppEntityTimeIq::checkIqType(const QString &tagName, const QString &xmlns)
{
    return tagName == u"time" && xmlns == ns_entity_time;
}

// ExternalServiceDiscoveryIq

bool QXmppExternalServiceDiscoveryIq::isExternalServiceDiscoveryIq(const QDomElement &element)
{
    return isIqType(element, u"services", ns_external_service_discovery);
}

// HttpUploadIq

bool QXmppHttpUploadRequestIq::isHttpUploadRequestIq(const QDomElement &element)
{
    return isIqType(element, u"request", ns_http_upload);
}

bool QXmppHttpUploadSlotIq::isHttpUploadSlotIq(const QDomElement &element)
{
    return isIqType(element, u"slot", ns_http_upload);
}

// IbbIq

bool QXmppIbbDataIq::isIbbDataIq(const QDomElement &element)
{
    return isIqType(element, u"data", ns_ibb);
}

bool QXmppIbbOpenIq::isIbbOpenIq(const QDomElement &element)
{
    return isIqType(element, u"open", ns_ibb);
}

bool QXmppIbbCloseIq::isIbbCloseIq(const QDomElement &element)
{
    return isIqType(element, u"close", ns_ibb);
}

// MamIq

bool QXmppMamQueryIq::isMamQueryIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_mam);
}

bool QXmppMamResultIq::isMamResultIq(const QDomElement &element)
{
    if (element.tagName() == u"iq") {
        QDomElement finElement = element.firstChildElement(u"fin"_s);
        if (!finElement.isNull() && finElement.namespaceURI() == ns_mam) {
            return true;
        }
    }
    return false;
}

// NonSaslIq

bool QXmppNonSASLAuthIq::isNonSASLAuthIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_auth);
}

// PingIq

bool QXmppPingIq::isPingIq(const QDomElement &element)
{
    return isIqType(element, u"ping", ns_ping) && element.attribute(u"type"_s) == u"get";
}

// RegisterIq

bool QXmppRegisterIq::isRegisterIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_register);
}

// RosterIq

bool QXmppRosterIq::isRosterIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_roster);
}

// VCardIq

bool QXmppVCardIq::isVCard(const QDomElement &el)
{
    return isIqType(el, u"vCard", ns_vcard);
}

bool QXmppVCardIq::checkIqType(const QString &tagName, const QString &xmlNamespace)
{
    return tagName == u"vCard" && xmlNamespace == ns_vcard;
}

// VersionIq

bool QXmppVersionIq::isVersionIq(const QDomElement &element)
{
    return isIqType(element, u"query", ns_version);
}

bool QXmppVersionIq::checkIqType(const QString &tagName, const QString &xmlNamespace)
{
    return tagName == u"query" && xmlNamespace == ns_version;
}

// SessionIq

bool QXmppSessionIq::isSessionIq(const QDomElement &element)
{
    return isIqType(element, u"session", ns_session);
}

void QXmppSessionIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(u"session"_s);
    writer->writeDefaultNamespace(ns_session.toString());
    writer->writeEndElement();
}

/// \endcond

// PubSubIq

/// \cond
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

static const QStringList PUBSUB_QUERIES = {
    u"affiliations"_s,
    u"default"_s,
    u"items"_s,
    u"publish"_s,
    u"retract"_s,
    u"subscribe"_s,
    u"subscription"_s,
    u"subscriptions"_s,
    u"unsubscribe"_s,
};

class QXmppPubSubIqPrivate : public QSharedData
{
public:
    QXmppPubSubIqPrivate();

    QXmppPubSubIq::QueryType queryType;
    QString queryJid;
    QString queryNode;
    QList<QXmppPubSubItem> items;
    QString subscriptionId;
    QString subscriptionType;
};

QXmppPubSubIqPrivate::QXmppPubSubIqPrivate()
    : queryType(QXmppPubSubIq::ItemsQuery)
{
}

QXmppPubSubIq::QXmppPubSubIq()
    : d(new QXmppPubSubIqPrivate)
{
}

QXmppPubSubIq::QXmppPubSubIq(const QXmppPubSubIq &iq) = default;

QXmppPubSubIq::~QXmppPubSubIq() = default;

QXmppPubSubIq &QXmppPubSubIq::operator=(const QXmppPubSubIq &iq) = default;

/// Returns the PubSub queryType for this IQ.

QXmppPubSubIq::QueryType QXmppPubSubIq::queryType() const
{
    return d->queryType;
}

/// Sets the PubSub queryType for this IQ.
///
/// \param queryType

void QXmppPubSubIq::setQueryType(QXmppPubSubIq::QueryType queryType)
{
    d->queryType = queryType;
}

/// Returns the JID being queried.

QString QXmppPubSubIq::queryJid() const
{
    return d->queryJid;
}

/// Sets the JID being queried.
///
/// \param queryJid

void QXmppPubSubIq::setQueryJid(const QString &queryJid)
{
    d->queryJid = queryJid;
}

/// Returns the node being queried.

QString QXmppPubSubIq::queryNode() const
{
    return d->queryNode;
}

/// Sets the node being queried.
///
/// \param queryNode

void QXmppPubSubIq::setQueryNode(const QString &queryNode)
{
    d->queryNode = queryNode;
}

/// Returns the subscription ID.

QString QXmppPubSubIq::subscriptionId() const
{
    return d->subscriptionId;
}

/// Sets the subscription ID.
///
/// \param subscriptionId

void QXmppPubSubIq::setSubscriptionId(const QString &subscriptionId)
{
    d->subscriptionId = subscriptionId;
}

/// Returns the IQ's items.

QList<QXmppPubSubItem> QXmppPubSubIq::items() const
{
    return d->items;
}

/// Sets the IQ's items.
///
/// \param items

void QXmppPubSubIq::setItems(const QList<QXmppPubSubItem> &items)
{
    d->items = items;
}

bool QXmppPubSubIq::isPubSubIq(const QDomElement &element)
{
    return element.firstChildElement(u"pubsub"_s).namespaceURI() == ns_pubsub;
}

void QXmppPubSubIq::parseElementFromChild(const QDomElement &element)
{
    const QDomElement pubSubElement = element.firstChildElement(u"pubsub"_s);

    const QDomElement queryElement = pubSubElement.firstChildElement();

    // determine query type
    const QString tagName = queryElement.tagName();
    int queryType = PUBSUB_QUERIES.indexOf(queryElement.tagName());
    if (queryType > -1) {
        d->queryType = QueryType(queryType);
    }

    d->queryJid = queryElement.attribute(u"jid"_s);
    d->queryNode = queryElement.attribute(u"node"_s);

    // parse contents
    QDomElement childElement;
    switch (d->queryType) {
    case QXmppPubSubIq::ItemsQuery:
    case QXmppPubSubIq::PublishQuery:
    case QXmppPubSubIq::RetractQuery:
        for (const auto &childElement : iterChildElements(queryElement, u"item")) {
            QXmppPubSubItem item;
            item.parse(childElement);
            d->items << item;
        }
        break;
    case QXmppPubSubIq::SubscriptionQuery:
        d->subscriptionId = queryElement.attribute(u"subid"_s);
        d->subscriptionType = queryElement.attribute(u"subscription"_s);
        break;
    default:
        break;
    }
}

void QXmppPubSubIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(u"pubsub"_s);
    writer->writeDefaultNamespace(ns_pubsub.toString());

    // write query type
    writer->writeStartElement(PUBSUB_QUERIES.at(d->queryType));
    writeOptionalXmlAttribute(writer, u"jid", d->queryJid);
    writeOptionalXmlAttribute(writer, u"node", d->queryNode);

    // write contents
    switch (d->queryType) {
    case QXmppPubSubIq::ItemsQuery:
    case QXmppPubSubIq::PublishQuery:
    case QXmppPubSubIq::RetractQuery:
        for (const auto &item : d->items) {
            item.toXml(writer);
        }
        break;
    case QXmppPubSubIq::SubscriptionQuery:
        writeOptionalXmlAttribute(writer, u"subid", d->subscriptionId);
        writeOptionalXmlAttribute(writer, u"subscription", d->subscriptionType);
        break;
    default:
        break;
    }
    writer->writeEndElement();
    writer->writeEndElement();
}
/// \endcond

// PubSubItem

/// \cond
class QXmppPubSubItemPrivate : public QSharedData
{
public:
    QString id;
    QXmppElement contents;
};

QXmppPubSubItem::QXmppPubSubItem()
    : d(new QXmppPubSubItemPrivate)
{
}

QXmppPubSubItem::QXmppPubSubItem(const QXmppPubSubItem &iq) = default;

QXmppPubSubItem::~QXmppPubSubItem() = default;

QXmppPubSubItem &QXmppPubSubItem::operator=(const QXmppPubSubItem &iq) = default;

/// Returns the ID of the PubSub item.

QString QXmppPubSubItem::id() const
{
    return d->id;
}

/// Sets the ID of the PubSub item.
///
/// \param id

void QXmppPubSubItem::setId(const QString &id)
{
    d->id = id;
}

/// Returns the contents of the PubSub item.

QXmppElement QXmppPubSubItem::contents() const
{
    return d->contents;
}

/// Sets the contents of the PubSub item.
///
/// \param contents

void QXmppPubSubItem::setContents(const QXmppElement &contents)
{
    d->contents = contents;
}

void QXmppPubSubItem::parse(const QDomElement &element)
{
    d->id = element.attribute(u"id"_s);
    d->contents = QXmppElement(element.firstChildElement());
}

void QXmppPubSubItem::toXml(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(u"item"_s);
    writeOptionalXmlAttribute(writer, u"id", d->id);
    d->contents.toXml(writer);
    writer->writeEndElement();
}
/// \endcond

// StarttlsPacket

constexpr std::array<QStringView, 3> STARTTLS_TYPES = {
    u"starttls",
    u"proceed",
    u"failure",
};

///
/// Constructs a new QXmppStartTlsPacket
///
/// \param type The type of the new QXmppStartTlsPacket.
///
QXmppStartTlsPacket::QXmppStartTlsPacket(Type type)
    : m_type(type)
{
}

QXmppStartTlsPacket::~QXmppStartTlsPacket() = default;

/// Returns the type of the STARTTLS packet
QXmppStartTlsPacket::Type QXmppStartTlsPacket::type() const
{
    return m_type;
}

/// Sets the type of the STARTTLS packet
void QXmppStartTlsPacket::setType(QXmppStartTlsPacket::Type type)
{
    m_type = type;
}

/// \cond
void QXmppStartTlsPacket::parse(const QDomElement &element)
{
    if (!QXmppStartTlsPacket::isStartTlsPacket(element)) {
        return;
    }

    m_type = enumFromString<Type>(STARTTLS_TYPES, element.tagName()).value_or(Invalid);
}

void QXmppStartTlsPacket::toXml(QXmlStreamWriter *writer) const
{
    if (m_type != Invalid) {
        writer->writeStartElement(STARTTLS_TYPES.at(size_t(m_type)).toString());
        writer->writeDefaultNamespace(ns_tls.toString());
        writer->writeEndElement();
    }
}
/// \endcond

///
/// Checks whether the given \p element is a STARTTLS packet according to
/// <a href="https://xmpp.org/rfcs/rfc6120.html#tls-process-initiate">RFC6120</a>.
///
/// \param element The element that should be checked for being a STARTTLS packet.
///
/// \returns True, if the element is a STARTTLS packet.
///
bool QXmppStartTlsPacket::isStartTlsPacket(const QDomElement &element)
{
    return element.namespaceURI() == ns_tls &&
        enumFromString<Type>(STARTTLS_TYPES, element.tagName()).has_value();
}

///
/// Checks whether the given \p element is a STARTTLS packet according to
/// <a href="https://xmpp.org/rfcs/rfc6120.html#tls-process-initiate">RFC6120</a>
/// and has the correct type.
///
/// \param element The element that should be checked for being a STARTTLS packet.
/// \param type The type the element needs to have.
///
/// \returns True, if the element is a STARTTLS packet and has the correct type.
///
bool QXmppStartTlsPacket::isStartTlsPacket(const QDomElement &element, Type type)
{
    return element.namespaceURI() == ns_tls && element.tagName() == STARTTLS_TYPES.at(size_t(type));
}

QT_WARNING_POP

// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppDiscoveryIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QCryptographicHash>
#include <QDomElement>
#include <QSharedData>

using namespace QXmpp::Private;

static bool identityLessThan(const QXmppDiscoveryIq::Identity &i1, const QXmppDiscoveryIq::Identity &i2)
{
    if (i1.category() < i2.category()) {
        return true;
    } else if (i1.category() > i2.category()) {
        return false;
    }

    if (i1.type() < i2.type()) {
        return true;
    } else if (i1.type() > i2.type()) {
        return false;
    }

    if (i1.language() < i2.language()) {
        return true;
    } else if (i1.language() > i2.language()) {
        return false;
    }

    if (i1.name() < i2.name()) {
        return true;
    } else if (i1.name() > i2.name()) {
        return false;
    }

    return false;
}

class QXmppDiscoveryIdentityPrivate : public QSharedData
{
public:
    QString category;
    QString language;
    QString name;
    QString type;
};

///
/// \class QXmppDiscoveryIq::Identity
///
/// \brief Identity represents one of possibly multiple identities of an
/// XMPP entity obtained from a service discovery request as defined in
/// \xep{0030, Service Discovery}.
///

QXmppDiscoveryIq::Identity::Identity()
    : d(new QXmppDiscoveryIdentityPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX_INNER(QXmppDiscoveryIq, Identity)

///
/// Returns the category (e.g. "account", "client", "conference", etc.) of the
/// identity.
///
/// See https://xmpp.org/registrar/disco-categories.html for more details.
///
QString QXmppDiscoveryIq::Identity::category() const
{
    return d->category;
}

///
/// Sets the category (e.g. "account", "client", "conference", etc.) of the
/// identity.
///
/// See https://xmpp.org/registrar/disco-categories.html for more details.
///
void QXmppDiscoveryIq::Identity::setCategory(const QString &category)
{
    d->category = category;
}

///
/// Returns the language code of the identity.
///
/// It is possible that the same identity (same type and same category) is
/// included multiple times with different languages and localized names.
///
QString QXmppDiscoveryIq::Identity::language() const
{
    return d->language;
}

///
/// Sets the language code of the identity.
///
/// It is possible that the same identity (same type and same category) is
/// included multiple times with different languages and localized names.
///
void QXmppDiscoveryIq::Identity::setLanguage(const QString &language)
{
    d->language = language;
}

///
/// Returns the human-readable name of the service.
///
QString QXmppDiscoveryIq::Identity::name() const
{
    return d->name;
}

///
/// Sets the human-readable name of the service.
///
void QXmppDiscoveryIq::Identity::setName(const QString &name)
{
    d->name = name;
}

///
/// Returns the service type in this category.
///
/// See https://xmpp.org/registrar/disco-categories.html for details.
///
QString QXmppDiscoveryIq::Identity::type() const
{
    return d->type;
}

///
/// Sets the service type in this category.
///
/// See https://xmpp.org/registrar/disco-categories.html for details.
///
void QXmppDiscoveryIq::Identity::setType(const QString &type)
{
    d->type = type;
}

/// \cond
std::optional<QXmppDiscoveryIq::Identity> QXmppDiscoveryIq::Identity::fromDom(const QDomElement &el)
{
    QXmppDiscoveryIq::Identity identity;
    identity.setLanguage(el.attributeNS(ns_xml.toString(), u"lang"_s));
    identity.setCategory(el.attribute(u"category"_s));
    identity.setName(el.attribute(u"name"_s));
    identity.setType(el.attribute(u"type"_s));
    return identity;
}

void QXmppDiscoveryIq::Identity::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"identity",
        OptionalAttribute { u"xml:lang", d->language },
        Attribute { u"category", d->category },
        OptionalAttribute { u"name", d->name },
        Attribute { u"type", d->type },
    });
}
/// \endcond

class QXmppDiscoveryItemPrivate : public QSharedData
{
public:
    QString jid;
    QString name;
    QString node;
};

///
/// \class QXmppDiscoveryIq::Item
///
/// Item represents a related XMPP entity that can be queried using \xep{0030,
/// Service Discovery}.
///

QXmppDiscoveryIq::Item::Item()
    : d(new QXmppDiscoveryItemPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX_INNER(QXmppDiscoveryIq, Item)

///
/// Returns the jid of the item.
///
QString QXmppDiscoveryIq::Item::jid() const
{
    return d->jid;
}

///
/// Sets the jid of the item.
///
void QXmppDiscoveryIq::Item::setJid(const QString &jid)
{
    d->jid = jid;
}

///
/// Returns the items human-readable name.
///
QString QXmppDiscoveryIq::Item::name() const
{
    return d->name;
}

///
/// Sets the items human-readable name.
///
void QXmppDiscoveryIq::Item::setName(const QString &name)
{
    d->name = name;
}

///
/// Returns a special service discovery node.
///
QString QXmppDiscoveryIq::Item::node() const
{
    return d->node;
}

///
/// Sets a special service discovery node.
///
void QXmppDiscoveryIq::Item::setNode(const QString &node)
{
    d->node = node;
}

/// \cond
std::optional<QXmppDiscoveryIq::Item> QXmppDiscoveryIq::Item::fromDom(const QDomElement &el)
{
    QXmppDiscoveryIq::Item item;
    item.setJid(el.attribute(u"jid"_s));
    item.setName(el.attribute(u"name"_s));
    item.setNode(el.attribute(u"node"_s));
    return item;
}

void QXmppDiscoveryIq::Item::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"item",
        Attribute { u"jid", d->jid },
        OptionalAttribute { u"name", d->name },
        OptionalAttribute { u"node", d->node },
    });
}
/// \endcond

class QXmppDiscoveryIqPrivate : public QSharedData
{
public:
    QStringList features;
    QList<QXmppDiscoveryIq::Identity> identities;
    QList<QXmppDiscoveryIq::Item> items;
    QXmppDataForm form;
    QString queryNode;
    QXmppDiscoveryIq::QueryType queryType;
};

///
/// \class QXmppDiscoveryIq
///
/// QXmppDiscoveryIq represents a discovery IQ request or result containing a
/// list of features and other information about an entity as defined by
/// \xep{0030, Service Discovery}.
///
/// \ingroup Stanzas
///

///
/// \enum QXmppDiscoveryIq::QueryType
///
/// Specifies the type of a service discovery query. An InfoQuery queries
/// identities and features, an ItemsQuery queries subservices in the form of
/// items.
///

QXmppDiscoveryIq::QXmppDiscoveryIq()
    : d(new QXmppDiscoveryIqPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppDiscoveryIq)

///
/// Returns the features of the service.
///
QStringList QXmppDiscoveryIq::features() const
{
    return d->features;
}

///
/// Sets the features of the service.
///
void QXmppDiscoveryIq::setFeatures(const QStringList &features)
{
    d->features = features;
}

///
/// Returns the list of identities for this service.
///
QList<QXmppDiscoveryIq::Identity> QXmppDiscoveryIq::identities() const
{
    return d->identities;
}

///
/// Sets the list of identities for this service.
///
void QXmppDiscoveryIq::setIdentities(const QList<QXmppDiscoveryIq::Identity> &identities)
{
    d->identities = identities;
}

///
/// Returns the list of service discovery items.
///
QList<QXmppDiscoveryIq::Item> QXmppDiscoveryIq::items() const
{
    return d->items;
}

///
/// Sets the list of service discovery items.
///
void QXmppDiscoveryIq::setItems(const QList<QXmppDiscoveryIq::Item> &items)
{
    d->items = items;
}

///
/// Returns the QXmppDataForm for this IQ, as defined by \xep{0128, Service
/// Discovery Extensions}.
///
QXmppDataForm QXmppDiscoveryIq::form() const
{
    return d->form;
}

///
/// Sets the QXmppDataForm for this IQ, as define by \xep{0128, Service
/// Discovery Extensions}.
///
/// \param form
///
void QXmppDiscoveryIq::setForm(const QXmppDataForm &form)
{
    d->form = form;
}

///
/// Returns the special node to query.
///
QString QXmppDiscoveryIq::queryNode() const
{
    return d->queryNode;
}

///
/// Sets the special node to query.
///
void QXmppDiscoveryIq::setQueryNode(const QString &node)
{
    d->queryNode = node;
}

///
/// Returns the query type (info query or items query).
///
QXmppDiscoveryIq::QueryType QXmppDiscoveryIq::queryType() const
{
    return d->queryType;
}

///
/// Sets the query type (info query or items query).
///
void QXmppDiscoveryIq::setQueryType(enum QXmppDiscoveryIq::QueryType type)
{
    d->queryType = type;
}

///
/// Calculate the verification string for \xep{0115, Entity Capabilities}.
///
QByteArray QXmppDiscoveryIq::verificationString() const
{
    QString S;
    QList<QXmppDiscoveryIq::Identity> sortedIdentities = d->identities;
    std::sort(sortedIdentities.begin(), sortedIdentities.end(), identityLessThan);
    QStringList sortedFeatures = d->features;
    std::sort(sortedFeatures.begin(), sortedFeatures.end());
    sortedFeatures.removeDuplicates();
    for (const auto &identity : sortedIdentities) {
        S += identity.category() + u'/' + identity.type() + u'/' + identity.language() + u'/' + identity.name() + u'<';
    }
    for (const auto &feature : sortedFeatures) {
        S += feature + u'<';
    }

    if (!d->form.isNull()) {
        QMap<QString, QXmppDataForm::Field> fieldMap;
        const auto fields = d->form.fields();
        for (const auto &field : fields) {
            fieldMap.insert(field.key(), field);
        }

        if (fieldMap.contains(u"FORM_TYPE"_s)) {
            const QXmppDataForm::Field field = fieldMap.take(u"FORM_TYPE"_s);
            S += field.value().toString() + u"<";

            QStringList keys = fieldMap.keys();
            std::sort(keys.begin(), keys.end());
            for (const auto &key : keys) {
                const QXmppDataForm::Field field = fieldMap.value(key);
                S += key + u'<';
                if (field.value().canConvert<QStringList>()) {
                    QStringList list = field.value().toStringList();
                    list.sort();
                    S += list.join(u'<');
                } else {
                    S += field.value().toString();
                }
                S += u'<';
            }
        } else {
            qWarning("QXmppDiscoveryIq form does not contain FORM_TYPE");
        }
    }

    QCryptographicHash hasher(QCryptographicHash::Sha1);
    hasher.addData(S.toUtf8());
    return hasher.result();
}

/// \cond
bool QXmppDiscoveryIq::checkIqType(const QString &tagName, const QString &xmlNamespace)
{
    return tagName == u"query" &&
        (xmlNamespace == ns_disco_info || xmlNamespace == ns_disco_items);
}

void QXmppDiscoveryIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = firstChildElement(element, u"query");
    d->queryNode = queryElement.attribute(u"node"_s);
    if (queryElement.namespaceURI() == ns_disco_items) {
        d->queryType = ItemsQuery;
    } else {
        d->queryType = InfoQuery;
    }

    d->features = parseSingleAttributeElements(queryElement, u"feature", ns_disco_info, u"var"_s);
    d->identities = parseChildElements<QList<Identity>>(queryElement);
    d->items = parseChildElements<QList<Item>>(queryElement);
    d->form = parseOptionalChildElement<QXmppDataForm>(queryElement).value_or(QXmppDataForm());
}

void QXmppDiscoveryIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        { u"query", d->queryType == InfoQuery ? ns_disco_info : ns_disco_items },
        OptionalAttribute { u"node", d->queryNode },
        // InfoQuery
        OptionalContent {
            d->queryType == InfoQuery,
            d->identities,
            SingleAttributeElements { u"feature", u"var", d->features } },
        OptionalContent {
            d->queryType == ItemsQuery,
            d->items,
        },
        d->form,
    });
}
/// \endcond

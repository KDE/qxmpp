// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
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

///
/// \class QXmppDiscoItem
///
/// Related entity that can be queried using \xep{0030, Service Discovery}.
///
/// \since QXmpp 1.12
///

/// \cond
std::optional<QXmppDiscoItem> QXmppDiscoItem::fromDom(const QDomElement &el)
{
    QXmppDiscoItem item;
    item.m_jid = el.attribute(u"jid"_s);
    if (item.m_jid.isEmpty()) {
        return {};
    }
    item.m_name = el.attribute(u"name"_s);
    item.m_node = el.attribute(u"node"_s);
    return item;
}

void QXmppDiscoItem::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"item",
        Attribute { u"jid", m_jid },
        OptionalAttribute { u"name", m_name },
        OptionalAttribute { u"node", m_node },
    });
}
/// \endcond

///
/// \class QXmppDiscoItems
///
/// Items query request or result as defined in \xep{0030, Service Discovery}.
///
/// \since QXmpp 1.12
///

/// \cond
std::optional<QXmppDiscoItems> QXmppDiscoItems::fromDom(const QDomElement &el)
{
    return QXmppDiscoItems { el.attribute(u"node"_s), parseChildElements<QList<QXmppDiscoItem>>(el) };
}

void QXmppDiscoItems::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { XmlTag, OptionalAttribute { u"node", m_node }, m_items });
}
/// \endcond

///
/// \class QXmppDiscoIdentity
///
/// Identity of an XMPP entity as defined in \xep{0030, Service Discovery}.
///
/// \since QXmpp 1.12
///

/// \cond
std::optional<QXmppDiscoIdentity> QXmppDiscoIdentity::fromDom(const QDomElement &el)
{
    QXmppDiscoIdentity identity {
        el.attribute(u"category"_s),
        el.attribute(u"type"_s),
        el.attribute(u"name"_s),
        el.attributeNS(ns_xml.toString(), u"lang"_s),
    };
    if (identity.category().isEmpty() || identity.type().isEmpty()) {
        return {};
    }
    return identity;
}

void QXmppDiscoIdentity::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"identity",
        OptionalAttribute { u"xml:lang", m_language },
        Attribute { u"category", m_category },
        OptionalAttribute { u"name", m_name },
        Attribute { u"type", m_type },
    });
}
/// \endcond

///
/// \class QXmppDiscoInfo
///
/// Info query request or result as defined in \xep{0030, Service Discovery}.
///
/// \since QXmpp 1.12
///

/// \cond
std::optional<QXmppDiscoInfo> QXmppDiscoInfo::fromDom(const QDomElement &el)
{
    return QXmppDiscoInfo {
        el.attribute(u"node"_s),
        parseChildElements<QList<QXmppDiscoIdentity>>(el),
        parseSingleAttributeElements(el, u"feature", ns_disco_info, u"var"_s),
        parseChildElements<QList<QXmppDataForm>>(el),
    };
}

void QXmppDiscoInfo::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        OptionalAttribute { u"node", m_node },
        m_identities,
        SingleAttributeElements { u"feature", u"var", m_features },
        m_dataForms,
    });
}
/// \endcond

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
    QList<QXmppDataForm> dataForms;
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
/// Returns the first of the included data dataForms as defined by \xep{0128, Service Discovery Extensions}.
///
/// Returns empty form if no form is included.
///
/// \deprecated Use dataForms() or findForm() instead.
///
QXmppDataForm QXmppDiscoveryIq::form() const
{
    if (d->dataForms.empty()) {
        return {};
    }
    if (d->dataForms.size() == 1) {
        return d->dataForms.constFirst();
    }

    // compat: with old behaviour: append all fields into one form
    QXmppDataForm mixedForm = d->dataForms.constLast();
    mixedForm.setFields({});

    // copy all fields
    QList<QXmppDataForm::Field> mixedFields;
    for (const auto &form : d->dataForms) {
        mixedFields << form.fields();
    }
    mixedForm.setFields(mixedFields);

    return mixedForm;
}

///
/// Sets included data dataForms as defined by \xep{0128, Service Discovery Extensions}.
///
/// \deprecated Use setForms() instead.
///
void QXmppDiscoveryIq::setForm(const QXmppDataForm &form)
{
    d->dataForms.clear();
    d->dataForms.append(form);
}

///
/// Returns included data forms as defined by \xep{0128, Service Discovery Extensions}.
///
/// \since QXmpp 1.12
///
const QList<QXmppDataForm> &QXmppDiscoveryIq::dataForms() const
{
    return d->dataForms;
}

///
/// Sets included data forms as defined by \xep{0128, Service Discovery Extensions}.
///
/// Each form must have a FORM_TYPE field and each form type MUST occur only once.
///
/// \since QXmpp 1.12
///
void QXmppDiscoveryIq::setDataForms(const QList<QXmppDataForm> &dataForms)
{
    d->dataForms = dataForms;
}

///
/// Looks for a data form with the given form type and returns it if found.
///
/// Data dataForms in service discovery info are defined in \xep{0128, Service Discovery Extensions}.
///
/// \since QXmpp 1.12
///
std::optional<QXmppDataForm> QXmppDiscoveryIq::dataForm(QStringView formType) const
{
    for (const auto &form : d->dataForms) {
        if (form.formType() == formType) {
            return form;
        }
    }
    return {};
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

    // extension data dataForms
    auto forms = d->dataForms;
    std::sort(forms.begin(), forms.end(), [](const auto &a, const auto &b) {
        return a.formType() < b.formType();
    });

    for (const auto &form : forms) {
        S += form.formType();
        S += u'<';

        auto fields = form.fields();
        std::sort(fields.begin(), fields.end(), [](const auto &a, const auto &b) {
            return a.key() < b.key();
        });

        for (const auto &field : fields) {
            if (field.key() != u"FORM_TYPE") {
                S += field.key() + u'<';
                if (field.value().canConvert<QStringList>()) {
                    QStringList list = field.value().toStringList();
                    list.sort();
                    S += list.join(u'<');
                } else {
                    S += field.value().toString();
                }
                S += u'<';
            }
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
    d->dataForms = parseChildElements<QList<QXmppDataForm>>(queryElement);
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
        d->dataForms,
    });
}
/// \endcond

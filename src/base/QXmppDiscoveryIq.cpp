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

static bool entityCapabilities1Compare(const QXmppDiscoIdentity &i1, const QXmppDiscoIdentity &i2)
{
    return std::tuple { i1.category(), i1.type(), i1.language(), i1.name() } <
        std::tuple { i2.category(), i2.type(), i2.language(), i2.name() };
}

///
/// \class QXmppDiscoItem
///
/// Related entity that can be queried using \xep{0030, Service Discovery}.
///
/// \note In QXmpp 1.11 and earlier, this class was called QXmppDiscoveryIq::Item.
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
/// \note In QXmpp 1.11 and earlier, this class was called QXmppDiscoveryIq::Item.
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

///
/// Looks for a data form with the given form type and returns it if found.
///
/// Data forms in service discovery info are defined in \xep{0128, Service Discovery Extensions}.
///
std::optional<QXmppDataForm> QXmppDiscoInfo::dataForm(QStringView formType) const
{
    return find(m_dataForms, formType, &QXmppDataForm::formType);
}

///
/// Calculates an \xep{0115, Entity Capabilities} hash value of this service discovery data object.
///
QByteArray QXmppDiscoInfo::calculateEntityCapabilitiesHash() const
{
    QString S;

    // identities
    auto identities = m_identities;
    std::sort(identities.begin(), identities.end(), entityCapabilities1Compare);

    for (const auto &identity : std::as_const(identities)) {
        S += identity.category() + u'/' + identity.type() + u'/' + identity.language() + u'/' + identity.name() + u'<';
    }

    // features
    auto features = m_features;
    std::sort(features.begin(), features.end());
    features.removeDuplicates();

    for (const auto &feature : std::as_const(features)) {
        S += feature + u'<';
    }

    // data forms
    auto forms = m_dataForms;
    std::sort(forms.begin(), forms.end(), [](const auto &a, const auto &b) {
        return a.formType() < b.formType();
    });

    for (auto &form : std::as_const(forms)) {
        S += form.formType();
        S += u'<';

        auto fields = form.constFields();
        std::sort(fields.begin(), fields.end(), [](const auto &a, const auto &b) {
            return a.key() < b.key();
        });

        for (const auto &field : std::as_const(fields)) {
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

    return QCryptographicHash::hash(S.toUtf8(), QCryptographicHash::Sha1);
}

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
    std::sort(sortedIdentities.begin(), sortedIdentities.end(), entityCapabilities1Compare);
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

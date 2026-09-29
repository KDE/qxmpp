// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppDiscoveryIq.h"

#include "QXmppConstants_p.h"
#include "QXmppDiscoveryIq_p.h"
#include "QXmppHashing_p.h"
#include "QXmppUtils_p.h"

#include "Algorithms.h"
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

/*!
    \class QXmppDiscoItem
    \inmodule QXmpp

    Related entity that can be queried using \xep{0030}{Service Discovery}.

    \note In QXmpp 1.11 and earlier, this class was called QXmppDiscoveryIq::Item.
*/

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

/*!
    \class QXmppDiscoItems
    \inmodule QXmpp

    Items query request or result as defined in \xep{0030}{Service Discovery}.

    \since QXmpp 1.12
*/

std::optional<QXmppDiscoItems> QXmppDiscoItems::fromDom(const QDomElement &el)
{
    return QXmppDiscoItems { el.attribute(u"node"_s), parseChildElements<QList<QXmppDiscoItem>>(el) };
}

void QXmppDiscoItems::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { XmlTag, OptionalAttribute { u"node", m_node }, m_items });
}

/*!
    \class QXmppDiscoIdentity
    \inmodule QXmpp

    Identity of an XMPP entity as defined in \xep{0030}{Service Discovery}.

    \note In QXmpp 1.11 and earlier, this class was called QXmppDiscoveryIq::Item.
*/

std::optional<QXmppDiscoIdentity> QXmppDiscoIdentity::fromDom(const QDomElement &el)
{
    QXmppDiscoIdentity identity {
        el.attribute(u"category"_s),
        el.attribute(u"type"_s),
        el.attribute(u"name"_s),
        el.attributeNS(staticString(ns_xml), u"lang"_s),
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

/*!
    \class QXmppDiscoInfo
    \inmodule QXmpp

    Info query request or result as defined in \xep{0030}{Service Discovery}.

    \since QXmpp 1.12
*/

/*!
    Looks for a data form with the given form type and returns it if found.

    Data forms in service discovery info are defined in \xep{0128}{Service Discovery Extensions}.
*/
std::optional<QXmppDataForm> QXmppDiscoInfo::dataForm(QStringView formType) const
{
    return find(m_dataForms, formType, &QXmppDataForm::formType);
}

/*! Calculates an \xep{0115}{Entity Capabilities} hash value of this service discovery data object. */
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

std::optional<QXmpp::HashAlgorithm> QXmpp::Private::capsHashAlgorithm(QStringView hash)
{
    // IANA Hash Function Textual Names, only algorithms considered secure enough for caps
    switch (auto algorithm = Enums::fromString<HashAlgorithm>(hash).value_or(HashAlgorithm::Unknown)) {
    case HashAlgorithm::Sha1:
    case HashAlgorithm::Sha224:
    case HashAlgorithm::Sha256:
    case HashAlgorithm::Sha384:
    case HashAlgorithm::Sha512:
        return algorithm;
    default:
        return {};
    }
}

std::optional<QByteArray> QXmpp::Private::capsVerificationString(const QDomElement &query, HashAlgorithm algorithm)
{
    const auto qtAlgorithm = toQCryptographicHashAlgorithm(algorithm);
    if (!qtAlgorithm) {
        return {};
    }

    // strings are compared as UTF-8 ("i;octet" collation)
    using Identity = std::tuple<QByteArray, QByteArray, QByteArray, QByteArray>;
    using Field = std::pair<QByteArray, QList<QByteArray>>;
    using Form = std::pair<QByteArray, std::vector<Field>>;

    auto hasDuplicates = [](const auto &sorted) {
        return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
    };
    auto values = [](const QDomElement &field) {
        QList<QByteArray> values;
        for (const auto &value : iterChildElements(field, u"value", ns_data)) {
            values.push_back(value.text().toUtf8());
        }
        return values;
    };

    std::vector<Identity> identities;
    for (const auto &el : iterChildElements(query, u"identity", ns_disco_info)) {
        identities.emplace_back(el.attribute(u"category"_s).toUtf8(),
                                el.attribute(u"type"_s).toUtf8(),
                                el.attributeNS(ns_xml.toString(), u"lang"_s).toUtf8(),
                                el.attribute(u"name"_s).toUtf8());
    }
    std::ranges::sort(identities);
    if (hasDuplicates(identities)) {
        return {};
    }

    QList<QByteArray> features;
    for (const auto &el : iterChildElements(query, u"feature", ns_disco_info)) {
        features.push_back(el.attribute(u"var"_s).toUtf8());
    }
    std::ranges::sort(features);
    if (hasDuplicates(features)) {
        return {};
    }

    std::vector<Form> forms;
    for (const auto &formEl : iterChildElements(query, u"x", ns_data)) {
        std::optional<QByteArray> formType;
        std::vector<Field> fields;
        for (const auto &fieldEl : iterChildElements(formEl, u"field", ns_data)) {
            if (fieldEl.attribute(u"var"_s) == u"FORM_TYPE") {
                auto formTypes = values(fieldEl);
                std::ranges::sort(formTypes);
                formTypes.erase(std::unique(formTypes.begin(), formTypes.end()), formTypes.end());
                if (formTypes.size() > 1) {
                    return {};
                }
                if (fieldEl.attribute(u"type"_s) == u"hidden" && formTypes.size() == 1) {
                    formType = formTypes.constFirst();
                }
            } else {
                auto fieldValues = values(fieldEl);
                std::ranges::sort(fieldValues);
                fields.emplace_back(fieldEl.attribute(u"var"_s).toUtf8(), std::move(fieldValues));
            }
        }
        // forms without a valid FORM_TYPE are ignored
        if (formType) {
            std::ranges::sort(fields, {}, &Field::first);
            forms.emplace_back(std::move(*formType), std::move(fields));
        }
    }
    std::ranges::sort(forms, {}, &Form::first);
    if (std::ranges::adjacent_find(forms, {}, &Form::first) != forms.end()) {
        return {};
    }

    QByteArray s;
    for (const auto &[category, type, lang, name] : identities) {
        s += category + '/' + type + '/' + lang + '/' + name + '<';
    }
    for (const auto &feature : std::as_const(features)) {
        s += feature + '<';
    }
    for (const auto &[formType, fields] : forms) {
        s += formType + '<';
        for (const auto &[var, fieldValues] : fields) {
            s += var + '<';
            for (const auto &value : fieldValues) {
                s += value + '<';
            }
        }
    }
    return QCryptographicHash::hash(s, *qtAlgorithm);
}

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

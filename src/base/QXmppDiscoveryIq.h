// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERY_H
#define QXMPPDISCOVERY_H

#include "QXmppDataFormBase.h"
#include "QXmppIq.h"

#include <QSharedDataPointer>

class QXmppDiscoveryIdentityPrivate;
class QXmppDiscoveryItemPrivate;
class QXmppDiscoveryIqPrivate;

class QXMPP_EXPORT QXmppDiscoItem
{
public:
    QXmppDiscoItem() { }
    /// Default constructor
    explicit QXmppDiscoItem(const QString &jid, const QString &name = {}, const QString &node = {})
        : m_jid(jid), m_name(name), m_node(node) { }

    /// Returns the JID of the item.
    const QString &jid() const { return m_jid; }
    /// Sets the JID of the item.
    void setJid(const QString &newJid) { m_jid = newJid; }

    /// Returns the name of the item.
    const QString &name() const { return m_name; }
    /// Sets the name of the item.
    void setName(const QString &newName) { m_name = newName; }

    /// Returns the node for querying the information.
    const QString &node() const { return m_node; }
    /// Sets the node for querying the information.
    void setNode(const QString &newNode) { m_node = newNode; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"item", QXmpp::Private::ns_disco_items };
    static std::optional<QXmppDiscoItem> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_jid;
    QString m_name;
    QString m_node;
};

class QXMPP_EXPORT QXmppDiscoItems
{
public:
    QXmppDiscoItems() { }
    /// Default constructor
    explicit QXmppDiscoItems(const QString &node, const QList<QXmppDiscoItem> &items = {})
        : m_node(node), m_items(items) { }

    /// Returns the items.
    const QList<QXmppDiscoItem> &items() const { return m_items; }
    /// Sets the items.
    void setItems(const QList<QXmppDiscoItem> &newItems) { m_items = newItems; }

    /// Returns the node of the query.
    const QString &node() const { return m_node; }
    /// Sets the node of the query.
    void setNode(const QString &newNode) { m_node = newNode; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"query", QXmpp::Private::ns_disco_items };
    static std::optional<QXmppDiscoItems> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QList<QXmppDiscoItem> m_items;
    QString m_node;
};

class QXMPP_EXPORT QXmppDiscoIdentity
{
public:
    QXmppDiscoIdentity() { }
    /// Default constructor
    explicit QXmppDiscoIdentity(const QString &category, const QString &type = {}, const QString &name = {}, const QString &lang = {})
        : m_category(category), m_type(type), m_name(name), m_language(lang) { }

    /// Returns the category of the identity.
    const QString &category() const { return m_category; }
    /// Sets the category of the identity.
    void setCategory(const QString &newCategory) { m_category = newCategory; }

    /// Returns the type of the identity.
    const QString &type() const { return m_type; }
    /// Sets the type of the identity.
    void setType(const QString &newType) { m_type = newType; }

    /// Returns the name or description of the identity.
    const QString &name() const { return m_name; }
    /// Sets the name or description of the identity.
    void setName(const QString &newName) { m_name = newName; }

    /// Returns the language of the name.
    const QString &language() const { return m_language; }
    /// Sets the language of the name.
    void setLanguage(const QString &newLanguage) { m_language = newLanguage; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"identity", QXmpp::Private::ns_disco_info };
    static std::optional<QXmppDiscoIdentity> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_category;
    QString m_type;
    QString m_name;
    QString m_language;
};

class QXMPP_EXPORT QXmppDiscoInfo
{
public:
    QXmppDiscoInfo() { }
    /// Default constructor
    explicit QXmppDiscoInfo(const QString &node, const QList<QXmppDiscoIdentity> &identities = {}, const QList<QString> &features = {}, const QList<QXmppDataForm> &dataForms = {})
        : m_node(node), m_identities(identities), m_features(features), m_dataForms(dataForms) { }

    /// Returns the node of the query.
    const QString &node() const { return m_node; }
    /// Sets the node of the query.
    void setNode(const QString &newNode) { m_node = newNode; }

    /// Returns the identities of the entity.
    const QList<QXmppDiscoIdentity> &identities() const { return m_identities; }
    /// Sets the identities of the entity.
    void setIdentities(const QList<QXmppDiscoIdentity> &newIdentities) { m_identities = newIdentities; }

    /// Returns the features supported by the entity.
    const QList<QString> &features() const { return m_features; }
    /// Sets the features supported by the entity.
    void setFeatures(const QList<QString> &newFeatures) { m_features = newFeatures; }

    /// Returns additional data forms as specified in \xep{0128, Service Discovery Extensions}.
    const QList<QXmppDataForm> &dataForms() const { return m_dataForms; }
    /// Sets additional data forms as specified in \xep{0128, Service Discovery Extensions}.
    /// Forms MUST have a FORM_TYPE and each FORM_TYPE MUST occur only once.
    void setDataForms(const QList<QXmppDataForm> &newDataForms) { m_dataForms = newDataForms; }

    QByteArray calculateEntityCapabilitiesHash() const;

    /// \cond
    static constexpr std::tuple XmlTag = { u"query", QXmpp::Private::ns_disco_info };
    static std::optional<QXmppDiscoInfo> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_node;
    QList<QXmppDiscoIdentity> m_identities;
    QList<QString> m_features;
    QList<QXmppDataForm> m_dataForms;
};

class QXMPP_EXPORT QXmppDiscoveryIq : public QXmppIq
{
public:
    class QXMPP_EXPORT Identity
    {
    public:
        Identity();
        QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(Identity)

        QString category() const;
        void setCategory(const QString &category);

        QString language() const;
        void setLanguage(const QString &language);

        QString name() const;
        void setName(const QString &name);

        QString type() const;
        void setType(const QString &type);

        /// \cond
        static constexpr std::tuple XmlTag = { u"identity", QXmpp::Private::ns_disco_info };
        static std::optional<Identity> fromDom(const QDomElement &el);
        void toXml(QXmlStreamWriter *writer) const;
        /// \endcond

    private:
        QSharedDataPointer<QXmppDiscoveryIdentityPrivate> d;
    };

    class QXMPP_EXPORT Item
    {
    public:
        Item();
        QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(Item)

        QString jid() const;
        void setJid(const QString &jid);

        QString name() const;
        void setName(const QString &name);

        QString node() const;
        void setNode(const QString &node);

        /// \cond
        static constexpr std::tuple XmlTag = { u"item", QXmpp::Private::ns_disco_items };
        static std::optional<Item> fromDom(const QDomElement &el);
        void toXml(QXmlStreamWriter *writer) const;
        /// \endcond

    private:
        QSharedDataPointer<QXmppDiscoveryItemPrivate> d;
    };

    QXmppDiscoveryIq();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppDiscoveryIq)

    enum QueryType {
        InfoQuery,
        ItemsQuery
    };

    QStringList features() const;
    void setFeatures(const QStringList &features);

    QList<QXmppDiscoveryIq::Identity> identities() const;
    void setIdentities(const QList<QXmppDiscoveryIq::Identity> &identities);

    QList<QXmppDiscoveryIq::Item> items() const;
    void setItems(const QList<QXmppDiscoveryIq::Item> &items);

    [[deprecated("Use dataForms() instead")]]
    QXmppDataForm form() const;
    [[deprecated("Use setDataForms() instead")]]
    void setForm(const QXmppDataForm &form);

    const QList<QXmppDataForm> &dataForms() const;
    void setDataForms(const QList<QXmppDataForm> &dataForms);
    std::optional<QXmppDataForm> dataForm(QStringView formType) const;

    /// Looks for a form with the form type of FormT and parses it if found.
    /// \since QXmpp 1.12
    template<QXmpp::Private::DataFormConvertible FormT>
    std::optional<FormT> dataForm() const
    {
        if (auto form = dataForm(QXmpp::Private::DataFormType<FormT>)) {
            if (auto result = FormT::fromDataForm(*form)) {
                return *result;
            }
        }
        return {};
    }

    QString queryNode() const;
    void setQueryNode(const QString &node);

    enum QueryType queryType() const;
    void setQueryType(enum QueryType type);

    QByteArray verificationString() const;

    /// \cond
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isDiscoveryIq(const QDomElement &element);
    static bool checkIqType(const QString &tagName, const QString &xmlNamespace);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;
    /// \endcond

private:
    QSharedDataPointer<QXmppDiscoveryIqPrivate> d;
};

#endif

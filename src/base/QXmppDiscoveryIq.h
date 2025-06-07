// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERY_H
#define QXMPPDISCOVERY_H

#include "QXmppDataForm.h"
#include "QXmppIq.h"

#include <QSharedDataPointer>

class QXmppDiscoveryIdentityPrivate;
class QXmppDiscoveryItemPrivate;
class QXmppDiscoveryIqPrivate;

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

    QXmppDataForm form() const;
    void setForm(const QXmppDataForm &form);

    QString queryNode() const;
    void setQueryNode(const QString &node);

    enum QueryType queryType() const;
    void setQueryType(enum QueryType type);

    QByteArray verificationString() const;

    static bool isDiscoveryIq(const QDomElement &element);
    /// \cond
    static bool checkIqType(const QString &tagName, const QString &xmlNamespace);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;
    /// \endcond

private:
    QSharedDataPointer<QXmppDiscoveryIqPrivate> d;
};

#endif

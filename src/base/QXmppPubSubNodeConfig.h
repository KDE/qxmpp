// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPUBSUBNODECONFIG_H
#define QXMPPPUBSUBNODECONFIG_H

#include "QXmppDataForm.h"
#include "QXmppDataFormBase.h"

#include <variant>

class QXmppPubSubNodeConfigPrivate;

class QXMPP_EXPORT QXmppPubSubNodeConfig : public QXmppExtensibleDataFormBase
{
public:
    /*!
        \inmodule QXmpp

        Sentinel type indicating that no item limit has been set.

        \sa ItemLimit
    */
    struct Unset { };

    /*!
        \inmodule QXmpp

        Sentinel type indicating that the item limit is set to the maximum
        value supported by the service.

        \sa ItemLimit
    */
    struct Max { };
    using ItemLimit = std::variant<Unset, uint64_t, Max>;

    /*!
        Describes who may subscribe to a node and retrieve its items.

        \value Open Anyone may subscribe and retrieve items.
        \value Presence Only contacts with a presence subscription may subscribe.
        \value Roster Only contacts in allowed roster groups may subscribe.
        \value Authorize Subscriptions must be approved by the node owner.
        \value Allowlist Only users on a whitelist set by the owner may subscribe.
    */
    enum AccessModel : uint8_t {
        Open,
        Presence,
        Roster,
        Authorize,
        Allowlist
    };

    /*!
        Describes who is allowed to publish items to a node.

        \value Publishers Only explicitly designated publishers may publish.
        \value Subscribers Any subscriber may publish.
        \value Anyone Anyone may publish, including non-subscribers.
    */
    enum PublishModel : uint8_t {
        Publishers,
        Subscribers,
        Anyone
    };

    /*!
        Describes the policy for associating child nodes with a collection node.

        \value All Any node may be associated as a child.
        \value Owners Only node owners may associate child nodes.
        \value Whitelist Only nodes whose owners are on the whitelist may be associated.
    */
    enum class ChildAssociationPolicy : uint8_t {
        All,
        Owners,
        Whitelist
    };

    /*!
        Describes who is shown as the publisher of an item.

        \value NodeOwner The node owner is shown as the publisher.
        \value Publisher The actual publisher of the item is shown.
    */
    enum ItemPublisher : uint8_t {
        NodeOwner,
        Publisher
    };

    /*!
        Describes the type of a PubSub node.

        \value Leaf A leaf node stores and distributes items.
        \value Collection A collection node contains other nodes.
    */
    enum NodeType : uint8_t {
        Leaf,
        Collection
    };

    /*!
        Describes the message type used to deliver event notifications.

        \value Normal Notifications are delivered as normal messages.
        \value Headline Notifications are delivered as headline messages.
    */
    enum NotificationType : uint8_t {
        Normal,
        Headline
    };

    /*!
        Describes when the last published item is sent to a new subscriber.

        \value Never The last item is never sent on subscription.
        \value OnSubscription The last item is sent when a subscription is created.
        \value OnSubscriptionAndPresence The last item is sent on subscription and on each presence change.
    */
    enum SendLastItemType : uint8_t {
        Never,
        OnSubscription,
        OnSubscriptionAndPresence
    };

    static std::optional<QXmppPubSubNodeConfig> fromDataForm(const QXmppDataForm &form);

    QXmppPubSubNodeConfig();
    QXmppPubSubNodeConfig(const QXmppPubSubNodeConfig &);
    QXmppPubSubNodeConfig(QXmppPubSubNodeConfig &&);
    ~QXmppPubSubNodeConfig() override;

    QXmppPubSubNodeConfig &operator=(const QXmppPubSubNodeConfig &);
    QXmppPubSubNodeConfig &operator=(QXmppPubSubNodeConfig &&);

    std::optional<AccessModel> accessModel() const;
    void setAccessModel(std::optional<AccessModel> accessModel);

    QString bodyXslt() const;
    void setBodyXslt(const QString &bodyXslt);

    std::optional<ChildAssociationPolicy> childAssociationPolicy() const;
    void setChildAssociationPolicy(std::optional<ChildAssociationPolicy> childAssociationPolicy);

    QStringList childAssociationAllowlist() const;
    void setChildAssociationAllowlist(const QStringList &childAssociationWhitelist);

    QStringList childNodes() const;
    void setChildNodes(const QStringList &childNodes);

    std::optional<quint32> childNodesMax() const;
    void setChildNodesMax(std::optional<quint32> childNodesMax);

    QStringList collections() const;
    void setCollections(const QStringList &collections);

    QStringList contactJids() const;
    void setContactJids(const QStringList &contactJids);

    QString dataFormXslt() const;
    void setDataFormXslt(const QString &dataFormXslt);

    std::optional<bool> notificationsEnabled() const;
    void setNotificationsEnabled(std::optional<bool> notificationsEnabled);

    std::optional<bool> includePayloads() const;
    void setIncludePayloads(std::optional<bool> includePayloads);

    QString description() const;
    void setDescription(const QString &description);

    std::optional<quint32> itemExpiry() const;
    void setItemExpiry(std::optional<quint32> itemExpiry);

    std::optional<ItemPublisher> notificationItemPublisher() const;
    void setNotificationItemPublisher(std::optional<ItemPublisher> notificationItemPublisher);

    QString language() const;
    void setLanguage(const QString &language);

    ItemLimit maxItems() const;
    void setMaxItems(ItemLimit maxItems);
    inline void resetMaxItems() { setMaxItems(Unset()); }

    std::optional<quint32> maxPayloadSize() const;
    void setMaxPayloadSize(std::optional<quint32> maxPayloadSize);

    std::optional<NodeType> nodeType() const;
    void setNodeType(std::optional<NodeType> nodeType);

    std::optional<QXmppPubSubNodeConfig::NotificationType> notificationType() const;
    void setNotificationType(std::optional<QXmppPubSubNodeConfig::NotificationType> notificationType);

    std::optional<bool> configNotificationsEnabled() const;
    void setConfigNotificationsEnabled(std::optional<bool> configNotificationsEnabled);

    std::optional<bool> deleteNotificationsEnabled() const;
    void setDeleteNotificationsEnabled(std::optional<bool> nodeDeleteNotificationsEnabled);

    std::optional<bool> retractNotificationsEnabled() const;
    void setRetractNotificationsEnabled(std::optional<bool> retractNotificationsEnabled);

    std::optional<bool> subNotificationsEnabled() const;
    void setSubNotificationsEnabled(std::optional<bool> subNotificationsEnabled);

    std::optional<bool> persistItems() const;
    void setPersistItems(std::optional<bool> persistItems);

    std::optional<bool> presenceBasedNotifications() const;
    void setPresenceBasedNotifications(std::optional<bool> presenceBasedNotifications);

    std::optional<PublishModel> publishModel() const;
    void setPublishModel(std::optional<PublishModel> publishModel);

    std::optional<bool> purgeWhenOffline() const;
    void setPurgeWhenOffline(std::optional<bool> purgeWhenOffline);

    QStringList allowedRosterGroups() const;
    void setAllowedRosterGroups(const QStringList &allowedRosterGroups);

    std::optional<SendLastItemType> sendLastItem() const;
    void setSendLastItem(std::optional<SendLastItemType> sendLastItem);

    std::optional<bool> temporarySubscriptions() const;
    void setTemporarySubscriptions(std::optional<bool> temporarySubscriptions);

    std::optional<bool> allowSubscriptions() const;
    void setAllowSubscriptions(std::optional<bool> allowSubscriptions);

    QString title() const;
    void setTitle(const QString &title);

    QString payloadType() const;
    void setPayloadType(const QString &payloadType);

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppPubSubNodeConfigPrivate> d;
};

class QXMPP_EXPORT QXmppPubSubPublishOptions : public QXmppPubSubNodeConfig
{
public:
    static std::optional<QXmppPubSubPublishOptions> fromDataForm(const QXmppDataForm &form);

protected:
    QString formType() const override;
};

Q_DECLARE_METATYPE(QXmppPubSubNodeConfig);
Q_DECLARE_METATYPE(QXmppPubSubPublishOptions);

#endif  // QXMPPPUBSUBNODECONFIG_H

// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPubSubNodeConfig.h"

#include "QXmppPubSubNodeConfig_p.h"
#include "QXmppVisitHelper_p.h"

#include "StringLiterals.h"

using namespace QXmpp::Private;

static const auto NODE_CONFIG_FORM_TYPE = u"http://jabber.org/protocol/pubsub#node_config"_s;
static const auto PUBLISH_OPTIONS_FORM_TYPE = u"http://jabber.org/protocol/pubsub#publish-options"_s;

static const auto ACCESS_MODEL = u"pubsub#access_model"_s;
static const auto BODY_XSLT = u"pubsub#body_xslt"_s;
static const auto CHILD_ASSOCIATION_POLICY = u"pubsub#children_association_policy"_s;
static const auto CHILD_ASSOCIATION_ALLOWLIST = u"pubsub#children_association_whitelist"_s;
static const auto CHILD_NODES = u"pubsub#children"_s;
static const auto CHILD_NODES_MAX = u"pubsub#children_max"_s;
static const auto COLLECTIONS = u"pubsub#collection"_s;
static const auto CONTACT_JIDS = u"pubsub#contact"_s;
static const auto DATA_FORM_XSLT = u"pubsub#dataform_xslt"_s;
static const auto NOTIFICATIONS_ENABLED = u"pubsub#deliver_notifications"_s;
static const auto INCLUDE_PAYLOADS = u"pubsub#deliver_payloads"_s;
static const auto DESCRIPTION = u"pubsub#description"_s;
static const auto ITEM_EXPIRY = u"pubsub#item_expire"_s;
static const auto NOTIFICATION_ITEM_PUBLISHER = u"pubsub#itemreply"_s;
static const auto LANGUAGE = u"pubsub#language"_s;
static const auto MAX_ITEMS = u"pubsub#max_items"_s;
static const auto MAX_PAYLOAD_SIZE = u"pubsub#max_payload_size"_s;
static const auto NODE_TYPE = u"pubsub#node_type"_s;
static const auto NOTIFICATION_TYPE = u"pubsub#notification_type"_s;
static const auto CONFIG_NOTIFICATIONS_ENABLED = u"pubsub#notify_config"_s;
static const auto NODE_DELETE_NOTIFICATIONS_ENABLED = u"pubsub#notify_delete"_s;
static const auto RETRACT_NOTIFICATIONS_ENABLED = u"pubsub#notify_retract"_s;
static const auto SUB_NOTIFICATIONS_ENABLED = u"pubsub#notify_sub"_s;
static const auto PERSIST_ITEMS = u"pubsub#persist_items"_s;
static const auto PRESENCE_BASED_NOTIFICATIONS = u"pubsub#presence_based_delivery"_s;
static const auto PUBLISH_MODEL = u"pubsub#publish_model"_s;
static const auto PURGE_WHEN_OFFLINE = u"pubsub#purge_offline"_s;
static const auto ALLOWED_ROSTER_GROUPS = u"pubsub#roster_groups_allowed"_s;
static const auto SEND_LAST_ITEM = u"pubsub#send_last_published_item"_s;
static const auto TEMPORARY_SUBSCRIPTIONS = u"pubsub#tempsub"_s;
static const auto ALLOW_SUBSCRIPTIONS = u"pubsub#subscribe"_s;
static const auto TITLE = u"pubsub#title"_s;
static const auto PAYLOAD_TYPE = u"pubsub#type"_s;

class QXmppPubSubNodeConfigPrivate : public QSharedData
{
public:
    std::optional<QXmppPubSubNodeConfig::AccessModel> accessModel;
    QString bodyXslt;
    std::optional<QXmppPubSubNodeConfig::ChildAssociationPolicy> childAssociationPolicy;
    QStringList childAssociationAllowlist;
    QStringList childNodes;
    std::optional<quint32> childNodesMax;
    QStringList collections;
    QStringList contactJids;
    QString dataFormXslt;
    std::optional<bool> notificationsEnabled;
    std::optional<bool> includePayloads;
    QString description;
    std::optional<quint32> itemExpiry;
    std::optional<QXmppPubSubNodeConfig::ItemPublisher> notificationItemPublisher;
    QString language;
    QXmppPubSubNodeConfig::ItemLimit maxItems;
    std::optional<quint32> maxPayloadSize;
    std::optional<QXmppPubSubNodeConfig::NodeType> nodeType;
    std::optional<QXmppPubSubNodeConfig::NotificationType> notificationType;
    std::optional<bool> configNotificationsEnabled;
    std::optional<bool> deleteNotificationsEnabled;
    std::optional<bool> retractNotificationsEnabled;
    std::optional<bool> subNotificationsEnabled;
    std::optional<bool> persistItems;
    std::optional<bool> presenceBasedNotifications;
    std::optional<QXmppPubSubNodeConfig::PublishModel> publishModel;
    std::optional<bool> purgeWhenOffline;
    QStringList allowedRosterGroups;
    std::optional<QXmppPubSubNodeConfig::SendLastItemType> sendLastItem;
    std::optional<bool> temporarySubscriptions;
    std::optional<bool> allowSubscriptions;
    QString title;
    QString payloadType;
};

/*!
    \class QXmppPubSubNodeConfig
    \inmodule QXmpp

    \brief The QXmppPubSubNodeConfig class represents the configuration of a PubSub node as
    defined by \xep{0060}{Publish-Subscribe}.

    The configuration is exchanged as a data form with the form type
    \c {http://jabber.org/protocol/pubsub#node_config}. It can be requested and changed by the
    node owner, e.g. using QXmppPubSubManager::requestNodeConfiguration() and
    QXmppPubSubManager::configureNode().

    All fields are optional: \c std::nullopt, empty strings and empty lists mean that a field is
    not set and it is not included in the serialized form. Fields that are not known to this
    class are kept as additional fields of the QXmppExtensibleDataFormBase.

    \sa QXmppPubSubPublishOptions, QXmppPubSubMetadata

    \since QXmpp 1.5

    \ingroup qxmpp-stanzas
*/

/*!
    \typealias QXmppPubSubNodeConfig::ItemLimit

    Maximum number of items of a node: either a number, Max for the maximum supported by the
    service or Unset if no limit has been set.
*/

/*!
    \fn void QXmppPubSubNodeConfig::resetMaxItems()

    Resets the maximum number of items to Unset.

    \sa maxItems(), setMaxItems()
*/

/*!
    Parses the node configuration from \a form.

    Returns \c std::nullopt if the form type of \a form is not
    \c {http://jabber.org/protocol/pubsub#node_config} or if the form could not be parsed.
*/
std::optional<QXmppPubSubNodeConfig> QXmppPubSubNodeConfig::fromDataForm(const QXmppDataForm &form)
{
    if (form.formType() != NODE_CONFIG_FORM_TYPE) {
        return std::nullopt;
    }

    QXmppPubSubNodeConfig nodeConfig;
    if (QXmppDataFormBase::fromDataForm(form, nodeConfig)) {
        return nodeConfig;
    }
    return std::nullopt;
}

/*! Constructs an empty node configuration. */
QXmppPubSubNodeConfig::QXmppPubSubNodeConfig()
    : d(new QXmppPubSubNodeConfigPrivate)
{
}

/*! Default copy-constructor */
QXmppPubSubNodeConfig::QXmppPubSubNodeConfig(const QXmppPubSubNodeConfig &) = default;
/*! Default move-constructor */
QXmppPubSubNodeConfig::QXmppPubSubNodeConfig(QXmppPubSubNodeConfig &&) = default;
QXmppPubSubNodeConfig::~QXmppPubSubNodeConfig() = default;
/*! Default assignment operator */
QXmppPubSubNodeConfig &QXmppPubSubNodeConfig::operator=(const QXmppPubSubNodeConfig &) = default;
/*! Default move-assignment operator */
QXmppPubSubNodeConfig &QXmppPubSubNodeConfig::operator=(QXmppPubSubNodeConfig &&) = default;

/*!
    Returns the access model of the node.

    The access model defines who may subscribe to the node and retrieve its items.

    This corresponds to the \c {pubsub#access_model} field.
*/
std::optional<QXmppPubSubNodeConfig::AccessModel> QXmppPubSubNodeConfig::accessModel() const
{
    return d->accessModel;
}

/*!
    Sets the access model of the node to \a accessModel.

    \sa accessModel()
*/
void QXmppPubSubNodeConfig::setAccessModel(std::optional<AccessModel> accessModel)
{
    d->accessModel = accessModel;
}

/*!
    Returns the URL of the XSL transformation for message bodies.

    The transformation is applied to payloads in order to generate an appropriate message body
    element.

    This corresponds to the \c {pubsub#body_xslt} field.
*/
QString QXmppPubSubNodeConfig::bodyXslt() const
{
    return d->bodyXslt;
}

/*!
    Sets the URL of the XSL transformation for message bodies to \a bodyXslt.

    \sa bodyXslt()
*/
void QXmppPubSubNodeConfig::setBodyXslt(const QString &bodyXslt)
{
    d->bodyXslt = bodyXslt;
}

/*!
    Returns the child association policy.

    Defines who may associate leaf nodes with a collection node.

    This corresponds to the \c {pubsub#children_association_policy} field.
*/
std::optional<QXmppPubSubNodeConfig::ChildAssociationPolicy> QXmppPubSubNodeConfig::childAssociationPolicy() const
{
    return d->childAssociationPolicy;
}

/*!
    Sets the child association policy to \a childAssociationPolicy.

    \sa childAssociationPolicy()
*/
void QXmppPubSubNodeConfig::setChildAssociationPolicy(std::optional<ChildAssociationPolicy> childAssociationPolicy)
{
    d->childAssociationPolicy = childAssociationPolicy;
}

/*!
    Returns the list of entities that may associate child nodes.

    Only used with the ChildAssociationPolicy::Whitelist policy.

    This corresponds to the \c {pubsub#children_association_whitelist} field.
*/
QStringList QXmppPubSubNodeConfig::childAssociationAllowlist() const
{
    return d->childAssociationAllowlist;
}

/*!
    Sets the list of entities that may associate child nodes to \a childAssociationWhitelist.

    \sa childAssociationAllowlist()
*/
void QXmppPubSubNodeConfig::setChildAssociationAllowlist(const QStringList &childAssociationWhitelist)
{
    d->childAssociationAllowlist = childAssociationWhitelist;
}

/*!
    Returns the child nodes of this collection node.

    Only applicable to collection nodes.

    This corresponds to the \c {pubsub#children} field.
*/
QStringList QXmppPubSubNodeConfig::childNodes() const
{
    return d->childNodes;
}

/*!
    Sets the child nodes of this collection node to \a childNodes.

    \sa childNodes()
*/
void QXmppPubSubNodeConfig::setChildNodes(const QStringList &childNodes)
{
    d->childNodes = childNodes;
}

/*!
    Returns the maximum number of child nodes.

    The maximum number of child nodes that can be associated with a collection node.

    This corresponds to the \c {pubsub#children_max} field.
*/
std::optional<quint32> QXmppPubSubNodeConfig::childNodesMax() const
{
    return d->childNodesMax;
}

/*!
    Sets the maximum number of child nodes to \a childNodesMax.

    \sa childNodesMax()
*/
void QXmppPubSubNodeConfig::setChildNodesMax(std::optional<quint32> childNodesMax)
{
    d->childNodesMax = childNodesMax;
}

/*!
    Returns the collection nodes this node is affiliated with.

    This corresponds to the \c {pubsub#collection} field.
*/
QStringList QXmppPubSubNodeConfig::collections() const
{
    return d->collections;
}

/*!
    Sets the collection nodes this node is affiliated with to \a collections.

    \sa collections()
*/
void QXmppPubSubNodeConfig::setCollections(const QStringList &collections)
{
    d->collections = collections;
}

/*!
    Returns the JIDs of the entities to contact with questions about the node.

    This corresponds to the \c {pubsub#contact} field.
*/
QStringList QXmppPubSubNodeConfig::contactJids() const
{
    return d->contactJids;
}

/*!
    Sets the JIDs of the entities to contact with questions about the node to \a contactJids.

    \sa contactJids()
*/
void QXmppPubSubNodeConfig::setContactJids(const QStringList &contactJids)
{
    d->contactJids = contactJids;
}

/*!
    Returns the URL of the XSL transformation for data forms.

    The transformation is applied to the payload format in order to generate a valid data form
    that the client could display.

    This corresponds to the \c {pubsub#dataform_xslt} field.
*/
QString QXmppPubSubNodeConfig::dataFormXslt() const
{
    return d->dataFormXslt;
}

/*!
    Sets the URL of the XSL transformation for data forms to \a dataFormXslt.

    \sa dataFormXslt()
*/
void QXmppPubSubNodeConfig::setDataFormXslt(const QString &dataFormXslt)
{
    d->dataFormXslt = dataFormXslt;
}

/*!
    Returns whether event notifications are delivered.

    This corresponds to the \c {pubsub#deliver_notifications} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::notificationsEnabled() const
{
    return d->notificationsEnabled;
}

/*!
    Sets whether event notifications are delivered (\a notificationsEnabled).

    \sa notificationsEnabled()
*/
void QXmppPubSubNodeConfig::setNotificationsEnabled(std::optional<bool> notificationsEnabled)
{
    d->notificationsEnabled = notificationsEnabled;
}

/*!
    Returns whether item payloads are included in event notifications.

    Only applicable to leaf nodes.

    This corresponds to the \c {pubsub#deliver_payloads} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::includePayloads() const
{
    return d->includePayloads;
}

/*!
    Sets whether item payloads are included in event notifications (\a includePayloads).

    \sa includePayloads()
*/
void QXmppPubSubNodeConfig::setIncludePayloads(std::optional<bool> includePayloads)
{
    d->includePayloads = includePayloads;
}

/*!
    Returns the description of the node.

    This corresponds to the \c {pubsub#description} field.
*/
QString QXmppPubSubNodeConfig::description() const
{
    return d->description;
}

/*!
    Sets the description of the node to \a description.

    \sa description()
*/
void QXmppPubSubNodeConfig::setDescription(const QString &description)
{
    d->description = description;
}

/*!
    Returns the number of seconds after which items are purged automatically.

    This corresponds to the \c {pubsub#item_expire} field.
*/
std::optional<quint32> QXmppPubSubNodeConfig::itemExpiry() const
{
    return d->itemExpiry;
}

/*!
    Sets the number of seconds after which items are purged automatically to \a itemExpiry.

    \sa itemExpiry()
*/
void QXmppPubSubNodeConfig::setItemExpiry(std::optional<quint32> itemExpiry)
{
    d->itemExpiry = itemExpiry;
}

/*!
    Returns who is shown as the publisher of items.

    Defines whether the node owner or the publisher of an item should receive replies to it.

    This corresponds to the \c {pubsub#itemreply} field.
*/
std::optional<QXmppPubSubNodeConfig::ItemPublisher> QXmppPubSubNodeConfig::notificationItemPublisher() const
{
    return d->notificationItemPublisher;
}

/*!
    Sets who is shown as the publisher of items to \a notificationItemPublisher.

    \sa notificationItemPublisher()
*/
void QXmppPubSubNodeConfig::setNotificationItemPublisher(std::optional<ItemPublisher> notificationItemPublisher)
{
    d->notificationItemPublisher = notificationItemPublisher;
}

/*!
    Returns the default language of the node.

    This corresponds to the \c {pubsub#language} field.
*/
QString QXmppPubSubNodeConfig::language() const
{
    return d->language;
}

/*!
    Sets the default language of the node to \a language.

    \sa language()
*/
void QXmppPubSubNodeConfig::setLanguage(const QString &language)
{
    d->language = language;
}

/*!
    Returns the maximum number of items the node keeps.

    The limit is either a number, Max for the maximum supported by the service or Unset.

    This corresponds to the \c {pubsub#max_items} field.
*/
QXmppPubSubNodeConfig::ItemLimit QXmppPubSubNodeConfig::maxItems() const
{
    return d->maxItems;
}

/*!
    Sets the maximum number of items the node keeps to \a maxItems.

    \sa maxItems()
*/
void QXmppPubSubNodeConfig::setMaxItems(ItemLimit maxItems)
{
    d->maxItems = maxItems;
}

/*!
    Returns the maximum payload size in bytes.

    This corresponds to the \c {pubsub#max_payload_size} field.
*/
std::optional<quint32> QXmppPubSubNodeConfig::maxPayloadSize() const
{
    return d->maxPayloadSize;
}

/*!
    Sets the maximum payload size in bytes to \a maxPayloadSize.

    \sa maxPayloadSize()
*/
void QXmppPubSubNodeConfig::setMaxPayloadSize(std::optional<quint32> maxPayloadSize)
{
    d->maxPayloadSize = maxPayloadSize;
}

/*!
    Returns the type of the node.

    Whether the node is a leaf node (the default) or a collection node.

    This corresponds to the \c {pubsub#node_type} field.
*/
std::optional<QXmppPubSubNodeConfig::NodeType> QXmppPubSubNodeConfig::nodeType() const
{
    return d->nodeType;
}

/*!
    Sets the type of the node to \a nodeType.

    \sa nodeType()
*/
void QXmppPubSubNodeConfig::setNodeType(std::optional<NodeType> nodeType)
{
    d->nodeType = nodeType;
}

/*!
    Returns the message type of event notifications.

    This corresponds to the \c {pubsub#notification_type} field.
*/
std::optional<QXmppPubSubNodeConfig::NotificationType> QXmppPubSubNodeConfig::notificationType() const
{
    return d->notificationType;
}

/*!
    Sets the message type of event notifications to \a notificationType.

    \sa notificationType()
*/
void QXmppPubSubNodeConfig::setNotificationType(std::optional<QXmppPubSubNodeConfig::NotificationType> notificationType)
{
    d->notificationType = notificationType;
}

/*!
    Returns whether subscribers are notified when the node configuration changes.

    This corresponds to the \c {pubsub#notify_config} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::configNotificationsEnabled() const
{
    return d->configNotificationsEnabled;
}

/*!
    Sets whether subscribers are notified when the node configuration changes (\a
    configNotificationsEnabled).

    \sa configNotificationsEnabled()
*/
void QXmppPubSubNodeConfig::setConfigNotificationsEnabled(std::optional<bool> configNotificationsEnabled)
{
    d->configNotificationsEnabled = configNotificationsEnabled;
}

/*!
    Returns whether subscribers are notified when the node is deleted.

    This corresponds to the \c {pubsub#notify_delete} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::deleteNotificationsEnabled() const
{
    return d->deleteNotificationsEnabled;
}

/*!
    Sets whether subscribers are notified when the node is deleted (\a
    nodeDeleteNotificationsEnabled).

    \sa deleteNotificationsEnabled()
*/
void QXmppPubSubNodeConfig::setDeleteNotificationsEnabled(std::optional<bool> nodeDeleteNotificationsEnabled)
{
    d->deleteNotificationsEnabled = nodeDeleteNotificationsEnabled;
}

/*!
    Returns whether subscribers are notified when items are removed from the node.

    This corresponds to the \c {pubsub#notify_retract} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::retractNotificationsEnabled() const
{
    return d->retractNotificationsEnabled;
}

/*!
    Sets whether subscribers are notified when items are removed from the node (\a
    retractNotificationsEnabled).

    \sa retractNotificationsEnabled()
*/
void QXmppPubSubNodeConfig::setRetractNotificationsEnabled(std::optional<bool> retractNotificationsEnabled)
{
    d->retractNotificationsEnabled = retractNotificationsEnabled;
}

/*!
    Returns whether owners are notified about new subscribers and unsubscribes.

    This corresponds to the \c {pubsub#notify_sub} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::subNotificationsEnabled() const
{
    return d->subNotificationsEnabled;
}

/*!
    Sets whether owners are notified about new subscribers and unsubscribes (\a
    subNotificationsEnabled).

    \sa subNotificationsEnabled()
*/
void QXmppPubSubNodeConfig::setSubNotificationsEnabled(std::optional<bool> subNotificationsEnabled)
{
    d->subNotificationsEnabled = subNotificationsEnabled;
}

/*!
    Returns whether items are persisted in the node.

    This corresponds to the \c {pubsub#persist_items} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::persistItems() const
{
    return d->persistItems;
}

/*!
    Sets whether items are persisted in the node (\a persistItems).

    \sa persistItems()
*/
void QXmppPubSubNodeConfig::setPersistItems(std::optional<bool> persistItems)
{
    d->persistItems = persistItems;
}

/*!
    Returns whether event notifications are only delivered to available users.

    This corresponds to the \c {pubsub#presence_based_delivery} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::presenceBasedNotifications() const
{
    return d->presenceBasedNotifications;
}

/*!
    Sets whether event notifications are only delivered to available users (\a
    presenceBasedNotifications).

    \sa presenceBasedNotifications()
*/
void QXmppPubSubNodeConfig::setPresenceBasedNotifications(std::optional<bool> presenceBasedNotifications)
{
    d->presenceBasedNotifications = presenceBasedNotifications;
}

/*!
    Returns the publish model of the node.

    The publish model defines who is allowed to publish items to the node.

    This corresponds to the \c {pubsub#publish_model} field.
*/
std::optional<QXmppPubSubNodeConfig::PublishModel> QXmppPubSubNodeConfig::publishModel() const
{
    return d->publishModel;
}

/*!
    Sets the publish model of the node to \a publishModel.

    \sa publishModel()
*/
void QXmppPubSubNodeConfig::setPublishModel(std::optional<PublishModel> publishModel)
{
    d->publishModel = publishModel;
}

/*!
    Returns whether all items are purged when the publisher goes offline.

    This corresponds to the \c {pubsub#purge_offline} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::purgeWhenOffline() const
{
    return d->purgeWhenOffline;
}

/*!
    Sets whether all items are purged when the publisher goes offline (\a purgeWhenOffline).

    \sa purgeWhenOffline()
*/
void QXmppPubSubNodeConfig::setPurgeWhenOffline(std::optional<bool> purgeWhenOffline)
{
    d->purgeWhenOffline = purgeWhenOffline;
}

/*!
    Returns the roster groups that are allowed to subscribe and retrieve items.

    Only used with the AccessModel::Roster access model.

    This corresponds to the \c {pubsub#roster_groups_allowed} field.
*/
QStringList QXmppPubSubNodeConfig::allowedRosterGroups() const
{
    return d->allowedRosterGroups;
}

/*!
    Sets the roster groups that are allowed to subscribe and retrieve items to \a
    allowedRosterGroups.

    \sa allowedRosterGroups()
*/
void QXmppPubSubNodeConfig::setAllowedRosterGroups(const QStringList &allowedRosterGroups)
{
    d->allowedRosterGroups = allowedRosterGroups;
}

/*!
    Returns when the last published item is sent to subscribers.

    This corresponds to the \c {pubsub#send_last_published_item} field.
*/
std::optional<QXmppPubSubNodeConfig::SendLastItemType> QXmppPubSubNodeConfig::sendLastItem() const
{
    return d->sendLastItem;
}

/*!
    Sets when the last published item is sent to subscribers to \a sendLastItem.

    \sa sendLastItem()
*/
void QXmppPubSubNodeConfig::setSendLastItem(std::optional<QXmppPubSubNodeConfig::SendLastItemType> sendLastItem)
{
    d->sendLastItem = sendLastItem;
}

/*!
    Returns whether all subscriptions are temporary.

    Temporary subscriptions are based on the presence of the subscriber.

    This corresponds to the \c {pubsub#tempsub} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::temporarySubscriptions() const
{
    return d->temporarySubscriptions;
}

/*!
    Sets whether all subscriptions are temporary (\a temporarySubscriptions).

    \sa temporarySubscriptions()
*/
void QXmppPubSubNodeConfig::setTemporarySubscriptions(std::optional<bool> temporarySubscriptions)
{
    d->temporarySubscriptions = temporarySubscriptions;
}

/*!
    Returns whether subscriptions to the node are allowed.

    This corresponds to the \c {pubsub#subscribe} field.
*/
std::optional<bool> QXmppPubSubNodeConfig::allowSubscriptions() const
{
    return d->allowSubscriptions;
}

/*!
    Sets whether subscriptions to the node are allowed (\a allowSubscriptions).

    \sa allowSubscriptions()
*/
void QXmppPubSubNodeConfig::setAllowSubscriptions(std::optional<bool> allowSubscriptions)
{
    d->allowSubscriptions = allowSubscriptions;
}

/*!
    Returns the human readable name of the node.

    This corresponds to the \c {pubsub#title} field.
*/
QString QXmppPubSubNodeConfig::title() const
{
    return d->title;
}

/*!
    Sets the human readable name of the node to \a title.

    \sa title()
*/
void QXmppPubSubNodeConfig::setTitle(const QString &title)
{
    d->title = title;
}

/*!
    Returns the type of the payload of the node's items.

    This is usually the namespace of the payload elements.

    This corresponds to the \c {pubsub#type} field.
*/
QString QXmppPubSubNodeConfig::payloadType() const
{
    return d->payloadType;
}

/*!
    Sets the type of the payload of the node's items to \a payloadType.

    \sa payloadType()
*/
void QXmppPubSubNodeConfig::setPayloadType(const QString &payloadType)
{
    d->payloadType = payloadType;
}

QString QXmppPubSubNodeConfig::formType() const
{
    return NODE_CONFIG_FORM_TYPE;
}

bool QXmppPubSubNodeConfig::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == ACCESS_MODEL) {
        d->accessModel = Enums::fromString<AccessModel>(field.value().toString());
    } else if (key == BODY_XSLT) {
        d->bodyXslt = value.toString();
    } else if (key == CHILD_ASSOCIATION_POLICY) {
        d->childAssociationPolicy = Enums::fromString<ChildAssociationPolicy>(value.toString());
    } else if (key == CHILD_ASSOCIATION_ALLOWLIST) {
        d->childAssociationAllowlist = value.toStringList();
    } else if (key == CHILD_NODES) {
        d->childNodes = value.toStringList();
    } else if (key == CHILD_NODES_MAX) {
        d->childNodesMax = parseUInt(value);
    } else if (key == COLLECTIONS) {
        d->collections = value.toStringList();
    } else if (key == CONTACT_JIDS) {
        d->contactJids = value.toStringList();
    } else if (key == DATA_FORM_XSLT) {
        d->dataFormXslt = value.toString();
    } else if (key == NOTIFICATIONS_ENABLED) {
        d->notificationsEnabled = parseBool(value);
    } else if (key == INCLUDE_PAYLOADS) {
        d->includePayloads = parseBool(value);
    } else if (key == DESCRIPTION) {
        d->description = value.toString();
    } else if (key == ITEM_EXPIRY) {
        d->itemExpiry = parseUInt(value);
    } else if (key == NOTIFICATION_ITEM_PUBLISHER) {
        d->notificationItemPublisher = Enums::fromString<ItemPublisher>(value.toString());
    } else if (key == LANGUAGE) {
        d->language = value.toString();
    } else if (key == MAX_ITEMS) {
        bool ok = false;
        if (const auto maxItems = value.toULongLong(&ok); ok) {
            d->maxItems = maxItems;
        } else if (value.typeId() == QMetaType::Type::QString && value.toString() == u"max") {
            d->maxItems = Max();
        } else {
            d->maxItems = Unset();
        }
    } else if (key == MAX_PAYLOAD_SIZE) {
        d->maxPayloadSize = parseUInt(value);
    } else if (key == NODE_TYPE) {
        d->nodeType = Enums::fromString<NodeType>(value.toString());
    } else if (key == NOTIFICATION_TYPE) {
        d->notificationType = Enums::fromString<NotificationType>(value.toString());
    } else if (key == CONFIG_NOTIFICATIONS_ENABLED) {
        d->configNotificationsEnabled = parseBool(value);
    } else if (key == NODE_DELETE_NOTIFICATIONS_ENABLED) {
        d->deleteNotificationsEnabled = parseBool(value);
    } else if (key == RETRACT_NOTIFICATIONS_ENABLED) {
        d->retractNotificationsEnabled = parseBool(value);
    } else if (key == SUB_NOTIFICATIONS_ENABLED) {
        d->subNotificationsEnabled = parseBool(value);
    } else if (key == PERSIST_ITEMS) {
        d->persistItems = parseBool(value);
    } else if (key == PRESENCE_BASED_NOTIFICATIONS) {
        d->presenceBasedNotifications = parseBool(value);
    } else if (key == PUBLISH_MODEL) {
        d->publishModel = Enums::fromString<PublishModel>(value.toString());
    } else if (key == PURGE_WHEN_OFFLINE) {
        d->purgeWhenOffline = parseBool(value);
    } else if (key == ALLOWED_ROSTER_GROUPS) {
        d->allowedRosterGroups = value.toStringList();
    } else if (key == SEND_LAST_ITEM) {
        d->sendLastItem = Enums::fromString<SendLastItemType>(value.toString());
    } else if (key == TEMPORARY_SUBSCRIPTIONS) {
        d->temporarySubscriptions = parseBool(value);
    } else if (key == ALLOW_SUBSCRIPTIONS) {
        d->allowSubscriptions = parseBool(value);
    } else if (key == TITLE) {
        d->title = value.toString();
    } else if (key == PAYLOAD_TYPE) {
        d->payloadType = value.toString();
    } else {
        return false;
    }
    return true;
}

void QXmppPubSubNodeConfig::serializeForm(QXmppDataForm &form) const
{
    using Type = QXmppDataForm::Field::Type;

    auto enumToString = [](auto value) {
        return Enums::toString(value);
    };

    serializeOptional(form,
                      Type::ListSingleField,
                      ACCESS_MODEL,
                      d->accessModel, enumToString);
    serializeNullable(form,
                      Type::TextSingleField,
                      BODY_XSLT,
                      d->bodyXslt);
    serializeOptional(form,
                      Type::ListSingleField,
                      CHILD_ASSOCIATION_POLICY,
                      d->childAssociationPolicy, enumToString);
    serializeEmptyable(form,
                       Type::TextMultiField,
                       CHILD_ASSOCIATION_ALLOWLIST,
                       d->childAssociationAllowlist);
    serializeEmptyable(form,
                       Type::TextMultiField,
                       CHILD_NODES,
                       d->childNodes);
    serializeOptionalNumber(form, Type::TextSingleField,
                            CHILD_NODES_MAX,
                            d->childNodesMax);
    serializeEmptyable(form,
                       Type::TextMultiField,
                       COLLECTIONS,
                       d->collections);
    serializeEmptyable(form,
                       Type::JidMultiField,
                       CONTACT_JIDS,
                       d->contactJids);
    serializeNullable(form,
                      Type::TextSingleField,
                      DATA_FORM_XSLT,
                      d->dataFormXslt);
    serializeOptional(form,
                      Type::BooleanField,
                      NOTIFICATIONS_ENABLED,
                      d->notificationsEnabled);
    serializeOptional(form,
                      Type::BooleanField,
                      INCLUDE_PAYLOADS,
                      d->includePayloads);
    serializeNullable(form,
                      Type::TextSingleField,
                      DESCRIPTION,
                      d->description);
    serializeOptionalNumber(form,
                            Type::TextSingleField,
                            ITEM_EXPIRY,
                            d->itemExpiry);
    serializeOptional(form,
                      Type::ListSingleField,
                      NOTIFICATION_ITEM_PUBLISHER,
                      d->notificationItemPublisher, enumToString);
    serializeNullable(form,
                      Type::TextSingleField,
                      LANGUAGE,
                      d->language);
    std::visit(overloaded {
                   [](Unset) {},
                   [&](uint64_t value) {
                       serializeValue(form, Type::TextSingleField, MAX_ITEMS, QString::number(value));
                   },
                   [&](Max) {
                       serializeValue(form, Type::TextSingleField, MAX_ITEMS, u"max"_s);
                   } },
               d->maxItems);
    serializeOptionalNumber(form,
                            Type::TextSingleField,
                            MAX_PAYLOAD_SIZE,
                            d->maxPayloadSize);
    serializeOptional(form,
                      Type::ListSingleField,
                      NODE_TYPE,
                      d->nodeType, enumToString);
    serializeOptional(form,
                      Type::ListSingleField,
                      NOTIFICATION_TYPE,
                      d->notificationType, enumToString);
    serializeOptional(form,
                      Type::BooleanField,
                      CONFIG_NOTIFICATIONS_ENABLED,
                      d->configNotificationsEnabled);
    serializeOptional(form,
                      Type::BooleanField,
                      NODE_DELETE_NOTIFICATIONS_ENABLED,
                      d->deleteNotificationsEnabled);
    serializeOptional(form,
                      Type::BooleanField,
                      RETRACT_NOTIFICATIONS_ENABLED,
                      d->retractNotificationsEnabled);
    serializeOptional(form,
                      Type::BooleanField,
                      SUB_NOTIFICATIONS_ENABLED,
                      d->subNotificationsEnabled);
    serializeOptional(form,
                      Type::BooleanField,
                      PERSIST_ITEMS,
                      d->persistItems);
    serializeOptional(form,
                      Type::BooleanField,
                      PRESENCE_BASED_NOTIFICATIONS,
                      d->presenceBasedNotifications);
    serializeOptional(form,
                      Type::ListSingleField,
                      PUBLISH_MODEL,
                      d->publishModel, enumToString);
    serializeOptional(form,
                      Type::BooleanField,
                      PURGE_WHEN_OFFLINE,
                      d->purgeWhenOffline);
    serializeEmptyable(form,
                       Type::ListMultiField,
                       ALLOWED_ROSTER_GROUPS,
                       d->allowedRosterGroups);
    serializeOptional(form,
                      Type::ListSingleField,
                      SEND_LAST_ITEM,
                      d->sendLastItem, enumToString);
    serializeOptional(form,
                      Type::BooleanField,
                      TEMPORARY_SUBSCRIPTIONS,
                      d->temporarySubscriptions);
    serializeOptional(form,
                      Type::BooleanField,
                      ALLOW_SUBSCRIPTIONS,
                      d->allowSubscriptions);
    serializeNullable(form,
                      Type::TextSingleField,
                      TITLE,
                      d->title);
    serializeNullable(form,
                      Type::TextSingleField,
                      PAYLOAD_TYPE,
                      d->payloadType);
}

/*!
    \class QXmppPubSubPublishOptions
    \inmodule QXmpp

    \brief The QXmppPubSubPublishOptions class represents the publish options of a PubSub
    publish request as defined by \xep{0060}{Publish-Subscribe}.

    Publish options are preconditions for publishing an item: the service only publishes the
    item if the node's configuration matches the options (or, if the node does not exist yet,
    creates it with these options). They use the same fields as QXmppPubSubNodeConfig, but are
    exchanged with the form type \c {http://jabber.org/protocol/pubsub#publish-options}.

    \sa QXmppPubSubManager::publishItem()

    \since QXmpp 1.5

    \ingroup qxmpp-stanzas
*/

QString QXmppPubSubPublishOptions::formType() const
{
    return PUBLISH_OPTIONS_FORM_TYPE;
}

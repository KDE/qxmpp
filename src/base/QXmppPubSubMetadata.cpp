// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPubSubMetadata.h"

#include "QXmppPubSubNodeConfig_p.h"
#include "QXmppVisitHelper_p.h"

#include "StringLiterals.h"

#include <QDateTime>

using namespace QXmpp::Private;

constexpr QStringView FORM_TYPE_METADATA = u"http://jabber.org/protocol/pubsub#metadata";

constexpr QStringView CONTACT_JIDS = u"pubsub#contact";
constexpr QStringView CREATION_DATE = u"pubsub#creation_date";
constexpr QStringView CREATOR_JID = u"pubsub#creator";
constexpr QStringView DESCRIPTION = u"pubsub#description";
constexpr QStringView LANGUAGE = u"pubsub#language";
constexpr QStringView ACCESS_MODEL = u"pubsub#access_model";
constexpr QStringView PUBLISH_MODEL = u"pubsub#publish_model";
constexpr QStringView SUBSCRIBER_COUNT = u"pubsub#num_subscribers";
constexpr QStringView OWNER_JIDS = u"pubsub#owner";
constexpr QStringView PUBLISHER_JIDS = u"pubsub#publisher";
constexpr QStringView TITLE = u"pubsub#title";
constexpr QStringView TYPE = u"pubsub#type";
constexpr QStringView MAX_ITEMS = u"pubsub#max_items";

class QXmppPubSubMetadataPrivate : public QSharedData
{
public:
    QStringList contactJids;
    QDateTime creationDate;
    QString creatorJid;
    QString description;
    QString language;
    std::optional<QXmppPubSubNodeConfig::AccessModel> accessModel;
    std::optional<QXmppPubSubNodeConfig::PublishModel> publishModel;
    std::optional<quint64> subscriberCount;
    QStringList ownerJids;
    QStringList publisherJids;
    QString title;
    QString type;
    QXmppPubSubMetadata::ItemLimit maxItems = QXmppPubSubMetadata::Unset();
};

/*!
    \class QXmppPubSubMetadata
    \inmodule QXmpp

    \brief The QXmppPubSubMetadata class represents the metadata of a PubSub node as defined by
    \xep{0060}{Publish-Subscribe}.

    Node metadata is provided by the service as a data form in the service discovery
    information of a node.

    All fields are optional: \c std::nullopt, invalid dates, empty strings and empty lists mean
    that a field is not set and it is not included in the serialized form.

    \sa QXmppPubSubNodeConfig

    \since QXmpp 1.5

    \ingroup qxmpp-stanzas
*/

/*!
    \typealias QXmppPubSubMetadata::ItemLimit

    Maximum number of items of a node: either a number, Max for the maximum supported by the
    service or Unset if no limit has been set.
*/

/*! Constructs empty node metadata. */
QXmppPubSubMetadata::QXmppPubSubMetadata()
    : d(new QXmppPubSubMetadataPrivate)
{
}

/*! Default copy-constructor */
QXmppPubSubMetadata::QXmppPubSubMetadata(const QXmppPubSubMetadata &) = default;
/*! Default move-constructor */
QXmppPubSubMetadata::QXmppPubSubMetadata(QXmppPubSubMetadata &&) = default;
QXmppPubSubMetadata::~QXmppPubSubMetadata() = default;
/*! Default assignment operator */
QXmppPubSubMetadata &QXmppPubSubMetadata::operator=(const QXmppPubSubMetadata &) = default;
/*! Default move-assignment operator */
QXmppPubSubMetadata &QXmppPubSubMetadata::operator=(QXmppPubSubMetadata &&) = default;

/*!
    Returns the JIDs of the entities to contact with questions about the node.

    This corresponds to the \c {pubsub#contact} field.
*/
QStringList QXmppPubSubMetadata::contactJids() const
{
    return d->contactJids;
}

/*!
    Sets the JIDs of the entities to contact with questions about the node to \a contactJids.

    \sa contactJids()
*/
void QXmppPubSubMetadata::setContactJids(const QStringList &contactJids)
{
    d->contactJids = contactJids;
}

/*!
    Returns the date and time when the node was created.

    This corresponds to the \c {pubsub#creation_date} field.
*/
QDateTime QXmppPubSubMetadata::creationDate() const
{
    return d->creationDate;
}

/*!
    Sets the date and time when the node was created to \a creationDate.

    \sa creationDate()
*/
void QXmppPubSubMetadata::setCreationDate(const QDateTime &creationDate)
{
    d->creationDate = creationDate;
}

/*!
    Returns the JID of the entity that created the node.

    This corresponds to the \c {pubsub#creator} field.
*/
QString QXmppPubSubMetadata::creatorJid() const
{
    return d->creatorJid;
}

/*!
    Sets the JID of the entity that created the node to \a creatorJid.

    \sa creatorJid()
*/
void QXmppPubSubMetadata::setCreatorJid(const QString &creatorJid)
{
    d->creatorJid = creatorJid;
}

/*!
    Returns the description of the node.

    This corresponds to the \c {pubsub#description} field.
*/
QString QXmppPubSubMetadata::description() const
{
    return d->description;
}

/*!
    Sets the description of the node to \a description.

    \sa description()
*/
void QXmppPubSubMetadata::setDescription(const QString &description)
{
    d->description = description;
}

/*!
    Returns the default language of the node.

    This corresponds to the \c {pubsub#language} field.
*/
QString QXmppPubSubMetadata::language() const
{
    return d->language;
}

/*!
    Sets the default language of the node to \a language.

    \sa language()
*/
void QXmppPubSubMetadata::setLanguage(const QString &language)
{
    d->language = language;
}

/*!
    Returns the access model of the node.

    The access model defines who may subscribe to the node and retrieve its items.

    This corresponds to the \c {pubsub#access_model} field.
*/
std::optional<QXmppPubSubNodeConfig::AccessModel> QXmppPubSubMetadata::accessModel() const
{
    return d->accessModel;
}

/*!
    Sets the access model of the node to \a accessModel.

    \sa accessModel()
*/
void QXmppPubSubMetadata::setAccessModel(std::optional<QXmppPubSubNodeConfig::AccessModel> accessModel)
{
    d->accessModel = accessModel;
}

/*!
    Returns the publish model of the node.

    The publish model defines who is allowed to publish items to the node.

    This corresponds to the \c {pubsub#publish_model} field.
*/
std::optional<QXmppPubSubNodeConfig::PublishModel> QXmppPubSubMetadata::publishModel() const
{
    return d->publishModel;
}

/*!
    Sets the publish model of the node to \a publishModel.

    \sa publishModel()
*/
void QXmppPubSubMetadata::setPublishModel(std::optional<QXmppPubSubNodeConfig::PublishModel> publishModel)
{
    d->publishModel = publishModel;
}

/*!
    Returns the number of subscribers to the node.

    This corresponds to the \c {pubsub#num_subscribers} field.
*/
std::optional<quint64> QXmppPubSubMetadata::numberOfSubscribers() const
{
    return d->subscriberCount;
}

/*!
    Sets the number of subscribers to the node to \a numberOfSubscribers.

    \sa numberOfSubscribers()
*/
void QXmppPubSubMetadata::setNumberOfSubscribers(const std::optional<quint64> &numberOfSubscribers)
{
    d->subscriberCount = numberOfSubscribers;
}

/*!
    Returns the JIDs of the owners of the node.

    This corresponds to the \c {pubsub#owner} field.
*/
QStringList QXmppPubSubMetadata::ownerJids() const
{
    return d->ownerJids;
}

/*!
    Sets the JIDs of the owners of the node to \a ownerJids.

    \sa ownerJids()
*/
void QXmppPubSubMetadata::setOwnerJids(const QStringList &ownerJids)
{
    d->ownerJids = ownerJids;
}

/*!
    Returns the JIDs of the entities that may publish to the node.

    This corresponds to the \c {pubsub#publisher} field.
*/
QStringList QXmppPubSubMetadata::publisherJids() const
{
    return d->publisherJids;
}

/*!
    Sets the JIDs of the entities that may publish to the node to \a publisherJids.

    \sa publisherJids()
*/
void QXmppPubSubMetadata::setPublisherJids(const QStringList &publisherJids)
{
    d->publisherJids = publisherJids;
}

/*!
    Returns the human readable name of the node.

    This corresponds to the \c {pubsub#title} field.
*/
QString QXmppPubSubMetadata::title() const
{
    return d->title;
}

/*!
    Sets the human readable name of the node to \a title.

    \sa title()
*/
void QXmppPubSubMetadata::setTitle(const QString &title)
{
    d->title = title;
}

/*!
    Returns the type of the payload of the node's items.

    This is usually the namespace of the payload elements.

    This corresponds to the \c {pubsub#type} field.
*/
QString QXmppPubSubMetadata::type() const
{
    return d->type;
}

/*!
    Sets the type of the payload of the node's items to \a type.

    \sa type()
*/
void QXmppPubSubMetadata::setType(const QString &type)
{
    d->type = type;
}

/*!
    Returns the maximum number of items the node keeps.

    The limit is either a number, Max for the maximum supported by the service or Unset.

    This corresponds to the \c {pubsub#max_items} field.
*/
auto QXmppPubSubMetadata::maxItems() const -> ItemLimit
{
    return d->maxItems;
}

/*!
    Sets the maximum number of items the node keeps to \a maxItems.

    \sa maxItems()
*/
void QXmppPubSubMetadata::setMaxItems(ItemLimit maxItems)
{
    d->maxItems = maxItems;
}

QString QXmppPubSubMetadata::formType() const
{
    return staticString(FORM_TYPE_METADATA);
}

bool QXmppPubSubMetadata::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == CONTACT_JIDS) {
        d->contactJids = value.toStringList();
    } else if (key == CREATION_DATE) {
        d->creationDate = QDateTime::fromString(field.value().toString(), Qt::ISODate).toUTC();
    } else if (key == CREATOR_JID) {
        d->creatorJid = value.toString();
    } else if (key == DESCRIPTION) {
        d->description = value.toString();
    } else if (key == LANGUAGE) {
        d->language = value.toString();
    } else if (key == ACCESS_MODEL) {
        d->accessModel = Enums::fromString<QXmppPubSubNodeConfig::AccessModel>(value.toString());
    } else if (key == PUBLISH_MODEL) {
        d->publishModel = Enums::fromString<QXmppPubSubNodeConfig::PublishModel>(value.toString());
    } else if (key == SUBSCRIBER_COUNT) {
        d->subscriberCount = parseULongLong(value);
    } else if (key == OWNER_JIDS) {
        d->ownerJids = value.toStringList();
    } else if (key == PUBLISHER_JIDS) {
        d->publisherJids = value.toStringList();
    } else if (key == TITLE) {
        d->title = value.toString();
    } else if (key == TYPE) {
        d->type = value.toString();
    } else if (key == MAX_ITEMS) {
        const auto string = value.toString();
        if (string == u"max") {
            d->maxItems = Max();
        } else {
            bool ok;
            if (const auto maxItems = field.value().toString().toULongLong(&ok); ok) {
                d->maxItems = maxItems;
            }
        }
    } else {
        return false;
    }

    return true;
}

void QXmppPubSubMetadata::serializeForm(QXmppDataForm &form) const
{
    using Type = QXmppDataForm::Field::Type;
    const auto numberToString = [](quint64 value) {
        return QString::number(value);
    };
    auto enumToString = [](auto value) {
        return Enums::toString(value);
    };

    serializeEmptyable(form, Type::JidMultiField, CONTACT_JIDS, d->contactJids);
    serializeDatetime(form, staticString(CREATION_DATE), d->creationDate);
    serializeNullable(form, Type::JidSingleField, CREATOR_JID, d->creatorJid);
    serializeNullable(form, Type::TextSingleField, DESCRIPTION, d->description);
    serializeNullable(form, Type::TextSingleField, LANGUAGE, d->language);
    serializeOptional(form, Type::ListSingleField, ACCESS_MODEL, d->accessModel, enumToString);
    serializeOptional(form, Type::ListSingleField, PUBLISH_MODEL, d->publishModel, enumToString);
    serializeOptional(form, Type::TextSingleField, SUBSCRIBER_COUNT, d->subscriberCount, numberToString);
    serializeEmptyable(form, Type::JidMultiField, OWNER_JIDS, d->ownerJids);
    serializeEmptyable(form, Type::JidMultiField, PUBLISHER_JIDS, d->publisherJids);
    serializeNullable(form, Type::TextSingleField, TITLE, d->title);
    serializeNullable(form, Type::TextSingleField, TYPE, d->type);
    serializeNullable(form, Type::TextSingleField, MAX_ITEMS,
                      std::visit(overloaded {
                                     [](Unset) { return QString(); },
                                     [](quint64 value) { return QString::number(value); },
                                     [](Max) { return u"max"_s; },
                                 },
                                 d->maxItems));
}

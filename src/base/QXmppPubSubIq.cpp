// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppDataForm.h"
#include "QXmppPubSubAffiliation.h"
#include "QXmppPubSubIq_p.h"
#include "QXmppPubSubSubscription.h"
#include "QXmppResultSet.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QSharedData>

using namespace QXmpp::Private;

/// \cond

///
/// \class QXmppPubSubIqBase
///
/// \brief The QXmppPubSubIqBase class is an abstract class used for parsing of
/// generic PubSub IQs as defined by \xep{0060, Publish-Subscribe}.
///
/// This class does not handle queries working with items. For a full-featured
/// PubSub-IQ, please use QXmppPubSubIq<T> with your needed item class.
///
/// \since QXmpp 1.5
///

///
/// \class QXmppPubSubIq
///
/// The QXmppPubSubIq class represents an IQ used for the publish-subscribe
/// mechanisms defined by \xep{0060, Publish-Subscribe}.
///
/// \ingroup Stanzas
///
/// \since QXmpp 1.5
///

///
/// \fn QXmppPubSubIq<T>::items()
///
/// Returns the IQ's items.
///

///
/// \fn QXmppPubSubIq<T>::setItems()
///
/// Sets the IQ's items.
///
/// \param items
///

///
/// \fn QXmppPubSubIq<T>::isPubSubIq()
///
/// Returns true, if the element is a valid PubSub IQ stanza. The payload of the
/// &lt;item/&gt; is also checked.
///

template<>
struct Enums::Data<PubSubIqBase::QueryType> {
    using enum PubSubIqBase::QueryType;
    static constexpr auto Values = makeValues<PubSubIqBase::QueryType>({
        { Affiliations, u"affiliations" },
        { OwnerAffiliations, u"affiliations" },
        { Configure, u"configure" },
        { Create, u"create" },
        { Default, u"default" },
        { OwnerDefault, u"default" },
        { Delete, u"delete" },
        { Items, u"items" },
        { Options, u"options" },
        { Publish, u"publish" },
        { Purge, u"purge" },
        { Retract, u"retract" },
        { Subscribe, u"subscribe" },
        { Subscription, u"subscription" },
        { Subscriptions, u"subscriptions" },
        { OwnerSubscriptions, u"subscriptions" },
        { Unsubscribe, u"unsubscribe" },
    });
};

namespace QXmpp::Private {

class PubSubIqPrivate : public QSharedData
{
public:
    PubSubIqBase::QueryType queryType = PubSubIqBase::Items;
    QString queryJid;
    QString queryNode;
    QString subscriptionId;
    QVector<QXmppPubSubSubscription> subscriptions;
    QVector<QXmppPubSubAffiliation> affiliations;
    uint32_t maxItems = 0;
    bool retractNotify = false;
    std::optional<QXmppDataForm> dataForm;
    std::optional<QXmppResultSetReply> itemsContinuation;
};

}  // namespace QXmpp::Private

///
/// Constructs a PubSub IQ.
///
PubSubIqBase::PubSubIqBase()
    : d(new PubSubIqPrivate)
{
}

/// Default copy-constructor
PubSubIqBase::PubSubIqBase(const PubSubIqBase &iq) = default;

PubSubIqBase::~PubSubIqBase() = default;

/// Default assignment operator
PubSubIqBase &PubSubIqBase::operator=(const PubSubIqBase &iq) = default;

///
/// Returns the PubSub query type for this IQ.
///
PubSubIqBase::QueryType PubSubIqBase::queryType() const
{
    return d->queryType;
}

///
/// Sets the PubSub query type for this IQ.
///
/// \param queryType
///
void PubSubIqBase::setQueryType(PubSubIqBase::QueryType queryType)
{
    d->queryType = queryType;
}

///
/// Returns the JID being queried.
///
QString PubSubIqBase::queryJid() const
{
    return d->queryJid;
}

///
/// Sets the JID being queried.
///
/// \param queryJid
///
void PubSubIqBase::setQueryJid(const QString &queryJid)
{
    d->queryJid = queryJid;
}

///
/// Returns the name of the node being queried.
///
QString PubSubIqBase::queryNode() const
{
    return d->queryNode;
}

///
/// Sets the name of the node being queried.
///
/// \param queryNodeName
///
void PubSubIqBase::setQueryNode(const QString &queryNodeName)
{
    d->queryNode = queryNodeName;
}

///
/// Returns the subscription ID for the request.
///
/// This does not work for SubscriptionQuery IQs, use subscription() instead.
///
QString PubSubIqBase::subscriptionId() const
{
    return d->subscriptionId;
}

///
/// Sets the subscription ID for the request.
///
/// This does not work for SubscriptionQuery IQs, use setSubscription() instead.
///
void PubSubIqBase::setSubscriptionId(const QString &subscriptionId)
{
    d->subscriptionId = subscriptionId;
}

///
/// Returns the included subscriptions.
///
QVector<QXmppPubSubSubscription> PubSubIqBase::subscriptions() const
{
    return d->subscriptions;
}

///
/// Sets the included subscriptions.
///
void PubSubIqBase::setSubscriptions(const QVector<QXmppPubSubSubscription> &subscriptions)
{
    d->subscriptions = subscriptions;
}

///
/// Returns the subscription.
///
/// This is a utility function for subscriptions(). It returns the first
/// subscription if existant. This can be used for both query types,
/// Subscription and Subscriptions.
///
std::optional<QXmppPubSubSubscription> PubSubIqBase::subscription() const
{
    if (d->subscriptions.isEmpty()) {
        return std::nullopt;
    }
    return d->subscriptions.first();
}

///
/// Sets the subscription.
///
/// This is a utility function for setSubscriptions(). It can be used for both
/// query types, Subscription and Subscriptions.
///
void PubSubIqBase::setSubscription(const std::optional<QXmppPubSubSubscription> &subscription)
{
    if (subscription) {
        d->subscriptions = { *subscription };
    } else {
        d->subscriptions.clear();
    }
}

///
/// Returns the included affiliations.
///
QVector<QXmppPubSubAffiliation> PubSubIqBase::affiliations() const
{
    return d->affiliations;
}

///
/// Sets the included affiliations.
///
void PubSubIqBase::setAffiliations(const QVector<QXmppPubSubAffiliation> &affiliations)
{
    d->affiliations = affiliations;
}

///
/// Returns the maximum of items that are requested.
///
/// This is only used for queries with type ItemsQuery.
///
std::optional<uint32_t> PubSubIqBase::maxItems() const
{
    if (d->maxItems) {
        return d->maxItems;
    }
    return std::nullopt;
}

///
/// Sets the maximum of items that are requested.
///
/// This is only used for queries with type ItemsQuery.
///
void PubSubIqBase::setMaxItems(std::optional<uint32_t> maxItems)
{
    d->maxItems = maxItems.value_or(0);
}

///
/// Returns whether to send notifications on retraction (default: false).
///
/// \since QXmpp 1.11
///
bool PubSubIqBase::retractNotify() const { return d->retractNotify; }

///
/// Sets whether to send notifications on retraction.
///
///  \since QXmpp 1.11
///
void PubSubIqBase::setRetractNotify(bool notify) { d->retractNotify = notify; }

///
/// Returns a data form if the IQ contains one.
///
std::optional<QXmppDataForm> PubSubIqBase::dataForm() const
{
    return d->dataForm;
}

///
/// Sets a data form (or clears it by setting std::nullopt).
///
void PubSubIqBase::setDataForm(const std::optional<QXmppDataForm> &dataForm)
{
    d->dataForm = dataForm;
}

///
/// Returns a description of which items have been returned.
///
/// If this value is set the results are incomplete.
///
std::optional<QXmppResultSetReply> PubSubIqBase::itemsContinuation() const
{
    return d->itemsContinuation;
}

///
/// Returns a description of which items have been returned.
///
/// If this value is set the results are incomplete.
///
void PubSubIqBase::setItemsContinuation(const std::optional<QXmppResultSetReply> &itemsContinuation)
{
    d->itemsContinuation = itemsContinuation;
}

bool PubSubIqBase::isPubSubIq(const QDomElement &element)
{
    // no special requirements for the item / it's payload
    return PubSubIqBase::isPubSubIq(element, [](const QDomElement &) {
        return true;
    });
}

bool PubSubIqBase::isPubSubIq(const QDomElement &element, bool (*isItemValid)(const QDomElement &))
{
    // IQs must have only one direct child element.
    const auto pubSubElement = element.firstChildElement();
    if (pubSubElement.tagName() != u"pubsub") {
        return false;
    }

    // check for correct namespace
    const bool isOwner = pubSubElement.namespaceURI() == ns_pubsub_owner;
    if (!isOwner && pubSubElement.namespaceURI() != ns_pubsub) {
        return false;
    }

    // check that the query type is valid
    auto queryElement = pubSubElement.firstChildElement();
    auto optionalType = queryTypeFromDomElement(queryElement);
    if (!optionalType) {
        return false;
    }
    auto queryType = *optionalType;

    // check for the "node" attribute
    switch (queryType) {
    case OwnerAffiliations:
    case Items:
    case Publish:
    case Retract:
    case Delete:
    case Purge:
        if (!queryElement.hasAttribute(u"node"_s)) {
            return false;
        }
    default:
        break;
    }

    // check for the "jid" attribute
    switch (queryType) {
    case Options:
    case OwnerSubscriptions:
    case Subscribe:
    case Unsubscribe:
        if (!queryElement.hasAttribute(u"jid"_s)) {
            return false;
        }
    default:
        break;
    }

    // check the individual content
    switch (queryType) {
    case Items:
    case Publish:
    case Retract:
        // check the items using isItemValid()
        for (const auto &itemElement : iterChildElements(queryElement, u"item")) {
            if (!isItemValid(itemElement)) {
                return false;
            }
        }
        break;
    case Subscription:
        if (!QXmppPubSubSubscription::isSubscription(queryElement)) {
            return false;
        }
        [[fallthrough]];
    case Delete:
    case Purge:
    case Configure:
        if (!isOwner) {
            return false;
        }
        break;
    case Affiliations:
    case OwnerAffiliations:
    case Create:
    case Default:
    case OwnerDefault:
    case Options:
    case Subscribe:
    case Subscriptions:
    case OwnerSubscriptions:
    case Unsubscribe:
        break;
    }
    return true;
}

void PubSubIqBase::parseElementFromChild(const QDomElement &element)
{
    const auto parseDataFormFromChild = [=](const QDomElement &element) -> std::optional<QXmppDataForm> {
        if (const auto subElement = firstChildElement(element, u"x", ns_data);
            !subElement.isNull()) {
            QXmppDataForm form;
            form.parse(subElement);
            return form;
        }
        return {};
    };

    const auto pubSubElement = element.firstChildElement(u"pubsub"_s);
    const auto queryElement = pubSubElement.firstChildElement();

    // parse query type
    if (auto type = queryTypeFromDomElement(queryElement)) {
        d->queryType = *type;
    } else {
        return;
    }

    // Subscription is special: The query element is directly handled by
    // QXmppPubSubSubscription.
    if (d->queryType == Subscription) {
        QXmppPubSubSubscription subscription;
        subscription.parse(queryElement);
        setSubscription(subscription);

        // form inside following <options/>
        d->dataForm = parseDataFormFromChild(firstChildElement(pubSubElement, u"options"));
        return;
    }

    d->queryJid = queryElement.attribute(u"jid"_s);
    d->queryNode = queryElement.attribute(u"node"_s);

    // retract notify
    if (d->queryType == Retract) {
        d->retractNotify = parseBoolean(queryElement.attribute(u"notify"_s)).value_or(false);
    }

    // parse subid
    switch (d->queryType) {
    case Items:
    case Unsubscribe:
    case Options:
        d->subscriptionId = queryElement.attribute(u"subid"_s);
    default:
        break;
    }

    // parse contents
    switch (d->queryType) {
    case Affiliations:
    case OwnerAffiliations:
        for (const auto &subElement : iterChildElements(queryElement, u"affiliation")) {
            if (QXmppPubSubAffiliation::isAffiliation(subElement)) {
                QXmppPubSubAffiliation affiliation;
                affiliation.parse(subElement);

                d->affiliations << std::move(affiliation);
            }
        }
        break;
    case Items:
        // Result Set Management (incomplete items result received)
        d->itemsContinuation = parseOptionalChildElement<QXmppResultSetReply>(pubSubElement);
        [[fallthrough]];
    case Publish:
    case Retract:
        parseItems(queryElement);

        if (d->queryType == Items) {
            d->maxItems = queryElement.attribute(u"max_items"_s).toUInt();
        } else if (d->queryType == Publish) {
            // form inside following <publish-options/>
            d->dataForm = parseDataFormFromChild(firstChildElement(pubSubElement, u"publish-options"));
        }

        break;
    case Subscriptions:
    case OwnerSubscriptions:
        for (const auto &subElement : iterChildElements(queryElement)) {
            if (QXmppPubSubSubscription::isSubscription(subElement)) {
                QXmppPubSubSubscription subscription;
                subscription.parse(subElement);

                d->subscriptions << std::move(subscription);
            }
        }
        break;
    case Configure:
    case Default:
    case OwnerDefault:
    case Options:
        // form in direct child <x/>
        d->dataForm = parseDataFormFromChild(queryElement);
        break;
    case Create:
        // form inside following <configure/>
        d->dataForm = parseDataFormFromChild(firstChildElement(pubSubElement, u"configure"));
        break;
    case Subscribe:
    case Subscription:
        // form inside following <options/>
        d->dataForm = parseDataFormFromChild(firstChildElement(pubSubElement, u"options"));
        break;
    case Delete:
    case Purge:
    case Unsubscribe:
        break;
    }
}

void PubSubIqBase::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);

    if (d->queryType == Subscription) {
        w.write(Element { { u"pubsub", ns_pubsub }, subscription().value_or(QXmppPubSubSubscription()) });
    } else {
        w.write(Element {
            { u"pubsub", queryTypeIsOwnerIq(d->queryType) ? ns_pubsub_owner : ns_pubsub },
            Element {
                d->queryType,
                OptionalAttribute { u"jid", d->queryJid },
                OptionalAttribute { u"node", d->queryNode },
                OptionalContent {
                    d->queryType == Retract && d->retractNotify,
                    Attribute { u"notify", true },
                },
                // subid
                [&] {
                    switch (d->queryType) {
                    case Items:
                    case Unsubscribe:
                    case Options:
                        w.write(OptionalAttribute { u"subid", d->subscriptionId });
                    default:
                        break;
                    }
                },
                // content
                [&] {
                    switch (d->queryType) {
                    case Affiliations:
                    case OwnerAffiliations:
                        w.write(d->affiliations);
                        break;
                    case Items:
                        if (d->maxItems > 0) {
                            w.write(Attribute { u"max_items", d->maxItems });
                        }
                        [[fallthrough]];
                    case Publish:
                    case Retract:
                        serializeItems(writer);
                        break;
                    case Subscriptions:
                    case OwnerSubscriptions:
                        w.write(d->subscriptions);
                        break;
                    case Configure:
                    case Default:
                    case OwnerDefault:
                    case Options:
                        if (auto form = d->dataForm) {
                            // make sure data form type is correct
                            switch (type()) {
                            case Result:
                                form->setType(QXmppDataForm::Result);
                                break;
                            default:
                                if (form->type() != QXmppDataForm::Cancel) {
                                    form->setType(QXmppDataForm::Submit);
                                }
                                break;
                            }
                            w.write(*form);
                        }
                        break;
                    case Create:
                    case Delete:
                    case Purge:
                    case Subscribe:
                    case Subscription:
                    case Unsubscribe:
                        break;
                    }
                },
            },
            // data form
            [&] {
                if (auto form = d->dataForm) {
                    // make sure form type is 'submit'
                    form->setType(type() == Result ? QXmppDataForm::Result : QXmppDataForm::Submit);

                    switch (d->queryType) {
                    case Create:
                        w.write(Element { u"configure", *form });
                        break;
                    case Publish:
                        w.write(Element { u"publish-options", *form });
                        break;
                    case Subscribe:
                    case Subscription:
                        w.write(Element { u"options", *form });
                        break;
                    default:
                        break;
                    }
                }
            },
            // Result Set Management
            OptionalContent { d->queryType == Items, d->itemsContinuation },
        });
    }
}

std::optional<PubSubIqBase::QueryType> PubSubIqBase::queryTypeFromDomElement(const QDomElement &element)
{
    QueryType type;
    if (auto queryType = Enums::fromString<QueryType>(element.tagName())) {
        type = *queryType;
    } else {
        return std::nullopt;
    }

    // Some queries can have ns_pubsub_owner and normal ns_pubsub. To
    // distinguish those after parsing those with ns_pubsub_owner are replaced
    // by another query type.

    if (element.namespaceURI() != ns_pubsub_owner) {
        return type;
    }

    switch (type) {
    case Affiliations:
        return OwnerAffiliations;
    case Default:
        return OwnerDefault;
    case Subscriptions:
        return OwnerSubscriptions;
    default:
        return type;
    }
}

bool PubSubIqBase::queryTypeIsOwnerIq(QueryType type)
{
    switch (type) {
    case OwnerAffiliations:
    case OwnerSubscriptions:
    case OwnerDefault:
    case Configure:
    case Delete:
    case Purge:
        return true;
    case Affiliations:
    case Create:
    case Default:
    case Items:
    case Options:
    case Publish:
    case Retract:
    case Subscribe:
    case Subscription:
    case Subscriptions:
    case Unsubscribe:
        return false;
    }
    return false;
}

/// \endcond

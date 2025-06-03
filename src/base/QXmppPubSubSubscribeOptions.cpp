// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPubSubSubscribeOptions.h"

#include "Algorithms.h"
#include "Enums.h"
#include "StringLiterals.h"

#include <QDateTime>

using namespace QXmpp::Private;

const auto SUBSCRIBE_OPTIONS_FORM_TYPE = u"http://jabber.org/protocol/pubsub#subscribe_options"_s;

const auto NOTIFICATIONS_ENABLED = u"pubsub#deliver"_s;
const auto DIGESTS_ENABLED = u"pubsub#digest"_s;
const auto DIGEST_FREQUENCY_MS = u"pubsub#digest_frequency"_s;
const auto BODY_INCLUDED = u"pubsub#include_body"_s;
const auto EXPIRE = u"pubsub#expire"_s;
const auto NOTIFICATION_RULES = u"pubsub#show-values"_s;
const auto SUBSCRIPTION_TYPE = u"pubsub#subscription_type"_s;
const auto SUBSCRIPTION_DEPTH = u"pubsub#subscription_depth"_s;

using PresenceState = QXmppPubSubSubscribeOptions::PresenceState;

template<>
struct Enums::Data<PresenceState> {
    using enum PresenceState;
    static constexpr bool IsFlags = true;
    static constexpr auto Values = makeValues<PresenceState>({
        { Away, u"away" },
        { Chat, u"chat" },
        { DoNotDisturb, u"dnd" },
        { Online, u"online" },
        { ExtendedAway, u"xa" },
    });
};

template<>
struct Enums::Data<QXmppPubSubSubscribeOptions::SubscriptionType> {
    using enum QXmppPubSubSubscribeOptions::SubscriptionType;
    static constexpr auto Values = makeValues<QXmppPubSubSubscribeOptions::SubscriptionType>({
        { Items, u"items" },
        { Nodes, u"nodes" },
    });
};

template<>
struct Enums::Data<QXmppPubSubSubscribeOptions::SubscriptionDepth> {
    using enum QXmppPubSubSubscribeOptions::SubscriptionDepth;
    static constexpr auto Values = makeValues<QXmppPubSubSubscribeOptions::SubscriptionDepth>({
        { TopLevelOnly, u"1" },
        { Recursive, u"all" },
    });
};

class QXmppPubSubSubscribeOptionsPrivate : public QSharedData
{
public:
    std::optional<bool> notificationsEnabled;
    std::optional<bool> digestsEnabled;
    std::optional<quint32> digestFrequencyMs;
    std::optional<bool> bodyIncluded;
    QDateTime expire;
    QXmppPubSubSubscribeOptions::PresenceStates notificationRules;
    std::optional<QXmppPubSubSubscribeOptions::SubscriptionType> subscriptionType;
    std::optional<QXmppPubSubSubscribeOptions::SubscriptionDepth> subscriptionDepth;
};

std::optional<QXmppPubSubSubscribeOptions> QXmppPubSubSubscribeOptions::fromDataForm(const QXmppDataForm &form)
{
    if (form.formType() == SUBSCRIBE_OPTIONS_FORM_TYPE) {
        QXmppPubSubSubscribeOptions options;
        options.parseForm(form);
        return options;
    }
    return std::nullopt;
}

QXmppPubSubSubscribeOptions::QXmppPubSubSubscribeOptions()
    : d(new QXmppPubSubSubscribeOptionsPrivate())
{
}

QXmppPubSubSubscribeOptions::QXmppPubSubSubscribeOptions(const QXmppPubSubSubscribeOptions &) = default;
QXmppPubSubSubscribeOptions::QXmppPubSubSubscribeOptions(QXmppPubSubSubscribeOptions &&) = default;
QXmppPubSubSubscribeOptions::~QXmppPubSubSubscribeOptions() = default;
QXmppPubSubSubscribeOptions &QXmppPubSubSubscribeOptions::operator=(const QXmppPubSubSubscribeOptions &) = default;
QXmppPubSubSubscribeOptions &QXmppPubSubSubscribeOptions::operator=(QXmppPubSubSubscribeOptions &&) = default;

std::optional<bool> QXmppPubSubSubscribeOptions::notificationsEnabled() const
{
    return d->notificationsEnabled;
}

void QXmppPubSubSubscribeOptions::setNotificationsEnabled(std::optional<bool> enabled)
{
    d->notificationsEnabled = enabled;
}

std::optional<bool> QXmppPubSubSubscribeOptions::digestsEnabled() const
{
    return d->digestsEnabled;
}

void QXmppPubSubSubscribeOptions::setDigestsEnabled(std::optional<bool> digestsEnabled)
{
    d->digestsEnabled = digestsEnabled;
}

std::optional<quint32> QXmppPubSubSubscribeOptions::digestFrequencyMs() const
{
    return d->digestFrequencyMs;
}

void QXmppPubSubSubscribeOptions::setDigestFrequencyMs(std::optional<quint32> digestFrequencyMs)
{
    d->digestFrequencyMs = digestFrequencyMs;
}

QDateTime QXmppPubSubSubscribeOptions::expire() const
{
    return d->expire;
}

void QXmppPubSubSubscribeOptions::setExpire(const QDateTime &expire)
{
    d->expire = expire;
}

std::optional<bool> QXmppPubSubSubscribeOptions::bodyIncluded() const
{
    return d->bodyIncluded;
}

void QXmppPubSubSubscribeOptions::setBodyIncluded(std::optional<bool> bodyIncluded)
{
    d->bodyIncluded = bodyIncluded;
}

QXmppPubSubSubscribeOptions::PresenceStates QXmppPubSubSubscribeOptions::notificationRules() const
{
    return d->notificationRules;
}

void QXmppPubSubSubscribeOptions::setNotificationRules(PresenceStates notificationRules)
{
    d->notificationRules = notificationRules;
}

std::optional<QXmppPubSubSubscribeOptions::SubscriptionType> QXmppPubSubSubscribeOptions::subscriptionType() const
{
    return d->subscriptionType;
}

void QXmppPubSubSubscribeOptions::setSubscriptionType(std::optional<SubscriptionType> subscriptionType)
{
    d->subscriptionType = subscriptionType;
}

std::optional<QXmppPubSubSubscribeOptions::SubscriptionDepth> QXmppPubSubSubscribeOptions::subscriptionDepth() const
{
    return d->subscriptionDepth;
}

void QXmppPubSubSubscribeOptions::setSubscriptionDepth(std::optional<SubscriptionDepth> subscriptionDepth)
{
    d->subscriptionDepth = subscriptionDepth;
}

QString QXmppPubSubSubscribeOptions::formType() const
{
    return SUBSCRIBE_OPTIONS_FORM_TYPE;
}

bool QXmppPubSubSubscribeOptions::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    if (field.type() == QXmppDataForm::Field::Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == NOTIFICATIONS_ENABLED) {
        d->notificationsEnabled = parseBool(value);
    } else if (key == DIGESTS_ENABLED) {
        d->digestsEnabled = parseBool(value);
    } else if (key == DIGEST_FREQUENCY_MS) {
        d->digestFrequencyMs = parseUInt(value);
    } else if (key == BODY_INCLUDED) {
        d->bodyIncluded = parseBool(value);
    } else if (key == EXPIRE) {
        d->expire = QDateTime::fromString(value.toString(), Qt::ISODate);
    } else if (key == NOTIFICATION_RULES) {
        d->notificationRules = Enums::fromStrings<PresenceState>(value.toStringList());
    } else if (key == SUBSCRIPTION_TYPE) {
        d->subscriptionType = Enums::fromString<SubscriptionType>(value.toString());
    } else if (key == SUBSCRIPTION_DEPTH) {
        d->subscriptionDepth = Enums::fromString<SubscriptionDepth>(value.toString());
    } else {
        return false;
    }
    return true;
}

void QXmppPubSubSubscribeOptions::serializeForm(QXmppDataForm &form) const
{
    using Type = QXmppDataForm::Field::Type;

    const auto numberToString = [](quint32 value) {
        return QString::number(value);
    };
    auto enumToString = [](auto value) {
        return Enums::toString(value).toString();
    };

    serializeOptional(form, Type::BooleanField, NOTIFICATIONS_ENABLED, d->notificationsEnabled);
    serializeOptional(form, Type::BooleanField, DIGESTS_ENABLED, d->digestsEnabled);
    serializeOptional(form, Type::TextSingleField, DIGEST_FREQUENCY_MS, d->digestFrequencyMs, numberToString);
    serializeDatetime(form, EXPIRE, d->expire);
    serializeOptional(form, Type::BooleanField, BODY_INCLUDED, d->bodyIncluded);
    serializeEmptyable(form, Type::ListMultiField, NOTIFICATION_RULES, QStringList(Enums::toStringList(d->notificationRules)));
    serializeOptional(form, Type::ListSingleField, SUBSCRIPTION_TYPE, d->subscriptionType, enumToString);
    serializeOptional(form, Type::ListSingleField, SUBSCRIPTION_DEPTH, d->subscriptionDepth, enumToString);
}

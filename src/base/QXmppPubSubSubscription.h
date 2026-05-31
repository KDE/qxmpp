// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPUBSUBSUBSCRIPTION_H
#define QXMPPPUBSUBSUBSCRIPTION_H

#include "QXmppGlobal.h"

#include <QDateTime>
#include <QMetaType>
#include <QSharedDataPointer>

class QXmppPubSubSubscriptionPrivate;
class QXmlStreamWriter;
class QDomElement;

class QXMPP_EXPORT QXmppPubSubSubscription
{
public:
    /*!
        The State enum describes the state of a subscription.

        \value Invalid No state information is included.
        \value None There is no subscription with the node.
        \value Pending A subscription is pending.
        \value Subscribed The user is subscribed to the node.
        \value Unconfigured The subscription requires configuration before it becomes active.
    */
    enum State : uint8_t {
        Invalid,
        None,
        Pending,
        Subscribed,
        Unconfigured,
    };

    /*!
        The SubscribeOptionsSupport enum describes whether the availability of a
        subscription configuration. This is also known as
        &lt;subscribe-options/&gt;.

        \value Unavailable A subscription configuration is not advertised.
        \value Available Configuration of the subscription is possible, but not required.
        \value Required Configuration of the subscription is required. No event notifications are going to be sent until the subscription has been configured.
    */
    enum ConfigurationSupport : uint8_t {
        Unavailable,
        Available,
        Required,
    };

    QXmppPubSubSubscription(const QString &jid = {},
                            const QString &node = {},
                            const QString &subId = {},
                            State state = Invalid,
                            ConfigurationSupport configurationSupport = Unavailable,
                            const QDateTime &expiry = {});
    QXmppPubSubSubscription(const QXmppPubSubSubscription &);
    QXmppPubSubSubscription(QXmppPubSubSubscription &&);
    ~QXmppPubSubSubscription();

    QXmppPubSubSubscription &operator=(const QXmppPubSubSubscription &);
    QXmppPubSubSubscription &operator=(QXmppPubSubSubscription &&);

    QString jid() const;
    void setJid(const QString &jid);

    QString node() const;
    void setNode(const QString &node);

    QString subId() const;
    void setSubId(const QString &subId);

    QDateTime expiry() const;
    void setExpiry(const QDateTime &expiry);

    State state() const;
    void setState(State state);

    ConfigurationSupport configurationSupport() const;
    void setConfigurationSupport(ConfigurationSupport support);
    bool isConfigurationSupported() const;
    bool isConfigurationRequired() const;

    static bool isSubscription(const QDomElement &);

    void parse(const QDomElement &);
    void toXml(QXmlStreamWriter *writer) const;

private:
    QSharedDataPointer<QXmppPubSubSubscriptionPrivate> d;
};

Q_DECLARE_TYPEINFO(QXmppPubSubSubscription, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QXmppPubSubSubscription)
Q_DECLARE_METATYPE(QXmppPubSubSubscription::State)
Q_DECLARE_METATYPE(QXmppPubSubSubscription::ConfigurationSupport)

#endif  // QXMPPPUBSUBSUBSCRIPTION_H

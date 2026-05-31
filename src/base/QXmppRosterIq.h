// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPROSTERIQ_H
#define QXMPPROSTERIQ_H

#include "QXmppConstants_p.h"
#include "QXmppIq.h"

#include <QList>
#include <QSet>
#include <QSharedDataPointer>

namespace QXmpp::Private {
struct RosterData;
}

class QXmppRosterIqPrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppRosterIq class represents a roster IQ.

    \ingroup Stanzas
*/
class QXMPP_EXPORT QXmppRosterIq : public QXmppIq
{
public:
    class ItemPrivate;

    /*!
        \inmodule QXmpp

        \brief The QXmppRosterIq::Item class represents a roster entry.
    */
    class QXMPP_EXPORT Item
    {
    public:
        /*!
            An enumeration for type of subscription with the bareJid in the roster.

            \value None the user does not have a subscription to the contact's presence information, and the contact does not have a subscription to the user's presence information.
            \value From the contact has a subscription to the user's presence information, but the user does not have a subscription to the contact's presence information.
            \value To the user has a subscription to the contact's presence information, but the contact does not have a subscription to the user's presence information.
            \value Both both the user and the contact have subscriptions to each other's presence information.
            \value Remove to delete a roster item.
            \value NotSet the subscription state was not specified.
        */
        enum SubscriptionType {
            None = 0,
            From = 1,
            To = 2,
            Both = 3,
            Remove = 4,
            NotSet = 8
        };

        Item();
        Item(const Item &other);
        Item(Item &&);
        ~Item();

        Item &operator=(const Item &other);
        Item &operator=(Item &&);

        QString bareJid() const;
        QSet<QString> groups() const;
        QString name() const;
        QString subscriptionStatus() const;
        SubscriptionType subscriptionType() const;
        bool isApproved() const;

        void setBareJid(const QString &);
        void setGroups(const QSet<QString> &);
        void setName(const QString &);
        void setSubscriptionStatus(const QString &);
        void setSubscriptionType(SubscriptionType);
        void setIsApproved(bool);

        // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
        bool isMixChannel() const;
        void setIsMixChannel(bool);

        QString mixParticipantId() const;
        void setMixParticipantId(const QString &);

        static constexpr std::tuple XmlTag = { u"item", QXmpp::Private::ns_roster };
        void parse(const QDomElement &element);
        void toXml(QXmlStreamWriter *writer) const;

    private:
        friend struct QXmpp::Private::RosterData;

        void toXml(QXmlStreamWriter *writer, bool external) const;

        QString getSubscriptionTypeStr() const;
        void setSubscriptionTypeFromStr(const QString &);

        QSharedDataPointer<ItemPrivate> d;
    };

    QXmppRosterIq();
    QXmppRosterIq(const QXmppRosterIq &);
    QXmppRosterIq(QXmppRosterIq &&);
    ~QXmppRosterIq() override;

    QXmppRosterIq &operator=(const QXmppRosterIq &);
    QXmppRosterIq &operator=(QXmppRosterIq &&);

    QString version() const;
    void setVersion(const QString &);

    QList<Item> items() const;
    void setItems(const QList<Item> &);
    void addItem(const Item &);

    // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    bool mixAnnotate() const;
    void setMixAnnotate(bool);

    static constexpr std::tuple PayloadXmlTag = { u"query", QXmpp::Private::ns_roster };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isRosterIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppRosterIqPrivate> d;
};

#endif  // QXMPPROSTERIQ_H

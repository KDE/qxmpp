// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the PubSub (XEP-0060) parse/serialize tests. Merging
// tst_QXmppPubSub, tst_QXmppPubSubEvent, tst_QXmppPubSubForms and
// tst_QXmppPubSubIq into one translation unit parses the shared Qt/QXmpp headers
// once instead of four times. main() runs every test class in turn. Note:
// tst_QXmppPubSubIq comes last because its `using namespace QXmpp::Private` must
// not leak into the other sections.

#include "QXmppDataForm.h"
#include "QXmppDataFormBase.h"
#include "QXmppPubSubAffiliation.h"
#include "QXmppPubSubBaseItem.h"
#include "QXmppPubSubEvent.h"
#include "QXmppPubSubIq_p.h"
#include "QXmppPubSubSubAuthorization.h"
#include "QXmppPubSubSubscription.h"
#include "QXmppResultSet.h"

#include "pubsubutil.h"
#include "util.h"

#include <QCoreApplication>
#include <QObject>

// ===================== tst_QXmppPubSub =====================

using Affiliation = QXmppPubSubAffiliation;
using AffiliationType = QXmppPubSubAffiliation::Affiliation;
using Subscription = QXmppPubSubSubscription;
using SubscriptionConfig = QXmppPubSubSubscription::ConfigurationSupport;
using SubscriptionState = QXmppPubSubSubscription::State;

enum PubSubNamespace {
    PubSubNs,
    PubSubEventNs,
    PubSubOwnerNs,
};
Q_DECLARE_METATYPE(PubSubNamespace)

template<typename T>
void parsePacket(T &packet, const QByteArray &xml, PubSubNamespace xmlns)
{
    QByteArray newXml;
    switch (xmlns) {
    case PubSubNs:
        newXml = "<outer xmlns='http://jabber.org/protocol/pubsub'>" + xml + "</outer>";
        break;
    case PubSubEventNs:
        newXml = "<outer xmlns='http://jabber.org/protocol/pubsub#event'>" + xml + "</outer>";
        break;
    case PubSubOwnerNs:
        newXml = "<outer xmlns='http://jabber.org/protocol/pubsub#owner'>" + xml + "</outer>";
        break;
    }
    packet.parse(xmlToDom(newXml).firstChildElement());
}

class tst_QXmppPubSub : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testAffiliation_data();
    Q_SLOT void testAffiliation();
    Q_SLOT void testIsAffiliation_data();
    Q_SLOT void testIsAffiliation();
    Q_SLOT void testSubscription_data();
    Q_SLOT void testSubscription();
    Q_SLOT void testItem();
    Q_SLOT void testIsItem_data();
    Q_SLOT void testIsItem();
    Q_SLOT void testTestItem();
};

void tst_QXmppPubSub::testAffiliation_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<AffiliationType>("type");
    QTest::addColumn<QString>("jid");
    QTest::addColumn<QString>("node");

#define ROW(name, xml, type, jid, node) \
    QTest::newRow(name) << QByteArrayLiteral(xml) << type << jid << node

    ROW("owner", "<affiliation affiliation='owner' node='node1'/>", AffiliationType::Owner, QString(), u"node1"_s);
    ROW("publisher", "<affiliation affiliation='publisher' node='node2'/>", AffiliationType::Publisher, QString(), u"node2"_s);
    ROW("outcast", "<affiliation affiliation='outcast' node='noise'/>", AffiliationType::Outcast, QString(), u"noise"_s);
    ROW("none", "<affiliation affiliation='none' node='stuff'/>", AffiliationType::None, QString(), u"stuff"_s);
    ROW("with-jid", "<affiliation affiliation='owner' jid='snob@qxmpp.org'/>", AffiliationType::Owner, u"snob@qxmpp.org"_s, QString());

#undef ROW
}

void tst_QXmppPubSub::testAffiliation()
{
    QFETCH(QByteArray, xml);
    QFETCH(AffiliationType, type);
    QFETCH(QString, jid);
    QFETCH(QString, node);

    Affiliation affiliation;
    parsePacket(affiliation, xml);
    QCOMPARE(affiliation.jid(), jid);
    QCOMPARE(affiliation.node(), node);
    QCOMPARE(affiliation.type(), type);
    serializePacket(affiliation, xml);

    affiliation = {};
    affiliation.setJid(jid);
    affiliation.setNode(node);
    affiliation.setType(type);
    serializePacket(affiliation, xml);
}

void tst_QXmppPubSub::testIsAffiliation_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("ps-correct")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub'><affiliation affiliation=\"owner\" node=\"node1\"/></parent>")
        << true;
    QTest::newRow("ps-missing-node")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub'><affiliation affiliation=\"owner\"/></parent>")
        << false;
    QTest::newRow("ps-invalid-affiliation")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub'><affiliation affiliation=\"gigaowner\" node=\"node1\"/></parent>")
        << false;
    QTest::newRow("psowner-correct")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub#owner'><affiliation affiliation=\"owner\" jid=\"snob@qxmpp.org\"/></parent>")
        << true;
    QTest::newRow("psowner-missing-jid")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub#owner'><affiliation affiliation=\"owner\"/></parent>")
        << false;
    QTest::newRow("psowner-invalid-affiliation")
        << QByteArrayLiteral("<parent xmlns='http://jabber.org/protocol/pubsub#owner'><affiliation affiliation=\"superowner\" jid=\"snob@qxmpp.org\"/></parent>")
        << false;
    QTest::newRow("invalid-namespace")
        << QByteArrayLiteral("<parent xmlns='urn:xmpp:mix:0'><affiliation affiliation=\"owner\" node=\"node1\"/></parent>")
        << false;
}

void tst_QXmppPubSub::testIsAffiliation()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accepted);

    auto dom = xmlToDom(xml).firstChildElement();
    QCOMPARE(Affiliation::isAffiliation(dom), accepted);
}

void tst_QXmppPubSub::testSubscription_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<PubSubNamespace>("pubSubNs");
    QTest::addColumn<SubscriptionState>("state");
    QTest::addColumn<QString>("jid");
    QTest::addColumn<QString>("node");
    QTest::addColumn<QString>("subid");
    QTest::addColumn<SubscriptionConfig>("configSupport");

#define ROW(name, xmlns, xml, state, jid, node, subid, configSupport) \
    QTest::newRow(name) << QByteArrayLiteral(xml) << xmlns << state << jid << node << subid << configSupport

    ROW("subscribed", PubSubNs, "<subscription jid='francisco@denmark.lit' node='node1' subscription='subscribed'/>", SubscriptionState::Subscribed, u"francisco@denmark.lit"_s, u"node1"_s, QString(), SubscriptionConfig::Unavailable);
    ROW("unconfigured", PubSubNs, "<subscription jid='francisco@denmark.lit' node='node5' subscription='unconfigured'/>", SubscriptionState::Unconfigured, u"francisco@denmark.lit"_s, u"node5"_s, QString(), SubscriptionConfig::Unavailable);
    ROW("subscribed-subid", PubSubNs, "<subscription jid='francisco@denmark.lit' node='node6' subscription='subscribed' subid='123-abc'/>", SubscriptionState::Subscribed, u"francisco@denmark.lit"_s, u"node6"_s, u"123-abc"_s, SubscriptionConfig::Unavailable);
    ROW("pending", PubSubNs, "<subscription jid='francisco@denmark.lit' node='princely_musings' subscription='pending'/>", SubscriptionState::Pending, u"francisco@denmark.lit"_s, u"princely_musings"_s, QString(), SubscriptionConfig::Unavailable);
    ROW("config-required", PubSubNs, "<subscription jid='francisco@denmark.lit' node='princely_musings' subscription='unconfigured'><subscribe-options><required/></subscribe-options></subscription>", SubscriptionState::Unconfigured, u"francisco@denmark.lit"_s, u"princely_musings"_s, QString(), SubscriptionConfig::Required);
    ROW("config-available", PubSubNs, "<subscription jid='francisco@denmark.lit' node='princely_musings' subscription='unconfigured'><subscribe-options/></subscription>", SubscriptionState::Unconfigured, u"francisco@denmark.lit"_s, u"princely_musings"_s, QString(), SubscriptionConfig::Available);

#undef ROW
}

void tst_QXmppPubSub::testSubscription()
{
    QFETCH(QByteArray, xml);
    QFETCH(PubSubNamespace, pubSubNs);
    QFETCH(SubscriptionState, state);
    QFETCH(QString, jid);
    QFETCH(QString, node);
    QFETCH(QString, subid);
    QFETCH(SubscriptionConfig, configSupport);

    QXmppPubSubSubscription sub;
    parsePacket(sub, xml, pubSubNs);
    serializePacket(sub, xml);
    QCOMPARE(sub.state(), state);
    QCOMPARE(sub.jid(), jid);
    QCOMPARE(sub.node(), node);
    QCOMPARE(sub.subId(), subid);
    QCOMPARE(sub.configurationSupport(), configSupport);

    switch (configSupport) {
    case SubscriptionConfig::Unavailable:
        if (state == SubscriptionState::Unconfigured) {
            QVERIFY(sub.isConfigurationRequired());
        } else {
            QVERIFY(!sub.isConfigurationRequired());
        }
        QVERIFY(!sub.isConfigurationSupported());
        break;
    case SubscriptionConfig::Available:
        if (state == SubscriptionState::Unconfigured) {
            QVERIFY(sub.isConfigurationRequired());
        } else {
            QVERIFY(!sub.isConfigurationRequired());
        }
        QVERIFY(sub.isConfigurationSupported());
        break;
    case SubscriptionConfig::Required:
        QVERIFY(sub.isConfigurationRequired());
        QVERIFY(sub.isConfigurationSupported());
        break;
    }

    sub = {};
    sub.setState(state);
    sub.setJid(jid);
    sub.setNode(node);
    sub.setSubId(subid);
    sub.setConfigurationSupport(configSupport);
    serializePacket(sub, xml);
}
void tst_QXmppPubSub::testItem()
{
    const auto xml = QByteArrayLiteral("<item id=\"abc1337\" publisher=\"lnj@qxmpp.org\"/>");

    QXmppPubSubBaseItem item;
    parsePacket(item, xml);

    QCOMPARE(item.id(), u"abc1337"_s);
    QCOMPARE(item.publisher(), u"lnj@qxmpp.org"_s);

    // test serialization with parsed item
    serializePacket(item, xml);

    // test serialization with constructor values
    item = QXmppPubSubBaseItem("abc1337", "lnj@qxmpp.org");
    serializePacket(item, xml);

    // test serialization with setters
    item = QXmppPubSubBaseItem();
    item.setId("abc1337");
    item.setPublisher("lnj@qxmpp.org");
    serializePacket(item, xml);
}

void tst_QXmppPubSub::testIsItem_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("valid");

    QTest::newRow("valid-id-publisher")
        << QByteArrayLiteral("<item id=\"abc1337\" publisher=\"lnj@qxmpp.org\"/>")
        << true;
    QTest::newRow("valid-id")
        << QByteArrayLiteral("<item id=\"abc1337\"/>")
        << true;
    QTest::newRow("valid-publisher")
        << QByteArrayLiteral("<item publisher=\"lnj@qxmpp.org\"/>")
        << true;
    QTest::newRow("valid")
        << QByteArrayLiteral("<item/>")
        << true;
    QTest::newRow("valid-payload")
        << QByteArrayLiteral("<item><payload xmlns=\"blah\"/></item>")
        << true;
    QTest::newRow("invalid-tag-name")
        << QByteArrayLiteral("<pubsub-item id=\"abc1337\" publisher=\"lnj@qxmpp.org\"/>")
        << false;
}

void tst_QXmppPubSub::testIsItem()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, valid);

    QCOMPARE(QXmppPubSubBaseItem::isItem(xmlToDom(xml)), valid);
}

void tst_QXmppPubSub::testTestItem()
{
    const auto xml = QByteArrayLiteral("<item id=\"abc1337\" publisher=\"lnj@qxmpp.org\"><test-payload/></item>");

    TestItem item;
    parsePacket(item, xml);
    serializePacket(item, xml);

    QVERIFY(item.parseCalled);
    QVERIFY(item.serializeCalled);

    const auto invalidXml = QByteArrayLiteral("<item id=\"abc1337\"><tune/></item>");
    QVERIFY(TestItem::isItem(xmlToDom(xml)));
    QVERIFY(!TestItem::isItem(xmlToDom(invalidXml)));
}

// ===================== tst_QXmppPubSubEvent =====================

Q_DECLARE_METATYPE(std::optional<QXmppPubSubSubscription>)
Q_DECLARE_METATYPE(std::optional<QXmppDataForm>)

class tst_QXmppPubSubEvent : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testBasic_data();
    Q_SLOT void testBasic();
    Q_SLOT void testCustomItem();
};

void tst_QXmppPubSubEvent::testBasic_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QXmppPubSubEventBase::EventType>("eventType");
    QTest::addColumn<QString>("node");
    QTest::addColumn<QStringList>("retractIds");
    QTest::addColumn<QString>("redirectUri");
    QTest::addColumn<std::optional<QXmppPubSubSubscription>>("subscription");
    QTest::addColumn<QList<QXmppPubSubBaseItem>>("items");
    QTest::addColumn<std::optional<QXmppDataForm>>("configurationForm");

#define ROW(name, xml, type, node, retractIds, redirectUri, subscription, items, configForm) \
    QTest::newRow(name) << QByteArrayLiteral(xml)                                            \
                        << type                                                              \
                        << node                                                              \
                        << (retractIds)                                                      \
                        << redirectUri                                                       \
                        << static_cast<std::optional<QXmppPubSubSubscription>>(subscription) \
                        << (items)                                                           \
                        << static_cast<std::optional<QXmppDataForm>>(configForm)

    ROW("items",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<items node=\"princely_musings\">"
        "<item id=\"ae890ac52d0df67ed7cfdf51b644e901\"/>"
        "</items>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Items,
        "princely_musings",
        QStringList(),
        QString(),
        std::nullopt,
        QList<QXmppPubSubBaseItem>() << QXmppPubSubBaseItem("ae890ac52d0df67ed7cfdf51b644e901"),
        std::nullopt);

    ROW("retract",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<items node=\"princely_musings\">"
        "<retract id=\"ae890ac52d0df67ed7cfdf51b644e901\"/>"
        "<retract id=\"34324897shdfjk948577342343243243\"/>"
        "</items>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Retract,
        "princely_musings",
        QStringList() << "ae890ac52d0df67ed7cfdf51b644e901"
                      << "34324897shdfjk948577342343243243",
        QString(),
        std::nullopt,
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

    ROW("configuration-notify",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<configuration node=\"princely_musings\"/>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Configuration,
        "princely_musings",
        QStringList(),
        QString(),
        std::nullopt,
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

    ROW("configuration",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<configuration node=\"princely_musings\">"
        "<x xmlns=\"jabber:x:data\" type=\"result\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>http://jabber.org/protocol/pubsub#node_config</value>"
        "</field>"
        "<field type=\"text-single\" var=\"pubsub#title\">"
        "<value>Princely Musings (Atom)</value>"
        "</field>"
        "</x>"
        "</configuration>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Configuration,
        "princely_musings",
        QStringList(),
        QString(),
        std::nullopt,
        QList<QXmppPubSubBaseItem>(),
        QXmppDataForm(QXmppDataForm::Result,
                      QList<QXmppDataForm::Field>()
                          << QXmppDataForm::Field(QXmppDataForm::Field::HiddenField,
                                                  "FORM_TYPE",
                                                  "http://jabber.org/protocol/pubsub#node_config")
                          << QXmppDataForm::Field(QXmppDataForm::Field::TextSingleField,
                                                  "pubsub#title",
                                                  "Princely Musings (Atom)")));

    ROW("purge",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<purge node=\"princely_musings\"/>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Purge,
        "princely_musings",
        QStringList(),
        QString(),
        std::nullopt,
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

    ROW("subscription-subscribed",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<subscription jid=\"horatio@denmark.lit\" node=\"princely_musings\" subscription=\"subscribed\"/>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Subscription,
        QString(),
        QStringList(),
        QString(),
        QXmppPubSubSubscription("horatio@denmark.lit", "princely_musings", {}, QXmppPubSubSubscription::Subscribed),
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

    ROW("subscription-none",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<subscription jid=\"polonius@denmark.lit\" node=\"princely_musings\" subscription=\"none\"/>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Subscription,
        QString(),
        QStringList(),
        QString(),
        QXmppPubSubSubscription("polonius@denmark.lit", "princely_musings", {}, QXmppPubSubSubscription::None),
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

    ROW("subscription-expiry",
        "<message id=\"foo\" type=\"normal\">"
        "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
        "<subscription jid=\"francisco@denmark.lit\" node=\"princely_musings\" subscription=\"subscribed\" subid=\"ba49252aaa4f5d320c24d3766f0bdcade78c78d3\" expiry=\"2006-02-28T23:59:59Z\"/>"
        "</event>"
        "</message>",
        QXmppPubSubEventBase::Subscription,
        QString(),
        QStringList(),
        QString(),
        QXmppPubSubSubscription("francisco@denmark.lit",
                                "princely_musings",
                                "ba49252aaa4f5d320c24d3766f0bdcade78c78d3",
                                QXmppPubSubSubscription::Subscribed,
                                QXmppPubSubSubscription::Unavailable,
                                QDateTime({ 2006, 02, 28 }, { 23, 59, 59 }, TimeZoneUTC)),
        QList<QXmppPubSubBaseItem>(),
        std::nullopt);

#undef ROW
}

void tst_QXmppPubSubEvent::testBasic()
{
    QFETCH(QByteArray, xml);
    QFETCH(QXmppPubSubEventBase::EventType, eventType);
    QFETCH(QString, node);
    QFETCH(QStringList, retractIds);
    QFETCH(QString, redirectUri);
    QFETCH(std::optional<QXmppPubSubSubscription>, subscription);
    QFETCH(QList<QXmppPubSubBaseItem>, items);
    QFETCH(std::optional<QXmppDataForm>, configurationForm);

    // parse
    QVERIFY(QXmppPubSubEvent<>::isPubSubEvent(xmlToDom(xml)));
    QXmppPubSubEvent event;
    parsePacket(event, xml);

    QCOMPARE(event.eventType(), eventType);
    QCOMPARE(event.node(), node);
    QCOMPARE(event.retractIds(), retractIds);
    QCOMPARE(event.redirectUri(), redirectUri);
    QCOMPARE(event.subscription().has_value(), subscription.has_value());
    if (subscription) {
        QCOMPARE(event.subscription()->jid(), subscription->jid());
        QCOMPARE(event.subscription()->node(), subscription->node());
        QCOMPARE(event.subscription()->state(), subscription->state());
        QCOMPARE(event.subscription()->subId(), subscription->subId());
        QCOMPARE(event.subscription()->expiry(), subscription->expiry());
    }
    QCOMPARE(event.items().count(), items.count());
    for (int i = 0; i < items.size(); i++) {
        QCOMPARE(event.items().at(i).id(), items.at(i).id());
        QCOMPARE(event.items().at(i).publisher(), items.at(i).publisher());
    }
    QCOMPARE(event.configurationForm().has_value(), configurationForm.has_value());
    if (configurationForm) {
        const auto parsedConfig = event.configurationForm();
        QCOMPARE(parsedConfig->fields().count(), configurationForm->constFields().count());
        for (int i = 0; i < configurationForm->constFields().count(); i++) {
            QCOMPARE(parsedConfig->fields().at(i).key(), configurationForm->constFields().at(i).key());
            QCOMPARE(parsedConfig->fields().at(i).value(), configurationForm->constFields().at(i).value());
            QCOMPARE(parsedConfig->fields().at(i).type(), configurationForm->constFields().at(i).type());
        }
    }

    // serialize from parsed
    serializePacket(event, xml);

    // serialize from setters
    event = QXmppPubSubEvent();
    event.setId("foo");
    event.setEventType(eventType);
    event.setNode(node);
    event.setRetractIds(retractIds);
    event.setRedirectUri(redirectUri);
    event.setSubscription(subscription);
    event.setItems(items);
    event.setConfigurationForm(configurationForm);

    serializePacket(event, xml);
}

void tst_QXmppPubSubEvent::testCustomItem()
{
    const QByteArray xml = "<message id=\"foo\" type=\"normal\">"
                           "<event xmlns=\"http://jabber.org/protocol/pubsub#event\">"
                           "<items node=\"princely_musings\">"
                           "<item id=\"42\"><test-payload/></item>"
                           "<item id=\"23\"><test-payload/></item>"
                           "</items>"
                           "</event>"
                           "</message>";

    // test isPubSubIq also checks item validity
    TestItem::isItemCalled = false;
    QVERIFY(QXmppPubSubEvent<TestItem>::isPubSubEvent(xmlToDom(xml)));
    QVERIFY(TestItem::isItemCalled);

    QXmppPubSubEvent<TestItem> event;
    parsePacket(event, xml);

    QCOMPARE(event.id(), u"foo"_s);
    QCOMPARE(event.eventType(), QXmppPubSubEvent<>::Items);
    QCOMPARE(event.node(), u"princely_musings"_s);
    QCOMPARE(event.items().count(), 2);
    QCOMPARE(event.items().at(0).id(), QString::number(42));
    QCOMPARE(event.items().at(1).id(), QString::number(23));
    QCOMPARE(event.items().at(0).publisher(), QString());
    QCOMPARE(event.items().at(1).publisher(), QString());
    QVERIFY(event.items().at(0).parseCalled);
    QVERIFY(event.items().at(1).parseCalled);
    QVERIFY(!event.items().at(0).serializeCalled);
    QVERIFY(!event.items().at(1).serializeCalled);

    // serialize from parsed
    serializePacket(event, xml);

    QVERIFY(event.items().at(0).serializeCalled);
    QVERIFY(event.items().at(1).serializeCalled);

    // serialize from setters
    event = QXmppPubSubEvent<TestItem>();
    event.setId("foo");
    event.setEventType(QXmppPubSubEvent<>::Items);
    event.setNode("princely_musings");
    event.setItems({ TestItem("42"), TestItem("23") });
    serializePacket(event, xml);
}

// ===================== tst_QXmppPubSubForms =====================

class tst_QXmppPubSubForms : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void subAuthorization();
};

void tst_QXmppPubSubForms::subAuthorization()
{
    QByteArray xml(R"(
<x xmlns="jabber:x:data" type="form">
<field type="hidden" var="FORM_TYPE">
<value>http://jabber.org/protocol/pubsub#subscribe_authorization</value>
</field>
<field type="boolean" var="pubsub#allow"><value>false</value></field>
<field type="text-single" var="pubsub#node"><value>princely_musings</value></field>
<field type="text-single" var="pubsub#subid"><value>123-abc</value></field>
<field type="jid-single" var="pubsub#subscriber_jid"><value>horatio@denmark.lit</value></field>
</x>)");

    QXmppDataForm form;
    parsePacket(form, xml);

    auto subAuthForm = QXmppPubSubSubAuthorization::fromDataForm(form);
    QVERIFY(subAuthForm.has_value());
    QCOMPARE(subAuthForm->subid(), u"123-abc"_s);
    QCOMPARE(subAuthForm->node(), u"princely_musings"_s);
    QCOMPARE(subAuthForm->subscriberJid(), u"horatio@denmark.lit"_s);
    QVERIFY(subAuthForm->allowSubscription().has_value());
    QCOMPARE(subAuthForm->allowSubscription().value(), false);

    form = subAuthForm->toDataForm();
    QVERIFY(!form.isNull());
    xml = QString::fromUtf8(xml).remove(QChar('\n')).toUtf8();
    serializePacket(form, xml);
}

// ===================== tst_QXmppPubSubIq =====================

using namespace QXmpp::Private;

class tst_QXmppPubSubIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testItems();
    Q_SLOT void testItemsResponse();
    Q_SLOT void testCreateNode();
    Q_SLOT void testDeleteNode();
    Q_SLOT void testPublish();
    Q_SLOT void testRetractItem();
    Q_SLOT void testSubscribe();
    Q_SLOT void testSubscription();
    Q_SLOT void testSubscriptions();
    Q_SLOT void testIsPubSubIq_data();
    Q_SLOT void testIsPubSubIq();

    Q_SLOT void testCustomItem();
};

void tst_QXmppPubSubIq::testItems()
{
    const QByteArray xml(
        "<iq"
        " id=\"items1\""
        " to=\"pubsub.shakespeare.lit\""
        " from=\"francisco@denmark.lit/barracks\""
        " type=\"get\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<items node=\"storage:bookmarks\"/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("items1"));
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("francisco@denmark.lit/barracks"));
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.queryType(), PubSubIq<>::Items);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("storage:bookmarks"));
    serializePacket(iq, xml);

    iq = PubSubIq();
    iq.setId(QLatin1String("items1"));
    iq.setTo(QLatin1String("pubsub.shakespeare.lit"));
    iq.setFrom(QLatin1String("francisco@denmark.lit/barracks"));
    iq.setType(QXmppIq::Get);
    iq.setQueryType(PubSubIq<>::Items);
    iq.setQueryJid({});
    iq.setQueryNode(QLatin1String("storage:bookmarks"));
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testItemsResponse()
{
    const QByteArray xml(
        "<iq"
        " id=\"items1\""
        " to=\"francisco@denmark.lit/barracks\""
        " from=\"pubsub.shakespeare.lit\""
        " type=\"result\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<items node=\"storage:bookmarks\">"
        "<item id=\"current\"/>"
        "</items>"
        "<set xmlns=\"http://jabber.org/protocol/rsm\">"
        "<first index=\"0\">current</first>"
        "<last>otheritemid</last>"
        "<count>19</count>"
        "</set>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("items1"));
    QCOMPARE(iq.to(), QLatin1String("francisco@denmark.lit/barracks"));
    QCOMPARE(iq.from(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.queryType(), PubSubIq<>::Items);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("storage:bookmarks"));
    QVERIFY(iq.itemsContinuation().has_value());
    QCOMPARE(iq.itemsContinuation()->count(), 19);
    QCOMPARE(iq.itemsContinuation()->index(), 0);
    QCOMPARE(iq.itemsContinuation()->first(), u"current"_s);
    QCOMPARE(iq.itemsContinuation()->last(), u"otheritemid"_s);
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testCreateNode()
{
    const QByteArray xml(
        "<iq id=\"create1\" to=\"pubsub.shakespeare.lit\" from=\"hamlet@denmark.lit/elsinore\" type=\"set\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<create node=\"princely_musings\"/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"create1"_s);
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("hamlet@denmark.lit/elsinore"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.queryType(), PubSubIq<>::Create);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("princely_musings"));
    serializePacket(iq, xml);

    iq = PubSubIq();
    iq.setId(QLatin1String("create1"));
    iq.setTo(QLatin1String("pubsub.shakespeare.lit"));
    iq.setFrom(QLatin1String("hamlet@denmark.lit/elsinore"));
    iq.setType(QXmppIq::Set);
    iq.setQueryType(PubSubIq<>::Create);
    iq.setQueryNode(QLatin1String("princely_musings"));
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testDeleteNode()
{
    const QByteArray xml(
        "<iq id=\"delete1\" to=\"pubsub.shakespeare.lit\" from=\"hamlet@denmark.lit/elsinore\" type=\"set\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub#owner\">"
        "<delete node=\"princely_musings\"/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"delete1"_s);
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("hamlet@denmark.lit/elsinore"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.queryType(), PubSubIq<>::Delete);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("princely_musings"));
    serializePacket(iq, xml);

    iq = PubSubIq<>();
    iq.setId(QLatin1String("delete1"));
    iq.setTo(QLatin1String("pubsub.shakespeare.lit"));
    iq.setFrom(QLatin1String("hamlet@denmark.lit/elsinore"));
    iq.setType(QXmppIq::Set);
    iq.setQueryType(PubSubIq<>::Delete);
    iq.setQueryNode(QLatin1String("princely_musings"));
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testPublish()
{
    const QByteArray xml(
        "<iq"
        " id=\"items1\""
        " to=\"pubsub.shakespeare.lit\""
        " from=\"francisco@denmark.lit/barracks\""
        " type=\"result\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<publish node=\"storage:bookmarks\">"
        "<item id=\"current\"/>"
        "</publish>"
        "</pubsub>"
        "</iq>");

    PubSubIq<> iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("items1"));
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("francisco@denmark.lit/barracks"));
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.queryType(), PubSubIq<>::Publish);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("storage:bookmarks"));
    serializePacket(iq, xml);

    // serialize using setters
    QXmppPubSubBaseItem item(u"current"_s);

    iq = PubSubIq();
    iq.setId(QLatin1String("items1"));
    iq.setTo(QLatin1String("pubsub.shakespeare.lit"));
    iq.setFrom(QLatin1String("francisco@denmark.lit/barracks"));
    iq.setType(QXmppIq::Result);
    iq.setQueryType(PubSubIq<>::Publish);
    iq.setQueryJid({});
    iq.setQueryNode(QLatin1String("storage:bookmarks"));
    iq.setItems({ item });

    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testRetractItem()
{
    QByteArray xml(
        "<iq"
        " id=\"retract1\""
        " to=\"pubsub.shakespeare.lit\""
        " from=\"hamlet@denmark.lit/elsinore\""
        " type=\"set\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<retract node=\"princely_musings\">"
        "<item id=\"ae890ac52d0df67ed7cfdf51b644e901\"/>"
        "</retract>"
        "</pubsub>"
        "</iq>");

    PubSubIq<> iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"retract1"_s);
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("hamlet@denmark.lit/elsinore"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.queryType(), PubSubIq<>::Retract);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QLatin1String("princely_musings"));
    QCOMPARE(iq.retractNotify(), false);
    QCOMPARE(iq.items().size(), 1);
    QCOMPARE(iq.items().first().id(), u"ae890ac52d0df67ed7cfdf51b644e901"_s);
    serializePacket(iq, xml);

    iq = PubSubIq();
    iq.setId(QLatin1String("retract1"));
    iq.setTo(QLatin1String("pubsub.shakespeare.lit"));
    iq.setFrom(QLatin1String("hamlet@denmark.lit/elsinore"));
    iq.setType(QXmppIq::Set);
    iq.setQueryType(PubSubIq<>::Retract);
    iq.setQueryJid({});
    iq.setQueryNode(QLatin1String("princely_musings"));

    QXmppPubSubBaseItem item;
    item.setId(u"ae890ac52d0df67ed7cfdf51b644e901"_s);
    iq.setItems({ item });

    serializePacket(iq, xml);

    xml = "<iq"
          " id=\"retract1\""
          " type=\"set\">"
          "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
          "<retract node=\"princely_musings\" notify='true'>"
          "<item id=\"ae890ac52d0df67ed7cfdf51b644e901\"/>"
          "</retract>"
          "</pubsub>"
          "</iq>";

    iq = PubSubIq<>();
    parsePacket(iq, xml);
    QCOMPARE(iq.retractNotify(), true);
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testSubscribe()
{
    const QByteArray xml(
        "<iq"
        " id=\"sub1\""
        " to=\"pubsub.shakespeare.lit\""
        " from=\"francisco@denmark.lit/barracks\""
        " type=\"set\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<subscribe jid=\"francisco@denmark.lit\" node=\"princely_musings\"/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("sub1"));
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("francisco@denmark.lit/barracks"));
    QCOMPARE(iq.type(), QXmppIq::Set);
    QCOMPARE(iq.queryType(), PubSubIq<>::Subscribe);
    QCOMPARE(iq.queryJid(), QLatin1String("francisco@denmark.lit"));
    QCOMPARE(iq.queryNode(), QLatin1String("princely_musings"));
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testSubscription()
{
    const QByteArray xml(
        "<iq"
        " id=\"sub1\""
        " to=\"francisco@denmark.lit/barracks\""
        " from=\"pubsub.shakespeare.lit\""
        " type=\"result\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<subscription jid=\"francisco@denmark.lit\""
        " node=\"princely_musings\""
        " subid=\"ba49252aaa4f5d320c24d3766f0bdcade78c78d3\"/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"sub1"_s);
    QCOMPARE(iq.to(), u"francisco@denmark.lit/barracks"_s);
    QCOMPARE(iq.from(), u"pubsub.shakespeare.lit"_s);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.queryType(), PubSubIq<>::Subscription);
    QCOMPARE(iq.subscription()->jid(), u"francisco@denmark.lit"_s);
    QCOMPARE(iq.subscription()->node(), u"princely_musings"_s);
    QCOMPARE(iq.subscription()->subId(), u"ba49252aaa4f5d320c24d3766f0bdcade78c78d3"_s);
    serializePacket(iq, xml);

    iq = PubSubIq();
    iq.setId("sub1");
    iq.setTo("francisco@denmark.lit/barracks");
    iq.setFrom("pubsub.shakespeare.lit");
    iq.setType(QXmppIq::Result);
    iq.setQueryType(PubSubIq<>::Subscription);
    iq.setSubscription(QXmppPubSubSubscription(
        "francisco@denmark.lit",
        "princely_musings",
        "ba49252aaa4f5d320c24d3766f0bdcade78c78d3"));
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testSubscriptions()
{
    const QByteArray xml(
        "<iq"
        " id=\"subscriptions1\""
        " to=\"pubsub.shakespeare.lit\""
        " from=\"francisco@denmark.lit/barracks\""
        " type=\"get\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<subscriptions/>"
        "</pubsub>"
        "</iq>");

    PubSubIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), QLatin1String("subscriptions1"));
    QCOMPARE(iq.to(), QLatin1String("pubsub.shakespeare.lit"));
    QCOMPARE(iq.from(), QLatin1String("francisco@denmark.lit/barracks"));
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.queryType(), PubSubIq<>::Subscriptions);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), QString());
    serializePacket(iq, xml);
}

void tst_QXmppPubSubIq::testIsPubSubIq_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("valid-pubsub-iq")
        << QByteArrayLiteral("<iq><pubsub xmlns=\"http://jabber.org/protocol/pubsub\"><items node=\"smth\"/></pubsub></iq>")
        << true;

    QTest::newRow("items-missing-node-name")
        << QByteArrayLiteral("<iq><pubsub xmlns=\"http://jabber.org/protocol/pubsub\"><items/></pubsub></iq>")
        << false;

    QTest::newRow("unknown-query-type")
        << QByteArrayLiteral("<iq><pubsub xmlns=\"http://jabber.org/protocol/pubsub\"><shuffle/></pubsub></iq>")
        << false;

    QTest::newRow("wrong-element")
        << QByteArrayLiteral("<iq><pubsub2 xmlns=\"http://jabber.org/protocol/pubsub\"><items node=\"smth\"/></pubsub2></iq>")
        << false;

    QTest::newRow("wrong-namespace")
        << QByteArrayLiteral("<iq><pubsub xmlns=\"urn:xmpp:pubsub2:0\"><items node=\"smth\"/></pubsub></iq>")
        << false;
}

void tst_QXmppPubSubIq::testIsPubSubIq()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isValid);

    QCOMPARE(PubSubIq<>::isPubSubIq(xmlToDom(xml)), isValid);
}

void tst_QXmppPubSubIq::testCustomItem()
{
    const QByteArray xml(
        "<iq id=\"a1\" type=\"result\">"
        "<pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
        "<items node=\"blah\">"
        "<item id=\"42\"><test-payload/></item>"
        "<item id=\"23\"><test-payload/></item>"
        "</items>"
        "</pubsub>"
        "</iq>");

    // test isPubSubIq also checks item validity
    TestItem::isItemCalled = false;
    QVERIFY(PubSubIq<TestItem>::isPubSubIq(xmlToDom(xml)));
    QVERIFY(TestItem::isItemCalled);

    PubSubIq<TestItem> iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.queryType(), PubSubIq<>::Items);
    QCOMPARE(iq.queryJid(), QString());
    QCOMPARE(iq.queryNode(), u"blah"_s);
    QCOMPARE(iq.items().size(), 2);
    QCOMPARE(iq.items().at(0).id(), u"42"_s);
    QCOMPARE(iq.items().at(1).id(), u"23"_s);
    QCOMPARE(iq.items().at(0).publisher(), QString());
    QCOMPARE(iq.items().at(1).publisher(), QString());

    QVERIFY(iq.items().at(0).parseCalled);
    QVERIFY(iq.items().at(1).parseCalled);
    QVERIFY(!iq.items().at(0).serializeCalled);
    QVERIFY(!iq.items().at(1).serializeCalled);

    serializePacket(iq, xml);

    iq = PubSubIq<TestItem>();
    iq.setId("a1");
    iq.setType(QXmppIq::Result);
    iq.setQueryType(PubSubIq<>::Items);
    iq.setQueryNode("blah");
    iq.setItems({ TestItem("42"), TestItem("23") });
    serializePacket(iq, xml);
}

// ============================================================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        tst_QXmppPubSub tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppPubSubEvent tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppPubSubForms tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    {
        tst_QXmppPubSubIq tc;
        status |= QTest::qExec(&tc, argc, argv);
    }
    return status;
}

#include "tst_QXmppPubSub.moc"

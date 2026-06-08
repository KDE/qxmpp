// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMessage.h"
#include "QXmppPresence.h"
#include "QXmppXmlElement.h"
#include "QXmppXmlExtensions.h"
#include "QXmppXmlRegistry.h"

#include "StringLiterals.h"
#include "util.h"

#include <QObject>
#include <QThread>

using namespace QXmpp;
using namespace QXmpp::Xml;

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------

// V3 parse style (fromDom → optional<T>).
struct FooExtension {
    static constexpr std::tuple XmlTag = { u"foo", u"urn:test:foo" };

    QString value;

    static std::optional<FooExtension> fromDom(const QDomElement &el)
    {
        if (el.tagName() != u"foo" || el.namespaceURI() != u"urn:test:foo") {
            return {};
        }
        return FooExtension { el.attribute(u"value"_s) };
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeStartElement(u"foo"_s);
        w->writeDefaultNamespace(u"urn:test:foo"_s);
        w->writeAttribute(u"value"_s, value);
        w->writeEndElement();
    }
};

// V2 parse style (parse → bool).
struct BarExtension {
    static constexpr std::tuple XmlTag = { u"bar", u"urn:test:bar" };

    int count = 0;

    bool parse(const QDomElement &el)
    {
        if (el.tagName() != u"bar" || el.namespaceURI() != u"urn:test:bar") {
            return false;
        }
        count = el.attribute(u"n"_s).toInt();
        return true;
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeStartElement(u"bar"_s);
        w->writeDefaultNamespace(u"urn:test:bar"_s);
        w->writeAttribute(u"n"_s, QString::number(count));
        w->writeEndElement();
    }
};

// Used for Generic scope tests.
struct GenericExtension {
    static constexpr std::tuple XmlTag = { u"debug", u"urn:test:debug" };

    static std::optional<GenericExtension> fromDom(const QDomElement &el)
    {
        if (el.tagName() != u"debug" || el.namespaceURI() != u"urn:test:debug") {
            return {};
        }
        return GenericExtension {};
    }

    void toXml(QXmlStreamWriter *w) const
    {
        w->writeEmptyElement(u"debug"_s);
        w->writeDefaultNamespace(u"urn:test:debug"_s);
    }
};

// ---------------------------------------------------------------------------

static QString serializeExtensions(const Extensions &ext)
{
    QString output;
    QXmlStreamWriter writer(&output);
    ext.toXml(&writer);
    return output;
}

class tst_QXmppXmlRegistry : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testRegisterAndParseMessage();
    Q_SLOT void testRegisterAndParsePresence();
    Q_SLOT void testParseV2Style();
    Q_SLOT void testGenericScope();
    Q_SLOT void testFallbackElement();
    Q_SLOT void testUnregister();
    Q_SLOT void testContainerApi();
    Q_SLOT void testRoundTripMessage();
};

void tst_QXmppXmlRegistry::testRegisterAndParseMessage()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message to='alice@example.org'>"
        "<foo xmlns='urn:test:foo' value='hello'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    QVERIFY(!msg.extensions().isEmpty());
    auto foo = msg.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"hello"_s);

    Registry::unregisterElement<FooExtension>();
}

void tst_QXmppXmlRegistry::testRegisterAndParsePresence()
{
    Registry::registerElement<FooExtension>(Scope::Presence);

    const auto xml = QByteArrayLiteral(
        "<presence>"
        "<foo xmlns='urn:test:foo' value='world'/>"
        "</presence>");

    QXmppPresence pres;
    pres.parse(xmlToDom(xml));

    QVERIFY(!pres.extensions().isEmpty());
    auto foo = pres.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"world"_s);

    Registry::unregisterElement<FooExtension>();
}

void tst_QXmppXmlRegistry::testParseV2Style()
{
    Registry::registerElement<BarExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message>"
        "<bar xmlns='urn:test:bar' n='7'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    auto bar = msg.extensions().get<BarExtension>();
    QVERIFY(bar.has_value());
    QCOMPARE(bar->count, 7);

    Registry::unregisterElement<BarExtension>();
}

void tst_QXmppXmlRegistry::testGenericScope()
{
    Registry::registerElement<GenericExtension>(Scope::Generic);

    const auto msgXml = QByteArrayLiteral(
        "<message>"
        "<debug xmlns='urn:test:debug'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(msgXml));
    QVERIFY(msg.extensions().contains<GenericExtension>());

    const auto presXml = QByteArrayLiteral(
        "<presence>"
        "<debug xmlns='urn:test:debug'/>"
        "</presence>");

    QXmppPresence pres;
    pres.parse(xmlToDom(presXml));
    QVERIFY(pres.extensions().contains<GenericExtension>());

    Registry::unregisterElement<GenericExtension>();
}

void tst_QXmppXmlRegistry::testFallbackElement()
{
    // No registration → unknown element must land as Xml::Element fallback.
    const auto xml = QByteArrayLiteral(
        "<message>"
        "<unknown xmlns='urn:test:unknown' key='v'/>"
        "</message>");

    QXmppMessage msg;
    msg.parse(xmlToDom(xml));

    QVERIFY(!msg.extensions().isEmpty());
    auto elements = msg.extensions().getAll<Element>();
    QCOMPARE(elements.size(), 1);
    QCOMPARE(elements.at(0).tag(), u"unknown"_s);
    QCOMPARE(elements.at(0).xmlns(), u"urn:test:unknown"_s);
    QCOMPARE(*elements.at(0).attribute(u"key"), u"v"_s);

    // Serialization round-trip: the fallback element is written back out.
    QString serialized = serializeExtensions(msg.extensions());
    QVERIFY(serialized.contains(u"unknown"_s));
    QVERIFY(serialized.contains(u"urn:test:unknown"_s));
}

void tst_QXmppXmlRegistry::testUnregister()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    const auto xml = QByteArrayLiteral(
        "<message>"
        "<foo xmlns='urn:test:foo' value='x'/>"
        "</message>");

    // With registration: typed.
    {
        QXmppMessage msg;
        msg.parse(xmlToDom(xml));
        QVERIFY(msg.extensions().contains<FooExtension>());
    }

    Registry::unregisterElement<FooExtension>();

    // After unregister: stored as Xml::Element fallback.
    {
        QXmppMessage msg;
        msg.parse(xmlToDom(xml));
        QVERIFY(!msg.extensions().contains<FooExtension>());
        QVERIFY(msg.extensions().contains<Element>());
    }
}

void tst_QXmppXmlRegistry::testContainerApi()
{
    Extensions ext;
    QVERIFY(ext.isEmpty());
    QCOMPARE(ext.size(), 0);

    ext.add(FooExtension { u"a"_s });
    ext.add(FooExtension { u"b"_s });
    ext.add(BarExtension { 3 });

    QCOMPARE(ext.size(), 3);
    QVERIFY(ext.contains<FooExtension>());
    QVERIFY(ext.contains<BarExtension>());

    auto foos = ext.getAll<FooExtension>();
    QCOMPARE(foos.size(), 2);
    QCOMPARE(foos.at(0).value, u"a"_s);
    QCOMPARE(foos.at(1).value, u"b"_s);

    QVERIFY(ext.remove<FooExtension>());
    QCOMPARE(ext.size(), 2);

    QCOMPARE(ext.removeAll<FooExtension>(), 1);
    QCOMPARE(ext.size(), 1);

    ext.clear();
    QVERIFY(ext.isEmpty());
}

void tst_QXmppXmlRegistry::testRoundTripMessage()
{
    Registry::registerElement<FooExtension>(Scope::Message);

    // Build a message with a registered extension.
    QXmppMessage out;
    out.setTo(u"bob@example.org"_s);
    out.extensions().add(FooExtension { u"roundtrip"_s });

    QByteArray serialized;
    QXmlStreamWriter sw(&serialized);
    out.toXml(&sw);

    // Parse back and verify.
    QXmppMessage in;
    in.parse(xmlToDom(serialized));

    auto foo = in.extensions().get<FooExtension>();
    QVERIFY(foo.has_value());
    QCOMPARE(foo->value, u"roundtrip"_s);

    Registry::unregisterElement<FooExtension>();
}

QTEST_MAIN(tst_QXmppXmlRegistry)
#include "tst_QXmppXmlRegistry.moc"

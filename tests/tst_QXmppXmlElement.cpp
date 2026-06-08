// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppXmlElement.h"

#include "StringLiterals.h"
#include "XmlWriter.h"
#include "util.h"

#include <QObject>

using namespace QXmpp;

class tst_QXmppXmlElement : public QObject
{
    Q_OBJECT
private:
    Q_SLOT void testParseSimple();
    Q_SLOT void testRoundTripSimple();
    Q_SLOT void testRoundTripText();
    Q_SLOT void testRoundTripComplex();
    Q_SLOT void testAttributes();
    Q_SLOT void testChildren();
    Q_SLOT void testEquality();
};

static QString serialize(const Xml::Element &element)
{
    QString output;
    QXmlStreamWriter writer(&output);
    QXmpp::Private::XmlWriter xmlWriter(&writer);
    xmlWriter.write(element);
    return output;
}

void tst_QXmppXmlElement::testParseSimple()
{
    auto element = Xml::Element::fromDom(xmlToDom(QByteArrayLiteral("<foo xmlns=\"urn:example\"/>")));
    QCOMPARE(element.tag(), u"foo"_s);
    QCOMPARE(element.xmlns(), u"urn:example"_s);
    QVERIFY(element.text().isEmpty());
    QVERIFY(element.attributes().empty());
    QVERIFY(element.children().empty());
}

void tst_QXmppXmlElement::testRoundTripSimple()
{
    const auto xml = QByteArrayLiteral("<foo xmlns=\"urn:example\"/>");
    auto element = Xml::Element::fromDom(xmlToDom(xml));
    QCOMPARE(serialize(element).toUtf8(), xml);
}

void tst_QXmppXmlElement::testRoundTripText()
{
    const auto xml = QByteArrayLiteral("<foo xmlns=\"urn:example\" id=\"a1\">hello</foo>");
    auto element = Xml::Element::fromDom(xmlToDom(xml));
    QCOMPARE(element.text(), u"hello"_s);
    QVERIFY(element.attribute(u"id").has_value());
    QCOMPARE(*element.attribute(u"id"), u"a1"_s);
    QCOMPARE(serialize(element).toUtf8(), xml);
}

void tst_QXmppXmlElement::testRoundTripComplex()
{
    const auto xml = QByteArrayLiteral(
        "<message xmlns=\"jabber:client\" type=\"chat\" id=\"42\">"
        "<body>text</body>"
        "<x xmlns=\"urn:example:other\" a=\"1\" b=\"2\">"
        "<inner>deep</inner>"
        "</x>"
        "</message>");

    auto element = Xml::Element::fromDom(xmlToDom(xml));

    QCOMPARE(element.tag(), u"message"_s);
    QCOMPARE(element.xmlns(), u"jabber:client"_s);
    QCOMPARE(element.children().size(), size_t(2));
    QCOMPARE(element.children().at(0).tag(), u"body"_s);
    QCOMPARE(element.children().at(0).text(), u"text"_s);
    QCOMPARE(element.children().at(1).tag(), u"x"_s);
    QCOMPARE(element.children().at(1).xmlns(), u"urn:example:other"_s);
    QCOMPARE(element.children().at(1).children().at(0).text(), u"deep"_s);

    // Serialization re-declares inherited namespaces, so the output is not
    // byte-identical to the input. Verify semantic round-trip stability instead:
    // re-parsing the serialized output yields the same structure.
    auto reparsed = Xml::Element::fromDom(xmlToDom(serialize(element).toUtf8()));
    QCOMPARE(element, reparsed);
}

void tst_QXmppXmlElement::testAttributes()
{
    Xml::Element element(u"foo"_s);

    element.setAttribute(u"a"_s, u"1"_s);
    element.setAttribute(u"b"_s, u"2"_s);
    QCOMPARE(element.attributes().size(), size_t(2));

    // add-or-replace: same name keeps a single entry with the new value
    element.setAttribute(u"a"_s, u"3"_s);
    QCOMPARE(element.attributes().size(), size_t(2));
    QCOMPARE(*element.attribute(u"a"), u"3"_s);

    QVERIFY(!element.attribute(u"missing").has_value());

    element.removeAttribute(u"a");
    QCOMPARE(element.attributes().size(), size_t(1));
    QVERIFY(!element.attribute(u"a").has_value());
    QCOMPARE(*element.attribute(u"b"), u"2"_s);
}

void tst_QXmppXmlElement::testChildren()
{
    Xml::Element element(u"root"_s);
    element.addChild(Xml::Element(u"a"_s));
    element.addChild(Xml::Element(u"b"_s));
    QCOMPARE(element.children().size(), size_t(2));
    QCOMPARE(element.children().at(0).tag(), u"a"_s);

    element.setChildren({ Xml::Element(u"c"_s) });
    QCOMPARE(element.children().size(), size_t(1));
    QCOMPARE(element.children().at(0).tag(), u"c"_s);
}

void tst_QXmppXmlElement::testEquality()
{
    Xml::Element a(u"foo"_s, u"urn:x"_s);
    a.setAttribute(u"k"_s, u"v"_s);
    a.addChild(Xml::Element(u"child"_s));

    auto b = a;
    QCOMPARE(a, b);

    b.setText(u"different"_s);
    QVERIFY(!(a == b));
}

QTEST_MAIN(tst_QXmppXmlElement)
#include "tst_QXmppXmlElement.moc"

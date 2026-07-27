// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2012 Oliver Goffart <ogoffart@woboq.com>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Tests for the IQ base class and for the IQ types that have no manager or
// topic test of their own; the other IQ parse tests live next to the code that
// uses them.

#include "QXmppDataForm.h"
#include "QXmppIq.h"
#include "QXmppNonSASLAuth.h"
#include "QXmppPushEnableIq.h"
#include "QXmppRpcIq.h"

#include "util.h"

#include <QObject>

// helpers: RpcIq
static void checkVariant(const QVariant &value, const QByteArray &xml)
{
    // serialise
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter writer(&buffer);
    QXmppRpcMarshaller::marshall(&writer, value);
    if (xml != buffer.data()) {
        qDebug() << "expect " << xml;
        qDebug() << "writing" << buffer.data();
    }
    QCOMPARE(buffer.data(), xml);

    // parse
    QStringList errors;
    QVariant test = QXmppRpcMarshaller::demarshall(xmlToDom(xml), errors);
    if (!errors.isEmpty()) {
        qDebug() << errors;
    }
    QCOMPARE(errors, QStringList());
    QCOMPARE(test, value);
}

class tst_QXmppIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testBasic_data();
    Q_SLOT void testBasic();

    // NonSaslAuthIq
    Q_SLOT void nonSaslAuthGet();
    Q_SLOT void nonSaslAuthSetPlain();
    Q_SLOT void nonSaslAuthSetDigest();

    // PushEnableIq
    Q_SLOT void pushEnable();
    Q_SLOT void pushDisable();
    Q_SLOT void pushEnableXmlNs();
    Q_SLOT void pushEnableDataForm();
    Q_SLOT void pushEnableIsEnableIq();

    // RpcIq
    Q_SLOT void rpcBase64();
    Q_SLOT void rpcBool();
    Q_SLOT void rpcDateTime();
    Q_SLOT void rpcDouble();
    Q_SLOT void rpcInt();
    Q_SLOT void rpcNil();
    Q_SLOT void rpcString();

    Q_SLOT void rpcArray();
    Q_SLOT void rpcStruct();

    Q_SLOT void rpcInvoke();
    Q_SLOT void rpcResponse();
    Q_SLOT void rpcResponseFault();
};

void tst_QXmppIq::testBasic_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<int>("type");

    QTest::newRow("get")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="get"/>)")
        << int(QXmppIq::Get);

    QTest::newRow("set")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="set"/>)")
        << int(QXmppIq::Set);

    QTest::newRow("result")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="result"/>)")
        << int(QXmppIq::Result);

    QTest::newRow("error")
        << QByteArray(R"(<iq id='a' to="foo@example.com/QXmpp" from="bar@example.com/QXmpp" type="error"/>)")
        << int(QXmppIq::Error);
}

void tst_QXmppIq::testBasic()
{
    QFETCH(QByteArray, xml);
    QFETCH(int, type);

    QXmppIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.id(), u"a");
    QCOMPARE(iq.to(), u"foo@example.com/QXmpp"_s);
    QCOMPARE(iq.from(), u"bar@example.com/QXmpp"_s);
    QCOMPARE(int(iq.type()), type);
    serializePacket(iq, xml);
}

void tst_QXmppIq::nonSaslAuthGet()
{
    // Client requests authentication fields from server
    const QByteArray xml(
        "<iq id=\"auth1\" to=\"shakespeare.lit\" type=\"get\">"
        "<query xmlns=\"jabber:iq:auth\"/>"
        "</iq>");

    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    serializePacket(iq, xml);
}

void tst_QXmppIq::nonSaslAuthSetPlain()
{
    // Client provides required information (plain)
    const QByteArray xml(
        "<iq id=\"auth2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:auth\">"
        "<username>bill</username>"
        "<password>Calli0pe</password>"
        "<resource>globe</resource>"
        "</query>"
        "</iq>");
    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.digest(), QByteArray());
    QCOMPARE(iq.password(), QLatin1String("Calli0pe"));
    QCOMPARE(iq.resource(), QLatin1String("globe"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::nonSaslAuthSetDigest()
{
    // Client provides required information (digest)
    const QByteArray xml(
        "<iq id=\"auth2\" type=\"set\">"
        "<query xmlns=\"jabber:iq:auth\">"
        "<username>bill</username>"
        "<digest>48fc78be9ec8f86d8ce1c39c320c97c21d62334d</digest>"
        "<resource>globe</resource>"
        "</query>"
        "</iq>");
    QXmppNonSASLAuthIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.username(), QLatin1String("bill"));
    QCOMPARE(iq.digest(), QByteArray("\x48\xfc\x78\xbe\x9e\xc8\xf8\x6d\x8c\xe1\xc3\x9c\x32\x0c\x97\xc2\x1d\x62\x33\x4d"));
    QCOMPARE(iq.password(), QString());
    QCOMPARE(iq.resource(), QLatin1String("globe"));
    serializePacket(iq, xml);
}

void tst_QXmppIq::pushEnable()
{
    const QByteArray xml(
        R"(<iq id="x42" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.mode(), QXmppPushEnableIq::Enable);
    QCOMPARE(iq.jid(), u"push-5.client.example"_s);
    QCOMPARE(iq.node(), u"yxs32uqsflafdk3iuqo"_s);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;
    sIq.setJid("push-5.client.example");
    sIq.setMode(QXmppPushEnableIq::Enable);
    sIq.setNode("yxs32uqsflafdk3iuqo");
    sIq.setType(QXmppIq::Set);
    sIq.setId("x42");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushDisable()
{
    const QByteArray xml(
        R"(<iq id="x97" type="set">)"
        R"(<disable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.mode(), QXmppPushEnableIq::Disable);
    QCOMPARE(iq.jid(), u"push-5.client.example"_s);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;
    sIq.setJid("push-5.client.example");
    sIq.setMode(QXmppPushEnableIq::Disable);
    sIq.setNode("yxs32uqsflafdk3iuqo");
    sIq.setType(QXmppIq::Set);
    sIq.setId("x97");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushEnableXmlNs()
{
    const QByteArray xml(
        R"(<iq type="set" id="x97">)"
        R"(<disable xmlns="urn:ympp:wrongns:0" jid="push-5.client.example"/>)"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.jid().isEmpty());
}

void tst_QXmppIq::pushEnableDataForm()
{
    const QByteArray xml(
        R"(<iq id="x43" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo">)"
        R"(<x xmlns="jabber:x:data" type="submit">)"
        R"(<field type="hidden" var="FORM_TYPE"><value>http://jabber.org/protocol/pubsub#publish-options</value></field>)"
        R"(<field type="text-single" var="secret"><value>eruio234vzxc2kla-91</value></field>)"
        "</x>"
        "</enable>"
        "</iq>");

    QXmppPushEnableIq iq;
    parsePacket(iq, xml);
    QVERIFY(!iq.dataForm().isNull());
    QCOMPARE(iq.dataForm().constFields().size(), 2);

    serializePacket(iq, xml);

    QXmppPushEnableIq sIq;

    QXmppDataForm::Field field0;
    field0.setKey("FORM_TYPE");
    field0.setType(QXmppDataForm::Field::HiddenField);
    field0.setValue("http://jabber.org/protocol/pubsub#publish-options");

    QXmppDataForm::Field field1;
    field1.setKey("secret");
    field1.setValue("eruio234vzxc2kla-91");

    QXmppDataForm form;
    form.setType(QXmppDataForm::Submit);
    form.setFields({ field0, field1 });

    sIq.setDataForm(form);

    sIq.setType(QXmppIq::Set);
    sIq.setMode(QXmppPushEnableIq::Enable);
    sIq.setId("x43");
    sIq.setJid("push-5.client.example");
    sIq.setNode("yxs32uqsflafdk3iuqo");

    serializePacket(sIq, xml);
}

void tst_QXmppIq::pushEnableIsEnableIq()
{
    const QByteArray xml(
        R"(<iq id="x42" type="set">)"
        R"(<enable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QVERIFY(QXmppPushEnableIq::isPushEnableIq(xmlToDom(xml)));

    const QByteArray xml2(
        R"(<iq id="x97" type="set">)"
        R"(<disable xmlns="urn:xmpp:push:0" jid="push-5.client.example" node="yxs32uqsflafdk3iuqo"/>)"
        "</iq>");

    QVERIFY(QXmppPushEnableIq::isPushEnableIq(xmlToDom(xml2)));
}

void tst_QXmppIq::rpcBase64()
{
    checkVariant(QByteArray("\0\1\2\3", 4),
                 QByteArray("<value><base64>AAECAw==</base64></value>"));
}

void tst_QXmppIq::rpcBool()
{
    checkVariant(false,
                 QByteArray("<value><boolean>false</boolean></value>"));
    checkVariant(true,
                 QByteArray("<value><boolean>true</boolean></value>"));
}

void tst_QXmppIq::rpcDateTime()
{
    checkVariant(QDateTime(QDate(1998, 7, 17), QTime(14, 8, 55)),
                 QByteArray("<value><dateTime.iso8601>1998-07-17T14:08:55</dateTime.iso8601></value>"));
}

void tst_QXmppIq::rpcDouble()
{
    checkVariant(double(-12.214),
                 QByteArray("<value><double>-12.214</double></value>"));
}

void tst_QXmppIq::rpcInt()
{
    checkVariant(int(-12),
                 QByteArray("<value><i4>-12</i4></value>"));
}

void tst_QXmppIq::rpcNil()
{
    checkVariant(QVariant(),
                 QByteArray("<value><nil/></value>"));
}

void tst_QXmppIq::rpcString()
{
    checkVariant(u"hello world"_s,
                 QByteArray("<value><string>hello world</string></value>"));
}

void tst_QXmppIq::rpcArray()
{
    checkVariant(QVariantList() << u"hello world"_s << double(-12.214),
                 QByteArray("<value><array><data>"
                            "<value><string>hello world</string></value>"
                            "<value><double>-12.214</double></value>"
                            "</data></array></value>"));
}

void tst_QXmppIq::rpcStruct()
{
    QMap<QString, QVariant> map;
    map["bar"] = u"hello \n world"_s;
    map["foo"] = double(-12.214);
    checkVariant(map,
                 QByteArray("<value><struct>"
                            "<member>"
                            "<name>bar</name>"
                            "<value><string>hello \n world</string></value>"
                            "</member>"
                            "<member>"
                            "<name>foo</name>"
                            "<value><double>-12.214</double></value>"
                            "</member>"
                            "</struct></value>"));
}

void tst_QXmppIq::rpcInvoke()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"responder@company-a.com/jrpc-server\""
        " from=\"requester@company-b.com/jrpc-client\""
        " type=\"set\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodCall>"
        "<methodName>examples.getStateName</methodName>"
        "<params>"
        "<param>"
        "<value><i4>6</i4></value>"
        "</param>"
        "<param>"
        "<value><string>two\nlines</string></value>"
        "</param>"
        "<param>"
        "<value><string><![CDATA[\n\n]]></string></value>"
        "</param>"
        "</params>"
        "</methodCall>"
        "</query>"
        "</iq>");

    QXmppRpcInvokeIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.method(), u"examples.getStateName");
    QCOMPARE(iq.arguments(), QVariantList() << int(6) << u"two\nlines"_s << u"\n\n"_s);

    const auto data = packetToXml(iq);
    if (data != xml) {
        qDebug() << "expect " << xml;
        qDebug() << "writing" << data;
    }
    QCOMPARE(data, xml);
}

void tst_QXmppIq::rpcResponse()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"requester@company-b.com/jrpc-client\""
        " from=\"responder@company-a.com/jrpc-server\""
        " type=\"result\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodResponse>"
        "<params>"
        "<param>"
        "<value><string>Colorado</string></value>"
        "</param>"
        "</params>"
        "</methodResponse>"
        "</query>"
        "</iq>");

    QXmppRpcResponseIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.faultCode(), 0);
    QCOMPARE(iq.faultString(), QString());
    QCOMPARE(iq.values(), QVariantList() << u"Colorado"_s);
    serializePacket(iq, xml);
}

void tst_QXmppIq::rpcResponseFault()
{
    const QByteArray xml(
        "<iq"
        " id=\"rpc1\""
        " to=\"requester@company-b.com/jrpc-client\""
        " from=\"responder@company-a.com/jrpc-server\""
        " type=\"result\">"
        "<query xmlns=\"jabber:iq:rpc\">"
        "<methodResponse>"
        "<fault>"
        "<value>"
        "<struct>"
        "<member>"
        "<name>faultCode</name>"
        "<value><i4>404</i4></value>"
        "</member>"
        "<member>"
        "<name>faultString</name>"
        "<value><string>Not found</string></value>"
        "</member>"
        "</struct>"
        "</value>"
        "</fault>"
        "</methodResponse>"
        "</query>"
        "</iq>");

    QXmppRpcResponseIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.faultCode(), 404);
    QCOMPARE(iq.faultString(), QLatin1String("Not found"));
    QCOMPARE(iq.values(), QVariantList());
    serializePacket(iq, xml);
}

QTEST_MAIN(tst_QXmppIq)
#include "tst_QXmppIq.moc"

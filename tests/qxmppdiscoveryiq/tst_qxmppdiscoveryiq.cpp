// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppContactAddresses.h"
#include "QXmppDiscoveryIq.h"

#include "util.h"

#include <QObject>

class tst_QXmppDiscoveryIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testDiscovery();
    Q_SLOT void testDiscoveryWithForm();
    Q_SLOT void discoInfo();
    Q_SLOT void discoItems();
    Q_SLOT void contactAddresses();
};

void tst_QXmppDiscoveryIq::testDiscovery()
{
    const QByteArray xml(
        "<iq id=\"disco1\" from=\"benvolio@capulet.lit/230193\" type=\"result\">"
        "<query xmlns=\"http://jabber.org/protocol/disco#info\">"
        "<identity category=\"client\" name=\"Exodus 0.9.1\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "</query>"
        "</iq>");

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppDiscoveryIq disco;
    QT_WARNING_POP
    parsePacket(disco, xml);
    QCOMPARE(disco.verificationString(), QByteArray::fromBase64("QgayPKawpkPSDYmwT/WM94uAlu0="));
    serializePacket(disco, xml);
}

void tst_QXmppDiscoveryIq::testDiscoveryWithForm()
{
    const QByteArray xml(
        "<iq id=\"disco1\" to=\"juliet@capulet.lit/chamber\" from=\"benvolio@capulet.lit/230193\" type=\"result\">"
        "<query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"http://psi-im.org#q07IKJEyjvHSyhy//CH0CxmKi8w=\">"
        "<identity xml:lang=\"en\" category=\"client\" name=\"Psi 0.11\" type=\"pc\"/>"
        "<identity xml:lang=\"el\" category=\"client\" name=\"Ψ 0.11\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "<x xmlns=\"jabber:x:data\" type=\"result\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>urn:xmpp:dataforms:softwareinfo</value>"
        "</field>"
        "<field type=\"text-multi\" var=\"ip_version\">"
        "<value>ipv4</value>"
        "<value>ipv6</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os\">"
        "<value>Mac</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os_version\">"
        "<value>10.5.1</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software\">"
        "<value>Psi</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software_version\">"
        "<value>0.11</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>");

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QXmppDiscoveryIq disco;
    QT_WARNING_POP
    parsePacket(disco, xml);
    QCOMPARE(disco.verificationString(), QByteArray::fromBase64("q07IKJEyjvHSyhy//CH0CxmKi8w="));
    serializePacket(disco, xml);

    auto softinfoForm = disco.dataForm(u"urn:xmpp:dataforms:softwareinfo");
    QVERIFY(softinfoForm.has_value());
}

void tst_QXmppDiscoveryIq::discoInfo()
{
    const auto xml = QByteArrayLiteral(
        "<query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"http://psi-im.org#q07IKJEyjvHSyhy//CH0CxmKi8w=\">"
        "<identity xml:lang=\"en\" category=\"client\" name=\"Psi 0.11\" type=\"pc\"/>"
        "<identity xml:lang=\"el\" category=\"client\" name=\"Ψ 0.11\" type=\"pc\"/>"
        "<feature var=\"http://jabber.org/protocol/caps\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#info\"/>"
        "<feature var=\"http://jabber.org/protocol/disco#items\"/>"
        "<feature var=\"http://jabber.org/protocol/muc\"/>"
        "<x xmlns=\"jabber:x:data\" type=\"result\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\">"
        "<value>urn:xmpp:dataforms:softwareinfo</value>"
        "</field>"
        "<field type=\"text-multi\" var=\"ip_version\">"
        "<value>ipv4</value>"
        "<value>ipv6</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os\">"
        "<value>Mac</value>"
        "</field>"
        "<field type=\"text-single\" var=\"os_version\">"
        "<value>10.5.1</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software\">"
        "<value>Psi</value>"
        "</field>"
        "<field type=\"text-single\" var=\"software_version\">"
        "<value>0.11</value>"
        "</field>"
        "</x>"
        "</query>");

    auto info = unwrap(QXmppDiscoInfo::fromDom(xmlToDom(xml)));
    QCOMPARE(info.calculateEntityCapabilitiesHash(), QByteArray::fromBase64("q07IKJEyjvHSyhy//CH0CxmKi8w="));
    serializePacket(info, xml);
}

void tst_QXmppDiscoveryIq::discoItems()
{
    const auto xml = QByteArrayLiteral(
        "<query xmlns='http://jabber.org/protocol/disco#items'>"
        "<item jid='368866411b877c30064a5f62b917cffe@test.org'/>"
        "<item jid='3300659945416e274474e469a1f0154c@test.org'/>"
        "<item jid='4e30f35051b7b8b42abe083742187228@test.org'/>"
        "<item jid='ae890ac52d0df67ed7cfdf51b644e901@test.org'/>"
        "</query>");

    auto items = unwrap(QXmppDiscoItems::fromDom(xmlToDom(xml)));
    QCOMPARE(items.items().size(), 4);
    QCOMPARE(items.items().at(0).jid(), u"368866411b877c30064a5f62b917cffe@test.org");
    serializePacket(items, xml);
}

void tst_QXmppDiscoveryIq::contactAddresses()
{
    auto xml = QByteArrayLiteral(
        "<x xmlns='jabber:x:data' type='result'>"
        "<field type='hidden' var='FORM_TYPE'>"
        "<value>http://jabber.org/network/serverinfo</value>"
        "</field>"
        "<field type='list-multi' var='abuse-addresses'>"
        "<value>mailto:abuse@shakespeare.lit</value>"
        "<value>xmpp:abuse@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='admin-addresses'>"
        "<value>mailto:xmpp@shakespeare.lit</value>"
        "<value>xmpp:admins@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='feedback-addresses'>"
        "<value>http://shakespeare.lit/feedback.php</value>"
        "<value>mailto:feedback@shakespeare.lit</value>"
        "<value>xmpp:feedback@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='sales-addresses'>"
        "<value>xmpp:bard@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='security-addresses'>"
        "<value>xmpp:security@shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='status-addresses'>"
        "<value>https://status.shakespeare.lit</value>"
        "</field>"
        "<field type='list-multi' var='support-addresses'>"
        "<value>http://shakespeare.lit/support.php</value>"
        "<value>xmpp:support@shakespeare.lit</value>"
        "</field>"
        "</x>");

    QXmppDataForm form;
    parsePacket(form, xml);

    auto parsed = QXmppContactAddresses::fromDataForm(form);
    QVERIFY(parsed.has_value());

    QCOMPARE(parsed->abuseAddresses(), (QStringList { u"mailto:abuse@shakespeare.lit"_s, u"xmpp:abuse@shakespeare.lit"_s }));

    form = parsed->toDataForm();
    form.setType(QXmppDataForm::Result);
    QVERIFY(!form.isNull());
    xml = QString::fromUtf8(xml).remove(QChar('\n')).toUtf8();
    serializePacket(form, xml);

    // findForm with parsing
    QXmppDiscoInfo info;
    info.setDataForms({ parsed->toDataForm() });
    auto contactAddresses = info.dataForm<QXmppContactAddresses>();
    QVERIFY(contactAddresses);
    QCOMPARE(contactAddresses->supportAddresses().constFirst(), u"http://shakespeare.lit/support.php");
}

QTEST_MAIN(tst_QXmppDiscoveryIq)
#include "tst_qxmppdiscoveryiq.moc"

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMessage.h"
#include "QXmppMucForms.h"
#include "QXmppMucManagerV2.h"
#include "QXmppPresence.h"
#include "QXmppPubSubManager.h"
#include "QXmppPubSubSubAuthorization.h"

#include "TestClient.h"

using namespace QXmpp;
using namespace QXmpp::Private;

class tst_QXmppMuc : public QObject
{
    Q_OBJECT

private:
    // PEP bookmarks
    Q_SLOT void bookmarks2Updates();
    Q_SLOT void bookmarks2Set();
    Q_SLOT void bookmarks2Remove();

    // MUC avatars
    Q_SLOT void avatarFetch();

    // MUC joining
    Q_SLOT void joinRoom();

    // muc#roominfo form
    Q_SLOT void roomInfoForm();
};

void tst_QXmppMuc::bookmarks2Updates()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    QSignalSpy resetSignal(muc, &QXmppMucManagerV2::bookmarksReset);
    QSignalSpy addedSignal(muc, &QXmppMucManagerV2::bookmarksAdded);
    QSignalSpy changedSignal(muc, &QXmppMucManagerV2::bookmarksChanged);
    QSignalSpy removedSignal(muc, &QXmppMucManagerV2::bookmarksRemoved);

    QVERIFY(!muc->bookmarks().has_value());

    muc->onConnected();
    test.expect(u"<iq id='qx1' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='urn:xmpp:bookmarks:1'/></pubsub></iq>"_s);
    test.inject(u"<iq id='qx1' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='urn:xmpp:bookmarks:1'>"
                u"<item id='theplay@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Play&apos;s the Thing' autojoin='true'><nick>JC</nick></conference></item>"
                u"<item id='orchard@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Orcard' autojoin='1'><nick>JC</nick><extensions><state xmlns='http://myclient.example/bookmark/state' minimized='true'/></extensions></conference></item>"
                u"</items></pubsub></iq>"_s);

    QCOMPARE(resetSignal.size(), 1);

    QVERIFY(muc->bookmarks().has_value());
    QCOMPARE(muc->bookmarks()->size(), 2);

    test.inject(u"<message from='juliet@capulet.lit' to='juliet@capulet.lit/balcony' type='headline' id='removed-room1'>"
                "<event xmlns='http://jabber.org/protocol/pubsub#event'>"
                "<items node='urn:xmpp:bookmarks:1'><retract id='theplay@conference.shakespeare.lit'/></items>"
                "</event></message>"_s);
    QCOMPARE(removedSignal.size(), 1);

    test.inject(u"<message from='juliet@capulet.lit' to='juliet@capulet.lit/balcony' type='headline' id='new-room1'>"
                "<event xmlns='http://jabber.org/protocol/pubsub#event'>"
                "<items node='urn:xmpp:bookmarks:1'>"
                "<item id='theplay@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Play&apos;s the Thing'><nick>JC</nick></conference></item>"
                "</items>"
                "</event>"
                "</message>"_s);
    test.inject(u"<message from='juliet@capulet.lit' to='juliet@capulet.lit/balcony' type='headline' id='new-room2'>"
                "<event xmlns='http://jabber.org/protocol/pubsub#event'>"
                "<items node='urn:xmpp:bookmarks:1'>"
                "<item id='theplay@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Play&apos;s the Thing' autojoin='1'><nick>JC</nick></conference></item>"
                "</items>"
                "</event>"
                "</message>"_s);
    QCOMPARE(addedSignal.size(), 1);
    QCOMPARE(changedSignal.size(), 1);
}

void tst_QXmppMuc::bookmarks2Set()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->setBookmark(QXmppMucBookmark { u"theplay@conference.shakespeare.lit"_s, u"The Play's the Thing"_s, true, u"JC"_s, {} });
    test.expect(
        u"<iq id='qx1' type='set'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<publish node='urn:xmpp:bookmarks:1'>"
        "<item id='theplay@conference.shakespeare.lit'>"
        "<conference xmlns='urn:xmpp:bookmarks:1' autojoin='true' name=\"The Play's the Thing\"><nick>JC</nick></conference></item>"
        "</publish>"
        "<publish-options>"
        "<x xmlns='jabber:x:data' type='submit'>"
        "<field type='hidden' var='FORM_TYPE'><value>http://jabber.org/protocol/pubsub#publish-options</value></field>"
        "<field type='list-single' var='pubsub#access_model'><value>whitelist</value></field>"
        "<field type='text-single' var='pubsub#max_items'><value>max</value></field>"
        "<field type='boolean' var='pubsub#persist_items'><value>true</value></field>"
        "<field type='list-single' var='pubsub#send_last_published_item'><value>never</value></field>"
        "</x>"
        "</publish-options>"
        "</pubsub>"
        "</iq>"_s);
    test.inject(u"<iq to='juliet@capulet.lit/balcony' type='result' id='qx1'/>"_s);

    expectFutureVariant<Success>(task);
}

void tst_QXmppMuc::bookmarks2Remove()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->removeBookmark(u"theplay@conference.shakespeare.lit"_s);

    test.expect(
        u"<iq id='qx1' to='juliet@capulet.lit' type='set'>"
        "<pubsub xmlns='http://jabber.org/protocol/pubsub'>"
        "<retract node='urn:xmpp:bookmarks:1' notify='true'>"
        "<item id='theplay@conference.shakespeare.lit'/>"
        "</retract>"
        "</pubsub>"
        "</iq>"_s);
    test.inject(u"<iq id='qx1' type='result'/>"_s);

    expectFutureVariant<Success>(task);
}

void tst_QXmppMuc::avatarFetch()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto muc = test.addNewExtension<QXmppMucManagerV2>();

    auto avatarTask = muc->fetchRoomAvatar(u"garden@chat.shakespeare.example.org"_s);
    test.inject(
        u"<iq type='result' id='qx1' to='juliet@capulet.lit/balcony' from='garden@chat.shakespeare.example.org'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'>"
        "<identity category='conference' type='text' name='The Garden'/>"
        "<feature var='http://jabber.org/protocol/muc'/>"
        "<feature var='vcard-temp'/>"
        "<x xmlns='jabber:x:data' type='result'>"
        "<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>"
        "<field var='muc#roominfo_avatarhash' type='text-multi' label='Avatar hash'><value>a31c4bd04de69663cfd7f424a8453f4674da37ff</value></field>"
        "</x>"
        "</query>"
        "</iq>"_s);

    test.inject(
        u"<iq type='result' id='qx3' to='juliet@capulet.example.com/balcony' from='garden@chat.shakespeare.example.org'><vCard xmlns='vcard-temp'>"
        "<PHOTO>"
        "<TYPE>image/svg+xml</TYPE>"
        "<BINVAL>PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIzMiIgaGVpZ2h0PSIzMiI+CiA8cmVjdCB4PSIwIiB5PSIwIiB3aWR0aD0iMzIiIGhlaWdodD0iMzIiIGZpbGw9InJlZCIvPgo8L3N2Zz4K</BINVAL>"
        "</PHOTO>"
        "</vCard></iq>"_s);

    auto avatar = expectFutureVariant<std::optional<QXmppMucManagerV2::Avatar>>(avatarTask);
    QVERIFY(avatar.has_value());
    QCOMPARE(avatar->contentType, u"image/svg+xml"_s);
}

void tst_QXmppMuc::joinRoom()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    // Inject self-presence (status 110): transitions JoiningOccupantPresences → JoiningRoomHistory
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    // Inject subject message: transitions JoiningRoomHistory → Joined and resolves task
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);

    auto room = expectFutureVariant<QXmppMucRoomV2>(task);
    QVERIFY(room.isValid());
    QCOMPARE(room.subject().value(), u"Cauldron"_s);
}

void tst_QXmppMuc::roomInfoForm()
{
    QByteArray xml(R"(
<x xmlns='jabber:x:data' type='result'>
<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>
<field var='muc#roominfo_description' label='Description'><value>The place for all good witches!</value></field>
<field var='muc#roominfo_contactjid' label='Contact Addresses'><value>crone1@shakespeare.lit</value></field>
<field var='muc#roominfo_subject' label='Current Discussion Topic'><value>Spells</value></field>
<field var='muc#roominfo_subjectmod' label='Subject can be modified'><value>true</value></field>
<field var='muc#roominfo_occupants' label='Number of occupants'><value>3</value></field>
<field var='muc#roominfo_ldapgroup' label='Associated LDAP Group'><value>cn=witches,dc=shakespeare,dc=lit</value></field>
<field var='muc#roominfo_lang' label='Language of discussion'><value>en</value></field>
<field var='muc#roominfo_logs' label='URL for discussion logs'><value>http://www.shakespeare.lit/chatlogs/coven/</value></field>
<field var='muc#maxhistoryfetch' label='Maximum Number of History Messages Returned by Room'><value>50</value></field>
<field var='muc#roominfo_pubsub' label='Associated pubsub node'><value>xmpp:pubsub.shakespeare.lit?;node=the-coven-node</value></field>
<field var='muc#roominfo_avatarhash' type='text-multi' label='Avatar hash'><value>a31c4bd04de69663cfd7f424a8453f4674da37ff</value><value>b9b256f999ded52c2fa14fb007c2e5b979450cbb</value></field>
</x>)");

    QXmppDataForm form;
    parsePacket(form, xml);

    auto roomInfo = QXmppMucRoomInfo::fromDataForm(form);
    QCOMPARE(roomInfo->description(), u"The place for all good witches!");
    QCOMPARE(roomInfo->contactJids(), QStringList { u"crone1@shakespeare.lit"_s });
    QCOMPARE(roomInfo->subject(), u"Spells");
    QVERIFY(roomInfo->subjectChangeable().has_value());
    QCOMPARE(roomInfo->subjectChangeable(), std::optional { true });
    QCOMPARE(roomInfo->occupants(), 3);
    QCOMPARE(roomInfo->language(), u"en");
    QCOMPARE(roomInfo->maxHistoryFetch(), 50);
    auto hashes = QStringList { u"a31c4bd04de69663cfd7f424a8453f4674da37ff"_s, u"b9b256f999ded52c2fa14fb007c2e5b979450cbb"_s };
    QCOMPARE(roomInfo->avatarHashes(), hashes);

    form = roomInfo->toDataForm();
    QVERIFY(!form.isNull());
    auto xml2 = QByteArrayLiteral(
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<field type=\"hidden\" var=\"FORM_TYPE\"><value>http://jabber.org/protocol/muc#roominfo</value></field>"
        "<field type=\"text-single\" var=\"muc#maxhistoryfetch\"><value>50</value></field>"
        "<field type=\"jid-multi\" var=\"muc#roominfo_contactjid\"><value>crone1@shakespeare.lit</value></field>"
        "<field type=\"text-single\" var=\"muc#roominfo_description\"><value>The place for all good witches!</value></field>"
        "<field type=\"text-single\" var=\"muc#roominfo_lang\"><value>en</value></field>"
        "<field type=\"text-single\" var=\"muc#roominfo_occupants\"><value>3</value></field>"
        "<field type=\"text-single\" var=\"muc#roominfo_subject\"><value>Spells</value></field>"
        "<field type=\"boolean\" var=\"muc#roominfo_subjectmod\"><value>true</value></field>"
        "<field type='text-multi' var='muc#roominfo_avatarhash'><value>a31c4bd04de69663cfd7f424a8453f4674da37ff</value><value>b9b256f999ded52c2fa14fb007c2e5b979450cbb</value></field>"
        "</x>");
    serializePacket(form, xml2);
}

QTEST_MAIN(tst_QXmppMuc)
#include "tst_qxmppmuc.moc"

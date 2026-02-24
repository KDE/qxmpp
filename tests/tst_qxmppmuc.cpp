// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMessage.h"
#include "QXmppMucForms.h"
#include "QXmppMucManagerV2.h"
#include "QXmppMucManagerV2_p.h"
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
    Q_SLOT void joinRoomWithHistory();
    Q_SLOT void joinRoomWithPassword();
    Q_SLOT void joinRoomTimeout();
    Q_SLOT void joinRoomTimerStopped();

    // MUC messages
    Q_SLOT void receiveMessage();
    Q_SLOT void sendMessage();
    Q_SLOT void sendMessageError();
    Q_SLOT void sendPrivateMessage();
    Q_SLOT void setSubject();
    Q_SLOT void changeNickname();
    Q_SLOT void participantNicknameChange();
    Q_SLOT void participantJoinLeave();
    Q_SLOT void participantsList();
    Q_SLOT void participantKicked();
    Q_SLOT void selfBanned();
    Q_SLOT void changePresence();
    Q_SLOT void leaveRoom();
    Q_SLOT void leaveRoomTimeout();

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

void tst_QXmppMuc::joinRoomWithHistory()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    Muc::HistoryOptions historyOpts;
    historyOpts.setMaxStanzas(20);

    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s, historyOpts);
    test.expect(
        u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'>"
        u"<history maxstanzas=\"20\"/></x></presence>"_s);

    // Inject self-presence
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    // Inject subject message
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);

    auto room = expectFutureVariant<QXmppMucRoomV2>(task);
    QVERIFY(room.isValid());
}

void tst_QXmppMuc::joinRoomWithPassword()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s, std::nullopt, u"cauldronburn"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'><password>cauldronburn</password></x></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);

    auto room = expectFutureVariant<QXmppMucRoomV2>(task);
    QVERIFY(room.isValid());
}

void tst_QXmppMuc::joinRoomTimeout()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Set a short timeout for testing (100ms)
    muc->d->timeout = std::chrono::milliseconds(100);

    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    // Don't inject any response, let the timer expire
    QTest::qWait(150);

    // Task should fail with timeout error
    QVERIFY(task.isFinished());
    auto result = expectVariant<QXmppError>(task.result());
    QVERIFY(result.description.contains(u"timed out"_s));
}

void tst_QXmppMuc::joinRoomTimerStopped()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Set a short timeout for testing (1 second)
    muc->d->timeout = std::chrono::milliseconds(1000);

    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    // Inject self-presence (status 110)
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    // Inject subject message: this should complete the join and stop the timer
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);

    auto room = expectFutureVariant<QXmppMucRoomV2>(task);
    QVERIFY(room.isValid());

    // Wait to ensure timer doesn't fire after join completion
    QTest::qWait(1500);

    // Task should still be completed successfully (not timed out)
    QVERIFY(task.isFinished());
    auto result = expectVariant<QXmppMucRoomV2>(task.result());
    QVERIFY(result.isValid());
}

void tst_QXmppMuc::receiveMessage()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room first
    auto task = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    expectFutureVariant<QXmppMucRoomV2>(task);

    // Now receive a live message
    QXmppMessage receivedMsg;
    QObject::connect(muc, &QXmppMucManagerV2::messageReceived, muc, [&](const QString &roomJid, const QXmppMessage &msg) {
        QCOMPARE(roomJid, u"coven@chat.shakespeare.lit"_s);
        receivedMsg = msg;
    });

    QXmppMessage liveMsg;
    parsePacket(liveMsg,
                "<message from='coven@chat.shakespeare.lit/firstwitch' type='groupchat'>"
                "<body>Thrice the brinded cat hath mew'd.</body>"
                "</message>");
    QVERIFY(muc->handleMessage(liveMsg));
    QCOMPARE(receivedMsg.body(), u"Thrice the brinded cat hath mew'd."_s);
}

void tst_QXmppMuc::sendMessage()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Send a message
    QXmppMessage msgToSend;
    msgToSend.setBody(u"Thrice the brinded cat hath mew'd."_s);
    auto sendTask = room.sendMessage(std::move(msgToSend));
    QVERIFY(!sendTask.isFinished());

    // Verify the sent XML contains origin-id and correct structure
    auto sent = test.takePacket();
    QXmppMessage sentMsg;
    parsePacket(sentMsg, sent.toUtf8());
    QCOMPARE(sentMsg.type(), QXmppMessage::GroupChat);
    QCOMPARE(sentMsg.to(), u"coven@chat.shakespeare.lit"_s);
    QCOMPARE(sentMsg.body(), u"Thrice the brinded cat hath mew'd."_s);
    QVERIFY(!sentMsg.originId().isEmpty());

    // Inject reflected message with same origin-id
    QXmppMessage reflected;
    parsePacket(reflected,
                QStringLiteral("<message from='coven@chat.shakespeare.lit/thirdwitch' type='groupchat'>"
                               "<body>Thrice the brinded cat hath mew'd.</body>"
                               "<origin-id xmlns='urn:xmpp:sid:0' id='%1'/>"
                               "</message>")
                    .arg(sentMsg.originId())
                    .toUtf8());
    muc->handleMessage(reflected);

    QVERIFY(sendTask.isFinished());
    expectVariant<QXmpp::Success>(sendTask.result());
}

void tst_QXmppMuc::sendMessageError()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Send a message
    QXmppMessage msgToSend;
    msgToSend.setBody(u"Hello!"_s);
    auto sendTask = room.sendMessage(std::move(msgToSend));
    QVERIFY(!sendTask.isFinished());

    auto sent = test.takePacket();
    QXmppMessage sentMsg;
    parsePacket(sentMsg, sent.toUtf8());

    // Inject error response with same origin-id
    QXmppMessage errorMsg;
    parsePacket(errorMsg,
                QStringLiteral("<message from='coven@chat.shakespeare.lit' type='error'>"
                               "<origin-id xmlns='urn:xmpp:sid:0' id='%1'/>"
                               "<error type='auth'><forbidden xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                               "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>You are not allowed to send messages</text>"
                               "</error>"
                               "</message>")
                    .arg(sentMsg.originId())
                    .toUtf8());
    muc->handleMessage(errorMsg);

    QVERIFY(sendTask.isFinished());
    auto error = expectVariant<QXmppError>(sendTask.result());
    QVERIFY(!error.description.isEmpty());
}

void tst_QXmppMuc::sendPrivateMessage()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room — firstwitch is already in the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence firstWitchPresence;
    parsePacket(firstWitchPresence,
                "<presence from='coven@chat.shakespeare.lit/firstwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchPresence);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // firstwitch was the first participant injected, so has ID 0
    auto participant = QXmppMucParticipant(muc, u"coven@chat.shakespeare.lit"_s, 0);
    QVERIFY(participant.isValid());
    QCOMPARE(participant.nickname().value(), u"firstwitch"_s);

    // Send a private message
    QXmppMessage pm;
    pm.setBody(u"I'll give thee a wind."_s);
    room.sendPrivateMessage(participant, std::move(pm));

    // Verify the sent XML
    auto sent = test.takePacket();
    QXmppMessage sentMsg;
    parsePacket(sentMsg, sent.toUtf8());
    QCOMPARE(sentMsg.type(), QXmppMessage::Chat);
    QCOMPARE(sentMsg.to(), u"coven@chat.shakespeare.lit/firstwitch"_s);
    QCOMPARE(sentMsg.body(), u"I'll give thee a wind."_s);
}

void tst_QXmppMuc::setSubject()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);
    QCOMPARE(room.subject().value(), u"Cauldron"_s);

    // Change subject
    auto subjectTask = room.setSubject(u"New Spells"_s);
    QVERIFY(!subjectTask.isFinished());

    // Verify the sent XML
    auto sent = test.takePacket();
    QXmppMessage sentMsg;
    parsePacket(sentMsg, sent.toUtf8());
    QCOMPARE(sentMsg.type(), QXmppMessage::GroupChat);
    QCOMPARE(sentMsg.subject(), u"New Spells"_s);
    QVERIFY(sentMsg.body().isEmpty());
    QVERIFY(!sentMsg.originId().isEmpty());

    // Inject reflected subject message with same origin-id
    QXmppMessage reflected;
    parsePacket(reflected,
                QStringLiteral("<message from='coven@chat.shakespeare.lit/thirdwitch' type='groupchat'>"
                               "<subject>New Spells</subject>"
                               "<origin-id xmlns='urn:xmpp:sid:0' id='%1'/>"
                               "</message>")
                    .arg(sentMsg.originId())
                    .toUtf8());
    muc->handleMessage(reflected);

    QVERIFY(subjectTask.isFinished());
    expectVariant<QXmpp::Success>(subjectTask.result());
    QCOMPARE(room.subject().value(), u"New Spells"_s);
}

void tst_QXmppMuc::changeNickname()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);
    QCOMPARE(room.nickname().value(), u"thirdwitch"_s);

    // Change nickname
    auto nickTask = room.setNickname(u"oldhag"_s);
    QVERIFY(!nickTask.isFinished());

    // Verify presence sent to new nick
    test.expect(u"<presence to='coven@chat.shakespeare.lit/oldhag'/>"_s);

    // Inject unavailable presence with 303 (old nick goes away, new nick in item)
    QXmppPresence unavail;
    parsePacket(unavail,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' nick='oldhag' role='participant'/>"
                "<status code='303'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(unavail);

    QVERIFY(nickTask.isFinished());
    expectVariant<QXmpp::Success>(nickTask.result());
    QCOMPARE(room.nickname().value(), u"oldhag"_s);
}

void tst_QXmppMuc::participantNicknameChange()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join room with firstwitch already present
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence firstWitchPresence;
    parsePacket(firstWitchPresence,
                "<presence from='coven@chat.shakespeare.lit/firstwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchPresence);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Get firstwitch participant handle (ID 0)
    auto participant = QXmppMucParticipant(muc, u"coven@chat.shakespeare.lit"_s, 0);
    QVERIFY(participant.isValid());
    QCOMPARE(participant.nickname().value(), u"firstwitch"_s);

    // firstwitch changes nickname: unavailable with 303
    QXmppPresence nickUnavailable;
    parsePacket(nickUnavailable,
                "<presence from='coven@chat.shakespeare.lit/firstwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant' nick='witch1'/>"
                "<status code='303'/>"
                "</x>"
                "</presence>");
    test.injectPresence(nickUnavailable);

    // Then available with new nickname
    QXmppPresence nickAvailable;
    parsePacket(nickAvailable,
                "<presence from='coven@chat.shakespeare.lit/witch1'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(nickAvailable);

    // Same participant handle, new nickname
    QVERIFY(participant.isValid());
    QCOMPARE(participant.nickname().value(), u"witch1"_s);
}

void tst_QXmppMuc::participantJoinLeave()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Track participantJoined signal
    QXmppMucParticipant joinedParticipant(muc, u""_s, 0);
    bool joinSignalReceived = false;
    QObject::connect(muc, &QXmppMucManagerV2::participantJoined, muc,
                     [&](const QString &roomJid, const QXmppMucParticipant &p) {
                         joinSignalReceived = true;
                         joinedParticipant = p;
                     });

    // firstwitch joins
    QXmppPresence firstWitchJoin;
    parsePacket(firstWitchJoin,
                "<presence from='coven@chat.shakespeare.lit/firstwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchJoin);

    QVERIFY(joinSignalReceived);
    QVERIFY(joinedParticipant.isValid());
    QCOMPARE(joinedParticipant.nickname().value(), u"firstwitch"_s);

    // Track participantLeft signal
    QXmppMucParticipant leftParticipant(muc, u""_s, 0);
    bool leftSignalReceived = false;
    Muc::LeaveReason leftReason = Muc::LeaveReason::Left;
    QObject::connect(muc, &QXmppMucManagerV2::participantLeft, muc,
                     [&](const QString &roomJid, const QXmppMucParticipant &p, Muc::LeaveReason reason) {
                         leftSignalReceived = true;
                         leftParticipant = p;
                         leftReason = reason;
                     });

    // firstwitch leaves
    QXmppPresence firstWitchLeave;
    parsePacket(firstWitchLeave,
                "<presence from='coven@chat.shakespeare.lit/firstwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='none'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchLeave);

    QVERIFY(leftSignalReceived);
    QCOMPARE(leftReason, Muc::LeaveReason::Left);
    // After the signal, participant data is cleaned up
    QVERIFY(!joinedParticipant.isValid());
}

void tst_QXmppMuc::participantsList()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Initially only self participant
    auto participants = room.participants();
    QCOMPARE(participants.size(), 1);
    QCOMPARE(participants.first().nickname().value(), u"thirdwitch"_s);

    // firstwitch joins
    QXmppPresence firstWitchJoin;
    parsePacket(firstWitchJoin,
                "<presence from='coven@chat.shakespeare.lit/firstwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchJoin);

    participants = room.participants();
    QCOMPARE(participants.size(), 2);

    // firstwitch leaves
    QXmppPresence firstWitchLeave;
    parsePacket(firstWitchLeave,
                "<presence from='coven@chat.shakespeare.lit/firstwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='none'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchLeave);

    participants = room.participants();
    QCOMPARE(participants.size(), 1);
    QCOMPARE(participants.first().nickname().value(), u"thirdwitch"_s);
}

void tst_QXmppMuc::participantKicked()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // firstwitch joins
    QXmppPresence firstWitchJoin;
    parsePacket(firstWitchJoin,
                "<presence from='coven@chat.shakespeare.lit/firstwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(firstWitchJoin);

    // firstwitch is kicked (status 307)
    Muc::LeaveReason receivedReason = Muc::LeaveReason::Left;
    bool leftSignalReceived = false;
    QObject::connect(muc, &QXmppMucManagerV2::participantLeft, muc,
                     [&](const QString &, const QXmppMucParticipant &, Muc::LeaveReason reason) {
                         leftSignalReceived = true;
                         receivedReason = reason;
                     });

    QXmppPresence kickPresence;
    parsePacket(kickPresence,
                "<presence from='coven@chat.shakespeare.lit/firstwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='none'/>"
                "<status code='307'/>"
                "</x>"
                "</presence>");
    test.injectPresence(kickPresence);

    QVERIFY(leftSignalReceived);
    QCOMPARE(receivedReason, Muc::LeaveReason::Kicked);
    QCOMPARE(room.participants().size(), 1);
}

void tst_QXmppMuc::selfBanned()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Track removedFromRoom signal
    Muc::LeaveReason removedReason = Muc::LeaveReason::Left;
    bool removedSignalReceived = false;
    bool roomValidDuringSignal = false;
    QObject::connect(muc, &QXmppMucManagerV2::removedFromRoom, muc,
                     [&](const QString &, Muc::LeaveReason reason) {
                         removedSignalReceived = true;
                         removedReason = reason;
                         roomValidDuringSignal = room.isValid();
                     });

    // We are banned (status 301 + 110)
    QXmppPresence banPresence;
    parsePacket(banPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='outcast' role='none'/>"
                "<status code='301'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(banPresence);

    QVERIFY(removedSignalReceived);
    QCOMPARE(removedReason, Muc::LeaveReason::Banned);
    QVERIFY(roomValidDuringSignal);
    // After signal handlers, room is cleaned up
    QVERIFY(!room.isValid());
}

void tst_QXmppMuc::changePresence()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Change availability
    QXmppPresence away;
    away.setAvailableStatusType(QXmppPresence::Away);
    away.setStatusText(u"brewing"_s);
    room.setPresence(std::move(away));

    // Verify presence sent to correct occupant JID
    auto sent = test.takePacket();
    QXmppPresence sentPresence;
    parsePacket(sentPresence, sent.toUtf8());
    QCOMPARE(sentPresence.to(), u"coven@chat.shakespeare.lit/thirdwitch"_s);
    QCOMPARE(sentPresence.availableStatusType(), QXmppPresence::Away);
    QCOMPARE(sentPresence.statusText(), u"brewing"_s);
}

void tst_QXmppMuc::leaveRoom()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    QVERIFY(room.isValid());
    QCOMPARE(room.joined().value(), true);

    // Leave the room
    auto leaveTask = room.leave();
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'/>"_s);

    QVERIFY(!leaveTask.isFinished());

    // Server confirms leave
    QXmppPresence leavePresence;
    parsePacket(leavePresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='none'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(leavePresence);

    QVERIFY(leaveTask.isFinished());
    expectVariant<QXmpp::Success>(leaveTask.result());
    QVERIFY(!room.isValid());
}

void tst_QXmppMuc::leaveRoomTimeout()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    muc->d->timeout = std::chrono::milliseconds(50);

    // Join the room
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Cauldron</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Leave the room
    auto leaveTask = room.leave();
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'/>"_s);
    QVERIFY(!leaveTask.isFinished());

    // Wait for timeout
    QTest::qWait(100);

    QVERIFY(leaveTask.isFinished());
    auto error = expectVariant<QXmppError>(leaveTask.result());
    QVERIFY(error.description.contains(u"timed out"_s));
    QVERIFY(!room.isValid());
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

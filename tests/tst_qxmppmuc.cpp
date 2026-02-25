// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppMessage.h"
#include "QXmppMucForms.h"
#include "QXmppMucIq.h"
#include "QXmppMucManagerV2.h"
#include "QXmppMucManagerV2_p.h"
#include "QXmppPepBookmarkManager.h"
#include "QXmppPresence.h"
#include "QXmppPubSubManager.h"
#include "QXmppPubSubSubAuthorization.h"
#include "QXmppSendResult.h"

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
    Q_SLOT void bookmarks2SetUpdate();
    Q_SLOT void bookmarks2Remove();

    // MUC avatars
    Q_SLOT void avatarFetch();

    // MUC joining
    Q_SLOT void joinRoom();
    Q_SLOT void joinRoomWithHistory();
    Q_SLOT void joinRoomWithPassword();
    Q_SLOT void joinRoomTimeout();
    Q_SLOT void joinRoomTimerStopped();
    Q_SLOT void joinRoomAlreadyInProgress();

    // MUC messages
    Q_SLOT void receiveMessage();
    Q_SLOT void sendMessage();
    Q_SLOT void sendMessageError();
    Q_SLOT void sendPrivateMessage();
    Q_SLOT void setSubject();
    Q_SLOT void changeNickname();
    Q_SLOT void changeNicknameTimeout();
    Q_SLOT void participantNicknameChange();
    Q_SLOT void participantJoinLeave();
    Q_SLOT void participantsList();
    Q_SLOT void participantKicked();
    Q_SLOT void selfBanned();
    Q_SLOT void roomDestroyed();
    Q_SLOT void changePresence();
    Q_SLOT void leaveRoom();
    Q_SLOT void leaveRoomTimeout();

    // Disconnect state management
    Q_SLOT void disconnectNoStreamManagement();
    Q_SLOT void disconnectResumedStream();
    Q_SLOT void disconnectNewStream();

    // Role and affiliation management
    Q_SLOT void setRole();
    Q_SLOT void setRoleParticipantGone();
    Q_SLOT void setAffiliation();
    Q_SLOT void requestAffiliationList();
    Q_SLOT void selfParticipant();
    Q_SLOT void permissions();
    Q_SLOT void permissionsSubjectChangeable();
    Q_SLOT void roomInfoProperties();
    Q_SLOT void roomInfoStatus104();
    Q_SLOT void roomInfoBindable();
    Q_SLOT void roomFeatureProperties();
    Q_SLOT void roomFeatureStatus172_173();
    Q_SLOT void requestVoice();
    Q_SLOT void voiceRequestReceived();
    Q_SLOT void answerVoiceRequest();
    Q_SLOT void inviteUser();
    Q_SLOT void invitationReceived();
    Q_SLOT void invitationReceivedUnknownRoom();
    Q_SLOT void declineInvitation();
    Q_SLOT void invitationDeclined();

    // Room creation and configuration
    Q_SLOT void joinRoomNotFound();
    Q_SLOT void createRoom();
    Q_SLOT void createRoomAlreadyExists();
    Q_SLOT void setRoomConfigCreation();
    Q_SLOT void cancelRoomCreation();
    Q_SLOT void reconfigureRoom();
    Q_SLOT void destroyRoom();
    Q_SLOT void subscribeToRoomConfig();
    Q_SLOT void setWatchRoomConfigFetch();
    Q_SLOT void rewatchRoomConfigStaleCache();
    Q_SLOT void requestRoomConfigJoinsInFlightFetch();

    // muc#roominfo form
    Q_SLOT void roomInfoForm();
    // muc#roomconfig form
    Q_SLOT void roomConfigForm();
};

static QString roomConfigRequestXml(const QString &roomJid = u"coven@chat.shakespeare.lit"_s)
{
    return u"<iq id='qx1' to='" + roomJid + u"' type='get'>"
                                            "<query xmlns='http://jabber.org/protocol/muc#owner'/>"
                                            "</iq>"_s;
}

static QString roomConfigResultXml(const QString &name = {})
{
    return u"<iq id='qx1' type='result'>"
           "<query xmlns='http://jabber.org/protocol/muc#owner'>"
           "<x xmlns='jabber:x:data' type='form'>"
           "<field type='hidden' var='FORM_TYPE'>"
           "<value>http://jabber.org/protocol/muc#roomconfig</value>"
           "</field>"_s +
        (name.isEmpty()
             ? QString {}
             : u"<field type='text-single' var='muc#roomconfig_roomname'>"
               "<value>"_s +
                 name + u"</value>"
                        "</field>"_s) +
        u"</x>"
        "</query></iq>"_s;
}

void tst_QXmppMuc::bookmarks2Updates()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *bm = test.addNewExtension<QXmppPepBookmarkManager>();

    QSignalSpy resetSignal(bm, &QXmppPepBookmarkManager::bookmarksReset);
    QSignalSpy addedSignal(bm, &QXmppPepBookmarkManager::bookmarksAdded);
    QSignalSpy changedSignal(bm, &QXmppPepBookmarkManager::bookmarksChanged);
    QSignalSpy removedSignal(bm, &QXmppPepBookmarkManager::bookmarksRemoved);

    QVERIFY(!bm->bookmarks().has_value());

    bm->onConnected();
    test.expect(u"<iq id='qx1' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='urn:xmpp:bookmarks:1'/></pubsub></iq>"_s);
    test.inject(u"<iq id='qx1' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='urn:xmpp:bookmarks:1'>"
                u"<item id='theplay@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Play&apos;s the Thing' autojoin='true'><nick>JC</nick></conference></item>"
                u"<item id='orchard@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Orcard' autojoin='1'><nick>JC</nick><extensions><state xmlns='http://myclient.example/bookmark/state' minimized='true'/></extensions></conference></item>"
                u"</items></pubsub></iq>"_s);

    QCOMPARE(resetSignal.size(), 1);

    QVERIFY(bm->bookmarks().has_value());
    QCOMPARE(bm->bookmarks()->size(), 2);

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
    auto *bm = test.addNewExtension<QXmppPepBookmarkManager>();

    auto task = bm->setBookmark(QXmppMucBookmark { u"theplay@conference.shakespeare.lit"_s, u"The Play's the Thing"_s, true, u"JC"_s, {} });
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

void tst_QXmppMuc::bookmarks2SetUpdate()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *bm = test.addNewExtension<QXmppPepBookmarkManager>();

    // Pre-populate the bookmark list (simulate initial fetch)
    bm->onConnected();
    test.expect(u"<iq id='qx1' type='get'><pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='urn:xmpp:bookmarks:1'/></pubsub></iq>"_s);
    test.inject(u"<iq id='qx1' type='result'><pubsub xmlns='http://jabber.org/protocol/pubsub'>"
                u"<items node='urn:xmpp:bookmarks:1'>"
                u"<item id='theplay@conference.shakespeare.lit'><conference xmlns='urn:xmpp:bookmarks:1' name='The Play' autojoin='true'><nick>JC</nick></conference></item>"
                u"</items></pubsub></iq>"_s);

    QVERIFY(bm->bookmarks().has_value());
    QCOMPARE(bm->bookmarks()->size(), 1);
    QCOMPARE(bm->bookmarks()->at(0).name(), u"The Play"_s);

    // Update the same bookmark with a new name
    auto task = bm->setBookmark(QXmppMucBookmark { u"theplay@conference.shakespeare.lit"_s, u"The Play's the Thing"_s, true, u"JC"_s, {} });
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

    // Must be updated in-place, no duplicate
    QCOMPARE(bm->bookmarks()->size(), 1);
    QCOMPARE(bm->bookmarks()->at(0).name(), u"The Play's the Thing"_s);
}

void tst_QXmppMuc::bookmarks2Remove()
{
    TestClient test(true);
    test.configuration().setJid(u"juliet@capulet.lit/balcony"_s);
    test.addNewExtension<QXmppPubSubManager>();
    auto *bm = test.addNewExtension<QXmppPepBookmarkManager>();

    auto task = bm->removeBookmark(u"theplay@conference.shakespeare.lit"_s);

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
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join manually so we can inject the disco#info result before enabling avatar watch.
    auto joinTask = muc->joinRoom(u"garden@chat.shakespeare.example.org"_s, u"juliet"_s);
    test.ignore();  // disco#info IQ (qx1)
    test.ignore();  // presence

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='garden@chat.shakespeare.example.org/juliet'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='garden@chat.shakespeare.example.org' type='groupchat'><subject>The Garden</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Inject disco#info result (qx1) with avatar hash + vcard-temp support.
    // Since watchAvatar is still false, this only populates roomInfo — no fetch yet.
    test.inject(
        u"<iq id='qx1' type='result' from='garden@chat.shakespeare.example.org'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'>"
        "<identity category='conference' type='text' name='The Garden'/>"
        "<feature var='http://jabber.org/protocol/muc'/>"
        "<feature var='vcard-temp'/>"
        "<x xmlns='jabber:x:data' type='result'>"
        "<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>"
        "<field var='muc#roominfo_avatarhash' type='text-multi'>"
        "<value>a31c4bd04de69663cfd7f424a8453f4674da37ff</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>"_s);

    QVERIFY(!room.avatar().value().has_value());

    // Now enable avatar watching — roomInfo is already available so fetch starts immediately.
    // The vcard IQ is sent with id=qx1 (counter was reset by the disco#info inject above).
    room.setWatchAvatar(true);
    test.ignore();  // consume the vcard IQ

    // Inject vcard result
    test.inject(
        u"<iq id='qx1' type='result' from='garden@chat.shakespeare.example.org'>"
        "<vCard xmlns='vcard-temp'>"
        "<PHOTO>"
        "<TYPE>image/svg+xml</TYPE>"
        "<BINVAL>PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIzMiIgaGVpZ2h0PSIzMiI+CiA8cmVjdCB4PSIwIiB5PSIwIiB3aWR0aD0iMzIiIGhlaWdodD0iMzIiIGZpbGw9InJlZCIvPgo8L3N2Zz4K</BINVAL>"
        "</PHOTO>"
        "</vCard>"
        "</iq>"_s);

    QVERIFY(room.avatar().value().has_value());
    QCOMPARE(room.avatar().value()->contentType, u"image/svg+xml"_s);
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

void tst_QXmppMuc::joinRoomAlreadyInProgress()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // First join — not yet completed
    auto task1 = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/thirdwitch'><x xmlns='http://jabber.org/protocol/muc'/></presence>"_s);
    QVERIFY(!task1.isFinished());

    // Second join while first is still in progress must fail immediately
    auto task2 = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    QVERIFY(task2.isFinished());
    expectVariant<QXmppError>(task2.result());

    // Complete the first join
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

    auto room = expectFutureVariant<QXmppMucRoomV2>(task1);
    QVERIFY(room.isValid());

    // Third join after fully joined must succeed idempotently
    auto task3 = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    QVERIFY(task3.isFinished());
    QVERIFY(expectVariant<QXmppMucRoomV2>(task3.result()).isValid());
}

static QXmppMucRoomV2 joinedRoom(TestClient &test, QXmppMucManagerV2 *muc,
                                 const QString &roomJid = u"coven@chat.shakespeare.lit"_s,
                                 const QString &nick = u"thirdwitch"_s)
{
    auto joinTask = muc->joinRoom(roomJid, nick);
    test.ignore();  // presence

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                QStringLiteral("<presence from='%1/%2'>"
                               "<x xmlns='http://jabber.org/protocol/muc#user'>"
                               "<item affiliation='admin' role='moderator'/>"
                               "<status code='110'/>"
                               "</x>"
                               "</presence>")
                    .arg(roomJid, nick)
                    .toUtf8());
    test.injectPresence(selfPresence);

    QXmppMessage subjectMsg;
    parsePacket(subjectMsg,
                QStringLiteral("<message from='%1' type='groupchat'><subject>Test</subject></message>")
                    .arg(roomJid)
                    .toUtf8());
    muc->handleMessage(subjectMsg);
    return expectFutureVariant<QXmppMucRoomV2>(joinTask);
}

// Completes muc->createRoom() up to the Creating state with config form fetched.
// Returns the room handle in Creating state.
static QXmppMucRoomV2 createdRoom(TestClient &test, QXmppMucManagerV2 *muc,
                                  const QString &roomJid = u"newroom@chat.shakespeare.lit"_s,
                                  const QString &nick = u"thirdwitch"_s)
{
    auto createTask = muc->createRoom(roomJid, nick);
    test.ignore();  // presence
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                QStringLiteral("<presence from='%1/%2'>"
                               "<x xmlns='http://jabber.org/protocol/muc#user'>"
                               "<item affiliation='owner' role='moderator'/>"
                               "<status code='110'/>"
                               "<status code='201'/>"
                               "</x>"
                               "</presence>")
                    .arg(roomJid, nick)
                    .toUtf8());
    test.injectPresence(selfPresence);
    test.ignore();  // config form IQ get
    test.inject(roomConfigResultXml());
    return expectFutureVariant<QXmppMucRoomV2>(createTask);
}

void tst_QXmppMuc::receiveMessage()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);
    QCOMPARE(room.subject().value(), u"Test"_s);

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

    auto room = joinedRoom(test, muc);
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

void tst_QXmppMuc::changeNicknameTimeout()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    muc->d->timeout = std::chrono::milliseconds(50);

    auto room = joinedRoom(test, muc);

    // Change nickname but never receive the server confirmation
    auto nickTask = room.setNickname(u"oldhag"_s);
    test.expect(u"<presence to='coven@chat.shakespeare.lit/oldhag'/>"_s);
    QVERIFY(!nickTask.isFinished());

    // Wait for timeout
    QTest::qWait(100);

    QVERIFY(nickTask.isFinished());
    auto error = expectVariant<QXmppError>(nickTask.result());
    QVERIFY(error.description.contains(u"timed out"_s));
    // Room should still be valid after a nick-change timeout
    QVERIFY(room.isValid());
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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

    // Track removedFromRoom signal
    Muc::LeaveReason removedReason = Muc::LeaveReason::Left;
    bool removedSignalReceived = false;
    bool roomValidDuringSignal = false;
    QObject::connect(muc, &QXmppMucManagerV2::removedFromRoom, muc,
                     [&](const QString &, Muc::LeaveReason reason, const std::optional<Muc::Destroy> &) {
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

void tst_QXmppMuc::roomDestroyed()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinedRoom(test, muc);

    // Track removedFromRoom signal
    Muc::LeaveReason removedReason = Muc::LeaveReason::Left;
    std::optional<Muc::Destroy> destroyInfo;
    bool removedSignalReceived = false;
    QObject::connect(muc, &QXmppMucManagerV2::removedFromRoom, muc,
                     [&](const QString &, Muc::LeaveReason reason, const std::optional<Muc::Destroy> &destroy) {
                         removedSignalReceived = true;
                         removedReason = reason;
                         destroyInfo = destroy;
                     });

    // Room is destroyed (XEP-0045 §10.9)
    QXmppPresence destroyPresence;
    parsePacket(destroyPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='none'/>"
                "<destroy jid='darkcave@chat.shakespeare.lit'>"
                "<reason>Moved to a new room</reason>"
                "</destroy>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(destroyPresence);

    QVERIFY(removedSignalReceived);
    QCOMPARE(removedReason, Muc::LeaveReason::RoomDestroyed);
    QVERIFY(destroyInfo.has_value());
    QCOMPARE(destroyInfo->alternateRoom(), u"darkcave@chat.shakespeare.lit"_s);
    QCOMPARE(destroyInfo->reason(), u"Moved to a new room"_s);
    QVERIFY(!room.isValid());
}

void tst_QXmppMuc::changePresence()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

    auto room = joinedRoom(test, muc);

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

void tst_QXmppMuc::disconnectNoStreamManagement()
{
    TestClient test(true);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinedRoom(test, muc);
    QVERIFY(room.isValid());
    QVERIFY(room.joined().value());

    // Simulate disconnect without stream management (intentional disconnect)
    test.setStreamManagementState(QXmppClient::NoStreamManagement);
    test.simulateDisconnected();

    // Room state must be cleared immediately
    QVERIFY(!room.isValid());
    QVERIFY(!room.joined().value());
}

void tst_QXmppMuc::disconnectResumedStream()
{
    TestClient test(true);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinedRoom(test, muc);
    QVERIFY(room.isValid());

    // Simulate network drop (SM enabled, disconnect without clearing SM)
    test.setStreamManagementState(QXmppClient::NewStream);
    test.simulateDisconnected();

    // Room state must be preserved (stream can potentially be resumed)
    QVERIFY(room.isValid());
    QVERIFY(room.joined().value());

    // Simulate successful stream resumption
    test.setStreamManagementState(QXmppClient::ResumedStream);
    test.simulateConnected();

    // Room state must still be intact after resume
    QVERIFY(room.isValid());
    QVERIFY(room.joined().value());
}

void tst_QXmppMuc::disconnectNewStream()
{
    TestClient test(true);
    test.addNewExtension<QXmppPubSubManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinedRoom(test, muc);
    QVERIFY(room.isValid());

    // Simulate network drop (SM enabled)
    test.setStreamManagementState(QXmppClient::NewStream);
    test.simulateDisconnected();

    // Room preserved while resumption is possible
    QVERIFY(room.isValid());

    // Simulate reconnect where stream resumption failed (NewStream)
    test.simulateConnected();

    // Room must be cleared now — server no longer knows about our presence
    QVERIFY(!room.isValid());
    QVERIFY(!room.joined().value());
}

void tst_QXmppMuc::setRole()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    // Inject another participant into the room
    QXmppPresence presence;
    parsePacket(presence,
                "<presence from='coven@chat.shakespeare.lit/pistol'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(presence);
    auto participants = room.participants();
    auto it = std::ranges::find_if(participants, [](const QXmppMucParticipant &p) {
        return p.nickname().value() == u"pistol"_s;
    });
    QVERIFY(it != participants.end());
    auto pistol = *it;

    // Change role to moderator
    auto task = room.setRole(pistol, QXmpp::Muc::Role::Moderator);
    QVERIFY(!task.isFinished());
    test.expect(u"<iq id='qx1' to='coven@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#admin'>"
                "<item nick='pistol' role='moderator'/>"
                "</query></iq>"_s);

    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(task.isFinished());
    expectVariant<QXmpp::Success>(task.result());
}

void tst_QXmppMuc::setRoleParticipantGone()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    // Capture a participant handle while they are in the room
    QXmppPresence joinPresence;
    parsePacket(joinPresence,
                "<presence from='coven@chat.shakespeare.lit/pistol'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='participant'/>"
                "</x>"
                "</presence>");
    test.injectPresence(joinPresence);
    auto participants = room.participants();
    auto it = std::ranges::find_if(participants, [](const QXmppMucParticipant &p) {
        return p.nickname().value() == u"pistol"_s;
    });
    QVERIFY(it != participants.end());
    auto pistol = *it;

    // Participant leaves
    QXmppPresence leavePresence;
    parsePacket(leavePresence,
                "<presence from='coven@chat.shakespeare.lit/pistol' type='unavailable'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='none' role='none'/>"
                "</x>"
                "</presence>");
    test.injectPresence(leavePresence);

    QVERIFY(!pistol.isValid());
    auto task = room.setRole(pistol, QXmpp::Muc::Role::Moderator);
    QVERIFY(task.isFinished());
    expectVariant<QXmppError>(task.result());
}

void tst_QXmppMuc::setAffiliation()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    auto task = room.setAffiliation(u"macbeth@shakespeare.lit"_s, QXmpp::Muc::Affiliation::Outcast, u"Treason"_s);
    QVERIFY(!task.isFinished());
    test.expect(u"<iq id='qx1' to='coven@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#admin'>"
                "<item affiliation='outcast' jid='macbeth@shakespeare.lit'>"
                "<reason>Treason</reason>"
                "</item>"
                "</query></iq>"_s);

    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(task.isFinished());
    expectVariant<QXmpp::Success>(task.result());
}

void tst_QXmppMuc::requestAffiliationList()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    auto task = room.requestAffiliationList(QXmpp::Muc::Affiliation::Outcast);
    QVERIFY(!task.isFinished());
    test.expect(u"<iq id='qx1' to='coven@chat.shakespeare.lit' type='get'>"
                "<query xmlns='http://jabber.org/protocol/muc#admin'>"
                "<item affiliation='outcast'/>"
                "</query></iq>"_s);

    test.inject(u"<iq id='qx1' type='result'>"
                "<query xmlns='http://jabber.org/protocol/muc#admin'>"
                "<item affiliation='outcast' jid='macbeth@shakespeare.lit'><reason>Treason</reason></item>"
                "<item affiliation='outcast' jid='iago@shakespeare.lit'/>"
                "</query></iq>"_s);

    QVERIFY(task.isFinished());
    auto items = expectVariant<QList<QXmpp::Muc::Item>>(task.result());
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].jid(), u"macbeth@shakespeare.lit"_s);
    QCOMPARE(items[0].affiliation(), QXmpp::Muc::Affiliation::Outcast);
    QCOMPARE(items[0].reason(), u"Treason"_s);
    QCOMPARE(items[1].jid(), u"iago@shakespeare.lit"_s);
}

void tst_QXmppMuc::selfParticipant()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Not yet joined: no self participant
    auto room = muc->room(u"coven@chat.shakespeare.lit"_s);
    QVERIFY(!room.selfParticipant().has_value());

    // Joined room with admin affiliation and moderator role
    auto room2 = joinedRoom(test, muc);
    auto self = room2.selfParticipant();
    QVERIFY(self.has_value());
    QCOMPARE(self->nickname().value(), u"thirdwitch"_s);
    QCOMPARE(self->role().value(), QXmpp::Muc::Role::Moderator);
    QCOMPARE(self->affiliation().value(), QXmpp::Muc::Affiliation::Admin);
}

void tst_QXmppMuc::permissions()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Not joined: all false, QBindable is empty (default-constructed bool = false)
    auto room = muc->room(u"coven@chat.shakespeare.lit"_s);
    QVERIFY(!room.canSendMessages().value());
    QVERIFY(!room.canChangeSubject().value());
    QVERIFY(!room.canSetRoles().value());
    QVERIFY(!room.canSetAffiliations().value());
    QVERIFY(!room.canConfigureRoom().value());

    // Joined as moderator + admin (from joinedRoom() fixture)
    auto room2 = joinedRoom(test, muc);
    QVERIFY(room2.canSendMessages().value());
    QVERIFY(room2.canChangeSubject().value());  // moderator -> always true
    QVERIFY(room2.canSetRoles().value());
    QVERIFY(room2.canSetAffiliations().value());  // admin
    QVERIFY(!room2.canConfigureRoom().value());   // not owner

    // Server changes our role to visitor -> can no longer send or set roles
    QXmppPresence demotion;
    parsePacket(demotion,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='admin' role='visitor'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(demotion);

    QVERIFY(!room2.canSendMessages().value());
    QVERIFY(!room2.canChangeSubject().value());
    QVERIFY(!room2.canSetRoles().value());
    QVERIFY(room2.canSetAffiliations().value());  // affiliation unchanged
}

void tst_QXmppMuc::permissionsSubjectChangeable()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    // Join as participant (not moderator)
    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    // First: presence sent; second: disco#info request fired by joinRoom
    test.ignore();  // presence
    test.ignore();  // disco#info

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // participant: canChangeSubject is false until disco#info arrives
    QVERIFY(room.canSendMessages().value());
    QVERIFY(!room.canChangeSubject().value());

    // Inject disco#info result with subjectChangeable = true (muc#roominfo_subjectmod)
    test.inject(u"<iq id='qx1' type='result' from='coven@chat.shakespeare.lit'>"
                "<query xmlns='http://jabber.org/ns/protocols/disco#info'>"
                "<identity category='conference' type='text'/>"
                "<feature var='http://jabber.org/protocol/muc'/>"
                "<x xmlns='jabber:x:data' type='result'>"
                "<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>"
                "<field var='muc#roominfo_subjectmod'><value>1</value></field>"
                "</x>"
                "</query></iq>"_s);

    QVERIFY(room.canChangeSubject().value());
}

static const auto discoRoomInfo =
    u"<iq id='qx1' type='result' from='coven@chat.shakespeare.lit'>"
    "<query xmlns='http://jabber.org/ns/protocols/disco#info'>"
    "<identity category='conference' type='text'/>"
    "<feature var='http://jabber.org/protocol/muc'/>"
    "<x xmlns='jabber:x:data' type='result'>"
    "<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>"
    "<field var='muc#roominfo_description'><value>A Witch Coven</value></field>"
    "<field var='muc#roominfo_lang'><value>en</value></field>"
    "<field var='muc#roominfo_contactjid' type='jid-multi'>"
    "<value>hag66@shakespeare.lit</value>"
    "<value>wiccarocks@shakespeare.lit</value>"
    "</field>"
    "</x>"
    "</query></iq>"_s;

void tst_QXmppMuc::roomInfoProperties()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // presence
    test.ignore();  // disco#info

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Before disco#info arrives: empty
    QVERIFY(room.description().value().isEmpty());
    QVERIFY(room.language().value().isEmpty());
    QVERIFY(room.contactJids().value().isEmpty());

    test.inject(discoRoomInfo);

    QCOMPARE(room.description().value(), u"A Witch Coven"_s);
    QCOMPARE(room.language().value(), u"en"_s);
    QCOMPARE(room.contactJids().value(), (QStringList { u"hag66@shakespeare.lit"_s, u"wiccarocks@shakespeare.lit"_s }));
}

void tst_QXmppMuc::roomInfoStatus104()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // disco#info
    test.ignore();  // presence

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Answer the initial disco#info with an empty result
    test.inject(u"<iq id='qx1' type='result' from='coven@chat.shakespeare.lit'>"
                "<query xmlns='http://jabber.org/ns/protocols/disco#info'>"
                "<identity category='conference' type='text'/>"
                "</query></iq>"_s);
    QVERIFY(room.description().value().isEmpty());

    // Inject a status-104 message — triggers a new disco#info fetch (CachePolicy::Strict)
    QXmppMessage status104;
    parsePacket(status104,
                "<message from='coven@chat.shakespeare.lit' type='groupchat'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<status code='104'/>"
                "</x>"
                "</message>");
    muc->handleMessage(status104);

    // A new disco#info IQ should have been sent (also id qx1 since counter was reset)
    test.ignore();  // consume the new disco#info request

    // Inject updated roominfo
    test.inject(u"<iq id='qx1' type='result' from='coven@chat.shakespeare.lit'>"
                "<query xmlns='http://jabber.org/ns/protocols/disco#info'>"
                "<identity category='conference' type='text'/>"
                "<x xmlns='jabber:x:data' type='result'>"
                "<field var='FORM_TYPE' type='hidden'><value>http://jabber.org/protocol/muc#roominfo</value></field>"
                "<field var='muc#roominfo_description'><value>Updated Coven</value></field>"
                "</x>"
                "</query></iq>"_s);

    QCOMPARE(room.description().value(), u"Updated Coven"_s);
}

void tst_QXmppMuc::roomInfoBindable()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // disco#info
    test.ignore();  // presence

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Before disco#info: roomInfo is nullopt
    QVERIFY(!room.roomInfo().value().has_value());

    // Inject disco#info with full roominfo
    test.inject(discoRoomInfo);

    // roomInfo() now holds the full form
    auto info = room.roomInfo().value();
    QVERIFY(info.has_value());
    QCOMPARE(info->description(), u"A Witch Coven"_s);
    QCOMPARE(info->language(), u"en"_s);
    QCOMPARE(info->contactJids(), (QStringList { u"hag66@shakespeare.lit"_s, u"wiccarocks@shakespeare.lit"_s }));

    // Convenience bindings are also populated from the same source
    QCOMPARE(room.description().value(), u"A Witch Coven"_s);
    QCOMPARE(room.language().value(), u"en"_s);
}

// Helper: join a room and return the QXmppMucRoomV2
static QXmppMucRoomV2 joinTestRoom(TestClient &test, QXmppMucManagerV2 *muc, const QString &roomJid, const QString &nick)
{
    auto joinTask = muc->joinRoom(roomJid, nick);
    test.ignore();  // disco#info or presence
    test.ignore();

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                QString("<presence from='%1/%2'>"
                        "<x xmlns='http://jabber.org/protocol/muc#user'>"
                        "<item affiliation='member' role='participant'/>"
                        "<status code='110'/>"
                        "</x>"
                        "</presence>")
                    .arg(roomJid, nick)
                    .toUtf8());
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, QString("<message from='%1' type='groupchat'><subject>Test</subject></message>").arg(roomJid).toUtf8());
    muc->handleMessage(subjectMsg);
    return expectFutureVariant<QXmppMucRoomV2>(joinTask);
}

void tst_QXmppMuc::requestVoice()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);

    auto task = room.requestVoice();
    test.expect(u"<message to='coven@chat.shakespeare.lit' type='normal'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#request</value>"
                "</field>"
                "<field type='list-single' var='muc#role'>"
                "<value>participant</value>"
                "</field>"
                "</x>"
                "</message>"_s);
    QVERIFY(!task.isFinished());
}

void tst_QXmppMuc::voiceRequestReceived()
{
    TestClient test(true);
    test.configuration().setJid(u"crone1@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"firstwitch"_s);

    QSignalSpy spy(muc, &QXmppMucManagerV2::voiceRequestReceived);

    // Room forwards voice request approval form to moderator
    QXmppMessage voiceReqMsg;
    parsePacket(voiceReqMsg,
                "<message from='coven@chat.shakespeare.lit' to='crone1@shakespeare.lit/pda'>"
                "<x xmlns='jabber:x:data' type='form'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#request</value>"
                "</field>"
                "<field type='list-single' var='muc#role'>"
                "<value>participant</value>"
                "</field>"
                "<field type='jid-single' var='muc#jid'>"
                "<value>hag66@shakespeare.lit/pda</value>"
                "</field>"
                "<field type='text-single' var='muc#roomnick'>"
                "<value>thirdwitch</value>"
                "</field>"
                "<field type='boolean' var='muc#request_allow'>"
                "<value>false</value>"
                "</field>"
                "</x>"
                "</message>");
    muc->handleMessage(voiceReqMsg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"coven@chat.shakespeare.lit");
    auto req = spy.at(0).at(1).value<QXmppMucVoiceRequest>();
    QCOMPARE(req.jid(), u"hag66@shakespeare.lit/pda");
    QCOMPARE(req.nick(), u"thirdwitch");
    QVERIFY(req.requestAllow().has_value());
    QCOMPARE(*req.requestAllow(), false);
}

void tst_QXmppMuc::answerVoiceRequest()
{
    TestClient test(true);
    test.configuration().setJid(u"crone1@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"firstwitch"_s);

    // Construct an incoming voice request
    QXmppMucVoiceRequest req;
    req.setJid(u"hag66@shakespeare.lit/pda"_s);
    req.setNick(u"thirdwitch"_s);
    req.setRequestAllow(false);

    // Approve: moderator sends form back with muc#request_allow=true
    auto task = room.answerVoiceRequest(req, true);
    test.expect(u"<message to='coven@chat.shakespeare.lit' type='normal'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#request</value>"
                "</field>"
                "<field type='list-single' var='muc#role'>"
                "<value>participant</value>"
                "</field>"
                "<field type='jid-single' var='muc#jid'>"
                "<value>hag66@shakespeare.lit/pda</value>"
                "</field>"
                "<field type='text-single' var='muc#roomnick'>"
                "<value>thirdwitch</value>"
                "</field>"
                "<field type='boolean' var='muc#request_allow'>"
                "<value>true</value>"
                "</field>"
                "</x>"
                "</message>"_s);
    QVERIFY(!task.isFinished());
}

void tst_QXmppMuc::roomFeatureProperties()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // presence
    test.ignore();  // disco#info

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Before disco#info: defaults
    QVERIFY(!room.isNonAnonymous().value());
    QVERIFY(room.isPublic().value());
    QVERIFY(!room.isMembersOnly().value());
    QVERIFY(!room.isModerated().value());
    QVERIFY(!room.isPersistent().value());
    QVERIFY(!room.isPasswordProtected().value());

    // Inject disco#info with all room feature flags set
    test.inject(u"<iq id='qx1' type='result' from='coven@chat.shakespeare.lit'>"
                "<query xmlns='http://jabber.org/protocol/disco#info'>"
                "<identity category='conference' type='text'/>"
                "<feature var='muc_nonanonymous'/>"
                "<feature var='muc_membersonly'/>"
                "<feature var='muc_moderated'/>"
                "<feature var='muc_persistent'/>"
                "<feature var='muc_passwordprotected'/>"
                "</query></iq>"_s);

    QVERIFY(room.isNonAnonymous().value());
    QVERIFY(!room.isPublic().value());  // muc_public absent → not public
    QVERIFY(room.isMembersOnly().value());
    QVERIFY(room.isModerated().value());
    QVERIFY(room.isPersistent().value());
    QVERIFY(room.isPasswordProtected().value());
}

void tst_QXmppMuc::roomFeatureStatus172_173()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto joinTask = muc->joinRoom(u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // presence
    test.ignore();  // disco#info

    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);
    QXmppMessage subjectMsg;
    parsePacket(subjectMsg, "<message from='coven@chat.shakespeare.lit' type='groupchat'><subject>Test</subject></message>");
    muc->handleMessage(subjectMsg);
    auto room = expectFutureVariant<QXmppMucRoomV2>(joinTask);

    // Initially semi-anonymous
    QVERIFY(!room.isNonAnonymous().value());

    // Status 172: room is now non-anonymous
    QXmppPresence status172;
    parsePacket(status172,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='172'/>"
                "</x>"
                "</presence>");
    test.injectPresence(status172);
    QVERIFY(room.isNonAnonymous().value());

    // Status 173: room is now semi-anonymous again
    QXmppPresence status173;
    parsePacket(status173,
                "<presence from='coven@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='173'/>"
                "</x>"
                "</presence>");
    test.injectPresence(status173);
    QVERIFY(!room.isNonAnonymous().value());
}

void tst_QXmppMuc::joinRoomNotFound()
{
    // joinRoom() on a non-existent room (status 201) must fail with an error
    // and send a cancel IQ to destroy the accidentally-created locked room.
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->joinRoom(u"newroom@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // presence sent

    // Server responds with status 201 (new locked room) + 110 (self-presence)
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='newroom@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='owner' role='moderator'/>"
                "<status code='110'/>"
                "<status code='201'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    // Manager sends cancel IQ to destroy the locked room
    test.expect(u"<iq id='qx1' to='newroom@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='cancel'/>"
                "</query></iq>"_s);

    // joinRoom task must have failed
    QVERIFY(task.isFinished());
    expectVariant<QXmppError>(task.result());
}

void tst_QXmppMuc::createRoom()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->createRoom(u"newroom@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.expect(u"<presence to='newroom@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc'/>"
                "</presence>"_s);

    // Server creates locked room, sends status 201 + 110
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='newroom@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='owner' role='moderator'/>"
                "<status code='110'/>"
                "<status code='201'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    // Manager auto-requests config form
    test.expect(u"<iq id='qx1' to='newroom@chat.shakespeare.lit' type='get'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'/>"
                "</iq>"_s);
    QVERIFY(!task.isFinished());

    // Server returns config form
    test.inject(u"<iq id='qx1' type='result'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='form'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#roomconfig</value>"
                "</field>"
                "<field type='text-single' var='muc#roomconfig_roomname'>"
                "<value>New Room</value>"
                "</field>"
                "</x>"
                "</query></iq>"_s);

    // createRoom task must now resolve
    QVERIFY(task.isFinished());
    auto room = expectFutureVariant<QXmppMucRoomV2>(task);
    QVERIFY(room.isValid());
    QVERIFY(!room.joined().value());  // still locked
    QVERIFY(room.canConfigureRoom().value());
}

void tst_QXmppMuc::createRoomAlreadyExists()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto task = muc->createRoom(u"existing@chat.shakespeare.lit"_s, u"thirdwitch"_s);
    test.ignore();  // presence

    // Server grants entry without status 201: room already existed
    QXmppPresence selfPresence;
    parsePacket(selfPresence,
                "<presence from='existing@chat.shakespeare.lit/thirdwitch'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<item affiliation='member' role='participant'/>"
                "<status code='110'/>"
                "</x>"
                "</presence>");
    test.injectPresence(selfPresence);

    QVERIFY(task.isFinished());
    expectFutureVariant<QXmppError>(task);
}

void tst_QXmppMuc::setRoomConfigCreation()
{
    TestClient test(true);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = createdRoom(test, muc);
    QVERIFY(!room.joined().value());

    // Submit config — room should become joined after IQ result
    QXmppMucRoomConfig config;
    config.setName(u"New Room"_s);
    auto task = room.setRoomConfig(config);
    test.expect(u"<iq id='qx1' to='newroom@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#roomconfig</value>"
                "</field>"
                "<field type='text-single' var='muc#roomconfig_roomname'>"
                "<value>New Room</value>"
                "</field>"
                "</x>"
                "</query></iq>"_s);
    QVERIFY(!task.isFinished());

    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(task.isFinished());
    expectVariant<QXmpp::Success>(task.result());
    QVERIFY(room.joined().value());
}

void tst_QXmppMuc::cancelRoomCreation()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = createdRoom(test, muc);

    auto task = room.cancelRoomCreation();
    test.expect(u"<iq id='qx1' to='newroom@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='cancel'/>"
                "</query></iq>"_s);
    QVERIFY(!task.isFinished());

    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(task.isFinished());
    expectVariant<QXmpp::Success>(task.result());
    QVERIFY(!room.isValid());
}

void tst_QXmppMuc::reconfigureRoom()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    // Request current config
    auto reqTask = room.requestRoomConfig();
    test.expect(roomConfigRequestXml());
    test.inject(u"<iq id='qx1' type='result'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='form'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#roomconfig</value>"
                "</field>"
                "<field type='text-single' var='muc#roomconfig_roomname'>"
                "<value>The Coven</value>"
                "</field>"
                "<field type='boolean' var='muc#roomconfig_persistentroom'>"
                "<value>1</value>"
                "</field>"
                "</x>"
                "</query></iq>"_s);
    QVERIFY(reqTask.isFinished());
    auto config = expectFutureVariant<QXmppMucRoomConfig>(reqTask);
    QCOMPARE(config.name(), u"The Coven"_s);
    QCOMPARE(config.isPersistent(), std::optional<bool>(true));

    // Submit updated config
    config.setName(u"The New Coven"_s);
    auto submitTask = room.setRoomConfig(config);
    test.expect(u"<iq id='qx1' to='coven@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field type='hidden' var='FORM_TYPE'>"
                "<value>http://jabber.org/protocol/muc#roomconfig</value>"
                "</field>"
                "<field type='text-single' var='muc#roomconfig_roomname'>"
                "<value>The New Coven</value>"
                "</field>"
                "<field type='boolean' var='muc#roomconfig_persistentroom'>"
                "<value>true</value>"
                "</field>"
                "</x>"
                "</query></iq>"_s);
    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(submitTask.isFinished());
    expectVariant<QXmpp::Success>(submitTask.result());
    QVERIFY(room.joined().value());  // still joined after reconfig
}

void tst_QXmppMuc::destroyRoom()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    auto task = room.destroyRoom(u"Meeting adjourned"_s, u"coven2@chat.shakespeare.lit"_s);
    test.expect(u"<iq id='qx1' to='coven@chat.shakespeare.lit' type='set'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<destroy jid='coven2@chat.shakespeare.lit'>"
                "<reason>Meeting adjourned</reason>"
                "</destroy>"
                "</query></iq>"_s);
    QVERIFY(!task.isFinished());

    test.inject(u"<iq id='qx1' type='result'/>"_s);
    QVERIFY(task.isFinished());
    expectVariant<QXmpp::Success>(task.result());
    QVERIFY(!room.isValid());
}

void tst_QXmppMuc::roomConfigForm()
{
    QByteArray xml(R"(
<x xmlns='jabber:x:data' type='form'>
<field type='hidden' var='FORM_TYPE'><value>http://jabber.org/protocol/muc#roomconfig</value></field>
<field type='text-single' var='muc#roomconfig_roomname'><value>The Coven</value></field>
<field type='text-single' var='muc#roomconfig_roomdesc'><value>A place for witches.</value></field>
<field type='text-single' var='muc#roomconfig_lang'><value>en</value></field>
<field type='boolean' var='muc#roomconfig_publicroom'><value>0</value></field>
<field type='boolean' var='muc#roomconfig_persistentroom'><value>1</value></field>
<field type='boolean' var='muc#roomconfig_membersonly'><value>1</value></field>
<field type='boolean' var='muc#roomconfig_moderatedroom'><value>1</value></field>
<field type='boolean' var='muc#roomconfig_passwordprotectedroom'><value>0</value></field>
<field type='list-single' var='muc#roomconfig_whois'><value>moderators</value></field>
<field type='boolean' var='muc#roomconfig_changesubject'><value>0</value></field>
<field type='boolean' var='muc#roomconfig_allowinvites'><value>1</value></field>
<field type='list-single' var='muc#roomconfig_allowpm'><value>participants</value></field>
<field type='boolean' var='muc#roomconfig_enablelogging'><value>0</value></field>
<field type='list-single' var='muc#roomconfig_maxusers'><value>50</value></field>
<field type='jid-multi' var='muc#roomconfig_roomowners'><value>crone1@shakespeare.lit</value></field>
<field type='jid-multi' var='muc#roomconfig_roomadmins'><value>wiccarocks@shakespeare.lit</value></field>
</x>)");

    QXmppDataForm form;
    parsePacket(form, xml);

    auto config = QXmppMucRoomConfig::fromDataForm(form);
    QVERIFY(config.has_value());
    QCOMPARE(config->name(), u"The Coven"_s);
    QCOMPARE(config->description(), u"A place for witches."_s);
    QCOMPARE(config->language(), u"en"_s);
    QCOMPARE(config->isPublic(), std::optional<bool>(false));
    QCOMPARE(config->isPersistent(), std::optional<bool>(true));
    QCOMPARE(config->isMembersOnly(), std::optional<bool>(true));
    QCOMPARE(config->isModerated(), std::optional<bool>(true));
    QCOMPARE(config->isPasswordProtected(), std::optional<bool>(false));
    QCOMPARE(config->whoCanDiscoverJids(), std::optional(QXmppMucRoomConfig::WhoCanDiscoverJids::Moderators));
    QCOMPARE(config->canOccupantsChangeSubject(), std::optional<bool>(false));
    QCOMPARE(config->canMembersInvite(), std::optional<bool>(true));
    QCOMPARE(config->allowPrivateMessages(), std::optional(QXmppMucRoomConfig::AllowPrivateMessages::Participants));
    QCOMPARE(config->enableLogging(), std::optional<bool>(false));
    QCOMPARE(config->maxUsers(), std::optional<int>(50));
    QCOMPARE(config->owners(), QStringList { u"crone1@shakespeare.lit"_s });
    QCOMPARE(config->admins(), QStringList { u"wiccarocks@shakespeare.lit"_s });

    // Round-trip: serialize and check
    auto form2 = config->toDataForm();
    QVERIFY(!form2.isNull());
    serializePacket(form2, QByteArrayLiteral("<x xmlns=\"jabber:x:data\" type=\"form\">"
                                             "<field type=\"hidden\" var=\"FORM_TYPE\"><value>http://jabber.org/protocol/muc#roomconfig</value></field>"
                                             "<field type=\"text-single\" var=\"muc#roomconfig_roomname\"><value>The Coven</value></field>"
                                             "<field type=\"text-single\" var=\"muc#roomconfig_roomdesc\"><value>A place for witches.</value></field>"
                                             "<field type=\"text-single\" var=\"muc#roomconfig_lang\"><value>en</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_publicroom\"><value>false</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_persistentroom\"><value>true</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_membersonly\"><value>true</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_moderatedroom\"><value>true</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_passwordprotectedroom\"><value>false</value></field>"
                                             "<field type=\"list-single\" var=\"muc#roomconfig_whois\"><value>moderators</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_changesubject\"><value>false</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_allowinvites\"><value>true</value></field>"
                                             "<field type=\"list-single\" var=\"muc#roomconfig_allowpm\"><value>participants</value></field>"
                                             "<field type=\"boolean\" var=\"muc#roomconfig_enablelogging\"><value>false</value></field>"
                                             "<field type=\"list-single\" var=\"muc#roomconfig_maxusers\"><value>50</value></field>"
                                             "<field type=\"jid-multi\" var=\"muc#roomconfig_roomowners\"><value>crone1@shakespeare.lit</value></field>"
                                             "<field type=\"jid-multi\" var=\"muc#roomconfig_roomadmins\"><value>wiccarocks@shakespeare.lit</value></field>"
                                             "</x>"));
}

void tst_QXmppMuc::subscribeToRoomConfig()
{
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    QVERIFY(!room.roomConfig().value().has_value());
    QVERIFY(!room.isWatchingRoomConfig());

    // requestRoomConfig(true) — fetches fresh and enables watch
    auto reqTask = room.requestRoomConfig(true);
    QVERIFY(room.isWatchingRoomConfig());
    test.expect(roomConfigRequestXml());
    QVERIFY(!reqTask.isFinished());

    test.inject(roomConfigResultXml(u"The Coven"_s));

    QVERIFY(reqTask.isFinished());
    auto config = expectFutureVariant<QXmppMucRoomConfig>(reqTask);
    QCOMPARE(config.name(), u"The Coven"_s);
    // roomConfig() bindable is also updated
    QVERIFY(room.roomConfig().value().has_value());
    QCOMPARE(room.roomConfig().value()->name(), u"The Coven"_s);

    // Second requestRoomConfig() returns cached value immediately — watching is active
    auto cachedTask = room.requestRoomConfig();
    QVERIFY(cachedTask.isFinished());
    QCOMPARE(expectFutureVariant<QXmppMucRoomConfig>(cachedTask).name(), u"The Coven"_s);

    // Status 104: config re-fetched automatically
    QXmppMessage status104;
    parsePacket(status104,
                "<message from='coven@chat.shakespeare.lit' type='groupchat'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<status code='104'/>"
                "</x>"
                "</message>");
    muc->handleMessage(status104);
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"The New Coven"_s));
    QCOMPARE(room.roomConfig().value()->name(), u"The New Coven"_s);

    // setWatchRoomConfig(false) — stop watching, cached value stays
    room.setWatchRoomConfig(false);
    QVERIFY(!room.isWatchingRoomConfig());
    QCOMPARE(room.roomConfig().value()->name(), u"The New Coven"_s);

    // requestRoomConfig() after disabling watch must re-fetch (cache may be stale)
    auto staleTask = room.requestRoomConfig();
    QVERIFY(!staleTask.isFinished());
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"The New Coven"_s));
    QVERIFY(staleTask.isFinished());
    QCOMPARE(expectFutureVariant<QXmppMucRoomConfig>(staleTask).name(), u"The New Coven"_s);

    // setWatchRoomConfig(true) after watch was disabled — always re-fetches (cache may be stale)
    room.setWatchRoomConfig(true);
    QVERIFY(room.isWatchingRoomConfig());
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"The New Coven"_s));
    QCOMPARE(room.roomConfig().value()->name(), u"The New Coven"_s);
}

void tst_QXmppMuc::setWatchRoomConfigFetch()
{
    // setWatchRoomConfig(true) with no prior fetch triggers a background fetch
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    QVERIFY(!room.roomConfig().value().has_value());
    room.setWatchRoomConfig(true);
    QVERIFY(room.isWatchingRoomConfig());
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml());
    QVERIFY(room.roomConfig().value().has_value());
}

void tst_QXmppMuc::rewatchRoomConfigStaleCache()
{
    // Regression test: re-enabling watch after it was disabled must not serve
    // a stale cached config. The cache may have been populated while watching
    // was active, but the room config could have changed while watching was off.
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    // Enable watching — triggers initial fetch
    room.setWatchRoomConfig(true);
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"Old Name"_s));
    QCOMPARE(room.roomConfig().value()->name(), u"Old Name"_s);

    // Disable watching — cache stays but is now potentially stale
    room.setWatchRoomConfig(false);
    QVERIFY(!room.isWatchingRoomConfig());

    // Config changed on server while not watching (no status 104 received)

    // Re-enable watching via setWatchRoomConfig — must re-fetch, not use stale cache
    room.setWatchRoomConfig(true);
    QVERIFY(room.isWatchingRoomConfig());
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"New Name"_s));
    QCOMPARE(room.roomConfig().value()->name(), u"New Name"_s);

    // requestRoomConfig() now returns the fresh config from cache (watching is active)
    auto cachedTask = room.requestRoomConfig();
    QVERIFY(cachedTask.isFinished());
    QCOMPARE(expectFutureVariant<QXmppMucRoomConfig>(cachedTask).name(), u"New Name"_s);

    // Re-enable watching via requestRoomConfig(true) when watching was off must also re-fetch
    room.setWatchRoomConfig(false);
    auto freshTask = room.requestRoomConfig(true);
    QVERIFY(room.isWatchingRoomConfig());
    QVERIFY(!freshTask.isFinished());
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"New Name"_s));
    QVERIFY(freshTask.isFinished());
    QCOMPARE(expectFutureVariant<QXmppMucRoomConfig>(freshTask).name(), u"New Name"_s);
}

void tst_QXmppMuc::requestRoomConfigJoinsInFlightFetch()
{
    // When a status-104 re-fetch is already in progress, requestRoomConfig() must join it
    // rather than returning the (about-to-be-superseded) cached value.
    TestClient test(true);
    test.configuration().setJid(u"hag66@shakespeare.lit/pda"_s);
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();
    auto room = joinedRoom(test, muc);

    // Enable watching — triggers initial fetch
    room.setWatchRoomConfig(true);
    test.expect(roomConfigRequestXml());
    test.inject(roomConfigResultXml(u"Old Name"_s));
    QCOMPARE(room.roomConfig().value()->name(), u"Old Name"_s);

    // Status 104: config changed, re-fetch starts but IQ response not yet received
    QXmppMessage status104;
    parsePacket(status104,
                "<message from='coven@chat.shakespeare.lit' type='groupchat'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<status code='104'/>"
                "</x>"
                "</message>");
    muc->handleMessage(status104);
    test.expect(roomConfigRequestXml());

    // requestRoomConfig() while fetch is in flight — must wait for the fresh result
    auto task = room.requestRoomConfig();
    QVERIFY(!task.isFinished());

    // Now the in-flight IQ response arrives with the updated config
    test.inject(roomConfigResultXml(u"New Name"_s));

    QVERIFY(task.isFinished());
    QCOMPARE(expectFutureVariant<QXmppMucRoomConfig>(task).name(), u"New Name"_s);
    QCOMPARE(room.roomConfig().value()->name(), u"New Name"_s);
}

void tst_QXmppMuc::inviteUser()
{
    TestClient test(true);
    test.configuration().setJid(u"crone1@shakespeare.lit/desktop"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    auto room = joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"firstwitch"_s);

    QXmpp::Muc::Invite invite;
    invite.setTo(u"hecate@shakespeare.lit"_s);
    invite.setReason(u"Hey Hecate!"_s);

    auto task = room.inviteUser(invite);
    test.expect(u"<message to='coven@chat.shakespeare.lit' type='normal'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<invite to='hecate@shakespeare.lit'>"
                "<reason>Hey Hecate!</reason>"
                "</invite>"
                "</x>"
                "</message>"_s);
    QVERIFY(!task.isFinished());
}

void tst_QXmppMuc::invitationReceived()
{
    TestClient test(true);
    test.configuration().setJid(u"hecate@shakespeare.lit/broom"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"thirdwitch"_s);

    QSignalSpy spy(muc, &QXmppMucManagerV2::invitationReceived);

    QXmppMessage msg;
    parsePacket(msg,
                "<message from='coven@chat.shakespeare.lit' to='hecate@shakespeare.lit'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<invite from='crone1@shakespeare.lit/desktop'>"
                "<reason>Hey Hecate!</reason>"
                "</invite>"
                "<password>cauldronburn</password>"
                "</x>"
                "</message>");
    muc->handleMessage(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"coven@chat.shakespeare.lit");
    auto invite = spy.at(0).at(1).value<QXmpp::Muc::Invite>();
    QCOMPARE(invite.from(), u"crone1@shakespeare.lit/desktop");
    QCOMPARE(invite.reason(), u"Hey Hecate!");
    QCOMPARE(spy.at(0).at(2).toString(), u"cauldronburn");
}

void tst_QXmppMuc::invitationReceivedUnknownRoom()
{
    // Invitation from a room the user hasn't joined yet must still be signalled.
    TestClient test(true);
    test.configuration().setJid(u"hecate@shakespeare.lit/broom"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    QSignalSpy spy(muc, &QXmppMucManagerV2::invitationReceived);

    QXmppMessage msg;
    parsePacket(msg,
                "<message from='coven@chat.shakespeare.lit' to='hecate@shakespeare.lit'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<invite from='crone1@shakespeare.lit/desktop'>"
                "<reason>Join us!</reason>"
                "</invite>"
                "</x>"
                "</message>");
    muc->handleMessage(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"coven@chat.shakespeare.lit");
    auto invite = spy.at(0).at(1).value<QXmpp::Muc::Invite>();
    QCOMPARE(invite.from(), u"crone1@shakespeare.lit/desktop");
    QCOMPARE(invite.reason(), u"Join us!");
    QVERIFY(spy.at(0).at(2).toString().isEmpty());
}

void tst_QXmppMuc::declineInvitation()
{
    TestClient test(true);
    test.configuration().setJid(u"hecate@shakespeare.lit/broom"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    QXmpp::Muc::Decline decline;
    decline.setTo(u"crone1@shakespeare.lit/desktop"_s);
    decline.setReason(u"Too busy."_s);

    auto task = muc->declineInvitation(u"coven@chat.shakespeare.lit"_s, decline);
    test.expect(u"<message to='coven@chat.shakespeare.lit' type='normal'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<decline to='crone1@shakespeare.lit/desktop'>"
                "<reason>Too busy.</reason>"
                "</decline>"
                "</x>"
                "</message>"_s);
    QVERIFY(!task.isFinished());
}

void tst_QXmppMuc::invitationDeclined()
{
    TestClient test(true);
    test.configuration().setJid(u"crone1@shakespeare.lit/desktop"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *muc = test.addNewExtension<QXmppMucManagerV2>();

    joinTestRoom(test, muc, u"coven@chat.shakespeare.lit"_s, u"firstwitch"_s);

    QSignalSpy spy(muc, &QXmppMucManagerV2::invitationDeclined);

    QXmppMessage msg;
    parsePacket(msg,
                "<message from='coven@chat.shakespeare.lit' to='crone1@shakespeare.lit/desktop'>"
                "<x xmlns='http://jabber.org/protocol/muc#user'>"
                "<decline from='hecate@shakespeare.lit'>"
                "<reason>Too busy.</reason>"
                "</decline>"
                "</x>"
                "</message>");
    muc->handleMessage(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), u"coven@chat.shakespeare.lit");
    auto decline = spy.at(0).at(1).value<QXmpp::Muc::Decline>();
    QCOMPARE(decline.from(), u"hecate@shakespeare.lit");
    QCOMPARE(decline.reason(), u"Too busy.");
}

QTEST_MAIN(tst_QXmppMuc)
#include "tst_qxmppmuc.moc"

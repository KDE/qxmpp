// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppMucManagerV2.h"
#include "QXmppPubSubManager.h"

#include "TestClient.h"

using namespace QXmpp;
using namespace QXmpp::Private;

class tst_QXmppMuc : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void bookmarks2Updates();
    Q_SLOT void bookmarks2Set();
    Q_SLOT void bookmarks2Remove();
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

QTEST_MAIN(tst_QXmppMuc)
#include "tst_qxmppmuc.moc"

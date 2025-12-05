// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppMucManagerV2.h"
#include "QXmppPubSubManager.h"

#include "TestClient.h"

using namespace QXmpp::Private;

class tst_QXmppMuc : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void bookmarks2Updates();
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

QTEST_MAIN(tst_QXmppMuc)
#include "tst_qxmppmuc.moc"

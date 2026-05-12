// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppRosterMemoryStorage.h"
#include "QXmppTask.h"

#include "Algorithms.h"
#include "StringLiterals.h"

#include <QtTest>

using namespace QXmpp::Private;

class tst_QXmppRosterMemoryStorage : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testEmpty();
    Q_SLOT void testReplaceAll();
    Q_SLOT void testUpsert();
    Q_SLOT void testRemove();
    Q_SLOT void testClear();

    static QXmppRosterIq::Item makeItem(const QString &jid, const QString &name = {})
    {
        QXmppRosterIq::Item item;
        item.setBareJid(jid);
        if (!name.isEmpty()) {
            item.setName(name);
        }
        return item;
    }
};

void tst_QXmppRosterMemoryStorage::testEmpty()
{
    QXmppRosterMemoryStorage storage;
    auto future = storage.load();
    QVERIFY(future.isFinished());
    auto cache = future.takeResult();
    QVERIFY(cache.version.isEmpty());
    QVERIFY(cache.items.empty());
}

void tst_QXmppRosterMemoryStorage::testReplaceAll()
{
    QXmppRosterMemoryStorage storage;
    const std::vector<QXmppRosterIq::Item> items = {
        makeItem(u"alice@example.org"_s, u"Alice"_s),
        makeItem(u"bob@example.org"_s, u"Bob"_s),
    };
    auto setFuture = storage.replaceAll(u"v1"_s, items);
    QVERIFY(setFuture.isFinished());

    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v1"_s);
    QCOMPARE(cache.items.size(), 2u);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice"_s);
    QCOMPARE(find(cache.items, u"bob@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Bob"_s);

    // A second replaceAll wipes the prior set.
    storage.replaceAll(u"v2"_s, { makeItem(u"carol@example.org"_s) });
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QVERIFY(find(cache.items, u"carol@example.org"_s, &QXmppRosterIq::Item::bareJid).has_value());
}

void tst_QXmppRosterMemoryStorage::testUpsert()
{
    QXmppRosterMemoryStorage storage;
    storage.upsertItem(u"v1"_s, makeItem(u"alice@example.org"_s, u"Alice"_s));
    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v1"_s);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice"_s);

    // Second upsert with same JID replaces the prior value and advances the version.
    storage.upsertItem(u"v2"_s, makeItem(u"alice@example.org"_s, u"Alice in Wonderland"_s));
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QCOMPARE(find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid)->name(), u"Alice in Wonderland"_s);
}

void tst_QXmppRosterMemoryStorage::testRemove()
{
    QXmppRosterMemoryStorage storage;
    storage.replaceAll(u"v1"_s, { makeItem(u"alice@example.org"_s), makeItem(u"bob@example.org"_s) });

    storage.removeItem(u"v2"_s, u"alice@example.org"_s);
    auto cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v2"_s);
    QCOMPARE(cache.items.size(), 1u);
    QVERIFY(!find(cache.items, u"alice@example.org"_s, &QXmppRosterIq::Item::bareJid).has_value());

    // Removing a non-existing JID is idempotent but still advances the version.
    storage.removeItem(u"v3"_s, u"nonexistent@example.org"_s);
    cache = storage.load().takeResult();
    QCOMPARE(cache.version, u"v3"_s);
    QCOMPARE(cache.items.size(), 1u);
}

void tst_QXmppRosterMemoryStorage::testClear()
{
    QXmppRosterMemoryStorage storage;
    storage.replaceAll(u"v1"_s, { makeItem(u"alice@example.org"_s) });

    storage.clear();
    auto cache = storage.load().takeResult();
    QVERIFY(cache.version.isEmpty());
    QVERIFY(cache.items.empty());
}

QTEST_MAIN(tst_QXmppRosterMemoryStorage)
#include "tst_QXmppRosterMemoryStorage.moc"

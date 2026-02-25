// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppBitsOfBinaryIq.h"

#include "util.h"

#include <QMimeType>
#include <QObject>

Q_DECLARE_METATYPE(QCryptographicHash::Algorithm)

class tst_QXmppBitsOfBinary : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testIq();
    Q_SLOT void testResult();
    Q_SLOT void testOtherSubelement();
    Q_SLOT void testIsBobIq();
    Q_SLOT void fromByteArray();

    Q_SLOT void testContentId();

    Q_SLOT void testFromContentId_data();
    Q_SLOT void testFromContentId();

    Q_SLOT void testFromCidUrl_data();
    Q_SLOT void testFromCidUrl();

    Q_SLOT void testEmpty();

    Q_SLOT void testIsValid_data();
    Q_SLOT void testIsValid();

    Q_SLOT void testIsBobContentId_data();
    Q_SLOT void testIsBobContentId();

    Q_SLOT void testUnsupportedAlgorithm();
};

void tst_QXmppBitsOfBinary::testIq()
{
    const QByteArray xml(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.from(), u"doctor@shakespeare.lit/pda"_s);
    QCOMPARE(iq.id(), u"get-data-1"_s);
    QCOMPARE(iq.to(), u"ladymacbeth@shakespeare.lit/castle"_s);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.cid().toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(iq.contentType(), QMimeType());
    QCOMPARE(iq.data(), QByteArray());
    QCOMPARE(iq.maxAge(), -1);
    serializePacket(iq, xml);

    iq = QXmppBitsOfBinaryIq();
    iq.setFrom(u"doctor@shakespeare.lit/pda"_s);
    iq.setId(u"get-data-1"_s);
    iq.setTo(u"ladymacbeth@shakespeare.lit/castle"_s);
    iq.setType(QXmppIq::Get);
    iq.setCid(QXmppBitsOfBinaryContentId::fromContentId(u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s));
    serializePacket(iq, xml);
}

void tst_QXmppBitsOfBinary::testResult()
{
    const QByteArray xml = QByteArrayLiteral(
        "<iq id=\"data-result\" "
        "to=\"doctor@shakespeare.lit/pda\" "
        "from=\"ladymacbeth@shakespeare.lit/castle\" "
        "type=\"result\">"
        "<data xmlns=\"urn:xmpp:bob\" "
        "cid=\"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org\" "
        "max-age=\"86400\" "
        "type=\"image/png\">"
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC"
        "</data>"
        "</iq>");

    const auto data = QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC"));

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.type(), QXmppIq::Result);
    QCOMPARE(iq.id(), u"data-result"_s);
    QCOMPARE(iq.cid().algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(iq.cid().hash(), QByteArray::fromHex(QByteArrayLiteral("5a4c38d44fc64805cbb2d92d8b208be13ff40c0f")));
    QCOMPARE(iq.contentType(), QMimeDatabase().mimeTypeForName(u"image/png"_s));
    QCOMPARE(iq.maxAge(), 86400);
    QCOMPARE(iq.data(), data);
    serializePacket(iq, xml);

    iq = QXmppBitsOfBinaryIq();
    iq.setId(u"data-result"_s);
    iq.setFrom(u"ladymacbeth@shakespeare.lit/castle"_s);
    iq.setTo(u"doctor@shakespeare.lit/pda"_s);
    iq.setType(QXmppIq::Result);
    iq.setCid(QXmppBitsOfBinaryContentId::fromContentId(
        u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s));
    iq.setContentType(QMimeDatabase().mimeTypeForName(u"image/png"_s));
    iq.setMaxAge(86400);
    iq.setData(data);
    serializePacket(iq, xml);
}

void tst_QXmppBitsOfBinary::testOtherSubelement()
{
    const QByteArray xml(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"org.example.other.data\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");

    QXmppBitsOfBinaryIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.from(), u"doctor@shakespeare.lit/pda"_s);
    QCOMPARE(iq.id(), u"get-data-1"_s);
    QCOMPARE(iq.to(), u"ladymacbeth@shakespeare.lit/castle"_s);
    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.cid().toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(iq.contentType(), QMimeType());
    QCOMPARE(iq.data(), QByteArray());
    QCOMPARE(iq.maxAge(), -1);
}

void tst_QXmppBitsOfBinary::testIsBobIq()
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    const QByteArray xmlSimple(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlSimple)), true);

    // IQs must have only one child element
    const QByteArray xmlMultipleElements(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:other-data-format:0\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "<data xmlns=\"urn:xmpp:bob\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlMultipleElements)), false);

    const QByteArray xmlWithoutBobData(
        "<iq id=\"get-data-1\" "
        "to=\"ladymacbeth@shakespeare.lit/castle\" "
        "from=\"doctor@shakespeare.lit/pda\" "
        "type=\"get\">"
        "<data xmlns=\"urn:xmpp:other-data-format:0\" cid=\"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org\"></data>"
        "</iq>");
    QCOMPARE(QXmppBitsOfBinaryIq::isBitsOfBinaryIq(xmlToDom(xmlWithoutBobData)), false);
    QT_WARNING_POP
}

void tst_QXmppBitsOfBinary::fromByteArray()
{
    auto data = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAALQAAAA8BAMAAAA9AI20AAAAG1BMVEX///8AAADf39+"
        "/v79/f39fX1+fn58/Pz8fHx/8ACGJAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADS0lEQV"
        "RYhe2WS3MSQRCAYTf7OKY1kT0CxsRjHmh5BENIjqEk6pHVhFzdikqO7CGyP9t59Ox2z"
        "y6UeWBVqugLzM70Nz39mqnV1lIWgBWiYXV0BYfNZ0mvwypds1r62vH/gf76ZL/88Qlc"
        "41zeAnQrpx5H3z1Npfr5ovmHusa9SpRiNNIOcdrto6PJ5LLfb5bp9zM+VDq/vptxDEa"
        "a1sql9I3R5KhtfQsA5gNCWYyulV3TyTUDdfL56BvdDl4x7RiybDq9uBgxh1TTPUHDvA"
        "qNQb+LpT5sWehxJZKKcU2MZ6sDE7PMgW2mdlBGdy6ODe6fJFdMI+us95dNqftDMdwU6"
        "+MhpuTS9slcy5TFAcwq0Jt6qssJMTQGp4BGURlmSsNoo5oHL4kqc66NdkDO75mIfCxm"
        "RAlvHxMLdcb7JONavMJbttXXKoMSneYu3OQTlwkUh4mNayi6js55/2VcsZOQfXIYelz"
        "xLcntEGc3WVCsCORJVCc5r0ajAcq+EO1Q0oPm7n7+X/3jEReGdL6qT7Ml6FCjY+quJC"
        "r+D01f6BG0SaHG56ZG32DnY2jcEV1+pU0kxTaEwaGcekN7jyu50U/TV4q6YeieyiNTu"
        "klDKZLukyjKVNwotCUB3B0XO1WjHT3c0DHSO2zACwut8GOiljJIHaJsrlof/fpWNzGM"
        "os6TgIY0hZNpJshzSi4igOhy3cl4qK+YgnqHkAYcZEgdW6/HyrEK7afoY7RCFzArLl2"
        "LLDdrdmmHZfROajwIDfWj8yQG+rzwlA3WvdJiMHtjUekiNrp1oCbmyZDEyKROGjFVDr"
        "PRzlkR9UAfG/OErnPxrop5BwpoEpXQorq2zcGxbnBJndx8Bh0yljGiGv0B4E8+YP3Xp"
        "2rGydZNy4csW8W2pIvWhvijoujRJ0luXsoymV+8AXvE9HjII72+oReS6OfomHe3xWg/"
        "f2coSbDa1XZ1CvGMjy1nH9KBl83oPnQKi+vAXKLjCrRvvT2WCMkPmSFbquiVuTH1qjv"
        "p4j/u7CWyI5/Hn3KAaJJ90eP0Zp1Kjets4WPaElkxheF7cpBESzXuIdLwyFjSub07tB"
        "6JjxH3DGiu+zwHHimdtFsMvKqG/nBxm2TwbvyU6LWs5RnJX4dSldg3QhDLAAAAAElFT"
        "kSuQmCC");
    auto size = data.size();
    auto bobData = QXmppBitsOfBinaryData::fromByteArray(std::move(data));
    QCOMPARE(bobData.cid().toContentId(), u"sha1+5a4c38d44fc64805cbb2d92d8b208be13ff40c0f@bob.xmpp.org"_s);
    QCOMPARE(bobData.data().size(), size);
}

void tst_QXmppBitsOfBinary::testContentId()
{
    // test fromCidUrl()
    QXmppBitsOfBinaryContentId cid = QXmppBitsOfBinaryContentId::fromCidUrl(QStringLiteral(
        "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"));

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);

    // test fromContentId()
    cid = QXmppBitsOfBinaryContentId::fromContentId(QStringLiteral(
        "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"));

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);

    // test setters
    cid = QXmppBitsOfBinaryContentId();
    cid.setHash(QByteArray::fromHex(QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242")));
    cid.setAlgorithm(QCryptographicHash::Sha1);

    QCOMPARE(cid.algorithm(), QCryptographicHash::Sha1);
    QCOMPARE(cid.hash().toHex(), QByteArrayLiteral("8f35fef110ffc5df08d579a50083ff9308fb6242"));
    QCOMPARE(cid.toCidUrl(), u"cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
    QCOMPARE(cid.toContentId(), u"sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s);
}

void tst_QXmppBitsOfBinary::testFromContentId_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << IS_VALID

    ROW("valid", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true);
    ROW("wrong-namespace", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob_222.xmpp.org", false);
    ROW("no-namespace", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@", false);
    ROW("url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("url-and-wrong-namespace", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob_222.xmpp.org", false);
    ROW("too-many-pluses", "sha1+sha256+sha3-256+blake2b256+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("wrong-hash-length", "cid:sha1+08d579a50083ff9308fb6242@bob.xmpp.org", false);

#undef ROW
}

void tst_QXmppBitsOfBinary::testFromContentId()
{
    QFETCH(QString, input);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::fromContentId(input).isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testFromCidUrl_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << IS_VALID

    ROW("valid", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true);
    ROW("no-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
    ROW("wrong-namespace", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@other", false);
    ROW("too-many-pluses", "cid:sha1+sha256+sha3-256+blake2b256+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false);
#undef ROW
}

void tst_QXmppBitsOfBinary::testFromCidUrl()
{
    QFETCH(QString, input);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::fromCidUrl(input).isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testEmpty()
{
    QXmppBitsOfBinaryContentId cid;
    QVERIFY(cid.toCidUrl().isEmpty());
    QVERIFY(cid.toContentId().isEmpty());
}

void tst_QXmppBitsOfBinary::testIsValid_data()
{
    QTest::addColumn<QByteArray>("hash");
    QTest::addColumn<QCryptographicHash::Algorithm>("algorithm");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, HASH, ALGORITHM, IS_VALID) \
    QTest::newRow(NAME) << QByteArray::fromHex(HASH) << ALGORITHM << IS_VALID

    ROW("valid",
        "8f35fef110ffc5df08d579a50083ff9308fb6242",
        QCryptographicHash::Sha1,
        true);
    ROW("valid-sha256",
        "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b",
        QCryptographicHash::Sha256,
        true);
    ROW("wrong-hash-length", "8f35fef110ffc5df08", QCryptographicHash::Sha1, false);

#undef ROW
}

void tst_QXmppBitsOfBinary::testIsValid()
{
    QFETCH(QByteArray, hash);
    QFETCH(QCryptographicHash::Algorithm, algorithm);
    QFETCH(bool, isValid);

    QXmppBitsOfBinaryContentId contentId;
    contentId.setAlgorithm(algorithm);
    contentId.setHash(hash);

    QCOMPARE(contentId.isValid(), isValid);
}

void tst_QXmppBitsOfBinary::testIsBobContentId_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("checkIsUrl");
    QTest::addColumn<bool>("isValid");

#define ROW(NAME, INPUT, CHECK_IS_URL, IS_VALID) \
    QTest::newRow(NAME) << QStringLiteral(INPUT) << CHECK_IS_URL << IS_VALID

    ROW("valid-url-check-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true, true);
    ROW("valid-url-no-check-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, true);
    ROW("valid-id-no-check-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, true);
    ROW("not-an-url", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", true, false);

    ROW("invalid-namespace-id", "sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org.org.org", false, false);
    ROW("invalid-namespace-url", "cid:sha1+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org.org.org", true, false);

    ROW("no-hash-algorithm", "sha18f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org", false, false);
#undef ROW
}

void tst_QXmppBitsOfBinary::testIsBobContentId()
{
    QFETCH(QString, input);
    QFETCH(bool, checkIsUrl);
    QFETCH(bool, isValid);

    QCOMPARE(QXmppBitsOfBinaryContentId::isBitsOfBinaryContentId(input, checkIsUrl), isValid);
}

void tst_QXmppBitsOfBinary::testUnsupportedAlgorithm()
{
    QCOMPARE(
        QXmppBitsOfBinaryContentId::fromContentId(
            u"blake2s160+8f35fef110ffc5df08d579a50083ff9308fb6242@bob.xmpp.org"_s),
        QXmppBitsOfBinaryContentId());
}

QTEST_MAIN(tst_QXmppBitsOfBinary)
#include "tst_qxmppbitsofbinary.moc"

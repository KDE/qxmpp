// SPDX-FileCopyrightText: 2015 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Yury Gubich <blue@macaw.me>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the file sharing tests. Merging the HTTP upload,
// transfer manager and file encryption tests into one translation unit parses
// the shared Qt/QXmpp headers once instead of once per file. Each original
// test keeps its own namespace; main() runs them in turn.
//
// Encrypted file sharing requires OpenSSL, so those tests are guarded by
// WITH_ENCRYPTION.

#include "QXmppBitsOfBinaryContentId.h"
#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppEncryptedFileSource.h"
#include "QXmppFileMetadata.h"
#include "QXmppFileShare_p.h"
#include "QXmppHash.h"
#include "QXmppHttpFileSharingProvider.h"
#include "QXmppHttpFileSource.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppHttpUploadManager.h"
#include "QXmppServer.h"
#include "QXmppThumbnail.h"
#include "QXmppTransferManager.h"
#include "QXmppUploadRequestManager.h"

#ifdef WITH_ENCRYPTION
#include "QXmppEncryptedFileSharingProvider.h"
#include "QXmppFileEncryption.h"
#include "QXmppFileSharingManager.h"

#include <openssl/err.h>
#endif

#include "QXmppStreamInitiationIq_p.h"

#include "Algorithms.h"
#include "IntegrationTesting.h"
#include "TestClient.h"
#include "TestPasswordChecker.h"
#include "util.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

namespace HttpUpload {

using namespace QXmpp;
using namespace QXmpp::Private;

static const auto UPLOAD_SERVICE_NAME = u"upload.montague.tld"_s;
constexpr quint64 MAX_FILE_SIZE = 500UL * 1024UL * 1024UL;

static void addUploadService(QXmppClient &client)
{
    QVERIFY(client.findExtension<QXmppUploadRequestManager>());
    QVERIFY(client.findExtension<QXmppDiscoveryManager>());

    QByteArray xml =
        "<iq from='" +
        UPLOAD_SERVICE_NAME.toUtf8() +
        "' id='step_02' to='romeo@montague.tld/garden' type='result'>"
        "<query xmlns='http://jabber.org/protocol/disco#info'>"
        "<identity category='store' type='file' name='HTTP File Upload' />"
        "<feature var='urn:xmpp:http:upload:0' />"
        "<x type='result' xmlns='jabber:x:data'>"
        "<field var='FORM_TYPE' type='hidden'>"
        "<value>urn:xmpp:http:upload:0</value>"
        "</field>"
        "<field var='max-file-size'>"
        "<value>" +
        QByteArray::number(MAX_FILE_SIZE) +
        "</value>"
        "</field>"
        "</x>"
        "</query>"
        "</iq>";
    auto *discovery = client.findExtension<QXmppDiscoveryManager>();
    QVERIFY(TestClient::handleStanza(discovery, xmlToDom(xml)));
}

class tst_QXmppHttpUploadManager : public QObject
{
    Q_OBJECT
private:
    // UploadRequestManager
    Q_SLOT void testDiscoveryService_data();
    Q_SLOT void testDiscoveryService();
    Q_SLOT void testHandleStanza_data();
    Q_SLOT void testHandleStanza();
    Q_SLOT void testSending_data();
    Q_SLOT void testSending();
    Q_SLOT void testSendingFuture_data();
    Q_SLOT void testSendingFuture();
    Q_SLOT void testUploadService();

    // HttpUploadManager
    Q_SLOT void testUpload();
    Q_SLOT void testSupport();
};

void tst_QXmppHttpUploadManager::testSupport()
{
    using Support = QXmppHttpUploadManager::Support;

    TestClient test;
    test.configuration().setJid(u"juliet@capulet.example"_s);
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = test.addNewExtension<QXmppHttpUploadManager>();

    // support seen by handlers of servicesChanged once a service has been found
    QList<Support> supportWithServices;
    connect(manager, &QXmppHttpUploadManager::servicesChanged, &test, [&] {
        if (!manager->services().isEmpty()) {
            supportWithServices.append(manager->support());
        }
    });

    test.setStreamManagementState(QXmppClient::NewStream);
    test.simulateConnected();
    QCOMPARE(manager->support(), Support::Unknown);

    auto itemsId = test.expectPacketRandomOrder(
        u"<iq id='qx1' to='capulet.example' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"_s);
    test.inject(u"<iq id='" + itemsId + u"' from='capulet.example' type='result'>"
                                        "<query xmlns='http://jabber.org/protocol/disco#items'>"
                                        "<item jid='upload.capulet.example'/>"
                                        "<item jid='slow.capulet.example'/>"
                                        "</query></iq>");

    auto uploadId = test.expectPacketRandomOrder(
        u"<iq id='qx1' to='upload.capulet.example' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"_s);
    test.inject(u"<iq id='" + uploadId + u"' from='upload.capulet.example' type='result'>"
                                         "<query xmlns='http://jabber.org/protocol/disco#info'>"
                                         "<identity category='store' type='file'/>"
                                         "<feature var='urn:xmpp:http:upload:0'/>"
                                         "</query></iq>");

    // supported before the discovery of the other service has finished
    QCOMPARE(manager->support(), Support::Supported);
    QCOMPARE(supportWithServices, QList { Support::Supported });
}

void tst_QXmppHttpUploadManager::testHandleStanza_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("accepted");
    QTest::addColumn<bool>("event");
    QTest::addColumn<bool>("error");

    QTest::newRow("notAccepted")
        << QByteArray("<message xmlns='jabber:client' "
                      "from='romeo@montague.example' "
                      "to='romeo@montague.example/home' "
                      "type='chat'>"
                      "<received xmlns='urn:xmpp:carbons:2'>"
                      "<forwarded xmlns='urn:xmpp:forward:0'>"
                      "<message xmlns='jabber:client' "
                      "from='juliet@capulet.example/balcony' "
                      "to='romeo@montague.example/garden' "
                      "type='chat'>"
                      "<body>What man art thou that, thus bescreen'd in night, so stumblest on my counsel?</body>"
                      "<thread>0e3141cd80894871a68e6fe6b1ec56fa</thread>"
                      "</message>"
                      "</forwarded>"
                      "</received>"
                      "</message>")
        << false << false << false;

    QTest::newRow("slotReceived")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='result'>"
                      "<slot xmlns='urn:xmpp:http:upload:0'>"
                      "<put url='https://upload.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg'>"
                      "<header name='Authorization'>Basic Base64String==</header>"
                      "<header name='Cookie'>foo=bar; user=romeo</header>"
                      "</put>"
                      "<get url='https://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg' />"
                      "</slot>"
                      "</iq>")
        << true << true << false;

    QTest::newRow("tooLargeError")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='error'>"
                      "<request xmlns='urn:xmpp:http:upload:0' filename='très cool.jpg' size='23456' content-type='image/jpeg' />"
                      "<error type='modify'>"
                      "<not-acceptable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas' />"
                      "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>File too large. The maximum file size is 20000 bytes</text>"
                      "<file-too-large xmlns='urn:xmpp:http:upload:0'>"
                      "<max-file-size>20000</max-file-size>"
                      "</file-too-large>"
                      "</error>"
                      "</iq>")
        << true << true << true;

    QTest::newRow("quotaReachedError")
        << QByteArray("<iq from='upload.montague.tld' id='step_03' to='romeo@montague.tld/garden' type='error'>"
                      "<request xmlns='urn:xmpp:http:upload:0' filename='très cool.jpg' size='23456' content-type='image/jpeg' />"
                      "<error type='wait'>"
                      "<resource-constraint xmlns='urn:ietf:params:xml:ns:xmpp-stanzas' />"
                      "<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>Quota reached. You can only upload 5 files in 5 minutes</text>"
                      "<retry xmlns='urn:xmpp:http:upload:0' stamp='2017-12-03T23:42:05Z' />"
                      "</error>"
                      "</iq>")
        << true << true << true;
}

void tst_QXmppHttpUploadManager::testHandleStanza()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, accepted);
    QFETCH(bool, event);
    QFETCH(bool, error);

    TestClient test;
    auto *manager = test.addNewExtension<QXmppUploadRequestManager>();

    bool eventReceived = false;
    bool errorReceived = false;

    QObject context;
    connect(manager, &QXmppUploadRequestManager::slotReceived, &context, [&](const auto &) {
        eventReceived = true;
        errorReceived = false;
    });
    connect(manager, &QXmppUploadRequestManager::requestFailed, &context, [&](const auto &) {
        eventReceived = true;
        errorReceived = true;
    });

    bool realAccepted = TestClient::handleStanza(manager, xmlToDom(xml));

    QCOMPARE(realAccepted, accepted);
    QCOMPARE(eventReceived, event);
    QCOMPARE(errorReceived, error);
}

void tst_QXmppHttpUploadManager::testDiscoveryService_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("discovered");

    QTest::newRow("mixDiscoveryStanzaIq")
        << QByteArray("<iq from='mix.shakespeare.example' id='lx09df27' to='hag66@shakespeare.example/UUID-c8y/1573' type='result'>"
                      "<query xmlns='http://jabber.org/protocol/disco#info'>"
                      "<identity category='conference' name='Shakespearean Chat Service' type='mix '/>"
                      "<feature var='urn:xmpp:mix:core:1' />"
                      "<feature var='urn:xmpp:mix:core:1#searchable' />"
                      "</query>"
                      "</iq>")
        << false;

    QTest::newRow("HTTPUploadDiscoveryStanzaIq")
        << "<iq from='" +
            UPLOAD_SERVICE_NAME.toUtf8() +
            "' id='step_02' to='romeo@montague.tld/garden' type='result'>"
            "<query xmlns='http://jabber.org/protocol/disco#info'>"
            "<identity category='store' type='file' name='HTTP File Upload' />"
            "<feature var='urn:xmpp:http:upload:0' />"
            "<x type='result' xmlns='jabber:x:data'>"
            "<field var='FORM_TYPE' type='hidden'>"
            "<value>urn:xmpp:http:upload:0</value>"
            "</field>"
            "<field var='max-file-size'>"
            "<value>" +
            QByteArray::number(MAX_FILE_SIZE) +
            "</value>"
            "</field>"
            "</x>"
            "</query>"
            "</iq>"
        << true;

    QTest::newRow("correct disco info with multiple forms")
        << "<iq from='" +
            UPLOAD_SERVICE_NAME.toUtf8() +
            "' id='step_02' to='romeo@montague.tld/garden' type='result'>"
            "<query xmlns='http://jabber.org/protocol/disco#info'>"
            "<identity category='store' type='file' name='HTTP File Upload' />"
            "<feature var='urn:xmpp:http:upload:0' />"
            "<x type='result' xmlns='jabber:x:data'>"
            "<field var='FORM_TYPE' type='hidden'>"
            "<value>urn:xmpp:http:upload:2</value>"
            "</field>"
            "</x>"
            "<x type='result' xmlns='jabber:x:data'>"
            "<field var='FORM_TYPE' type='hidden'>"
            "<value>urn:xmpp:http:upload:0</value>"
            "</field>"
            "<field var='max-file-size'>"
            "<value>" +
            QByteArray::number(MAX_FILE_SIZE) +
            "</value>"
            "</field>"
            "</x>"
            "<x type='result' xmlns='jabber:x:data'>"
            "<field var='FORM_TYPE' type='hidden'>"
            "<value>urn:xmpp:http:new-fancy-upload:0</value>"
            "</field>"
            "</x>"
            "</query>"
            "</iq>"
        << true;
}

void tst_QXmppHttpUploadManager::testDiscoveryService()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, discovered);

    TestClient test;
    auto *discovery = test.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = test.addNewExtension<QXmppUploadRequestManager>();

    bool accepted = TestClient::handleStanza(discovery, xmlToDom(xml));
    QCOMPARE(accepted, true);
    QCOMPARE(manager->serviceFound(), discovered);

    if (manager->serviceFound()) {
        QCOMPARE(manager->uploadServices().at(0).jid(), UPLOAD_SERVICE_NAME);
        QCOMPARE(manager->uploadServices().at(0).sizeLimit(), qint64(MAX_FILE_SIZE));
    }
}

void tst_QXmppHttpUploadManager::testSending_data()
{
    QTest::addColumn<QFileInfo>("fileInfo");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<qint64>("fileSize");
    QTest::addColumn<QString>("fileType");

    QTest::newRow("fileInfo")
        << QFileInfo(":/test.svg")
        << "test.svg"
        << 2280LL
        << "image/svg+xml";

    QTest::newRow("fileWithSizeBelowLimit")
        << QFileInfo()
        << "whatever.jpeg"
        << 698547LL
        << "image/jpeg";

    QTest::newRow("fileWithSizeAboveLimit")
        << QFileInfo()
        << "some.pdf"
        << 65896498547LL
        << "application/pdf";

    // there is no size above limit handling in request manager
    // there is also no code that selects an upload service with proper
    // size limit above requesting file size.
    // Is it something to worry about?
}

void tst_QXmppHttpUploadManager::testSending()
{
    QFETCH(QFileInfo, fileInfo);
    QFETCH(QString, fileName);
    QFETCH(qint64, fileSize);
    QFETCH(QString, fileType);

    auto expectedMimeType = QMimeDatabase().mimeTypeForName(fileType);

    TestClient test;
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = test.addNewExtension<QXmppUploadRequestManager>();

    addUploadService(test);

    QString returnId;
    if (!fileInfo.baseName().isEmpty()) {
        returnId = manager->requestUploadSlot(fileInfo);
    } else {
        returnId = manager->requestUploadSlot(fileName, fileSize, expectedMimeType);
    }

    QXmppHttpUploadRequestIq iq;
    parsePacket(iq, test.takePacket().toUtf8());

    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.to(), UPLOAD_SERVICE_NAME);
    QCOMPARE(iq.fileName(), fileName);
    QCOMPARE(iq.size(), fileSize);
    QCOMPARE(iq.contentType(), expectedMimeType);

    // The client is not connected, so we never get an ID back (the packet was not sent).
    QVERIFY(returnId.isNull());
}

void tst_QXmppHttpUploadManager::testSendingFuture_data()
{
    testSending_data();
}

void tst_QXmppHttpUploadManager::testSendingFuture()
{
    QFETCH(QFileInfo, fileInfo);
    QFETCH(QString, fileName);
    QFETCH(qint64, fileSize);
    QFETCH(QString, fileType);

    auto expectedMimeType = QMimeDatabase().mimeTypeForName(fileType);

    TestClient test;
    test.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = test.addNewExtension<QXmppUploadRequestManager>();

    addUploadService(test);

    auto future = [=]() {
        if (!fileInfo.baseName().isEmpty()) {
            return manager->requestSlot(fileInfo);
        } else {
            return manager->requestSlot(fileName, fileSize, expectedMimeType);
        }
    }();

    QVERIFY(!future.isFinished());

    // check sent packet
    QXmppHttpUploadRequestIq iq;
    parsePacket(iq, test.takePacket().toUtf8());

    QCOMPARE(iq.type(), QXmppIq::Get);
    QCOMPARE(iq.to(), UPLOAD_SERVICE_NAME);
    QCOMPARE(iq.fileName(), fileName);
    QCOMPARE(iq.size(), fileSize);
    QCOMPARE(iq.contentType(), expectedMimeType);

    // inject reply
    QByteArray reply =
        "<iq from='" + iq.to().toUtf8() + "' id='" + iq.id().toUtf8() + "' to='" + iq.from().toUtf8() +
        "' type='result'>"
        "<slot xmlns='urn:xmpp:http:upload:0'>"
        "<put url='https://upload.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg'>"
        "<header name='Authorization'>Basic Base64String==</header>"
        "<header name='Content-type'>application/json</header>"
        "<header name='Cookie'>foo=bar; user=romeo</header>"
        "</put>"
        "<get url='https://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg' />"
        "</slot>"
        "</iq>";
    test.inject(reply);
    auto slot = expectFutureVariant<QXmppHttpUploadSlotIq>(future);

    QCOMPARE(slot.getUrl(), QUrl("https://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    QCOMPARE(slot.putUrl(), QUrl("https://upload.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    // checks that the disallowed 'Content-type' header is not set
    QCOMPARE(slot.putHeaders().size(), 2);
    QCOMPARE(slot.putHeaders().keys()[0], u"Authorization"_s);
    QCOMPARE(slot.putHeaders().keys()[1], u"Cookie"_s);
}

void tst_QXmppHttpUploadManager::testUploadService()
{
    QXmppUploadService service;
    QCOMPARE(service.sizeLimit(), -1LL);
    QVERIFY(service.jid().isNull());

    service.setSizeLimit(256LL * 1024LL * 1024LL);
    QCOMPARE(service.sizeLimit(), 256LL * 1024LL * 1024LL);

    service.setJid(u"upload.shakespeare.lit"_s);
    QCOMPARE(service.jid(), u"upload.shakespeare.lit"_s);
}

void tst_QXmppHttpUploadManager::testUpload()
{
    using DiscoInfoResult = std::variant<QXmppDiscoveryIq, QXmppError>;

    SKIP_IF_INTEGRATION_TESTS_DISABLED()

    TestClient test;
    auto *disco = test.addNewExtension<QXmppDiscoveryManager>();
    test.addNewExtension<QXmppUploadRequestManager>();
    auto *netManager = new QNetworkAccessManager(&test);
    if (IntegrationTests::ignoreSslErrors()) {
        connect(netManager, &QNetworkAccessManager::sslErrors, netManager, [](QNetworkReply *reply, const QList<QSslError> &) {
            reply->ignoreSslErrors();
        });
    }
    auto *uploadManager = test.addNewExtension<QXmppHttpUploadManager>(netManager);

    test.connectToServer(IntegrationTests::clientConfiguration());
    QSignalSpy(&test, &QXmppClient::connected).wait();
    QVERIFY(test.isConnected());

    // get server items
    auto items = expectVariant<QList<QXmppDiscoItem>>(wait(disco->items(test.configuration().domain()).toFuture(this)));
    // request disco info for each item
    auto infoFutures = transform<std::vector<std::tuple<QString, QXmppTask<Result<QXmppDiscoInfo>>>>>(items, [disco](const auto &item) {
        return std::tuple { item.jid(), disco->info(item.jid(), item.node()) };
    });
    auto uploadServiceJid = [&]() {
        for (auto &[jid, task] : infoFutures) {
            auto result = expectVariant<QXmppDiscoInfo>(wait(task.toFuture(this)));
            for (const auto &identity : result.identities()) {
                if (identity.category() == u"store" &&
                    identity.type() == u"file" &&
                    result.features().contains("urn:xmpp:http:upload:0")) {
                    return jid;
                }
            }
        }
        return QString();
    }();

    // check whether the server supports HTTP File Upload
    if (uploadServiceJid.isEmpty()) {
        QSKIP("The server does not support HTTP File Upload.");
    }

    auto upload = uploadManager->uploadFile(QFileInfo(":/test.svg"), "test_renamed.png", uploadServiceJid);
    QVERIFY2(!upload->isFinished(), "Uploading resulted instantly in an error");

    {
        // check sent request
        QXmppHttpUploadRequestIq iq;
        parsePacket(iq, test.takeLastPacket().toUtf8());

        QCOMPARE(iq.contentType().name(), u"image/svg+xml"_s);
        QCOMPARE(iq.fileName(), u"test_renamed.png"_s);
        QCOMPARE(iq.size(), 2280LL);
    }

    // test signals
    QSignalSpy finishedSpy(upload.get(), &QXmppHttpUpload::finished);
    QSignalSpy progressSpy(upload.get(), &QXmppHttpUpload::progressChanged);
    QTRY_COMPARE(finishedSpy.size(), 1);

    // test result
    auto result = upload->result().value();
    if (std::holds_alternative<QXmppError>(result)) {
        qDebug() << "Upload failed:" << std::get<QXmppError>(result).description;
        QVERIFY2(false, "Uploading the file failed");
    }
    QVERIFY(!progressSpy.empty());

    auto url = expectVariant<QUrl>(std::move(result));
    QCOMPARE(upload->bytesSent(), 2280ULL);
    QCOMPARE(upload->bytesTotal(), 2280ULL);
    QCOMPARE(upload->progress(), 1.0);

    qDebug() << "Uploaded file to" << url.toDisplayString();
}

}  // namespace HttpUpload

// ============================================================

namespace Transfer {

class tst_QXmppTransferManager : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void init();
    Q_SLOT void testSendFile_data();
    Q_SLOT void testSendFile();

    Q_SLOT void acceptFile(QXmppTransferJob *job);

    QBuffer receiverBuffer;
    QXmppTransferJob *receiverJob;
};

void tst_QXmppTransferManager::init()
{
    receiverBuffer.close();
    receiverBuffer.setData(QByteArray());
    receiverJob = nullptr;
}

void tst_QXmppTransferManager::acceptFile(QXmppTransferJob *job)
{
    receiverJob = job;
    receiverBuffer.open(QIODevice::WriteOnly);
    job->accept(&receiverBuffer);
}

void tst_QXmppTransferManager::testSendFile_data()
{
    QTest::addColumn<QXmppTransferJob::Method>("senderMethods");
    QTest::addColumn<QXmppTransferJob::Method>("receiverMethods");
    QTest::addColumn<bool>("works");

#ifdef Q_OS_WIN
    // On Windows CI, SOCKS transfers fail due to network interface issues in VM environments.
    // The discovered IP addresses may not be reachable from within the same machine.
    // Only test InBand transfers on Windows.
    QTest::newRow("inband - any") << QXmppTransferJob::InBandMethod << QXmppTransferJob::AnyMethod << true;
    QTest::newRow("inband - inband") << QXmppTransferJob::InBandMethod << QXmppTransferJob::InBandMethod << true;
    QTest::newRow("inband - socks") << QXmppTransferJob::InBandMethod << QXmppTransferJob::SocksMethod << false;
#else
    QTest::newRow("any - any") << QXmppTransferJob::AnyMethod << QXmppTransferJob::AnyMethod << true;
    QTest::newRow("any - inband") << QXmppTransferJob::AnyMethod << QXmppTransferJob::InBandMethod << true;
    QTest::newRow("any - socks") << QXmppTransferJob::AnyMethod << QXmppTransferJob::SocksMethod << true;

    QTest::newRow("inband - any") << QXmppTransferJob::InBandMethod << QXmppTransferJob::AnyMethod << true;
    QTest::newRow("inband - inband") << QXmppTransferJob::InBandMethod << QXmppTransferJob::InBandMethod << true;
    QTest::newRow("inband - socks") << QXmppTransferJob::InBandMethod << QXmppTransferJob::SocksMethod << false;

    QTest::newRow("socks - any") << QXmppTransferJob::SocksMethod << QXmppTransferJob::AnyMethod << true;
    QTest::newRow("socks - inband") << QXmppTransferJob::SocksMethod << QXmppTransferJob::InBandMethod << false;
    QTest::newRow("socks - socks") << QXmppTransferJob::SocksMethod << QXmppTransferJob::SocksMethod << true;
#endif
}

void tst_QXmppTransferManager::testSendFile()
{
    QFETCH(QXmppTransferJob::Method, senderMethods);
    QFETCH(QXmppTransferJob::Method, receiverMethods);
    QFETCH(bool, works);

    const QString testDomain("localhost");
    const QHostAddress testHost(QHostAddress::LocalHost);
    const quint16 testPort = 12003;

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::StdoutLogging);

    // prepare server
    TestPasswordChecker passwordChecker;
    passwordChecker.addCredentials("sender", "testpwd");
    passwordChecker.addCredentials("receiver", "testpwd");

    QXmppServer server;
    server.setDomain(testDomain);
    server.setLogger(&logger);
    server.setPasswordChecker(&passwordChecker);
    server.listenForClients(testHost, testPort);

    // prepare sender
    TestClient sender;
    auto *senderManager = new QXmppTransferManager;
    senderManager->setSupportedMethods(senderMethods);
    sender.addExtension(senderManager);
    sender.setLogger(&logger);

    QXmppConfiguration config;
    config.setDomain(testDomain);
    config.setHost(testHost.toString());
    config.setPort(testPort);
    config.setUser("sender");
    config.setPassword("testpwd");
    sender.connectToServer(config);
    sender.waitForConnect();
    QCOMPARE(sender.isConnected(), true);

    // prepare receiver
    TestClient receiver;
    auto *receiverManager = new QXmppTransferManager;
    receiverManager->setSupportedMethods(receiverMethods);
    connect(receiverManager, &QXmppTransferManager::fileReceived,
            this, &tst_QXmppTransferManager::acceptFile);
    receiver.addExtension(receiverManager);
    receiver.setLogger(&logger);

    config.setUser("receiver");
    config.setPassword("testpwd");
    receiver.connectToServer(config);
    receiver.waitForConnect();
    QCOMPARE(receiver.isConnected(), true);

    // send file
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QXmppTransferJob *senderJob = senderManager->sendFile(receiver.configuration().jid(), ":/test.svg");
    QVERIFY(senderJob);
    QCOMPARE(senderJob->localFileUrl(), QUrl::fromLocalFile(":/test.svg"));
    connect(senderJob, &QXmppTransferJob::finished, &loop, &QEventLoop::quit);
    timeout.start(10000);
    loop.exec();
    QVERIFY2(senderJob->state() == QXmppTransferJob::FinishedState, "Sender job timed out");

    if (works) {
        QCOMPARE(senderJob->error(), QXmppTransferJob::NoError);

        // finish receiving file
        QVERIFY(receiverJob);
        connect(receiverJob, &QXmppTransferJob::finished, &loop, &QEventLoop::quit);
        timeout.start(10000);
        loop.exec();
        QVERIFY2(receiverJob->state() == QXmppTransferJob::FinishedState, "Receiver job timed out");

        QCOMPARE(receiverJob->error(), QXmppTransferJob::NoError);

        // check received file
        QFile expectedFile(":/test.svg");
        QVERIFY(expectedFile.open(QIODevice::ReadOnly));
        const QByteArray expectedData = expectedFile.readAll();
        QCOMPARE(receiverBuffer.data(), expectedData);
    } else {
        QCOMPARE(senderJob->error(), QXmppTransferJob::AbortError);

        QVERIFY(!receiverJob);

        QCOMPARE(receiverBuffer.data(), QByteArray());
    }
}

}  // namespace Transfer

// ============================================================

namespace OmemoMediaSharing {

using namespace QXmpp;
using namespace QXmpp::Private::OmemoMediaSharing;

class tst_OmemoMediaSharing : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void aesGcmUrl_data();
    Q_SLOT void aesGcmUrl();
    Q_SLOT void parseBody_data();
    Q_SLOT void parseBody();
};

// example of XEP-0454
constexpr auto EXAMPLE_URL = u"https://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg";
constexpr auto EXAMPLE_IV = "8c3d050e9386ec173861778f";
constexpr auto EXAMPLE_KEY = "68e9af38a97aaf82faa4063b4d0878a61261534410c8a84331eaac851759f587";
constexpr auto EXAMPLE_AESGCM_URL = u"aesgcm://download.montague.tld/4a771ac1-f0b2-4a4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg#8c3d050e9386ec173861778f68e9af38a97aaf82faa4063b4d0878a61261534410c8a84331eaac851759f587";

static QXmppEncryptedFileSource exampleSource()
{
    QXmppEncryptedFileSource source;
    source.setCipher(Aes256GcmNoPad);
    source.setIv(QByteArray::fromHex(EXAMPLE_IV));
    source.setKey(QByteArray::fromHex(EXAMPLE_KEY));
    source.setHttpSources({ QXmppHttpFileSource(QUrl(QString(EXAMPLE_URL))) });
    return source;
}

void tst_OmemoMediaSharing::aesGcmUrl_data()
{
    QTest::addColumn<QXmppEncryptedFileSource>("source");
    QTest::addColumn<std::optional<QString>>("url");

    QTest::newRow("example") << exampleSource() << std::optional<QString>(EXAMPLE_AESGCM_URL);

    auto source = exampleSource();
    source.setCipher(Aes256CbcPkcs7);
    QTest::newRow("cbc") << source << std::optional<QString>();

    source = exampleSource();
    source.setCipher(Aes128GcmNoPad);
    source.setKey(QByteArray(16, 'k'));
    QTest::newRow("aes128") << source << std::optional<QString>();

    source = exampleSource();
    source.setIv(QByteArray(32, 'i'));
    QTest::newRow("iv-size") << source << std::optional<QString>();

    source = exampleSource();
    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"http://download.montague.tld/file.jpg"_s)) });
    QTest::newRow("http") << source << std::optional<QString>();

    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"http://download.montague.tld/file.jpg"_s)), QXmppHttpFileSource(QUrl(QString(EXAMPLE_URL))) });
    QTest::newRow("https-second") << source << std::optional<QString>(EXAMPLE_AESGCM_URL);
}

void tst_OmemoMediaSharing::aesGcmUrl()
{
    QFETCH(QXmppEncryptedFileSource, source);
    QFETCH(std::optional<QString>, url);

    QCOMPARE(OmemoMediaSharing::aesGcmUrl(source), url);
}

void tst_OmemoMediaSharing::parseBody_data()
{
    QTest::addColumn<QString>("body");
    QTest::addColumn<bool>("valid");

    const auto url = QString(EXAMPLE_AESGCM_URL);
    QTest::newRow("example") << url << true;
    QTest::newRow("trailing-newline") << url + u"\n"_s << true;
    QTest::newRow("thumbnail") << url + u"\ndata:image/jpeg;base64,/9j/4AAQSkZJRgABAQ=="_s << true;
    QTest::newRow("thumbnail-xep") << url + u"\ndata:image/jpeg,/9j/4AAQSkZJRgABAQ=="_s << true;
    QTest::newRow("thumbnail-png") << url + u"\ndata:image/png;base64,iVBORw0KGgo="_s << false;
    QTest::newRow("thumbnail-invalid") << url + u"\ndata:image/jpeg;base64,%%%"_s << false;
    QTest::newRow("text-before") << u"Look: "_s + url << false;
    QTest::newRow("text-after") << url + u" nice"_s << false;
    QTest::newRow("two-urls") << url + u"\n"_s + url << false;
    QTest::newRow("https") << QString(EXAMPLE_URL) + u"#8c3d050e9386ec173861778f68e9af38a97aaf82faa4063b4d0878a61261534410c8a84331eaac851759f587"_s << false;
    QTest::newRow("no-fragment") << url.left(url.indexOf(u'#')) << false;
    QTest::newRow("short-fragment") << url.chopped(2) << false;
    QTest::newRow("no-hex") << url.chopped(1) + u"x"_s << false;
    QTest::newRow("percent-encoded") << url.chopped(2) + u"%41"_s << false;
    QTest::newRow("legacy-iv") << url.left(url.indexOf(u'#') + 1) + u"8c3d050e9386ec173861778f00000000"_s + QString::fromLatin1(EXAMPLE_KEY) << true;
}

void tst_OmemoMediaSharing::parseBody()
{
    QFETCH(QString, body);
    QFETCH(bool, valid);

    auto parsed = OmemoMediaSharing::parseBody(body);
    QCOMPARE(parsed.has_value(), valid);
    if (!parsed) {
        return;
    }

    const auto &file = parsed->fileShare;
    QCOMPARE(file.metadata().filename(), u"très cool.jpg"_s);
    QCOMPARE(file.metadata().mediaType()->name(), u"image/jpeg"_s);
    QCOMPARE(file.encryptedSources().size(), 1);
    const auto &source = file.encryptedSources().constFirst();
    const auto url = body.trimmed().section(u'\n', 0, 0);
    QCOMPARE(source.cipher(), Aes256GcmNoPad);
    QCOMPARE(source.iv(), QByteArray::fromHex(url.section(u'#', 1).toLatin1()).chopped(32));
    QCOMPARE(source.key(), QByteArray::fromHex(EXAMPLE_KEY));
    QCOMPARE(source.httpSources().size(), 1);
    QCOMPARE(source.httpSources().constFirst().url(), QUrl(QString(EXAMPLE_URL)));
    QCOMPARE(OmemoMediaSharing::aesGcmUrl(source), url);

    QCOMPARE(parsed->thumbnail.has_value(), body.contains(u"data:"));
    if (parsed->thumbnail) {
        QCOMPARE(parsed->thumbnail->data(), QByteArray::fromBase64("/9j/4AAQSkZJRgABAQ=="));
        QCOMPARE(parsed->thumbnail->contentType().name(), u"image/jpeg"_s);
        QCOMPARE(file.metadata().thumbnails().size(), 1);
        QCOMPARE(file.metadata().thumbnails().constFirst().uri(), parsed->thumbnail->cid().toCidUrl());
    }
}

}  // namespace OmemoMediaSharing

// ============================================================

#ifdef WITH_ENCRYPTION

namespace FileEncryption {

using namespace QXmpp;
using namespace QXmpp::Private;
using namespace QXmpp::Private::Encryption;

class tst_QXmppFileEncryption : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void basic();
    Q_SLOT void deviceEncrypt();
    Q_SLOT void deviceDecrypt_data();
    Q_SLOT void deviceDecrypt();
    Q_SLOT void paddingSize();
    Q_SLOT void deviceEncryptReadAfterEnd();
    Q_SLOT void deviceDecryptFinishTwice();
    Q_SLOT void errorQueueCleared();
    Q_SLOT void gcmTag();
    Q_SLOT void gcmTagMismatch_data();
    Q_SLOT void gcmTagMismatch();
    Q_SLOT void gcmWithoutTag();
    Q_SLOT void gcmChunked();
    Q_SLOT void downloadAesGcm_data();
    Q_SLOT void downloadAesGcm();
    Q_SLOT void downloadAesGcmHash_data();
    Q_SLOT void downloadAesGcmHash();
    Q_SLOT void downloadCutOff_data();
    Q_SLOT void downloadCutOff();
    Q_SLOT void downloadCutOffLarge_data();
    Q_SLOT void downloadCutOffLarge();
    Q_SLOT void downloadAbortTooLarge();
    Q_SLOT void downloadUnencryptedNotCutOff();
    Q_SLOT void metadataSize_data();
    Q_SLOT void metadataSize();
    Q_SLOT void uploadDefaultCipher();
};

void tst_QXmppFileEncryption::basic()
{
    QByteArray data =
        "This is an example text message";
    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "data";

    auto encrypted = process(data, Aes256CbcPkcs7, Encode, key, iv);
    qDebug() << data.size() << "->" << encrypted.size();
    auto decrypted = process(encrypted, Aes256CbcPkcs7, Decode, key, iv);
    QCOMPARE(decrypted, data);
}

void tst_QXmppFileEncryption::deviceEncrypt()
{
    QByteArray data =
        "v2qtI8tx5DxM6axUAZ+xsEwrtb0VYafAPlMWqpVMG+5PBE5wbZ7MZhDUEIdFkxchOIJqt";
    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "12345678901234567890123456789012";

    auto buffer = std::make_unique<QBuffer>(&data);
    buffer->open(QIODevice::ReadOnly);

    EncryptionDevice encDev(std::move(buffer), Aes256CbcPkcs7, key, iv);

    auto encrypted = encDev.readAll();

    auto decrypted = process(encrypted, Aes256CbcPkcs7, Decode, key, iv);
    QCOMPARE(decrypted, data);
}

void tst_QXmppFileEncryption::deviceDecrypt_data()
{
    QTest::addColumn<int>("cipherId");
    QTest::addColumn<QByteArray>("key");

    QTest::newRow("aes128-gcm")
        << int(Aes128GcmNoPad)
        << QByteArray("1234567890123456");
    QTest::newRow("aes256-gcm")
        << int(Aes256GcmNoPad)
        << QByteArray("12345678901234567890123456789012");
    QTest::newRow("aes256-cbc-pkcs7")
        << int(Aes256CbcPkcs7)
        << QByteArray("12345678901234567890123456789012");
}

void tst_QXmppFileEncryption::deviceDecrypt()
{
    QFETCH(int, cipherId);
    QFETCH(QByteArray, key);
    auto cipher = Cipher(cipherId);

    QByteArray data =
        "v2qtI8tx5DxM6axUAZ+xsEwrtb0VYafAPlMWqpVMG+5PBE5wbZ7MZhDUEIdFkxchOIJqt";
    QByteArray iv = "12345678901234567890123456789012";

    // setup input io device
    auto buffer = std::make_unique<QBuffer>(&data);
    buffer->open(QIODevice::ReadOnly);

    // encrypt data
    EncryptionDevice encDevice(std::move(buffer), cipher, key, iv);
    auto encrypted = encDevice.readAll();
    QVERIFY(!encrypted.isEmpty());

    // compare with process() function
    QCOMPARE(encrypted, process(data, cipher, Encode, key, iv));

    qDebug() << "Encrypted:" << data.size() << "->" << encrypted.size();

    // decrypt data with decryption device
    QByteArray decrypted;
    buffer = std::make_unique<QBuffer>(&decrypted);
    buffer->open(QIODevice::WriteOnly);

    DecryptionDevice decDev(std::move(buffer), cipher, key, iv);
    decDev.write(encrypted);
    decDev.close();

    qDebug() << "Decrypted:" << encrypted.size() << "->" << decrypted.size();
    QCOMPARE(decrypted, process(encrypted, cipher, Decode, key, iv));
    QCOMPARE(decrypted, data);
}

void tst_QXmppFileEncryption::paddingSize()
{
    constexpr auto MAX_BYTES_TEST = 1024;

    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "12345678901234567890123456789012";

    for (int i = 1; i <= MAX_BYTES_TEST; i++) {
        QByteArray data(i, 'a');
        auto buffer = std::make_unique<QBuffer>(&data);
        buffer->open(QIODevice::ReadOnly);

        EncryptionDevice encDev(std::move(buffer), Aes256CbcPkcs7, key, iv);
        auto reportedSize = encDev.size();
        auto encryptedData = encDev.readAll();

        QCOMPARE(reportedSize, encryptedData.size());

        auto decryptedData = process(encryptedData, Aes256CbcPkcs7, Decode, key, iv);
        QCOMPARE(decryptedData, data);
    }
}

void tst_QXmppFileEncryption::deviceEncryptReadAfterEnd()
{
    QByteArray data = "This is an example text message";
    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "12345678901234567890123456789012";

    auto buffer = std::make_unique<QBuffer>(&data);
    buffer->open(QIODevice::ReadOnly);

    EncryptionDevice encDev(std::move(buffer), Aes256CbcPkcs7, key, iv);
    auto encrypted = encDev.readAll();
    QCOMPARE(encrypted, process(data, Aes256CbcPkcs7, Encode, key, iv));

    // reading again must not finalize the cipher a second time (i.e. append another padding block)
    QVERIFY(encDev.read(16).isEmpty());
}

void tst_QXmppFileEncryption::deviceDecryptFinishTwice()
{
    QByteArray data = "This is an example text message";
    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "12345678901234567890123456789012";
    auto encrypted = process(data, Aes256CbcPkcs7, Encode, key, iv);

    QByteArray decrypted;
    auto buffer = std::make_unique<QBuffer>(&decrypted);
    buffer->open(QIODevice::WriteOnly);

    // QXmppEncryptedFileSharingProvider calls finish() after the device has been closed
    DecryptionDevice decDev(std::move(buffer), Aes256CbcPkcs7, key, iv);
    decDev.write(encrypted);
    decDev.close();
    decDev.finish();

    QCOMPARE(decrypted, data);
    QCOMPARE(ERR_peek_error(), 0UL);
}

void tst_QXmppFileEncryption::errorQueueCleared()
{
    QByteArray key = "12345678901234567890123456789012";
    QByteArray iv = "12345678901234567890123456789012";

    // not a multiple of the block size
    QVERIFY(process(QByteArray(17, 'a'), Aes256CbcPkcs7, Decode, key, iv).isEmpty());

    // errors must not be left in the thread's error queue (would break QSslSocket)
    QCOMPARE(ERR_peek_error(), 0UL);
}

static QByteArray encryptWithDevice(const QByteArray &data, Cipher cipher, const QByteArray &key, const QByteArray &iv)
{
    auto input = data;
    auto buffer = std::make_unique<QBuffer>(&input);
    buffer->open(QIODevice::ReadOnly);
    EncryptionDevice device(std::move(buffer), cipher, key, iv);
    auto size = device.size();
    auto encrypted = device.readAll();
    [&] { QCOMPARE(encrypted.size(), size); }();
    return encrypted;
}

static std::optional<QByteArray> decryptWithDevice(const QByteArray &data, Cipher cipher, const QByteArray &key, const QByteArray &iv, qsizetype chunkSize)
{
    QByteArray decrypted;
    auto buffer = std::make_unique<QBuffer>(&decrypted);
    buffer->open(QIODevice::WriteOnly);
    DecryptionDevice device(std::move(buffer), cipher, key, iv);
    for (qsizetype i = 0; i < data.size(); i += chunkSize) {
        device.write(data.mid(i, chunkSize));
    }
    if (device.finish() != DecryptionDevice::Success) {
        return {};
    }
    return decrypted;
}

void tst_QXmppFileEncryption::gcmTag()
{
    // test case 14 of the GCM specification (McGrew, Viega)
    auto key = QByteArray(32, '\0');
    auto iv = QByteArray(12, '\0');
    auto data = QByteArray(16, '\0');
    auto expected = QByteArray::fromHex("cea7403d4d606b6e074ec5d3baf39d18"
                                        "d0d1c8a799996bf0265b98b5d48ab919");

    QCOMPARE(process(data, Aes256GcmNoPad, Encode, key, iv), expected);
    QCOMPARE(encryptWithDevice(data, Aes256GcmNoPad, key, iv), expected);
    QCOMPARE(process(expected, Aes256GcmNoPad, Decode, key, iv), data);
    QCOMPARE(decryptWithDevice(expected, Aes256GcmNoPad, key, iv, 1024), data);
}

void tst_QXmppFileEncryption::gcmTagMismatch_data()
{
    QTest::addColumn<int>("position");

    QTest::newRow("ciphertext") << 0;
    QTest::newRow("tag") << -1;
}

void tst_QXmppFileEncryption::gcmTagMismatch()
{
    QFETCH(int, position);

    auto key = generateKey(Aes256GcmNoPad);
    auto iv = generateInitializationVector(Aes256GcmNoPad);
    auto encrypted = encryptWithDevice("This is an example text message", Aes256GcmNoPad, key, iv);
    QVERIFY(decryptWithDevice(encrypted, Aes256GcmNoPad, key, iv, 1024));

    auto &byte = encrypted[position < 0 ? encrypted.size() + position : position];
    byte = char(byte ^ 1);
    QVERIFY(!decryptWithDevice(encrypted, Aes256GcmNoPad, key, iv, 1024));
    QVERIFY(process(encrypted, Aes256GcmNoPad, Decode, key, iv).isEmpty());

    // without (complete) tag
    QVERIFY(!decryptWithDevice(encrypted.left(10), Aes256GcmNoPad, key, iv, 1024));
    QVERIFY(!decryptWithDevice({}, Aes256GcmNoPad, key, iv, 1024));
    QCOMPARE(ERR_peek_error(), 0UL);
}

// Without a tag, all data is decrypted but reported as unauthenticated.
void tst_QXmppFileEncryption::gcmWithoutTag()
{
    const auto key = generateKey(Aes256GcmNoPad);
    const auto iv = generateInitializationVector(Aes256GcmNoPad);
    for (auto size : { 0, 1, 15, 16, 17, 1000 }) {
        const QByteArray data(size, 'a');
        const auto encrypted = encryptWithDevice(data, Aes256GcmNoPad, key, iv).chopped(16);

        QByteArray decrypted;
        auto buffer = std::make_unique<QBuffer>(&decrypted);
        buffer->open(QIODevice::WriteOnly);
        DecryptionDevice device(std::move(buffer), Aes256GcmNoPad, key, iv);
        device.write(encrypted);
        device.close();
        QCOMPARE(device.finish(), DecryptionDevice::Unauthenticated);
        QCOMPARE(decrypted, data);
    }
    QCOMPARE(ERR_peek_error(), 0UL);
}

void tst_QXmppFileEncryption::gcmChunked()
{
    QByteArray data;
    for (int i = 0; i < 1000; i++) {
        data.append(char(i));
    }

    // encryption, read like an upload
    for (auto size : { 0, 1, 15, 16, 17, 1000 }) {
        const auto key = generateKey(Aes256GcmNoPad);
        const auto iv = generateInitializationVector(Aes256GcmNoPad);
        auto input = data.left(size);
        const auto expected = encryptWithDevice(input, Aes256GcmNoPad, key, iv);
        QCOMPARE(expected, process(input, Aes256GcmNoPad, Encode, key, iv));
        for (auto chunkSize : { 1, 7, 16, 17, 1000 }) {
            auto buffer = std::make_unique<QBuffer>(&input);
            buffer->open(QIODevice::ReadOnly);
            EncryptionDevice device(std::move(buffer), Aes256GcmNoPad, key, iv);
            QByteArray encrypted;
            for (auto chunk = device.read(chunkSize); !chunk.isEmpty(); chunk = device.read(chunkSize)) {
                encrypted += chunk;
            }
            QCOMPARE(encrypted, expected);
        }
        QCOMPARE(decryptWithDevice(expected, Aes256GcmNoPad, key, iv, 7), data.left(size));
    }

    // legacy IV size of OMEMO Media Sharing
    for (auto ivSize : { 12, 16 }) {
        auto key = generateKey(Aes256GcmNoPad);
        auto iv = QByteArray(ivSize, 'i');
        auto encrypted = encryptWithDevice(data, Aes256GcmNoPad, key, iv);
        QCOMPARE(encrypted.size(), data.size() + 16);
        for (auto chunkSize : { 1, 7, 16, 17, 1000, 1016 }) {
            QCOMPARE(decryptWithDevice(encrypted, Aes256GcmNoPad, key, iv, chunkSize), data);
        }
    }
}

// serves fixed data for all HTTP sources
class FakeHttpProvider : public QXmppFileSharingProvider
{
public:
    using SourceType = QXmppHttpFileSource;

    explicit FakeHttpProvider(QByteArray data) : m_data(std::move(data)) { }

    auto downloadFile(const std::any &, std::unique_ptr<QIODevice> target, std::function<void(quint64, quint64)>, std::function<void(DownloadResult)> reportFinished) -> std::shared_ptr<Download> override
    {
        // in chunks like a network reply
        constexpr qsizetype CHUNK_SIZE = 10'000;
        for (qsizetype i = 0; i < m_data.size(); i += CHUNK_SIZE) {
            const auto chunk = m_data.mid(i, CHUNK_SIZE);
            if (target->write(chunk) != chunk.size()) {
                auto error = QXmppError::fromIoDevice(*target);
                target->close();
                reportFinished(std::move(error));
                return {};
            }
        }
        target->close();
        reportFinished(Success {});
        return {};
    }
    auto uploadFile(std::unique_ptr<QIODevice> source, const QXmppFileMetadata &, std::function<void(quint64, quint64)>, std::function<void(UploadResult)> reportFinished) -> std::shared_ptr<Upload> override
    {
        m_data = source->readAll();
        reportFinished(std::any(QXmppHttpFileSource(QUrl(u"https://upload.example.org/file"_s))));
        return {};
    }
    const QByteArray &data() const { return m_data; }

private:
    QByteArray m_data;
};

void tst_QXmppFileEncryption::downloadAesGcm_data()
{
    QTest::addColumn<bool>("modified");

    QTest::newRow("valid") << false;
    QTest::newRow("modified") << true;
}

// A file shared via OMEMO Media Sharing is downloaded and its authentication tag checked.
void tst_QXmppFileEncryption::downloadAesGcm()
{
    QFETCH(bool, modified);

    const QByteArray data(1000, 'a');
    const auto key = generateKey(Aes256GcmNoPad);
    const auto iv = generateInitializationVector(Aes256GcmNoPad);
    auto encrypted = process(data, Aes256GcmNoPad, Encode, key, iv);
    if (modified) {
        encrypted[500] = char(encrypted[500] ^ 1);
    }

    QString body = u"aesgcm://upload.example.org/file.txt#"_s + QString::fromLatin1(QByteArray(iv + key).toHex());
    auto parsed = QXmpp::Private::OmemoMediaSharing::parseBody(body);
    QVERIFY(parsed);

    QXmppFileSharingManager manager;
    auto httpProvider = std::make_shared<FakeHttpProvider>(encrypted);
    manager.registerProvider(httpProvider);
    manager.registerProvider(std::make_shared<QXmppEncryptedFileSharingProvider>(&manager, httpProvider));

    QByteArray output;
    auto buffer = std::make_unique<QBuffer>(&output);
    buffer->open(QIODevice::WriteOnly);
    auto download = manager.downloadFile(parsed->fileShare, std::move(buffer));
    QVERIFY(download->isFinished());

    if (modified) {
        QVERIFY(std::holds_alternative<QXmppError>(download->result()));
    } else {
        QVERIFY(std::holds_alternative<QXmppFileDownload::Downloaded>(download->result()));
        QCOMPARE(output, data);
    }
}

void tst_QXmppFileEncryption::downloadAesGcmHash_data()
{
    QTest::addColumn<bool>("tag");
    QTest::addColumn<bool>("modified");
    QTest::addColumn<std::optional<HashAlgorithm>>("hashAlgorithm");
    QTest::addColumn<bool>("success");

    const std::optional<HashAlgorithm> noHash;
    const std::optional strong(HashAlgorithm::Sha256);
    const std::optional weak(HashAlgorithm::Md5);
    QTest::newRow("tag") << true << false << noHash << true;
    QTest::newRow("tag+hash") << true << false << strong << true;
    QTest::newRow("no-tag") << false << false << noHash << false;
    QTest::newRow("no-tag+hash") << false << false << strong << true;
    QTest::newRow("no-tag+weak-hash") << false << false << weak << false;
    QTest::newRow("no-tag+hash+modified") << false << true << strong << false;
    QTest::newRow("tag+hash+modified") << true << true << strong << false;
}

// The authentication tag is not needed if there is a strong hash (XEP-0448).
void tst_QXmppFileEncryption::downloadAesGcmHash()
{
    QFETCH(bool, tag);
    QFETCH(bool, modified);
    QFETCH(std::optional<HashAlgorithm>, hashAlgorithm);
    QFETCH(bool, success);

    const QByteArray data(1000, 'a');
    QXmppEncryptedFileSource source;
    source.setCipher(Aes256GcmNoPad);
    source.setKey(generateKey(Aes256GcmNoPad));
    source.setIv(generateInitializationVector(Aes256GcmNoPad));
    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"https://upload.example.org/file"_s)) });

    QXmppFileMetadata metadata;
    if (hashAlgorithm) {
        QXmppHash hash;
        hash.setAlgorithm(*hashAlgorithm);
        hash.setHash(QCryptographicHash::hash(data, *hashAlgorithm == HashAlgorithm::Md5 ? QCryptographicHash::Md5 : QCryptographicHash::Sha256));
        metadata.setHashes({ hash });
    }
    QXmppFileShare file;
    file.setMetadata(metadata);
    file.setEncryptedSourecs({ source });

    auto encrypted = process(data, Aes256GcmNoPad, Encode, source.key(), source.iv());
    if (!tag) {
        encrypted.chop(16);
    }
    if (modified) {
        encrypted[500] = char(encrypted[500] ^ 1);
    }

    QXmppFileSharingManager manager;
    auto httpProvider = std::make_shared<FakeHttpProvider>(encrypted);
    manager.registerProvider(httpProvider);
    manager.registerProvider(std::make_shared<QXmppEncryptedFileSharingProvider>(&manager, httpProvider));

    QTemporaryDir dir;
    const auto path = dir.filePath(u"file"_s);
    auto output = std::make_unique<QFile>(path);
    QVERIFY(output->open(QIODevice::WriteOnly));
    auto download = manager.downloadFile(file, std::move(output));
    QTRY_VERIFY(download->isFinished());

    QCOMPARE(std::holds_alternative<QXmppFileDownload::Downloaded>(download->result()), success);
    if (success) {
        QFile result(path);
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), data);
    }
}

void tst_QXmppFileEncryption::downloadCutOff_data()
{
    QTest::addColumn<int>("cipherId");
    QTest::addColumn<bool>("tag");
    QTest::addColumn<int>("padding");
    QTest::addColumn<std::optional<uint64_t>>("size");
    QTest::addColumn<bool>("success");
    QTest::addColumn<int>("outputSize");

    const std::optional<uint64_t> noSize;
    const std::optional<uint64_t> size(1000);
    QTest::newRow("gcm") << int(Aes256GcmNoPad) << true << 0 << size << true << 1000;
    QTest::newRow("gcm-padding") << int(Aes256GcmNoPad) << true << 300 << size << true << 1000;
    QTest::newRow("gcm-no-tag") << int(Aes256GcmNoPad) << false << 0 << size << true << 1000;
    QTest::newRow("gcm-no-tag-padding") << int(Aes256GcmNoPad) << false << 300 << size << true << 1000;
    // padding smaller than the tag
    QTest::newRow("gcm-no-tag-small-padding") << int(Aes256GcmNoPad) << false << 7 << size << true << 1000;
    QTest::newRow("cbc") << int(Aes256CbcPkcs7) << true << 0 << size << true << 1000;
    QTest::newRow("cbc-padding") << int(Aes256CbcPkcs7) << true << 300 << size << true << 1000;
    // up to 1 MiB is tolerated for small files
    QTest::newRow("gcm-max-padding") << int(Aes256GcmNoPad) << true << 1024 * 1024 << size << true << 1000;
    QTest::newRow("gcm-too-large") << int(Aes256GcmNoPad) << true << 1024 * 1024 + 1 << size << false << 1000;
    QTest::newRow("cbc-too-large") << int(Aes256CbcPkcs7) << true << 2 * 1024 * 1024 << size << false << 1000;
    // the size is unknown: the padding is kept and the hash does not match
    QTest::newRow("gcm-no-size") << int(Aes256GcmNoPad) << true << 0 << noSize << true << 1000;
    QTest::newRow("gcm-padding-no-size") << int(Aes256GcmNoPad) << true << 300 << noSize << false << 1300;
    // wrong size: the hash does not match
    QTest::newRow("gcm-size-too-small") << int(Aes256GcmNoPad) << true << 0 << std::optional<uint64_t>(999) << false << 999;
    QTest::newRow("gcm-size-too-large") << int(Aes256GcmNoPad) << true << 0 << std::optional<uint64_t>(2000) << true << 1000;
}

// Additional bytes of decrypted files are cut off (XEP-0448).
void tst_QXmppFileEncryption::downloadCutOff()
{
    QFETCH(int, cipherId);
    QFETCH(bool, tag);
    QFETCH(int, padding);
    QFETCH(std::optional<uint64_t>, size);
    QFETCH(bool, success);
    QFETCH(int, outputSize);
    const auto cipher = Cipher(cipherId);

    const QByteArray data(1000, 'a');
    QXmppEncryptedFileSource source;
    source.setCipher(cipher);
    source.setKey(generateKey(cipher));
    source.setIv(generateInitializationVector(cipher));
    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"https://upload.example.org/file"_s)) });

    QXmppHash hash;
    hash.setAlgorithm(HashAlgorithm::Sha256);
    hash.setHash(QCryptographicHash::hash(data, QCryptographicHash::Sha256));
    QXmppFileMetadata metadata;
    metadata.setSize(size);
    metadata.setHashes({ hash });
    QXmppFileShare file;
    file.setMetadata(metadata);
    file.setEncryptedSourecs({ source });

    auto encrypted = process(data + QByteArray(padding, '\0'), cipher, Encode, source.key(), source.iv());
    if (!tag) {
        encrypted.chop(16);
    }

    QXmppFileSharingManager manager;
    auto httpProvider = std::make_shared<FakeHttpProvider>(encrypted);
    manager.registerProvider(httpProvider);
    manager.registerProvider(std::make_shared<QXmppEncryptedFileSharingProvider>(&manager, httpProvider));

    QTemporaryDir dir;
    const auto path = dir.filePath(u"file"_s);
    auto output = std::make_unique<QFile>(path);
    QVERIFY(output->open(QIODevice::WriteOnly));
    auto download = manager.downloadFile(file, std::move(output));
    QTRY_VERIFY(download->isFinished());

    QCOMPARE(std::holds_alternative<QXmppFileDownload::Downloaded>(download->result()), success);
    QCOMPARE(QFileInfo(path).size(), outputSize);
}

// For large files, the tolerance is 10 % of their size.
void tst_QXmppFileEncryption::downloadCutOffLarge_data()
{
    QTest::addColumn<int>("padding");
    QTest::addColumn<bool>("success");

    QTest::newRow("max-padding") << 2 * 1024 * 1024 << true;
    QTest::newRow("too-large") << 2 * 1024 * 1024 + 1 << false;
}

void tst_QXmppFileEncryption::downloadCutOffLarge()
{
    QFETCH(int, padding);
    QFETCH(bool, success);

    const QByteArray data(20 * 1024 * 1024, 'a');
    QXmppEncryptedFileSource source;
    source.setCipher(Aes256GcmNoPad);
    source.setKey(generateKey(Aes256GcmNoPad));
    source.setIv(generateInitializationVector(Aes256GcmNoPad));
    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"https://upload.example.org/file"_s)) });
    QXmppFileMetadata metadata;
    metadata.setSize(data.size());
    QXmppFileShare file;
    file.setMetadata(metadata);
    file.setEncryptedSourecs({ source });

    QXmppFileSharingManager manager;
    auto httpProvider = std::make_shared<FakeHttpProvider>(process(data + QByteArray(padding, '\0'), Aes256GcmNoPad, Encode, source.key(), source.iv()));
    manager.registerProvider(httpProvider);
    manager.registerProvider(std::make_shared<QXmppEncryptedFileSharingProvider>(&manager, httpProvider));

    QByteArray output;
    auto buffer = std::make_unique<QBuffer>(&output);
    buffer->open(QIODevice::WriteOnly);
    auto download = manager.downloadFile(file, std::move(buffer));
    QVERIFY(download->isFinished());
    QCOMPARE(std::holds_alternative<QXmppFileDownload::Downloaded>(download->result()), success);
    if (success) {
        QCOMPARE(output, data);
    }
}

// The HTTP download is aborted if the file is much larger than its size.
void tst_QXmppFileEncryption::downloadAbortTooLarge()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    bool disconnected = false;
    qint64 sent = 0;
    connect(&server, &QTcpServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, &server, [&] { disconnected = true; });
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            socket->readAll();
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n");
        });
        // endless response
        connect(socket, &QTcpSocket::bytesWritten, socket, [&, socket] {
            if (socket->bytesToWrite() == 0) {
                sent += socket->write(QByteArray(65536, 'x'));
            }
        });
    });

    QXmppEncryptedFileSource source;
    source.setCipher(Aes256GcmNoPad);
    source.setKey(generateKey(Aes256GcmNoPad));
    source.setIv(generateInitializationVector(Aes256GcmNoPad));
    source.setHttpSources({ QXmppHttpFileSource(QUrl(u"http://127.0.0.1:%1/file"_s.arg(server.serverPort()))) });
    QXmppFileMetadata metadata;
    metadata.setSize(1000);
    QXmppFileShare file;
    file.setMetadata(metadata);
    file.setEncryptedSourecs({ source });

    QNetworkAccessManager network;
    QXmppFileSharingManager manager;
    QXmppHttpUploadManager uploadManager(&network);
    auto httpProvider = std::make_shared<QXmppHttpFileSharingProvider>(&uploadManager, &network);
    manager.registerProvider(httpProvider);
    manager.registerProvider(std::make_shared<QXmppEncryptedFileSharingProvider>(&manager, httpProvider));

    QByteArray output;
    auto buffer = std::make_unique<QBuffer>(&output);
    buffer->open(QIODevice::WriteOnly);
    auto download = manager.downloadFile(file, std::move(buffer));
    QTRY_VERIFY(download->isFinished());
    const auto result = download->result();
    QVERIFY(std::holds_alternative<QXmppError>(result));
    QCOMPARE(std::get<QXmppError>(result).description, u"The file is much larger than its specified size."_s);
    QTRY_VERIFY(disconnected);
    QVERIFY(sent < 20 * 1024 * 1024);
}

void tst_QXmppFileEncryption::downloadUnencryptedNotCutOff()
{
    const QByteArray data(1000, 'a');
    QXmppFileMetadata metadata;
    metadata.setSize(900);
    QXmppFileShare file;
    file.setMetadata(metadata);
    file.setHttpSources({ QXmppHttpFileSource(QUrl(u"https://upload.example.org/file"_s)) });

    QXmppFileSharingManager manager;
    manager.registerProvider(std::make_shared<FakeHttpProvider>(data));

    QByteArray output;
    auto buffer = std::make_unique<QBuffer>(&output);
    buffer->open(QIODevice::WriteOnly);
    auto download = manager.downloadFile(file, std::move(buffer));
    QVERIFY(download->isFinished());
    QCOMPARE(output, data);
}

void tst_QXmppFileEncryption::metadataSize_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<std::optional<uint64_t>>("size");

    QTest::newRow("zero") << u"0"_s << std::optional<uint64_t>(0);
    QTest::newRow("large") << u"5000000000"_s << std::optional<uint64_t>(5000000000);
    QTest::newRow("invalid") << u"abc"_s << std::optional<uint64_t>();
    QTest::newRow("negative") << u"-1"_s << std::optional<uint64_t>();
    QTest::newRow("empty") << QString() << std::optional<uint64_t>();
}

// An invalid size must not be parsed as 0, the file would be cut off completely.
void tst_QXmppFileEncryption::metadataSize()
{
    QFETCH(QString, text);
    QFETCH(std::optional<uint64_t>, size);

    QXmppFileMetadata metadata;
    QVERIFY(metadata.parse(xmlToDom(u"<file xmlns='urn:xmpp:file:metadata:0'><size>"_s + text + u"</size></file>"_s)));
    QCOMPARE(metadata.size(), size);
}

// By default, uploaded files can be shared via OMEMO Media Sharing.
void tst_QXmppFileEncryption::uploadDefaultCipher()
{
    QXmppFileSharingManager manager;
    auto httpProvider = std::make_shared<FakeHttpProvider>(QByteArray());
    QXmppEncryptedFileSharingProvider provider(&manager, httpProvider);

    QByteArray data(1000, 'a');
    auto buffer = std::make_unique<QBuffer>(&data);
    buffer->open(QIODevice::ReadOnly);
    std::optional<QXmppFileSharingProvider::UploadResult> result;
    provider.uploadFile(std::move(buffer), {}, {}, [&](auto r) { result = std::move(r); });

    QVERIFY(result && std::holds_alternative<std::any>(*result));
    const auto source = std::any_cast<QXmppEncryptedFileSource>(std::get<std::any>(*result));
    QCOMPARE(source.cipher(), Aes256GcmNoPad);
    QVERIFY(QXmpp::Private::OmemoMediaSharing::aesGcmUrl(source));
    QCOMPARE(process(httpProvider->data(), Aes256GcmNoPad, Decode, source.key(), source.iv()), data);
}

}  // namespace FileEncryption

#endif  // WITH_ENCRYPTION

// ============================================================

namespace HttpUploadIq {

class tst_QXmppHttpUploadIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void httpUploadRequest();
    Q_SLOT void httpUploadIsRequest_data();
    Q_SLOT void httpUploadIsRequest();
    Q_SLOT void httpUploadSlot();
    Q_SLOT void httpUploadIsSlot_data();
    Q_SLOT void httpUploadIsSlot();
};

void tst_QXmppHttpUploadIq::httpUploadRequest()
{
    const QByteArray xml(
        "<iq id=\"step_03\" "
        "to=\"upload.montague.tld\" "
        "from=\"romeo@montague.tld/garden\" "
        "type=\"get\">"
        "<request xmlns=\"urn:xmpp:http:upload:0\" "
        "filename=\"très cool.jpg\" "
        "size=\"23456\" "
        "content-type=\"image/jpeg\"/>"
        "</iq>");

    QXmppHttpUploadRequestIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.fileName(), u"très cool.jpg"_s);
    QCOMPARE(iq.size(), 23456);
    QCOMPARE(iq.contentType().name(), u"image/jpeg"_s);
    serializePacket(iq, xml);

    // test setters
    iq.setFileName("icon.png");
    QCOMPARE(iq.fileName(), u"icon.png"_s);
    iq.setSize(23421337);
    QCOMPARE(iq.size(), 23421337);
    iq.setContentType(QMimeDatabase().mimeTypeForName("image/png"));
    QCOMPARE(iq.contentType().name(), u"image/png"_s);
}

void tst_QXmppHttpUploadIq::httpUploadIsRequest_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isRequest");

    QTest::newRow("empty-iq")
        << QByteArray("<iq/>")
        << false;
    QTest::newRow("wrong-ns")
        << QByteArray("<iq><request xmlns=\"some:other:request\"/></iq>")
        << false;
    QTest::newRow("correct")
        << QByteArray("<iq><request xmlns=\"urn:xmpp:http:upload:0\"/></iq>")
        << true;
}

void tst_QXmppHttpUploadIq::httpUploadIsRequest()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isRequest);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppHttpUploadRequestIq::isHttpUploadRequestIq(xmlToDom(xml)), isRequest);
    QT_WARNING_POP
}

void tst_QXmppHttpUploadIq::httpUploadSlot()
{
    const QByteArray xml(
        "<iq id=\"step_03\" "
        "to=\"romeo@montague.tld/garden\" "
        "from=\"upload.montague.tld\" "
        "type=\"result\">"
        "<slot xmlns=\"urn:xmpp:http:upload:0\">"
        "<put url=\"https://upload.montague.tld/4a771ac1-f0b2-4a4a-970"
        "0-f2a26fa2bb67/tr%C3%A8s%20cool.jpg\">"
        "<header name=\"Authorization\">Basic Base64String==</header>"
        "<header name=\"Cookie\">foo=bar; user=romeo</header>"
        "</put>"
        "<get url=\"https://download.montague.tld/4a771ac1-f0b2-4a4a-9"
        "700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg\"/>"
        "</slot>"
        "</iq>");

    QXmppHttpUploadSlotIq iq;
    parsePacket(iq, xml);
    QCOMPARE(iq.putUrl(), QUrl("https://upload.montague.tld/4a771ac1-f0b2-4a4a"
                               "-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    QCOMPARE(iq.getUrl(), QUrl("https://download.montague.tld/4a771ac1-f0b2-4a"
                               "4a-9700-f2a26fa2bb67/tr%C3%A8s%20cool.jpg"));
    QMap<QString, QString> headers;
    headers["Authorization"] = "Basic Base64String==";
    headers["Cookie"] = "foo=bar; user=romeo";
    QCOMPARE(iq.putHeaders(), headers);
    serializePacket(iq, xml);

    // test setters
    iq.setGetUrl(QUrl("https://dl.example.org/user/file"));
    QCOMPARE(iq.getUrl(), QUrl("https://dl.example.org/user/file"));
    iq.setPutUrl(QUrl("https://ul.example.org/user/file"));
    QCOMPARE(iq.putUrl(), QUrl("https://ul.example.org/user/file"));
    QMap<QString, QString> emptyMap;
    iq.setPutHeaders(emptyMap);
    QCOMPARE(iq.putHeaders(), emptyMap);
}

void tst_QXmppHttpUploadIq::httpUploadIsSlot_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<bool>("isSlot");

    QTest::newRow("empty-iq")
        << QByteArray("<iq/>")
        << false;
    QTest::newRow("wrong-ns")
        << QByteArray("<iq><slot xmlns=\"some:other:slot\"/></iq>")
        << false;
    QTest::newRow("correct")
        << QByteArray("<iq><slot xmlns=\"urn:xmpp:http:upload:0\"/></iq>")
        << true;
}

void tst_QXmppHttpUploadIq::httpUploadIsSlot()
{
    QFETCH(QByteArray, xml);
    QFETCH(bool, isSlot);

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QCOMPARE(QXmppHttpUploadSlotIq::isHttpUploadSlotIq(xmlToDom(xml)), isSlot);
    QT_WARNING_POP
}

}  // namespace HttpUploadIq

// ============================================================

namespace StreamInitiationIq {

class tst_QXmppStreamInitiationIq : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void streamInitiationFileInfo_data();
    Q_SLOT void streamInitiationFileInfo();
    Q_SLOT void streamInitiationOffer();
    Q_SLOT void streamInitiationResult();
};

void tst_QXmppStreamInitiationIq::streamInitiationFileInfo_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QDateTime>("date");
    QTest::addColumn<QString>("description");
    QTest::addColumn<QByteArray>("hash");
    QTest::addColumn<QString>("name");
    QTest::addColumn<qint64>("size");

    QTest::newRow("normal")
        << QByteArray("<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" name=\"test.txt\" size=\"1022\"/>")
        << QDateTime().toUTC()
        << QString()
        << QByteArray()
        << u"test.txt"_s
        << qint64(1022);

    QTest::newRow("full")
        << QByteArray("<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" "
                      "date=\"1969-07-21T02:56:15Z\" "
                      "hash=\"552da749930852c69ae5d2141d3766b1\" "
                      "name=\"test.txt\" "
                      "size=\"1022\">"
                      "<desc>This is a test. If this were a real file...</desc>"
                      "</file>")
        << QDateTime(QDate(1969, 7, 21), QTime(2, 56, 15), TimeZoneUTC)
        << u"This is a test. If this were a real file..."_s
        << QByteArray::fromHex("552da749930852c69ae5d2141d3766b1")
        << u"test.txt"_s
        << qint64(1022);
}

void tst_QXmppStreamInitiationIq::streamInitiationFileInfo()
{
    QFETCH(QByteArray, xml);
    QFETCH(QDateTime, date);
    QFETCH(QString, description);
    QFETCH(QByteArray, hash);
    QFETCH(QString, name);
    QFETCH(qint64, size);

    QXmppTransferFileInfo info;
    parsePacket(info, xml);
    QCOMPARE(info.date(), date);
    QCOMPARE(info.description(), description);
    QCOMPARE(info.hash(), hash);
    QCOMPARE(info.name(), name);
    QCOMPARE(info.size(), size);
    serializePacket(info, xml);
}

void tst_QXmppStreamInitiationIq::streamInitiationOffer()
{
    QByteArray xml(
        "<iq id=\"offer1\" to=\"receiver@jabber.org/resource\" type=\"set\">"
        "<si xmlns=\"http://jabber.org/protocol/si\" id=\"a0\" mime-type=\"text/plain\" profile=\"http://jabber.org/protocol/si/profile/file-transfer\">"
        "<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" name=\"test.txt\" size=\"1022\"/>"
        "<feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
        "<x xmlns=\"jabber:x:data\" type=\"form\">"
        "<field type=\"list-single\" var=\"stream-method\">"
        "<option><value>http://jabber.org/protocol/bytestreams</value></option>"
        "<option><value>http://jabber.org/protocol/ibb</value></option>"
        "</field>"
        "</x>"
        "</feature>"
        "</si>"
        "</iq>");

    QXmppStreamInitiationIq iq;
    parsePacket(iq, xml);
    QVERIFY(!iq.featureForm().isNull());
    QVERIFY(!iq.fileInfo().isNull());
    QCOMPARE(iq.fileInfo().name(), u"test.txt"_s);
    QCOMPARE(iq.fileInfo().size(), qint64(1022));
    serializePacket(iq, xml);
}

void tst_QXmppStreamInitiationIq::streamInitiationResult()
{
    QByteArray xml(
        "<iq id=\"offer1\" to=\"sender@jabber.org/resource\" type=\"result\">"
        "<si xmlns=\"http://jabber.org/protocol/si\">"
        "<feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
        "<x xmlns=\"jabber:x:data\" type=\"submit\">"
        "<field type=\"list-single\" var=\"stream-method\">"
        "<value>http://jabber.org/protocol/bytestreams</value>"
        "</field>"
        "</x>"
        "</feature>"
        "</si>"
        "</iq>");

    QXmppStreamInitiationIq iq;
    parsePacket(iq, xml);
    QVERIFY(iq.fileInfo().isNull());
    serializePacket(iq, xml);
}

}  // namespace StreamInitiationIq

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = runTests<HttpUpload::tst_QXmppHttpUploadManager, Transfer::tst_QXmppTransferManager, HttpUploadIq::tst_QXmppHttpUploadIq, StreamInitiationIq::tst_QXmppStreamInitiationIq, OmemoMediaSharing::tst_OmemoMediaSharing>(argc, argv);
#ifdef WITH_ENCRYPTION
    status |= runTests<FileEncryption::tst_QXmppFileEncryption>(argc, argv);
#endif
    return status;
}

#include "FileSharing.moc"

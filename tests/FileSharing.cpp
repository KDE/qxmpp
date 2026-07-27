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

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppHttpUploadIq.h"
#include "QXmppHttpUploadManager.h"
#include "QXmppServer.h"
#include "QXmppTransferManager.h"
#include "QXmppUploadRequestManager.h"

#ifdef WITH_ENCRYPTION
#include "QXmppFileEncryption.h"
#endif

#include "QXmppStreamInitiationIq_p.h"

#include "Algorithms.h"
#include "IntegrationTesting.h"
#include "TestClient.h"
#include "TestPasswordChecker.h"
#include "util.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QMimeDatabase>
#include <QObject>
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
    QVERIFY(discovery->handleStanza(xmlToDom(xml)));
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
};

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

    bool realAccepted = manager->handleStanza(xmlToDom(xml));

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

    bool accepted = discovery->handleStanza(xmlToDom(xml));
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
    auto *uploadManager = test.addNewExtension<QXmppHttpUploadManager>();

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
    finishedSpy.wait();
    QCOMPARE(finishedSpy.size(), 1);
    QVERIFY(!progressSpy.empty());

    // test result
    auto result = upload->result().value();
    if (std::holds_alternative<QXmppError>(result)) {
        qDebug() << "Upload failed:" << std::get<QXmppError>(result).description;
        QVERIFY2(false, "Uploading the file failed");
    }

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
    int status = runTests<HttpUpload::tst_QXmppHttpUploadManager, Transfer::tst_QXmppTransferManager, HttpUploadIq::tst_QXmppHttpUploadIq, StreamInitiationIq::tst_QXmppStreamInitiationIq>(argc, argv);
#ifdef WITH_ENCRYPTION
    status |= runTests<FileEncryption::tst_QXmppFileEncryption>(argc, argv);
#endif
    return status;
}

#include "FileSharing.moc"

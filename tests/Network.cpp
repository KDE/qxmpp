// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

// Combined test binary for the low-level networking tests. Merging
// tst_QXmppServer, tst_QXmppStunMessage and tst_QXmppSocks into one
// translation unit parses the shared Qt/QXmpp headers once instead of once per
// file. Each original test keeps its own namespace; main() runs them in turn.

#include "QXmppClient.h"
#include "QXmppServer.h"
#include "QXmppSocks.h"
#include "QXmppStun.h"

#include "TestPasswordChecker.h"
#include "util.h"

#include <QCoreApplication>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace Server {

class tst_QXmppServer : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testConnect_data();
    Q_SLOT void testConnect();
};

void tst_QXmppServer::testConnect_data()
{
    QTest::addColumn<QString>("username");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("mechanism");
    QTest::addColumn<bool>("connected");

    QTest::newRow("plain-good") << "testuser"
                                << "testpwd"
                                << "PLAIN" << true;
    QTest::newRow("plain-bad-username") << "baduser"
                                        << "testpwd"
                                        << "PLAIN" << false;
    QTest::newRow("plain-bad-password") << "testuser"
                                        << "badpwd"
                                        << "PLAIN" << false;

    QTest::newRow("digest-good") << "testuser"
                                 << "testpwd"
                                 << "DIGEST-MD5" << true;
    QTest::newRow("digest-bad-username") << "baduser"
                                         << "testpwd"
                                         << "DIGEST-MD5" << false;
    QTest::newRow("digest-bad-password") << "testuser"
                                         << "badpwd"
                                         << "DIGEST-MD5" << false;
}

void tst_QXmppServer::testConnect()
{
    QFETCH(QString, username);
    QFETCH(QString, password);
    QFETCH(QString, mechanism);
    QFETCH(bool, connected);

    const QString testDomain("localhost");
    const QHostAddress testHost(QHostAddress::LocalHost);
    const quint16 testPort = 12001;

    QXmppLogger logger;
    // logger.setLoggingType(QXmppLogger::StdoutLogging);

    // prepare server
    TestPasswordChecker passwordChecker;
    passwordChecker.addCredentials("testuser", "testpwd");

    QXmppServer server;
    server.setDomain(testDomain);
    server.setLogger(&logger);
    server.setPasswordChecker(&passwordChecker);
    server.listenForClients(testHost, testPort);

    // prepare client
    QXmppClient client;
    client.setLogger(&logger);

    QEventLoop loop;
    connect(&client, &QXmppClient::connected, &loop, &QEventLoop::quit);
    connect(&client, &QXmppClient::disconnected, &loop, &QEventLoop::quit);

    QXmppConfiguration config;
    config.setDomain(testDomain);
    config.setHost(testHost.toString());
    config.setPort(testPort);
    config.setUser(username);
    config.setPassword(password);
    config.setSaslAuthMechanism(mechanism);
    config.setDisabledSaslMechanisms({});
    client.connectToServer(config);
    loop.exec();
    QCOMPARE(client.isConnected(), connected);
}

}  // namespace Server

// ============================================================

namespace StunMessage {

class tst_QXmppStunMessage : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testFingerprint();
    Q_SLOT void testIntegrity();
    Q_SLOT void testIPv4Address();
    Q_SLOT void testIPv6Address();
    Q_SLOT void testXorIPv4Address();
    Q_SLOT void testXorIPv6Address();
};

void tst_QXmppStunMessage::testFingerprint()
{
    // without fingerprint
    QXmppStunMessage msg;
    msg.setType(0x0001);
    QCOMPARE(msg.encode(QByteArray(), false),
             QByteArray("\x00\x01\x00\x00\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 20));

    // with fingerprint
    QCOMPARE(msg.encode(QByteArray(), true),
             QByteArray("\x00\x01\x00\x08\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x28\x00\x04\xB2\xAA\xF9\xF6", 28));
}

void tst_QXmppStunMessage::testIntegrity()
{
    QXmppStunMessage msg;
    msg.setType(0x0001);
    QCOMPARE(msg.encode(QByteArray("somesecret"), false),
             QByteArray("\x00\x01\x00\x18\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x14\x96\x4B\x40\xD1\x84\x67\x6A\xFD\xB5\xE0\x7C\xC5\x1F\xFB\xBD\xA2\x61\xAF\xB1\x26", 44));
}

void tst_QXmppStunMessage::testIPv4Address()
{
    // encode
    QXmppStunMessage msg;
    msg.setType(0x0001);
    msg.mappedHost = QHostAddress("127.0.0.1");
    msg.mappedPort = 12345;
    QByteArray packet = msg.encode(QByteArray(), false);
    QCOMPARE(packet,
             QByteArray("\x00\x01\x00\x0C\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x08\x00\x01\x30\x39\x7F\x00\x00\x01", 32));

    // decode
    QXmppStunMessage msg2;
    msg2.decode(packet);
    QCOMPARE(msg2.mappedHost, QHostAddress("127.0.0.1"));
    QCOMPARE(msg2.mappedPort, quint16(12345));
}

void tst_QXmppStunMessage::testIPv6Address()
{
    // encode
    QXmppStunMessage msg;
    msg.setType(0x0001);
    msg.mappedHost = QHostAddress("::1");
    msg.mappedPort = 12345;
    const QByteArray packet = msg.encode(QByteArray(), false);
    QCOMPARE(packet,
             QByteArray("\x00\x01\x00\x18\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x14\x00\x02\x30\x39\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 44));

    // decode
    QXmppStunMessage msg2;
    msg2.decode(packet);
    QCOMPARE(msg2.mappedHost, QHostAddress("::1"));
    QCOMPARE(msg2.mappedPort, quint16(12345));
}

void tst_QXmppStunMessage::testXorIPv4Address()
{
    // encode
    QXmppStunMessage msg;
    msg.setType(0x0001);
    msg.xorMappedHost = QHostAddress("127.0.0.1");
    msg.xorMappedPort = 12345;
    QByteArray packet = msg.encode(QByteArray(), false);
    QCOMPARE(packet,
             QByteArray("\x00\x01\x00\x0C\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x08\x00\x01\x11\x2B\x5E\x12\xA4\x43", 32));

    // decode
    QXmppStunMessage msg2;
    msg2.decode(packet);
    QCOMPARE(msg2.xorMappedHost, QHostAddress("127.0.0.1"));
    QCOMPARE(msg2.xorMappedPort, quint16(12345));
}

void tst_QXmppStunMessage::testXorIPv6Address()
{
    // encode
    QXmppStunMessage msg;
    msg.setType(0x0001);
    msg.xorMappedHost = QHostAddress("::1");
    msg.xorMappedPort = 12345;
    const QByteArray packet = msg.encode(QByteArray(), false);
    QCOMPARE(packet,
             QByteArray("\x00\x01\x00\x18\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x14\x00\x02\x11\x2B\x21\x12\xA4\x42\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 44));

    // decode
    QXmppStunMessage msg2;
    msg2.decode(packet);
    QCOMPARE(msg2.xorMappedHost, QHostAddress("::1"));
    QCOMPARE(msg2.xorMappedPort, quint16(12345));
}

}  // namespace StunMessage

// ============================================================

namespace Socks {

class tst_QXmppSocks : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void init();
    Q_SLOT void newConnectionSlot(QTcpSocket *socket, QString hostName, quint16 port);

    Q_SLOT void testClient_data();
    Q_SLOT void testClient();
    Q_SLOT void testClientAndServer();
    Q_SLOT void testServer_data();
    Q_SLOT void testServer();

    QTcpSocket *m_connectionSocket;
    QString m_connectionHostName;
    quint16 m_connectionPort;
};

void tst_QXmppSocks::init()
{
    m_connectionSocket = nullptr;
    m_connectionHostName = QString();
    m_connectionPort = 0;
}

void tst_QXmppSocks::newConnectionSlot(QTcpSocket *socket, QString hostName, quint16 port)
{
    m_connectionSocket = socket;
    m_connectionHostName = hostName;
    m_connectionPort = port;
}

void tst_QXmppSocks::testClient_data()
{
    QTest::addColumn<QByteArray>("serverHandshake");
    QTest::addColumn<bool>("serverHandshakeWorks");
    QTest::addColumn<QByteArray>("serverConnect");
    QTest::addColumn<bool>("serverConnectWorks");
    QTest::addColumn<QByteArray>("clientReceivedData");

    QTest::newRow("no authentication - good connect")
        << QByteArray::fromHex("0500") << true
        << QByteArray::fromHex("050000030e7777772e676f6f676c652e636f6d0050") << true
        << QByteArray();
    QTest::newRow("no authentication - good connect and data")
        << QByteArray::fromHex("0500") << true
        << QByteArray::fromHex("050000030e7777772e676f6f676c652e636f6d0050001122") << true
        << QByteArray::fromHex("001122");

    QTest::newRow("no authentication - bad connect")
        << QByteArray::fromHex("0500") << true
        << QByteArray::fromHex("0500") << false
        << QByteArray();
    QTest::newRow("bad authentication")
        << QByteArray::fromHex("05ff") << false
        << QByteArray() << false
        << QByteArray();
}

void tst_QXmppSocks::testClient()
{
    QFETCH(QByteArray, serverHandshake);
    QFETCH(bool, serverHandshakeWorks);
    QFETCH(QByteArray, serverConnect);
    QFETCH(bool, serverConnectWorks);
    QFETCH(QByteArray, clientReceivedData);

    QTcpServer server;
    QVERIFY(server.listen());
    QVERIFY(server.serverPort() != 0);

    QXmppSocksClient client("127.0.0.1", server.serverPort());

    QEventLoop loop;
    connect(&server, &QTcpServer::newConnection, &loop, &QEventLoop::quit);

    client.connectToHost("www.google.com", 80);
    loop.exec();

    // receive client handshake
    m_connectionSocket = server.nextPendingConnection();
    QVERIFY(m_connectionSocket);

    connect(m_connectionSocket, &QAbstractSocket::disconnected, &loop, &QEventLoop::quit);
    connect(m_connectionSocket, &QIODevice::readyRead, &loop, &QEventLoop::quit);
    loop.exec();
    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionSocket->state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionSocket->readAll(), QByteArray::fromHex("050100"));

    // receive client connect
    m_connectionSocket->write(serverHandshake);
    loop.exec();
    if (!serverHandshakeWorks) {
        QCOMPARE(client.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(m_connectionSocket->state(), QAbstractSocket::UnconnectedState);
        return;
    }

    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionSocket->state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionSocket->readAll(), QByteArray::fromHex("050100030e7777772e676f6f676c652e636f6d0050"));

    // wait for client to be ready
    connect(&client, &QXmppSocksClient::ready, &loop, &QEventLoop::quit);
    m_connectionSocket->write(serverConnect);
    loop.exec();
    if (!serverConnectWorks) {
        QCOMPARE(client.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(m_connectionSocket->state(), QAbstractSocket::UnconnectedState);
        return;
    }

    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionSocket->state(), QAbstractSocket::ConnectedState);
    QByteArray received = client.readAll();
    QCOMPARE(received, clientReceivedData);

    // disconnect
    client.disconnectFromHost();
}

void tst_QXmppSocks::testClientAndServer()
{
    QXmppSocksServer server;
    QVERIFY(server.listen());
    QVERIFY(server.serverPort() != 0);
    connect(&server, &QXmppSocksServer::newConnection,
            this, &tst_QXmppSocks::newConnectionSlot);

    QXmppSocksClient client("127.0.0.1", server.serverPort());

    QEventLoop loop;
    connect(&client, &QXmppSocksClient::ready, &loop, &QEventLoop::quit);

    client.connectToHost("www.google.com", 80);
    loop.exec();

    // check client
    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);

    // check server
    QVERIFY(m_connectionSocket);
    QCOMPARE(m_connectionSocket->state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionHostName, QLatin1String("www.google.com"));
    QCOMPARE(m_connectionPort, quint16(80));

    // disconnect
    client.disconnectFromHost();
}

void tst_QXmppSocks::testServer_data()
{
    QTest::addColumn<QByteArray>("clientHandshake");
    QTest::addColumn<bool>("clientHandshakeWorks");
    QTest::addColumn<QByteArray>("clientConnect");
    QTest::addColumn<bool>("clientConnectWorks");

    QTest::newRow("no authentication - connect to www.google.com:80")
        << QByteArray::fromHex("050100") << true
        << QByteArray::fromHex("050100030e7777772e676f6f676c652e636f6d0050") << true;
    QTest::newRow("no authentication - bad connect")
        << QByteArray::fromHex("050100") << true
        << QByteArray::fromHex("0500") << false;
    QTest::newRow("no authentication or GSSAPI - connect to www.google.com:80")
        << QByteArray::fromHex("05020001") << true
        << QByteArray::fromHex("050100030e7777772e676f6f676c652e636f6d0050") << true;

    QTest::newRow("bad SOCKS version")
        << QByteArray::fromHex("060100") << false
        << QByteArray() << false;
    QTest::newRow("no methods")
        << QByteArray::fromHex("0500") << false
        << QByteArray() << false;
    QTest::newRow("GSSAPI only")
        << QByteArray::fromHex("050101") << false
        << QByteArray() << false;
}

void tst_QXmppSocks::testServer()
{
    QFETCH(QByteArray, clientHandshake);
    QFETCH(bool, clientHandshakeWorks);
    QFETCH(QByteArray, clientConnect);
    QFETCH(bool, clientConnectWorks);

    QXmppSocksServer server;
    QVERIFY(server.listen());
    QVERIFY(server.serverPort() != 0);
    connect(&server, &QXmppSocksServer::newConnection,
            this, &tst_QXmppSocks::newConnectionSlot);

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    QVERIFY2(client.waitForConnected(), qPrintable(client.errorString()));

    QEventLoop loop;
    connect(&client, &QAbstractSocket::disconnected, &loop, &QEventLoop::quit);
    connect(&client, &QIODevice::readyRead, &loop, &QEventLoop::quit);

    // send client handshake
    client.write(clientHandshake);
    loop.exec();
    if (!clientHandshakeWorks) {
        // consume any last data
        QByteArray data = client.readAll();
        if (client.state() != QAbstractSocket::UnconnectedState) {
            loop.exec();
        }

        QCOMPARE(client.state(), QAbstractSocket::UnconnectedState);

        QVERIFY(!m_connectionSocket);
        QVERIFY(m_connectionHostName.isNull());
        QCOMPARE(m_connectionPort, quint16(0));
        return;
    }

    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(client.readAll(), QByteArray::fromHex("0500"));

    // request connect to www.google.com port 80
    client.write(clientConnect);
    loop.exec();
    if (!clientConnectWorks) {
        QCOMPARE(client.state(), QAbstractSocket::UnconnectedState);

        QVERIFY(!m_connectionSocket);
        QVERIFY(m_connectionHostName.isNull());
        QCOMPARE(m_connectionPort, quint16(0));
        return;
    }

    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(client.readAll(), QByteArray::fromHex("050000030e7777772e676f6f676c652e636f6d0050"));

    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    QVERIFY(m_connectionSocket);
    QCOMPARE(m_connectionSocket->state(), QAbstractSocket::ConnectedState);
    QCOMPARE(m_connectionHostName, QLatin1String("www.google.com"));
    QCOMPARE(m_connectionPort, quint16(80));

    // disconnect
    client.disconnectFromHost();
}

}  // namespace Socks

QXMPP_TEST_MAIN(Server::tst_QXmppServer, StunMessage::tst_QXmppStunMessage, Socks::tst_QXmppSocks)

#include "Network.moc"

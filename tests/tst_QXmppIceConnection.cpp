// SPDX-FileCopyrightText: 2015 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppStun.h"
#include "QXmppStunServer.h"

#include "IntegrationTesting.h"

#include <QHostInfo>
#include <QNetworkDatagram>
#include <QTest>
#include <QUdpSocket>

using namespace QXmpp;

// Answers Binding Requests with the sender address, which is enough to gather
// a server-reflexive candidate.
class StunTestServer : public QObject
{
    Q_OBJECT

public:
    explicit StunTestServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_socket, &QUdpSocket::readyRead, this, &StunTestServer::readPendingDatagrams);
    }

    bool listen() { return m_socket.bind(QHostAddress::LocalHost); }
    StunServer address() const { return { QHostAddress(QHostAddress::LocalHost), m_socket.localPort() }; }

private:
    void readPendingDatagrams()
    {
        while (m_socket.hasPendingDatagrams()) {
            auto datagram = m_socket.receiveDatagram();

            QXmppStunMessage request;
            if (!request.decode(datagram.data())) {
                continue;
            }
            if (request.messageMethod() != QXmppStunMessage::Binding ||
                request.messageClass() != QXmppStunMessage::Request) {
                continue;
            }

            QXmppStunMessage response;
            response.setId(request.id());
            response.setType(int(QXmppStunMessage::Binding) | int(QXmppStunMessage::Response));
            response.xorMappedHost = datagram.senderAddress();
            response.xorMappedPort = datagram.senderPort();

            m_socket.writeDatagram(response.encode(), datagram.senderAddress(), datagram.senderPort());
        }
    }

    QUdpSocket m_socket;
};

class tst_QXmppIceConnection : public QObject
{
    Q_OBJECT

private:
    Q_SLOT void testBind();
    Q_SLOT void testBindStun();
    Q_SLOT void testBindStunNetwork();
    Q_SLOT void testConnect();
};

void tst_QXmppIceConnection::testBind()
{
    const int componentId = 1024;

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::StdoutLogging);

    QXmppIceConnection client;
    connect(&client, &QXmppLoggable::logMessage,
            &logger, &QXmppLogger::log);
    client.setIceControlling(true);
    client.addComponent(componentId);

    QXmppIceComponent *component = client.component(componentId);
    QVERIFY(component);

    QCOMPARE(client.gatheringState(), QXmppIceConnection::NewGatheringState);
    client.bind(QXmppIceComponent::discoverAddresses());
    QCOMPARE(client.gatheringState(), QXmppIceConnection::CompleteGatheringState);
    QCOMPARE(client.localCandidates().size(), component->localCandidates().size());
    QVERIFY(!client.localCandidates().isEmpty());
    const auto &localCandidates = client.localCandidates();
    for (const auto &c : localCandidates) {
        QCOMPARE(c.component(), componentId);
        QCOMPARE(c.type(), QXmppJingleCandidate::HostType);
    }
}

void tst_QXmppIceConnection::testBindStun()
{
    const int componentId = 1024;

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::StdoutLogging);

    StunTestServer stunServer;
    QVERIFY(stunServer.listen());

    QXmppIceConnection client;
    connect(&client, &QXmppLoggable::logMessage,
            &logger, &QXmppLogger::log);
    client.setIceControlling(true);
    client.setStunServers({ stunServer.address() });
    client.addComponent(componentId);

    QXmppIceComponent *component = client.component(componentId);
    QVERIFY(component);

    QCOMPARE(client.gatheringState(), QXmppIceConnection::NewGatheringState);
    // discoverAddresses() skips the loopback interface the STUN server runs on
    client.bind({ QHostAddress(QHostAddress::LocalHost) });
    QCOMPARE(client.gatheringState(), QXmppIceConnection::BusyGatheringState);

    QTRY_COMPARE(client.gatheringState(), QXmppIceConnection::CompleteGatheringState);

    bool foundReflexive = false;
    QCOMPARE(client.localCandidates().size(), component->localCandidates().size());
    QVERIFY(!client.localCandidates().isEmpty());
    const auto &localCandidates = client.localCandidates();
    for (const auto &c : localCandidates) {
        QCOMPARE(c.component(), componentId);
        if (c.type() == QXmppJingleCandidate::ServerReflexiveType) {
            foundReflexive = true;
        } else {
            QCOMPARE(c.type(), QXmppJingleCandidate::HostType);
        }
    }
    QVERIFY(foundReflexive);
}

// Same as testBindStun(), but against a real STUN server. Opt-in because
// unreachable addresses only time out after about a minute.
void tst_QXmppIceConnection::testBindStunNetwork()
{
    if (!IntegrationTests::enabled()) {
        QSKIP("Export 'QXMPP_TESTS_INTEGRATION_ENABLED=1' to enable.");
    }

    const int componentId = 1024;

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::StdoutLogging);

    QHostInfo stunInfo = QHostInfo::fromName("stun.l.google.com");
    QVERIFY(!stunInfo.addresses().isEmpty());

    QXmppIceConnection client;
    connect(&client, &QXmppLoggable::logMessage,
            &logger, &QXmppLogger::log);
    client.setIceControlling(true);
    QList<StunServer> stunServers;
    for (auto &address : stunInfo.addresses()) {
        stunServers.push_back({ address, 19302 });
    }
    client.setStunServers(stunServers);
    client.addComponent(componentId);

    QXmppIceComponent *component = client.component(componentId);
    QVERIFY(component);

    QCOMPARE(client.gatheringState(), QXmppIceConnection::NewGatheringState);
    client.bind(QXmppIceComponent::discoverAddresses());
    QCOMPARE(client.gatheringState(), QXmppIceConnection::BusyGatheringState);

    QTRY_COMPARE_WITH_TIMEOUT(client.gatheringState(), QXmppIceConnection::CompleteGatheringState, 120'000);

    bool foundReflexive = false;
    QCOMPARE(client.localCandidates().size(), component->localCandidates().size());
    QVERIFY(!client.localCandidates().isEmpty());
    const auto &localCandidates = client.localCandidates();
    for (const auto &c : localCandidates) {
        QCOMPARE(c.component(), componentId);
        if (c.type() == QXmppJingleCandidate::ServerReflexiveType) {
            foundReflexive = true;
        } else {
            QCOMPARE(c.type(), QXmppJingleCandidate::HostType);
        }
    }
    QVERIFY(foundReflexive);
}

void tst_QXmppIceConnection::testConnect()
{
    const int componentId = 1024;

    QXmppLogger logger;
    logger.setLoggingType(QXmppLogger::StdoutLogging);

    QXmppIceConnection clientL;
    connect(&clientL, &QXmppLoggable::logMessage,
            &logger, &QXmppLogger::log);
    clientL.setIceControlling(true);
    clientL.addComponent(componentId);
    clientL.bind(QXmppIceComponent::discoverAddresses());

    QXmppIceConnection clientR;
    connect(&clientR, &QXmppLoggable::logMessage,
            &logger, &QXmppLogger::log);
    clientR.setIceControlling(false);
    clientR.addComponent(componentId);
    clientR.bind(QXmppIceComponent::discoverAddresses());

    // exchange credentials
    clientL.setRemoteUser(clientR.localUser());
    clientL.setRemotePassword(clientR.localPassword());
    clientR.setRemoteUser(clientL.localUser());
    clientR.setRemotePassword(clientL.localPassword());

    // exchange candidates
    const auto &rLocalCandidates = clientR.localCandidates();
    for (const auto &candidate : rLocalCandidates) {
        clientL.addRemoteCandidate(candidate);
    }
    const auto &lLocalCandidates = clientL.localCandidates();
    for (const auto &candidate : lLocalCandidates) {
        clientR.addRemoteCandidate(candidate);
    }

    // start ICE
    clientL.connectToHost();
    clientR.connectToHost();

    QTRY_VERIFY(clientL.isConnected());
    QTRY_VERIFY(clientR.isConnected());
}

QTEST_MAIN(tst_QXmppIceConnection)
#include "tst_QXmppIceConnection.moc"

// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef CLIENTTESTING_H
#define CLIENTTESTING_H

#include "QXmppClient.h"
#include "QXmppDiscoveryManager.h"

#include "util.h"

#include <chrono>

class QXmppOutgoingClient;
class QXmppOutgoingClientPrivate;
namespace QXmpp::Private::Sasl2 {
struct StreamFeature;
}

class TestClient : public QXmppClient
{
    Q_OBJECT
public:
    TestClient(bool enableDebug = false, bool enableAutoReset = true);
    ~TestClient() override;

    QXmppOutgoingClient *stream() const;
    QXmppOutgoingClientPrivate *streamPrivate() const;

    // Test wrappers for the private outgoing-client entry points exercised by the SASL2 + FAST
    // listener-replacement regression test.
    void startSasl2Auth(const QXmpp::Private::Sasl2::StreamFeature &feature);
    void handlePacketReceived(const QDomElement &el);

    template<typename String>
    void inject(const String &xml) { inject(xmlToDom(xml)); }

    void injectPresence(const QXmppPresence &presence);
    void simulateConnected();
    void simulateDisconnected();

    void inject(const QDomElement &element);

    void expect(QString &&packet);
    // compares packets, ignoring different IDs and order of sending
    // returns ID of the packet that matched
    QString expectPacketRandomOrder(QString &&expected);
    // connects with a new stream and answers the disco#info request to jid with the features
    void connectAndAnswerDiscoInfo(const QString &jid, const QStringList &features);
    QString takePacket();
    QString takeLastPacket();
    void expectNoPacket() const;
    void ignore();

    void resetIdCount();
    void setStreamManagementState(QXmppClient::StreamManagementState state);
    void setStreamResumable(bool resumable);
    void setLoopbackIgnoresNetwork(bool ignores);
    void simulateKeepAliveTimeout();
    void simulateSocketError();
    std::chrono::milliseconds reconnectionInterval() const;
    bool isReconnectionScheduled() const;
    void sendRegularPing();
    void expirePingTimer();
    std::chrono::milliseconds pingTimeout() const;
    std::chrono::milliseconds nextPingCheck() const;
    void waitForConnect();

private:
    void onLoggerMessage(QXmppLogger::MessageType type, const QString &text);

    bool debugEnabled;
    bool autoResetEnabled;
    QList<QString> m_sentPackets;
};

// Checks that the feature watch returned by watchSupport is based on feature in the disco#info of
// jid, for the account juliet@capulet.example.
template<typename Manager, typename WatchSupport>
void checkWatchSupport(WatchSupport watchSupport, const QString &jid, const QString &feature)
{
    TestClient client;
    client.configuration().setJid(u"juliet@capulet.example"_s);
    client.addNewExtension<QXmppDiscoveryManager>();
    auto *manager = client.addNewExtension<Manager>();

    auto watch = std::invoke(watchSupport, manager);
    QVERIFY(!watch.supported().value());

    client.connectAndAnswerDiscoInfo(jid, { feature });
    QVERIFY(watch.supported().value());
}

#endif  // CLIENTTESTING_H

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
    // passes a stanza to the handlers of QXmpp::Connection::StanzaReceived and the extensions,
    // returns whether it has been handled
    bool handleStanza(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata = {});
    bool handleStanza(const QString &xml) { return handleStanza(xmlToDom(xml)); }
    // passes a stanza to the handlers of the client of \a extension, like handleStanza()
    static bool handleStanza(QXmppClientExtension *extension, const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata = {});
    // runs a message received from the stream through the message pipeline
    void injectMessage(const QString &xml);
    // like a stanza received from the stream
    void injectStanza(const QString &xml);

    void expect(QString &&packet);
    // compares packets, ignoring different IDs and order of sending
    // returns ID of the packet that matched
    QString expectPacketRandomOrder(QString &&expected);
    // connects with a new stream and answers the disco#info request to jid with the features
    void connectAndAnswerDiscoInfo(const QString &jid, const QStringList &features);
    QString takePacket();
    QString takeLastPacket();
    void expectNoPacket() const;
    bool hasPacket() const { return !m_sentPackets.isEmpty(); }
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

// Returns the service discovery features a new extension of type Extension adds to a client that
// already has the extensions of the types Dependencies.
template<typename Extension, typename... Dependencies>
QStringList addedDiscoveryFeatures()
{
    QXmppClient client(QXmppClient::NoExtensions);
    auto *disco = client.addNewExtension<QXmppDiscoveryManager>();
    (client.addNewExtension<Dependencies>(), ...);
    const auto baseFeatures = disco->buildClientInfo().features();

    if constexpr (std::is_constructible_v<Extension, QXmppClient *>) {
        client.addNewExtension<Extension>(&client);
    } else {
        client.addNewExtension<Extension>();
    }
    auto features = disco->buildClientInfo().features();
    features.removeIf([&](const QString &feature) { return baseFeatures.contains(feature); });
    return features;
}

#endif  // CLIENTTESTING_H

// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef CLIENTTESTING_H
#define CLIENTTESTING_H

#include "QXmppClient.h"

#include "util.h"

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
    QString takePacket();
    QString takeLastPacket();
    void expectNoPacket() const;
    void ignore();

    void resetIdCount();
    void setStreamManagementState(QXmppClient::StreamManagementState state);
    void waitForConnect();

private:
    void onLoggerMessage(QXmppLogger::MessageType type, const QString &text);

    bool debugEnabled;
    bool autoResetEnabled;
    QList<QString> m_sentPackets;
};

#endif  // CLIENTTESTING_H

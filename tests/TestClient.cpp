// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "TestClient.h"

#include "QXmppClientExtension.h"  // needed for qDeleteAll(d->extensions)
#include "QXmppClient_p.h"
#include "QXmppOutgoingClient.h"
#include "QXmppSasl_p.h"
#include "QXmppUtils_p.h"

#include <QEventLoop>

TestClient::TestClient(bool enableDebug, bool enableAutoReset)
    : QXmppClient(),
      debugEnabled(enableDebug),
      autoResetEnabled(enableAutoReset)
{
    // clear extensions
    qDeleteAll(d->extensions);
    d->extensions.clear();
    // enable stream management (so IQ requests are not stopped)
    d->stream->enableStreamManagement(true);
    // setup logging (for expect())
    logger()->setLoggingType(QXmppLogger::SignalLogging);
    connect(logger(), &QXmppLogger::message, this, &TestClient::onLoggerMessage);
    // In all case, start with a 0 default id
    resetIdCount();
}

TestClient::~TestClient() = default;

QXmppOutgoingClient *TestClient::stream() const { return d->stream; }
QXmppOutgoingClientPrivate *TestClient::streamPrivate() const { return d->stream->d.get(); }

void TestClient::startSasl2Auth(const QXmpp::Private::Sasl2::StreamFeature &feature)
{
    d->stream->startSasl2Auth(feature);
}

void TestClient::handlePacketReceived(const QDomElement &el)
{
    d->stream->handlePacketReceived(el);
}

void TestClient::injectPresence(const QXmppPresence &presence) { Q_EMIT presenceReceived(presence); }
void TestClient::simulateConnected() { Q_EMIT connected(); }
void TestClient::simulateDisconnected() { Q_EMIT disconnected(); }

void TestClient::inject(const QDomElement &element)
{
    if (!d->stream->handleIqResponse(element)) {
        for (auto *extension : std::as_const(d->extensions)) {
            if (extension->handleStanza(element)) {
                break;
            }
        }
    }
    if (autoResetEnabled) {
        resetIdCount();
    }
}

void TestClient::expect(QString &&packet)
{
    QVERIFY2(!m_sentPackets.empty(), "No packet was sent!");

    auto expectedXml = rewriteXml(packet);
    auto actualXml = rewriteXml(m_sentPackets.takeFirst());
    QCOMPARE(actualXml, expectedXml);

    if (autoResetEnabled) {
        resetIdCount();
    }
}

QString TestClient::expectPacketRandomOrder(QString &&expected)
{
    VERIFY2(!m_sentPackets.empty(), "No packet was sent!");

    auto [expectedXml, _] = rewriteXmlWithoutStanzaId(expected);
    for (const auto &packet : std::as_const(m_sentPackets)) {
        auto [packetFormattedXml, stanzaId] = rewriteXmlWithoutStanzaId(packet);
        if (packetFormattedXml == expectedXml) {
            []() { QVERIFY(true); }();
            m_sentPackets.removeOne(packet);
            return stanzaId;
        }
    }

    // Failure
    qDebug() << "Expected:";
    qDebug() << expectedXml;
    qDebug() << "Got:";
    for (const auto &packet : m_sentPackets) {
        auto [xml, id] = rewriteXmlWithoutStanzaId(packet);
        qDebug() << xml;
    }

    []() { QFAIL("Expected packet was not sent!"); }();
    return {};
}

QString TestClient::takePacket()
{
    [this]() { QVERIFY(!m_sentPackets.isEmpty()); }();
    return m_sentPackets.takeFirst();
}

QString TestClient::takeLastPacket()
{
    [this]() { QVERIFY(!m_sentPackets.isEmpty()); }();
    return m_sentPackets.takeLast();
}

void TestClient::expectNoPacket() const
{
    if (!m_sentPackets.empty()) {
        qDebug() << "Unexpected:";
        for (const auto &packet : m_sentPackets) {
            qDebug().noquote() << " *" << packet;
        }
    }
    VERIFY2(m_sentPackets.empty(), "Unexpected packet sent!");
}

void TestClient::ignore()
{
    m_sentPackets.takeFirst();
    if (autoResetEnabled) {
        resetIdCount();
    }
}

void TestClient::resetIdCount()
{
    QXmpp::Private::globalStanzaIdCounter = 0;
}

void TestClient::setStreamManagementState(QXmppClient::StreamManagementState state)
{
    switch (state) {
    case QXmppClient::StreamManagementState::NoStreamManagement:
        d->stream->c2sStreamManager().setEnabled(false);
        break;
    case QXmppClient::StreamManagementState::NewStream:
        d->stream->c2sStreamManager().setEnabled(true);
        d->stream->c2sStreamManager().setResumed(false);
        break;
    case QXmppClient::StreamManagementState::ResumedStream:
        d->stream->c2sStreamManager().setEnabled(true);
        d->stream->c2sStreamManager().setResumed(true);
        break;
    }
}

void TestClient::waitForConnect()
{
    QEventLoop loop;
    connect(this, &QXmppClient::connected, &loop, &QEventLoop::quit);
    connect(this, &QXmppClient::disconnected, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(isConnected());
}

void TestClient::onLoggerMessage(QXmppLogger::MessageType type, const QString &text)
{
    if (text == u"<r xmlns=\"urn:xmpp:sm:3\"/>") {
        return;
    }

    if (debugEnabled) {
        QStringView typeString;
        switch (type) {
        case QXmppLogger::DebugMessage:
            typeString = u"DEBUG";
            break;
        case QXmppLogger::InformationMessage:
            typeString = u"INFO";
            break;
        case QXmppLogger::WarningMessage:
            typeString = u"WARNING";
            break;
        case QXmppLogger::ReceivedMessage:
            typeString = u"RECEIVED";
            break;
        case QXmppLogger::SentMessage:
            typeString = u"SENT";
            break;
        default:
            Q_UNREACHABLE();
        }
        qDebug().noquote() << typeString << text;
    }

    if (type == QXmppLogger::SentMessage) {
        m_sentPackets << text;
    }
}

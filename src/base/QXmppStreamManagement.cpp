// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPacket_p.h"
#include "QXmppStreamManagement_p.h"

#include "StringLiterals.h"
#include "XmppSocket.h"

namespace QXmpp::Private {

StreamAckManager::StreamAckManager(XmppSocket &socket)
    : socket(socket)
{
}

bool StreamAckManager::handleStanza(const QDomElement &stanza)
{
    if (auto ack = elementFromDom<SmAck>(stanza)) {
        handleAcknowledgement(*ack);
        return true;
    }
    if (auto req = elementFromDom<SmRequest>(stanza)) {
        sendAcknowledgement();
        return true;
    }

    auto tagName = stanza.tagName();
    if (tagName == u"message" || tagName == u"presence" || tagName == u"iq") {
        m_lastIncomingSequenceNumber++;
    }
    return false;
}

void StreamAckManager::onSessionClosed()
{
    m_enabled = false;
}

void StreamAckManager::enableStreamManagement(bool resetSequenceNumber)
{
    m_enabled = true;

    if (resetSequenceNumber) {
        m_lastOutgoingSequenceNumber = 0;
        m_lastIncomingSequenceNumber = 0;

        // resend unacked stanzas
        if (!m_unacknowledgedStanzas.isEmpty()) {
            auto oldUnackedStanzas = m_unacknowledgedStanzas;
            m_unacknowledgedStanzas.clear();

            for (auto &packet : oldUnackedStanzas) {
                m_unacknowledgedStanzas.insert(++m_lastOutgoingSequenceNumber, packet);
                socket.sendData(packet.data());
            }

            sendAcknowledgementRequest();
        }
    } else {
        // resend unacked stanzas
        if (!m_unacknowledgedStanzas.isEmpty()) {
            for (auto &packet : m_unacknowledgedStanzas) {
                socket.sendData(packet.data());
            }

            sendAcknowledgementRequest();
        }
    }
}

void StreamAckManager::setAcknowledgedSequenceNumber(unsigned int sequenceNumber)
{
    for (auto it = m_unacknowledgedStanzas.begin(); it != m_unacknowledgedStanzas.end();) {
        if (it.key() <= sequenceNumber) {
            it->reportFinished(QXmpp::SendSuccess { true });
            it = m_unacknowledgedStanzas.erase(it);
        } else {
            break;
        }
    }
}

QXmppTask<SendResult> StreamAckManager::send(QXmppPacket &&packet)
{
    return std::get<1>(internalSend(std::move(packet)));
}

bool StreamAckManager::sendPacketCompat(QXmppPacket &&packet)
{
    return std::get<0>(internalSend(std::move(packet)));
}

// Returns written to socket (bool) and QXmppTask
std::tuple<bool, QXmppTask<SendResult>> StreamAckManager::internalSend(QXmppPacket &&packet)
{
    // the writtenToSocket parameter is just for backwards compat
    bool writtenToSocket = socket.sendData(packet.data());

    // handle stream management
    if (m_enabled && packet.isXmppStanza()) {
        m_unacknowledgedStanzas.insert(++m_lastOutgoingSequenceNumber, packet);
        sendAcknowledgementRequest();
    } else {
        if (writtenToSocket) {
            packet.reportFinished(QXmpp::SendSuccess { false });
        } else {
            packet.reportFinished(QXmppError {
                u"Couldn't write data to socket. No stream management enabled."_s,
                QXmpp::SendError::SocketWriteError,
            });
        }
    }

    return { writtenToSocket, packet.task() };
}

void StreamAckManager::handleAcknowledgement(SmAck ack)
{
    if (!m_enabled) {
        return;
    }

    setAcknowledgedSequenceNumber(ack.seqNo);
}

void StreamAckManager::sendAcknowledgement()
{
    if (!m_enabled) {
        return;
    }

    socket.sendData(serializeXml(SmAck { m_lastIncomingSequenceNumber }));
}

void StreamAckManager::sendAcknowledgementRequest()
{
    if (!m_enabled) {
        return;
    }

    // send packet
    socket.sendData(serializeXml(SmRequest {}));
}

void StreamAckManager::resetCache()
{
    for (auto &packet : m_unacknowledgedStanzas) {
        packet.reportFinished(QXmppError {
            u"Disconnected"_s,
            QXmpp::SendError::Disconnected });
    }

    m_unacknowledgedStanzas.clear();
}

}  // namespace QXmpp::Private

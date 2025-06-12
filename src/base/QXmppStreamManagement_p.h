// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTREAMMANAGEMENT_P_H
#define QXMPPSTREAMMANAGEMENT_P_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"
#include "QXmppSendResult.h"
#include "QXmppStanza.h"
#include "QXmppStanza_p.h"
#include "QXmppTask.h"

#include "Xml.h"

#include <QDomDocument>
#include <QXmlStreamWriter>

class QXmppPacket;

namespace QXmpp::Private {
class XmlWriter;
class XmppSocket;
}  // namespace QXmpp::Private

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API.  It exists for the convenience
// of the QXmppIncomingClient and QXmppOutgoingClient classes.
//
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

namespace QXmpp::Private {

struct SmEnable {
    static constexpr std::tuple XmlTag = { u"enable", ns_stream_management };
    bool resume = false;
    uint64_t max;
};

struct SmEnabled {
    static constexpr std::tuple XmlTag = { u"enabled", ns_stream_management };
    bool resume = false;
    QString id;
    uint64_t max = 0;
    QString location;
};

struct SmResume {
    static constexpr std::tuple XmlTag = { u"resume", ns_stream_management };
    uint32_t h = 0;
    QString previd;
};

struct SmResumed {
    static constexpr std::tuple XmlTag = { u"resumed", ns_stream_management };
    uint32_t h = 0;
    QString previd;
};

struct SmFailed {
    static constexpr std::tuple XmlTag = { u"failed", ns_stream_management };
    std::optional<uint32_t> h;
    std::optional<QXmppStanza::Error::Condition> error;
};

struct SmAck {
    static constexpr std::tuple XmlTag = { u"a", ns_stream_management };
    uint32_t seqNo = 0;
};

struct SmRequest {
    static constexpr std::tuple XmlTag = { u"r", ns_stream_management };
};

template<>
struct XmlSpec<SmEnable> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmEnable::resume, u"resume", BoolDefaultSerializer { false } },
        XmlOptionalAttribute { &SmEnable::max, u"max", PositiveIntSerializer() },
    };
};

template<>
struct XmlSpec<SmEnabled> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmEnabled::resume, u"resume", BoolDefaultSerializer { false } },
        XmlOptionalAttribute { &SmEnabled::id, u"id" },
        XmlOptionalAttribute { &SmEnabled::max, u"max", PositiveIntSerializer() },
        XmlOptionalAttribute { &SmEnabled::location, u"location" },
    };
};

template<>
struct XmlSpec<SmResume> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmResume::h, u"h" },
        XmlAttribute { &SmResume::previd, u"previd" },
    };
};

template<>
struct XmlSpec<SmResumed> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmResumed::h, u"h" },
        XmlAttribute { &SmResumed::previd, u"previd" },
    };
};

template<>
struct XmlSpec<SmFailed> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmFailed::h, u"h" },
        XmlOptionalEnumElement { &SmFailed::error, ns_stanza },
    };
};

template<>
struct XmlSpec<SmAck> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmAck::seqNo, u"h" },
    };
};

template<>
struct XmlSpec<SmRequest> {
    static constexpr std::tuple Spec = {};
};

//
// This manager handles sending and receiving of stream management acks.
// Enabling of stream management and stream resumption is done in the C2sStreamManager.
//
class StreamAckManager
{
public:
    explicit StreamAckManager(XmppSocket &socket);

    bool enabled() const { return m_enabled; }
    unsigned int lastIncomingSequenceNumber() const { return m_lastIncomingSequenceNumber; }

    void handlePacketSent(QXmppPacket &packet, bool sentData);
    bool handleStanza(const QDomElement &stanza);
    void onSessionClosed();

    void resetCache();
    void enableStreamManagement(bool resetSequenceNumber);
    void setAcknowledgedSequenceNumber(unsigned int sequenceNumber);

    QXmppTask<QXmpp::SendResult> send(QXmppPacket &&);
    bool sendPacketCompat(QXmppPacket &&);
    std::tuple<bool, QXmppTask<QXmpp::SendResult>> internalSend(QXmppPacket &&);

    void sendAcknowledgementRequest();

private:
    void handleAcknowledgement(SmAck ack);

    void sendAcknowledgement();

    QXmpp::Private::XmppSocket &socket;

    bool m_enabled = false;
    QMap<unsigned int, QXmppPacket> m_unacknowledgedStanzas;
    unsigned int m_lastOutgoingSequenceNumber = 0;
    unsigned int m_lastIncomingSequenceNumber = 0;
};

}  // namespace QXmpp::Private

#endif

// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTANZAHANDLING_P_H
#define QXMPPSTANZAHANDLING_P_H

#include "QXmppClientExtension.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppIqHandling.h"
#include "QXmppMessageContext.h"
#include "QXmppMessages.h"

#include <QDomElement>

namespace QXmpp::Private {

// Payload of IQ requests parsed with a class derived from QXmppIq, for handlers of
// QXmpp::Iq::Received not yet using a payload type.
template<typename IqClass>
    requires HasPayloadXmlTag<IqClass>
struct LegacyIqRequest {
    static constexpr auto XmlTag = PayloadXmlTag<IqClass>;

    IqClass iq;

    static std::optional<LegacyIqRequest> fromDom(const QDomElement &payload)
    {
        LegacyIqRequest request;
        request.iq.parse(payload.parentNode().toElement());
        return request;
    }
    // requests are never sent
    void toXml(QXmlStreamWriter *) const { }
};

// Converts the result of a handler of the deprecated QXmpp::handleIqRequests() without payload.
inline Iq::Reply legacyIqReply(const QXmppIq &iq)
{
    if (iq.type() == QXmppIq::Error) {
        return Iq::Reply(iq.error());
    }
    return Iq::Reply();
}

// Returns whether the IQ is a request (get or set). Requests are answered by the handlers of
// QXmpp::Iq::Received.
inline bool isIqRequest(const QDomElement &iq)
{
    const auto type = iq.attribute(QStringLiteral("type"));
    return type == u"get" || type == u"set";
}

// Adapts a function handling incoming stanzas to the QXmpp::Client::StanzaReceived hook. It
// returns whether it has handled the stanza and gets no end-to-end encrypted stanzas, so it does
// not reply unencrypted.
template<typename Object>
auto unencryptedStanzaHandler(Object *object, bool (Object::*function)(const QDomElement &))
{
    return [object, function](const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata) {
        return !e2eeMetadata && (object->*function)(stanza) ? HookResult::Stop : HookResult::Continue;
    };
}

// Adapts a function handling incoming stanzas, also end-to-end encrypted ones, to the
// QXmpp::Client::StanzaReceived hook. It returns whether it has handled the stanza.
template<typename Object>
auto stanzaHandler(Object *object, bool (Object::*function)(const QDomElement &, const std::optional<QXmppE2eeMetadata> &))
{
    return [object, function](const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata) {
        return (object->*function)(stanza, e2eeMetadata) ? HookResult::Stop : HookResult::Continue;
    };
}

// Adapts a function handling incoming messages to the QXmpp::Messages::Received hook. It returns
// whether it has handled the message. Like the deprecated QXmppMessageHandler, it gets no
// messages of archive queries.
template<typename Object>
auto messageHandler(Object *object, bool (Object::*function)(const QXmppMessage &))
{
    return [object, function](Messages::Context &context) {
        if (context.origin() == Messages::Origin::Mam) {
            return HookResult::Continue;
        }
        return (object->*function)(context.message()) ? HookResult::Stop : HookResult::Continue;
    };
}

}  // namespace QXmpp::Private

#endif  // QXMPPSTANZAHANDLING_P_H

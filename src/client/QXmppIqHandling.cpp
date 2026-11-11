// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppIqHandling.h"

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppHooks_p.h"

#include "StringLiterals.h"

#include <QPointer>

namespace QXmpp::Iq {

/*!
    Returns an error reply for handlers not supporting end-to-end encrypted requests.
*/
Reply Reply::encryptionNotSupported()
{
    using Error = QXmppStanza::Error;
    return Reply(Error(Error::Cancel, Error::FeatureNotImplemented, u"Not supported with end-to-end encryption."_s));
}

}  // namespace QXmpp::Iq

namespace QXmpp::Private {

namespace {

// Serializes the payload of a reply created by a handler of QXmpp::Iq::Received.
class ReplyIq : public QXmppIq
{
public:
    explicit ReplyIq(Iq::Reply reply)
        : QXmppIq(reply.error() ? QXmppIq::Error : QXmppIq::Result), m_reply(std::move(reply))
    {
        if (const auto &error = m_reply.error()) {
            setError(*error);
        }
    }

protected:
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override
    {
        m_reply.writePayload(writer);
    }

private:
    Iq::Reply m_reply;
};

void sendReply(QXmppClient *client, const QDomElement &request, Iq::Reply &&reply, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
{
    ReplyIq iq(std::move(reply));
    iq.setId(request.attribute(u"id"_s));
    iq.setTo(request.attribute(u"from"_s));
    LegacyE2ee::reply(client, std::move(iq), e2eeMetadata);
}

QXmppTask<void> answerIqRequest(QPointer<QXmppClient> client, Hooks hooks, IqRequestKey key, QDomElement element, Iq::Context context)
{
    auto reply = co_await hooks.run<IqRequestReceived>(key, element, context);
    if (!client) {
        co_return;
    }

    // the owner of the handler has been destroyed or the handler has dropped its task
    using Error = QXmppStanza::Error;
    sendReply(client, element, reply ? std::move(*reply) : Iq::Reply(Error(Error::Cancel, Error::ServiceUnavailable)), context.e2eeMetadata());
}

}  // namespace

bool IqRequests::process(QXmppClient *client, const Hooks &hooks, const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
{
    if (element.tagName() != u"iq") {
        return false;
    }
    const auto typeString = element.attribute(u"type"_s);
    Iq::Type type;
    if (typeString == u"get") {
        type = Iq::Type::Get;
    } else if (typeString == u"set") {
        type = Iq::Type::Set;
    } else {
        return false;
    }

    // requests without an id can't be answered (RFC 6120, section 8.2.3)
    if (element.attribute(u"id"_s).isEmpty()) {
        return true;
    }

    // requests need exactly one payload (RFC 6120, section 8.2.3)
    const auto payload = element.firstChildElement();
    if (payload.isNull() || !payload.nextSiblingElement().isNull()) {
        using Error = QXmppStanza::Error;
        sendReply(client, element, Error(Error::Modify, Error::BadRequest, u"IQ requests must contain exactly one child element."_s), e2eeMetadata);
        return true;
    }

    IqRequestKey key { type, payload.tagName(), payload.namespaceURI() };
    if (!hooks.hasHandlers<IqRequestReceived>(key)) {
        return false;
    }

    answerIqRequest(client, hooks, std::move(key), element, Iq::Context(element.attribute(u"from"_s), e2eeMetadata));
    return true;
}

// helpers of handleIqRequests()
void sendIqReply(QXmppClient *client,
                 const QString &requestId,
                 const QString &requestFrom,
                 const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                 QXmppIq &&iq)
{
    // default type is 'result'
    switch (iq.type()) {
    case QXmppIq::Get:
    case QXmppIq::Set:
        iq.setType(QXmppIq::Result);
        break;
    case QXmppIq::Error:
    case QXmppIq::Result:
        break;
    }

    iq.setTo(requestFrom);
    iq.setId(requestId);
    LegacyE2ee::reply(client, std::move(iq), e2eeMetadata);
}

std::tuple<bool, QString, QString> checkIsIqRequest(const QDomElement &el)
{
    if (el.tagName() != u"iq") {
        return { false, {}, {} };
    }
    auto queryElement = el.firstChildElement();
    auto iqType = el.attribute(u"type"_s);
    if (iqType != u"get" && iqType != u"set") {
        return { false, {}, {} };
    }
    return { true, queryElement.tagName(), queryElement.namespaceURI() };
}

}  // namespace QXmpp::Private

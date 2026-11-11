// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef IQSENDING_H
#define IQSENDING_H

#include "QXmppClient.h"
#include "QXmppError.h"
#include "QXmppIq.h"
#include "QXmppIqStanza.h"
#include "QXmppUtils.h"
#include "QXmppVisitHelper_p.h"

#include "Algorithms.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <optional>

namespace QXmpp::Private {

// Parses the payload of an IQ result or the error of an IQ error, or passes the error.
template<typename Payload>
auto parseIqResponse(std::variant<QDomElement, QXmppError> &&sendResult) -> std::variant<Payload, QXmppStanza::Error, QXmppError>
{
    using StanzaError = QXmppStanza::Error;
    using Result = std::variant<Payload, QXmppStanza::Error, QXmppError>;

    return map<Result>(
        [](QDomElement &&el) -> Result {
            if (auto result = Iq::Result<Payload>::fromDom(el)) {
                return std::move(result->payload);
            }
            if (auto error = Iq::Error<>::fromDom(el)) {
                return std::move(error->error);
            }

            auto type = el.attribute(u"type"_s);
            if (type == u"result") {
                return QXmppError {
                    u"Failed to parse IQ result payload"_s,
                    StanzaError(StanzaError::Cancel, StanzaError::UndefinedCondition),
                };
            } else if (type == u"error") {
                return QXmppError { u"Failed to parse error response"_s, {} };
            }
            return QXmppError { u"Received unexpected IQ type"_s, {} };
        },
        std::move(sendResult));
}

template<typename Payload>
auto parseIqResponseFlat(std::variant<QDomElement, QXmppError> &&sendResult)
{
    using Result = std::variant<Payload, QXmppError>;
    return map<Result>(
        [](QXmppStanza::Error &&e) -> Result { return QXmppError { e.text(), std::move(e) }; },
        parseIqResponse<Payload>(std::move(sendResult)));
}

// For usage with QXmppClient::sendIq()
template<typename Payload>
class CompatIq : public QXmppIq
{
public:
    std::optional<Payload> payload;

    template<Iq::Type Type>
        requires(Type == Iq::Type::Get || Type == Iq::Type::Set || Type == Iq::Type::Result)
    CompatIq(Iq::Stanza<Type, Payload> &&iq) : QXmppIq(), payload(std::move(iq.payload))
    {
        if (!iq.id.isEmpty()) {
            setId(iq.id);
        }
        setFrom(iq.from);
        setTo(iq.to);
        setLang(iq.lang);
        if constexpr (Type == Iq::Type::Get) {
            setType(QXmppIq::Get);
        } else if constexpr (Type == Iq::Type::Set) {
            setType(QXmppIq::Set);
        } else if constexpr (Type == Iq::Type::Result) {
            setType(QXmppIq::Result);
        } else if constexpr (Type == Iq::Type::Error) {
            setType(QXmppIq::Error);
        }
    }

    CompatIq(QXmppIq::Type type, std::optional<Payload> &&payload = {})
        : QXmppIq(type), payload(std::move(payload))
    {
    }

    void parseElementFromChild(const QDomElement &el) override
    {
        payload = parseElement<Payload>(el.firstChildElement());
    }
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override
    {
        XmlWriter(writer).write(payload);
    }
};

template<Iq::Type Type, typename Payload>
CompatIq(Iq::Stanza<Type, Payload> &&) -> CompatIq<Payload>;

template<typename Payload>
CompatIq(QXmppIq::Type, Payload) -> CompatIq<Payload>;

// helpers to be added to the QXmppClient in the future

template<typename Response, typename Payload>
static QXmppTask<std::variant<Response, QXmppError>> get(QXmppClient *client, const QString &to, Payload &&payload)
{
    return chain<std::variant<Response, QXmppError>>(
        client->sendIq(CompatIq { Iq::Get<std::decay_t<Payload>> { generateSequentialStanzaId(), {}, to, {}, std::forward<Payload>(payload) } }),
        client,
        parseIqResponseFlat<Response>);
}

template<typename Response, typename Payload>
static QXmppTask<std::variant<Response, QXmppError>> set(QXmppClient *client, const QString &to, Payload &&payload)
{
    return chain<std::variant<Response, QXmppError>>(
        client->sendIq(CompatIq { Iq::Set<std::decay_t<Payload>> { generateSequentialStanzaId(), {}, to, {}, std::forward<Payload>(payload) } }),
        client,
        parseIqResponseFlat<Response>);
}

// Set IQ without response payload — returns Result<Success> (success or error)
template<typename Payload>
static QXmppTask<QXmpp::Result<>> set(QXmppClient *client, const QString &to, Payload &&payload)
{
    return client->sendGenericIq(CompatIq { Iq::Set<std::decay_t<Payload>> { generateSequentialStanzaId(), {}, to, {}, std::forward<Payload>(payload) } });
}

// Get IQ without response payload
// Use this for expected results to be an empty <iq type='result'/>.
// A separate name from the general-purpose get<Response, Payload>() helper avoids overload ambiguity.
template<typename Payload>
static QXmppTask<QXmpp::Result<>> getEmpty(QXmppClient *client, const QString &to, Payload &&payload)
{
    return client->sendGenericIq(CompatIq { Iq::Get<std::decay_t<Payload>> { generateSequentialStanzaId(), {}, to, {}, std::forward<Payload>(payload) } });
}

}  // namespace QXmpp::Private

#endif  // IQSENDING_H

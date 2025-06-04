// SPDX-License-Identifier: LGPL-2.1-or-later
//
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>

#ifndef IQ_H
#define IQ_H

#include "QXmppError.h"
#include "QXmppIq.h"
#include "QXmppStanza.h"
#include "QXmppVisitHelper_p.h"

#include "XmlWriter.h"

#include <optional>

#include <QString>

namespace QXmpp::Private {

enum class IqType {
    Get,
    Set,
    Result,
    Error,
};

template<>
struct Enums::Data<IqType> {
    using enum IqType;
    static constexpr auto Values = makeValues<IqType>({
        { Get, u"get" },
        { Set, u"set" },
        { Result, u"result" },
        { Error, u"error" },
    });
};

template<IqType, typename Payload = void>
struct Iq;

template<typename Payload = void>
using GetIq = Iq<IqType::Get, Payload>;

template<typename Payload = void>
using SetIq = Iq<IqType::Set, Payload>;

template<typename Payload = void>
using ResultIq = Iq<IqType::Result, Payload>;

template<typename Payload = void>
using ErrorIq = Iq<IqType::Error, Payload>;

template<IqType Type, typename Payload = void>
auto iqFromDomImpl(const QDomElement &el) -> std::optional<Iq<Type, Payload>>;

template<IqType Type, typename Payload>
    requires((Type == IqType::Get || Type == IqType::Set) && !std::is_void_v<Payload>)
struct Iq<Type, Payload> {
    QString id;
    QString from;
    QString to;
    QString lang;
    Payload payload;

    using PayloadType = Payload;
    static constexpr auto IqType = Type;

    static std::optional<Iq> fromDom(const QDomElement &el) { return iqFromDomImpl<Type, Payload>(el); }
    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"iq",
            Attribute { u"id", id },
            OptionalAttribute { u"from", from },
            OptionalAttribute { u"to", to },
            Attribute { u"type", Type },
            OptionalAttribute { u"xml:lang", lang },
            payload,
        });
    }
};

template<typename Payload>
struct Iq<IqType::Result, Payload> {
    QString id;
    QString from;
    QString to;
    QString lang;
    std::conditional_t<std::is_void_v<Payload>, std::monostate, Payload> payload;

    using PayloadType = Payload;

    static std::optional<Iq> fromDom(const QDomElement &el) { return iqFromDomImpl<IqType::Result, Payload>(el); }
    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"iq",
            Attribute { u"id", id },
            OptionalAttribute { u"from", from },
            OptionalAttribute { u"to", to },
            Attribute { u"type", IqType::Result },
            OptionalAttribute { u"xml:lang", lang },
            payload,
        });
    }
};

template<typename Payload>
struct Iq<IqType::Error, Payload> {
    QString id;
    QString from;
    QString to;
    QString lang;
    std::conditional_t<std::is_void_v<Payload>, std::monostate, std::optional<Payload>> payload;
    QXmppStanza::Error error;

    using PayloadType = Payload;

    static std::optional<Iq> fromDom(const QDomElement &el) { return iqFromDomImpl<IqType::Error, Payload>(el); }
    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            u"iq",
            Attribute { u"id", id },
            OptionalAttribute { u"from", from },
            OptionalAttribute { u"to", to },
            Attribute { u"type", IqType::Error },
            OptionalAttribute { u"xml:lang", lang },
            payload,
            error,
        });
    }
};

template<IqType Type, typename Payload>
auto iqFromDomImpl(const QDomElement &el) -> std::optional<Iq<Type, Payload>>
{
    using IqT = Iq<Type, Payload>;

    if (el.tagName() == u"iq" && el.hasAttribute(u"id"_s) && el.attribute(u"type"_s) == Type) {
        auto id = el.attribute(u"id"_s);
        auto from = el.attribute(u"from"_s);
        auto to = el.attribute(u"to"_s);
        auto lang = el.attributeNS(ns_xml.toString(), u"lang"_s);

        if constexpr (Type == IqType::Get || Type == IqType::Set) {
            if (auto payload = parseOptionalElement<Payload>(el.firstChildElement())) {
                return IqT {
                    std::move(id),
                    std::move(from),
                    std::move(to),
                    std::move(lang),
                    std::move(*payload),
                };
            }
        } else if constexpr (Type == IqType::Result) {
            if constexpr (std::is_void_v<Payload>) {
                return IqT { std::move(id), std::move(from), std::move(to), std::move(lang) };
            } else {
                if (auto payload = parseOptionalElement<Payload>(el.firstChildElement())) {
                    return IqT {
                        std::move(id),
                        std::move(from),
                        std::move(to),
                        std::move(lang),
                        std::move(*payload),
                    };
                }
            }
        } else if constexpr (Type == IqType::Error) {
            auto errorEl = el.lastChildElement();
            auto payloadEl = errorEl.previousSiblingElement();

            if (auto error = parseOptionalElement<QXmppStanza::Error>(errorEl)) {
                return IqT {
                    std::move(id),
                    std::move(from),
                    std::move(to),
                    std::move(lang),
                    parseOptionalElement<Payload>(payloadEl),
                    std::move(*error),
                };
            }
        }
    }
    return {};
}

// parse Iq type from QDomElement or pass error
template<typename Payload>
auto parseIqResponse(std::variant<QDomElement, QXmppError> &&sendResult) -> std::variant<Payload, QXmppError>
{
    using StanzaError = QXmppStanza::Error;
    using Result = std::variant<Payload, QXmppError>;

    return map<Result>(
        [](QDomElement &&el) -> Result {
            auto type = el.attribute(u"type"_s);
            if (type == IqType::Result) {
                if (auto payload = parseElement<Payload>(el.firstChildElement())) {
                    return std::move(*payload);
                } else {
                    return QXmppError {
                        u"Failed to parse IQ result payload"_s,
                        StanzaError(StanzaError::Cancel, StanzaError::UndefinedCondition),
                    };
                }
            } else if (type == IqType::Error) {
                if (auto error = parseElement<StanzaError>(el.lastChildElement())) {
                    return QXmppError { error->text(), std::move(*error) };
                } else {
                    return QXmppError {
                        u"Failed to parse error response"_s,
                        StanzaError(StanzaError::Cancel, StanzaError::UndefinedCondition),
                    };
                }
            } else {
                return QXmppError {
                    u"Received unexpected IQ type"_s,
                    StanzaError(StanzaError::Modify, StanzaError::UnexpectedRequest),
                };
            }
        },
        std::move(sendResult));
}

// For usage with QXmppClient::sendIq()
template<typename Payload>
class CompatIq : public QXmppIq
{
public:
    std::optional<Payload> payload;

    template<IqType Type>
        requires(Type == IqType::Get || Type == IqType::Set)
    CompatIq(Iq<Type, Payload> &&iq) : QXmppIq(), payload(std::move(iq.payload))
    {
        setId(iq.id);
        setFrom(iq.from);
        setTo(iq.to);
        setLang(iq.lang);
        switch (Type) {
        case IqType::Get:
            setType(QXmppIq::Get);
            break;
        case IqType::Set:
            setType(QXmppIq::Set);
            break;
        case IqType::Result:
            setType(QXmppIq::Result);
            break;
        case IqType::Error:
            setType(QXmppIq::Error);
            break;
        }
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

}  // namespace QXmpp::Private

#endif  // IQ_H

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPIQSTANZA_H
#define QXMPPIQSTANZA_H

#include "QXmppGlobal.h"
#include "QXmppStanza.h"
#include "QXmppUtils_p.h"

#include <functional>
#include <memory>
#include <optional>
#include <variant>

#include <QDomElement>
#include <QString>

class QXmlStreamWriter;

namespace QXmpp::Iq {

/*!
    \enum QXmpp::Iq::Type
    \inmodule QXmpp

    Type of an IQ stanza.

    \value Get Request for information.
    \value Set Request changing data.
    \value Result Successful response.
    \value Error Error response.

    \since QXmpp 1.18
*/
enum class Type : uint8_t {
    Get,
    Set,
    Result,
    Error,
};

template<Type T, typename Payload = void>
struct Stanza;

template<typename Payload>
using Get = Stanza<Type::Get, Payload>;

template<typename Payload>
using Set = Stanza<Type::Set, Payload>;

template<typename Payload = void>
using Result = Stanza<Type::Result, Payload>;

template<typename Payload = void>
using Error = Stanza<Type::Error, Payload>;

}  // namespace QXmpp::Iq

namespace QXmpp::Private {

QXMPP_EXPORT void writeIqStart(QXmlStreamWriter *writer, Iq::Type type, const QString &id, const QString &from, const QString &to, const QString &lang);
QXMPP_EXPORT void writeIqEnd(QXmlStreamWriter *writer);
QXMPP_EXPORT bool isIq(const QDomElement &element, Iq::Type type);

// Writes payloads that are only serializable with the internal XmlWriter.
QXMPP_EXPORT void writeXmlWriterPayload(QXmlStreamWriter *writer, const void *payload, void (*write)(XmlWriter &, const void *));

// Writes the payload of an IQ.
template<typename Payload>
struct IqPayloadWriter;

template<QXmlStreamSerializeable Payload>
struct IqPayloadWriter<Payload> {
    static void write(QXmlStreamWriter *writer, const Payload &payload) { payload.toXml(writer); }
};

// Payloads of QXmpp may only be serializable with the internal XmlWriter.
template<XmlWriterSerializeable Payload>
    requires(!QXmlStreamSerializeable<Payload>)
struct IqPayloadWriter<Payload> {
    static void write(QXmlStreamWriter *writer, const Payload &payload)
    {
        writeXmlWriterPayload(writer, &payload, [](XmlWriter &w, const void *p) {
            static_cast<const Payload *>(p)->toXml(w);
        });
    }
};

template<typename Payload>
concept IqPayload = requires(QXmlStreamWriter *writer, const Payload &payload) {
    IqPayloadWriter<Payload>::write(writer, payload);
};

template<typename Payload>
void writeIqPayload(QXmlStreamWriter *writer, const Payload &payload)
{
    if constexpr (std::is_same_v<Payload, std::monostate>) {
        // empty
    } else if constexpr (IsStdOptional<Payload>) {
        if (payload) {
            IqPayloadWriter<typename Payload::value_type>::write(writer, *payload);
        }
    } else {
        IqPayloadWriter<Payload>::write(writer, payload);
    }
}

// Writes the payload of an outgoing IQ: at most one element, exactly one in requests. Empty if
// the IQ has no payload.
using IqPayloadWriterFunction = std::function<void(QXmlStreamWriter *)>;

// Keeps the payload until the IQ is written, without the payload type.
template<typename Payload>
IqPayloadWriterFunction makeIqPayloadWriter(Payload &&payload)
{
    using P = std::decay_t<Payload>;
    if constexpr (std::is_same_v<P, std::monostate>) {
        return {};
    } else {
        // shared, so the writer can be copied without copying the payload
        auto shared = std::make_shared<const P>(std::forward<Payload>(payload));
        return [shared](QXmlStreamWriter *writer) { writeIqPayload(writer, *shared); };
    }
}

// Writes already serialized XML, e.g. the payload of a QXmppIq.
QXMPP_EXPORT IqPayloadWriterFunction makeRawXmlWriter(QByteArray xml);

// An IQ to be sent. The payload is kept type-erased and only serialized when the IQ is written,
// so the IQ can be sent, encrypted or passed on without the original payload type.
struct OutgoingIq {
    Iq::Type type = Iq::Type::Get;
    QString id;
    QString from;
    QString to;
    QString lang;
    IqPayloadWriterFunction payload;
    std::optional<QXmppStanza::Error> error;

    // Serializes the complete IQ.
    QXMPP_EXPORT QByteArray toXml() const;
};

// Writes the child elements of an IQ (the payload and the error). Unlike inside of an IQ, the
// error declares its namespace, so the content can be written elsewhere, e.g. into an SCE
// envelope.
QXMPP_EXPORT void writeIqContent(QXmlStreamWriter *writer, const IqPayloadWriterFunction &payload, const std::optional<QXmppStanza::Error> &error);

}  // namespace QXmpp::Private

namespace QXmpp::Iq {

/*!
    \struct QXmpp::Iq::Stanza
    \inmodule QXmpp

    \brief An IQ stanza of type \c T with a typed payload.

    Use the aliases QXmpp::Iq::Get, QXmpp::Iq::Set, QXmpp::Iq::Result and QXmpp::Iq::Error.

    Requests (get and set) always have exactly one payload. Results may have a payload, errors may
    contain the payload of the request in addition to the error (\rfcsection{6120}{8.2.3}). Parsing
    rejects IQs violating these rules or without an id.

    The stanzas are aggregates, so only the needed fields can be set via designated initializers:
    \code
    QXmpp::Iq::Get<VersionQuery> iq { .to = u"example.org"_s, .payload = VersionQuery {} };
    \endcode

    The payload needs a static \c XmlTag, needs to be parsable from a QDomElement and serializable
    to a QXmlStreamWriter, like the elements of QXmpp::Xml::Registry.

    \since QXmpp 1.18
*/
template<Type T, typename Payload>
struct Stanza {
    static_assert(T == Type::Get || T == Type::Set);
    static_assert(!std::is_void_v<Payload>, "IQ requests require a payload");

    /*! Id of the IQ, required for serialization */
    QString id;
    /*! JID of the sender */
    QString from;
    /*! JID of the recipient */
    QString to;
    /*! Language of the IQ (xml:lang) */
    QString lang;
    /*! Payload of the IQ */
    Payload payload;

    using PayloadType = Payload;
    static constexpr Type StanzaType = T;

    /*!
        Parses the IQ from \a el.

        Returns nothing if the type or the payload does not match, the id is empty or the IQ does
        not contain exactly one child element (\rfcsection{6120}{8.2.3}).
    */
    static std::optional<Stanza> fromDom(const QDomElement &el)
    {
        if (Private::isIq(el, T)) {
            if (auto parsedPayload = Private::parseOptionalElement<Payload>(el.firstChildElement())) {
                return Stanza {
                    el.attribute(QStringLiteral("id")),
                    el.attribute(QStringLiteral("from")),
                    el.attribute(QStringLiteral("to")),
                    el.attributeNS(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("lang")),
                    std::move(*parsedPayload),
                };
            }
        }
        return {};
    }

    /*! Serializes the IQ to \a writer. */
    void toXml(QXmlStreamWriter *writer) const
    {
        Private::writeIqStart(writer, T, id, from, to, lang);
        Private::writeIqPayload(writer, payload);
        Private::writeIqEnd(writer);
    }
};

template<typename Payload>
struct Stanza<Type::Result, Payload> {
    QString id;
    QString from;
    QString to;
    QString lang;
    std::conditional_t<std::is_void_v<Payload>, std::monostate, Payload> payload;

    using PayloadType = Payload;
    static constexpr Type StanzaType = Type::Result;

    static std::optional<Stanza> fromDom(const QDomElement &el)
    {
        if (Private::isIq(el, Type::Result)) {
            auto id = el.attribute(QStringLiteral("id"));
            auto from = el.attribute(QStringLiteral("from"));
            auto to = el.attribute(QStringLiteral("to"));
            auto lang = el.attributeNS(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("lang"));

            if constexpr (std::is_void_v<Payload>) {
                return Stanza { std::move(id), std::move(from), std::move(to), std::move(lang), {} };
            } else if (auto parsedPayload = Private::parseOptionalElement<Payload>(el.firstChildElement())) {
                return Stanza { std::move(id), std::move(from), std::move(to), std::move(lang), std::move(*parsedPayload) };
            }
        }
        return {};
    }

    void toXml(QXmlStreamWriter *writer) const
    {
        Private::writeIqStart(writer, Type::Result, id, from, to, lang);
        Private::writeIqPayload(writer, payload);
        Private::writeIqEnd(writer);
    }
};

template<typename Payload>
struct Stanza<Type::Error, Payload> {
    QString id;
    QString from;
    QString to;
    QString lang;
    std::conditional_t<std::is_void_v<Payload>, std::monostate, std::optional<Payload>> payload;
    QXmppStanza::Error error;

    using PayloadType = Payload;
    static constexpr Type StanzaType = Type::Error;

    static std::optional<Stanza> fromDom(const QDomElement &el)
    {
        if (Private::isIq(el, Type::Error)) {
            auto errorEl = el.lastChildElement();
            auto payloadEl = errorEl.previousSiblingElement();

            if (auto error = Private::parseOptionalElement<QXmppStanza::Error>(errorEl)) {
                Stanza iq {
                    el.attribute(QStringLiteral("id")),
                    el.attribute(QStringLiteral("from")),
                    el.attribute(QStringLiteral("to")),
                    el.attributeNS(QStringLiteral("http://www.w3.org/XML/1998/namespace"), QStringLiteral("lang")),
                    {},
                    std::move(*error),
                };
                if constexpr (!std::is_void_v<Payload>) {
                    iq.payload = Private::parseOptionalElement<Payload>(payloadEl);
                }
                return iq;
            }
        }
        return {};
    }

    void toXml(QXmlStreamWriter *writer) const
    {
        Private::writeIqStart(writer, Type::Error, id, from, to, lang);
        Private::writeIqPayload(writer, payload);
        error.toXml(writer);
        Private::writeIqEnd(writer);
    }
};

/*!
    \struct QXmpp::Iq::DefaultResponse
    \inmodule QXmpp

    \brief Selects the response payload of QXmpp::Iq::ResponseTraits when sending an IQ request.

    \sa QXmppClient::sendIq()

    \since QXmpp 1.18
*/
struct DefaultResponse { };

/*!
    \struct QXmpp::Iq::ResponseTraits
    \inmodule QXmpp

    \brief Defines the payload of the response to the IQ request \c Request.

    The member type \c Payload is the payload of the IQ result, \c void stands for an empty result.
    By default, the result of a get request contains the same payload as the request and the result
    of a set request is empty.

    Specialize the traits for requests with different responses:
    \code
    template<>
    struct QXmpp::Iq::ResponseTraits<QXmpp::Iq::Get<MyQuery>> {
        using Payload = MyQueryResult;
    };
    \endcode

    \sa QXmppClient::sendIq()

    \since QXmpp 1.18
*/
template<typename Request>
struct ResponseTraits;

template<typename RequestPayload>
struct ResponseTraits<Get<RequestPayload>> {
    using Payload = RequestPayload;
};

template<typename RequestPayload>
struct ResponseTraits<Set<RequestPayload>> {
    using Payload = void;
};

}  // namespace QXmpp::Iq

#endif  // QXMPPIQSTANZA_H

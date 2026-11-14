// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPIQHANDLING_H
#define QXMPPIQHANDLING_H

#include "QXmppAsync_p.h"
#include "QXmppClient.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppHooks.h"
#include "QXmppIq.h"
#include "QXmppIqStanza.h"
#include "QXmppUtils_p.h"
#include "QXmppXmlTags_p.h"

#include "StringLiterals.h"

#include <functional>
#include <variant>

#include <QDomElement>

namespace QXmpp::Private {
struct IqRequests;
}

namespace QXmpp::Iq {

/*!
    \class QXmpp::Iq::Context
    \inmodule QXmpp

    Information about an incoming IQ request besides its payload.

    \since QXmpp 1.18
*/
class Context
{
public:
    /*! \internal */
    Context(QString from, std::optional<QXmppE2eeMetadata> e2eeMetadata)
        : m_from(std::move(from)), m_e2eeMetadata(std::move(e2eeMetadata))
    {
    }

    /*! Returns the JID of the sender of the request. */
    const QString &from() const { return m_from; }
    /*!
        Returns the end-to-end encryption metadata of the request, if it was end-to-end encrypted.
        The reply is end-to-end encrypted the same way.
    */
    const std::optional<QXmppE2eeMetadata> &e2eeMetadata() const { return m_e2eeMetadata; }
    /*! Returns whether the request was end-to-end encrypted. */
    bool isEncrypted() const { return m_e2eeMetadata.has_value(); }

private:
    QString m_from;
    std::optional<QXmppE2eeMetadata> m_e2eeMetadata;
};

/*!
    \class QXmpp::Iq::Reply
    \inmodule QXmpp

    Reply to an incoming IQ request: a result with an optional payload or an error.

    Usually, handlers return the payload, QXmpp::Success or a QXmppStanza::Error and the reply is
    created from it.

    \since QXmpp 1.18
*/
class QXMPP_EXPORT Reply
{
public:
    /*! Constructs an empty result. */
    Reply() = default;
    /*! Constructs an error reply with \a error. */
    Reply(QXmppStanza::Error error) : m_error(std::move(error)) { }

    /*! Constructs a result with \a payload. */
    template<Private::IqPayload Payload>
    static Reply result(Payload payload)
    {
        Reply reply;
        reply.m_payload = Private::makeIqPayloadWriter(std::move(payload));
        return reply;
    }

    static Reply encryptionNotSupported();

    /*! Returns the error, if this is an error reply. */
    const std::optional<QXmppStanza::Error> &error() const { return m_error; }

private:
    friend struct Private::IqRequests;

    Private::IqPayloadWriterFunction m_payload;
    std::optional<QXmppStanza::Error> m_error;
};

class EncryptionSelection
{
public:
    /*! \internal */
    EncryptionSelection(Type type, QString to, bool required, QString encryption = {})
        : m_to(std::move(to)), m_encryption(std::move(encryption)), m_type(type), m_required(required)
    {
    }

    /*! Returns the type of the IQ, requests and responses are encrypted. */
    Type type() const { return m_type; }
    /*! Returns the JID of the recipient, the own bare JID for IQs to the own account. */
    const QString &to() const { return m_to; }
    /*!
        Returns whether sending fails if no encryption is selected (see
        QXmppSendStanzaParams::isEncryptionRequired()).
    */
    bool isRequired() const { return m_required; }

    /*!
        Returns the namespace of the encryption (see QXmpp::E2ee), empty for sending the IQ
        unencrypted.
    */
    const QString &encryption() const { return m_encryption; }
    /*! Sets the namespace of the \a encryption. */
    void setEncryption(QStringView encryption) { m_encryption = encryption.toString(); }

private:
    QString m_to;
    QString m_encryption;
    Type m_type;
    bool m_required;
};

struct SelectEncryption {
    static constexpr QStringView Name = u"org.qxmpp.iq.select-encryption";
    using Signature = QXmppTask<void>(EncryptionSelection &selection);
};

}  // namespace QXmpp::Iq

namespace QXmpp::Private {

// Key of IqRequestReceived: the type of the request and the tag name and namespace of its payload
struct IqRequestKey {
    Iq::Type type;
    QString tag;
    QString xmlns;
};

// Keyed hook answering incoming IQ requests of type get and set. Only the handler for the key of
// the request is run, it returns the reply. Typed handlers are added via QXmpp::Iq::Received.
struct IqRequestReceived {
    static constexpr QStringView Name = u"org.qxmpp.iq.request-received";
    using Key = IqRequestKey;
    using Signature = QXmppTask<Iq::Reply>(const QDomElement &iq, const Iq::Context &context);
};

template<>
struct HookKeyTraits<IqRequestKey> {
    static QString toString(const IqRequestKey &key)
    {
        return HookKeyTraits<std::tuple<QString, QString, QString>>::toString({ QString::number(int(key.type)), key.tag, key.xmlns });
    }
};

template<typename T>
struct IsIqReplyVariant : std::false_type { };

template<typename... Types>
struct IsIqReplyVariant<std::variant<Types...>> : std::true_type { };

template<typename T>
Iq::Reply toIqReply(T &&value)
{
    using Value = std::decay_t<T>;
    if constexpr (std::is_same_v<Value, Iq::Reply>) {
        return std::forward<T>(value);
    } else if constexpr (std::is_same_v<Value, QXmppStanza::Error>) {
        return Iq::Reply(std::forward<T>(value));
    } else if constexpr (std::is_same_v<Value, Success>) {
        return Iq::Reply();
    } else if constexpr (IsIqReplyVariant<Value>::value) {
        return std::visit([](auto &&alternative) { return toIqReply(std::forward<decltype(alternative)>(alternative)); },
                          std::forward<T>(value));
    } else if constexpr (IqPayload<Value>) {
        return Iq::Reply::result(Value(std::forward<T>(value)));
    } else {
        static_assert(sizeof(Value) == 0, "Handlers of IQ requests must return a payload, QXmpp::Success, QXmppStanza::Error, QXmpp::Iq::Reply or a std::variant or QXmppTask of them.");
    }
}

template<typename T>
QXmppTask<Iq::Reply> awaitIqReply(QXmppTask<T> task)
{
    if constexpr (std::is_void_v<T>) {
        co_await task;
        co_return Iq::Reply();
    } else {
        co_return toIqReply(co_await task);
    }
}

template<typename T>
QXmppTask<Iq::Reply> toIqReplyTask(T &&value)
{
    if constexpr (IsTask<std::decay_t<T>>) {
        return awaitIqReply(std::forward<T>(value));
    } else {
        return makeReadyHookTask<Iq::Reply>(toIqReply(std::forward<T>(value)));
    }
}

}  // namespace QXmpp::Private

namespace QXmpp::Iq {

/*!
    \struct QXmpp::Iq::Received
    \inmodule QXmpp

    Hook for incoming IQ requests of the type \c IqT, e.g. \c {QXmpp::Iq::Get<Payload>}. The
    handler is only called for requests of this type with the tag name and namespace of
    \c {Payload::XmlTag}.

    \code
    hooks().add<QXmpp::Iq::Received<QXmpp::Iq::Get<VersionQuery>>>([](QXmpp::Iq::Get<VersionQuery> &&iq, const QXmpp::Iq::Context &context) {
        return VersionQuery { u"MyApp"_s, u"1.0"_s };
    });
    \endcode

    The handler gets the request and optionally a QXmpp::Iq::Context, both parameters can also be
    declared as \c auto. It returns the payload of the result, QXmpp::Success for an empty result,
    a QXmppStanza::Error, a QXmpp::Iq::Reply or a \c std::variant of them, also as QXmppTask. The
    reply gets the id of the request and is sent to its sender. Requests whose payload cannot be
    parsed are answered with \c bad-request.

    End-to-end encrypted requests are also passed to the handler and the reply is end-to-end
    encrypted the same way. Handlers not supporting them check QXmpp::Iq::Context::isEncrypted()
    and return QXmpp::Iq::Reply::encryptionNotSupported().

    Handlers can reply asynchronously, e.g. as coroutine:
    \code
    hooks().add<QXmpp::Iq::Received<QXmpp::Iq::Set<MyQuery>>>([this](QXmpp::Iq::Set<MyQuery> &&iq, const QXmpp::Iq::Context &context) -> QXmppTask<QXmpp::Iq::Reply> {
        if (context.isEncrypted()) {
            co_return QXmpp::Iq::Reply::encryptionNotSupported();
        }
        if (!isAllowed(iq.from)) {
            co_return QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Forbidden);
        }
        co_await store(std::move(iq.payload));
        co_return QXmpp::Iq::Reply();
    });
    \endcode

    If the owner of the handler is destroyed before it has replied, the request is answered with
    \c service-unavailable.

    Only one handler per type and payload is used. A handler with a lower sequence number (see
    QXmpp::HookSequence) replaces the handler of another extension. Requests without handler are
    passed to QXmpp::Connection::StanzaReceived.

    \since QXmpp 1.18
*/
template<typename IqT>
    requires(IqT::StanzaType == Type::Get || IqT::StanzaType == Type::Set) && Private::HasXmlTag<typename IqT::PayloadType>
struct Received {
    using Target = Private::IqRequestReceived;

    static Private::IqRequestKey key()
    {
        auto [tag, xmlns] = IqT::PayloadType::XmlTag;
        return Private::IqRequestKey { IqT::StanzaType, QStringView(tag).toString(), QStringView(xmlns).toString() };
    }

    template<typename Handler>
        requires std::invocable<std::decay_t<Handler> &, IqT &&, const Context &> || std::invocable<std::decay_t<Handler> &, IqT &&>
    static std::function<QXmppTask<Reply>(const QDomElement &, const Context &)> adapt(Handler &&handler)
    {
        return [handler = std::forward<Handler>(handler)](const QDomElement &element, const Context &context) mutable -> QXmppTask<Reply> {
            auto iq = IqT::fromDom(element);
            if (!iq) {
                return Private::makeReadyHookTask<Reply>(Reply(QXmppStanza::Error(QXmppStanza::Error::Modify, QXmppStanza::Error::BadRequest)));
            }
            if constexpr (std::invocable<std::decay_t<Handler> &, IqT &&, const Context &>) {
                return Private::toIqReplyTask(std::invoke(handler, std::move(*iq), context));
            } else {
                return Private::toIqReplyTask(std::invoke(handler, std::move(*iq)));
            }
        };
    }
};

}  // namespace QXmpp::Iq

namespace QXmpp {

namespace Private {

QXMPP_EXPORT void sendIqReply(QXmppClient *client,
                              const QString &requestId,
                              const QString &requestFrom,
                              const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                              QXmppIq &&iq);

QXMPP_EXPORT std::tuple<bool, QString, QString> checkIsIqRequest(const QDomElement &el);

template<typename... VariantTypes>
void processHandleIqResult(QXmppClient *client,
                           const QString &requestId,
                           const QString &requestFrom,
                           const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                           std::variant<VariantTypes...> &&result)
{
    std::visit(
        [&]<typename T>(T &&value) {
            static_assert(
                std::is_base_of_v<QXmppIq, T> || std::is_same_v<QXmppStanza::Error, T> || IsTask<T>,
                "QXmppIq based type, QXmppStanza::Error or QXmppTask required");

            if constexpr (std::is_base_of_v<QXmppIq, T>) {
                sendIqReply(client, requestId, requestFrom, e2eeMetadata, std::move(value));
            } else if constexpr (std::is_same_v<QXmppStanza::Error, T>) {
                QXmppIq iq;
                iq.setType(QXmppIq::Error);
                iq.setError(value);
                sendIqReply(client, requestId, requestFrom, e2eeMetadata, std::move(iq));
            } else if constexpr (IsTask<T>) {
                value.then(client, [=](TaskValueType<T> &&v) {
                    processHandleIqResult(client, requestId, requestFrom, e2eeMetadata, std::move(v));
                });
            }
        },
        std::move(result));
}

template<typename T>
void processHandleIqResult(QXmppClient *client,
                           const QString &requestId,
                           const QString &requestFrom,
                           const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                           QXmppTask<T> task)
{
    task.then(client, [client, requestId, requestFrom, e2eeMetadata](T result) {
        processHandleIqResult(client, requestId, requestFrom, e2eeMetadata, result);
    });
}

template<typename T, typename = std::enable_if_t<std::is_base_of_v<QXmppIq, T>, void>>
void processHandleIqResult(QXmppClient *client,
                           const QString &requestId,
                           const QString &requestFrom,
                           const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                           T &&result)
{
    sendIqReply(client, requestId, requestFrom, e2eeMetadata, std::move(result));
}

template<typename Handler, typename IqType>
auto invokeIqHandler(Handler handler, IqType &&iq)
{
    if constexpr (std::is_invocable<Handler, IqType &&>()) {
        return handler(std::move(iq));
    } else {
        return handler->handleIq(std::move(iq));
    }
}

template<typename IqType, typename Handler>
bool handleIqType(Handler handler,
                  QXmppClient *client,
                  const QDomElement &element,
                  const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                  const QString &tagName,
                  const QString &xmlNamespace)
{
    if (isPayloadType<IqType>(std::tuple { tagName, xmlNamespace })) {
        if (auto iq = parseElement<IqType>(element)) {
            if constexpr (std::is_base_of_v<QXmppIq, IqType>) {
                iq->setE2eeMetadata(e2eeMetadata);
            }

            processHandleIqResult(client, element.attribute(u"id"_s), element.attribute(u"from"_s), e2eeMetadata,
                                  invokeIqHandler(std::forward<Handler>(handler), std::move(*iq)));
            return true;
        }
    }
    return false;
}

}  // namespace Private

/*!
    Parses IQ requests, calls a handler and sends an IQ result or error.

    It is the easiest to explain this function with a few examples.

    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq>(element, e2eeMetadata, client, [](QXmppVersionIq iq) -> std::variant<QXmppVersionIq, QXmppStanza::Error> {
        if (iq.type() == QXmppIq::Get) {
            QXmppVersionIq response;
            response.setName(u"MyApp"_s);
            response.setVersion(u"1.0"_s);
            // id, to and type of the IQ are set automatically.
            return response;
        }
        return QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::BadRequest, u"IQ must be of type 'get'."_s);
    });
    \endcode

    It is also possible to handle multiple IQ types.
    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, e2eeMetadata, client, [](std::variant<QXmppVersionIq, QXmppVCardIq> iqVariant) {
        // ...
    });
    \endcode
    It doesn't need to be a std::variant, it's only important that the object is callable with all
    the IQ types. You can for example use different lambdas per type using this 'overloaded' helper.
    \code
    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, e2eeMetadata, client, overloaded {
        [](QXmppVersionIq iq) {
            // ...
        },
        [](QXmppVCardIq iq) {
            // ...
        },
    });
    \endcode

    Another option is to pass an object with \c {handleIq()} functions.
    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, e2eeMetadata, client, this);
    // will call this->handleIq(QXmppVersionIq) or this->handleIq(QXmppVCardIq)
    \endcode

    The return type of the handler function can be:
    \list 1
    \li a QXmppIq based type
    \li a std::variant of QXmppIq based types (multiple are possible) and optionally also
        QXmppStanza::Error
    \li a QXmppTask of 1. or 2.
    \endlist

    You don't need to set the values for id or the to address on the IQ result because that's done
    automatically. Unless you want to return an error IQ you also don't need to set the IQ type.

    If you return an QXmppStanza::Error, a normal QXmppIq with the error will be sent.

    The provided optional QXmppE2eeMetadata is set on the parsed IQ and used to encrypt the IQ
    response like the request.

    \a element is the DOM element that might contain an IQ. \a e2eeMetadata is the end-to-end
    encryption metadata that is used to encrypt the response correctly and to be set on the
    parsed IQ. \a client is the client that should be used to send the response. \a handler is
    a function that can handle the IQ types from the template parameter or an object that has
    handleIq() functions for each of the IQ types.

    Returns whether the IQ could be parsed, handled and a response was or will be sent.

    \since QXmpp 1.5
    \deprecated since QXmpp 1.18, add a handler via QXmpp::Iq::Received, it is called directly
    for its IQ requests:
    \code
    hooks().add<QXmpp::Iq::Received<QXmpp::Iq::Get<VersionQuery>>>([](auto &&) {
        return VersionQuery { u"MyApp"_s, u"1.0"_s };
    });
    \endcode
*/
#if QXMPP_DEPRECATED_SINCE(1, 18)
template<typename... IqTypes, typename Handler>
[[deprecated("Use QXmpp::Iq::Received")]]
bool handleIqRequests(const QDomElement &element,
                      const std::optional<QXmppE2eeMetadata> &e2eeMetadata,
                      QXmppClient *client,
                      Handler handler)
{
    if (auto [isRequest, tagName, xmlns] = Private::checkIsIqRequest(element); isRequest) {
        return (Private::handleIqType<IqTypes>(handler, client, element, e2eeMetadata, tagName, xmlns) || ...);
    }
    return false;
}

/*!
    Parses IQ requests, calls a handler and sends an IQ result or error.

    It is the easiest to explain this function with a few examples.

    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq>(element, client, [](QXmppVersionIq iq) -> std::variant<QXmppVersionIq, QXmppStanza::Error> {
        if (iq.type() == QXmppIq::Get) {
            QXmppVersionIq response;
            response.setName(u"MyApp"_s);
            response.setVersion(u"1.0"_s);
            // id, to and type of the IQ are set automatically.
            return response;
        }
        return QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::BadRequest, u"IQ must be of type 'get'."_s);
    });
    \endcode

    It is also possible to handle multiple IQ types.
    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, client, [](std::variant<QXmppVersionIq, QXmppVCardIq> iqVariant) {
        // ...
    });
    \endcode
    It doesn't need to be a std::variant, it's only important that the object is callable with all
    the IQ types. You can for example use different lambdas per type using this 'overloaded' helper.
    \code
    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, client, overloaded {
        [](QXmppVersionIq iq) {
            // ...
        },
        [](QXmppVCardIq iq) {
            // ...
        },
    });
    \endcode

    Another option is to pass an object with \c {handleIq()} functions.
    \code
    auto handled = QXmpp::handleIqRequests<QXmppVersionIq, QXmppVCardIq>(element, client, this);
    // will call this->handleIq(QXmppVersionIq) or this->handleIq(QXmppVCardIq)
    \endcode

    The return type of the handler function can be:
    \list 1
    \li a QXmppIq based type
    \li a std::variant of QXmppIq based types (multiple are possible) and optionally also
        QXmppStanza::Error
    \li a QXmppTask of 1. or 2.
    \endlist

    You don't need to set the values for id or the to address on the IQ result because that's done
    automatically. Unless you want to return an error IQ you also don't need to set the IQ type.

    If you return an QXmppStanza::Error, a normal QXmppIq with the error will be sent.

    \a element is the DOM element that might contain an IQ. \a client is the client that
    should be used to send the response. \a handler is a function that can handle the IQ types
    from the template parameter or an object that has handleIq() functions for each of the IQ
    types.

    Returns whether the IQ could be parsed, handled and a response was or will be sent.

    \since QXmpp 1.5
    \deprecated since QXmpp 1.18, add a handler via QXmpp::Iq::Received, it is called directly
    for its IQ requests:
    \code
    hooks().add<QXmpp::Iq::Received<QXmpp::Iq::Get<VersionQuery>>>([](auto &&) {
        return VersionQuery { u"MyApp"_s, u"1.0"_s };
    });
    \endcode
*/
template<typename... IqTypes, typename Handler>
[[deprecated("Use QXmpp::Iq::Received")]]
bool handleIqRequests(const QDomElement &element, QXmppClient *client, Handler handler)
{
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    return handleIqRequests<IqTypes...>(element, std::nullopt, client, std::forward<Handler>(handler));
    QT_WARNING_POP
}
#endif

}  // namespace QXmpp

#endif  // QXMPPIQHANDLING_H

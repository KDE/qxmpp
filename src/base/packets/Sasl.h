// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SASL_H
#define SASL_H

#include "QXmppConstants_p.h"

#include "Xml.h"

namespace QXmpp::Private {

namespace Sasl {

enum class ErrorCondition {
    Aborted,
    AccountDisabled,
    CredentialsExpired,
    EncryptionRequired,
    IncorrectEncoding,
    InvalidAuthzid,
    InvalidMechanism,
    MalformedRequest,
    MechanismTooWeak,
    NotAuthorized,
    TemporaryAuthFailure,
};

struct Auth {
    static constexpr std::tuple XmlTag = { u"auth", ns_sasl };
    QString mechanism;
    QByteArray value;
};

struct Challenge {
    static constexpr std::tuple XmlTag = { u"challenge", ns_sasl };
    QByteArray value;
};

struct Failure {
    static constexpr std::tuple XmlTag = { u"failure", ns_sasl };
    std::optional<ErrorCondition> condition;
    QString text;
};

struct Response {
    static constexpr std::tuple XmlTag = { u"response", ns_sasl };
    QByteArray value;
};

struct Success {
    static constexpr std::tuple XmlTag = { u"success", ns_sasl };
};

}  // namespace Sasl

template<>
struct Enums::Data<Sasl::ErrorCondition> {
    using enum Sasl::ErrorCondition;
    static inline constexpr auto Values = makeValues<Sasl::ErrorCondition>({
        { Aborted, u"aborted" },
        { AccountDisabled, u"account-disabled" },
        { CredentialsExpired, u"credentials-expired" },
        { EncryptionRequired, u"encryption-required" },
        { IncorrectEncoding, u"incorrect-encoding" },
        { InvalidAuthzid, u"invalid-authzid" },
        { InvalidMechanism, u"invalid-mechanism" },
        { MalformedRequest, u"malformed-request" },
        { MechanismTooWeak, u"mechanism-too-weak" },
        { NotAuthorized, u"not-authorized" },
        { TemporaryAuthFailure, u"temporary-auth-failure" },
    });
};

template<>
struct XmlSpec<Sasl::Auth> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &Sasl::Auth::mechanism, u"mechanism" },
        XmlOptionalText { &Sasl::Auth::value, Base64Serializer() },
    };
};

template<>
struct XmlSpec<Sasl::Challenge> {
    static constexpr std::tuple Spec = {
        XmlOptionalText { &Sasl::Challenge::value, Base64Serializer() },
    };
};

// Custom parsing with mapping of 'bad-auth'
struct SaslFailureConditionSerializer {
    std::optional<Sasl::ErrorCondition> parse(const QString &s) const
    {
        auto e = Enums::fromString<Sasl::ErrorCondition>(s);
        if (!e) {
            // RFC3920 defines the error condition as "not-authorized", but
            // some broken servers use "bad-auth" instead. We tolerate this
            // by remapping the error to "not-authorized".
            if (s == u"bad-auth") {
                e = Sasl::ErrorCondition::NotAuthorized;
            } else {
                throw InvalidValueError("Sasl::ErrorCondition", s);
            }
        }
        return e;
    }
    auto serialize(std::optional<Sasl::ErrorCondition> value) const { return Enums::toString(value.value()); }
    bool hasValue(std::optional<Sasl::ErrorCondition> value) const { return value.has_value(); }
    auto defaultValue() const { return std::optional<Sasl::ErrorCondition>(); }
};

template<>
struct XmlSpec<Sasl::Failure> {
    static constexpr std::tuple Spec = {
        XmlOptionalEnumElement { &Sasl::Failure::condition, ns_sasl, SaslFailureConditionSerializer() },
        XmlOptionalTextElement { &Sasl::Failure::text, u"text" },
    };
};

template<>
struct XmlSpec<Sasl::Response> {
    static constexpr std::tuple Spec = {
        XmlOptionalText { &Sasl::Response::value, Base64Serializer() },
    };
};

template<>
struct XmlSpec<Sasl::Success> {
    static constexpr std::tuple Spec = {};
};

}  // namespace QXmpp::Private

#endif  // SASL_H

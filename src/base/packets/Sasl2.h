// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef SASL2_H
#define SASL2_H

#include "Bind2.h"
#include "Fast.h"
#include "Sasl.h"

namespace QXmpp::Private {

namespace Sasl2 {

struct StreamFeature {
    static constexpr std::tuple XmlTag = { u"authentication", ns_sasl_2 };
    QList<QString> mechanisms;
    std::optional<Bind2Feature> bind2Feature;
    std::optional<FastFeature> fast;
    bool streamResumptionAvailable = false;
};

struct UserAgent {
    static constexpr std::tuple XmlTag = { u"user-agent", ns_sasl_2 };
    QUuid id;
    QString software;
    QString device;
};

struct Authenticate {
    static constexpr std::tuple XmlTag = { u"authenticate", ns_sasl_2 };
    QString mechanism;
    QByteArray initialResponse;
    std::optional<UserAgent> userAgent;
    std::optional<Bind2Request> bindRequest;
    std::optional<SmResume> smResume;
    std::optional<FastTokenRequest> tokenRequest;
    std::optional<FastRequest> fast;
};

struct Challenge {
    static constexpr std::tuple XmlTag = { u"challenge", ns_sasl_2 };
    QByteArray data;
};

struct Response {
    static constexpr std::tuple XmlTag = { u"response", ns_sasl_2 };
    QByteArray data;
};

struct Success {
    static constexpr std::tuple XmlTag = { u"success", ns_sasl_2 };
    std::optional<QByteArray> additionalData;
    QString authorizationIdentifier;
    // extensions
    std::optional<Bind2Bound> bound;
    std::optional<SmResumed> smResumed;
    std::optional<SmFailed> smFailed;
    std::optional<FastToken> token;
};

struct Failure {
    static constexpr std::tuple XmlTag = { u"failure", ns_sasl_2 };
    Sasl::ErrorCondition condition;
    QString text;
    // extensions
};

struct Continue {
    static constexpr std::tuple XmlTag = { u"continue", ns_sasl_2 };
    QByteArray additionalData;
    std::vector<QString> tasks;
    QString text;
};

struct Abort {
    static constexpr std::tuple XmlTag = { u"abort", ns_sasl_2 };
    QString text;
};

}  // namespace Sasl2

template<>
struct XmlSpec<Sasl2::StreamFeature> {
    static constexpr std::tuple Spec = {
        XmlTextElements { &Sasl2::StreamFeature::mechanisms, u"mechanism" },
        XmlElement {
            u"inline",
            Optional(),
            XmlReference { &Sasl2::StreamFeature::bind2Feature },
            XmlReference { &Sasl2::StreamFeature::fast },
            XmlReference { &Sasl2::StreamFeature::streamResumptionAvailable, { u"sm", ns_stream_management } },
        },
    };
};

template<>
struct XmlSpec<Sasl2::UserAgent> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &Sasl2::UserAgent::id, u"id" },
        XmlOptionalTextElement { &Sasl2::UserAgent::software, u"software" },
        XmlOptionalTextElement { &Sasl2::UserAgent::device, u"device" },
    };
};

template<>
struct XmlSpec<Sasl2::Authenticate> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &Sasl2::Authenticate::mechanism, u"mechanism" },
        XmlOptionalTextElement { &Sasl2::Authenticate::initialResponse, u"initial-response", Base64Serializer() },
        XmlReference { &Sasl2::Authenticate::userAgent },
        XmlReference { &Sasl2::Authenticate::bindRequest },
        XmlReference { &Sasl2::Authenticate::smResume },
        XmlReference { &Sasl2::Authenticate::tokenRequest },
        XmlReference { &Sasl2::Authenticate::fast },
    };
};

template<>
struct XmlSpec<Sasl2::Challenge> {
    static constexpr std::tuple Spec = {
        XmlText { &Sasl2::Challenge::data, Base64Serializer() },
    };
};

template<>
struct XmlSpec<Sasl2::Response> {
    static constexpr std::tuple Spec = {
        XmlText { &Sasl2::Response::data, Base64Serializer() },
    };
};

template<>
struct XmlSpec<Sasl2::Success> {
    static constexpr std::tuple Spec = {
        XmlOptionalTextElement { &Sasl2::Success::additionalData, u"additional-data", OptionalBase64Serializer() },
        XmlTextElement { &Sasl2::Success::authorizationIdentifier, u"authorization-identifier" },
        XmlReference { &Sasl2::Success::bound },
        XmlReference { &Sasl2::Success::smResumed },
        XmlReference { &Sasl2::Success::smFailed },
        XmlReference { &Sasl2::Success::token },
    };
};

template<>
struct XmlSpec<Sasl2::Failure> {
    static constexpr std::tuple Spec = {
        XmlEnumElement { &Sasl2::Failure::condition, ns_sasl },
        XmlOptionalTextElement { &Sasl2::Failure::text, u"text" },
    };
};

template<>
struct XmlSpec<Sasl2::Continue> {
    static constexpr std::tuple Spec = {
        XmlOptionalTextElement { &Sasl2::Continue::additionalData, u"additional-data", Base64Serializer() },
        XmlElement {
            u"tasks",
            Required(),
            XmlTextElements { &Sasl2::Continue::tasks, u"task" },
        },
        XmlOptionalTextElement { &Sasl2::Continue::text, u"text" },
    };
};

template<>
struct XmlSpec<Sasl2::Abort> {
    static constexpr std::tuple Spec = {
        XmlOptionalTextElement { &Sasl2::Abort::text, u"text" },
    };
};

}  // namespace QXmpp::Private

#endif  // SASL2_H

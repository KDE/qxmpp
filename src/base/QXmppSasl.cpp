// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppSasl2UserAgent.h"
#include "QXmppSasl_p.h"
#include "QXmppUtils.h"
#include "QXmppVisitHelper_p.h"

#include "Algorithms.h"
#include "StringLiterals.h"
#include "Xml.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>
#include <QUrlQuery>
#include <QXmlStreamWriter>
#include <QtEndian>

using std::visit;
using namespace QXmpp::Private;

static QByteArray forcedNonce;

// https://www.iana.org/assignments/named-information/named-information.xhtml#hash-alg
template<>
struct Enums::Data<IanaHashAlgorithm> {
    using enum IanaHashAlgorithm;
    static constexpr auto Values = makeValues<IanaHashAlgorithm>({
        { Sha256, u"SHA-256" },
        { Sha384, u"SHA-384" },
        { Sha512, u"SHA-512" },
        { Sha3_224, u"SHA3-224" },
        { Sha3_256, u"SHA3-256" },
        { Sha3_384, u"SHA3-384" },
        { Sha3_512, u"SHA3-512" },
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        { Blake2s_256, u"BLAKE2S-256" },
        { Blake2b_256, u"BLAKE2B-256" },
        { Blake2b_512, u"BLAKE2B-512" },
#endif
    });
};

template<>
struct Enums::Data<SaslScramMechanism::Algorithm> {
    using enum SaslScramMechanism::Algorithm;
    static constexpr auto Values = makeValues<SaslScramMechanism::Algorithm>({
        { Sha1, u"SCRAM-SHA-1" },
        { Sha256, u"SCRAM-SHA-256" },
        { Sha512, u"SCRAM-SHA-512" },
        { Sha3_512, u"SCRAM-SHA3-512" },
    });
};

template<>
struct Enums::Data<SaslHtMechanism::ChannelBindingType> {
    using enum SaslHtMechanism::ChannelBindingType;
    static constexpr auto Values = makeValues<SaslHtMechanism::ChannelBindingType>({
        { TlsServerEndpoint, u"ENDP" },
        { TlsUnique, u"UNIQ" },
        { TlsExporter, u"EXPR" },
        { None, u"NONE" },
    });
};

namespace QXmpp::Private {

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

template<>
struct XmlSpec<Bind2Feature> {
    static constexpr std::tuple Spec = {
        XmlElement {
            u"inline",
            XmlSingleAttributeElements { &Bind2Feature::features, Tag { u"feature", ns_bind2 }, u"var" },
        },
    };
};

template<>
struct XmlSpec<Bind2Request> {
    static constexpr std::tuple Spec = {
        XmlElement { { u"tag", ns_bind2 }, Optional(), XmlText { &Bind2Request::tag } },
    };
};

namespace Sasl {

std::optional<Auth> Auth::fromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<Auth>(el); }

void Auth::toXml(XmlWriter &writer) const { XmlSpecSerializer::serialize(writer, *this); }

std::optional<Challenge> Challenge::fromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<Challenge>(el); }

void Challenge::toXml(XmlWriter &writer) const { XmlSpecSerializer::serialize(writer, *this); }

std::optional<Failure> Failure::fromDom(const QDomElement &el)
{
    if (!isElementType<Failure>(el)) {
        return {};
    }

    auto errorConditionString = el.firstChildElement().tagName();

    Failure failure {
        Enums::fromString<ErrorCondition>(errorConditionString),
        el.firstChildElement(u"text"_s).text(),
    };

    // RFC3920 defines the error condition as "not-authorized", but
    // some broken servers use "bad-auth" instead. We tolerate this
    // by remapping the error to "not-authorized".
    if (!failure.condition && errorConditionString == u"bad-auth") {
        failure.condition = ErrorCondition::NotAuthorized;
    }

    return failure;
}

void Failure::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        OptionalEnumElement { condition },
        OptionalContent {
            !text.isEmpty(),
            Element { u"text", Attribute { u"xml:lang", u"en" }, Characters { text } },
        },
    });
}

std::optional<Response> Response::fromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<Response>(el); }
void Response::toXml(XmlWriter &writer) const { XmlSpecSerializer::serialize(writer, *this); }

std::optional<Success> Success::fromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<Success>(el); }
void Success::toXml(XmlWriter &writer) const { XmlSpecSerializer::serialize(writer, *this); }

}  // namespace Sasl

std::optional<Bind2Feature> Bind2Feature::fromDom(const QDomElement &el) { return XmlSpecParser::fromDomImpl<Bind2Feature>(el); }
void Bind2Feature::toXml(XmlWriter &writer) const { return XmlSpecSerializer::serialize(writer, *this); }

std::optional<Bind2Request> Bind2Request::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"bind" || el.namespaceURI() != ns_bind2) {
        return {};
    }

    return Bind2Request {
        firstChildElement(el, u"tag", ns_bind2).text(),
        !firstChildElement(el, u"inactive", ns_csi).isNull(),
        !firstChildElement(el, u"enable", ns_carbons).isNull(),
        parseOptionalChildElement<SmEnable>(el),
    };
}

void Bind2Request::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        OptionalTextElement { u"tag", tag },
        OptionalContent { csiInactive, Element { { u"inactive", ns_csi } } },
        OptionalContent { carbonsEnable, Element { { u"enable", ns_carbons } } },
        smEnable,
    });
}

std::optional<Bind2Bound> Bind2Bound::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"bound" || el.namespaceURI() != ns_bind2) {
        return {};
    }

    return Bind2Bound {
        .smFailed = parseOptionalChildElement<SmFailed>(el),
        .smEnabled = parseOptionalChildElement<SmEnabled>(el),
    };
}

void Bind2Bound::toXml(XmlWriter &writer) const
{
    writer.write(Element { XmlTag, smFailed, smEnabled });
}

std::optional<FastFeature> FastFeature::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"fast" || el.namespaceURI() != ns_fast) {
        return {};
    }

    return FastFeature {
        .mechanisms = parseTextElements<std::vector<QString>>(el, u"mechanism", ns_fast),
        .tls0rtt = parseBoolean(el.attribute(QStringLiteral("tls-0rtt"))).value_or(false),
    };
}

void FastFeature::toXml(XmlWriter &writer) const
{
    writer.write(Element { XmlTag, TextElements { u"mechanism", mechanisms } });
}

std::optional<FastTokenRequest> FastTokenRequest::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"request-token" || el.namespaceURI() != ns_fast) {
        return {};
    }
    return FastTokenRequest { el.attribute(QStringLiteral("mechanism")) };
}

void FastTokenRequest::toXml(XmlWriter &writer) const
{
    writer.write(Element { XmlTag, Attribute { u"mechanism", mechanism } });
}

std::optional<FastToken> FastToken::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"token" || el.namespaceURI() != ns_fast) {
        return {};
    }

    return FastToken {
        QXmppUtils::datetimeFromString(el.attribute(QStringLiteral("expiry"))),
        el.attribute(QStringLiteral("token")),
    };
}

void FastToken::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        Attribute { u"expiry", expiry },
        Attribute { u"token", token },
    });
}

std::optional<FastRequest> FastRequest::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"fast" || el.namespaceURI() != ns_fast) {
        return {};
    }
    return FastRequest {
        parseInt<uint64_t>(el.attribute(QStringLiteral("count"))),
        parseBoolean(el.attribute(QStringLiteral("invalidate"))).value_or(false),
    };
}

void FastRequest::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        OptionalAttribute { u"count", count },
        OptionalAttribute { u"invalidate", DefaultedBool { invalidate, false } },
    });
}

namespace Sasl2 {

std::optional<StreamFeature> StreamFeature::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"authentication" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    StreamFeature feature;
    feature.mechanisms = parseTextElements<QList<QString>>(el, u"mechanism", ns_sasl_2);
    if (auto inlineEl = firstChildElement(el, u"inline", ns_sasl_2); !inlineEl.isNull()) {
        feature.bind2Feature = parseOptionalChildElement<Bind2Feature>(inlineEl);
        feature.fast = parseOptionalChildElement<FastFeature>(inlineEl);
        feature.streamResumptionAvailable = hasChild<SmFeature>(inlineEl);
    }
    return feature;
}

void StreamFeature::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        TextElements { u"mechanism", mechanisms },
        OptionalContent {
            bind2Feature || fast || streamResumptionAvailable,
            Element {
                u"inline",
                bind2Feature,
                fast,
                OptionalContent { streamResumptionAvailable, Element { { u"sm", ns_stream_management } } },
            },
        },
    });
}

std::optional<UserAgent> UserAgent::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"user-agent" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    return UserAgent {
        QUuid::fromString(el.attribute(u"id"_s)),
        firstChildElement(el, u"software", ns_sasl_2).text(),
        firstChildElement(el, u"device", ns_sasl_2).text(),
    };
}

void UserAgent::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        u"user-agent",
        OptionalAttribute { u"id", id },
        OptionalTextElement { u"software", software },
        OptionalTextElement { u"device", device },
    });
}

std::optional<Authenticate> Authenticate::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"authenticate" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }
    return Authenticate {
        el.attribute(u"mechanism"_s),
        parseBase64(firstChildElement(el, u"initial-response", ns_sasl_2).text()).value_or(QByteArray()),
        parseOptionalChildElement<UserAgent>(el),
        parseOptionalChildElement<Bind2Request>(el),
        parseOptionalChildElement<SmResume>(el),
        parseOptionalChildElement<FastTokenRequest>(el),
        parseOptionalChildElement<FastRequest>(el),
    };
}

void Authenticate::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        Attribute { u"mechanism", mechanism },
        OptionalTextElement { u"initial-response", Base64 { initialResponse } },
        userAgent,
        bindRequest,
        smResume,
        tokenRequest,
        fast,
    });
}

std::optional<Challenge> Challenge::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"challenge" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    if (auto result = parseBase64(el.text())) {
        return Challenge { *result };
    }
    return {};
}

void Challenge::toXml(XmlWriter &writer) const
{
    writer.write(TextElement { XmlTag, Base64 { data } });
}

std::optional<Response> Response::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"response" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    if (auto result = parseBase64(el.text())) {
        return Response { *result };
    }
    return {};
}

void Response::toXml(XmlWriter &writer) const
{
    writer.write(TextElement { XmlTag, Base64 { data } });
}

std::optional<Success> Success::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"success" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    Success output;

    if (auto subEl = firstChildElement(el, u"additional-data", ns_sasl_2); !subEl.isNull()) {
        if (auto result = parseBase64(subEl.text())) {
            output.additionalData = *result;
        } else {
            // invalid base64 data
            return {};
        }
    }

    output.authorizationIdentifier = firstChildElement(el, u"authorization-identifier", ns_sasl_2).text();
    output.bound = parseOptionalChildElement<Bind2Bound>(el);
    output.smResumed = parseOptionalChildElement<SmResumed>(el);
    output.smFailed = parseOptionalChildElement<SmFailed>(el);
    output.token = parseOptionalChildElement<FastToken>(el);

    return output;
}

void Success::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        OptionalTextElement { u"additional-data", into<Base64>(additionalData) },
        TextElement { u"authorization-identifier", authorizationIdentifier },
        bound,
        smResumed,
        smFailed,
        token,
    });
}

std::optional<Failure> Failure::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"failure" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    // SASL error condition
    auto condition = Enums::fromString<Sasl::ErrorCondition>(firstChildElement(el, {}, ns_sasl).tagName());
    if (!condition) {
        return {};
    }

    return Failure {
        condition.value(),
        firstChildElement(el, u"text", ns_sasl_2).text(),
    };
}

void Failure::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        Element { Tag { condition, ns_sasl } },
        OptionalTextElement { u"text", text },
    });
}

std::optional<Continue> Continue::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"continue" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }

    Continue output;

    if (auto subEl = firstChildElement(el, u"additional-data", ns_sasl_2); !subEl.isNull()) {
        if (auto result = parseBase64(subEl.text())) {
            output.additionalData = *result;
        } else {
            // invalid base64 data
            return {};
        }
    }

    output.tasks = parseTextElements<std::vector<QString>>(firstChildElement(el, u"tasks", ns_sasl_2), u"task", ns_sasl_2);
    // tasks are mandatory
    if (output.tasks.empty()) {
        return {};
    }

    output.text = firstChildElement(el, u"text", ns_sasl_2).text();

    return output;
}

void Continue::toXml(XmlWriter &writer) const
{
    writer.write(Element {
        XmlTag,
        OptionalTextElement { u"additional-data", Base64 { additionalData } },
        Element {
            u"tasks",
            TextElements { u"task", tasks },
        },
        OptionalTextElement { u"text", text },
    });
}

std::optional<Abort> Abort::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"abort" || el.namespaceURI() != ns_sasl_2) {
        return {};
    }
    return Abort { firstChildElement(el, u"text", ns_sasl_2).text() };
}

void Abort::toXml(XmlWriter &writer) const
{
    writer.write(Element { XmlTag, OptionalTextElement { u"text", text } });
}

}  // namespace Sasl2

QCryptographicHash::Algorithm ianaHashAlgorithmToQt(IanaHashAlgorithm alg)
{
#define CASE(_algorithm)                \
    case IanaHashAlgorithm::_algorithm: \
        return QCryptographicHash::_algorithm;

    switch (alg) {
        CASE(Sha256)
        CASE(Sha384)
        CASE(Sha512)
        CASE(Sha3_224)
        CASE(Sha3_256)
        CASE(Sha3_384)
        CASE(Sha3_512)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        CASE(Blake2s_256)
        CASE(Blake2b_256)
        CASE(Blake2b_512)
#endif
    }
    Q_UNREACHABLE();
#undef CASE
}

std::optional<SaslScramMechanism> SaslScramMechanism::fromString(QStringView string)
{
    return into<SaslScramMechanism>(Enums::fromString<Algorithm>(string));
}

QString SaslScramMechanism::toString() const
{
    return Enums::toString(algorithm).toString();
}

QCryptographicHash::Algorithm SaslScramMechanism::qtAlgorithm() const
{
    switch (algorithm) {
    case Sha1:
        return QCryptographicHash::Sha1;
    case Sha256:
        return QCryptographicHash::Sha256;
    case Sha512:
        return QCryptographicHash::Sha512;
    case Sha3_512:
        return QCryptographicHash::Sha3_512;
    }
    Q_UNREACHABLE();
}

std::optional<SaslHtMechanism> SaslHtMechanism::fromString(QStringView string)
{
    // prefix
    static constexpr QStringView prefix = u"HT-";
    if (!string.startsWith(prefix)) {
        return {};
    }
    string = string.mid(prefix.size());

    // hash algorithm
    // C++23: use enumerate view
    using enum IanaHashAlgorithm;
    std::optional<IanaHashAlgorithm> algorithm;
    for (int i = int(Sha256); i <= int(_End); ++i) {
        auto algorithmString = Enums::toString(IanaHashAlgorithm(i));
        if (string.startsWith(algorithmString)) {
            algorithm = IanaHashAlgorithm(i);
            string = string.mid(algorithmString.size());
        }
    }
    if (!algorithm) {
        return {};
    }

    if (!string.startsWith(u'-')) {
        return {};
    }
    string = string.mid(1);

    // channel-binding type
    if (auto cbType = Enums::fromString<ChannelBindingType>(string)) {
        return SaslHtMechanism { *algorithm, *cbType };
    }
    return {};
}

QString SaslHtMechanism::toString() const
{
    return u"HT-" + hashAlgorithm + u'-' + channelBindingType;
}

std::optional<SaslMechanism> SaslMechanism::fromString(QStringView str)
{
    if (str.startsWith(u"SCRAM-")) {
        return into<SaslMechanism>(SaslScramMechanism::fromString(str));
    }
    if (str.startsWith(u"HT-")) {
        return into<SaslMechanism>(SaslHtMechanism::fromString(str));
    }
    if (str == u"DIGEST-MD5") {
        return { { SaslDigestMd5Mechanism() } };
    }
    if (str == u"PLAIN") {
        return { { SaslPlainMechanism() } };
    }
    if (str == u"ANONYMOUS") {
        return { { SaslAnonymousMechanism() } };
    }
    if (str == u"X-FACEBOOK-PLATFORM") {
        return { { SaslXFacebookMechanism() } };
    }
    if (str == u"X-MESSENGER-OAUTH2") {
        return { { SaslXWindowsLiveMechanism() } };
    }
    if (str == u"X-OAUTH2") {
        return { { SaslXGoogleMechanism() } };
    }
    return {};
}

QString SaslMechanism::toString() const
{
    return visit(
        overloaded {
            [](SaslScramMechanism scram) { return scram.toString(); },
            [](SaslHtMechanism ht) { return ht.toString(); },
            [](SaslDigestMd5Mechanism) { return u"DIGEST-MD5"_s; },
            [](SaslPlainMechanism) { return u"PLAIN"_s; },
            [](SaslAnonymousMechanism) { return u"ANONYMOUS"_s; },
            [](SaslXFacebookMechanism) { return u"X-FACEBOOK-PLATFORM"_s; },
            [](SaslXWindowsLiveMechanism) { return u"X-MESSENGER-OAUTH2"_s; },
            [](SaslXGoogleMechanism) { return u"X-OAUTH2"_s; },
        },
        *this);
}

std::optional<HtToken> HtToken::fromXml(QXmlStreamReader &r)
{
    if (r.name() != u"ht-token" || r.namespaceUri() != ns_qxmpp_credentials) {
        return {};
    }
    const auto &attrs = r.attributes();
    if (auto mechanism = SaslHtMechanism::fromString(attrs.value("mechanism"_L1))) {
        return HtToken {
            *mechanism,
            attrs.value("secret"_L1).toString(),
            QXmppUtils::datetimeFromString(toString60(attrs.value("expiry"_L1))),
        };
    }
    return {};
}

void HtToken::toXml(XmlWriter &w) const
{
    w.write(Element {
        u"ht-token",
        Attribute { u"mechanism", mechanism.toString() },
        Attribute { u"secret", secret },
        Attribute { u"expiry", expiry },
    });
}

}  // namespace QXmpp::Private

///
/// \class QXmppSasl2UserAgent
///
/// \brief User-agent for identifying devices across reconnects, defined in \xep{0388, Extensible
/// SASL Profile}.
///
/// \since QXmpp 1.7
///

struct QXmppSasl2UserAgentPrivate : QSharedData, Sasl2::UserAgent { };

/// Default-constructor
QXmppSasl2UserAgent::QXmppSasl2UserAgent()
    : d(new QXmppSasl2UserAgentPrivate())
{
}

/// Constructs a new user-agent with given values.
QXmppSasl2UserAgent::QXmppSasl2UserAgent(QUuid deviceId, const QString &softwareName, const QString &deviceName)
    : d(new QXmppSasl2UserAgentPrivate { QSharedData(), Sasl2::UserAgent { deviceId, softwareName, deviceName } })
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppSasl2UserAgent)

///
/// Returns the unique and stable ID of this device.
///
/// This ID is intended to be persistent across reconnects and reboots of the used device.
///
QUuid QXmppSasl2UserAgent::deviceId() const
{
    return d->id;
}

///
/// Sets the unique and stable ID of this device.
///
/// This ID is intended to be persistent across reconnects and reboots of the used device.
///
void QXmppSasl2UserAgent::setDeviceId(QUuid id)
{
    d->id = id;
}

///
/// Returns the name of the used software (e.g. *AwesomeXMPP*).
///
const QString &QXmppSasl2UserAgent::softwareName() const
{
    return d->software;
}

///
/// Sets the name of the used software (e.g. *AwesomeXMPP*).
///
void QXmppSasl2UserAgent::setSoftwareName(const QString &software)
{
    d->software = software;
}

///
/// Returns the name of this device (e.g. *Kiva's Phone*).
///
const QString &QXmppSasl2UserAgent::deviceName() const
{
    return d->device;
}

///
/// Sets the name of this device (e.g. *Kiva's Phone*).
///
void QXmppSasl2UserAgent::setDeviceName(const QString &device)
{
    d->device = device;
}

// Calculate digest response for use with XMPP/SASL.

static QByteArray calculateDigest(const QByteArray &method, const QByteArray &digestUri, const QByteArray &secret, const QByteArray &nonce, const QByteArray &cnonce, const QByteArray &nc)
{
    const QByteArray A1 = secret + ':' + nonce + ':' + cnonce;
    const QByteArray A2 = method + ':' + digestUri;

    QByteArray HA1 = QCryptographicHash::hash(A1, QCryptographicHash::Md5).toHex();
    QByteArray HA2 = QCryptographicHash::hash(A2, QCryptographicHash::Md5).toHex();
    const QByteArray KD = HA1 + ':' + nonce + ':' + nc + ':' + cnonce + ":auth:" + HA2;
    return QCryptographicHash::hash(KD, QCryptographicHash::Md5).toHex();
}

static QByteArray generateNonce()
{
    if (!forcedNonce.isEmpty()) {
        return forcedNonce;
    }

    QByteArray nonce = QXmppUtils::generateRandomBytes(32);

    // The random data can the '=' char is not valid as it is a delimiter,
    // so to be safe, base64 the nonce
    return nonce.toBase64();
}

static QMap<char, QByteArray> parseGS2(const QByteArray &ba)
{
    QMap<char, QByteArray> map;
    const auto keyValuePairs = ba.split(u',');
    for (const auto &keyValue : keyValuePairs) {
        if (keyValue.size() >= 2 && keyValue[1] == '=') {
            map[keyValue[0]] = keyValue.mid(2);
        }
    }
    return map;
}

bool QXmppSaslClient::isMechanismAvailable(SaslMechanism mechanism, const Credentials &credentials)
{
    return visit(
        overloaded {
            [&](SaslHtMechanism ht) {
                return credentials.htToken &&
                    credentials.htToken->mechanism == ht &&
                    ht.channelBindingType == SaslHtMechanism::None;
            },
            [&](std::variant<SaslScramMechanism, SaslDigestMd5Mechanism, SaslPlainMechanism>) {
                return !credentials.password.isEmpty();
            },
            [&](SaslXFacebookMechanism) {
                return !credentials.facebookAccessToken.isEmpty() && !credentials.facebookAppId.isEmpty();
            },
            [&](SaslXWindowsLiveMechanism) {
                return !credentials.windowsLiveAccessToken.isEmpty();
            },
            [&](SaslXGoogleMechanism) {
                return !credentials.googleAccessToken.isEmpty();
            },
            [](SaslAnonymousMechanism) {
                return true;
            } },
        mechanism);
}

///
/// Creates an SASL client for the given mechanism.
///
std::unique_ptr<QXmppSaslClient> QXmppSaslClient::create(const QString &string, QObject *parent)
{
    if (auto mechanism = SaslMechanism::fromString(string)) {
        return create(*mechanism, parent);
    }
    return nullptr;
}

std::unique_ptr<QXmppSaslClient> QXmppSaslClient::create(SaslMechanism mechanism, QObject *parent)
{
    return visit<std::unique_ptr<QXmppSaslClient>>(
        overloaded {
            [&](SaslScramMechanism scram) {
                return std::make_unique<QXmppSaslClientScram>(scram, parent);
            },
            [&](SaslHtMechanism ht) {
                return std::make_unique<QXmppSaslClientHt>(ht, parent);
            },
            [&](SaslPlainMechanism) {
                return std::make_unique<QXmppSaslClientPlain>(parent);
            },
            [&](SaslDigestMd5Mechanism) {
                return std::make_unique<QXmppSaslClientDigestMd5>(parent);
            },
            [&](SaslAnonymousMechanism) {
                return std::make_unique<QXmppSaslClientAnonymous>(parent);
            },
            [&](SaslXFacebookMechanism) {
                return std::make_unique<QXmppSaslClientFacebook>(parent);
            },
            [&](SaslXWindowsLiveMechanism) {
                return std::make_unique<QXmppSaslClientWindowsLive>(parent);
            },
            [&](SaslXGoogleMechanism) {
                return std::make_unique<QXmppSaslClientGoogle>(parent);
            },
        },
        mechanism);
}

QXmppSaslClientAnonymous::QXmppSaslClientAnonymous(QObject *parent)
    : QXmppSaslClient(parent), m_step(0)
{
}

std::optional<QByteArray> QXmppSaslClientAnonymous::respond(const QByteArray &)
{
    if (m_step == 0) {
        m_step++;
        return QByteArray();
    } else {
        warning(u"QXmppSaslClientAnonymous : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientDigestMd5::QXmppSaslClientDigestMd5(QObject *parent)
    : QXmppSaslClient(parent), m_nc(QByteArrayLiteral("00000001")), m_step(0)
{
    m_cnonce = generateNonce();
}

void QXmppSaslClientDigestMd5::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_password = credentials.password;
}

std::optional<QByteArray> QXmppSaslClientDigestMd5::respond(const QByteArray &challenge)
{
    const QByteArray digestUri = u"%1/%2"_s.arg(serviceType(), host()).toUtf8();

    if (m_step == 0) {
        m_step++;
        return QByteArray();
    } else if (m_step == 1) {
        const QMap<QByteArray, QByteArray> input = QXmppSaslDigestMd5::parseMessage(challenge);

        if (!input.contains(QByteArrayLiteral("nonce"))) {
            warning(u"QXmppSaslClientDigestMd5 : Invalid input on step 1"_s);
            return {};
        }

        // determine realm
        const QByteArray realm = input.value(QByteArrayLiteral("realm"));

        // determine quality of protection
        const QList<QByteArray> qops = input.value(QByteArrayLiteral("qop"), QByteArrayLiteral("auth")).split(',');
        if (!qops.contains(QByteArrayLiteral("auth"))) {
            warning(u"QXmppSaslClientDigestMd5 : Invalid quality of protection"_s);
            return {};
        }

        m_nonce = input.value(QByteArrayLiteral("nonce"));
        m_secret = QCryptographicHash::hash(
            QByteArray(username().toUtf8() + QByteArrayLiteral(":") + realm + QByteArrayLiteral(":") + m_password.toUtf8()),
            QCryptographicHash::Md5);

        // Build response
        QMap<QByteArray, QByteArray> output;
        output[QByteArrayLiteral("username")] = username().toUtf8();
        if (!realm.isEmpty()) {
            output[QByteArrayLiteral("realm")] = realm;
        }
        output[QByteArrayLiteral("nonce")] = m_nonce;
        output[QByteArrayLiteral("qop")] = QByteArrayLiteral("auth");
        output[QByteArrayLiteral("cnonce")] = m_cnonce;
        output[QByteArrayLiteral("nc")] = m_nc;
        output[QByteArrayLiteral("digest-uri")] = digestUri;
        output[QByteArrayLiteral("response")] = calculateDigest(QByteArrayLiteral("AUTHENTICATE"), digestUri, m_secret, m_nonce, m_cnonce, m_nc);
        output[QByteArrayLiteral("charset")] = QByteArrayLiteral("utf-8");

        m_step++;
        return QXmppSaslDigestMd5::serializeMessage(output);
    } else if (m_step == 2) {
        const QMap<QByteArray, QByteArray> input = QXmppSaslDigestMd5::parseMessage(challenge);

        // check new challenge
        if (input.value(QByteArrayLiteral("rspauth")) != calculateDigest(QByteArray(), digestUri, m_secret, m_nonce, m_cnonce, m_nc)) {
            warning(u"QXmppSaslClientDigestMd5 : Invalid challenge on step 2"_s);
            return {};
        }

        m_step++;
        return QByteArray();
    } else {
        warning(u"QXmppSaslClientDigestMd5 : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientFacebook::QXmppSaslClientFacebook(QObject *parent)
    : QXmppSaslClient(parent), m_step(0)
{
}

void QXmppSaslClientFacebook::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_accessToken = credentials.facebookAccessToken;
    m_appId = credentials.facebookAppId;
}
std::optional<QByteArray> QXmppSaslClientFacebook::respond(const QByteArray &challenge)
{
    if (m_step == 0) {
        // no initial response
        m_step++;
        return QByteArray();
    } else if (m_step == 1) {
        // parse request
        QUrlQuery requestUrl(QString::fromUtf8(challenge));
        if (!requestUrl.hasQueryItem(u"method"_s) || !requestUrl.hasQueryItem(u"nonce"_s)) {
            warning(u"QXmppSaslClientFacebook : Invalid challenge, nonce or method missing"_s);
            return {};
        }

        // build response
        QUrlQuery responseUrl;
        responseUrl.addQueryItem(u"access_token"_s, m_accessToken);
        responseUrl.addQueryItem(u"api_key"_s, m_appId);
        responseUrl.addQueryItem(u"call_id"_s, QString());
        responseUrl.addQueryItem(u"method"_s, requestUrl.queryItemValue(u"method"_s));
        responseUrl.addQueryItem(u"nonce"_s, requestUrl.queryItemValue(u"nonce"_s));
        responseUrl.addQueryItem(u"v"_s, u"1.0"_s);

        m_step++;
        return responseUrl.query().toUtf8();
    } else {
        warning(u"QXmppSaslClientFacebook : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientGoogle::QXmppSaslClientGoogle(QObject *parent)
    : QXmppSaslClient(parent), m_step(0)
{
}

void QXmppSaslClientGoogle::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_accessToken = credentials.googleAccessToken;
}

std::optional<QByteArray> QXmppSaslClientGoogle::respond(const QByteArray &)
{
    if (m_step == 0) {
        // send initial response
        m_step++;
        return QString(u'\0' + username() + u'\0' + m_accessToken).toUtf8();
    } else {
        warning(u"QXmppSaslClientGoogle : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientPlain::QXmppSaslClientPlain(QObject *parent)
    : QXmppSaslClient(parent), m_step(0)
{
}

void QXmppSaslClientPlain::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_password = credentials.password;
}

std::optional<QByteArray> QXmppSaslClientPlain::respond(const QByteArray &)
{
    if (m_step == 0) {
        m_step++;
        return QString(u'\0' + username() + u'\0' + m_password).toUtf8();
    } else {
        warning(u"QXmppSaslClientPlain : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientScram::QXmppSaslClientScram(SaslScramMechanism mechanism, QObject *parent)
    : QXmppSaslClient(parent),
      m_mechanism(mechanism),
      m_step(0),
      m_dklen(QCryptographicHash::hashLength(m_mechanism.qtAlgorithm()))
{
    m_nonce = generateNonce();
}

void QXmppSaslClientScram::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_password = credentials.password;
}

std::optional<QByteArray> QXmppSaslClientScram::respond(const QByteArray &challenge)
{
    if (m_step == 0) {
        m_gs2Header = QByteArrayLiteral("n,,");
        m_clientFirstMessageBare = QByteArrayLiteral("n=") + username().toUtf8() + QByteArrayLiteral(",r=") + m_nonce;

        m_step++;
        return m_gs2Header + m_clientFirstMessageBare;
    } else if (m_step == 1) {
        // validate input
        const QMap<char, QByteArray> input = parseGS2(challenge);
        const QByteArray nonce = input.value('r');
        const QByteArray salt = QByteArray::fromBase64(input.value('s'));
        const int iterations = input.value('i').toInt();
        if (!nonce.startsWith(m_nonce) || salt.isEmpty() || iterations < 1) {
            return {};
        }

        // calculate proofs
        const QByteArray clientFinalMessageBare = QByteArrayLiteral("c=") + m_gs2Header.toBase64() + QByteArrayLiteral(",r=") + nonce;
        const QByteArray saltedPassword = QPasswordDigestor::deriveKeyPbkdf2(
            m_mechanism.qtAlgorithm(), m_password.toUtf8(), salt, iterations, m_dklen);
        const QByteArray clientKey = QMessageAuthenticationCode::hash(QByteArrayLiteral("Client Key"), saltedPassword, m_mechanism.qtAlgorithm());
        const QByteArray storedKey = QCryptographicHash::hash(clientKey, m_mechanism.qtAlgorithm());
        const QByteArray authMessage = m_clientFirstMessageBare + QByteArrayLiteral(",") + challenge + QByteArrayLiteral(",") + clientFinalMessageBare;
        QByteArray clientProof = QMessageAuthenticationCode::hash(authMessage, storedKey, m_mechanism.qtAlgorithm());
        std::transform(clientProof.cbegin(), clientProof.cend(), clientKey.cbegin(),
                       clientProof.begin(), std::bit_xor<char>());

        const QByteArray serverKey = QMessageAuthenticationCode::hash(QByteArrayLiteral("Server Key"), saltedPassword, m_mechanism.qtAlgorithm());
        m_serverSignature = QMessageAuthenticationCode::hash(authMessage, serverKey, m_mechanism.qtAlgorithm());

        m_step++;
        return clientFinalMessageBare + QByteArrayLiteral(",p=") + clientProof.toBase64();
    } else if (m_step == 2) {
        const QMap<char, QByteArray> input = parseGS2(challenge);
        m_step++;
        if (QByteArray::fromBase64(input.value('v')) == m_serverSignature) {
            return QByteArray();
        }
        return {};
    } else {
        warning(u"QXmppSaslClientPlain : Invalid step"_s);
        return {};
    }
}

QXmppSaslClientWindowsLive::QXmppSaslClientWindowsLive(QObject *parent)
    : QXmppSaslClient(parent), m_step(0)
{
}

void QXmppSaslClientWindowsLive::setCredentials(const QXmpp::Private::Credentials &credentials)
{
    m_accessToken = credentials.windowsLiveAccessToken;
}

std::optional<QByteArray> QXmppSaslClientWindowsLive::respond(const QByteArray &)
{
    if (m_step == 0) {
        // send initial response
        m_step++;
        return QByteArray::fromBase64(m_accessToken.toLatin1());
    } else {
        warning(u"QXmppSaslClientWindowsLive : Invalid step"_s);
        return {};
    }
}

class QXmppSaslServerPrivate
{
public:
    QString username;
    QString password;
    QByteArray passwordDigest;
    QString realm;
};

QXmppSaslServer::QXmppSaslServer(QObject *parent)
    : QXmppLoggable(parent),
      d(std::make_unique<QXmppSaslServerPrivate>())
{
}

QXmppSaslServer::~QXmppSaslServer() = default;

/// Creates an SASL server for the given mechanism.
std::unique_ptr<QXmppSaslServer> QXmppSaslServer::create(const QString &mechanism, QObject *parent)
{
    if (mechanism == u"PLAIN") {
        return std::make_unique<QXmppSaslServerPlain>(parent);
    } else if (mechanism == u"DIGEST-MD5") {
        return std::make_unique<QXmppSaslServerDigestMd5>(parent);
    } else if (mechanism == u"ANONYMOUS") {
        return std::make_unique<QXmppSaslServerAnonymous>(parent);
    } else {
        return {};
    }
}

/// Returns the username.
QString QXmppSaslServer::username() const
{
    return d->username;
}

/// Sets the username.
void QXmppSaslServer::setUsername(const QString &username)
{
    d->username = username;
}

/// Returns the password.
QString QXmppSaslServer::password() const
{
    return d->password;
}

/// Sets the password.
void QXmppSaslServer::setPassword(const QString &password)
{
    d->password = password;
}

/// Returns the password digest.
QByteArray QXmppSaslServer::passwordDigest() const
{
    return d->passwordDigest;
}

/// Sets the password digest.
void QXmppSaslServer::setPasswordDigest(const QByteArray &digest)
{
    d->passwordDigest = digest;
}

/// Returns the realm.
QString QXmppSaslServer::realm() const
{
    return d->realm;
}

/// Sets the realm.
void QXmppSaslServer::setRealm(const QString &realm)
{
    d->realm = realm;
}

QXmppSaslServerAnonymous::QXmppSaslServerAnonymous(QObject *parent)
    : QXmppSaslServer(parent), m_step(0)
{
}

QString QXmppSaslServerAnonymous::mechanism() const
{
    return u"ANONYMOUS"_s;
}

QXmppSaslServer::Response QXmppSaslServerAnonymous::respond(const QByteArray &request, QByteArray &response)
{
    Q_UNUSED(request);
    if (m_step == 0) {
        m_step++;
        response = QByteArray();
        return Succeeded;
    } else {
        warning(u"QXmppSaslServerAnonymous : Invalid step"_s);
        return Failed;
    }
}

QXmppSaslServerDigestMd5::QXmppSaslServerDigestMd5(QObject *parent)
    : QXmppSaslServer(parent), m_step(0)
{
    m_nonce = generateNonce();
}

QString QXmppSaslServerDigestMd5::mechanism() const
{
    return u"DIGEST-MD5"_s;
}

QXmppSaslServer::Response QXmppSaslServerDigestMd5::respond(const QByteArray &request, QByteArray &response)
{
    if (m_step == 0) {
        QMap<QByteArray, QByteArray> output;
        output[QByteArrayLiteral("nonce")] = m_nonce;
        if (!realm().isEmpty()) {
            output[QByteArrayLiteral("realm")] = realm().toUtf8();
        }
        output[QByteArrayLiteral("qop")] = QByteArrayLiteral("auth");
        output[QByteArrayLiteral("charset")] = QByteArrayLiteral("utf-8");
        output[QByteArrayLiteral("algorithm")] = QByteArrayLiteral("md5-sess");

        m_step++;
        response = QXmppSaslDigestMd5::serializeMessage(output);
        return Challenge;
    } else if (m_step == 1) {
        const QMap<QByteArray, QByteArray> input = QXmppSaslDigestMd5::parseMessage(request);
        const QByteArray realm = input.value(QByteArrayLiteral("realm"));
        const QByteArray digestUri = input.value(QByteArrayLiteral("digest-uri"));

        if (input.value(QByteArrayLiteral("qop")) != QByteArrayLiteral("auth")) {
            warning(u"QXmppSaslServerDigestMd5 : Invalid quality of protection"_s);
            return Failed;
        }

        setUsername(QString::fromUtf8(input.value(QByteArrayLiteral("username"))));
        if (password().isEmpty() && passwordDigest().isEmpty()) {
            return InputNeeded;
        }

        m_nc = input.value(QByteArrayLiteral("nc"));
        m_cnonce = input.value(QByteArrayLiteral("cnonce"));
        if (!password().isEmpty()) {
            m_secret = QCryptographicHash::hash(
                QByteArray(username().toUtf8() + QByteArrayLiteral(":") + realm + QByteArrayLiteral(":") + password().toUtf8()),
                QCryptographicHash::Md5);
        } else {
            m_secret = passwordDigest();
        }

        if (input.value(QByteArrayLiteral("response")) != calculateDigest(QByteArrayLiteral("AUTHENTICATE"), digestUri, m_secret, m_nonce, m_cnonce, m_nc)) {
            return Failed;
        }

        QMap<QByteArray, QByteArray> output;
        output[QByteArrayLiteral("rspauth")] = calculateDigest(QByteArray(), digestUri, m_secret, m_nonce, m_cnonce, m_nc);

        m_step++;
        response = QXmppSaslDigestMd5::serializeMessage(output);
        return Challenge;
    } else if (m_step == 2) {
        m_step++;
        response = QByteArray();
        return Succeeded;
    } else {
        warning(u"QXmppSaslServerDigestMd5 : Invalid step"_s);
        return Failed;
    }
}

QXmppSaslServerPlain::QXmppSaslServerPlain(QObject *parent)
    : QXmppSaslServer(parent), m_step(0)
{
}

QString QXmppSaslServerPlain::mechanism() const
{
    return u"PLAIN"_s;
}

QXmppSaslServer::Response QXmppSaslServerPlain::respond(const QByteArray &request, QByteArray &response)
{
    if (m_step == 0) {
        if (request.isEmpty()) {
            response = QByteArray();
            return Challenge;
        }

        QList<QByteArray> auth = request.split('\0');
        if (auth.size() != 3) {
            warning(u"QXmppSaslServerPlain : Invalid input"_s);
            return Failed;
        }
        setUsername(QString::fromUtf8(auth[1]));
        setPassword(QString::fromUtf8(auth[2]));

        m_step++;
        response = QByteArray();
        return InputNeeded;
    } else {
        warning(u"QXmppSaslServerPlain : Invalid step"_s);
        return Failed;
    }
}

void QXmppSaslDigestMd5::setNonce(const QByteArray &nonce)
{
    forcedNonce = nonce;
}

QMap<QByteArray, QByteArray> QXmppSaslDigestMd5::parseMessage(const QByteArray &ba)
{
    QMap<QByteArray, QByteArray> map;
    int startIndex = 0;
    int pos = 0;
    while ((pos = ba.indexOf('=', startIndex)) >= 0) {
        // key get name and skip equals
        const QByteArray key = ba.mid(startIndex, pos - startIndex).trimmed();
        pos++;

        if (pos == ba.size()) {
            // end of the input
            map.insert(key, QByteArray());
            startIndex = pos;
        } else if (ba.at(pos) == '"') {
            // check whether string is quoted
            // skip opening quote
            pos++;
            int endPos = ba.indexOf('"', pos);
            // skip quoted quotes
            while (endPos >= 0 && ba.at(endPos - 1) == '\\') {
                endPos = ba.indexOf('"', endPos + 1);
            }
            if (endPos < 0) {
                qWarning("Unfinished quoted string");
                return map;
            }
            // unquote
            QByteArray value = ba.mid(pos, endPos - pos);
            value.replace("\\\"", "\"");
            value.replace("\\\\", "\\");
            map[key] = value;
            // skip closing quote and comma
            startIndex = endPos + 2;
        } else {
            // non-quoted string
            int endPos = ba.indexOf(',', pos);
            if (endPos < 0) {
                endPos = ba.size();
            }
            map[key] = ba.mid(pos, endPos - pos);
            // skip comma
            startIndex = endPos + 1;
        }
    }
    return map;
}

QByteArray QXmppSaslDigestMd5::serializeMessage(const QMap<QByteArray, QByteArray> &map)
{
    QByteArray ba;
    for (auto itr = map.begin(); itr != map.end(); itr++) {
        if (!ba.isEmpty()) {
            ba.append(',');
        }
        ba.append(itr.key() + QByteArrayLiteral("="));
        auto value = itr.value();
        const char *separators = "()<>@,;:\\\"/[]?={} \t";
        bool quote = false;
        for (const char *c = separators; *c; c++) {
            if (value.contains(*c)) {
                quote = true;
                break;
            }
        }
        if (quote) {
            value.replace('\\', QByteArrayLiteral("\\\\"));
            value.replace('\"', QByteArrayLiteral("\\\""));
            ba.append('"' + value + '"');
        } else {
            ba.append(value);
        }
    }
    return ba;
}

std::optional<QByteArray> QXmppSaslClientHt::respond(const QByteArray &challenge)
{
    // TODO: verify <additional-data/> provided by SASL 2 (hmac of 'Responder' + cb data).

    Q_ASSERT(m_mechanism.channelBindingType == QXmpp::Private::SaslHtMechanism::None);

    if (m_done || !challenge.isEmpty() || !m_token || m_mechanism != m_token->mechanism) {
        return {};
    }

    // calculate token hash
    QMessageAuthenticationCode hmac(
        ianaHashAlgorithmToQt(m_mechanism.hashAlgorithm),
        m_token->secret.toUtf8());
    hmac.addData("Initiator");

    m_done = true;
    return username().toUtf8() + char(0) + hmac.result();
}

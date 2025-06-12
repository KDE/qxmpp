// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSASL_P_H
#define QXMPPSASL_P_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"
#include "QXmppLogger.h"
#include "QXmppNonza.h"
#include "QXmppStreamManagement_p.h"
#include "QXmppUtils_p.h"

#include "XmlWriter.h"

#include <optional>

#include <QCryptographicHash>
#include <QDateTime>
#include <QMap>
#include <QUuid>

class QDomElement;
class QXmlStreamWriter;
class QXmppSaslServerPrivate;

namespace QXmpp::Private {
class SaslManager;
}

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

class XmlWriter;

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

//
// Bind 2
//

struct Bind2Feature {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    std::vector<QString> features;
};

struct Bind2Request {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    QString tag;
    // bind2 extensions
    bool csiInactive = false;
    bool carbonsEnable = false;
    std::optional<SmEnable> smEnable;
};

struct Bind2Bound {
    static constexpr std::tuple XmlTag = { u"bound", ns_bind2 };
    // extensions
    std::optional<SmFailed> smFailed;
    std::optional<SmEnabled> smEnabled;
};

template<>
struct XmlSpec<Bind2Feature> {
    static constexpr std::tuple Spec = {
        XmlElement {
            u"inline",
            Optional(),
            XmlSingleAttributeElements { &Bind2Feature::features, Tag { u"feature", ns_bind2 }, u"var" },
        },
    };
};

template<>
struct XmlSpec<Bind2Request> {
    static constexpr std::tuple Spec = {
        XmlElement { u"tag", Optional(), XmlText { &Bind2Request::tag } },
        XmlReference { &Bind2Request::csiInactive, { u"inactive", ns_csi } },
        XmlReference { &Bind2Request::carbonsEnable, { u"enable", ns_carbons } },
        XmlReference { &Bind2Request::smEnable },
    };
};

template<>
struct XmlSpec<Bind2Bound> {
    static constexpr std::tuple Spec = {
        XmlReference { &Bind2Bound::smFailed },
        XmlReference { &Bind2Bound::smEnabled },
    };
};

//
// FAST
//

struct FastFeature {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };
    bool tls0rtt = false;
    std::vector<QString> mechanisms;
};

struct FastTokenRequest {
    static constexpr std::tuple XmlTag = { u"request-token", ns_fast };
    QString mechanism;
};

struct FastToken {
    static constexpr std::tuple XmlTag = { u"token", ns_fast };
    QDateTime expiry;
    QString token;
};

struct FastRequest {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };
    std::optional<uint64_t> count;
    bool invalidate = false;
};

template<>
struct XmlSpec<FastFeature> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &FastFeature::tls0rtt, u"tls-0rtt", BoolDefaultSerializer(false) },
        XmlTextElements { &FastFeature::mechanisms, u"mechanism" },
    };
};

template<>
struct XmlSpec<FastTokenRequest> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &FastTokenRequest::mechanism, u"mechanism" },
    };
};

template<>
struct XmlSpec<FastToken> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &FastToken::expiry, u"expiry" },
        XmlAttribute { &FastToken::token, u"token" },
    };
};

template<>
struct XmlSpec<FastRequest> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &FastRequest::count, u"count" },
        XmlOptionalAttribute { &FastRequest::invalidate, u"invalidate", BoolDefaultSerializer(false) },
    };
};

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

enum class IanaHashAlgorithm {
    Sha256,
    Sha384,
    Sha512,
    Sha3_224,
    Sha3_256,
    Sha3_384,
    Sha3_512,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    Blake2s_256,
    Blake2b_256,
    Blake2b_512,
    _End = Blake2b_512,
#else
    _End = Sha3_512,
#endif
};

QCryptographicHash::Algorithm ianaHashAlgorithmToQt(IanaHashAlgorithm alg);

//
// SASL mechanisms
//

struct SaslScramMechanism {
    static std::optional<SaslScramMechanism> fromString(QStringView str);
    QString toString() const;

    QCryptographicHash::Algorithm qtAlgorithm() const;

    auto operator<=>(const SaslScramMechanism &) const = default;

    enum Algorithm {
        Sha1,
        Sha256,
        Sha512,
        Sha3_512,
    } algorithm;
};

struct SaslHtMechanism {
    static std::optional<SaslHtMechanism> fromString(QStringView);
    QString toString() const;

    auto operator<=>(const SaslHtMechanism &) const = default;

    enum ChannelBindingType {
        TlsServerEndpoint,
        TlsUnique,
        TlsExporter,
        None,
    };

    IanaHashAlgorithm hashAlgorithm;
    ChannelBindingType channelBindingType;
};

struct SaslDigestMd5Mechanism {
    auto operator<=>(const SaslDigestMd5Mechanism &) const = default;
};
struct SaslPlainMechanism {
    auto operator<=>(const SaslPlainMechanism &) const = default;
};
struct SaslAnonymousMechanism {
    auto operator<=>(const SaslAnonymousMechanism &) const = default;
};
struct SaslXFacebookMechanism {
    auto operator<=>(const SaslXFacebookMechanism &) const = default;
};
struct SaslXWindowsLiveMechanism {
    auto operator<=>(const SaslXWindowsLiveMechanism &) const = default;
};
struct SaslXGoogleMechanism {
    auto operator<=>(const SaslXGoogleMechanism &) const = default;
};

// Note that the order of the variant alternatives defines the preference/strength of the mechanisms.
struct SaslMechanism
    : std::variant<SaslXGoogleMechanism,
                   SaslXWindowsLiveMechanism,
                   SaslXFacebookMechanism,
                   SaslAnonymousMechanism,
                   SaslPlainMechanism,
                   SaslDigestMd5Mechanism,
                   SaslScramMechanism,
                   SaslHtMechanism> {
    static std::optional<SaslMechanism> fromString(QStringView str);
    QString toString() const;
};

inline QDebug operator<<(QDebug dbg, SaslMechanism mechanism) { return dbg << mechanism.toString(); }

//
// Credentials
//

struct HtToken {
    static std::optional<HtToken> fromXml(QXmlStreamReader &);
    void toXml(XmlWriter &) const;
    bool operator==(const HtToken &other) const = default;

    SaslHtMechanism mechanism;
    QString secret;
    QDateTime expiry;
};

struct Credentials {
    QString password;
    std::optional<HtToken> htToken;

    // Facebook
    QString facebookAccessToken;
    QString facebookAppId;
    // Google
    QString googleAccessToken;
    // Windows Live
    QString windowsLiveAccessToken;
};

}  // namespace QXmpp::Private

class QXMPP_AUTOTEST_EXPORT QXmppSaslClient : public QXmppLoggable
{
    Q_OBJECT
public:
    QXmppSaslClient(QObject *parent) : QXmppLoggable(parent) { }

    QString host() const { return m_host; }
    void setHost(const QString &host) { m_host = host; }

    QString serviceType() const { return m_serviceType; }
    void setServiceType(const QString &serviceType) { m_serviceType = serviceType; }

    QString username() const { return m_username; }
    void setUsername(const QString &username) { m_username = username; }

    virtual void setCredentials(const QXmpp::Private::Credentials &) = 0;
    virtual QXmpp::Private::SaslMechanism mechanism() const = 0;
    virtual std::optional<QByteArray> respond(const QByteArray &challenge) = 0;

    static bool isMechanismAvailable(QXmpp::Private::SaslMechanism, const QXmpp::Private::Credentials &);
    static std::unique_ptr<QXmppSaslClient> create(const QString &mechanism, QObject *parent = nullptr);
    static std::unique_ptr<QXmppSaslClient> create(QXmpp::Private::SaslMechanism mechanism, QObject *parent = nullptr);

private:
    friend class QXmpp::Private::SaslManager;

    QString m_host;
    QString m_serviceType;
    QString m_username;
    QString m_password;
};

class QXMPP_AUTOTEST_EXPORT QXmppSaslServer : public QXmppLoggable
{
    Q_OBJECT
public:
    enum Response {
        Challenge = 0,
        Succeeded = 1,
        Failed = 2,
        InputNeeded = 3
    };

    QXmppSaslServer(QObject *parent = nullptr);
    ~QXmppSaslServer() override;

    QString username() const;
    void setUsername(const QString &username);

    QString password() const;
    void setPassword(const QString &password);

    QByteArray passwordDigest() const;
    void setPasswordDigest(const QByteArray &digest);

    QString realm() const;
    void setRealm(const QString &realm);

    virtual QString mechanism() const = 0;
    virtual Response respond(const QByteArray &challenge, QByteArray &response) = 0;

    static std::unique_ptr<QXmppSaslServer> create(const QString &mechanism, QObject *parent = nullptr);

private:
    const std::unique_ptr<QXmppSaslServerPrivate> d;
};

class QXMPP_AUTOTEST_EXPORT QXmppSaslDigestMd5
{
public:
    static void setNonce(const QByteArray &nonce);

    // message parsing and serialization
    static QMap<QByteArray, QByteArray> parseMessage(const QByteArray &ba);
    static QByteArray serializeMessage(const QMap<QByteArray, QByteArray> &map);
};

class QXmppSaslClientAnonymous : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientAnonymous(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override { }
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslAnonymousMechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    int m_step;
};

class QXmppSaslClientDigestMd5 : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientDigestMd5(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslDigestMd5Mechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    QString m_password;
    QByteArray m_cnonce;
    QByteArray m_nc;
    QByteArray m_nonce;
    QByteArray m_secret;
    int m_step;
};

class QXmppSaslClientFacebook : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientFacebook(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslXFacebookMechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    int m_step;
    QString m_accessToken;
    QString m_appId;
};

class QXmppSaslClientGoogle : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientGoogle(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslXGoogleMechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    QString m_accessToken;
    int m_step;
};

class QXmppSaslClientPlain : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientPlain(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslPlainMechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    QString m_password;
    int m_step;
};

class QXmppSaslClientScram : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientScram(QXmpp::Private::SaslScramMechanism mechanism, QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { m_mechanism }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    QXmpp::Private::SaslScramMechanism m_mechanism;
    int m_step;
    QString m_password;
    uint32_t m_dklen;
    QByteArray m_gs2Header;
    QByteArray m_clientFirstMessageBare;
    QByteArray m_serverSignature;
    QByteArray m_nonce;
};

class QXmppSaslClientHt : public QXmppSaslClient
{
    Q_OBJECT
    using HtMechanism = QXmpp::Private::SaslHtMechanism;

public:
    QXmppSaslClientHt(HtMechanism mechanism, QObject *parent)
        : QXmppSaslClient(parent), m_mechanism(mechanism)
    {
    }

    void setCredentials(const QXmpp::Private::Credentials &credentials) override { m_token = credentials.htToken; }
    QXmpp::Private::SaslMechanism mechanism() const override { return { m_mechanism }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    std::optional<QXmpp::Private::HtToken> m_token;
    HtMechanism m_mechanism;
    bool m_done = false;
};

class QXmppSaslClientWindowsLive : public QXmppSaslClient
{
    Q_OBJECT
public:
    QXmppSaslClientWindowsLive(QObject *parent = nullptr);
    void setCredentials(const QXmpp::Private::Credentials &) override;
    QXmpp::Private::SaslMechanism mechanism() const override { return { QXmpp::Private::SaslXWindowsLiveMechanism() }; }
    std::optional<QByteArray> respond(const QByteArray &challenge) override;

private:
    QString m_accessToken;
    int m_step;
};

class QXmppSaslServerAnonymous : public QXmppSaslServer
{
    Q_OBJECT
public:
    QXmppSaslServerAnonymous(QObject *parent = nullptr);
    QString mechanism() const override;

    Response respond(const QByteArray &challenge, QByteArray &response) override;

private:
    int m_step;
};

class QXmppSaslServerDigestMd5 : public QXmppSaslServer
{
    Q_OBJECT
public:
    QXmppSaslServerDigestMd5(QObject *parent = nullptr);
    QString mechanism() const override;

    Response respond(const QByteArray &challenge, QByteArray &response) override;

private:
    QByteArray m_cnonce;
    QByteArray m_nc;
    QByteArray m_nonce;
    QByteArray m_secret;
    int m_step;
};

class QXmppSaslServerPlain : public QXmppSaslServer
{
    Q_OBJECT
public:
    QXmppSaslServerPlain(QObject *parent = nullptr);
    QString mechanism() const override;

    Response respond(const QByteArray &challenge, QByteArray &response) override;

private:
    int m_step;
};

#endif

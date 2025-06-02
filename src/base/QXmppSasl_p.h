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
    static std::optional<Auth> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QString mechanism;
    QByteArray value;
};

struct Challenge {
    static constexpr std::tuple XmlTag = { u"challenge", ns_sasl };
    static std::optional<Challenge> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QByteArray value;
};

struct Failure {
    static constexpr std::tuple XmlTag = { u"failure", ns_sasl };
    static std::optional<Failure> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    std::optional<ErrorCondition> condition;
    QString text;
};

struct Response {
    static constexpr std::tuple XmlTag = { u"response", ns_sasl };
    static std::optional<Response> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QByteArray value;
};

struct Success {
    static constexpr std::tuple XmlTag = { u"success", ns_sasl };
    static std::optional<Success> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;
};

}  // namespace Sasl

struct Bind2Feature {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    static std::optional<Bind2Feature> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    std::vector<QString> features;
};

struct Bind2Request {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    static std::optional<Bind2Request> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QString tag;
    // bind2 extensions
    bool csiInactive = false;
    bool carbonsEnable = false;
    std::optional<SmEnable> smEnable;
};

struct Bind2Bound {
    static constexpr std::tuple XmlTag = { u"bound", ns_bind2 };
    static std::optional<Bind2Bound> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    // extensions
    std::optional<SmFailed> smFailed;
    std::optional<SmEnabled> smEnabled;
};

struct FastFeature {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };
    static std::optional<FastFeature> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    std::vector<QString> mechanisms;
    bool tls0rtt = false;
};

struct FastTokenRequest {
    static constexpr std::tuple XmlTag = { u"request-token", ns_fast };
    static std::optional<FastTokenRequest> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QString mechanism;
};

struct FastToken {
    static constexpr std::tuple XmlTag = { u"token", ns_fast };
    static std::optional<FastToken> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QDateTime expiry;
    QString token;
};

struct FastRequest {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };
    static std::optional<FastRequest> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    std::optional<uint64_t> count;
    bool invalidate = false;
};

namespace Sasl2 {

struct StreamFeature {
    static constexpr std::tuple XmlTag = { u"authentication", QXmpp::Private::ns_sasl_2 };
    static std::optional<StreamFeature> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QList<QString> mechanisms;
    std::optional<Bind2Feature> bind2Feature;
    std::optional<FastFeature> fast;
    bool streamResumptionAvailable = false;
};

struct UserAgent {
    static constexpr std::tuple XmlTag = { u"user-agent", QXmpp::Private::ns_sasl_2 };
    static std::optional<UserAgent> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QUuid id;
    QString software;
    QString device;
};

struct Authenticate {
    static constexpr std::tuple XmlTag = { u"authenticate", QXmpp::Private::ns_sasl_2 };
    static std::optional<Authenticate> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QString mechanism;
    QByteArray initialResponse;
    std::optional<UserAgent> userAgent;
    std::optional<Bind2Request> bindRequest;
    std::optional<SmResume> smResume;
    std::optional<FastTokenRequest> tokenRequest;
    std::optional<FastRequest> fast;
};

struct Challenge {
    static constexpr std::tuple XmlTag = { u"challenge", QXmpp::Private::ns_sasl_2 };
    static std::optional<Challenge> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QByteArray data;
};

struct Response {
    static constexpr std::tuple XmlTag = { u"response", QXmpp::Private::ns_sasl_2 };
    static std::optional<Response> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QByteArray data;
};

struct Success {
    static constexpr std::tuple XmlTag = { u"success", QXmpp::Private::ns_sasl_2 };
    static std::optional<Success> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    std::optional<QByteArray> additionalData;
    QString authorizationIdentifier;
    // extensions
    std::optional<Bind2Bound> bound;
    std::optional<SmResumed> smResumed;
    std::optional<SmFailed> smFailed;
    std::optional<FastToken> token;
};

struct Failure {
    static constexpr std::tuple XmlTag = { u"failure", QXmpp::Private::ns_sasl_2 };
    static std::optional<Failure> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    Sasl::ErrorCondition condition;
    QString text;
    // extensions
};

struct Continue {
    static constexpr std::tuple XmlTag = { u"continue", QXmpp::Private::ns_sasl_2 };
    static std::optional<Continue> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QByteArray additionalData;
    std::vector<QString> tasks;
    QString text;
};

struct Abort {
    static constexpr std::tuple XmlTag = { u"abort", QXmpp::Private::ns_sasl_2 };
    static std::optional<Abort> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    QString text;
};

}  // namespace Sasl2

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

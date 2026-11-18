// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2019 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2020 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppUri.h"

#include "QXmppMessage_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"

#include <algorithm>

#include <QUrlQuery>

using namespace QXmpp::Private;
using namespace QXmpp::Uri;

constexpr QStringView SCHEME = u"xmpp";
constexpr QChar QUERY_ITEM_DELIMITER = u';';
constexpr QChar QUERY_ITEM_KEY_DELIMITER = u'=';
constexpr QStringView OMEMO_FINGERPRINT_KEY_PREFIX = u"omemo-sid-";
constexpr QStringView OMEMO_FINGERPRINT_LEGACY_KEY = u"omemo";

// Adds a key-value pair to a query if the value is not empty.
static void addKeyValuePairToQuery(QUrlQuery &query, const QString &key, QStringView value)
{
    if (!value.isEmpty()) {
        query.addQueryItem(key, value.toString());
    }
}

// Extracts the fully-encoded value of a query's key-value pair.
static QString queryItemValue(const QUrlQuery &query, const QString &key)
{
    return query.queryItemValue(key, QUrl::FullyDecoded);
}

/*!
    \namespace QXmpp::Uri
    \inmodule QXmpp

    Contains URI classes that can be serialized to URI queries (see QXmppUri).

    \since QXmpp 1.8
*/

namespace QXmpp::Uri {

/*!
    \struct QXmpp::Uri::Command
    \inmodule QXmpp

    A "command" query from \xep{0050}{Ad-Hoc Commands}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Invite
    \inmodule QXmpp

    An "invite" query from \xep{0045}{Multi-User Chat}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Join
    \inmodule QXmpp

    A "join" query from \xep{0045}{Multi-User Chat}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Login
    \inmodule QXmpp

    A "login" query, not formally specified.

    Used in the wild, e.g. by Kaidan.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Message
    \inmodule QXmpp

    A "message" query defined in \xep{0147}{XMPP URI Scheme Query Components}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Unregister
    \inmodule QXmpp

    An "unregister" query defined in \xep{0077}{In-Band Registration}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Unsubscribe
    \inmodule QXmpp

    An "unsubscribe" query defined in \xep{0147}{XMPP URI Scheme Query Components}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Register
    \inmodule QXmpp

    A "register" query defined in \xep{0077}{In-Band Registration}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Remove
    \inmodule QXmpp

    A "remove" query defined in \xep{0147}{XMPP URI Scheme Query Components}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Roster
    \inmodule QXmpp

    A "roster" query defined in \xep{0147}{XMPP URI Scheme Query Components}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::Subscribe
    \inmodule QXmpp

    A "subscribe" query defined in \xep{0147}{XMPP URI Scheme Query Components}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::TrustMessage
    \inmodule QXmpp

    A "trust-message" query defined in \xep{0434}{Trust Messages (TM)}.

    \since QXmpp 1.8
*/

/*!
    \struct QXmpp::Uri::OmemoFingerprint
    \inmodule QXmpp

    The fingerprint of an OMEMO device in the URI format of Conversations, not formally specified.

    Conversations includes the fingerprints of the devices of an account in the URIs it shares,
    e.g. in QR codes, so that scanning them verifies the keys:

    \c {xmpp:alice@example.org?omemo-sid-123456=08a6...;omemo-sid-654321=c7f1...}

    Each fingerprint is a key-value pair \c {omemo-sid-<device ID>=<key>} with the key encoded as
    Base16. The fingerprints can be the only items of the query (without a query type) or follow
    any query, e.g. \c {xmpp:alice@example.org?message;omemo-sid-123456=08a6...}. Older versions
    of Conversations used \c {omemo=<key>} without the device ID, it is parsed with the device ID
    0. See QXmppUri::omemoFingerprints().

    The format of the key depends on the OMEMO version, the URI doesn't tell which one is used:
    \list
    \li OMEMO 0 (\c {eu.siacs.conversations.axolotl}), used by Conversations: the Curve25519
    public identity key (32 bytes) without the leading key type byte (0x05) of its serialization
    in the bundle.
    \li OMEMO 2 (\c {urn:xmpp:omemo:2}), used by QXmppOmemoManager: the Ed25519 public identity
    key (32 bytes), as in QXmppOmemoDevice::keyId().
    \endlist

    Both are the same key in different representations: the Curve25519 key is the Montgomery form
    of the Ed25519 key. QXmppUri doesn't convert the keys. To compare a key from a URI of
    Conversations with an OMEMO 2 key ID, convert the Ed25519 key to its Montgomery form; the
    other direction is ambiguous because the Curve25519 key lacks the sign bit.

    \since QXmpp 1.18
*/

/*!
    \struct QXmpp::Uri::CustomQuery
    \inmodule QXmpp

    A query with a custom name and custom key-value pairs.

    Queries will be parsed into this type if they are unknown.

    \since QXmpp 1.8
*/

}  // namespace QXmpp::Uri

static void serializeUrlQuery(const Command &command, QUrlQuery &query)
{
    query.addQueryItem(u"command"_s, {});

    addKeyValuePairToQuery(query, u"node"_s, command.node);
    addKeyValuePairToQuery(query, u"action"_s, command.action);
}

static void serializeUrlQuery(const Invite &invite, QUrlQuery &query)
{
    query.addQueryItem(u"invite"_s, {});

    addKeyValuePairToQuery(query, u"jid"_s, invite.inviteeJid);
    addKeyValuePairToQuery(query, u"password"_s, invite.password);
}

static void serializeUrlQuery(const Join &join, QUrlQuery &query)
{
    query.addQueryItem(u"join"_s, {});

    addKeyValuePairToQuery(query, u"password"_s, join.password);
}

static void serializeUrlQuery(const Login &login, QUrlQuery &query)
{
    query.addQueryItem(u"login"_s, {});

    addKeyValuePairToQuery(query, u"password"_s, login.password);
}

static void serializeUrlQuery(const Message &message, QUrlQuery &query)
{
    query.addQueryItem(u"message"_s, {});

    addKeyValuePairToQuery(query, u"from"_s, message.from);
    addKeyValuePairToQuery(query, u"id"_s, message.id);
    if (message.type) {
        addKeyValuePairToQuery(query, u"type"_s, Enums::toString(*message.type));
    }
    addKeyValuePairToQuery(query, QStringLiteral("subject"), message.subject);
    addKeyValuePairToQuery(query, QStringLiteral("body"), message.body);
    addKeyValuePairToQuery(query, QStringLiteral("thread"), message.thread);
}

static void serializeUrlQuery(const Unregister &, QUrlQuery &query)
{
    query.addQueryItem(u"unregister"_s, {});
}

static void serializeUrlQuery(const Unsubscribe &, QUrlQuery &query)
{
    query.addQueryItem(u"unsubscribe"_s, {});
}

static void serializeUrlQuery(const Register &, QUrlQuery &query)
{
    query.addQueryItem(u"register"_s, {});
}

static void serializeUrlQuery(const Remove &, QUrlQuery &query)
{
    query.addQueryItem(u"remove"_s, {});
}

static void serializeUrlQuery(const Roster &roster, QUrlQuery &query)
{
    query.addQueryItem(u"roster"_s, {});

    addKeyValuePairToQuery(query, u"name"_s, roster.name);
    addKeyValuePairToQuery(query, u"group"_s, roster.group);
}

static void serializeUrlQuery(const Subscribe &, QUrlQuery &query)
{
    query.addQueryItem(u"subscribe"_s, {});
}

static void serializeUrlQuery(const TrustMessage &trustMessage, QUrlQuery &query)
{
    query.addQueryItem(u"trust-message"_s, {});

    addKeyValuePairToQuery(query, QStringLiteral("encryption"), trustMessage.encryption);

    for (auto &identifier : trustMessage.trustKeyIds) {
        addKeyValuePairToQuery(query, QStringLiteral("trust"), identifier);
    }

    for (auto &identifier : trustMessage.distrustKeyIds) {
        addKeyValuePairToQuery(query, QStringLiteral("distrust"), identifier);
    }
}

static void serializeUrlQuery(const CustomQuery &custom, QUrlQuery &query)
{
    query.addQueryItem(custom.query, {});
    for (const auto &[key, value] : custom.parameters) {
        query.addQueryItem(key, value);
    }
}

Command parseCommandQuery(const QUrlQuery &q)
{
    return {
        queryItemValue(q, u"node"_s),
        queryItemValue(q, u"action"_s),
    };
}

Invite parseInviteQuery(const QUrlQuery &q)
{
    return {
        queryItemValue(q, u"jid"_s),
        queryItemValue(q, u"password"_s),
    };
}

Join parseJoinQuery(const QUrlQuery &q)
{
    return {
        queryItemValue(q, u"password"_s),
    };
}

Login parseLoginQuery(const QUrlQuery &q)
{
    return {
        queryItemValue(q, u"password"_s),
    };
}

Message parseMessageQuery(const QUrlQuery &q)
{
    return {
        queryItemValue(q, u"subject"_s),
        queryItemValue(q, u"body"_s),
        queryItemValue(q, u"thread"_s),
        queryItemValue(q, u"id"_s),
        queryItemValue(q, u"from"_s),
        Enums::fromString<QXmppMessage::Type>(queryItemValue(q, u"type"_s)),
    };
}

Roster parseRosterQuery(const QUrlQuery &q)
{
    return Roster {
        queryItemValue(q, u"name"_s),
        queryItemValue(q, u"group"_s),
    };
}

TrustMessage parseTrustMessageQuery(const QUrlQuery &q)
{
    return TrustMessage {
        queryItemValue(q, QStringLiteral("encryption")),
        q.allQueryItemValues(QStringLiteral("trust"), QUrl::FullyDecoded),
        q.allQueryItemValues(QStringLiteral("distrust"), QUrl::FullyDecoded),
    };
}

CustomQuery parseCustomQuery(const QUrlQuery &q)
{
    auto queryItems = q.queryItems();
    auto queryName = queryItems.first().first;
    queryItems.pop_front();
    return CustomQuery { queryName, queryItems };
}

// Whether the key is the key of an OMEMO fingerprint parameter of Conversations
// ("omemo-sid-<device ID>=<key ID>" or legacy "omemo=<key ID>").
static bool isOmemoFingerprintKey(const QString &key)
{
    return key.startsWith(OMEMO_FINGERPRINT_KEY_PREFIX) || key == OMEMO_FINGERPRINT_LEGACY_KEY;
}

// Parses an OMEMO fingerprint parameter, returns no value if it is invalid.
static std::optional<OmemoFingerprint> parseOmemoFingerprint(const QString &key, const QString &value)
{
    uint32_t deviceId = 0;
    if (key != OMEMO_FINGERPRINT_LEGACY_KEY) {
        bool ok = false;
        deviceId = QStringView(key).mid(OMEMO_FINGERPRINT_KEY_PREFIX.size()).toUInt(&ok);
        if (!ok || deviceId == 0) {
            return {};
        }
    }

    auto isHexDigit = [](QChar c) {
        return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
    };
    if (value.isEmpty() || value.size() % 2 != 0 || !std::ranges::all_of(value, isHexDigit)) {
        return {};
    }

    return OmemoFingerprint { deviceId, QByteArray::fromHex(value.toLatin1()) };
}

static void serializeOmemoFingerprint(const OmemoFingerprint &fingerprint, QUrlQuery &query)
{
    auto key = fingerprint.deviceId == 0
        ? OMEMO_FINGERPRINT_LEGACY_KEY.toString()
        : OMEMO_FINGERPRINT_KEY_PREFIX + QString::number(fingerprint.deviceId);
    query.addQueryItem(key, QString::fromLatin1(fingerprint.keyId.toHex()));
}

struct QXmppUriPrivate : QSharedData {
    QString jid;
    std::any query;
    QList<OmemoFingerprint> omemoFingerprints;
};

/*! Creates an empty XMPP URI */
QXmppUri::QXmppUri()
    : d(new QXmppUriPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppUri)

/*! Parses an XMPP URI from the string \a input.

    Returns the parsed URI or an error if the string could not be parsed. Returns true on success. */
std::variant<QXmppUri, QXmppError> QXmppUri::fromString(const QString &input)
{
    QUrl url(input);
    if (!url.isValid()) {
        return QXmppError { u"Invalid URI"_s, {} };
    }
    if (url.scheme() != SCHEME) {
        return QXmppError { u"Wrong URI scheme (is '%1', must be xmpp)"_s.arg(url.scheme()), {} };
    }

    QXmppUri uri;
    uri.setJid(url.path());

    if (url.hasQuery()) {
        QUrlQuery urlQuery;
        urlQuery.setQueryDelimiters(QUERY_ITEM_KEY_DELIMITER, QUERY_ITEM_DELIMITER);
        urlQuery.setQuery(url.query(QUrl::FullyEncoded));

        // Extract the OMEMO fingerprints of Conversations, they can be combined with any query
        // or be the only items (not starting with a query type).
        QList<QString> fingerprintKeys;
        for (const auto &[key, value] : urlQuery.queryItems(QUrl::FullyDecoded)) {
            if (isOmemoFingerprintKey(key)) {
                // invalid fingerprints are ignored like Conversations does
                if (auto fingerprint = parseOmemoFingerprint(key, value)) {
                    uri.d->omemoFingerprints.append(std::move(*fingerprint));
                }
                fingerprintKeys.append(key);
            }
        }
        for (const auto &key : std::as_const(fingerprintKeys)) {
            urlQuery.removeAllQueryItems(key);
        }

        // Check if there is at least one query item.
        if (!urlQuery.isEmpty()) {
            auto queryItems = urlQuery.queryItems();
            Q_ASSERT(!queryItems.isEmpty());
            auto [queryString, queryValue] = queryItems.first();

            if (!queryValue.isEmpty()) {
                // invalid XMPP URI: first query query pair must have only key, no value
                return QXmppError { u"Invalid URI query: got key-value pair (instead of key only) for first query parameter."_s, {} };
            }

            if (queryString == u"command") {
                uri.d->query = parseCommandQuery(urlQuery);
            } else if (queryString == u"invite") {
                uri.d->query = parseInviteQuery(urlQuery);
            } else if (queryString == u"join") {
                uri.d->query = parseJoinQuery(urlQuery);
            } else if (queryString == u"login") {
                uri.d->query = parseLoginQuery(urlQuery);
            } else if (queryString == u"message") {
                uri.d->query = parseMessageQuery(urlQuery);
            } else if (queryString == u"register") {
                uri.d->query = Register();
            } else if (queryString == u"remove") {
                uri.d->query = Remove();
            } else if (queryString == u"roster") {
                uri.d->query = parseRosterQuery(urlQuery);
            } else if (queryString == u"subscribe") {
                uri.d->query = Subscribe();
            } else if (queryString == u"trust-message") {
                uri.d->query = parseTrustMessageQuery(urlQuery);
            } else if (queryString == u"unregister") {
                uri.d->query = Unregister();
            } else if (queryString == u"unsubscribe") {
                uri.d->query = Unsubscribe();
            } else {
                uri.d->query = parseCustomQuery(urlQuery);
            }
        }
    }

    return uri;
}

template<typename T>
bool serialize(const std::any &query, QUrlQuery &urlQuery)
{
    if (query.type() == typeid(T)) {
        serializeUrlQuery(std::any_cast<T>(query), urlQuery);
        return true;
    }
    return false;
}

/*! Serializes the URI to a string. */
QString QXmppUri::toString()
{
    QUrl url;
    url.setScheme(staticString(SCHEME));
    url.setPath(d->jid);

    // add URI query
    QUrlQuery urlQuery;
    urlQuery.setQueryDelimiters(QUERY_ITEM_KEY_DELIMITER, QUERY_ITEM_DELIMITER);

    if (d->query.has_value()) {
        serialize<Command>(d->query, urlQuery) ||
            serialize<Invite>(d->query, urlQuery) ||
            serialize<Join>(d->query, urlQuery) ||
            serialize<Login>(d->query, urlQuery) ||
            serialize<Message>(d->query, urlQuery) ||
            serialize<Unregister>(d->query, urlQuery) ||
            serialize<Unsubscribe>(d->query, urlQuery) ||
            serialize<Register>(d->query, urlQuery) ||
            serialize<Remove>(d->query, urlQuery) ||
            serialize<Roster>(d->query, urlQuery) ||
            serialize<Subscribe>(d->query, urlQuery) ||
            serialize<TrustMessage>(d->query, urlQuery) ||
            serialize<CustomQuery>(d->query, urlQuery);
    }

    for (const auto &fingerprint : std::as_const(d->omemoFingerprints)) {
        serializeOmemoFingerprint(fingerprint, urlQuery);
    }

    url.setQuery(urlQuery);

    return QString::fromUtf8(url.toEncoded(QUrl::FullyEncoded));
}

/*!
    Returns the JID this URI is about.

    This can also be e.g. a MUC room in case of a Join action.
*/
QString QXmppUri::jid() const
{
    return d->jid;
}

/*!
    Sets the JID this URI links to.

    \a jid.
*/
void QXmppUri::setJid(const QString &jid)
{
    d->jid = jid;
}

/*!
    Returns the query of the URI.

    It may be empty (has_value() returns false). Possible URI types are available in the namespace
    QXmpp::Uri.
*/
std::any QXmppUri::query() const
{
    return d->query;
}

void QXmppUri::setQuery(std::any &&query)
{
    d->query = std::move(query);
}

/*!
    Returns the OMEMO fingerprints of the URI in the format of Conversations (see
    QXmpp::Uri::OmemoFingerprint).

    Invalid fingerprints are ignored when parsing.

    \since QXmpp 1.18
*/
QList<OmemoFingerprint> QXmppUri::omemoFingerprints() const
{
    return d->omemoFingerprints;
}

/*!
    Sets the OMEMO fingerprints of the URI.

    \a fingerprints are serialized after the query, the keys as lowercase Base16. A fingerprint
    with device ID 0 is serialized in the legacy format \c {omemo=<key>}.

    \since QXmpp 1.18
*/
void QXmppUri::setOmemoFingerprints(const QList<OmemoFingerprint> &fingerprints)
{
    d->omemoFingerprints = fingerprints;
}

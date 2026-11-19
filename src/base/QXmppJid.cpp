// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppJid.h"

#include "QXmppPrecis_p.h"

#include "StringLiterals.h"

#include <algorithm>

#include <QDebug>
#include <QHostAddress>
#include <QUrl>

using namespace QXmpp::Private;

namespace QXmpp {

namespace {

constexpr qsizetype maxPartLength = 1023;
constexpr qsizetype maxDnsNameLength = 253;
constexpr qsizetype maxDnsLabelLength = 63;

using PartResult = std::variant<QString, JidParsingError>;

// The 10 characters XEP-0106 escapes, with their escape codes.
struct Escaping {
    char16_t character;
    char16_t code[2];
};

constexpr Escaping escapings[] = {
    { u' ', { u'2', u'0' } },
    { u'"', { u'2', u'2' } },
    { u'&', { u'2', u'6' } },
    { u'\'', { u'2', u'7' } },
    { u'/', { u'2', u'f' } },
    { u':', { u'3', u'a' } },
    { u'<', { u'3', u'c' } },
    { u'>', { u'3', u'e' } },
    { u'@', { u'4', u'0' } },
    { u'\\', { u'5', u'c' } },
};

qsizetype utf8Length(QStringView string)
{
    qsizetype length = 0;
    for (auto c : string) {
        auto u = c.unicode();
        length += u < 0x80 ? 1 : u < 0x800 || QChar::isSurrogate(u) ? 2
                                                                    : 3;
    }
    return length;
}

JidParsingError error(JidParsingError::Type type, QString description)
{
    return JidParsingError { type, std::move(description) };
}

template<typename Enforce>
PartResult preparePart(QStringView input, Enforce enforce, JidParsingError::Type errorType, QStringView partName)
{
    auto result = enforce(input);
    if (auto *precisError = std::get_if<Precis::Error>(&result)) {
        return error(errorType, partName.toString() + u": "_s + precisError->description);
    }
    auto output = std::get<QString>(std::move(result));
    if (utf8Length(output) > maxPartLength) {
        return error(errorType, partName.toString() + u" is longer than 1023 bytes."_s);
    }
    return output;
}

// RFC 7622, section 3.3
PartResult prepareLocalpart(QStringView localpart)
{
    auto result = preparePart(localpart, Precis::enforceUsernameCaseMapped, JidParsingError::InvalidLocalpart, u"Localpart");
    if (auto *output = std::get_if<QString>(&result)) {
        // Checked after enforcement, because width mapping can produce these characters.
        for (auto c : { u'"', u'&', u'\'', u'/', u':', u'<', u'>', u'@' }) {
            if (output->contains(QChar(c))) {
                return error(JidParsingError::InvalidLocalpart, u"Localpart: Character '%1' is not allowed (see XEP-0106 for escaping)."_s.arg(QChar(c)));
            }
        }
    }
    return result;
}

// RFC 7622, section 3.4
PartResult prepareResource(QStringView resource)
{
    return preparePart(resource, Precis::enforceOpaqueString, JidParsingError::InvalidResourcepart, u"Resourcepart");
}

bool isDecOctet(QStringView s)
{
    if (s.isEmpty() || s.size() > 3 || !std::ranges::all_of(s, [](QChar c) { return c.isDigit() && c.unicode() < 0x80; })) {
        return false;
    }
    if (s.size() > 1 && s.front() == u'0') {
        return false;
    }
    return s.toInt() <= 255;
}

// RFC 3986, IPv4address
bool isIpv4Address(QStringView domain)
{
    qsizetype parts = 0;
    for (auto part : domain.tokenize(u'.')) {
        if (++parts > 4 || !isDecOctet(part)) {
            return false;
        }
    }
    return parts == 4;
}

// NR-LDH label or A-label (RFC 5890, section 2.3.1), already lowercase
bool isValidAceLabel(QStringView label)
{
    if (label.isEmpty() || label.size() > maxDnsLabelLength) {
        return false;
    }
    if (!std::ranges::all_of(label, [](QChar c) { return (c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'-'; })) {
        return false;
    }
    if (label.front() == u'-' || label.back() == u'-') {
        return false;
    }
    // R-LDH labels are reserved; of those only A-labels ("xn--") are allowed.
    return !(label.size() >= 4 && label[2] == u'-' && label[3] == u'-') || label.startsWith(u"xn--");
}

// RFC 7622, section 3.2
PartResult prepareDomain(QStringView domain)
{
    auto invalid = [](QString description) {
        return error(JidParsingError::InvalidDomainpart, u"Domainpart: "_s + description);
    };

    if (!domain.isEmpty() && (domain.back() == u'.' || domain.back() == u'\u3002' || domain.back() == u'\uFF0E' || domain.back() == u'\uFF61')) {
        domain.chop(1);
    }
    if (domain.isEmpty()) {
        return invalid(u"Domainpart is empty."_s);
    }

    // IP-literal (RFC 3986); zone identifiers must not be sent over the wire (RFC 9844)
    if (domain.front() == u'[') {
        QHostAddress address;
        if (domain.back() != u']' || domain.contains(u'%') || !address.setAddress(domain.sliced(1, domain.size() - 2).toString()) ||
            address.protocol() != QAbstractSocket::IPv6Protocol) {
            return invalid(u"Invalid IPv6 address literal."_s);
        }
        return u'[' + address.toString() + u']';
    }
    if (isIpv4Address(domain)) {
        return domain.toString();
    }

    QString aceString;
    const bool plainAscii = std::ranges::all_of(domain, [](QChar c) { return c.unicode() < 0x80; }) &&
        !domain.contains(u"xn--", Qt::CaseInsensitive);
    if (plainAscii) {
        aceString = domain.toString().toLower();
    } else {
        aceString = QString::fromLatin1(QUrl::toAce(domain.toString(), QUrl::IgnoreIDNWhitelist));
        if (aceString.isEmpty()) {
            return invalid(u"Not a valid internationalized domain name."_s);
        }
    }

    if (aceString.size() > maxDnsNameLength) {
        return invalid(u"Domain name is longer than 253 characters."_s);
    }
    QStringView lastLabel;
    for (auto label : QStringView(aceString).tokenize(u'.')) {
        if (!isValidAceLabel(label)) {
            return invalid(u"Invalid DNS label '%1'."_s.arg(label));
        }
        lastLabel = label;
    }
    // Not a valid IPv4 address (checked above), but also not a host name.
    if (std::ranges::all_of(lastLabel, [](QChar c) { return c.isDigit(); })) {
        return invalid(u"Top-level label must not be numeric."_s);
    }

    if (plainAscii) {
        return aceString;
    }

    // RFC 7622, section 3.2.1: A-labels are converted to U-labels.
    auto unicode = QUrl::fromAce(aceString.toLatin1(), QUrl::IgnoreIDNWhitelist);
    for (auto label : QStringView(unicode).tokenize(u'.')) {
        if (label.startsWith(u"xn--")) {
            return invalid(u"Invalid A-label '%1'."_s.arg(label));
        }
    }
    if (utf8Length(unicode) > maxPartLength) {
        return invalid(u"Domainpart is longer than 1023 bytes."_s);
    }
    return unicode;
}

}  // namespace

/*!
    \struct QXmpp::JidParsingError
    \inmodule QXmpp

    Error returned when a string is not a valid \l{QXmpp::Jid}{JID}.

    \since QXmpp 1.18
*/

/*!
    \enum QXmpp::JidParsingError::Type

    \value Empty The JID string is empty.
    \value InvalidLocalpart The localpart is empty or violates \rfcsection{7622}{3.3}.
    \value InvalidDomainpart The domainpart is empty or violates \rfcsection{7622}{3.2}.
    \value InvalidResourcepart The resourcepart is empty or violates \rfcsection{7622}{3.4}.
*/

/*!
    \variable QXmpp::JidParsingError::type

    Category of the error.
*/

/*!
    \variable QXmpp::JidParsingError::description

    Human-readable English description of the error, meant for logging and debugging.
*/

/*!
    \class QXmpp::Jid
    \inmodule QXmpp

    \brief An XMPP address (JID) as defined in \rfc{7622}.

    A Jid is always stored in its canonical form: the localpart is enforced using the PRECIS
    UsernameCaseMapped profile (\rfc{8265}), the domainpart is converted to Unicode labels
    (IDNA2008) and lowercased, and the resourcepart is enforced using the PRECIS OpaqueString
    profile. As a result, two Jids can be compared with \c{==} and used as hash keys directly.
    Note that this also means that the original case of the localpart and domainpart is not
    preserved: \c{Juliet@Example.com} becomes \c{juliet@example.com}.

    A Jid can only be created from valid input; a default-constructed Jid is null.

    \code
    auto jid = QXmpp::Jid::fromString(u"Juliet@Example.com/Balcony");
    jid->localpart();  // "juliet"
    jid->bare();       // juliet@example.com
    \endcode

    \since QXmpp 1.18
*/

/*!
    \fn QXmpp::Jid::Jid()

    Constructs a null Jid.
*/

Jid::Jid(QString jid, qsizetype atPos, qsizetype slashPos)
    : m_jid(std::move(jid)), m_atPos(atPos), m_slashPos(slashPos)
{
}

/*!
    Parses and enforces the JID string \a jid.

    The string is split into its parts as described in \rfcsection{7622}{3.2}, before any of
    the parts is normalized.
*/
std::variant<Jid, JidParsingError> Jid::parse(QStringView jid)
{
    if (jid.isEmpty()) {
        return error(JidParsingError::Empty, u"JID is empty."_s);
    }

    std::optional<QStringView> resource;
    if (auto slash = jid.indexOf(u'/'); slash >= 0) {
        resource = jid.sliced(slash + 1);
        jid = jid.first(slash);
    }
    std::optional<QStringView> localpart;
    if (auto at = jid.indexOf(u'@'); at >= 0) {
        localpart = jid.first(at);
        jid = jid.sliced(at + 1);
    }
    return build(localpart, jid, resource);
}

/*!
    Parses \a jid like parse(), but returns \c{std::nullopt} instead of an error.
*/
std::optional<Jid> Jid::fromString(QStringView jid)
{
    auto result = parse(jid);
    if (auto *value = std::get_if<Jid>(&result)) {
        return std::move(*value);
    }
    return {};
}

/*!
    Creates a Jid from its parts and enforces them.

    An empty \a localpart or \a resource means that the JID has no localpart or resourcepart.
    The \a domain is required. The \a localpart must already be escaped, see escapeLocalpart().
*/
std::variant<Jid, JidParsingError> Jid::fromParts(QStringView localpart, QStringView domain, QStringView resource)
{
    return build(localpart.isEmpty() ? std::nullopt : std::optional(localpart),
                 domain,
                 resource.isEmpty() ? std::nullopt : std::optional(resource));
}

// Whitespace and invisible characters, e.g. copied along with a JID.
static bool isIgnoredInUserInput(QChar c)
{
    switch (c.unicode()) {
    case 0x200B:  // zero width space
    case 0x200C:  // zero width non-joiner
    case 0x200D:  // zero width joiner
    case 0x200E:  // left-to-right mark
    case 0x200F:  // right-to-left mark
    case 0x202A:
    case 0x202B:
    case 0x202C:
    case 0x202D:
    case 0x202E:  // bidi embeddings and overrides
    case 0x2060:  // word joiner
    case 0x2066:
    case 0x2067:
    case 0x2068:
    case 0x2069:  // bidi isolates
    case 0xFEFF:  // byte order mark
        return true;
    default:
        return c.isSpace();
    }
}

static QStringView trimUserInput(QStringView input)
{
    while (!input.isEmpty() && isIgnoredInUserInput(input.front())) {
        input = input.sliced(1);
    }
    while (!input.isEmpty() && isIgnoredInUserInput(input.back())) {
        input.chop(1);
    }
    return input;
}

/*!
    Creates a Jid from a JID entered by a user, e.g. in a dialog for adding a contact.

    In contrast to parse(), whitespace and invisible characters at the beginning and end of
    \a input and of its parts are ignored, a slash at the end is ignored and the localpart is
    escaped as defined in \xep{0106}{JID Escaping}, so that a user can enter
    \c{d'artagnan@musketeers.lit}. The localpart ends at the last \c{@} before the first
    \c{/}, so the localpart of \c{user@host@example.com} is \c{user@host}.

    \code
    QXmpp::Jid::fromUserInput(u" d'artagnan@musketeers.lit ");  // d\27artagnan@musketeers.lit
    \endcode

    Localparts without characters that need to be escaped are used as they are, so that already
    escaped JIDs, e.g. copied from another client, are not escaped twice. Use toDisplayString()
    to show the result to the user.
*/
std::variant<Jid, JidParsingError> Jid::fromUserInput(QStringView input)
{
    input = trimUserInput(input);
    if (input.isEmpty()) {
        return error(JidParsingError::Empty, u"JID is empty."_s);
    }

    std::optional<QStringView> resource;
    if (auto slash = input.indexOf(u'/'); slash >= 0) {
        if (auto value = trimUserInput(input.sliced(slash + 1)); !value.isEmpty()) {
            resource = value;
        }
        input = input.first(slash);
    }

    std::optional<QString> localpart;
    if (auto at = input.lastIndexOf(u'@'); at >= 0) {
        auto raw = trimUserInput(input.first(at));
        auto needsEscaping = std::ranges::any_of(raw, [](QChar c) {
            return c != u'\\' && std::ranges::any_of(escapings, [c](const Escaping &e) { return c == QChar(e.character); });
        });
        localpart = needsEscaping ? escapeLocalpart(raw) : raw.toString();
        input = input.sliced(at + 1);
    }

    return build(localpart ? std::optional<QStringView>(*localpart) : std::nullopt, trimUserInput(input), resource);
}

std::variant<Jid, JidParsingError> Jid::build(std::optional<QStringView> localpart, QStringView domain, std::optional<QStringView> resource)
{
    QString local;
    if (localpart) {
        auto result = prepareLocalpart(*localpart);
        if (auto *err = std::get_if<JidParsingError>(&result)) {
            return *err;
        }
        local = std::get<QString>(std::move(result));
    }

    auto domainResult = prepareDomain(domain);
    if (auto *err = std::get_if<JidParsingError>(&domainResult)) {
        return *err;
    }

    QString res;
    if (resource) {
        auto result = prepareResource(*resource);
        if (auto *err = std::get_if<JidParsingError>(&result)) {
            return *err;
        }
        res = std::get<QString>(std::move(result));
    }

    QString jid;
    qsizetype atPos = -1;
    qsizetype slashPos = -1;
    if (localpart) {
        jid = local + u'@';
        atPos = local.size();
    }
    jid += std::get<QString>(domainResult);
    if (resource) {
        slashPos = jid.size();
        jid += u'/' + res;
    }
    return Jid(std::move(jid), atPos, slashPos);
}

/*!
    \fn bool QXmpp::Jid::isNull() const

    Returns true if this is a default-constructed Jid.
*/

/*!
    \fn bool QXmpp::Jid::isBare() const

    Returns true if the Jid is not null and has no resourcepart.
*/

/*!
    \fn bool QXmpp::Jid::isFull() const

    Returns true if the Jid has a resourcepart.
*/

/*!
    \fn bool QXmpp::Jid::isDomain() const

    Returns true if the Jid consists of a domainpart only.
*/

/*!
    \fn bool QXmpp::Jid::hasLocalpart() const

    Returns true if the Jid has a localpart.
*/

/*!
    \fn bool QXmpp::Jid::hasResource() const

    Returns true if the Jid has a resourcepart.
*/

/*!
    Returns the localpart in its escaped (wire) form, or an empty view if there is none.

    \sa unescapedLocalpart()
*/
QStringView Jid::localpart() const
{
    return m_atPos < 0 ? QStringView() : QStringView(m_jid).first(m_atPos);
}

/*!
    Returns the localpart with \xep{0106}{JID Escaping} reverted, for presentation to users.
*/
QString Jid::unescapedLocalpart() const
{
    return unescapeLocalpart(localpart());
}

/*!
    Returns the domainpart with Unicode labels (U-labels).

    \sa domainAce()
*/
QStringView Jid::domain() const
{
    auto begin = m_atPos + 1;
    auto end = m_slashPos < 0 ? m_jid.size() : m_slashPos;
    return QStringView(m_jid).sliced(begin, end - begin);
}

/*!
    Returns the domainpart in ASCII-compatible encoding (A-labels), e.g. for DNS lookups.

    IP literals are returned unchanged.
*/
QByteArray Jid::domainAce() const
{
    auto domain = this->domain();
    if (domain.isEmpty() || domain.front() == u'[') {
        return domain.toLatin1();
    }
    return QUrl::toAce(domain.toString(), QUrl::IgnoreIDNWhitelist);
}

/*!
    Returns the resourcepart, or an empty view if there is none.
*/
QStringView Jid::resource() const
{
    return m_slashPos < 0 ? QStringView() : QStringView(m_jid).sliced(m_slashPos + 1);
}

/*!
    Returns the Jid without its resourcepart.
*/
Jid Jid::bare() const
{
    if (m_slashPos < 0) {
        return *this;
    }
    return Jid(m_jid.first(m_slashPos), m_atPos, -1);
}

/*!
    Returns a Jid consisting only of the domainpart.
*/
Jid Jid::domainJid() const
{
    if (isNull()) {
        return {};
    }
    return Jid(domain().toString(), -1, -1);
}

/*!
    Returns a copy of the bare Jid with \a resource as resourcepart.

    Returns an error if this Jid is null or \a resource is not a valid resourcepart.
*/
std::variant<Jid, JidParsingError> Jid::withResource(QStringView resource) const
{
    if (isNull()) {
        return error(JidParsingError::Empty, u"JID is empty."_s);
    }
    if (resource.isEmpty()) {
        return error(JidParsingError::InvalidResourcepart, u"Resourcepart: String is empty."_s);
    }
    auto result = prepareResource(resource);
    if (auto *err = std::get_if<JidParsingError>(&result)) {
        return *err;
    }
    auto bareJid = bare().m_jid;
    auto slashPos = bareJid.size();
    return Jid(bareJid + u'/' + std::get<QString>(result), m_atPos, slashPos);
}

/*!
    \fn QString QXmpp::Jid::toString() const

    Returns the canonical string representation of the Jid, as used on the wire.
*/

/*!
    \fn QXmpp::Jid::operator QString() const

    Returns the canonical string representation of the Jid, like toString().

    This allows passing a Jid to functions taking a JID as QString. Comparisons with a QString
    compare the canonical form with the string as it is.
*/

/*!
    Returns the Jid for presentation to users, with \xep{0106}{JID Escaping} reverted.

    The result must not be used for anything else than display.
*/
QString Jid::toDisplayString() const
{
    if (m_atPos < 0) {
        return m_jid;
    }
    auto display = unescapedLocalpart();
    display += QStringView(m_jid).sliced(m_atPos);
    return display;
}

/*!
    Escapes the characters in \a localpart that are not allowed in a localpart according to
    \xep{0106}{JID Escaping}.

    Use this to build a localpart from user input or from an address of a foreign system.
    Leading and trailing spaces are removed, since they cannot be escaped.

    \code
    QXmpp::Jid::escapeLocalpart(u"d'artagnan");  // "d\27artagnan"
    \endcode
*/
QString Jid::escapeLocalpart(QStringView localpart)
{
    while (localpart.startsWith(u' ')) {
        localpart = localpart.sliced(1);
    }
    while (localpart.endsWith(u' ')) {
        localpart.chop(1);
    }

    auto isEscapeCode = [](QStringView s) {
        return s.size() >= 2 && std::ranges::any_of(escapings, [s](const Escaping &e) {
                   return s[0].toLower() == QChar(e.code[0]) && s[1].toLower() == QChar(e.code[1]);
               });
    };

    QString output;
    output.reserve(localpart.size());
    for (qsizetype i = 0; i < localpart.size(); ++i) {
        auto c = localpart[i];
        if (c == u'\\') {
            // Only a backslash that starts an escape sequence needs to be escaped itself.
            if (isEscapeCode(localpart.sliced(i + 1))) {
                output += u"\\5c";
            } else {
                output += c;
            }
            continue;
        }
        auto it = std::ranges::find(escapings, c.unicode(), &Escaping::character);
        if (it != std::end(escapings)) {
            output += u'\\';
            output += QChar(it->code[0]);
            output += QChar(it->code[1]);
        } else {
            output += c;
        }
    }
    return output;
}

/*!
    Reverts \xep{0106}{JID Escaping} of \a localpart.

    Only the escape sequences defined in XEP-0106 are replaced; any other backslash sequence is
    kept as is.
*/
QString Jid::unescapeLocalpart(QStringView localpart)
{
    if (!localpart.contains(u'\\')) {
        return localpart.toString();
    }

    QString output;
    output.reserve(localpart.size());
    for (qsizetype i = 0; i < localpart.size(); ++i) {
        if (localpart[i] == u'\\' && i + 2 < localpart.size()) {
            auto it = std::ranges::find_if(escapings, [&](const Escaping &e) {
                return localpart[i + 1] == QChar(e.code[0]) && localpart[i + 2] == QChar(e.code[1]);
            });
            if (it != std::end(escapings)) {
                output += QChar(it->character);
                i += 2;
                continue;
            }
        }
        output += localpart[i];
    }
    return output;
}

/*!
    \fn bool QXmpp::Jid::operator==(const Jid &a, const Jid &b)

    Returns true if \a a and \a b are the same JID (\rfcsection{7622}{3.2.3}, 3.3 and 3.4).
*/

/*!
    \fn std::strong_ordering QXmpp::Jid::operator<=>(const Jid &a, const Jid &b)

    Orders \a a and \a b by their canonical string representation.
*/

/*!
    \fn size_t QXmpp::Jid::qHash(const Jid &jid, size_t seed)

    Returns the hash value for \a jid using \a seed.
*/

/*!
    \relates QXmpp::Jid

    Writes \a jid to \a debug.
*/
QDebug operator<<(QDebug debug, const Jid &jid)
{
    QDebugStateSaver saver(debug);
    debug.nospace().noquote() << "QXmpp::Jid(" << jid.toString() << ')';
    return debug;
}

}  // namespace QXmpp

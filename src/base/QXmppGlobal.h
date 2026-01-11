// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPGLOBAL_H
#define QXMPPGLOBAL_H

#include "qxmpp_export.h"

#include <variant>

#include <QString>

struct QXmppError;

#define QXMPP_AUTOTEST_EXPORT
#define QXMPP_PRIVATE_EXPORT QXMPP_EXPORT

// This macro expands a numeric value of the form 0xMMNNPP (MM = major,
// NN = minor, PP = patch) that specifies QXmpp's version number. For
// example, if you compile your application against QXmpp 1.2.3, the
// QXMPP_VERSION macro will expand to 0x010203. You can use QXMPP_VERSION to
// use the latest QXmpp features where available.
#define QXMPP_VERSION QT_VERSION_CHECK(QXMPP_VERSION_MAJOR, QXMPP_VERSION_MINOR, QXMPP_VERSION_PATCH)

inline QLatin1String QXmppVersion()
{
    return QLatin1String(
        QT_STRINGIFY(QXMPP_VERSION_MAJOR) "." QT_STRINGIFY(QXMPP_VERSION_MINOR) "." QT_STRINGIFY(QXMPP_VERSION_PATCH));
}

// This sets which deprecated functions should still be usable
// It works exactly like QT_DISABLE_DEPRECATED_BEFORE
#ifndef QXMPP_DISABLE_DEPRECATED_BEFORE
#define QXMPP_DISABLE_DEPRECATED_BEFORE 0x0
#endif

// This works exactly like QT_DEPRECATED_SINCE, but checks QXMPP_DISABLE_DEPRECATED_BEFORE instead.
#define QXMPP_DEPRECATED_SINCE(major, minor) (QT_VERSION_CHECK(major, minor, 0) > QXMPP_DISABLE_DEPRECATED_BEFORE)

// Adds constructor and operator declarations to a ".h" file corresponding to the rule of six.
// A default constructor has to be declared manually.
#define QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(name) \
    name(const name &);                         \
    name(name &&) noexcept;                     \
    ~name();                                    \
    name &operator=(const name &);              \
    name &operator=(name &&) noexcept;

// Adds constructor and operator definitions to a ".cpp" file corresponding to the rule of six.
// A default constructor has to be defined manually.
#define QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(name)     \
    name::name(const name &) = default;            \
    name::name(name &&) noexcept = default;        \
    name::~name() = default;                       \
    name &name::operator=(const name &) = default; \
    name &name::operator=(name &&) noexcept = default;
// Same as QXMPP_PRIVATE_DEFINE_RULE_OF_SIX but for an inner class.
#define QXMPP_PRIVATE_DEFINE_RULE_OF_SIX_INNER(outer, name)             \
    outer::name::name(const outer::name &) = default;                   \
    outer::name::name(outer::name &&) noexcept = default;               \
    outer::name::~name() = default;                                     \
    outer::name &outer::name::operator=(const outer::name &) = default; \
    outer::name &outer::name::operator=(outer::name &&) noexcept = default;

/*!
    \namespace QXmpp
    \inmodule QXmpp

    Contains global functions and enumerations.

    \since QXmpp 1.5
*/
namespace QXmpp {

/*!
    This enum describes different end-to-end encryption methods. These can
    be used to mark a stanza as encrypted or decrypted with a specific algorithm
    (e.g., for \xep{0380}{Explicit Message Encryption}).

    \since QXmpp 1.5

    \value NoEncryption No encryption.
    \value UnknownEncryption Unknown encryption.
    \value Otr \xep{0364}{Current Off-the-Record Messaging Usage}.
    \value LegacyOpenPgp \xep{0027}{Current Jabber OpenPGP Usage}.
    \value Ox \xep{0373}{OpenPGP for XMPP}.
    \value Omemo0 \xep{0384}{OMEMO Encryption}.
    \value Omemo1 \xep{0384}{OMEMO Encryption} since version 0.4.
    \value Omemo2 \xep{0384}{OMEMO Encryption} since version 0.8.
    \value OTR \xep{0364}{Current Off-the-Record Messaging Usage} \deprecated This enum is deprecated since QXmpp 1.5. Use \c QXmpp::Otr instead.
    \value LegacyOpenPGP \xep{0027}{Current Jabber OpenPGP Usage} \deprecated This enum is deprecated since QXmpp 1.5. Use \c QXmpp::LegacyOpenPgp instead.
    \value OX \xep{0373}{OpenPGP for XMPP} \deprecated This enum is deprecated since QXmpp 1.5. Use \c QXmpp::Ox instead.
    \value OMEMO \xep{0384}{OMEMO Encryption} \deprecated This enum is deprecated since QXmpp 1.5. Use \c QXmpp::Omemo0 instead.
*/
enum EncryptionMethod {
    NoEncryption,
    UnknownEncryption,
    Otr,
    LegacyOpenPgp,
    Ox,
    Omemo0,
    Omemo1,
    Omemo2,

// Keep in sync with namespaces and names in Global/Message.cpp!

#if QXMPP_DEPRECATED_SINCE(1, 5)
    OTR = Otr,
    LegacyOpenPGP = LegacyOpenPgp,
    OX = Ox,
    OMEMO = Omemo0,
#endif
};

/*!
    Parsing/serialization mode when using Stanza Content Encryption
    (\xep{0420}{Stanza Content Encryption}).

    \since QXmpp 1.5

    \value SceAll Processes all known elements.
    \value ScePublic Only processes 'public' elements (e.g. needed for routing).
    \value SceSensitive Only processes sensitive elements that should be encrypted.
*/
enum SceMode : uint8_t {
    SceAll,
    ScePublic,
    SceSensitive,
};

/*!
    Returns true if a mode is enabled in both \a mode1 and \a mode2.

    When an SceMode is given you can use this to check whether Public or Private
    elements are enabled.

    \since QXmpp 1.5
*/
inline constexpr bool operator&(SceMode mode1, SceMode mode2)
{
    return mode1 == SceAll || mode1 == mode2;
}

/*!
    Cipher for encrypting data streams and files.

    \since QXmpp 1.5

    \value Aes128GcmNoPad
    \value Aes256GcmNoPad
    \value Aes256CbcPkcs7
*/
enum Cipher {
    Aes128GcmNoPad,
    Aes256GcmNoPad,
    Aes256CbcPkcs7,
};

/*!
    \inmodule QXmpp

    An empty struct indicating success in results.

    \since QXmpp 1.5
*/
struct Success { };

/*!
    \inmodule QXmpp

    Unit struct used to indicate that a process has been cancelled.

    \since QXmpp 1.5
*/
struct Cancelled { };

/*!
    \inmodule QXmpp

    Unit struct indicating a client-side timeout (or keep-alive) error.

    It occurs if no response is received from the connected entity within a given timeout.

    \since QXmpp 1.7
*/
struct TimeoutError { };

/*!
    \typealias QXmpp::Result

    Generic result type offering value or QXmppError.

    T defaults to QXmpp::Success since QXmpp 1.13.

    \since QXmpp 1.12
*/
template<typename T = Success>
using Result = std::variant<T, QXmppError>;

/*!
    Returns whether a result contains the expected value
    \since QXmpp 1.13

    \a r.
*/
template<typename T>
bool hasValue(const Result<T> &r) { return std::holds_alternative<T>(r); }

/*!
    Returns whether a result contains an error

    \since QXmpp 1.13

    \a r.
*/
template<typename T>
bool hasError(const Result<T> &r) { return std::holds_alternative<QXmppError>(r); }

/*!
    Returns the value of a result or throws

    \since QXmpp 1.13

    \a r.
*/
template<typename T>
const T &getValue(const Result<T> &r) { return std::get<T>(r); }

/*!
    Returns the value of a result or throws

    \since QXmpp 1.13

    \a r.
*/
template<typename T>
T &getValue(Result<T> &r) { return std::get<T>(r); }

/*!
    Returns the value of a result or throws

    \since QXmpp 1.13
*/
template<typename T>
T getValue(Result<T> &&r) { return std::get<T>(std::move(r)); }

// getError() is defined in QXmppError.h

}  // namespace QXmpp

#endif  // QXMPPGLOBAL_H

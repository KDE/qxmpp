// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTREAMERROR_H
#define QXMPPSTREAMERROR_H

namespace QXmpp {

/*!
    All XMPP top-level stream errors

    \since QXmpp 1.7

    \value BadFormat
    \value BadNamespacePrefix
    \value Conflict
    \value ConnectionTimeout
    \value HostGone
    \value HostUnknown
    \value ImproperAddressing
    \value InternalServerError
    \value InvalidFrom
    \value InvalidId
    \value InvalidNamespace
    \value InvalidXml
    \value NotAuthorized
    \value NotWellFormed
    \value PolicyViolation
    \value RemoteConnectionFailed
    \value Reset
    \value ResourceConstraint
    \value RestrictedXml
    \value SystemShutdown
    \value UndefinedCondition
    \value UnsupportedEncoding
    \value UnsupportedFeature
    \value UnsupportedStanzaType
    \value UnsupportedVersion
*/
enum class StreamError {
    BadFormat,
    BadNamespacePrefix,
    Conflict,
    ConnectionTimeout,
    HostGone,
    HostUnknown,
    ImproperAddressing,
    InternalServerError,
    InvalidFrom,
    InvalidId,
    InvalidNamespace,
    InvalidXml,
    NotAuthorized,
    NotWellFormed,
    PolicyViolation,
    RemoteConnectionFailed,
    Reset,
    ResourceConstraint,
    RestrictedXml,
    SystemShutdown,
    UndefinedCondition,
    UnsupportedEncoding,
    UnsupportedFeature,
    UnsupportedStanzaType,
    UnsupportedVersion,
};

}  // namespace QXmpp

#endif  // QXMPPSTREAMERROR_H

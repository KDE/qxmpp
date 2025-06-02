// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef STREAMERROR_H
#define STREAMERROR_H

#include "QXmppError.h"
#include "QXmppStreamError.h"

#include "Enums.h"

#include <variant>

class QDomElement;
class QXmlStreamWriter;
namespace QXmpp::Private {
class XmlWriter;
}

namespace QXmpp::Private {

template<>
struct Enums::Data<StreamError> {
    using enum StreamError;
    static constexpr auto Values = makeValues<StreamError>({
        { BadFormat, u"bad-format" },
        { BadNamespacePrefix, u"bad-namespace-prefix" },
        { Conflict, u"conflict" },
        { ConnectionTimeout, u"connection-timeout" },
        { HostGone, u"host-gone" },
        { HostUnknown, u"host-unknown" },
        { ImproperAddressing, u"improper-addressing" },
        { InternalServerError, u"internal-server-error" },
        { InvalidFrom, u"invalid-from" },
        { InvalidId, u"invalid-id" },
        { InvalidNamespace, u"invalid-namespace" },
        { InvalidXml, u"invalid-xml" },
        { NotAuthorized, u"not-authorized" },
        { NotWellFormed, u"not-well-formed" },
        { PolicyViolation, u"policy-violation" },
        { RemoteConnectionFailed, u"remote-connection-failed" },
        { Reset, u"reset" },
        { ResourceConstraint, u"resource-constraint" },
        { RestrictedXml, u"restricted-xml" },
        { SystemShutdown, u"system-shutdown" },
        { UndefinedCondition, u"undefined-condition" },
        { UnsupportedEncoding, u"unsupported-encoding" },
        { UnsupportedFeature, u"unsupported-feature" },
        { UnsupportedStanzaType, u"unsupported-stanza-type" },
        { UnsupportedVersion, u"unsupported-version" },
    });
};

// implemented in Stream.cpp
struct StreamErrorElement {
    struct SeeOtherHost {
        QString host;
        quint16 port;

        bool operator==(const SeeOtherHost &o) const { return host == o.host && port == o.port; }
    };

    using Condition = std::variant<StreamError, SeeOtherHost>;

    static std::variant<StreamErrorElement, QXmppError> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;

    Condition condition;
    QString text;

    bool operator==(const StreamErrorElement &o) const { return condition == o.condition && text == o.text; }
};

}  // namespace QXmpp::Private

#endif  // STREAMERROR_H

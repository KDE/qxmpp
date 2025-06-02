// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTANZA_P_H
#define QXMPPSTANZA_P_H

#include "QXmppStanza.h"

#include "Enums.h"

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QXmpp API. It exists for the convenience
// of the QXmppStanza class.
//
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

namespace QXmpp::Private {

template<>
struct Enums::Data<QXmppStanza::Error::Condition> {
    using enum QXmppStanza::Error::Condition;
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    static constexpr auto Values = makeValues<QXmppStanza::Error::Condition>({
        { NoCondition, {} },
        { BadRequest, u"bad-request" },
        { Conflict, u"conflict" },
        { FeatureNotImplemented, u"feature-not-implemented" },
        { Forbidden, u"forbidden" },
        { Gone, u"gone" },
        { InternalServerError, u"internal-server-error" },
        { ItemNotFound, u"item-not-found" },
        { JidMalformed, u"jid-malformed" },
        { NotAcceptable, u"not-acceptable" },
        { NotAllowed, u"not-allowed" },
        { NotAuthorized, u"not-authorized" },
        { PaymentRequired, u"payment-required" },
        { RecipientUnavailable, u"recipient-unavailable" },
        { Redirect, u"redirect" },
        { RegistrationRequired, u"registration-required" },
        { RemoteServerNotFound, u"remote-server-not-found" },
        { RemoteServerTimeout, u"remote-server-timeout" },
        { ResourceConstraint, u"resource-constraint" },
        { ServiceUnavailable, u"service-unavailable" },
        { SubscriptionRequired, u"subscription-required" },
        { UndefinedCondition, u"undefined-condition" },
        { UnexpectedRequest, u"unexpected-request" },
        { PolicyViolation, u"policy-violation" },
    });
    QT_WARNING_POP
};

template<>
struct Enums::Data<QXmppStanza::Error::Type> {
    using enum QXmppStanza::Error::Type;
    static constexpr auto NullValue = NoType;
    static constexpr auto Values = makeValues<QXmppStanza::Error::Type>({
        { NoType, {} },
        { Cancel, u"cancel" },
        { Continue, u"continue" },
        { Modify, u"modify" },
        { Auth, u"auth" },
        { Wait, u"wait" },
    });
};

}  // namespace QXmpp::Private

#endif

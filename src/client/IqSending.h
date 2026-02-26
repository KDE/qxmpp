// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef IQSENDING_H
#define IQSENDING_H

#include "QXmppClient.h"
#include "QXmppUtils.h"

#include "Iq.h"

namespace QXmpp::Private {

// helpers to be added to the QXmppClient in the future

template<typename Response, typename Payload>
static QXmppTask<std::variant<Response, QXmppError>> get(QXmppClient *client, const QString &to, Payload &&payload)
{
    return chain<std::variant<Response, QXmppError>>(
        client->sendIq(CompatIq { GetIq<Payload> { generateSequentialStanzaId(), {}, to, {}, std::move(payload) } }),
        client,
        parseIqResponseFlat<Response>);
}

template<typename Response, typename Payload>
static QXmppTask<std::variant<Response, QXmppError>> set(QXmppClient *client, const QString &to, Payload &&payload)
{
    return chain<std::variant<Response, QXmppError>>(
        client->sendIq(CompatIq { SetIq<Payload> { generateSequentialStanzaId(), {}, to, {}, std::move(payload) } }),
        client,
        parseIqResponseFlat<Response>);
}

// Set IQ without response payload — returns Result<Success> (success or error)
template<typename Payload>
static QXmppTask<QXmpp::Result<>> set(QXmppClient *client, const QString &to, Payload &&payload)
{
    return client->sendGenericIq(CompatIq { SetIq<std::decay_t<Payload>> { generateSequentialStanzaId(), {}, to, {}, std::forward<Payload>(payload) } });
}

}  // namespace QXmpp::Private

#endif  // IQSENDING_H

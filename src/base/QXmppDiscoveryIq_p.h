// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYIQ_P_H
#define QXMPPDISCOVERYIQ_P_H

#include "QXmppDiscoveryIq.h"
#include "QXmppHash.h"

#include "Enums.h"

class QDomElement;

namespace QXmpp::Private {

using namespace Enums;

template<>
struct Enums::Data<Disco::Category> {
    using enum Disco::Category;
    static constexpr auto Values = makeValues<Disco::Category>({
        { Account, u"account" },
        { Auth, u"auth" },
        { Automation, u"automation" },
        { Client, u"client" },
        { Collaboration, u"collaboration" },
        { Component, u"component" },
        { Conference, u"conference" },
        { Directory, u"directory" },
        { Gateway, u"gateway" },
        { Headline, u"headline" },
        { Hierarchy, u"hierarchy" },
        { Proxy, u"proxy" },
        { PubSub, u"pubsub" },
        { Server, u"server" },
        { Store, u"store" },
    });
};

template<>
struct Enums::Data<Disco::Type> {
    using enum Disco::Type;
    static constexpr auto Values = makeValues<Disco::Type>({
        { Bot, u"bot" },
        { Console, u"console" },
        { Game, u"game" },
        { Handheld, u"handheld" },
        { Pc, u"pc" },
        { Phone, u"phone" },
        { Sms, u"sms" },
        { Tablet, u"tablet" },
        { Web, u"web" },
        { Irc, u"irc" },
        { Mix, u"mix" },
        { Text, u"text" },
        { Collection, u"collection" },
        { Leaf, u"leaf" },
        { Pep, u"pep" },
        { Service, u"service" },
        { File, u"file" },
    });
};

// XEP-0115: Entity Capabilities
// Returns the algorithm of a 'hash' attribute if it can be used to verify the information.
QXMPP_EXPORT std::optional<HashAlgorithm> capsHashAlgorithm(QStringView hash);
// Generates the verification string from a disco#info query element, which unlike
// QXmppDiscoInfo keeps the values exactly as received. Returns nothing if the response is
// ill-formed or the algorithm is not supported.
QXMPP_EXPORT std::optional<QByteArray> capsVerificationString(const QDomElement &query, HashAlgorithm algorithm);

}  // namespace QXmpp::Private

#endif  // QXMPPDISCOVERYIQ_P_H

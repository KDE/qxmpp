// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BIND_H
#define BIND_H

#include "QXmppPackets_p.h"

#include "Xml.h"

namespace QXmpp::Private {

// struct in QXmppPackets_p.h

template<>
struct XmlSpec<Bind> {
    static constexpr std::tuple Spec = {
        XmlOptionalTextElement { &Bind::jid, u"jid" },
        XmlOptionalTextElement { &Bind::resource, u"resource" },
    };
};

}  // namespace QXmpp::Private

#endif  // BIND_H

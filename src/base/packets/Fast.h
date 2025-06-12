// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FAST_H
#define FAST_H

#include "QXmppConstants_p.h"

#include "Xml.h"

namespace QXmpp::Private {

struct FastFeature {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };

    bool tls0rtt = false;
    std::vector<QString> mechanisms;
};

struct FastTokenRequest {
    static constexpr std::tuple XmlTag = { u"request-token", ns_fast };

    QString mechanism;
};

struct FastToken {
    static constexpr std::tuple XmlTag = { u"token", ns_fast };

    QDateTime expiry;
    QString token;
};

struct FastRequest {
    static constexpr std::tuple XmlTag = { u"fast", ns_fast };

    std::optional<uint64_t> count;
    bool invalidate = false;
};

template<>
struct XmlSpec<FastFeature> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &FastFeature::tls0rtt, u"tls-0rtt", BoolDefaultSerializer(false) },
        XmlTextElements { &FastFeature::mechanisms, u"mechanism" },
    };
};

template<>
struct XmlSpec<FastTokenRequest> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &FastTokenRequest::mechanism, u"mechanism" },
    };
};

template<>
struct XmlSpec<FastToken> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &FastToken::expiry, u"expiry" },
        XmlAttribute { &FastToken::token, u"token" },
    };
};

template<>
struct XmlSpec<FastRequest> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &FastRequest::count, u"count" },
        XmlOptionalAttribute { &FastRequest::invalidate, u"invalidate", BoolDefaultSerializer(false) },
    };
};

}  // namespace QXmpp::Private

#endif  // FAST_H

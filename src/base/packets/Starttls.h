// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef STARTTLS_H
#define STARTTLS_H

#include "Xml.h"

namespace QXmpp::Private {

struct StarttlsRequest {
    static constexpr std::tuple XmlTag = { u"starttls", ns_tls };
};

struct StarttlsProceed {
    static constexpr std::tuple XmlTag = { u"proceed", ns_tls };
};

template<>
struct XmlSpec<StarttlsRequest> {
    static constexpr std::tuple Spec = {};
};

template<>
struct XmlSpec<StarttlsProceed> {
    static constexpr std::tuple Spec = {};
};

}  // namespace QXmpp::Private

#endif  // STARTTLS_H

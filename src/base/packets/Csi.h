// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef CSI_H
#define CSI_H

#include "Xml.h"

namespace QXmpp::Private {

struct CsiActive {
    static constexpr std::tuple XmlTag = { u"active", ns_csi };
};

struct CsiInactive {
    static constexpr std::tuple XmlTag = { u"inactive", ns_csi };
};

template<>
struct XmlSpec<CsiActive> {
    static constexpr std::tuple Spec = {};
};

template<>
struct XmlSpec<CsiInactive> {
    static constexpr std::tuple Spec = {};
};

}  // namespace QXmpp::Private

#endif  // CSI_H

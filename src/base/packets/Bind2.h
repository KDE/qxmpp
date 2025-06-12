// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BIND2_H
#define BIND2_H

#include "StreamManagement.h"

namespace QXmpp::Private {

struct Bind2Feature {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    std::vector<QString> features;
};

struct Bind2Request {
    static constexpr std::tuple XmlTag = { u"bind", ns_bind2 };
    QString tag;
    // bind2 extensions
    bool csiInactive = false;
    bool carbonsEnable = false;
    std::optional<SmEnable> smEnable;
};

struct Bind2Bound {
    static constexpr std::tuple XmlTag = { u"bound", ns_bind2 };
    // extensions
    std::optional<SmFailed> smFailed;
    std::optional<SmEnabled> smEnabled;
};

template<>
struct XmlSpec<Bind2Feature> {
    static constexpr std::tuple Spec = {
        XmlElement {
            u"inline",
            Optional(),
            XmlSingleAttributeElements { &Bind2Feature::features, Tag { u"feature", ns_bind2 }, u"var" },
        },
    };
};

template<>
struct XmlSpec<Bind2Request> {
    static constexpr std::tuple Spec = {
        XmlElement { u"tag", Optional(), XmlText { &Bind2Request::tag } },
        XmlReference { &Bind2Request::csiInactive, { u"inactive", ns_csi } },
        XmlReference { &Bind2Request::carbonsEnable, { u"enable", ns_carbons } },
        XmlReference { &Bind2Request::smEnable },
    };
};

template<>
struct XmlSpec<Bind2Bound> {
    static constexpr std::tuple Spec = {
        XmlReference { &Bind2Bound::smFailed },
        XmlReference { &Bind2Bound::smEnabled },
    };
};

}  // namespace QXmpp::Private

#endif  // BIND2_H

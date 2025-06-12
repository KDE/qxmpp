// SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef STREAMMANAGEMENT_H
#define STREAMMANAGEMENT_H

#include "QXmppConstants_p.h"
#include "QXmppStanza_p.h"

#include "Xml.h"

namespace QXmpp::Private {

struct SmEnable {
    static constexpr std::tuple XmlTag = { u"enable", ns_stream_management };
    bool resume = false;
    uint64_t max;
};

struct SmEnabled {
    static constexpr std::tuple XmlTag = { u"enabled", ns_stream_management };
    bool resume = false;
    QString id;
    uint64_t max = 0;
    QString location;
};

struct SmResume {
    static constexpr std::tuple XmlTag = { u"resume", ns_stream_management };
    uint32_t h = 0;
    QString previd;
};

struct SmResumed {
    static constexpr std::tuple XmlTag = { u"resumed", ns_stream_management };
    uint32_t h = 0;
    QString previd;
};

struct SmFailed {
    static constexpr std::tuple XmlTag = { u"failed", ns_stream_management };
    std::optional<uint32_t> h;
    std::optional<QXmppStanza::Error::Condition> error;
};

struct SmAck {
    static constexpr std::tuple XmlTag = { u"a", ns_stream_management };
    uint32_t seqNo = 0;
};

struct SmRequest {
    static constexpr std::tuple XmlTag = { u"r", ns_stream_management };
};

template<>
struct XmlSpec<SmEnable> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmEnable::resume, u"resume", BoolDefaultSerializer { false } },
        XmlOptionalAttribute { &SmEnable::max, u"max", PositiveIntSerializer() },
    };
};

template<>
struct XmlSpec<SmEnabled> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmEnabled::resume, u"resume", BoolDefaultSerializer { false } },
        XmlOptionalAttribute { &SmEnabled::id, u"id" },
        XmlOptionalAttribute { &SmEnabled::max, u"max", PositiveIntSerializer() },
        XmlOptionalAttribute { &SmEnabled::location, u"location" },
    };
};

template<>
struct XmlSpec<SmResume> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmResume::h, u"h" },
        XmlAttribute { &SmResume::previd, u"previd" },
    };
};

template<>
struct XmlSpec<SmResumed> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmResumed::h, u"h" },
        XmlAttribute { &SmResumed::previd, u"previd" },
    };
};

template<>
struct XmlSpec<SmFailed> {
    static constexpr std::tuple Spec = {
        XmlOptionalAttribute { &SmFailed::h, u"h" },
        XmlOptionalEnumElement { &SmFailed::error, ns_stanza },
    };
};

template<>
struct XmlSpec<SmAck> {
    static constexpr std::tuple Spec = {
        XmlAttribute { &SmAck::seqNo, u"h" },
    };
};

template<>
struct XmlSpec<SmRequest> {
    static constexpr std::tuple Spec = {};
};

}  // namespace QXmpp::Private

#endif  // STREAMMANAGEMENT_H

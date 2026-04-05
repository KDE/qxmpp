// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPP_PING_H
#define QXMPP_PING_H

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"

#include "XmlWriter.h"

#include <optional>
#include <tuple>

#include <QDomElement>

namespace QXmpp::Private {

struct Ping {
    static constexpr std::tuple XmlTag = { u"ping", ns_ping };

    static std::optional<Ping> fromDom(const QDomElement &el)
    {
        if (elementXmlTag(el) != XmlTag) {
            return {};
        }
        return Ping {};
    }

    void toXml(QXmlStreamWriter *writer) const
    {
        XmlWriter(writer).write(Element { XmlTag });
    }
};

}  // namespace QXmpp::Private

#endif  // QXMPP_PING_H

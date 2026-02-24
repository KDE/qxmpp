// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucData.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace QXmpp::Muc {

std::optional<HistoryOptions> HistoryOptions::fromDom(const QDomElement &el)
{
    HistoryOptions opts;
    auto parseInt = [&](const QString &name) -> std::optional<int> {
        if (!el.hasAttribute(name)) {
            return std::nullopt;
        }
        return el.attribute(name).toInt();
    };
    opts.m_maxChars = parseInt(u"maxchars"_s);
    opts.m_maxStanzas = parseInt(u"maxstanzas"_s);
    opts.m_seconds = parseInt(u"seconds"_s);
    if (el.hasAttribute(u"since"_s)) {
        opts.m_since = QXmppUtils::datetimeFromString(el.attribute(u"since"_s));
    }
    return opts;
}

void HistoryOptions::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"history",
        OptionalAttribute { u"maxchars", m_maxChars },
        OptionalAttribute { u"maxstanzas", m_maxStanzas },
        OptionalAttribute { u"seconds", m_seconds },
        OptionalAttribute { u"since", m_since },
    });
}

}  // namespace QXmpp::Muc

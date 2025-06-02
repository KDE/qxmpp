// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef STREAM_H
#define STREAM_H

#include "QXmppConstants_p.h"

#include <optional>

#include <QMetaType>
#include <QString>

class QDomElement;
class QXmlStreamReader;
namespace QXmpp::Private {
class XmlWriter;
}

namespace QXmpp::Private {

struct StreamOpen {
    static StreamOpen fromXml(QXmlStreamReader &reader);
    void toXml(XmlWriter &) const;

    QString to;
    QString from;
    QString id;
    QString version;
    QString xmlns;
};

struct StarttlsRequest {
    static constexpr std::tuple XmlTag = { u"starttls", ns_tls };
    static std::optional<StarttlsRequest> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;
};

struct StarttlsProceed {
    static constexpr std::tuple XmlTag = { u"proceed", ns_tls };
    static std::optional<StarttlsProceed> fromDom(const QDomElement &);
    void toXml(XmlWriter &) const;
};

struct CsiActive {
    static constexpr std::tuple XmlTag = { u"active", ns_csi };
    void toXml(XmlWriter &) const;
};

struct CsiInactive {
    static constexpr std::tuple XmlTag = { u"inactive", ns_csi };
    void toXml(XmlWriter &) const;
};

}  // namespace QXmpp::Private

Q_DECLARE_METATYPE(QXmpp::Private::StreamOpen)

#endif  // STREAM_H

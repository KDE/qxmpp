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

}  // namespace QXmpp::Private

Q_DECLARE_METATYPE(QXmpp::Private::StreamOpen)

#endif  // STREAM_H

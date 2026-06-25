// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPBITSOFBINARYDATACONTAINER_H
#define QXMPPBITSOFBINARYDATACONTAINER_H

#include "QXmppBitsOfBinaryData.h"

#include <optional>

#include <QList>

class QDomElement;
class QXmlStreamWriter;

class QXMPP_EXPORT QXmppBitsOfBinaryDataList : public QList<QXmppBitsOfBinaryData>
{
public:
    QXmppBitsOfBinaryDataList();
    ~QXmppBitsOfBinaryDataList();

    std::optional<QXmppBitsOfBinaryData> find(const QXmppBitsOfBinaryContentId &cid) const;

    QXmppBitsOfBinaryDataList(const QList<QXmppBitsOfBinaryData> &data) : QList<QXmppBitsOfBinaryData>(data) { }
    QXmppBitsOfBinaryDataList(QList<QXmppBitsOfBinaryData> &&data) : QList<QXmppBitsOfBinaryData>(std::move(data)) { }

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;
};

#endif  // QXMPPBITSOFBINARYDATACONTAINER_H

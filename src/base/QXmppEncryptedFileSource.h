// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPENCRYPTEDFILESOURCE_H
#define QXMPPENCRYPTEDFILESOURCE_H

#include "QXmppGlobal.h"
#include "QXmppHash.h"
#include "QXmppHttpFileSource.h"

#include <QList>
#include <QSharedDataPointer>
#include <QUrl>

class QXmppEncryptedFileSourcePrivate;

// exported for tests
class QXMPP_EXPORT QXmppEncryptedFileSource
{
public:
    QXmppEncryptedFileSource();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppEncryptedFileSource)

    QXmpp::Cipher cipher() const;
    void setCipher(QXmpp::Cipher newCipher);

    const QByteArray &key() const;
    void setKey(const QByteArray &newKey);

    const QByteArray &iv() const;
    void setIv(const QByteArray &newIv);

    const QList<QXmppHash> &hashes() const;
    void setHashes(const QList<QXmppHash> &newHashes);

    const QList<QXmppHttpFileSource> &httpSources() const;
    void setHttpSources(const QList<QXmppHttpFileSource> &newHttpSources);

    static constexpr std::tuple XmlTag = { u"encrypted", QXmpp::Private::ns_esfs };
    bool parse(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;

private:
    QSharedDataPointer<QXmppEncryptedFileSourcePrivate> d;
};

#endif  // QXMPPENCRYPTEDFILESOURCE_H

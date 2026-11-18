// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPE2EEMETADATA_H
#define QXMPPE2EEMETADATA_H

#include "QXmppGlobal.h"

#include <optional>

#include <QSharedDataPointer>

class QDateTime;
class QXmppE2eeMetadataPrivate;

namespace QXmpp::E2ee {

// What an encryption encrypts of a message. Defined here instead of in QXmppE2eeProvider.h, since
// QXmppE2eeMetadata reports it for received messages and OutgoingMessage and Sent for sent ones.
enum class ContentEncryption {
    // Stanza Content Encryption (XEP-0420): all sensitive elements, e.g. OMEMO 2
    Sce,
    // only the body, other sensitive elements are sent in the clear if allowed by PrepareBodyOnly
    // or dropped, e.g. OMEMO 0
    BodyOnly,
};

}  // namespace QXmpp::E2ee

class QXMPP_EXPORT QXmppE2eeMetadata
{
public:
    QXmppE2eeMetadata();
    QXmppE2eeMetadata(QSharedDataPointer<QXmppE2eeMetadataPrivate> d);
    QXmppE2eeMetadata(const QXmppE2eeMetadata &other);
    QXmppE2eeMetadata(QXmppE2eeMetadata &&);
    ~QXmppE2eeMetadata();

    QXmppE2eeMetadata &operator=(const QXmppE2eeMetadata &other);
    QXmppE2eeMetadata &operator=(QXmppE2eeMetadata &&);

    QXmpp::EncryptionMethod encryption() const;
    void setEncryption(QXmpp::EncryptionMethod encryption);
    QString encryptionNamespace() const;
    void setEncryptionNamespace(const QString &encryption);

    std::optional<QXmpp::E2ee::ContentEncryption> contentEncryption() const;
    void setContentEncryption(std::optional<QXmpp::E2ee::ContentEncryption> contentEncryption);

    QByteArray senderKey() const;
    void setSenderKey(const QByteArray &keyId);

    // XEP-0420: Stanza Content Encryption
    QDateTime sceTimestamp() const;
    void setSceTimestamp(const QDateTime &timestamp);

private:
    friend class QXmppStanza;

    QSharedDataPointer<QXmppE2eeMetadataPrivate> d;
};

#endif  // QXMPPE2EEMETADATA_H

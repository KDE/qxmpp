// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPFILESHARE_P_H
#define QXMPPFILESHARE_P_H

#include "QXmppBitsOfBinaryData.h"
#include "QXmppFileShare.h"

#include <optional>

class QXmppEncryptedFileSource;

// XEP-0454: OMEMO Media Sharing
namespace QXmpp::Private::OmemoMediaSharing {

// The aesgcm:// URL of an encrypted file source, nullopt if it can not be expressed as one: only
// AES-256-GCM with an HTTPS source can.
QXMPP_EXPORT std::optional<QString> aesGcmUrl(const QXmppEncryptedFileSource &source);

struct Body {
    QXmppFileShare fileShare;
    // thumbnail referenced by the metadata of the file share
    std::optional<QXmppBitsOfBinaryData> thumbnail;
};

// Parses a body consisting of an aesgcm:// URL, optionally followed by a JPEG thumbnail as data
// URI in the next line. nullopt if the body contains anything else.
QXMPP_EXPORT std::optional<Body> parseBody(QStringView body);

}  // namespace QXmpp::Private::OmemoMediaSharing

#endif  // QXMPPFILESHARE_P_H

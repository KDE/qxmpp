// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPHTTPUPLOADIQ_P_H
#define QXMPPHTTPUPLOADIQ_P_H

#include "QXmppHttpUploadIq.h"

#include "Enums.h"

namespace QXmpp::Private {

template<>
struct Enums::Data<HttpUpload::Purpose> {
    using enum HttpUpload::Purpose;
    static constexpr auto Values = makeValues<HttpUpload::Purpose>({
        { Message, u"message" },
        { Profile, u"profile" },
        { Ephemeral, u"ephemeral" },
        { Permanent, u"permanent" },
    });
};

QXMPP_PRIVATE_EXPORT QString httpUploadPurposeFeature(HttpUpload::Purpose purpose);

// Returns each header once: the values of Cookie are combined, of Authorization
// and Expires only the first value is used.
QXMPP_PRIVATE_EXPORT QList<std::pair<QByteArray, QByteArray>> mergeHttpUploadHeaders(const QList<HttpUpload::Header> &headers);

}  // namespace QXmpp::Private

#endif  // QXMPPHTTPUPLOADIQ_P_H

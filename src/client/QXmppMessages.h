// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMESSAGES_H
#define QXMPPMESSAGES_H

#include "QXmppHooks.h"

namespace QXmpp::Messages {

class Context;

enum class Origin {
    Stream,
    CarbonReceived,
    CarbonSent,
    Mam,
    Injected,
};

enum class DecryptionState {
    NotEncrypted,
    Decrypted,
    NoEnvelope,
    Failed,
    Unsupported,
};

struct ResolveSender {
    static constexpr QStringView Name = u"org.qxmpp.message.resolve-sender";
    using Signature = QXmppTask<QXmpp::HookResult>(Context &context);
};

struct Received {
    static constexpr QStringView Name = u"org.qxmpp.message.received";
    using Signature = QXmpp::HookResult(Context &context);
};

}  // namespace QXmpp::Messages

#endif  // QXMPPMESSAGES_H

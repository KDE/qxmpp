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

// Attached to the Context of messages with Origin::Mam.
class ArchiveInfo
{
public:
    // Bare JID of the archive, e.g. the own account or a MUC room.
    const QString &archiveJid() const { return m_archiveJid; }
    void setArchiveJid(const QString &jid) { m_archiveJid = jid; }

    // ID of the message in the archive.
    const QString &id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }

    // Real JID of the sender as reported by a MUC archive (XEP-0313), empty if there is none
    // or more than one. Not validated, see QXmppMucManagerV2.
    const QString &occupantJid() const { return m_occupantJid; }
    void setOccupantJid(const QString &jid) { m_occupantJid = jid; }

private:
    QString m_archiveJid;
    QString m_id;
    QString m_occupantJid;
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

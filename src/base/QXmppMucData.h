// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCDATA_H
#define QXMPPMUCDATA_H

#include "QXmppConstants_p.h"
#include "QXmppGlobal.h"

#include <optional>

#include <QDateTime>

class QDomElement;
class QXmlStreamWriter;

namespace QXmpp {

namespace Muc {

///
/// \brief History options for joining a MUC room (XEP-0045 §7.2.13).
///
/// This class allows you to restrict the room history sent to the client upon joining.
/// All options are optional; if none are set, the server's default history is sent.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT HistoryOptions
{
public:
    /// Returns the maximum number of characters of history to request, or \c std::nullopt if unrestricted.
    std::optional<int> maxChars() const { return m_maxChars; }
    /// Sets the maximum number of characters of history to request. Pass \c std::nullopt for unrestricted.
    void setMaxChars(std::optional<int> value) { m_maxChars = value; }

    /// Returns the maximum number of history stanzas to request, or \c std::nullopt if unrestricted.
    std::optional<int> maxStanzas() const { return m_maxStanzas; }
    /// Sets the maximum number of history stanzas to request. Pass \c std::nullopt for unrestricted.
    void setMaxStanzas(std::optional<int> value) { m_maxStanzas = value; }

    /// Returns the seconds window for history, or \c std::nullopt if unrestricted.
    std::optional<int> seconds() const { return m_seconds; }
    /// Sets the seconds window for history. Pass \c std::nullopt for unrestricted.
    void setSeconds(std::optional<int> value) { m_seconds = value; }

    /// Returns the earliest time to include in history, or \c std::nullopt if unrestricted.
    std::optional<QDateTime> since() const { return m_since; }
    /// Sets the earliest time to include in history. Pass \c std::nullopt for unrestricted.
    void setSince(std::optional<QDateTime> value) { m_since = value; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"history", QXmpp::Private::ns_muc };
    static std::optional<HistoryOptions> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    std::optional<int> m_maxChars;
    std::optional<int> m_maxStanzas;
    std::optional<int> m_seconds;
    std::optional<QDateTime> m_since;
};

enum class Affiliation {
    Outcast,
    None,
    Member,
    Admin,
    Owner,
};

enum class Role {
    None,
    Visitor,
    Participant,
    Moderator,
};

///
/// \brief Reason why a participant left a MUC room.
///
/// \since QXmpp 1.15
///
enum class LeaveReason {
    /// The participant left the room voluntarily.
    Left,
    /// The participant was kicked by a moderator (XEP-0045 §8.2, status 307).
    Kicked,
    /// The participant was banned from the room (XEP-0045 §8.4, status 301).
    Banned,
    /// The participant was removed because their affiliation changed (status 321).
    AffiliationChanged,
    /// The participant was removed because the room became members-only (status 332).
    MembersOnly,
};

}  // namespace Muc

}  // namespace QXmpp

#endif  // QXMPPMUCDATA_H

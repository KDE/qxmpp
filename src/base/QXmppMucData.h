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
    /// The room was destroyed by its owner (XEP-0045 §10.9).
    RoomDestroyed,
};

///
/// \brief Information about a destroyed MUC room (XEP-0045 §10.9).
///
/// When a room owner destroys a room, the server sends an unavailable presence
/// containing a \c &lt;destroy/&gt; element. This may include an alternate room
/// JID and a human-readable reason.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT Destroy
{
public:
    /// Returns the JID of an alternate room, or an empty string if none was provided.
    QString alternateRoom() const { return m_alternateRoom; }
    /// Sets the JID of an alternate room.
    void setAlternateRoom(const QString &alternateRoom) { m_alternateRoom = alternateRoom; }

    /// Returns the human-readable reason for the room destruction.
    QString reason() const { return m_reason; }
    /// Sets the human-readable reason for the room destruction.
    void setReason(const QString &reason) { m_reason = reason; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"destroy", QXmpp::Private::ns_muc_user };
    static std::optional<Destroy> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_alternateRoom;
    QString m_reason;
};

///
/// \brief A MUC room item carrying role and/or affiliation data (XEP-0045).
///
/// Used as a typed, modern replacement for QXmppMucItem in the V2 API.
/// Items are returned from QXmppMucRoomV2::requestAffiliationList() and
/// can represent any combination of JID, nickname, role, affiliation,
/// reason, and actor.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT Item
{
public:
    /// Returns the (bare) JID of the user, or an empty string if not set.
    QString jid() const { return m_jid; }
    /// Sets the (bare) JID of the user.
    void setJid(const QString &jid) { m_jid = jid; }

    /// Returns the nickname of the occupant, or an empty string if not set.
    QString nick() const { return m_nick; }
    /// Sets the nickname of the occupant.
    void setNick(const QString &nick) { m_nick = nick; }

    /// Returns the affiliation, or \c std::nullopt if unspecified.
    std::optional<Affiliation> affiliation() const { return m_affiliation; }
    /// Sets the affiliation.
    void setAffiliation(std::optional<Affiliation> affiliation) { m_affiliation = affiliation; }

    /// Returns the role, or \c std::nullopt if unspecified.
    std::optional<Role> role() const { return m_role; }
    /// Sets the role.
    void setRole(std::optional<Role> role) { m_role = role; }

    /// Returns the human-readable reason, or an empty string if not set.
    QString reason() const { return m_reason; }
    /// Sets the human-readable reason.
    void setReason(const QString &reason) { m_reason = reason; }

    /// Returns the JID of the actor who performed the action, or an empty string if not set.
    QString actor() const { return m_actor; }
    /// Sets the JID of the actor.
    void setActor(const QString &actor) { m_actor = actor; }

    /// \cond
    static std::optional<Item> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_jid;
    QString m_nick;
    std::optional<Affiliation> m_affiliation;
    std::optional<Role> m_role;
    QString m_reason;
    QString m_actor;
};

///
/// \brief Room avatar data (content type and raw bytes).
///
/// \since QXmpp 1.15
///
struct QXMPP_EXPORT Avatar {
    QString contentType;
    QByteArray data;
};

}  // namespace Muc

}  // namespace QXmpp

#endif  // QXMPPMUCDATA_H

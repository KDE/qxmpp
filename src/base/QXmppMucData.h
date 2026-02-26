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
    static constexpr std::tuple XmlTag = { u"item", QXmpp::Private::ns_muc_admin };
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
    QString contentType;  ///< MIME type of the avatar image (e.g. \c "image/png").
    QByteArray data;      ///< Raw avatar image bytes.
};

///
/// \brief A mediated MUC invitation as defined by \xep{0045, Multi-User Chat} §7.8.2.
///
/// When sending an invitation, set \e to to the invitee's JID.
/// When receiving a forwarded invitation from the room, \e from holds the inviter's JID.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT Invite
{
public:
    /// Returns the invitee's JID this invitation is addressed to (set when sending).
    QString to() const { return m_to; }
    /// Sets the invitee's JID.
    void setTo(const QString &jid) { m_to = jid; }

    /// Returns the inviter's JID (set by the room when forwarding to the invitee).
    QString from() const { return m_from; }
    /// Sets the from JID.
    void setFrom(const QString &jid) { m_from = jid; }

    /// Returns the optional human-readable reason.
    QString reason() const { return m_reason; }
    /// Sets the optional human-readable reason.
    void setReason(const QString &reason) { m_reason = reason; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"invite", QStringView {} };
    static std::optional<Invite> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_to;
    QString m_from;
    QString m_reason;
};

///
/// \brief A mediated MUC invitation decline as defined by \xep{0045, Multi-User Chat} §7.8.2.
///
/// When sending a decline, set \e to to the original inviter's JID.
/// When receiving a forwarded decline from the room, \e from holds the invitee's JID.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT Decline
{
public:
    /// Returns the JID of the inviter this decline is addressed to (set when sending).
    QString to() const { return m_to; }
    /// Sets the JID to send the decline to.
    void setTo(const QString &jid) { m_to = jid; }

    /// Returns the JID of the invitee who declined (set by the room when forwarding).
    QString from() const { return m_from; }
    /// Sets the from JID.
    void setFrom(const QString &jid) { m_from = jid; }

    /// Returns the optional human-readable reason.
    QString reason() const { return m_reason; }
    /// Sets the optional human-readable reason.
    void setReason(const QString &reason) { m_reason = reason; }

    /// \cond
    static constexpr std::tuple XmlTag = { u"decline", QStringView {} };
    static std::optional<Decline> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QString m_to;
    QString m_from;
    QString m_reason;
};

///
/// \brief Represents the \c x element with namespace \c muc\#user
/// as defined by \xep{0045, Multi-User Chat}.
///
/// Used in messages to carry mediated invitations, invitation declines, and status codes.
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT UserQuery
{
public:
    /// Returns the MUC status codes carried in this element.
    QList<uint32_t> statusCodes() const { return m_statusCodes; }
    /// Sets the MUC status codes.
    void setStatusCodes(QList<uint32_t> codes) { m_statusCodes = std::move(codes); }

    /// Returns the mediated invitation, if present.
    std::optional<Invite> invite() const { return m_invite; }
    /// Sets the mediated invitation.
    void setInvite(std::optional<Invite> invite) { m_invite = std::move(invite); }

    /// Returns the room password for password-protected rooms (empty if none).
    QString password() const { return m_password; }
    /// Sets the room password.
    void setPassword(const QString &password) { m_password = password; }

    /// Returns the invitation decline, if present.
    std::optional<Decline> decline() const { return m_decline; }
    /// Sets the invitation decline.
    void setDecline(std::optional<Decline> decline) { m_decline = std::move(decline); }

    /// \cond
    static constexpr std::tuple XmlTag = { u"x", QXmpp::Private::ns_muc_user };
    static std::optional<UserQuery> fromDom(const QDomElement &el);
    void toXml(QXmlStreamWriter *writer) const;
    /// \endcond

private:
    QList<uint32_t> m_statusCodes;
    std::optional<Invite> m_invite;
    QString m_password;
    std::optional<Decline> m_decline;
};

}  // namespace Muc

}  // namespace QXmpp

#endif  // QXMPPMUCDATA_H

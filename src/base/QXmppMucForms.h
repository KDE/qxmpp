// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCFORMS_H
#define QXMPPMUCFORMS_H

#include "QXmppDataFormBase.h"

struct QXmppMucRoomInfoPrivate;

class QXMPP_EXPORT QXmppMucRoomInfo : public QXmppExtensibleDataFormBase
{
public:
    /// FORM_TYPE of this data form
    static constexpr QStringView DataFormType = QXmpp::Private::ns_muc_roominfo;
    static std::optional<QXmppMucRoomInfo> fromDataForm(const QXmppDataForm &);

    QXmppMucRoomInfo();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucRoomInfo)

    std::optional<bool> subjectChangeable() const;
    void setSubjectChangeable(std::optional<bool> newSubjectChangeable);

    QString subject() const;
    void setSubject(const QString &newSubject);

    std::optional<quint32> occupants() const;
    void setOccupants(std::optional<quint32> newOccupants);

    QString language() const;
    void setLanguage(const QString &newLanguage);

    QString description() const;
    void setDescription(const QString &newDescription);

    QStringList contactJids() const;
    void setContactJids(const QStringList &newContactJids);

    std::optional<quint32> maxHistoryFetch() const;
    void setMaxHistoryFetch(std::optional<quint32> newMaxHistoryFetch);

    QStringList avatarHashes() const;
    void setAvatarHashes(const QStringList &hashes);

    bool operator==(const QXmppMucRoomInfo &other) const;

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucRoomInfoPrivate> d;
};

struct QXmppMucVoiceRequestPrivate;

class QXMPP_EXPORT QXmppMucVoiceRequest : public QXmppExtensibleDataFormBase
{
public:
    /// FORM_TYPE of this data form
    static constexpr QStringView DataFormType = QXmpp::Private::ns_muc_request;
    static std::optional<QXmppMucVoiceRequest> fromDataForm(const QXmppDataForm &);

    QXmppMucVoiceRequest();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucVoiceRequest)

    QString jid() const;
    void setJid(const QString &jid);

    QString nick() const;
    void setNick(const QString &nick);

    std::optional<bool> requestAllow() const;
    void setRequestAllow(std::optional<bool> allow);

    QXmppDataForm toDataForm() const override;

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucVoiceRequestPrivate> d;
};

Q_DECLARE_METATYPE(QXmppMucVoiceRequest)

struct QXmppMucRoomConfigPrivate;

///
/// \brief The QXmppMucRoomConfig class represents the \c muc#roomconfig data form.
///
/// Returned by QXmppMucRoomV2::requestRoomConfig() and submitted via
/// QXmppMucRoomV2::submitRoomConfig().
///
/// \since QXmpp 1.15
///
class QXMPP_EXPORT QXmppMucRoomConfig : public QXmppExtensibleDataFormBase
{
public:
    /// Controls who may send private messages inside the room.
    enum class AllowPrivateMessages {
        Anyone,        ///< Anyone in the room may send private messages.
        Participants,  ///< Only participants (with voice) may send private messages.
        Moderators,    ///< Only moderators may send private messages.
        Nobody,        ///< Private messages are disabled.
    };

    /// Controls which occupants can discover the real JIDs of other occupants.
    enum class WhoCanDiscoverJids {
        Moderators,  ///< Only moderators can discover real JIDs (semi-anonymous room).
        Anyone,      ///< All occupants can see real JIDs (non-anonymous room).
    };

    /// FORM_TYPE of this data form.
    static constexpr QStringView DataFormType = QXmpp::Private::ns_muc_roomconfig;
    static std::optional<QXmppMucRoomConfig> fromDataForm(const QXmppDataForm &);

    QXmppMucRoomConfig();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucRoomConfig)

    QString name() const;
    void setName(const QString &name);

    QString description() const;
    void setDescription(const QString &description);

    QString language() const;
    void setLanguage(const QString &language);

    std::optional<bool> isPublic() const;
    void setPublic(std::optional<bool> value);

    std::optional<bool> isPersistent() const;
    void setPersistent(std::optional<bool> value);

    std::optional<bool> isMembersOnly() const;
    void setMembersOnly(std::optional<bool> value);

    std::optional<bool> isModerated() const;
    void setModerated(std::optional<bool> value);

    std::optional<bool> isPasswordProtected() const;
    void setPasswordProtected(std::optional<bool> value);

    QString password() const;
    void setPassword(const QString &password);

    std::optional<WhoCanDiscoverJids> whoCanDiscoverJids() const;
    void setWhoCanDiscoverJids(std::optional<WhoCanDiscoverJids> value);

    std::optional<bool> canOccupantsChangeSubject() const;
    void setCanOccupantsChangeSubject(std::optional<bool> value);

    std::optional<bool> canMembersInvite() const;
    void setCanMembersInvite(std::optional<bool> value);

    std::optional<AllowPrivateMessages> allowPrivateMessages() const;
    void setAllowPrivateMessages(std::optional<AllowPrivateMessages> value);

    std::optional<bool> enableLogging() const;
    void setEnableLogging(std::optional<bool> value);

    std::optional<int> maxUsers() const;
    void setMaxUsers(std::optional<int> value);

    QStringList owners() const;
    void setOwners(const QStringList &owners);

    QStringList admins() const;
    void setAdmins(const QStringList &admins);

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucRoomConfigPrivate> d;
};

#endif  // QXMPPMUCFORMS_H

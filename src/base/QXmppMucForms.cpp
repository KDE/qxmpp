// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucForms.h"

#include "QXmppMucForms_p.h"

#include "StringLiterals.h"

#include <QDebug>

using namespace QXmpp;
using namespace QXmpp::Private;

///
/// \class QXmppMucRoomInfo
///
/// `muc#roominfo` form as defined in \xep{0045, Multi-User Chat}.
///
/// \since QXmpp 1.13
///

struct QXmppMucRoomInfoPrivate : QSharedData {
    std::optional<quint32> maxHistoryFetch;
    QStringList contactJids;
    QString description;
    QString language;
    std::optional<quint32> occupants;
    QString subject;
    std::optional<bool> subjectChangeable;
    QStringList avatarHashes;
};

/// Tries to parse form into QXmppMucRoomInfo.
std::optional<QXmppMucRoomInfo> QXmppMucRoomInfo::fromDataForm(const QXmppDataForm &form)
{
    if (auto parsed = QXmppMucRoomInfo();
        QXmppDataFormBase::fromDataForm(form, parsed)) {
        return parsed;
    }
    return std::nullopt;
}

QXmppMucRoomInfo::QXmppMucRoomInfo()
    : d(new QXmppMucRoomInfoPrivate {})
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMucRoomInfo)

QString QXmppMucRoomInfo::formType() const
{
    return ns_muc_roominfo.toString();
}

bool QXmppMucRoomInfo::parseField(const QXmppDataForm::Field &field)
{
    // ignore hidden fields
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == u"muc#maxhistoryfetch") {
        d->maxHistoryFetch = parseUInt(value);
    } else if (key == u"muc#roominfo_contactjid") {
        d->contactJids = value.toStringList();
    } else if (key == u"muc#roominfo_description") {
        d->description = value.toString();
    } else if (key == u"muc#roominfo_lang") {
        d->language = value.toString();
    } else if (key == u"muc#roominfo_occupants") {
        d->occupants = parseUInt(value);
    } else if (key == u"muc#roominfo_subject") {
        d->subject = value.toString();
    } else if (key == u"muc#roominfo_subjectmod") {
        d->subjectChangeable = parseBool(value);
    } else if (key == u"muc#roominfo_avatarhash") {
        d->avatarHashes = value.toStringList();
    } else {
        return false;
    }
    return true;
}

void QXmppMucRoomInfo::serializeForm(QXmppDataForm &f) const
{
    using Type = QXmppDataForm::Field::Type;
    serializeOptionalNumber(f, Type::TextSingleField, u"muc#maxhistoryfetch", d->maxHistoryFetch);
    serializeEmptyable(f, Type::JidMultiField, u"muc#roominfo_contactjid", d->contactJids);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roominfo_description", d->description);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roominfo_lang", d->language);
    serializeOptionalNumber(f, Type::TextSingleField, u"muc#roominfo_occupants", d->occupants);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roominfo_subject", d->subject);
    serializeOptional(f, Type::BooleanField, u"muc#roominfo_subjectmod", d->subjectChangeable);
    serializeEmptyable(f, Type::TextMultiField, u"muc#roominfo_avatarhash", d->avatarHashes);
}

/// Returns Maximum Number of History Messages Returned by Room
std::optional<quint32> QXmppMucRoomInfo::maxHistoryFetch() const
{
    return d->maxHistoryFetch;
}

/// Sets Maximum Number of History Messages Returned by Room
void QXmppMucRoomInfo::setMaxHistoryFetch(std::optional<quint32> newMaxHistoryFetch)
{
    d->maxHistoryFetch = newMaxHistoryFetch;
}

/// Returns Contact Addresses (normally, room owner or owners)
QStringList QXmppMucRoomInfo::contactJids() const
{
    return d->contactJids;
}

/// Sets Contact Addresses (normally, room owner or owners)
void QXmppMucRoomInfo::setContactJids(const QStringList &newContactJids)
{
    d->contactJids = newContactJids;
}

/// Returns Short Description of Room
QString QXmppMucRoomInfo::description() const
{
    return d->description;
}

/// Sets Short Description of Room
void QXmppMucRoomInfo::setDescription(const QString &newDescription)
{
    d->description = newDescription;
}

/// Returns Natural Language for Room Discussions
QString QXmppMucRoomInfo::language() const
{
    return d->language;
}

/// Sets Natural Language for Room Discussions
void QXmppMucRoomInfo::setLanguage(const QString &newLanguage)
{
    d->language = newLanguage;
}

/// Returns Current Number of Occupants in Room
std::optional<quint32> QXmppMucRoomInfo::occupants() const
{
    return d->occupants;
}

/// Sets Current Number of Occupants in Room
void QXmppMucRoomInfo::setOccupants(std::optional<quint32> newOccupants)
{
    d->occupants = newOccupants;
}

/// Returns Current Discussion Topic
QString QXmppMucRoomInfo::subject() const
{
    return d->subject;
}

/// Sets Current Discussion Topic
void QXmppMucRoomInfo::setSubject(const QString &newSubject)
{
    d->subject = newSubject;
}

/// Returns whether the room subject can be modified by participants
std::optional<bool> QXmppMucRoomInfo::subjectChangeable() const
{
    return d->subjectChangeable;
}

/// Sets whether the room subject can be modified by participants
void QXmppMucRoomInfo::setSubjectChangeable(std::optional<bool> newSubjectChangeable)
{
    d->subjectChangeable = newSubjectChangeable;
}

/// Returns hashes of the vCard-temp avatar of this room.
QStringList QXmppMucRoomInfo::avatarHashes() const
{
    return d->avatarHashes;
}

/// Sets hashes of the vCard-temp avatar of this room.
void QXmppMucRoomInfo::setAvatarHashes(const QStringList &hashes)
{
    d->avatarHashes = hashes;
}

/// Equality operator
bool QXmppMucRoomInfo::operator==(const QXmppMucRoomInfo &other) const
{
    return d->maxHistoryFetch == other.d->maxHistoryFetch &&
        d->contactJids == other.d->contactJids &&
        d->description == other.d->description &&
        d->language == other.d->language &&
        d->occupants == other.d->occupants &&
        d->subject == other.d->subject &&
        d->subjectChangeable == other.d->subjectChangeable &&
        d->avatarHashes == other.d->avatarHashes;
}

///
/// \class QXmppMucVoiceRequest
///
/// \brief Represents a voice request data form (muc#request) for moderated rooms.
///
/// Used for both requesting and approving/denying voice in a moderated MUC room,
/// as defined in \xep{0045, Multi-User Chat}, §7.13 and §8.6.
///
/// \since QXmpp 1.15
///

struct QXmppMucVoiceRequestPrivate : QSharedData {
    QString jid;
    QString nick;
    std::optional<bool> requestAllow;
};

/// Tries to parse \a form into a QXmppMucVoiceRequest.
std::optional<QXmppMucVoiceRequest> QXmppMucVoiceRequest::fromDataForm(const QXmppDataForm &form)
{
    if (auto parsed = QXmppMucVoiceRequest();
        QXmppDataFormBase::fromDataForm(form, parsed)) {
        return parsed;
    }
    return std::nullopt;
}

QXmppMucVoiceRequest::QXmppMucVoiceRequest()
    : d(new QXmppMucVoiceRequestPrivate {})
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMucVoiceRequest)

QString QXmppMucVoiceRequest::formType() const
{
    return ns_muc_request.toString();
}

bool QXmppMucVoiceRequest::parseField(const QXmppDataForm::Field &field)
{
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == u"muc#jid") {
        d->jid = value.toString();
    } else if (key == u"muc#roomnick") {
        d->nick = value.toString();
    } else if (key == u"muc#request_allow") {
        d->requestAllow = parseBool(value);
    } else {
        return false;
    }
    return true;
}

void QXmppMucVoiceRequest::serializeForm(QXmppDataForm &f) const
{
    using Type = QXmppDataForm::Field::Type;
    // muc#role is always "participant" for voice requests
    serializeEmptyable(f, Type::ListSingleField, u"muc#role", u"participant"_s);
    serializeEmptyable(f, Type::JidSingleField, u"muc#jid", d->jid);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roomnick", d->nick);
    serializeOptional(f, Type::BooleanField, u"muc#request_allow", d->requestAllow);
}

QXmppDataForm QXmppMucVoiceRequest::toDataForm() const
{
    auto form = QXmppDataFormBase::toDataForm();
    form.setType(QXmppDataForm::Submit);
    return form;
}

///
/// Returns the full JID of the user requesting voice.
///
/// This is set by the room when forwarding the request to moderators.
///
QString QXmppMucVoiceRequest::jid() const
{
    return d->jid;
}

///
/// Sets the full JID of the user requesting voice.
///
void QXmppMucVoiceRequest::setJid(const QString &jid)
{
    d->jid = jid;
}

///
/// Returns the room nickname of the user requesting voice.
///
/// This is set by the room when forwarding the request to moderators.
///
QString QXmppMucVoiceRequest::nick() const
{
    return d->nick;
}

///
/// Sets the room nickname of the user requesting voice.
///
void QXmppMucVoiceRequest::setNick(const QString &nick)
{
    d->nick = nick;
}

///
/// Returns whether voice is granted or denied.
///
/// This is \c nullopt in incoming requests (before the moderator has responded).
/// When the moderator calls QXmppMucRoomV2::answerVoiceRequest(), this is set to
/// \c true (approve) or \c false (deny).
///
std::optional<bool> QXmppMucVoiceRequest::requestAllow() const
{
    return d->requestAllow;
}

///
/// Sets whether the voice request is approved or denied.
///
/// Pass \c true to approve or \c false to deny. This is set internally by
/// QXmppMucRoomV2::answerVoiceRequest() before sending the response form.
///
void QXmppMucVoiceRequest::setRequestAllow(std::optional<bool> allow)
{
    d->requestAllow = allow;
}

///
/// \class QXmppMucRoomConfig
///
/// \brief Represents the \c muc#roomconfig data form for configuring a MUC room.
///
/// Used by room owners to set room properties during creation (reserved-room flow,
/// see QXmppMucManagerV2::createRoom()) and during subsequent reconfiguration
/// (see QXmppMucRoomV2::requestRoomConfig() / QXmppMucRoomV2::submitRoomConfig()).
///
/// \since QXmpp 1.15
///

struct QXmppMucRoomConfigPrivate : QSharedData {
    QString name;
    QString description;
    QString language;
    std::optional<bool> isPublic;
    std::optional<bool> isPersistent;
    std::optional<bool> isMembersOnly;
    std::optional<bool> isModerated;
    std::optional<bool> isPasswordProtected;
    QString password;
    std::optional<QXmppMucRoomConfig::WhoCanDiscoverJids> whoCanDiscoverJids;
    std::optional<bool> canOccupantsChangeSubject;
    std::optional<bool> canMembersInvite;
    std::optional<QXmppMucRoomConfig::AllowPrivateMessages> allowPrivateMessages;
    std::optional<bool> enableLogging;
    std::optional<int> maxUsers;
    QStringList owners;
    QStringList admins;
};

/// Tries to parse \a form into a QXmppMucRoomConfig.
std::optional<QXmppMucRoomConfig> QXmppMucRoomConfig::fromDataForm(const QXmppDataForm &form)
{
    if (auto parsed = QXmppMucRoomConfig();
        QXmppDataFormBase::fromDataForm(form, parsed)) {
        return parsed;
    }
    return std::nullopt;
}

QXmppMucRoomConfig::QXmppMucRoomConfig()
    : d(new QXmppMucRoomConfigPrivate {})
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMucRoomConfig)

QString QXmppMucRoomConfig::formType() const
{
    return ns_muc_roomconfig.toString();
}

bool QXmppMucRoomConfig::parseField(const QXmppDataForm::Field &field)
{
    using Type = QXmppDataForm::Field::Type;
    if (field.type() == Type::HiddenField) {
        return false;
    }

    const auto key = field.key();
    const auto value = field.value();

    if (key == u"muc#roomconfig_roomname") {
        d->name = value.toString();
    } else if (key == u"muc#roomconfig_roomdesc") {
        d->description = value.toString();
    } else if (key == u"muc#roomconfig_lang") {
        d->language = value.toString();
    } else if (key == u"muc#roomconfig_publicroom") {
        d->isPublic = parseBool(value);
    } else if (key == u"muc#roomconfig_persistentroom") {
        d->isPersistent = parseBool(value);
    } else if (key == u"muc#roomconfig_membersonly") {
        d->isMembersOnly = parseBool(value);
    } else if (key == u"muc#roomconfig_moderatedroom") {
        d->isModerated = parseBool(value);
    } else if (key == u"muc#roomconfig_passwordprotectedroom") {
        d->isPasswordProtected = parseBool(value);
    } else if (key == u"muc#roomconfig_roomsecret") {
        d->password = value.toString();
    } else if (key == u"muc#roomconfig_whois") {
        d->whoCanDiscoverJids = Enums::fromString<WhoCanDiscoverJids>(value.toString());
    } else if (key == u"muc#roomconfig_changesubject") {
        d->canOccupantsChangeSubject = parseBool(value);
    } else if (key == u"muc#roomconfig_allowinvites") {
        d->canMembersInvite = parseBool(value);
    } else if (key == u"muc#roomconfig_allowpm") {
        d->allowPrivateMessages = Enums::fromString<AllowPrivateMessages>(value.toString());
    } else if (key == u"muc#roomconfig_enablelogging") {
        d->enableLogging = parseBool(value);
    } else if (key == u"muc#roomconfig_maxusers") {
        const auto str = value.toString();
        if (str == u"none" || str.isEmpty()) {
            d->maxUsers = 0;
        } else {
            bool ok = false;
            const int n = str.toInt(&ok);
            if (ok) {
                d->maxUsers = n;
            }
        }
    } else if (key == u"muc#roomconfig_roomowners") {
        d->owners = value.toStringList();
    } else if (key == u"muc#roomconfig_roomadmins") {
        d->admins = value.toStringList();
    } else {
        return false;
    }
    return true;
}

void QXmppMucRoomConfig::serializeForm(QXmppDataForm &f) const
{
    using Type = QXmppDataForm::Field::Type;
    serializeEmptyable(f, Type::TextSingleField, u"muc#roomconfig_roomname", d->name);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roomconfig_roomdesc", d->description);
    serializeEmptyable(f, Type::TextSingleField, u"muc#roomconfig_lang", d->language);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_publicroom", d->isPublic);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_persistentroom", d->isPersistent);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_membersonly", d->isMembersOnly);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_moderatedroom", d->isModerated);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_passwordprotectedroom", d->isPasswordProtected);
    serializeEmptyable(f, Type::TextPrivateField, u"muc#roomconfig_roomsecret", d->password);
    if (d->whoCanDiscoverJids.has_value()) {
        const QString val = (*d->whoCanDiscoverJids == WhoCanDiscoverJids::Anyone) ? u"anyone"_s : u"moderators"_s;
        serializeValue(f, Type::ListSingleField, u"muc#roomconfig_whois"_s, val);
    }
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_changesubject", d->canOccupantsChangeSubject);
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_allowinvites", d->canMembersInvite);
    if (d->allowPrivateMessages.has_value()) {
        QString val;
        switch (*d->allowPrivateMessages) {
        case AllowPrivateMessages::Anyone:
            val = u"anyone"_s;
            break;
        case AllowPrivateMessages::Participants:
            val = u"participants"_s;
            break;
        case AllowPrivateMessages::Moderators:
            val = u"moderators"_s;
            break;
        case AllowPrivateMessages::Nobody:
            val = u"none"_s;
            break;
        }
        serializeValue(f, Type::ListSingleField, u"muc#roomconfig_allowpm"_s, val);
    }
    serializeOptional(f, Type::BooleanField, u"muc#roomconfig_enablelogging", d->enableLogging);
    if (d->maxUsers.has_value()) {
        const QString val = (*d->maxUsers == 0) ? u"none"_s : QString::number(*d->maxUsers);
        serializeValue(f, Type::ListSingleField, u"muc#roomconfig_maxusers"_s, val);
    }
    serializeEmptyable(f, Type::JidMultiField, u"muc#roomconfig_roomowners", d->owners);
    serializeEmptyable(f, Type::JidMultiField, u"muc#roomconfig_roomadmins", d->admins);
}

///
/// Returns the natural-language name of the room.
///
/// Corresponds to \c muc#roomconfig_roomname.
///
///
QString QXmppMucRoomConfig::name() const
{
    return d->name;
}

///
/// Sets the natural-language name of the room.
///
///
void QXmppMucRoomConfig::setName(const QString &name)
{
    d->name = name;
}

///
/// Returns the short description of the room.
///
/// Corresponds to \c muc#roomconfig_roomdesc.
///
///
QString QXmppMucRoomConfig::description() const
{
    return d->description;
}

///
/// Sets the short description of the room.
///
///
void QXmppMucRoomConfig::setDescription(const QString &description)
{
    d->description = description;
}

///
/// Returns the natural language for room discussions (BCP 47 language tag).
///
/// Corresponds to \c muc#roomconfig_lang.
///
///
QString QXmppMucRoomConfig::language() const
{
    return d->language;
}

///
/// Sets the natural language for room discussions (BCP 47 language tag).
///
///
void QXmppMucRoomConfig::setLanguage(const QString &language)
{
    d->language = language;
}

///
/// Returns whether the room is publicly searchable via service discovery.
///
/// Corresponds to \c muc#roomconfig_publicroom. Returns \c nullopt if the
/// server did not include this field in the configuration form.
///
///
std::optional<bool> QXmppMucRoomConfig::isPublic() const
{
    return d->isPublic;
}

///
/// Sets whether the room is publicly searchable via service discovery.
///
///
void QXmppMucRoomConfig::setPublic(std::optional<bool> value)
{
    d->isPublic = value;
}

///
/// Returns whether the room persists after the last occupant exits.
///
/// Corresponds to \c muc#roomconfig_persistentroom. Returns \c nullopt if not
/// included in the form.
///
///
std::optional<bool> QXmppMucRoomConfig::isPersistent() const
{
    return d->isPersistent;
}

///
/// Sets whether the room persists after the last occupant exits.
///
///
void QXmppMucRoomConfig::setPersistent(std::optional<bool> value)
{
    d->isPersistent = value;
}

///
/// Returns whether only members are allowed to enter the room.
///
/// Corresponds to \c muc#roomconfig_membersonly.
///
///
std::optional<bool> QXmppMucRoomConfig::isMembersOnly() const
{
    return d->isMembersOnly;
}

///
/// Sets whether only members are allowed to enter the room.
///
///
void QXmppMucRoomConfig::setMembersOnly(std::optional<bool> value)
{
    d->isMembersOnly = value;
}

///
/// Returns whether only participants with voice may send messages to all occupants.
///
/// Corresponds to \c muc#roomconfig_moderatedroom.
///
///
std::optional<bool> QXmppMucRoomConfig::isModerated() const
{
    return d->isModerated;
}

///
/// Sets whether the room is moderated (only voice holders may send messages).
///
///
void QXmppMucRoomConfig::setModerated(std::optional<bool> value)
{
    d->isModerated = value;
}

///
/// Returns whether a password is required to enter the room.
///
/// Corresponds to \c muc#roomconfig_passwordprotectedroom.
///
///
std::optional<bool> QXmppMucRoomConfig::isPasswordProtected() const
{
    return d->isPasswordProtected;
}

///
/// Sets whether a password is required to enter the room.
///
/// If set to \c true, also set password() to a non-empty string.
///
///
void QXmppMucRoomConfig::setPasswordProtected(std::optional<bool> value)
{
    d->isPasswordProtected = value;
}

///
/// Returns the room password.
///
/// Only relevant when isPasswordProtected() is \c true. Corresponds to
/// \c muc#roomconfig_roomsecret.
///
///
QString QXmppMucRoomConfig::password() const
{
    return d->password;
}

///
/// Sets the room entry password.
///
///
void QXmppMucRoomConfig::setPassword(const QString &password)
{
    d->password = password;
}

///
/// Returns which occupants may discover the real JIDs of other occupants.
///
/// Corresponds to \c muc#roomconfig_whois. \c WhoCanDiscoverJids::Moderators
/// means the room is semi-anonymous; \c WhoCanDiscoverJids::Anyone means
/// non-anonymous.
///
///
std::optional<QXmppMucRoomConfig::WhoCanDiscoverJids> QXmppMucRoomConfig::whoCanDiscoverJids() const
{
    return d->whoCanDiscoverJids;
}

///
/// Sets which occupants may discover the real JIDs of other occupants.
///
///
void QXmppMucRoomConfig::setWhoCanDiscoverJids(std::optional<WhoCanDiscoverJids> value)
{
    d->whoCanDiscoverJids = value;
}

///
/// Returns whether regular occupants are allowed to change the room subject.
///
/// Corresponds to \c muc#roomconfig_changesubject.
///
///
std::optional<bool> QXmppMucRoomConfig::canOccupantsChangeSubject() const
{
    return d->canOccupantsChangeSubject;
}

///
/// Sets whether regular occupants are allowed to change the room subject.
///
///
void QXmppMucRoomConfig::setCanOccupantsChangeSubject(std::optional<bool> value)
{
    d->canOccupantsChangeSubject = value;
}

///
/// Returns whether members are allowed to invite others to the room.
///
/// Corresponds to \c muc#roomconfig_allowinvites.
///
///
std::optional<bool> QXmppMucRoomConfig::canMembersInvite() const
{
    return d->canMembersInvite;
}

///
/// Sets whether members are allowed to invite others to the room.
///
///
void QXmppMucRoomConfig::setCanMembersInvite(std::optional<bool> value)
{
    d->canMembersInvite = value;
}

///
/// Returns who is allowed to send private messages inside the room.
///
/// Corresponds to \c muc#roomconfig_allowpm.
///
///
std::optional<QXmppMucRoomConfig::AllowPrivateMessages> QXmppMucRoomConfig::allowPrivateMessages() const
{
    return d->allowPrivateMessages;
}

///
/// Sets who is allowed to send private messages inside the room.
///
///
void QXmppMucRoomConfig::setAllowPrivateMessages(std::optional<AllowPrivateMessages> value)
{
    d->allowPrivateMessages = value;
}

///
/// Returns whether public logging of the room is enabled.
///
/// Corresponds to \c muc#roomconfig_enablelogging.
///
///
std::optional<bool> QXmppMucRoomConfig::enableLogging() const
{
    return d->enableLogging;
}

///
/// Sets whether public logging of the room is enabled.
///
///
void QXmppMucRoomConfig::setEnableLogging(std::optional<bool> value)
{
    d->enableLogging = value;
}

///
/// Returns the maximum number of occupants allowed in the room.
///
/// A value of 0 means unlimited. Corresponds to \c muc#roomconfig_maxusers.
/// Returns \c nullopt if not included in the form.
///
///
std::optional<int> QXmppMucRoomConfig::maxUsers() const
{
    return d->maxUsers;
}

///
/// Sets the maximum number of occupants allowed in the room.
///
/// Use 0 for unlimited. Corresponds to \c muc#roomconfig_maxusers.
///
///
void QXmppMucRoomConfig::setMaxUsers(std::optional<int> value)
{
    d->maxUsers = value;
}

///
/// Returns the list of room owner JIDs.
///
/// Corresponds to \c muc#roomconfig_roomowners.
///
///
QStringList QXmppMucRoomConfig::owners() const
{
    return d->owners;
}

///
/// Sets the list of room owner JIDs.
///
///
void QXmppMucRoomConfig::setOwners(const QStringList &owners)
{
    d->owners = owners;
}

///
/// Returns the list of room admin JIDs.
///
/// Corresponds to \c muc#roomconfig_roomadmins.
///
///
QStringList QXmppMucRoomConfig::admins() const
{
    return d->admins;
}

///
/// Sets the list of room admin JIDs.
///
///
void QXmppMucRoomConfig::setAdmins(const QStringList &admins)
{
    d->admins = admins;
}

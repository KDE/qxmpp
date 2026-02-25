// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucForms.h"

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

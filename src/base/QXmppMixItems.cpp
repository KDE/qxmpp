// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppDataFormBase.h"
#include "QXmppMixConfigItem.h"
#include "QXmppMixInfoItem.h"
#include "QXmppMixIq_p.h"
#include "QXmppMixParticipantItem.h"
#include "QXmppUtils_p.h"

#include "Enums.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDateTime>

using namespace QXmpp::Private;

constexpr QStringView NAME = u"Name";
constexpr QStringView DESCRIPTION = u"Description";
constexpr QStringView CONTACT_JIDS = u"Contact";

constexpr QStringView LAST_EDITOR_JID_KEY = u"Last Change Made By";
constexpr QStringView OWNER_JIDS_KEY = u"Owner";
constexpr QStringView ADMINISTRATOR_JIDS_KEY = u"Administrator";
constexpr QStringView CHANNEL_DELETION_KEY = u"End of Life";
constexpr QStringView NODES_KEY = u"Nodes Present";
constexpr QStringView MESSAGES_SUBSCRIBE_ROLE_KEY = u"Messages Node Subscription";
constexpr QStringView MESSAGES_RETRACT_ROLE_KEY = u"Administrator Message Retraction Rights";
constexpr QStringView PRESENCE_SUBSCRIBE_ROLE_KEY = u"Presence Node Subscription";
constexpr QStringView PARTICIPANTS_SUBSCRIBE_ROLE_KEY = u"Participants Node Subscription";
constexpr QStringView INFORMATION_SUBSCRIBE_ROLE_KEY = u"Information Node Subscription";
constexpr QStringView INFORMATION_UPDATE_ROLE_KEY = u"Information Node Update Rights";
constexpr QStringView ALLOWED_JIDS_SUBSCRIBE_ROLE_KEY = u"Allowed Node Subscription";
constexpr QStringView BANNED_JIDS_SUBSCRIBE_ROLE_KEY = u"Banned Node Subscription";
constexpr QStringView CONFIGURATION_READ_ROLE_KEY = u"Configuration Node Access";
constexpr QStringView AVATARS_UPDATE_ROLE_KEY = u"Avatar Nodes Update Rights";
constexpr QStringView NICKNAME_REQUIRED_KEY = u"Mandatory Nicks";
constexpr QStringView PRESENCE_REQUIRED_KEY = u"Participants Must Provide Presence";
constexpr QStringView ONLY_PARTICIPANTS_PERMITTED_TO_SUBMIT_PRESENCE_KEY = u"Open Presence";
constexpr QStringView OWN_MESSAGE_RETRACTION_PERMITTED_KEY = u"User Message Retraction";
constexpr QStringView INVITATIONS_PERMITTED_KEY = u"Participation Addition by Invitation from Participant";
constexpr QStringView PRIVATE_MESSAGES_PERMITTED_KEY = u"Private Messages";

static const QMap<QXmppMixConfigItem::Node, QStringView> NODES = {
    { QXmppMixConfigItem::Node::AllowedJids, u"allowed" },
    { QXmppMixConfigItem::Node::AvatarData, u"avatar" },
    { QXmppMixConfigItem::Node::AvatarMetadata, u"avatar" },
    { QXmppMixConfigItem::Node::BannedJids, u"banned" },
    { QXmppMixConfigItem::Node::Information, u"information" },
    { QXmppMixConfigItem::Node::JidMap, u"jidmap-visible" },
    { QXmppMixConfigItem::Node::Participants, u"participants" },
    { QXmppMixConfigItem::Node::Presence, u"presence" },
};

template<>
struct Enums::Data<QXmppMixConfigItem::Role> {
    using enum QXmppMixConfigItem::Role;
    static constexpr auto Values = makeValues<QXmppMixConfigItem::Role>({
        { Owner, u"owners" },
        { Administrator, u"admins" },
        { Participant, u"participants" },
        { Allowed, u"allowed" },
        { Anyone, u"anyone" },
        { Nobody, u"nobody" },
    });
};

class QXmppMixConfigItemPrivate : public QSharedData, public QXmppDataFormBase
{
public:
    QXmppDataForm::Type dataFormType = QXmppDataForm::None;
    QString lastEditorJid;
    QStringList ownerJids;
    QStringList administratorJids;
    QDateTime channelDeletion;
    QXmppMixConfigItem::Nodes nodes;
    std::optional<QXmppMixConfigItem::Role> messagesSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> messagesRetractRole;
    std::optional<QXmppMixConfigItem::Role> presenceSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> participantsSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> informationSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> informationUpdateRole;
    std::optional<QXmppMixConfigItem::Role> allowedJidsSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> bannedJidsSubscribeRole;
    std::optional<QXmppMixConfigItem::Role> configurationReadRole;
    std::optional<QXmppMixConfigItem::Role> avatarUpdateRole;
    std::optional<bool> nicknameRequired;
    std::optional<bool> presenceRequired;
    std::optional<bool> onlyParticipantsPermittedToSubmitPresence;
    std::optional<bool> ownMessageRetractionPermitted;
    std::optional<bool> invitationsPermitted;
    std::optional<bool> privateMessagesPermitted;

    void reset()
    {
        dataFormType = QXmppDataForm::None;
        lastEditorJid.clear();
        ownerJids.clear();
        administratorJids.clear();
        channelDeletion = {};
        nodes = {};
        messagesSubscribeRole = std::nullopt;
        messagesRetractRole = std::nullopt;
        presenceSubscribeRole = std::nullopt;
        participantsSubscribeRole = std::nullopt;
        informationSubscribeRole = std::nullopt;
        informationUpdateRole = std::nullopt;
        allowedJidsSubscribeRole = std::nullopt;
        bannedJidsSubscribeRole = std::nullopt;
        configurationReadRole = std::nullopt;
        avatarUpdateRole = std::nullopt;
        nicknameRequired = std::nullopt;
        presenceRequired = std::nullopt;
        onlyParticipantsPermittedToSubmitPresence = std::nullopt;
        ownMessageRetractionPermitted = std::nullopt;
        invitationsPermitted = std::nullopt;
        privateMessagesPermitted = std::nullopt;
    }

    QString formType() const override
    {
        return staticString(ns_mix_admin);
    }

    void parseForm(const QXmppDataForm &form) override
    {
        using Role = QXmppMixConfigItem::Role;
        dataFormType = form.type();
        const auto fields = form.fields();

        for (const auto &field : fields) {
            const auto key = field.key();
            const auto value = field.value();

            if (key == LAST_EDITOR_JID_KEY) {
                lastEditorJid = value.toString();
            } else if (key == OWNER_JIDS_KEY) {
                ownerJids = value.toStringList();
            } else if (key == ADMINISTRATOR_JIDS_KEY) {
                administratorJids = value.toStringList();
            } else if (key == CHANNEL_DELETION_KEY) {
                channelDeletion = value.toDateTime();
            } else if (key == NODES_KEY) {
                nodes = listToNodes(value.toStringList());
            } else if (key == MESSAGES_SUBSCRIBE_ROLE_KEY) {
                messagesSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == MESSAGES_RETRACT_ROLE_KEY) {
                messagesRetractRole = Enums::fromString<Role>(value.toString());
            } else if (key == PRESENCE_SUBSCRIBE_ROLE_KEY) {
                presenceSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == PARTICIPANTS_SUBSCRIBE_ROLE_KEY) {
                participantsSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == INFORMATION_SUBSCRIBE_ROLE_KEY) {
                informationSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == INFORMATION_UPDATE_ROLE_KEY) {
                informationUpdateRole = Enums::fromString<Role>(value.toString());
            } else if (key == ALLOWED_JIDS_SUBSCRIBE_ROLE_KEY) {
                allowedJidsSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == BANNED_JIDS_SUBSCRIBE_ROLE_KEY) {
                bannedJidsSubscribeRole = Enums::fromString<Role>(value.toString());
            } else if (key == CONFIGURATION_READ_ROLE_KEY) {
                configurationReadRole = Enums::fromString<Role>(value.toString());
            } else if (key == AVATARS_UPDATE_ROLE_KEY) {
                avatarUpdateRole = Enums::fromString<Role>(value.toString());
            } else if (key == NICKNAME_REQUIRED_KEY) {
                nicknameRequired = value.toBool();
            } else if (key == PRESENCE_REQUIRED_KEY) {
                presenceRequired = value.toBool();
            } else if (key == ONLY_PARTICIPANTS_PERMITTED_TO_SUBMIT_PRESENCE_KEY) {
                onlyParticipantsPermittedToSubmitPresence = value.toBool();
            } else if (key == OWN_MESSAGE_RETRACTION_PERMITTED_KEY) {
                ownMessageRetractionPermitted = value.toBool();
            } else if (key == INVITATIONS_PERMITTED_KEY) {
                invitationsPermitted = value.toBool();
            } else if (key == PRIVATE_MESSAGES_PERMITTED_KEY) {
                privateMessagesPermitted = value.toBool();
            }
        }
    }

    void serializeForm(QXmppDataForm &form) const override
    {
        form.setType(dataFormType);

        using Type = QXmppDataForm::Field::Type;

        serializeNullable(form, Type::JidSingleField, staticString(LAST_EDITOR_JID_KEY), lastEditorJid);
        serializeEmptyable(form, Type::JidMultiField, staticString(OWNER_JIDS_KEY), ownerJids);
        serializeEmptyable(form, Type::JidMultiField, staticString(ADMINISTRATOR_JIDS_KEY), administratorJids);
        serializeDatetime(form, staticString(CHANNEL_DELETION_KEY), channelDeletion);
        serializeEmptyable(form, Type::ListMultiField, staticString(NODES_KEY), nodesToList(nodes));
        serializeRole(form, staticString(MESSAGES_SUBSCRIBE_ROLE_KEY), messagesSubscribeRole);
        serializeRole(form, staticString(MESSAGES_RETRACT_ROLE_KEY), messagesRetractRole);
        serializeRole(form, staticString(PRESENCE_SUBSCRIBE_ROLE_KEY), presenceSubscribeRole);
        serializeRole(form, staticString(PARTICIPANTS_SUBSCRIBE_ROLE_KEY), participantsSubscribeRole);
        serializeRole(form, staticString(INFORMATION_SUBSCRIBE_ROLE_KEY), informationSubscribeRole);
        serializeRole(form, staticString(INFORMATION_UPDATE_ROLE_KEY), informationUpdateRole);
        serializeRole(form, staticString(ALLOWED_JIDS_SUBSCRIBE_ROLE_KEY), allowedJidsSubscribeRole);
        serializeRole(form, staticString(BANNED_JIDS_SUBSCRIBE_ROLE_KEY), bannedJidsSubscribeRole);
        serializeRole(form, staticString(CONFIGURATION_READ_ROLE_KEY), configurationReadRole);
        serializeRole(form, staticString(AVATARS_UPDATE_ROLE_KEY), avatarUpdateRole);
        serializeOptional(form, Type::ListSingleField, staticString(NICKNAME_REQUIRED_KEY), nicknameRequired);
        serializeOptional(form, Type::ListSingleField, staticString(PRESENCE_REQUIRED_KEY), presenceRequired);
        serializeOptional(form, Type::ListSingleField, staticString(ONLY_PARTICIPANTS_PERMITTED_TO_SUBMIT_PRESENCE_KEY), onlyParticipantsPermittedToSubmitPresence);
        serializeOptional(form, Type::ListSingleField, staticString(OWN_MESSAGE_RETRACTION_PERMITTED_KEY), ownMessageRetractionPermitted);
        serializeOptional(form, Type::ListSingleField, staticString(INVITATIONS_PERMITTED_KEY), invitationsPermitted);
        serializeOptional(form, Type::ListSingleField, staticString(PRIVATE_MESSAGES_PERMITTED_KEY), privateMessagesPermitted);
    }

    // Serializes a role to a form field with the given name in the data form.
    static void serializeRole(QXmppDataForm &form, const QString &name, std::optional<QXmppMixConfigItem::Role> role)
    {
        if (role) {
            serializeValue(form, QXmppDataForm::Field::Type::ListSingleField, name, Enums::toString(*role));
        }
    }

    // Converts a nodes flag to a list of nodes.
    static QStringList nodesToList(QXmppMixConfigItem::Nodes nodes)
    {
        QStringList nodeList;

        for (auto itr = NODES.cbegin(); itr != NODES.cend(); ++itr) {
            if (nodes.testFlag(itr.key())) {
                nodeList.append(itr.value().toString());
            }
        }

        return nodeList;
    }

    // Converts a list of nodes to a nodes flag.
    static QXmppMixConfigItem::Nodes listToNodes(const QStringList &nodeList)
    {
        QXmppMixConfigItem::Nodes nodes;

        for (auto itr = NODES.cbegin(); itr != NODES.cend(); ++itr) {
            if (nodeList.contains(itr.value())) {
                nodes |= itr.key();
            }
        }

        return nodes;
    }
};

/*!
    \class QXmppMixConfigItem
    \inmodule QXmpp

    \brief The QXmppMixConfigItem class represents a PubSub item of a MIX channel containing its
    configuration as defined by \xep{0369}{Mediated Information eXchange (MIX)}.

    \since QXmpp 1.7

    \ingroup Stanzas
*/

/*!
    \enum QXmppMixConfigItem::Node

    PubSub node belonging to a MIX channel.

    \value AllowedJids JIDs allowed to participate in the channel. If this
    node does not exist, all JIDs are allowed to participate.
    \value AvatarData Channel's avatar data.
    \value AvatarMetadata Channel's avatar metadata.
    \value BannedJids JIDs banned from participating in the channel.
    \value Configuration Channel's configuration.
    \value Information Channel's information.
    \value JidMap Mappings from the participants' IDs to their JIDs (needed
    for JID-hidden channels).
    \value Messages Messages sent through the channel.
    \value Participants Users participating in the channel.
    \value Presence Presence of users participating in the channel.
*/

/*!
    \enum QXmppMixConfigItem::Role

    Roles for a MIX channel with various rights.

    The rights are defined in a strictly hierarchical manner following the order of this
    enumeration, so that for example Owners will always have rights that Administrators have.

    \value Owner Allowed to update the channel configuration. Specified by
    the channel configuration.
    \value Administrator Allowed to update the JIDs that are allowed to
    participate or banned from participating in a channel. Specified in the
    channel configuration.
    \value Participant Participant of the channel.
    \value Allowed User that is allowed to participate in the channel.
    Users are allowed if their JIDs do not match a JID in the
    \c Node::BannedJids node and either there is no \c Node::AllowedJids
    node or their JIDs match a JID in it.
    \value Anyone Any user, including users in the BannedJids.
    \value Nobody No user, including owners and administrators.
*/

QXmppMixConfigItem::QXmppMixConfigItem()
    : d(new QXmppMixConfigItemPrivate)
{
}

/*! Default copy-constructor */
QXmppMixConfigItem::QXmppMixConfigItem(const QXmppMixConfigItem &) = default;
/*! Default move-constructor */
QXmppMixConfigItem::QXmppMixConfigItem(QXmppMixConfigItem &&) = default;
/*! Default assignment operator */
QXmppMixConfigItem &QXmppMixConfigItem::operator=(const QXmppMixConfigItem &) = default;
/*! Default move-assignment operator */
QXmppMixConfigItem &QXmppMixConfigItem::operator=(QXmppMixConfigItem &&) = default;
QXmppMixConfigItem::~QXmppMixConfigItem() = default;

/*!
    Returns the type of the data form that contains the channel's configuration.
*/
QXmppDataForm::Type QXmppMixConfigItem::formType() const
{
    return d->dataFormType;
}

/*!
    Sets the data form's \a formType that contains the channel's configuration.
*/
void QXmppMixConfigItem::setFormType(QXmppDataForm::Type formType)
{
    d->dataFormType = formType;
}

/*!
    Returns the bare JID of the user that made the latest change to the channel's configuration.

    The JID is set by the server on each configuration change.
*/
QString QXmppMixConfigItem::lastEditorJid() const
{
    return d->lastEditorJid;
}

/*!
    Sets the bare \a lastEditorJid of the user that made the latest change to
    the channel's configuration.

    \sa lastEditorJid()
*/
void QXmppMixConfigItem::setLastEditorJid(const QString &lastEditorJid)
{
    d->lastEditorJid = lastEditorJid;
}

/*!
    Returns the bare JIDs of the channel owners.

    When a channel is created, the JID of the user that created it is set as the first owner.

    \sa Role::Owner
*/
QStringList QXmppMixConfigItem::ownerJids() const
{
    return d->ownerJids;
}

/*!
    Sets the bare \a ownerJids of the channel owners.

    \sa ownerJids()
*/
void QXmppMixConfigItem::setOwnerJids(const QStringList &ownerJids)
{
    d->ownerJids = ownerJids;
}

/*!
    Returns the bare JIDs of the channel administrators.

    \sa Role::Administrator
*/
QStringList QXmppMixConfigItem::administratorJids() const
{
    return d->administratorJids;
}

/*!
    Sets the bare \a administratorJids of the channel administrators.

    \sa administratorJids()
*/
void QXmppMixConfigItem::setAdministratorJids(const QStringList &administratorJids)
{
    d->administratorJids = administratorJids;
}

/*!
    Returns the date and time when the channel is automatically deleted.

    If no date/time is set, the channel is permanent.
*/
QDateTime QXmppMixConfigItem::channelDeletion() const
{
    return d->channelDeletion;
}

/*!
    Sets \a channelDeletion, the date and time when the channel is
    automatically deleted.

    \sa channelDeletion()
*/
void QXmppMixConfigItem::setChannelDeletion(const QDateTime &channelDeletion)
{
    d->channelDeletion = channelDeletion;
}

/*!
    Returns which nodes are present for the channel.
*/
QXmppMixConfigItem::Nodes QXmppMixConfigItem::nodes() const
{
    return d->nodes;
}

/*!
    Sets which \a nodes are present for the channel.
*/
void QXmppMixConfigItem::setNodes(Nodes nodes)
{
    d->nodes = nodes;
}

/*!
    Returns the role that is permitted to subscribe to messages sent through the channel.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::messagesSubscribeRole() const
{
    return d->messagesSubscribeRole;
}

/*!
    Sets the \a messagesSubscribeRole that is permitted to subscribe to
    messages sent through the channel.

    Only the following roles are valid:
    \list
        \li Role::Participant
        \li Role::Allowed
        \li Role::Anyone
    \endlist
*/
void QXmppMixConfigItem::setMessagesSubscribeRole(std::optional<Role> messagesSubscribeRole)
{
    d->messagesSubscribeRole = messagesSubscribeRole;
}

/*!
    Returns the role that is permitted to retract any message sent through the channel.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::messagesRetractRole() const
{
    return d->messagesRetractRole;
}

/*!
    Sets the \a messagesRetractRole that is permitted to retract any message
    sent through the channel.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Nobody
    \endlist
*/
void QXmppMixConfigItem::setMessagesRetractRole(std::optional<Role> messagesRetractRole)
{
    d->messagesRetractRole = messagesRetractRole;
}

/*!
    Returns the role that is permitted to subscribe to the channel's user' presence.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::presenceSubscribeRole() const
{
    return d->presenceSubscribeRole;
}

/*!
    Sets the \a presenceSubscribeRole that is permitted to subscribe to the
    channel's users' presence.

    Only the following roles are valid:
    \list
        \li Role::Participant
        \li Role::Allowed
        \li Role::Anyone
    \endlist
*/
void QXmppMixConfigItem::setPresenceSubscribeRole(std::optional<Role> presenceSubscribeRole)
{
    d->presenceSubscribeRole = presenceSubscribeRole;
}

/*!
    Returns the role that is permitted to subscribe to the channel's participants.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::participantsSubscribeRole() const
{
    return d->participantsSubscribeRole;
}

/*!
    Sets the \a participantsSubscribeRole that is permitted to subscribe to the
    channel's participants.
*/
void QXmppMixConfigItem::setParticipantsSubscribeRole(std::optional<Role> participantsSubscribeRole)
{
    d->participantsSubscribeRole = participantsSubscribeRole;
}

/*!
    Returns the role that is permitted to subscribe to the channel's information.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::informationSubscribeRole() const
{
    return d->informationSubscribeRole;
}

/*!
    Sets the \a informationSubscribeRole that is permitted to subscribe to the
    channel's information.

    Only the following roles are valid:
    \list
        \li Role::Participant
        \li Role::Allowed
        \li Role::Anyone
    \endlist
*/
void QXmppMixConfigItem::setInformationSubscribeRole(std::optional<Role> informationSubscribeRole)
{
    d->informationSubscribeRole = informationSubscribeRole;
}

/*!
    Returns the role that is permitted to update the channel's information.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::informationUpdateRole() const
{
    return d->informationUpdateRole;
}

/*!
    Sets the \a informationUpdateRole that is permitted to update the
    channel's information.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Participant
    \endlist
*/
void QXmppMixConfigItem::setInformationUpdateRole(std::optional<Role> informationUpdateRole)
{
    d->informationUpdateRole = informationUpdateRole;
}

/*!
    Returns the role that is permitted to subscribe to the JIDs that are allowed to participate in
    the channel.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::allowedJidsSubscribeRole() const
{
    return d->allowedJidsSubscribeRole;
}

/*!
    Sets the \a allowedJidsSubscribeRole that is permitted to subscribe to the
    JIDs that are allowed to participate in the channel.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Participant
        \li Role::Allowed
        \li Role::Nobody
    \endlist
*/
void QXmppMixConfigItem::setAllowedJidsSubscribeRole(std::optional<Role> allowedJidsSubscribeRole)
{
    d->allowedJidsSubscribeRole = allowedJidsSubscribeRole;
}

/*!
    Returns the role that is permitted to subscribe to the JIDs that are banned from participating
    in the channel.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::bannedJidsSubscribeRole() const
{
    return d->bannedJidsSubscribeRole;
}

/*!
    Sets the \a bannedJidsSubscribeRole that is permitted to subscribe to the
    JIDs that are banned from participating in the channel.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Participant
        \li Role::Allowed
        \li Role::Nobody
    \endlist
*/
void QXmppMixConfigItem::setBannedJidsSubscribeRole(std::optional<Role> bannedJidsSubscribeRole)
{
    d->bannedJidsSubscribeRole = bannedJidsSubscribeRole;
}

/*!
    Returns the role that is permitted to subscribe to and read the channel's configuration.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::configurationReadRole() const
{
    return d->configurationReadRole;
}

/*!
    Sets the \a configurationReadRole that is permitted to subscribe to and
    read the channel's configuration.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Participant
        \li Role::Allowed
        \li Role::Nobody
    \endlist
*/
void QXmppMixConfigItem::setConfigurationReadRole(std::optional<Role> configurationReadRole)
{
    d->configurationReadRole = configurationReadRole;
}

/*!
    Returns the role that is permitted to update the channel's avatar.
*/
std::optional<QXmppMixConfigItem::Role> QXmppMixConfigItem::avatarUpdateRole() const
{
    return d->avatarUpdateRole;
}

/*!
    Sets the \a avatarUpdateRole that is permitted to update the channel's
    avatar.

    Only the following roles are valid:
    \list
        \li Role::Owner
        \li Role::Administrator
        \li Role::Participant
    \endlist
*/
void QXmppMixConfigItem::setAvatarUpdateRole(std::optional<Role> avatarUpdateRole)
{
    d->avatarUpdateRole = avatarUpdateRole;
}

/*!
    Returns whether participants need nicknames.
*/
std::optional<bool> QXmppMixConfigItem::nicknameRequired() const
{
    return d->nicknameRequired;
}

/*!
    Sets via \a nicknameRequired whether participants need nicknames.
*/
void QXmppMixConfigItem::setNicknameRequired(std::optional<bool> nicknameRequired)
{
    d->nicknameRequired = nicknameRequired;
}

/*!
    Returns whether participants need to share their presence.
*/
std::optional<bool> QXmppMixConfigItem::presenceRequired() const
{
    return d->presenceRequired;
}

/*!
    Sets via \a presenceRequired whether participants need to share their
    presence.
*/
void QXmppMixConfigItem::setPresenceRequired(std::optional<bool> presenceRequired)
{
    d->presenceRequired = presenceRequired;
}

/*!
    Returns whether only participants are permitted to share their presence.
*/
std::optional<bool> QXmppMixConfigItem::onlyParticipantsPermittedToSubmitPresence() const
{
    return d->onlyParticipantsPermittedToSubmitPresence;
}

/*!
    Sets via \a onlyParticipantsPermittedToSubmitPresence whether only
    participants are permitted to share their presence.
*/
void QXmppMixConfigItem::setOnlyParticipantsPermittedToSubmitPresence(std::optional<bool> onlyParticipantsPermittedToSubmitPresence)
{
    d->onlyParticipantsPermittedToSubmitPresence = onlyParticipantsPermittedToSubmitPresence;
}

/*!
    Returns whether users are permitted to retract their own messages sent through the channel.
*/
std::optional<bool> QXmppMixConfigItem::ownMessageRetractionPermitted() const
{
    return d->ownMessageRetractionPermitted;
}

/*!
    Sets via \a ownMessageRetractionPermitted whether users are permitted to
    retract their own messages sent through the channel.
*/
void QXmppMixConfigItem::setOwnMessageRetractionPermitted(std::optional<bool> ownMessageRetractionPermitted)
{
    d->ownMessageRetractionPermitted = ownMessageRetractionPermitted;
}

/*!
    Returns whether channel participants are permitted to invite users to the
    channel.

    In order to use that feature, the participant must request the invitation from the channel and
    send it to the invitee.
    The invitee can use the invitation to join the channel.

    \sa QXmppMixInvitation
*/
std::optional<bool> QXmppMixConfigItem::invitationsPermitted() const
{
    return d->invitationsPermitted;
}

/*!
    Sets via \a invitationsPermitted whether participants are permitted to
    invite users to the channel.

    \sa invitationsPermitted()
*/
void QXmppMixConfigItem::setInvitationsPermitted(std::optional<bool> invitationsPermitted)
{
    d->invitationsPermitted = invitationsPermitted;
}

/*!
    Returns whether participants are permitted to exchange private messages through the channel.
*/
std::optional<bool> QXmppMixConfigItem::privateMessagesPermitted() const
{
    return d->privateMessagesPermitted;
}

/*!
    Sets via \a privateMessagesPermitted whether participants are permitted to
    exchange private messages through the channel.
*/
void QXmppMixConfigItem::setPrivateMessagesPermitted(std::optional<bool> privateMessagesPermitted)
{
    d->privateMessagesPermitted = privateMessagesPermitted;
}

/*! Returns true if the given DOM \a element is a MIX channel config item. */
bool QXmppMixConfigItem::isItem(const QDomElement &element)
{
    return QXmppPubSubBaseItem::isItem(element, [](const QDomElement &payload) {
        // Check FORM_TYPE without parsing a full QXmppDataForm.
        if (payload.tagName() != u'x' || payload.namespaceURI() != ns_data) {
            return false;
        }
        for (auto fieldEl = payload.firstChildElement();
             !fieldEl.isNull();
             fieldEl = fieldEl.nextSiblingElement()) {
            if (fieldEl.attribute(u"var"_s) == u"FORM_TYPE") {
                return fieldEl.firstChildElement(u"value"_s).text() == ns_mix_admin;
            }
        }
        return false;
    });
}

void QXmppMixConfigItem::parsePayload(const QDomElement &payload)
{
    d->reset();

    QXmppDataForm form;
    form.parse(payload);

    d->parseForm(form);
}

void QXmppMixConfigItem::serializePayload(QXmlStreamWriter *writer) const
{
    d->toDataForm().toXml(writer);
}

class QXmppMixInfoItemPrivate : public QSharedData, public QXmppDataFormBase
{
public:
    QXmppDataForm::Type dataFormType = QXmppDataForm::None;
    QString name;
    QString description;
    QStringList contactJids;

    void reset()
    {
        dataFormType = QXmppDataForm::None;
        name.clear();
        description.clear();
        contactJids.clear();
    }

    QString formType() const override
    {
        return staticString(ns_mix);
    }

    void parseForm(const QXmppDataForm &form) override
    {
        dataFormType = form.type();
        const auto fields = form.fields();

        for (const auto &field : fields) {
            const auto key = field.key();
            const auto value = field.value();

            if (key == NAME) {
                name = value.toString();
            } else if (key == DESCRIPTION) {
                description = value.toString();
            } else if (key == CONTACT_JIDS) {
                contactJids = value.toStringList();
            }
        }
    }

    void serializeForm(QXmppDataForm &form) const override
    {
        form.setType(dataFormType);

        using Type = QXmppDataForm::Field::Type;
        serializeNullable(form, Type::TextSingleField, staticString(NAME), name);
        serializeNullable(form, Type::TextSingleField, staticString(DESCRIPTION), description);
        serializeEmptyable(form, Type::JidMultiField, staticString(CONTACT_JIDS), contactJids);
    }
};

/*!
    \class QXmppMixInfoItem
    \inmodule QXmpp

    \brief The QXmppMixInfoItem class represents a PubSub item of a MIX
    channel containing channel information as defined by \xep{0369}{Mediated
    Information eXchange (MIX)}.

    \since QXmpp 1.5

    \ingroup Stanzas
*/

QXmppMixInfoItem::QXmppMixInfoItem()
    : d(new QXmppMixInfoItemPrivate)
{
}

/*! Default copy-constructor */
QXmppMixInfoItem::QXmppMixInfoItem(const QXmppMixInfoItem &) = default;
/*! Default move-constructor */
QXmppMixInfoItem::QXmppMixInfoItem(QXmppMixInfoItem &&) = default;
/*! Default assignment operator */
QXmppMixInfoItem &QXmppMixInfoItem::operator=(const QXmppMixInfoItem &) = default;
/*! Default move-assignment operator */
QXmppMixInfoItem &QXmppMixInfoItem::operator=(QXmppMixInfoItem &&) = default;
QXmppMixInfoItem::~QXmppMixInfoItem() = default;

/*!
    Returns the type of the data form that contains the channel information.
*/
QXmppDataForm::Type QXmppMixInfoItem::formType() const
{
    return d->dataFormType;
}

/*!
    Sets the data form's \a formType that contains the channel information.
*/
void QXmppMixInfoItem::setFormType(QXmppDataForm::Type formType)
{
    d->dataFormType = formType;
}

/*!
    Returns the user-specified name of the MIX channel. This is not the name
    part of the channel's JID.
*/
const QString &QXmppMixInfoItem::name() const
{
    return d->name;
}

/*! Sets the \a name of the channel. */
void QXmppMixInfoItem::setName(QString name)
{
    d->name = std::move(name);
}

/*! Returns the description of the channel. This string might be very long. */
const QString &QXmppMixInfoItem::description() const
{
    return d->description;
}

/*! Sets the longer channel \a description. */
void QXmppMixInfoItem::setDescription(QString description)
{
    d->description = std::move(description);
}

/*! Returns a list of JIDs that are responsible for this channel. */
const QStringList &QXmppMixInfoItem::contactJids() const
{
    return d->contactJids;
}

/*!
    Sets a list of public JIDs that are responsible for this channel.

    \a contactJids.
*/
void QXmppMixInfoItem::setContactJids(QStringList contactJids)
{
    d->contactJids = std::move(contactJids);
}

/*! Returns true, if the given dom \a element is a MIX channel info item. */
bool QXmppMixInfoItem::isItem(const QDomElement &element)
{
    return QXmppPubSubBaseItem::isItem(element, [](const QDomElement &payload) {
        // check FORM_TYPE without parsing a full QXmppDataForm
        if (payload.tagName() != u'x' || payload.namespaceURI() != ns_data) {
            return false;
        }
        for (const auto &fieldEl : iterChildElements(payload)) {
            if (fieldEl.attribute(u"var"_s) == u"FORM_TYPE") {
                return fieldEl.firstChildElement(u"value"_s).text() == ns_mix;
            }
        }
        return false;
    });
}

void QXmppMixInfoItem::parsePayload(const QDomElement &payload)
{
    d->reset();

    QXmppDataForm form;
    form.parse(payload);

    d->parseForm(form);
}

void QXmppMixInfoItem::serializePayload(QXmlStreamWriter *writer) const
{
    d->toDataForm().toXml(writer);
}

class QXmppMixParticipantItemPrivate : public QSharedData
{
public:
    QString nick;
    QString jid;
};

/*!
    \class QXmppMixParticipantItem
    \inmodule QXmpp

    The QXmppMixParticipantItem class represents a PubSub item of a MIX channel
    participant as defined by \xep{0369}{Mediated Information eXchange (MIX)}.

    \since QXmpp 1.5

    \ingroup Stanzas
*/

QXmppMixParticipantItem::QXmppMixParticipantItem()
    : d(new QXmppMixParticipantItemPrivate)
{
}

/*! Default copy-constructor */
QXmppMixParticipantItem::QXmppMixParticipantItem(const QXmppMixParticipantItem &) = default;
/*! Default move-constructor */
QXmppMixParticipantItem::QXmppMixParticipantItem(QXmppMixParticipantItem &&) = default;
/*! Default assignment operator */
QXmppMixParticipantItem &QXmppMixParticipantItem::operator=(const QXmppMixParticipantItem &) = default;
/*! Default move-assignment operator */
QXmppMixParticipantItem &QXmppMixParticipantItem::operator=(QXmppMixParticipantItem &&) = default;
QXmppMixParticipantItem::~QXmppMixParticipantItem() = default;

/*! Returns the participant's nickname. */
const QString &QXmppMixParticipantItem::nick() const
{
    return d->nick;
}

/*!
    Sets the participants nickname.

    \a nick.
*/
void QXmppMixParticipantItem::setNick(QString nick)
{
    d->nick = std::move(nick);
}

/*! Returns the participant's JID. */
const QString &QXmppMixParticipantItem::jid() const
{
    return d->jid;
}

/*!
    Sets the participant's JID.

    \a jid.
*/
void QXmppMixParticipantItem::setJid(QString jid)
{
    d->jid = std::move(jid);
}

void QXmppMixParticipantItem::parsePayload(const QDomElement &payload)
{
    d->nick = payload.firstChildElement(u"nick"_s).text();
    d->jid = payload.firstChildElement(u"jid"_s).text();
}

void QXmppMixParticipantItem::serializePayload(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        { u"participant", ns_mix },
        OptionalTextElement { u"jid", d->jid },
        OptionalTextElement { u"nick", d->nick },
    });
}

/*! Returns true, if this dom \a element is a MIX participant item. */
bool QXmppMixParticipantItem::isItem(const QDomElement &element)
{
    return QXmppPubSubBaseItem::isItem(element, [](const QDomElement &payload) {
        return payload.tagName() == u"participant" &&
            payload.namespaceURI() == ns_mix;
    });
}

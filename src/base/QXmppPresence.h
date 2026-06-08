// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPPRESENCE_H
#define QXMPPPRESENCE_H

#include "QXmppJingleIq.h"
#include "QXmppMucData.h"
#include "QXmppMucIq.h"
#include "QXmppStanza.h"

class QXmppPresencePrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppPresence class represents an XMPP presence stanza.

    \ingroup Stanzas
*/
class QXMPP_EXPORT QXmppPresence : public QXmppStanza
{
public:
    /*!
        This enum is used to describe a presence type.

        \value Error An error has occurred regarding processing or delivery of a previously-sent presence stanza.
        \value Available Signals that the sender is online and available for communication.
        \value Unavailable Signals that the sender is no longer available for communication.
        \value Subscribe The sender wishes to subscribe to the recipient's  presence.
        \value Subscribed The sender has allowed the recipient to receive their presence.
        \value Unsubscribe The sender is unsubscribing from another entity's presence.
        \value Unsubscribed The subscription request has been denied or a previously-granted subscription has been cancelled.
        \value Probe A request for an entity's current presence; SHOULD be generated only by a server on behalf of a user.
    */
    enum Type {
        Error = 0,
        Available,
        Unavailable,
        Subscribe,
        Subscribed,
        Unsubscribe,
        Unsubscribed,
        Probe
    };

    /*!
        This enum is used to describe an availability status.

        \value Online The entity or resource is online.
        \value Away The entity or resource is temporarily away.
        \value XA The entity or resource is away for an extended period.
        \value DND The entity or resource is busy ("Do Not Disturb").
        \value Chat The entity or resource is actively interested in chatting.
        \value Invisible obsolete \xep{0018}{Invisible Presence}.
    */
    enum AvailableStatusType {
        Online = 0,
        Away,
        XA,
        DND,
        Chat,
        Invisible
    };

    /*!
        This enum is used to describe vCard updates as defined by
        \xep{0153}{vCard-Based Avatars}

        \value VCardUpdateNone Protocol is not supported.
        \value VCardUpdateNoPhoto User is not using any image.
        \value VCardUpdateValidPhoto User is advertising an image.
        \value VCardUpdateNotReady User is not ready to advertise an image.
    */
    enum VCardUpdateType {
        VCardUpdateNone = 0,
        VCardUpdateNoPhoto,
        VCardUpdateValidPhoto,
        VCardUpdateNotReady
    };

    QXmppPresence(QXmppPresence::Type type = QXmppPresence::Available);
    QXmppPresence(const QXmppPresence &other);
    QXmppPresence(QXmppPresence &&);
    ~QXmppPresence() override;

    QXmppPresence &operator=(const QXmppPresence &other);
    QXmppPresence &operator=(QXmppPresence &&);

    bool isXmppStanza() const override;

    AvailableStatusType availableStatusType() const;
    void setAvailableStatusType(AvailableStatusType type);

    int priority() const;
    void setPriority(int priority);

    QXmppPresence::Type type() const;
    void setType(QXmppPresence::Type);

    QString statusText() const;
    void setStatusText(const QString &statusText);

    // XEP-0045: Multi-User Chat
    [[deprecated("Use mucParticipantItem()")]]
    QXmppMucItem mucItem() const;
    [[deprecated("Use setMucParticipantItem()")]]
    void setMucItem(const QXmppMucItem &item);
    QXmpp::Muc::Item mucParticipantItem() const;
    void setMucParticipantItem(const QXmpp::Muc::Item &item);

    QString mucPassword() const;
    void setMucPassword(const QString &password);

    QList<int> mucStatusCodes() const;
    void setMucStatusCodes(const QList<int> &codes);

    bool isMucSupported() const;
    void setMucSupported(bool supported);

    std::optional<QXmpp::Muc::HistoryOptions> mucHistory() const;
    void setMucHistory(std::optional<QXmpp::Muc::HistoryOptions>);

    std::optional<QXmpp::Muc::Destroy> mucDestroy() const;
    void setMucDestroy(std::optional<QXmpp::Muc::Destroy>);

    // XEP-0153: vCard-Based Avatars
    QByteArray photoHash() const;
    void setPhotoHash(const QByteArray &);

    VCardUpdateType vCardUpdateType() const;
    void setVCardUpdateType(VCardUpdateType type);

    // XEP-0115: Entity Capabilities
    QString capabilityHash() const;
    void setCapabilityHash(const QString &);

    QString capabilityNode() const;
    void setCapabilityNode(const QString &);

    QByteArray capabilityVer() const;
    void setCapabilityVer(const QByteArray &);

    // XEP-0272: Multiparty Jingle (Muji)
    bool isPreparingMujiSession() const;
    void setIsPreparingMujiSession(bool isPreparingMujiSession);

    QVector<QXmppJingleIq::Content> mujiContents() const;
    void setMujiContents(const QVector<QXmppJingleIq::Content> &mujiContents);

    // XEP-0283: Moved
    QString oldJid() const;
    void setOldJid(const QString &oldJid);

    // XEP-0319: Last User Interaction in Presence
    QDateTime lastUserInteraction() const;
    void setLastUserInteraction(const QDateTime &);

    // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    QString mixUserJid() const;
    void setMixUserJid(const QString &);

    QString mixUserNick() const;
    void setMixUserNick(const QString &);

    // XEP-0421: Occupant identifiers for semi-anonymous MUCs
    QString mucOccupantId() const;
    void setMucOccupantId(const QString &);

    void parse(const QDomElement &element) override;
    void toXml(QXmlStreamWriter *writer) const override;

#if QXMPP_DEPRECATED_SINCE(1, 12)
    [[deprecated("Legacy entity capabilities (used 2003-2007)")]]
    QStringList capabilityExt() const;
#endif

private:
    void parseExtension(const QDomElement &element, QXmppElementList &unknownElements);

    QSharedDataPointer<QXmppPresencePrivate> d;
};

#endif  // QXMPPPRESENCE_H

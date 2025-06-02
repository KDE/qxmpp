// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppPresence.h"

#include "QXmppConstants_p.h"
#include "QXmppJingleIq.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDateTime>
#include <QDomElement>

using namespace QXmpp::Private;

template<>
struct Enums::Data<QXmppPresence::Type> {
    using enum QXmppPresence::Type;
    static constexpr auto NullValue = Available;
    static constexpr auto Values = makeValues<QXmppPresence::Type>({
        { Error, u"error" },
        { Available, {} },
        { Unavailable, u"unavailable" },
        { Subscribe, u"subscribe" },
        { Subscribed, u"subscribed" },
        { Unsubscribe, u"unsubscribe" },
        { Unsubscribed, u"unsubscribed" },
        { Probe, u"probe" },
    });
};

template<>
struct Enums::Data<QXmppPresence::AvailableStatusType> {
    using enum QXmppPresence::AvailableStatusType;
    static constexpr auto NullValue = Online;
    static constexpr auto Values = makeValues<QXmppPresence::AvailableStatusType>({
        { Online, {} },
        { Away, u"away" },
        { XA, u"xa" },
        { DND, u"dnd" },
        { Chat, u"chat" },
        { Invisible, u"invisible" },
    });
};

class QXmppPresencePrivate : public QSharedData
{
public:
    QXmppPresencePrivate();

    QXmppPresence::Type type;
    QXmppPresence::AvailableStatusType availableStatusType;
    QString statusText;
    int priority;

    // XEP-0045: Multi-User Chat
    QXmppMucItem mucItem;
    QString mucPassword;
    QList<int> mucStatusCodes;
    bool mucSupported;

    // XEP-0115: Entity Capabilities
    QString capabilityHash;
    QString capabilityNode;
    QByteArray capabilityVer;
    // Legacy XEP-0115: Entity Capabilities
    QStringList capabilityExt;

    // XEP-0153: vCard-Based Avatars
    // photoHash: the SHA1 hash of the avatar image data itself (not the base64-encoded version)
    // in accordance with RFC 3174
    QByteArray photoHash;
    QXmppPresence::VCardUpdateType vCardUpdateType;

    // XEP-0272: Multiparty Jingle (Muji)
    bool isPreparingMujiSession = false;
    QVector<QXmppJingleIq::Content> mujiContents;

    // XEP-0319: Last User Interaction in Presence
    QDateTime lastUserInteraction;

    // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    QString mixUserJid;
    QString mixUserNick;

    // XEP-0283: Moved
    QString oldJid;
};

QXmppPresencePrivate::QXmppPresencePrivate()
    : type(QXmppPresence::Available),
      availableStatusType(QXmppPresence::Online),
      priority(0),
      mucSupported(false),
      vCardUpdateType(QXmppPresence::VCardUpdateNone)
{
}

///
/// Constructs a QXmppPresence.
///
QXmppPresence::QXmppPresence(QXmppPresence::Type type)
    : d(new QXmppPresencePrivate)
{
    d->type = type;
}

/// Copy-constructor.
QXmppPresence::QXmppPresence(const QXmppPresence &other) = default;
/// Move-constructor.
QXmppPresence::QXmppPresence(QXmppPresence &&) = default;
/// Destroys a QXmppPresence.
QXmppPresence::~QXmppPresence() = default;
/// Assignemnt operator.
QXmppPresence &QXmppPresence::operator=(const QXmppPresence &other) = default;
/// Move-assignemnt operator.
QXmppPresence &QXmppPresence::operator=(QXmppPresence &&) = default;

///
/// Indicates if the QXmppStanza is a stanza in the XMPP sence (i. e. a message,
/// iq or presence)
///
/// \since QXmpp 1.0
///
bool QXmppPresence::isXmppStanza() const
{
    return true;
}

///
/// Returns the availability status type, for instance busy or away.
///
/// This will not tell you whether a contact is connected, check whether
/// type() is QXmppPresence::Available instead.
///
QXmppPresence::AvailableStatusType QXmppPresence::availableStatusType() const
{
    return d->availableStatusType;
}

/// Sets the availability status type, for instance busy or away.
void QXmppPresence::setAvailableStatusType(AvailableStatusType type)
{
    d->availableStatusType = type;
}

/// Returns the priority level of the resource.
int QXmppPresence::priority() const
{
    return d->priority;
}

/// Sets the \a priority level of the resource.
void QXmppPresence::setPriority(int priority)
{
    d->priority = priority;
}

/// Returns the status text, a textual description of the user's status.
QString QXmppPresence::statusText() const
{
    return d->statusText;
}

///
/// Sets the status text, a textual description of the user's status.
///
/// \param statusText The status text, for example "Gone fishing".
///
void QXmppPresence::setStatusText(const QString &statusText)
{
    d->statusText = statusText;
}

///
/// Returns the presence type.
///
/// You can use this method to determine the action which needs to be
/// taken in response to receiving the presence. For instance, if the type is
/// QXmppPresence::Available or QXmppPresence::Unavailable, you could update
/// the icon representing a contact's availability.
///
QXmppPresence::Type QXmppPresence::type() const
{
    return d->type;
}

/// Sets the presence type.
void QXmppPresence::setType(QXmppPresence::Type type)
{
    d->type = type;
}

/// Returns the photo-hash of the VCardUpdate.
QByteArray QXmppPresence::photoHash() const
{
    return d->photoHash;
}

///
/// Sets the photo-hash of the VCardUpdate.
///
/// \param photoHash as QByteArray
///
void QXmppPresence::setPhotoHash(const QByteArray &photoHash)
{
    d->photoHash = photoHash;
}

/// Returns the type of VCardUpdate
QXmppPresence::VCardUpdateType QXmppPresence::vCardUpdateType() const
{
    return d->vCardUpdateType;
}

/// Sets the type of VCardUpdate
void QXmppPresence::setVCardUpdateType(VCardUpdateType type)
{
    d->vCardUpdateType = type;
}

/// \xep{0115}: Entity Capabilities
QString QXmppPresence::capabilityHash() const
{
    return d->capabilityHash;
}

/// \xep{0115}: Entity Capabilities
void QXmppPresence::setCapabilityHash(const QString &hash)
{
    d->capabilityHash = hash;
}

/// \xep{0115}: Entity Capabilities
QString QXmppPresence::capabilityNode() const
{
    return d->capabilityNode;
}

/// \xep{0115}: Entity Capabilities
void QXmppPresence::setCapabilityNode(const QString &node)
{
    d->capabilityNode = node;
}

/// \xep{0115}: Entity Capabilities
QByteArray QXmppPresence::capabilityVer() const
{
    return d->capabilityVer;
}

/// \xep{0115}: Entity Capabilities
void QXmppPresence::setCapabilityVer(const QByteArray &ver)
{
    d->capabilityVer = ver;
}

/// Legacy \xep{0115}: Entity Capabilities
QStringList QXmppPresence::capabilityExt() const
{
    return d->capabilityExt;
}

///
/// Returns whether a \xep{0272, Multiparty Jingle (Muji)} session is being prepared.
///
/// \return whether a Muji session is being prepared
///
/// \since QXmpp 1.5
///
bool QXmppPresence::isPreparingMujiSession() const
{
    return d->isPreparingMujiSession;
}

///
/// Sets whether a \xep{0272, Multiparty Jingle (Muji)} session is being prepared.
///
/// \param isPreparingMujiSession whether a Muji session is being prepared
///
/// \since QXmpp 1.5
///
void QXmppPresence::setIsPreparingMujiSession(bool isPreparingMujiSession)
{
    d->isPreparingMujiSession = isPreparingMujiSession;
}

///
/// Returns \xep{0272, Multiparty Jingle (Muji)} contents.
///
/// \return Muji contents
///
/// \since QXmpp 1.5
///
QVector<QXmppJingleIq::Content> QXmppPresence::mujiContents() const
{
    return d->mujiContents;
}

///
/// Sets \xep{0272, Multiparty Jingle (Muji)} contents.
///
/// \param mujiContents Muji contents
///
/// \since QXmpp 1.5
///
void QXmppPresence::setMujiContents(const QVector<QXmppJingleIq::Content> &mujiContents)
{
    d->mujiContents = mujiContents;
}

/// Returns the MUC item.
QXmppMucItem QXmppPresence::mucItem() const
{
    return d->mucItem;
}

/// Sets the MUC item.
void QXmppPresence::setMucItem(const QXmppMucItem &item)
{
    d->mucItem = item;
}

/// Returns the password used to join a MUC room.
QString QXmppPresence::mucPassword() const
{
    return d->mucPassword;
}

/// Sets the password used to join a MUC room.
void QXmppPresence::setMucPassword(const QString &password)
{
    d->mucPassword = password;
}

/// Returns the MUC status codes.
QList<int> QXmppPresence::mucStatusCodes() const
{
    return d->mucStatusCodes;
}

/// Sets the MUC status codes.
void QXmppPresence::setMucStatusCodes(const QList<int> &codes)
{
    d->mucStatusCodes = codes;
}

/// Returns true if the sender has indicated MUC support.
bool QXmppPresence::isMucSupported() const
{
    return d->mucSupported;
}

/// Sets whether MUC is \a supported.
void QXmppPresence::setMucSupported(bool supported)
{
    d->mucSupported = supported;
}

///
/// Returns the \xep{0283, Moved} user's old jid.
///
/// \since QXmpp 1.9
///
QString QXmppPresence::oldJid() const
{
    return d->oldJid;
}

///
/// Sets the \xep{0283, Moved} user's old jid.
///
/// \since QXmpp 1.9
///
void QXmppPresence::setOldJid(const QString &oldJid)
{
    d->oldJid = oldJid;
}

///
/// Returns when the last user interaction with the client took place. See
/// \xep{0319}: Last User Interaction in Presence for details.
///
/// \since QXmpp 1.0
///
QDateTime QXmppPresence::lastUserInteraction() const
{
    return d->lastUserInteraction;
}

///
/// Sets the time of the last user interaction as defined in \xep{0319}: Last
/// User Interaction in Presence.
///
/// \since QXmpp 1.0
///
void QXmppPresence::setLastUserInteraction(const QDateTime &lastUserInteraction)
{
    d->lastUserInteraction = lastUserInteraction;
}

///
/// Returns the actual (full) JID of the MIX channel participant.
///
/// \since QXmpp 1.1
///
QString QXmppPresence::mixUserJid() const
{
    return d->mixUserJid;
}

///
/// Sets the actual (full) JID of the MIX channel participant.
///
/// \since QXmpp 1.1
///
void QXmppPresence::setMixUserJid(const QString &mixUserJid)
{
    d->mixUserJid = mixUserJid;
}

///
/// Returns the MIX participant's nickname.
///
/// \since QXmpp 1.1
///
QString QXmppPresence::mixUserNick() const
{
    return d->mixUserNick;
}

///
/// Sets the MIX participant's nickname.
///
/// \since QXmpp 1.1
///
void QXmppPresence::setMixUserNick(const QString &mixUserNick)
{
    d->mixUserNick = mixUserNick;
}

/// \cond
void QXmppPresence::parse(const QDomElement &element)
{
    QXmppStanza::parse(element);

    // attributes
    d->type = Enums::fromString<Type>(element.attribute(u"type"_s)).value_or(Available);

    QXmppElementList unknownElements;
    for (const auto &childElement : iterChildElements(element)) {
        if (childElement.tagName() == u"show") {
            d->availableStatusType = Enums::fromString<AvailableStatusType>(childElement.text()).value_or(Online);
        } else if (childElement.tagName() == u"status") {
            d->statusText = childElement.text();
        } else if (childElement.tagName() == u"priority") {
            d->priority = childElement.text().toInt();
            // parse presence extensions
            // XEP-0033: Extended Stanza Addressing and errors are parsed by QXmppStanza
        } else if (!(childElement.tagName() == u"addresses" && childElement.namespaceURI() == ns_extended_addressing) &&
                   childElement.tagName() != u"error") {
            parseExtension(childElement, unknownElements);
        }
    }

    setExtensions(unknownElements);
}

void QXmppPresence::parseExtension(const QDomElement &element, QXmppElementList &unknownElements)
{
    // XEP-0045: Multi-User Chat
    if (element.tagName() == u"x" && element.namespaceURI() == ns_muc) {
        d->mucSupported = true;
        d->mucPassword = element.firstChildElement(u"password"_s).text();
    } else if (element.tagName() == u"x" && element.namespaceURI() == ns_muc_user) {
        d->mucItem.parse(firstChildElement(element, u"item"));

        d->mucStatusCodes.clear();
        for (const auto &statusElement : iterChildElements(element, u"status")) {
            d->mucStatusCodes << statusElement.attribute(u"code"_s).toInt();
        }
        // XEP-0115: Entity Capabilities
    } else if (element.tagName() == u"c" && element.namespaceURI() == ns_capabilities) {
        d->capabilityNode = element.attribute(u"node"_s);
        d->capabilityVer = QByteArray::fromBase64(element.attribute(u"ver"_s).toLatin1());
        d->capabilityHash = element.attribute(u"hash"_s);
        d->capabilityExt = element.attribute(u"ext"_s).split(u' ', Qt::SkipEmptyParts);
        // XEP-0153: vCard-Based Avatars
    } else if (element.namespaceURI() == ns_vcard_update) {
        QDomElement photoElement = element.firstChildElement(u"photo"_s);
        if (photoElement.isNull()) {
            d->photoHash = {};
            d->vCardUpdateType = VCardUpdateNotReady;
        } else {
            d->photoHash = QByteArray::fromHex(photoElement.text().toLatin1());
            if (d->photoHash.isEmpty()) {
                d->vCardUpdateType = VCardUpdateNoPhoto;
            } else {
                d->vCardUpdateType = VCardUpdateValidPhoto;
            }
        }
        // XEP-0272: Multiparty Jingle (Muji)
    } else if (element.tagName() == u"muji" && element.namespaceURI() == ns_muji) {
        if (!element.firstChildElement(u"preparing"_s).isNull()) {
            d->isPreparingMujiSession = true;
        }

        // allow any namespace (not just ns_jingle): XEP has no XML schema and examples don't have
        // jingle namespace, but muji namespace
        d->mujiContents = transformFilter<QVector<QXmppJingleIq::Content>>(iterChildElements(element, u"content"), parseElement<QXmppJingleIq::Content>);
        // XEP-0283: Moved
    } else if (element.tagName() == u"moved" && element.namespaceURI() == ns_moved) {
        d->oldJid = element.firstChildElement(u"old-jid"_s).text();
        // XEP-0319: Last User Interaction in Presence
    } else if (element.tagName() == u"idle" && element.namespaceURI() == ns_idle) {
        if (element.hasAttribute(u"since"_s)) {
            const QString since = element.attribute(u"since"_s);
            d->lastUserInteraction = QXmppUtils::datetimeFromString(since);
        }
        // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    } else if (element.tagName() == u"mix" && element.namespaceURI() == ns_mix_presence) {
        d->mixUserJid = element.firstChildElement(u"jid"_s).text();
        d->mixUserNick = element.firstChildElement(u"nick"_s).text();
    } else {
        unknownElements << element;
    }
}

void QXmppPresence::toXml(QXmlStreamWriter *xmlWriter) const
{
    XmlWriter w(xmlWriter);
    w.write(Element {
        u"presence",
        OptionalAttribute { u"xml:lang", lang() },
        OptionalAttribute { u"id", id() },
        OptionalAttribute { u"to", to() },
        OptionalAttribute { u"from", from() },
        OptionalAttribute { u"type", d->type },
        OptionalTextElement { u"show", d->availableStatusType },
        OptionalTextElement { u"status", d->statusText },
        OptionalTextElement { u"priority", d->priority != 0 ? std::make_optional(d->priority) : std::nullopt },
        errorOptional(),
        // XEP-0045: Multi-User Chat
        OptionalContent {
            d->mucSupported,
            Element { { u"x", ns_muc }, OptionalTextElement { u"password", d->mucPassword } },
        },
        OptionalContent {
            !d->mucItem.isNull() || !d->mucStatusCodes.isEmpty(),
            Element {
                { u"x", ns_muc_user },
                d->mucItem,
                SingleAttributeElements { u"status", u"code", d->mucStatusCodes },
            },
        },
        // XEP-0115: Entity Capabilities
        OptionalContent {
            !d->capabilityNode.isEmpty() && !d->capabilityVer.isEmpty() && !d->capabilityHash.isEmpty(),
            Element {
                { u"c", ns_capabilities },
                Attribute { u"hash", d->capabilityHash },
                Attribute { u"node", d->capabilityNode },
                Attribute { u"ver", Base64 { d->capabilityVer } },
            },
        },
        // XEP-0153: vCard-Based Avatars
        OptionalContent {
            d->vCardUpdateType != VCardUpdateNone,
            Element {
                { u"x", ns_vcard_update },
                [&] {
                    if (d->vCardUpdateType == VCardUpdateNoPhoto) {
                        w.write(Element { u"photo" });
                    } else if (d->vCardUpdateType == VCardUpdateValidPhoto) {
                        w.write(TextElement { u"photo", d->photoHash.toHex() });
                    }
                },
            },
        },
        // XEP-0272: Multiparty Jingle (Muji)
        OptionalContent {
            d->isPreparingMujiSession || !d->mujiContents.isEmpty(),
            Element {
                { u"muji", ns_muji },
                OptionalContent { d->isPreparingMujiSession, Element { u"preparing" } },
                d->mujiContents,
            },
        },
        // XEP-0283: Moved
        OptionalContent {
            !d->oldJid.isEmpty(),
            Element { { u"moved", ns_moved }, TextElement { u"old-jid", d->oldJid } },
        },
        // XEP-0319: Last User Interaction in Presence
        OptionalContent {
            d->lastUserInteraction.isValid(),
            Element { { u"idle", ns_idle }, Attribute { u"since", d->lastUserInteraction } },
        },
        // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
        OptionalContent {
            !d->mixUserJid.isEmpty() || !d->mixUserNick.isEmpty(),
            Element {
                { u"mix", ns_mix_presence },
                OptionalTextElement { u"jid", d->mixUserJid },
                OptionalTextElement { u"nick", d->mixUserNick },
            },
        },
        // unknown extensions
        [&] { QXmppStanza::extensionsToXml(xmlWriter); },
    });
}
/// \endcond

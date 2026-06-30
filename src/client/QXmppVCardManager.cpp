// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppVCardManager.h"

#include "QXmppAccountMigrationManager.h"
#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppError.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"
#include "QXmppVCardIq.h"

#include "Async.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

using namespace QXmpp;
using namespace QXmpp::Private;

namespace QXmpp::Private {

struct VCardData {
    QXmppVCardIq vCard;

    static std::variant<VCardData, QXmppError> fromDom(const QDomElement &el)
    {
        Q_ASSERT(el.tagName() == u"vcard");
        Q_ASSERT(el.namespaceURI() == ns_qxmpp_export);

        auto vCardEl = firstChildElement(el, u"vCard", ns_vcard);
        if (vCardEl.isNull()) {
            return QXmppError { u"Missing required <vCard/> element."_s, {} };
        }

        VCardData d;
        // element needs to be the parent of <vCard/>
        d.vCard.parseElementFromChild(el);
        return d;
    }

    void toXml(XmlWriter &w) const
    {
        w.write(Element { u"vcard", [&] { vCard.toXmlElementFromChild(w); } });
    }

    // XEP-0227 stores the native <vCard xmlns='vcard-temp'/> element directly.
    static std::variant<VCardData, QXmppError> fromDomPie(const QDomElement &el)
    {
        Q_ASSERT(el.tagName() == u"vCard");
        Q_ASSERT(el.namespaceURI() == ns_vcard);

        // parseElementFromChild() expects the parent and looks up its <vCard/> child, so
        // pass the parent of the matched element.
        VCardData d;
        d.vCard.parseElementFromChild(el.parentNode().toElement());
        return d;
    }

    void toXmlPie(QXmlStreamWriter *writer) const
    {
        vCard.toXmlElementFromChild(writer);
    }
};

void serializeVCardData(const VCardData &data, QXmlStreamWriter &writer)
{
    XmlWriter(&writer).write(data);
}

void serializeVCardDataPie(const VCardData &data, QXmlStreamWriter &writer)
{
    data.toXmlPie(&writer);
}

}  // namespace QXmpp::Private

class QXmppVCardManagerPrivate
{
public:
    QXmppVCardIq clientVCard;
    bool isClientVCardReceived = false;
};

QXmppVCardManager::QXmppVCardManager()
    : d(std::make_unique<QXmppVCardManagerPrivate>())
{
    QXmppExportData::registerExtension<VCardData, VCardData::fromDom, serializeVCardData>(u"vcard", ns_qxmpp_export);
    QXmppExportData::registerExtension<VCardData, VCardData::fromDomPie, serializeVCardDataPie>(QXmppExportData::Format::Xep0227, u"vCard", ns_vcard);
}

QXmppVCardManager::~QXmppVCardManager() = default;

/*!
    Fetches the VCard of a bare JID.

    \since QXmpp 1.8

    \a bareJid.
*/
QXmppTask<QXmppVCardManager::VCardIqResult> QXmppVCardManager::fetchVCard(const QString &bareJid)
{
    co_return parseIq<QXmppVCardIq>(co_await client()->sendIq(QXmppVCardIq(bareJid)));
}

/*!
    Sets the VCard of the currently connected account.

    \since QXmpp 1.8

    \a vCard.
*/
QXmppTask<QXmppVCardManager::Result> QXmppVCardManager::setVCard(const QXmppVCardIq &vCard)
{
    auto vCardIq = vCard;
    vCardIq.setTo(client()->configuration().jidBare());
    vCardIq.setFrom({});
    vCardIq.setType(QXmppIq::Set);
    return client()->sendGenericIq(std::move(vCardIq));
}

/*!
    This function requests the server for the vCard of the specified \a jid (a JID of a
    specific entry in the roster). Once received the signal vCardReceived() is emitted.
*/
QString QXmppVCardManager::requestVCard(const QString &jid)
{
    return client()->sendLegacyId(QXmppVCardIq(jid));
}

/*! Returns the vCard of the connected client. */
const QXmppVCardIq &QXmppVCardManager::clientVCard() const
{
    return d->clientVCard;
}

/*!
    Sets the vCard of the connected client.

    \a clientVCard.
*/
void QXmppVCardManager::setClientVCard(const QXmppVCardIq &clientVCard)
{
    d->clientVCard = clientVCard;
    d->clientVCard.setTo({});
    d->clientVCard.setFrom({});
    d->clientVCard.setType(QXmppIq::Set);
    auto vcard = d->clientVCard;
    client()->send(std::move(vcard));
}

/*!
    This function requests the server for vCard of the connected user itself.
    Once received the signal clientVCardReceived() is emitted. Received vCard
    can be get using clientVCard().
*/
QString QXmppVCardManager::requestClientVCard()
{
    return requestVCard();
}

/*! Returns true if vCard of the connected client has been received else false. */
bool QXmppVCardManager::isClientVCardReceived() const
{
    return d->isClientVCardReceived;
}

QStringList QXmppVCardManager::discoveryFeatures() const
{
    return {
        // XEP-0054: vcard-temp
        staticString(ns_vcard),
    };
}

bool QXmppVCardManager::handleStanza(const QDomElement &element)
{
    if (isIqElement<QXmppVCardIq>(element)) {
        QXmppVCardIq vCardIq;
        vCardIq.parse(element);

        if (vCardIq.from().isEmpty() || vCardIq.from() == client()->configuration().jidBare()) {
            d->clientVCard = vCardIq;
            d->isClientVCardReceived = true;
            Q_EMIT clientVCardReceived();
        }

        Q_EMIT vCardReceived(vCardIq);

        return true;
    }

    return false;
}

void QXmppVCardManager::onRegistered(QXmppClient *client)
{
    if (auto manager = client->findExtension<QXmppAccountMigrationManager>()) {
        using DataResult = std::variant<VCardData, QXmppError>;

        auto importData = [this](const VCardData &data) {
            return setVCard(data.vCard);
        };

        auto exportData = [this]() -> QXmppTask<DataResult> {
            co_return mapSuccess(
                co_await fetchVCard(this->client()->configuration().jidBare()),
                [](const QXmppVCardIq &iq) { return VCardData { iq }; });
        };

        manager->registerExportData<VCardData>(importData, exportData);
    }
}

void QXmppVCardManager::onUnregistered(QXmppClient *client)
{
    if (auto manager = client->findExtension<QXmppAccountMigrationManager>()) {
        manager->unregisterExportData<VCardData>();
    }
}

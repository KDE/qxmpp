// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppVersionManager.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager.h"
#include "QXmppIqHandling.h"
#include "QXmppStanzaHandling_p.h"
#include "QXmppUtils.h"
#include "QXmppVersionIq.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QSysInfo>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace {

// XEP-0092: Software Version
struct SoftwareVersion {
    static constexpr std::tuple XmlTag = { u"query", ns_version };

    QString name;
    QString version;
    QString os;

    static std::optional<SoftwareVersion> fromDom(const QDomElement &el)
    {
        return SoftwareVersion {
            el.firstChildElement(u"name"_s).text(),
            el.firstChildElement(u"version"_s).text(),
            el.firstChildElement(u"os"_s).text(),
        };
    }
    void toXml(XmlWriter &w) const
    {
        w.write(Element {
            XmlTag,
            OptionalTextElement { u"name", name },
            OptionalTextElement { u"os", os },
            OptionalTextElement { u"version", version },
        });
    }
};

}  // namespace

class QXmppVersionManagerPrivate
{
public:
    QString clientName;
    QString clientVersion;
    QString clientOs;
};

QXmppVersionManager::QXmppVersionManager()
    : d(std::make_unique<QXmppVersionManagerPrivate>())
{
    hooks().add<Client::StanzaReceived>(Private::unencryptedStanzaHandler(this, &QXmppVersionManager::handleIncomingStanza));
    hooks().add<Iq::Received<Iq::Get<SoftwareVersion>>>([this](Iq::Get<SoftwareVersion> &&) {
        return SoftwareVersion { d->clientName, d->clientVersion, d->clientOs };
    });
    hooks().add<Disco::CollectInfo>([](QList<QString> &features, auto &) {
        // XEP-0092: Software Version
        features << staticString(ns_version);
    });

    d->clientName = qApp->applicationName();
    if (d->clientName.isEmpty()) {
        d->clientName = u"Based on QXmpp"_s;
    }

    d->clientOs = QSysInfo::prettyProductName();
    d->clientVersion = qApp->applicationVersion();
    if (d->clientVersion.isEmpty()) {
        d->clientVersion = QXmppVersion();
    }
}

QXmppVersionManager::~QXmppVersionManager() = default;

/*!
    Request version information from the specified XMPP entity.

    \a jid.
*/
QString QXmppVersionManager::requestVersion(const QString &jid)
{
    QXmppVersionIq request;
    request.setType(QXmppIq::Get);
    request.setTo(jid);
    return client()->sendLegacyId(request);
}

/*! Sets the local XMPP client's \a name. */
void QXmppVersionManager::setClientName(const QString &name)
{
    d->clientName = name;
}

/*! Sets the local XMPP client's \a version. */
void QXmppVersionManager::setClientVersion(const QString &version)
{
    d->clientVersion = version;
}

/*!
    Sets the local XMPP client's operating system.

    \a os.
*/
void QXmppVersionManager::setClientOs(const QString &os)
{
    d->clientOs = os;
}

/*!
    Returns the local XMPP client's name.

    By default this is set to the QApplication::applicationName(), or
    "Based on QXmpp" if not specified.
*/
QString QXmppVersionManager::clientName() const
{
    return d->clientName;
}

/*!
    Returns the local XMPP client's version.

    By default this is set to QApplication::applicationVersion(), or
    QXmpp's version if not specified.
*/
QString QXmppVersionManager::clientVersion() const
{
    return d->clientVersion;
}

/*!
    Returns the local XMPP client's operating system.

    By default this equals to QSysInfo::prettyProductName() which contains the
    OS name and version (e.g. "Windows 8.1" or "Debian GNU/Linux buster").
*/
QString QXmppVersionManager::clientOs() const
{
    return d->clientOs;
}

bool QXmppVersionManager::handleIncomingStanza(const QDomElement &element)
{
    if (isIqElement<QXmppVersionIq>(element) && !isIqRequest(element)) {
        QXmppVersionIq versionIq;
        versionIq.parse(element);

        if (versionIq.type() == QXmppIq::Result) {
            // emit response
            Q_EMIT versionReceived(versionIq);
        }

        return true;
    }

    return false;
}

QXmppVersionIq QXmppVersionManager::handleIq(QXmppVersionIq &&iq)
{
    QXmppVersionIq response;
    response.setType(QXmppIq::Result);
    response.setName(clientName());
    response.setVersion(clientVersion());
    response.setOs(clientOs());
    return response;
}

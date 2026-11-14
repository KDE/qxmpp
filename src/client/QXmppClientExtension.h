// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCLIENTEXTENSION_H
#define QXMPPCLIENTEXTENSION_H

#include "QXmppDiscoveryIq.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppExtension.h"
#include "QXmppHooks.h"
#include "QXmppLogger.h"

#include <memory>

class QDomElement;

class QXmppClient;
class QXmppClientExtensionPrivate;
class QXmppMessage;

namespace QXmpp::Connection {

struct StanzaReceived {
    static constexpr QStringView Name = u"org.qxmpp.connection.stanza-received";
    using Signature = QXmpp::HookResult(const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata);
};

}  // namespace QXmpp::Connection

/*!
    \inmodule QXmpp

    \brief The QXmppClientExtension class is the base class for QXmppClient
    extensions.

    If you want to extend QXmppClient, for instance to support an IQ type
    which is not natively supported, you can subclass QXmppClientExtension
    and add handlers to hooks, e.g. QXmpp::Connection::StanzaReceived for incoming stanzas or
    QXmpp::Messages::Received for incoming messages. You can then add your extension to the
    client instance using QXmppClient::addExtension().

    \ingroup qxmpp-core
*/
class QXMPP_EXPORT QXmppClientExtension : public QXmppLoggable, public QXmppExtension
{
    Q_OBJECT

public:
    QXmppClientExtension();
    ~QXmppClientExtension() override;

    // Always declared, so the layout of the vtable does not depend on QXMPP_DISABLE_DEPRECATED_BEFORE.
    [[deprecated("Use the QXmpp::Disco::CollectInfo hook")]]
    virtual QStringList discoveryFeatures() const;
    [[deprecated("Use the QXmpp::Disco::CollectInfo hook")]]
    virtual QList<QXmppDiscoIdentity> discoveryIdentities() const;

    [[deprecated("Use the QXmpp::Connection::StanzaReceived hook")]]
    virtual bool handleStanza(const QDomElement &stanza);
    [[deprecated("Use the QXmpp::Connection::StanzaReceived hook")]]
    virtual bool handleStanza(const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata);

protected:
    QXmppClient *client() const;
    QXmpp::Hooks hooks() const;
    virtual void setClient(QXmppClient *client);

    virtual void onRegistered(QXmppClient *client);
    virtual void onUnregistered(QXmppClient *client);

    void injectIq(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata);
    bool injectMessage(QXmppMessage &&message);

private:
    std::unique_ptr<QXmppClientExtensionPrivate> d;

    friend class QXmppClient;
};

#endif

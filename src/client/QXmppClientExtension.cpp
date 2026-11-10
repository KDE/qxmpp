// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClientExtension.h"

#include "QXmppClient.h"
#include "QXmppClient_p.h"
#include "QXmppHooks_p.h"

using namespace QXmpp;

class QXmppClientExtensionPrivate
{
public:
    QXmppClient *client = nullptr;
    // handlers of this extension, added to each client the extension is added to
    Private::HookRegistry hooks;
};

/*! Constructs a QXmppClient extension. */
QXmppClientExtension::QXmppClientExtension()
    : d(std::make_unique<QXmppClientExtensionPrivate>())
{
}

QXmppClientExtension::~QXmppClientExtension() = default;

/*!
    Returns the discovery features to add to the client.

    \deprecated since QXmpp 1.18, add the features via the QXmpp::Disco::CollectInfo hook
    instead. Implementations are still used, after the handlers of the hook.
*/
QStringList QXmppClientExtension::discoveryFeatures() const
{
    return QStringList();
}

/*!
    Returns the discovery identities to add to the client.

    \deprecated since QXmpp 1.18, add the identities via the QXmpp::Disco::CollectInfo hook
    instead. Implementations are still used, after the handlers of the hook.
*/
QList<QXmppDiscoIdentity> QXmppClientExtension::discoveryIdentities() const
{
    return QList<QXmppDiscoIdentity>();
}

/*!
    \brief You need to implement this method to process incoming XMPP
    stanzas.

    \a stanza is the DOM element to be handled. \a e2eeMetadata, if the element has been
    decrypted, contains metadata about the encryption.

    Returns true if the stanza was handled and no further processing should occur, or false to
    let other extensions process the stanza.

    \since QXmpp 1.5
*/
bool QXmppClientExtension::handleStanza(const QDomElement &, const std::optional<QXmppE2eeMetadata> &)
{
    return false;
}

/*! Returns the client which loaded this extension. */
QXmppClient *QXmppClientExtension::client() const
{
    return d->client;
}

/*!
    Returns the hooks of the client with this extension as the owner of added handlers.

    Handlers can already be added in the constructor of the extension. All handlers, whenever
    they were added, are active from the time the extension is added to a client until it is
    removed or destroyed. An extension can only be added to one client. Hooks can only be run
    while the extension is added to a client.

    \since QXmpp 1.18
*/
Hooks QXmppClientExtension::hooks() const
{
    auto *self = const_cast<QXmppClientExtension *>(this);
    return { d->client ? &d->client->d->hookRegistry : nullptr, &d->hooks, self };
}

/*!
    Sets the \a client which loaded this extension.
*/
void QXmppClientExtension::setClient(QXmppClient *client)
{
    if (d->client != nullptr) {
        onUnregistered(d->client);
        d->client->d->hookRegistry.removeAll(this);
    }

    d->client = client;

    if (client != nullptr) {
        client->d->hookRegistry.addAll(d->hooks);
        onRegistered(client);
    }
}

/*!
    Called after the extension has been added to the QXmppClient \a client.
*/
void QXmppClientExtension::onRegistered(QXmppClient *client)
{
    Q_UNUSED(client);
}

/*!
    Called after the extension has been removed from the QXmppClient \a client.
*/
void QXmppClientExtension::onUnregistered(QXmppClient *client)
{
    Q_UNUSED(client);
}

/*!
    Injects the IQ \a element into the client.

    The IQ is handled like any other stanza received via the XMPP stream.

    \a e2eeMetadata is the end-to-end encryption metadata for the IQ. It should be set if the
    stanza has been decrypted with an end-to-end encryption.

    \since QXmpp 1.5
*/
void QXmppClientExtension::injectIq(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata)
{
    client()->injectIq(element, e2eeMetadata);
}

/*!
    Injects a message stanza into the client.
    Returns whether the stanza was handled.

    The stanza is processed by the client with all extensions implementing
    MessageHandler.

    \since QXmpp 1.5

    \a message.
*/
bool QXmppClientExtension::injectMessage(QXmppMessage &&message)
{
    return client()->injectMessage(std::move(message));
}

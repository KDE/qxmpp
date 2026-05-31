// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppClientExtension.h"

#include "QXmppClient.h"

/*! Constructs a QXmppClient extension. */
QXmppClientExtension::QXmppClientExtension()
    : m_client(nullptr)
{
}

QXmppClientExtension::~QXmppClientExtension() = default;

/*! Returns the discovery features to add to the client. */
QStringList QXmppClientExtension::discoveryFeatures() const
{
    return QStringList();
}

/*! Returns the discovery identities to add to the client. */
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
    return m_client;
}

/*!
    Sets the \a client which loaded this extension.
*/
void QXmppClientExtension::setClient(QXmppClient *client)
{
    if (m_client != nullptr) {
        onUnregistered(m_client);
    }

    m_client = client;

    if (client != nullptr) {
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

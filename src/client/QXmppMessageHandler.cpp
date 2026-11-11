// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
    \class QXmppMessageHandler
    \inmodule QXmpp

    Interface for handling messages.

    \deprecated since QXmpp 1.18, add a handler to the QXmpp::Messages::Received hook instead.
    Implementations are still called, after the handlers of the hook, but not for messages of
    archive queries.

    \since QXmpp 1.5
*/

/*!
    \fn bool QXmppMessageHandler::handleMessage(const QXmppMessage &message)

    Handles \a message.

    Returns whether the message has been handled and no other extensions should process it.
*/

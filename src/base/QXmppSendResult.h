// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSENDRESULT_H
#define QXMPPSENDRESULT_H

#include "QXmppError.h"

#include <variant>

namespace QXmpp {

/*!
    \inmodule QXmpp

    \brief Describes the type of a packet sending error.

    \since QXmpp 1.5

    \value SocketWriteError The packet was written to the socket with no success (only happens when Stream Management is disabled).
    \value Disconnected The packet couldn't be sent because the connection hasn't been (re)established.
    \value EncryptionError The packet couldn't be sent because prior encryption failed.
    \value Cancelled The packet was not sent because sending has been cancelled, e.g. by a
    handler of the QXmpp::Messages::Prepare hook. (Since QXmpp 1.18)
    \value ConfirmationTimeout The packet was sent, but its delivery has not been confirmed in
    time, e.g. a MUC room or MIX channel has not reflected the message. It may have been
    delivered nevertheless. (Since QXmpp 1.18)
    \value NoEncryptionRecipients The packet couldn't be encrypted because it can not be encrypted
    for any of the recipients, e.g. none of their devices has an accepted trust level. Clients
    should not send it unencrypted without asking the user: the server could hide the devices.
    (Since QXmpp 1.18)
*/
enum class SendError : uint8_t {
    SocketWriteError,
    Disconnected,
    EncryptionError,
    Cancelled,
    ConfirmationTimeout,
    NoEncryptionRecipients,
};

/*!
    \inmodule QXmpp

    \brief A struct indicating success when sending packets.

    \since QXmpp 1.5
*/
struct SendSuccess {
    /*! Indicates whether the packet has been acknowledged by the other peer. */
    bool acknowledged = false;
};

/*!
    \typealias QXmpp::SendResult

    A variant containing either a SendSuccess object or a QXmppError.

    The QXmppError will likely contain a SendError.
*/
using SendResult = std::variant<SendSuccess, QXmppError>;

}  // namespace QXmpp

#endif  // QXMPPSENDRESULT_H

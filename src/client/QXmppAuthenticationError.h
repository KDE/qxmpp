// SPDX-FileCopyrightText: 2024 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPAUTHENTICATIONERROR_H
#define QXMPPAUTHENTICATIONERROR_H

#include "QXmppGlobal.h"

#include <any>

namespace QXmpp {

/*!
    \inmodule QXmpp

    Indicates an authentication error

    \since QXmpp 1.7
*/
struct AuthenticationError {
    /*!
        Describes the type of the authentication error.

        \value NotAuthorized The provided credentials have been rejected by the server.
        \value AccountDisabled The server did not allow authentication because the account is currently disabled.
        \value CredentialsExpired Used credentials are not valid anymore.
        \value EncryptionRequired Authentication is only allowed with an encrypted connection.
        \value MechanismMismatch Authenticated could not be completed because the server did not offer a compatible authentication mechanism.
        \value ProcessingError Local error while processing authentication challenges.
        \value RequiredTasks The server requested for tasks that are not supported.
    */
    enum Type {
        NotAuthorized,
        AccountDisabled,
        CredentialsExpired,
        EncryptionRequired,
        MechanismMismatch,
        ProcessingError,
        RequiredTasks,
    };
    /*! Type of the authentication error */
    Type type;
    /*! Error message from the server */
    QString text;
    /*! Protocol-specific details provided to the error */
    std::any details;
};

}  // namespace QXmpp

#endif  // QXMPPAUTHENTICATIONERROR_H

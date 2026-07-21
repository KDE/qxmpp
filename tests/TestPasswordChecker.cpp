// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "TestPasswordChecker.h"

void TestPasswordChecker::addCredentials(const QString &user, const QString &password)
{
    m_credentials.insert(user, password);
}

QXmppPasswordReply::Error TestPasswordChecker::getPassword(const QXmppPasswordRequest &request, QString &password)
{
    if (m_credentials.contains(request.username())) {
        password = m_credentials.value(request.username());
        return QXmppPasswordReply::NoError;
    }
    return QXmppPasswordReply::AuthorizationError;
}

bool TestPasswordChecker::hasGetPassword() const
{
    return true;
}

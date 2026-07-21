// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef TESTS_TESTPASSWORDCHECKER_H
#define TESTS_TESTPASSWORDCHECKER_H

#include "QXmppPasswordChecker.h"

#include <QMap>

class TestPasswordChecker : public QXmppPasswordChecker
{
public:
    void addCredentials(const QString &user, const QString &password);

    /*! Retrieves the password for the given username. */
    QXmppPasswordReply::Error getPassword(const QXmppPasswordRequest &request, QString &password) override;

    /*! Returns whether getPassword() is enabled. */
    bool hasGetPassword() const override;

private:
    QMap<QString, QString> m_credentials;
};

#endif  // TESTS_TESTPASSWORDCHECKER_H

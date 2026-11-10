// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef INTEGRATIONTESTING_H
#define INTEGRATIONTESTING_H

#include "QXmppConfiguration.h"

#include <QDebug>
#include <QtGlobal>

#define ENV_ENABLED "QXMPP_TESTS_INTEGRATION_ENABLED"
#define ENV_JID "QXMPP_JID"
#define ENV_PASSWORD "QXMPP_PASSWORD"
#define ENV_JID_2 "QXMPP_JID_2"
#define ENV_PASSWORD_2 "QXMPP_PASSWORD_2"

#define SKIP_IF_INTEGRATION_TESTS_DISABLED()                            \
    if (!IntegrationTests::enabled()) {                                 \
        QSKIP("Export 'QXMPP_TESTS_INTEGRATION_ENABLED=1' to enable."); \
    } else if (!IntegrationTests::credentialsAvailable()) {             \
        QFAIL("No credentials for integration tests provided! "         \
              "Export 'QXMPP_JID' and 'QXMPP_PASSWORD'.");              \
    }

class IntegrationTests
{
public:
    static QString environmentVariable(const char *varName, const QString &defaultValue = {})
    {
        return qEnvironmentVariable(varName, defaultValue);
    }

    static bool enabled()
    {
        return environmentVariable(ENV_ENABLED, "0") == "1";
    }

    // for local test servers with self-signed certificates
    static bool ignoreSslErrors()
    {
        return environmentVariable("QXMPP_TESTS_IGNORE_SSL_ERRORS", "0") == "1";
    }

    static bool credentialsAvailable()
    {
        return !qEnvironmentVariableIsEmpty(ENV_JID) && !qEnvironmentVariableIsEmpty(ENV_PASSWORD);
    }

    static QXmppConfiguration clientConfiguration()
    {
        QXmppConfiguration config;
        config.setJid(environmentVariable(ENV_JID));
        config.setPassword(environmentVariable(ENV_PASSWORD));
        return config;
    }

    // a second account, e.g. for exchanging messages
    static bool secondAccountAvailable()
    {
        return !qEnvironmentVariableIsEmpty(ENV_JID_2) && !qEnvironmentVariableIsEmpty(ENV_PASSWORD_2);
    }

    static QXmppConfiguration secondClientConfiguration()
    {
        QXmppConfiguration config;
        config.setJid(environmentVariable(ENV_JID_2));
        config.setPassword(environmentVariable(ENV_PASSWORD_2));
        return config;
    }
};

#undef ENV_ENABLED
#undef ENV_JID
#undef ENV_PASSWORD
#undef ENV_JID_2
#undef ENV_PASSWORD_2

#endif  // INTEGRATIONTESTING_H

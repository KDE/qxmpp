// SPDX-FileCopyrightText: 2026 J SHIVA SHANKAR <jangamshiva2005@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "example_1a_echoClientOmemo.h"

#include "QXmppAtmManager.h"
#include "QXmppCarbonManagerV2.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMessage.h"
#include "QXmppOmemoManager.h"
#include "QXmppPubSubManager.h"

#include <QCoreApplication>

EchoClientOmemo::EchoClientOmemo(QObject *parent)
    : QXmppClient(parent)
{
    connect(this, &QXmppClient::messageReceived, this, &EchoClientOmemo::messageReceived);

    addNewExtension<QXmppPubSubManager>();
    addNewExtension<QXmppCarbonManagerV2>();

    addNewExtension<QXmppAtmManager>(&m_trustStorage);

    m_omemoManager = addNewExtension<QXmppOmemoManager>(&m_omemoStorage);

    setEncryptionExtension(m_omemoManager);
    m_omemoManager->setSecurityPolicy(QXmpp::Toakafa);

    // This example uses QXmppOmemoMemoryStorage and QXmppAtmTrustMemoryStorage,
    // which keep everything in memory only. load() will always fail on startup
    // (nothing persisted), so setUp() runs on every launch and the client gets a
    // new OMEMO device identity each time.
    //
    // A real application must implement persistent QXmppOmemoStorage and
    // QXmppTrustStorage subclasses. The typical flow is:
    //   1. Call load() before connecting.
    //   2. If load() returned true, OMEMO is already initialized.
    //   3. If load() returned false, call setUp() after `connected` to perform
    //      the one-time device registration, and persist the state.

    m_omemoManager->load().then(this, [](bool success) {
        if (success) {
            qDebug() << "OMEMO keys loaded successfully.";
        } else {
            qWarning() << "Failed to load OMEMO keys!";
        }
    });

    connect(this, &QXmppClient::connected, this, [this]() {
        if (m_omemoInitialized) {
            return;
        }
        m_omemoManager->setUp().then(this, [this](bool success) {
            if (success) {
                m_omemoInitialized = true;
                qDebug() << "OMEMO keys set up successfully.";
            } else {
                qWarning() << "Failed to set up OMEMO keys!";
            }
        });
    });
}

EchoClientOmemo::~EchoClientOmemo() = default;

void EchoClientOmemo::messageReceived(const QXmppMessage &message)
{
    if (message.body().isEmpty()) {
        return;
    }

    reply(QXmppMessage({}, message.from(), u"Your message: " + message.body()),
          message.e2eeMetadata());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    EchoClientOmemo client;
    client.logger()->setLoggingType(QXmppLogger::StdoutLogging);
    client.connectToServer("qxmpp.test1@qxmpp.org", "qxmpp123");

    return app.exec();
}

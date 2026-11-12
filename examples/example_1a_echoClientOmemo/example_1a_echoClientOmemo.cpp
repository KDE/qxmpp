// SPDX-FileCopyrightText: 2026 J SHIVA SHANKAR <jangamshiva2005@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "example_1a_echoClientOmemo.h"

#include "QXmppAtmManager.h"
#include "QXmppCarbonManagerV2.h"
#include "QXmppE2eeMetadata.h"
#include "QXmppMessage.h"
#include "QXmppMessages.h"
#include "QXmppOmemoManager.h"
#include "QXmppOmemoMemoryStorage.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils.h"

#include <QCoreApplication>

EchoClientOmemo::EchoClientOmemo(QObject *parent)
    : QXmppClient(parent)
{
    connect(this, &QXmppClient::messageReceived, this, &EchoClientOmemo::messageReceived);

    addNewExtension<QXmppPubSubManager>();
    addNewExtension<QXmppCarbonManagerV2>();
    addNewExtension<QXmppAtmManager>(&m_trustStorage);

    // The memory storage keeps everything in memory only: the bot gets a new OMEMO device on every
    // start. A real application implements QXmpp::Omemo::Storage persistently.
    //
    // The manager loads its data from the storage and creates and publishes the own device on the
    // first login. OMEMO 0.3 and OMEMO 2 are both enabled by default.
    auto *omemoManager = addNewExtension<QXmpp::Omemo::Manager>(std::make_shared<QXmpp::Omemo::MemoryStorage>());
    connect(omemoManager, &QXmpp::Omemo::Manager::errorOccurred, this, [](const QXmppError &error) {
        qWarning() << "OMEMO error:" << error.description;
    });

    // The encryption of outgoing messages is selected per contact: replies use the encryption of
    // the last message of the contact. A real application would use a setting of the user per
    // conversation instead.
    hooks(this).add<QXmpp::Messages::SelectEncryption>([this](QXmpp::Messages::EncryptionSelection &selection) {
        if (const auto encryption = m_encryptions.value(QXmppUtils::jidToBareJid(selection.to())); !encryption.isEmpty()) {
            selection.setEncryption(encryption);
        }
    });
}

EchoClientOmemo::~EchoClientOmemo() = default;

void EchoClientOmemo::messageReceived(const QXmppMessage &message)
{
    if (message.body().isEmpty()) {
        return;
    }

    const auto metadata = message.e2eeMetadata();
    const auto encryption = metadata ? metadata->encryptionNamespace() : QString();
    if (encryption == QXmpp::E2ee::Omemo0 || encryption == QXmpp::E2ee::Omemo2) {
        m_encryptions.insert(QXmppUtils::jidToBareJid(message.from()), encryption);
    } else {
        m_encryptions.remove(QXmppUtils::jidToBareJid(message.from()));
    }

    send(QXmppMessage({}, message.from(), u"Your message: " + message.body()));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    EchoClientOmemo client;
    client.logger()->setLoggingType(QXmppLogger::StdoutLogging);
    client.connectToServer("qxmpp.test1@qxmpp.org", "qxmpp123");

    return app.exec();
}

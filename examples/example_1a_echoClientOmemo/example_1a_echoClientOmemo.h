// SPDX-FileCopyrightText: 2026 J SHIVA SHANKAR <jangamshiva2005@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef EXAMPLE_1A_ECHOCLIENTOMEMO_H
#define EXAMPLE_1A_ECHOCLIENTOMEMO_H

#include "QXmppAtmTrustMemoryStorage.h"
#include "QXmppClient.h"
#include "QXmppOmemoMemoryStorage.h"

class QXmppMessage;
class QXmppOmemoManager;

class EchoClientOmemo : public QXmppClient
{
    Q_OBJECT

public:
    EchoClientOmemo(QObject *parent = nullptr);
    ~EchoClientOmemo() override;

    void messageReceived(const QXmppMessage &);

private:
    QXmppAtmTrustMemoryStorage m_trustStorage;
    QXmppOmemoMemoryStorage m_omemoStorage;
    QXmppOmemoManager *m_omemoManager = nullptr;
    bool m_omemoInitialized = false;
};

#endif  // EXAMPLE_1A_ECHOCLIENTOMEMO_H

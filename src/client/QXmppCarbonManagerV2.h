// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCARBONMANAGERV2_H
#define QXMPPCARBONMANAGERV2_H

#include "QXmppClientExtension.h"

#include <QProperty>

class QXMPP_EXPORT QXmppCarbonManagerV2 : public QXmppClientExtension
{
    Q_OBJECT
public:
    QXmppCarbonManagerV2();
    ~QXmppCarbonManagerV2();

    QBindable<bool> enabled() const;

    bool handleStanza(const QDomElement &, const std::optional<QXmppE2eeMetadata> &) override;

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void enableCarbons();

    QProperty<bool> m_enabled = QProperty<bool> { false };
};

#endif  // QXMPPCARBONMANAGERV2_H

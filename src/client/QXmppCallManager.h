// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALLMANAGER_H
#define QXMPPCALLMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppTask.h"

class QXmppCall;
class QXmppCallManagerPrivate;
class QXmppIq;
class QXmppJingleIq;
class QXmppPresence;

namespace QXmpp {
struct StunServer;
struct TurnServer;
}  // namespace QXmpp

class QXMPP_EXPORT QXmppCallManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    /*!
        Media type for starting a call.

        \value Audio Only contains an audio stream.
        \value AudioVideo Contains audio and video streams.
    */
    enum class Media {
        Audio,
        AudioVideo,
    };

    QXmppCallManager();
    ~QXmppCallManager() override;

    void setFallbackStunServers(const QList<QXmpp::StunServer> &);
    void setFallbackTurnServer(const std::optional<QXmpp::TurnServer> &);
    bool dtlsRequired() const;
    void setDtlsRequired(bool);

    QStringList discoveryFeatures() const override;
    bool handleStanza(const QDomElement &element) override;

    Q_SIGNAL void callReceived(std::unique_ptr<QXmppCall> &call);

    std::unique_ptr<QXmppCall> call(const QString &jid, Media media = Media::Audio, const QString &proposedSid = {});

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    void onCallDestroyed(QObject *object);
    void onDisconnected();
    using IncomingIqResult = std::variant<QXmppIq, QXmppStanza::Error, QXmppTask<std::variant<QXmppIq>>>;
    IncomingIqResult handleIq(QXmppJingleIq &&iq);
    void onPresenceReceived(const QXmppPresence &presence);
    QXmppTask<void> refreshStunTurnConfig();

    const std::unique_ptr<QXmppCallManagerPrivate> d;
    friend class QXmppCall;
    friend class QXmppCallPrivate;
    friend class QXmppCallManagerPrivate;
    friend class tst_QXmppCallManager;
};

#endif

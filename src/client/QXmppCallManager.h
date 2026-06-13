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

/*!
    \inmodule QXmpp

    \brief The QXmppCallManager class provides support for making and receiving
    voice and video calls using \xep{0166}{Jingle}.

    Session initiation is performed as described by \xep{0166}{Jingle}, \xep{0167}{Jingle RTP
    Sessions} and \xep{0176}{Jingle ICE-UDP Transport Method}.

    The data stream is connected using Interactive Connectivity Establishment (RFC 5245) and data
    is transferred using Real Time Protocol (RFC 3550) packets.

    To make use of this manager, you need to instantiate it and load it into the QXmppClient
    instance as follows:
    ```cpp
    auto *client = new QXmppClient();
    auto *callManager = client->addNewExtension<QXmppCallManager>();
    ```

    \section1 Call interaction

    Incoming calls are exposed via the callReceived() signal. You can take ownership of the call by
    moving the unique_ptr, otherwise the call manager will decline and delete the call. You can
    accept or reject (hang up) the call.

    Outgoing calls are created using call().

    In both cases you are responsible for taking ownership of the call. Note that QXmppCalls in
    another state than finished require the QXmppCallManager to be active, though. You must not
    delete the QXmppCallManager until all QXmppCalls are in finished state.

    \section1 XEP-0320: Use of DTLS-SRTP in Jingle Sessions

    DTLS-SRTP allows to encrypt peer-to-peer calls. Internally, a TLS handshake is done to
    negotiate keys for SRTP (Secure RTP). By default DTLS is not enforced, this can be done using
    setDtlsRequired(), though.

    DTLS-SRTP by default exchanges the fingerprint via unencrypted XMPP packets. This means that
    the XMPP server could potentially replace the fingerprint or prevent the clients from using
    DTLS at all. However, the actual media connection is typically peer-to-peer, so the XMPP server
    does not have access to the transmitted data.

    Support for DTLS-SRTP is available since QXmpp 1.11.

    \warning THIS API IS NOT FINALIZED YET

    \ingroup Managers
*/
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

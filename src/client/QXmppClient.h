// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2023 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCLIENT_H
#define QXMPPCLIENT_H

#include "QXmppConfiguration.h"
#include "QXmppLogger.h"
#include "QXmppPresence.h"
#include "QXmppSendResult.h"
#include "QXmppSendStanzaParams.h"
#include "QXmppStreamError.h"

#include <memory>
#include <variant>

#include <QAbstractSocket>
#include <QObject>
#include <QSslError>

template<typename T>
class QXmppTask;

class QXmppE2eeExtension;
class QXmppClientExtension;
class QXmppClientPrivate;
class QXmppMessage;
class QXmppOutgoingClient;
class QXmppPresence;
class QXmppIq;

// managers
class QXmppDiscoveryIq;
class QXmppRosterManager;
class QXmppVCardManager;
class QXmppVersionManager;

namespace QXmpp::Private {
struct SessionBegin;
}

/*!
    \inmodule QXmpp

    \brief The QXmppClient class is the main class for connecting to an XMPP server.

    It provides the core functionality for establishing a connection, sending and
    receiving stanzas, and managing client extensions.

    This class will provide the handle/reference to QXmppRosterManager
    (roster management), QXmppVCardManager (vCard manager), and
    QXmppVersionManager (software version information).

    By default, the client will automatically try reconnecting to the server.
    You can change that behaviour using
    QXmppConfiguration::setAutoReconnectionEnabled().

    Not all the managers or extensions have been enabled by default. One can
    enable/disable the managers using the functions \c addExtension() and
    \c removeExtension(). \c findExtension() can be used to find a
    reference/pointer to a particular instantiated and enabled manager.

    List of managers enabled by default:
    \list
        \li QXmppRosterManager
        \li QXmppVCardManager
        \li QXmppVersionManager
        \li QXmppDiscoveryManager
        \li QXmppEntityTimeManager
    \endlist

    \section1 Connection details

    If no explicit host and port are configured, the client will look up the SRV records of the
    domain of the configured JID. Since QXmpp 1.8 both TCP and direct TLS records are looked up
    and connection via direct TLS is preferred as it saves the extra round trip from STARTTLS. See
    also \xep{0368}{SRV records for XMPP over TLS}.

    On connection errors the other SRV records are tested too (if multiple are available).

    For servers without SRV records or if looking up the records did not succeed, domain and the
    default port of 5223 (TLS) and 5222 (TCP/STARTTLS) are tried.

    \section1 Usage of FAST token-based authentication

    QXmpp uses \xep{0484}{Fast Authentication Streamlining Tokens} if enabled and supported by the
    server. FAST tokens can be requested after a first time authentication using a password or
    another strong authentication mechanism. The tokens can then be used to log in, without a
    password. The tokens are linked to a specific device ID (set via the SASL 2 user agent) and
    only this device can use the token. Tokens also expire and are rotated by the server.

    The advantage of this mechanism is that a client does not necessarily need to store the
    password of an account and in the future clients that are logged in could be listed and logged
    out manually. FAST also allows for performance improvements as it only requires one round trip
    for authentication (and may be included in TLS 0-RTT data although that is not implemented in
    QXmpp) while other mechanisms like SCRAM need multiple round trips.

    FAST itself is enabled by default (see QXmppConfiguration::useFastTokenAuthentication()), but
    you also need to set a SASL user agent with a stable device ID, so FAST can be used.

    After that you can login and use QXmppCredentials to serialize the token data and store it
    permanently. Note that the token may change over time, though.

    \ingroup Core
*/
class QXMPP_EXPORT QXmppClient : public QXmppLoggable
{
    Q_OBJECT

    /*!
        \property QXmppClient::logger

        The QXmppLogger associated with the current QXmppClient
    */
    Q_PROPERTY(QXmppLogger *logger READ logger WRITE setLogger NOTIFY loggerChanged)
    /*!
        \property QXmppClient::state

        The client's current state
    */
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    using IqResult = std::variant<QDomElement, QXmppError>;
    using EmptyResult = std::variant<QXmpp::Success, QXmppError>;

    /*!
        An enumeration for type of error.
        Error could come due a TCP socket or XML stream or due to various stanzas.

        \value NoError No error.
        \value SocketError Error due to TCP socket.
        \value KeepAliveError Error due to no response to a keep alive.
        \value XmppStreamError Error due to XML stream.
    */
    enum Error {
        NoError,
        SocketError,
        KeepAliveError,
        XmppStreamError,
    };
    Q_ENUM(Error)

    /*!
        This enumeration describes a client state.

        \value DisconnectedState Disconnected from the server.
        \value ConnectingState Trying to connect to the server.
        \value ConnectedState Connected to the server.
    */
    enum State {
        DisconnectedState,
        ConnectingState,
        ConnectedState
    };
    Q_ENUM(State)

    /*!
        Describes the use of \xep{0198}{Stream Management}

        \value NoStreamManagement Stream Management is not used.
        \value NewStream Stream Management is used and the previous stream has not been resumed.
        \value ResumedStream Stream Management is used and the previous stream has been resumed.
    */
    enum StreamManagementState {
        NoStreamManagement,
        NewStream,
        ResumedStream
    };

    /*!
        Used to decide which extensions should be added by default.
        \since QXmpp 1.6

        \value NoExtensions Creates a client without any extensions.
        \value BasicExtensions Creates a client with the default set of extensions.
    */
    enum InitialExtensions {
        NoExtensions,
        BasicExtensions,
    };

    QXmppClient(InitialExtensions, QObject *parent = nullptr);
    QXmppClient(QObject *parent = nullptr);
    ~QXmppClient() override;

    bool addExtension(QXmppClientExtension *extension);
    template<typename T, typename... Args>
    T *addNewExtension(Args... args)
    {
        // it's impossible that addExtension() returns false: ext is a new object
        auto *ext = new T(args...);
        addExtension(ext);
        return ext;
    }
    bool insertExtension(int index, QXmppClientExtension *extension);
    bool removeExtension(QXmppClientExtension *extension);
    QXmppE2eeExtension *encryptionExtension() const;
    void setEncryptionExtension(QXmppE2eeExtension *);

    QList<QXmppClientExtension *> extensions() const;

    /*!
        \brief Returns the extension which can be cast into type T*, or 0
        if there is no such extension.

        Usage example:
        \code
        QXmppDiscoveryManager* ext = client->findExtension<QXmppDiscoveryManager>();
        if(ext)
        {
        //extension found, do stuff...
        }
        \endcode
    */
    template<typename T>
    T *findExtension() const
    {
        const QList<QXmppClientExtension *> list = extensions();
        for (auto ext : list) {
            T *extension = qobject_cast<T *>(ext);
            if (extension) {
                return extension;
            }
        }
        return nullptr;
    }

    /*!
        \brief Returns the index of an extension

        Usage example:
        \code
        int index = client->indexOfExtension<QXmppDiscoveryManager>();
        if (index > 0) {
        // extension found, do stuff...
        } else {
        // extension not found
        }
        \endcode

        \since QXmpp 1.2
    */
    template<typename T>
    int indexOfExtension() const
    {
        auto list = extensions();
        for (int i = 0; i < list.size(); ++i) {
            if (qobject_cast<T *>(list.at(i)) != nullptr) {
                return i;
            }
        }
        return -1;
    }

    bool isAuthenticated() const;
    bool isConnected() const;

    bool isActive() const;
    void setActive(bool active);

    StreamManagementState streamManagementState() const;

    QXmppPresence clientPresence() const;
    void setClientPresence(const QXmppPresence &presence);

    QXmppConfiguration &configuration();

    QXmppLogger *logger() const;
    void setLogger(QXmppLogger *logger);

    QAbstractSocket::SocketError socketError();
    QString socketErrorString() const;

    State state() const;
    QXmppStanza::Error::Condition xmppStreamError();

    QXmppTask<QXmpp::SendResult> sendSensitive(QXmppStanza &&, const std::optional<QXmppSendStanzaParams> & = {});
    QXmppTask<QXmpp::SendResult> send(QXmppStanza &&, const std::optional<QXmppSendStanzaParams> & = {});
    QXmppTask<QXmpp::SendResult> reply(QXmppStanza &&stanza, const std::optional<QXmppE2eeMetadata> &e2eeMetadata, const std::optional<QXmppSendStanzaParams> & = {});
    QXmppTask<IqResult> sendIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> & = {});
    QXmppTask<IqResult> sendSensitiveIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> & = {});
    QXmppTask<EmptyResult> sendGenericIq(QXmppIq &&, const std::optional<QXmppSendStanzaParams> & = {});

    /*!
        This signal is emitted when the client connects successfully to the
        XMPP server i.e. when a successful XMPP connection is established.
        XMPP Connection involves following sequential steps:
        \list
            \li TCP socket connection
            \li Client sends start stream
            \li Server sends start stream
            \li TLS negotiation (encryption)
            \li Authentication
            \li Resource binding
            \li Session establishment
        \endlist

        After all these steps a successful XMPP connection is established and
        connected() signal is emitted.

        After the connected() signal is emitted QXmpp will send the roster
        request to the server. On receiving the roster, QXmpp will emit
        QXmppRosterManager::rosterReceived(). After this signal,
        QXmppRosterManager object gets populated and you can use
        \c findExtension<QXmppRosterManager>() to get the handle of
        QXmppRosterManager object.
    */
    Q_SIGNAL void connected();

    /*! This signal is emitted when the XMPP connection disconnects. */
    Q_SIGNAL void disconnected();

    /*!
        This signal is emitted when the XMPP connection encounters any error.
        The QXmppClient::Error parameter specifies the type of error occurred.
        It could be due to TCP socket or the xml stream or the stanza.
        Depending upon the type of error occurred use the respective get function to
        know the error.
    */
    Q_SIGNAL void error(QXmppClient::Error);

    /*!
        This signal is emitted when the XMPP connection encounters any fatal error.

        \a error is a detailed error object with a description. It may contain
        \list
            \li QAbstractSocket::SocketError
            \li QXmpp::TimeoutError
            \li QXmpp::StreamError
            \li QXmpp::AuthenticationError
            \li QXmpp::BindError
        \endlist

        \since QXmpp 1.7
    */
    Q_SIGNAL void errorOccurred(const QXmppError &error);

    /*! This signal is emitted when the \a logger changes. */
    Q_SIGNAL void loggerChanged(QXmppLogger *logger);

    /*!
        Notifies that an XMPP message stanza is received. The QXmppMessage
        parameter contains the details of the message sent to this client.
        In other words whenever someone sends you a message this signal is
        emitted.

        \a message is the received message stanza.
    */
    Q_SIGNAL void messageReceived(const QXmppMessage &message);

    /*!
        Notifies that an XMPP presence stanza is received. The QXmppPresence
        parameter contains the details of the presence sent to this client.
        This signal is emitted when someone login/logout or when someone's status
        changes Busy, Idle, Invisible etc.

        \a presence is the received presence stanza.
    */
    Q_SIGNAL void presenceReceived(const QXmppPresence &presence);

    /*!
        This signal is emitted when IQs of type result or error are received by
        the client and no registered QXmppClientExtension could handle it.

        This is useful when it is only important to check whether the response
        of an IQ was successful. However, the recommended way is still to use an
        additional QXmppClientExtension for this kind of tasks.

        \a iq is the received IQ stanza.
    */
    Q_SIGNAL void iqReceived(const QXmppIq &iq);

    /*!
        This signal is emitted to indicate that one or more SSL errors were
        encountered while establishing the identity of the server.

        \a errors.
    */
    Q_SIGNAL void sslErrors(const QList<QSslError> &errors);

    /*! This signal is emitted when the client \a state changes. */
    Q_SIGNAL void stateChanged(QXmppClient::State state);

    /*!
        Emitted when the credentials, e.g. tokens have changed.

        This means that the QXmppCredentials in the QXmppConfiguration of this client has changed.

        \since QXmpp 1.8
    */
    Q_SIGNAL void credentialsChanged();

    Q_SLOT void connectToServer(const QXmppConfiguration &, const QXmppPresence &initialPresence = {});
    Q_SLOT void connectToServer(const QString &jid, const QString &password);
    Q_SLOT void disconnectFromServer();

#if QXMPP_DEPRECATED_SINCE(1, 1)
    [[deprecated("Use findExtension<QXmppRosterManager>() instead")]]
    QXmppRosterManager &rosterManager();

    [[deprecated("Use findExtension<QXmppVCardManager>() instead")]]
    QXmppVCardManager &vCardManager();

    [[deprecated("Use findExtension<QXmppVersionManager>() instead")]]
    QXmppVersionManager &versionManager();
#endif
#if QXMPP_DEPRECATED_SINCE(1, 12)
    [[deprecated("Use async send()")]]
    Q_SLOT bool sendPacket(const QXmppNonza &);
    [[deprecated("Use async send()")]]
    Q_SLOT void sendMessage(const QString &bareJid, const QString &message);
#endif

    bool sendLegacy(const QXmppNonza &s)
    {
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        return sendPacket(s);
        QT_WARNING_POP
    }
    QString sendLegacyId(const QXmppStanza &s) { return sendLegacy(s) ? s.id() : QString(); }

private:
    QXmppOutgoingClient *stream() const;
    void injectIq(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &e2eeMetadata);
    bool injectMessage(QXmppMessage &&message);

    void setIgnoredStreamErrors(const QList<QXmpp::StreamError> &);

    void _q_elementReceived(const QDomElement &element, bool &handled);
    void _q_reconnect();
    void onInternalSocketStateChanged();
    void _q_streamConnected(const QXmpp::Private::SessionBegin &);
    void _q_streamDisconnected();

    const std::unique_ptr<QXmppClientPrivate> d;

    friend class QXmppClientExtension;
    friend class QXmppCarbonManagerV2;
    friend class QXmppRegistrationManager;
    friend class TestClient;
};

#endif  // QXMPPCLIENT_H

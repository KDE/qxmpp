// SPDX-FileCopyrightText: 2019 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Niels Ole Salscheider <ole@salscheider.org>
// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPCALL_H
#define QXMPPCALL_H

#include "QXmppCallStream.h"
#include "QXmppClientExtension.h"
#include "QXmppLogger.h"

#include <QMetaType>
#include <QObject>

class QHostAddress;
class QXmppCallPrivate;
class QXmppCallManager;
class QXmppCallManagerPrivate;
class QXmppError;

/*!
    \inmodule QXmpp

    \brief The QXmppCall class represents a voice or video call using \xep{0166}{Jingle}.

    \note THIS API IS NOT FINALIZED YET

    \sa QXmppCallManager
*/
class QXMPP_EXPORT QXmppCall : public QXmppLoggable
{
    Q_OBJECT
    /*!
        \property QXmppCall::direction

        The call's direction
    */
    Q_PROPERTY(Direction direction READ direction CONSTANT)
    /*!
        \property QXmppCall::jid

        The remote party's JID
    */
    Q_PROPERTY(QString jid READ jid CONSTANT)
    /*!
        \property QXmppCall::state

        The call's state
    */
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    /*!
        This enum is used to describe the direction of a call.

        \value IncomingDirection The call is incoming.
        \value OutgoingDirection The call is outgoing.
    */
    enum Direction {
        IncomingDirection,
        OutgoingDirection
    };
    Q_ENUM(Direction)

    /*!
        This enum is used to describe the state of a call.

        \value ConnectingState The call is being connected.
        \value ActiveState The call is active.
        \value DisconnectingState The call is being disconnected.
        \value FinishedState The call is finished.
    */
    enum State {
        ConnectingState = 0,
        ActiveState = 1,
        DisconnectingState = 2,
        FinishedState = 3
    };
    Q_ENUM(State)

    ~QXmppCall();

    QXmppCall::Direction direction() const;
    QString jid() const;
    QString sid() const;
    QXmppCall::State state() const;
    std::optional<QXmppError> error() const;

    GstElement *pipeline() const;
    QXmppCallStream *audioStream() const;
    QXmppCallStream *videoStream() const;

    bool isEncrypted() const;
    bool videoSupported() const;

    void accept();
    void decline();
    void hangUp();
    void addVideo();

    /*! \brief This signal is emitted when a call is connected. */
    Q_SIGNAL void connected();

    /*! \brief This signal is emitted when a call is finished. */
    Q_SIGNAL void finished();

    /*! \brief This signal is emitted when the remote party is ringing. */
    Q_SIGNAL void ringing();

    /*! \brief This signal is emitted when the call \a state changes. */
    Q_SIGNAL void stateChanged(QXmppCall::State state);

    /*! \brief This signal is emitted when a \a stream is created. */
    Q_SIGNAL void streamCreated(QXmppCallStream *stream);

private:
    void onLocalCandidatesChanged(QXmppCallStream *stream);
    void terminated();

    QXmppCall(const QString &jid, const QString &sid, Direction direction, QXmppCallManager *manager);
    QXmppCall(const QString &jid, const QString &sid, Direction direction, State state, QXmppError &&error, QXmppCallManager *manager);

    const std::unique_ptr<QXmppCallPrivate> d;
    friend class QXmppCallManager;
    friend class QXmppCallManagerPrivate;
    friend class QXmppCallPrivate;
};

Q_DECLARE_METATYPE(QXmppCall::State)

#endif

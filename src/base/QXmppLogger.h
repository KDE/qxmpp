// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPLOGGER_H
#define QXMPPLOGGER_H

#include "QXmppGlobal.h"

#include <memory>

#include <QObject>

#ifdef QXMPP_LOGGABLE_TRACE
#define qxmpp_loggable_trace(x) QString("%1(0x%2) %3").arg(metaObject()->className(), QString::number(reinterpret_cast<qint64>(this), 16), x)
#else
#define qxmpp_loggable_trace(x) (x)
#endif

class QXmppLoggerPrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppLogger class represents a sink for logging messages.

    \ingroup Core
*/
class QXMPP_EXPORT QXmppLogger : public QObject
{
    Q_OBJECT
    Q_FLAGS(MessageType MessageTypes)

    /*!
        \property QXmppLogger::logFilePath

        The path to which logging messages should be written
    */
    Q_PROPERTY(QString logFilePath READ logFilePath WRITE setLogFilePath NOTIFY logFilePathChanged)
    /*!
        \property QXmppLogger::loggingType

        The handler for logging messages
    */
    Q_PROPERTY(LoggingType loggingType READ loggingType WRITE setLoggingType NOTIFY loggingTypeChanged)
    /*!
        \property QXmppLogger::messageTypes

        The types of messages to log
    */
    Q_PROPERTY(MessageTypes messageTypes READ messageTypes WRITE setMessageTypes NOTIFY messageTypesChanged)
    /*!
        \property QXmppLogger::prettyXml

        Whether to pretty-print Sent/Received XML stanzas with indentation
    */
    Q_PROPERTY(bool prettyXml READ prettyXml WRITE setPrettyXml NOTIFY prettyXmlChanged)
    /*!
        \property QXmppLogger::colorMode

        How to decide whether to use ANSI color escapes for pretty XML
    */
    Q_PROPERTY(ColorMode colorMode READ colorMode WRITE setColorMode NOTIFY colorModeChanged)

public:
    /*!
        This enum describes how log message are handled.

        \value NoLogging Log messages are discarded.
        \value FileLogging Log messages are written to a file.
        \value StdoutLogging Log messages are written to the standard output.
        \value SignalLogging Log messages are emitted as a signal.
    */
    enum LoggingType {
        NoLogging = 0,
        FileLogging = 1,
        StdoutLogging = 2,
        SignalLogging = 4
    };
    Q_ENUM(LoggingType)

    /*!
        This enum describes a type of log message.

        \value NoMessage No message type.
        \value DebugMessage Debugging message.
        \value InformationMessage Informational message.
        \value WarningMessage Warning message.
        \value ReceivedMessage Message received from server.
        \value SentMessage Message sent to server.
        \value AnyMessage Any message type.
    */
    enum MessageType {
        NoMessage = 0,
        DebugMessage = 1,
        InformationMessage = 2,
        WarningMessage = 4,
        ReceivedMessage = 8,
        SentMessage = 16,
        AnyMessage = 31
    };
    Q_DECLARE_FLAGS(MessageTypes, MessageType)

    /*!
        Controls ANSI color output for pretty-printed XML.
        \value ColorOff Never emit color escapes
        \value ColorOn Always emit color escapes
        \value ColorAuto Emit colors when loggingType is StdoutLogging and stdout is a TTY
    */
    enum ColorMode {
        ColorOff,
        ColorOn,
        ColorAuto,
    };
    Q_ENUM(ColorMode)

    QXmppLogger(QObject *parent = nullptr);
    ~QXmppLogger() override;

    static QXmppLogger *getLogger();

    // documentation needs to be here, see https://stackoverflow.com/questions/49192523/
    /*! Returns the handler for logging messages. */
    QXmppLogger::LoggingType loggingType();
    void setLoggingType(QXmppLogger::LoggingType type);
    Q_SIGNAL void loggingTypeChanged();

    // documentation needs to be here, see https://stackoverflow.com/questions/49192523/
    /*!
        Returns the path to which logging messages should be written.

        \sa loggingType()
    */
    QString logFilePath();
    void setLogFilePath(const QString &path);
    Q_SIGNAL void logFilePathChanged();

    // documentation needs to be here, see https://stackoverflow.com/questions/49192523/
    /*! Returns the types of messages to log. */
    QXmppLogger::MessageTypes messageTypes();
    void setMessageTypes(QXmppLogger::MessageTypes types);
    Q_SIGNAL void messageTypesChanged();

    // documentation needs to be here, see https://stackoverflow.com/questions/49192523/
    /*!
        Returns whether Sent/Received XML stanzas should be pretty-printed.

        \since QXmpp 1.16
    */
    bool prettyXml() const;
    void setPrettyXml(bool enable);
    Q_SIGNAL void prettyXmlChanged();

    // documentation needs to be here, see https://stackoverflow.com/questions/49192523/
    /*!
        Returns the current ANSI color mode for pretty-printed XML.

        \since QXmpp 1.16
    */
    QXmppLogger::ColorMode colorMode() const;
    void setColorMode(QXmppLogger::ColorMode mode);
    Q_SIGNAL void colorModeChanged();

    void enablePrettyXml(bool enable = true);

    Q_SLOT virtual void setGauge(const QString &gauge, double value);
    Q_SLOT virtual void updateCounter(const QString &counter, qint64 amount);

    Q_SLOT void log(QXmppLogger::MessageType type, const QString &text);
    Q_SLOT void reopen();

    /*!
        This signal is emitted whenever a log message is received.

        \a type and \a text.
    */
    Q_SIGNAL void message(QXmppLogger::MessageType type, const QString &text);

private:
    static QXmppLogger *m_logger;
    const std::unique_ptr<QXmppLoggerPrivate> d;
};

/*!
    \inmodule QXmpp

    \brief The QXmppLoggable class represents a source of logging messages.

    \ingroup Core
*/
class QXMPP_EXPORT QXmppLoggable : public QObject
{
    Q_OBJECT

public:
    QXmppLoggable(QObject *parent = nullptr);

protected:
    void childEvent(QChildEvent *event) override;

    /*! Logs a debugging \a message. */
    void debug(const QString &message)
    {
        Q_EMIT logMessage(QXmppLogger::DebugMessage, qxmpp_loggable_trace(message));
    }

    /*! Logs an informational \a message. */
    void info(const QString &message)
    {
        Q_EMIT logMessage(QXmppLogger::InformationMessage, qxmpp_loggable_trace(message));
    }

    /*! Logs a warning \a message. */
    void warning(const QString &message)
    {
        Q_EMIT logMessage(QXmppLogger::WarningMessage, qxmpp_loggable_trace(message));
    }

    /*!
        Logs a received packet.

        \a message.
    */
    void logReceived(const QString &message)
    {
        Q_EMIT logMessage(QXmppLogger::ReceivedMessage, qxmpp_loggable_trace(message));
    }

    /*!
        Logs a sent packet.

        \a message.
    */
    void logSent(const QString &message)
    {
        Q_EMIT logMessage(QXmppLogger::SentMessage, qxmpp_loggable_trace(message));
    }

public:
    /*! Sets the given \a gauge to \a value. */
    Q_SIGNAL void setGauge(const QString &gauge, double value);

    /*!
        This signal is emitted to send logging messages.

        \a msg and \a type.
    */
    Q_SIGNAL void logMessage(QXmppLogger::MessageType type, const QString &msg);

    /*! Updates the given \a counter by \a amount. */
    Q_SIGNAL void updateCounter(const QString &counter, qint64 amount = 1);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QXmppLogger::MessageTypes)
#endif  // QXMPPLOGGER_H

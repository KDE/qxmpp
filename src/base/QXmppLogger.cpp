// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppLogger.h"

#include "QXmppXmlFormatter.h"

#include "StringLiterals.h"

#include <iostream>

#include <QChildEvent>
#include <QDateTime>
#include <QFile>
#include <QMetaType>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <io.h>
#define qxmpp_isatty_stdout() (::_isatty(::_fileno(stdout)) != 0)
#else
#include <unistd.h>
#define qxmpp_isatty_stdout() (::isatty(STDOUT_FILENO) != 0)
#endif

QXmppLogger *QXmppLogger::m_logger = nullptr;

static QStringView typeName(QXmppLogger::MessageType type)
{
    switch (type) {
    case QXmppLogger::DebugMessage:
        return u"DEBUG";
    case QXmppLogger::InformationMessage:
        return u"INFO";
    case QXmppLogger::WarningMessage:
        return u"WARNING";
    case QXmppLogger::ReceivedMessage:
        return u"RECEIVED";
    case QXmppLogger::SentMessage:
        return u"SENT";
    default:
        return {};
    }
}

static QStringView typeColor(QXmppLogger::MessageType type)
{
    switch (type) {
    case QXmppLogger::DebugMessage:
        return u"\x1b[90m";  // bright black (dim)
    case QXmppLogger::InformationMessage:
        return u"\x1b[32m";  // green
    case QXmppLogger::WarningMessage:
        return u"\x1b[33m";  // yellow
    case QXmppLogger::ReceivedMessage:
        return u"\x1b[94m";  // bright blue
    case QXmppLogger::SentMessage:
        return u"\x1b[95m";  // bright magenta
    default:
        return {};
    }
}

static QString formatted(QXmppLogger::MessageType type, const QString &text, bool colorize)
{
    QString date = QDateTime::currentDateTime().toString(u"yyyy-MM-dd HH:mm:ss"_s);
    QString name = typeName(type).toString();
    QString typeStr;
    if (colorize) {
        date = u"\x1b[90m"_s + date + u"\x1b[0m"_s;  // bright black / grey
        // Content-sized chip; alignment padding lives *outside* the colored block
        // so the right edge of every chip falls at the same column.
        QString inner = u' ' + name + u' ';
        QString outerLeftPad(10 - inner.size(), u' ');
        typeStr = outerLeftPad + u"\x1b[7m"_s + typeColor(type).toString() + inner + u"\x1b[0m"_s;
    } else {
        typeStr = name.rightJustified(8);
    }
    // Direction marker on the header line: <<< outgoing, >>> incoming.
    QString arrows;
    if (type == QXmppLogger::SentMessage || type == QXmppLogger::ReceivedMessage) {
        arrows = (type == QXmppLogger::SentMessage)
            ? u">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"_s
            : u"<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"_s;
        if (colorize) {
            arrows = typeColor(type).toString() + arrows + u"\x1b[0m"_s;
        }
        arrows = u' ' + arrows;
    }
    // XML stanzas start on their own line so the structure is readable;
    // text-only messages stay inline.
    QChar separator = (type == QXmppLogger::SentMessage || type == QXmppLogger::ReceivedMessage)
        ? u'\n'
        : u' ';
    return date + u' ' + typeStr + arrows + separator + text;
}

static void relaySignals(QXmppLoggable *from, QXmppLoggable *to)
{
    QObject::connect(from, &QXmppLoggable::logMessage,
                     to, &QXmppLoggable::logMessage);
    QObject::connect(from, &QXmppLoggable::setGauge,
                     to, &QXmppLoggable::setGauge);
    QObject::connect(from, &QXmppLoggable::updateCounter,
                     to, &QXmppLoggable::updateCounter);
}

/// Constructs a new QXmppLoggable.
///
/// \param parent

QXmppLoggable::QXmppLoggable(QObject *parent)
    : QObject(parent)
{
    auto *logParent = qobject_cast<QXmppLoggable *>(parent);
    if (logParent) {
        relaySignals(this, logParent);
    }
}

/// \cond
void QXmppLoggable::childEvent(QChildEvent *event)
{
    auto *child = qobject_cast<QXmppLoggable *>(event->child());
    if (!child) {
        return;
    }

    if (event->added()) {
        relaySignals(child, this);
    } else if (event->removed()) {
        disconnect(child, &QXmppLoggable::logMessage,
                   this, &QXmppLoggable::logMessage);
        disconnect(child, &QXmppLoggable::setGauge,
                   this, &QXmppLoggable::setGauge);
        disconnect(child, &QXmppLoggable::updateCounter,
                   this, &QXmppLoggable::updateCounter);
    }
}
/// \endcond

class QXmppLoggerPrivate
{
public:
    QXmppLoggerPrivate();

    QXmppLogger::LoggingType loggingType;
    QFile *logFile;
    QString logFilePath;
    QXmppLogger::MessageTypes messageTypes;
    bool prettyXml = false;
    QXmppLogger::ColorMode colorMode = QXmppLogger::ColorAuto;
};

QXmppLoggerPrivate::QXmppLoggerPrivate()
    : loggingType(QXmppLogger::NoLogging),
      logFile(nullptr),
      logFilePath(u"QXmppClientLog.log"_s),
      messageTypes(QXmppLogger::AnyMessage)
{
}

///
/// Constructs a new QXmppLogger.
///
/// \param parent
///
QXmppLogger::QXmppLogger(QObject *parent)
    : QObject(parent),
      d(std::make_unique<QXmppLoggerPrivate>())
{
    // make it possible to pass QXmppLogger::MessageType between threads
    qRegisterMetaType<QXmppLogger::MessageType>("QXmppLogger::MessageType");
}

QXmppLogger::~QXmppLogger() = default;

///
/// Returns the default logger.
///
QXmppLogger *QXmppLogger::getLogger()
{
    if (!m_logger) {
        m_logger = new QXmppLogger();
    }

    return m_logger;
}

QXmppLogger::LoggingType QXmppLogger::loggingType()
{
    return d->loggingType;
}

/// Sets the handler for logging messages.
void QXmppLogger::setLoggingType(QXmppLogger::LoggingType type)
{
    if (d->loggingType != type) {
        d->loggingType = type;
        reopen();
        Q_EMIT loggingTypeChanged();
    }
}

///
/// \fn QXmppLogger::loggingTypeChanged()
///
/// Emitted when the logging type has been changed.
///
/// \since QXmpp 1.7
///

QXmppLogger::MessageTypes QXmppLogger::messageTypes()
{
    return d->messageTypes;
}

/// Sets the types of messages to log.
void QXmppLogger::setMessageTypes(QXmppLogger::MessageTypes types)
{
    if (d->messageTypes != types) {
        d->messageTypes = types;
        Q_EMIT messageTypesChanged();
    }
}

///
/// \fn QXmppLogger::messageTypesChanged()
///
/// Emitted when the message types have been changed.
///
/// \since QXmpp 1.7
///

/// Add a logging message.
void QXmppLogger::log(QXmppLogger::MessageType type, const QString &text)
{
    // filter messages
    if (!d->messageTypes.testFlag(type)) {
        return;
    }

    bool colorize = false;
    if (d->prettyXml) {
        switch (d->colorMode) {
        case ColorOn:
            colorize = true;
            break;
        case ColorOff:
            colorize = false;
            break;
        case ColorAuto:
            colorize = (d->loggingType == StdoutLogging) && qxmpp_isatty_stdout();
            break;
        }
    }

    QString payload = text;
    if (d->prettyXml && (type == SentMessage || type == ReceivedMessage) && !text.isEmpty()) {
        payload = QXmpp::formatXmlForDebug(text, true, 2, colorize);
    }

    switch (d->loggingType) {
    case QXmppLogger::FileLogging:
        if (!d->logFile) {
            d->logFile = new QFile(d->logFilePath);
            d->logFile->open(QIODevice::WriteOnly | QIODevice::Append);
        }
        // Never write ANSI escapes to log files.
        QTextStream(d->logFile) << formatted(type, payload, false) << "\n";
        break;
    case QXmppLogger::StdoutLogging:
        std::cout << qPrintable(formatted(type, payload, colorize)) << std::endl;
        break;
    case QXmppLogger::SignalLogging:
        Q_EMIT message(type, payload);
        break;
    default:
        break;
    }
}

///
/// Returns whether Sent/Received XML stanzas are pretty-printed.
///
/// \since QXmpp 1.16
///
bool QXmppLogger::prettyXml() const
{
    return d->prettyXml;
}

///
/// Sets whether Sent/Received XML stanzas should be pretty-printed.
///
/// \since QXmpp 1.16
///
void QXmppLogger::setPrettyXml(bool enable)
{
    if (d->prettyXml != enable) {
        d->prettyXml = enable;
        Q_EMIT prettyXmlChanged();
    }
}

///
/// Returns the current ANSI color mode used for pretty-printed XML.
///
/// \since QXmpp 1.16
///
QXmppLogger::ColorMode QXmppLogger::colorMode() const
{
    return d->colorMode;
}

///
/// Sets the ANSI color mode for pretty-printed XML. See ColorMode.
///
/// \since QXmpp 1.16
///
void QXmppLogger::setColorMode(QXmppLogger::ColorMode mode)
{
    if (d->colorMode != mode) {
        d->colorMode = mode;
        Q_EMIT colorModeChanged();
    }
}

///
/// Enables pretty-printing of Sent/Received XML stanzas and sets ColorAuto so
/// ANSI escapes appear on a TTY.
///
/// Equivalent to calling setPrettyXml(enable) and, if enabling,
/// setColorMode(ColorAuto).
///
/// \since QXmpp 1.16
///
void QXmppLogger::enablePrettyXml(bool enable)
{
    setPrettyXml(enable);
    if (enable) {
        setColorMode(ColorAuto);
    }
}

///
/// Sets the given \a gauge to \a value.
///
/// NOTE: the base implementation does nothing.
///
void QXmppLogger::setGauge(const QString &gauge, double value)
{
    Q_UNUSED(gauge)
    Q_UNUSED(value)
}

///
/// Updates the given \a counter by \a amount.
///
/// NOTE: the base implementation does nothing.
///
void QXmppLogger::updateCounter(const QString &counter, qint64 amount)
{
    Q_UNUSED(counter)
    Q_UNUSED(amount)
}

QString QXmppLogger::logFilePath()
{
    return d->logFilePath;
}

///
/// Sets the path to which logging messages should be written.
///
/// \param path
///
/// \sa setLoggingType()
///
void QXmppLogger::setLogFilePath(const QString &path)
{
    if (d->logFilePath != path) {
        d->logFilePath = path;
        reopen();
        Q_EMIT logFilePathChanged();
    }
}

///
/// \fn QXmppLogger::logFilePathChanged()
///
/// Emitted when the log file path has been changed.
///
/// \since QXmpp 1.7
///

/// If logging to a file, causes the file to be re-opened.
void QXmppLogger::reopen()
{
    if (d->logFile) {
        delete d->logFile;
        d->logFile = nullptr;
    }
}

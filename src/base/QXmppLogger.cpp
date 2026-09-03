// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppLogger.h"

#include "QXmppConstants_p.h"
#include "QXmppXmlFormatter_p.h"

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

// Detect a bare XEP-0198 ack request/answer (<r/> or <a .../>).
// Allocation-free: QStringView slicing/contains do not allocate, and the single-char
// guards reject normal stanzas before the namespace scan runs.
static bool isStreamManagementAck(QStringView text)
{
    // skip leading whitespace without allocating
    qsizetype i = 0;
    while (i < text.size() && text.at(i).isSpace()) {
        ++i;
    }
    auto s = text.sliced(i);
    // element name is a single 'r' or 'a' followed by whitespace, '/', or '>'
    if (s.size() < 3 || s.at(0) != u'<') {
        return false;
    }
    QChar name = s.at(1);
    if (name != u'r' && name != u'a') {
        return false;
    }
    QChar after = s.at(2);
    if (!after.isSpace() && after != u'/' && after != u'>') {
        return false;
    }
    // only reached by genuine <r .../> / <a .../>; confirm the SM namespace
    return s.contains(QXmpp::Private::ns_stream_management);
}

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

/*!
    Constructs a new QXmppLoggable with \a parent.
*/

QXmppLoggable::QXmppLoggable(QObject *parent)
    : QObject(parent)
{
    auto *logParent = qobject_cast<QXmppLoggable *>(parent);
    if (logParent) {
        relaySignals(this, logParent);
    }
}

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

class QXmppLoggerPrivate
{
public:
    QXmppLoggerPrivate();

    QXmppLogger::LoggingType loggingType;
    QFile *logFile;
    QString logFilePath;
    QXmppLogger::MessageTypes messageTypes;
    QXmppLogger::OutputMode outputMode = QXmppLogger::OutputMode::Auto;
    QXmppLogger::ColorMode colorMode = QXmppLogger::ColorAuto;
    bool filterStreamManagementAcks = true;
    std::optional<qsizetype> elideXmlTextAbove = QXmppLogger::DefaultElideXmlTextAbove;
    std::optional<qsizetype> elideLogMessagesAbove = QXmppLogger::DefaultElideLogMessagesAbove;

    bool prettyOutput() const
    {
        switch (outputMode) {
        case QXmppLogger::OutputMode::Auto:
            return loggingType == QXmppLogger::StdoutLogging;
        case QXmppLogger::OutputMode::Raw:
            return false;
        case QXmppLogger::OutputMode::Pretty:
            return true;
        }
        return false;
    }
};

QXmppLoggerPrivate::QXmppLoggerPrivate()
    : loggingType(QXmppLogger::NoLogging),
      logFile(nullptr),
      logFilePath(u"QXmppClientLog.log"_s),
      messageTypes(QXmppLogger::AnyMessage)
{
}

/*!
    Constructs a new QXmppLogger with \a parent.
*/
QXmppLogger::QXmppLogger(QObject *parent)
    : QObject(parent),
      d(std::make_unique<QXmppLoggerPrivate>())
{
    // make it possible to pass QXmppLogger::MessageType between threads
    qRegisterMetaType<QXmppLogger::MessageType>("QXmppLogger::MessageType");
}

QXmppLogger::~QXmppLogger() = default;

/*! Returns the default logger. */
QXmppLogger *QXmppLogger::getLogger()
{
    if (!m_logger) {
        m_logger = new QXmppLogger();
    }

    return m_logger;
}

/*! Returns the handler for logging messages. */
QXmppLogger::LoggingType QXmppLogger::loggingType()
{
    return d->loggingType;
}

/*!
    Sets the handler for logging messages.

    \a type.
*/
void QXmppLogger::setLoggingType(QXmppLogger::LoggingType type)
{
    if (d->loggingType != type) {
        d->loggingType = type;
        reopen();
        Q_EMIT loggingTypeChanged();
    }
}

/*!
    \fn QXmppLogger::loggingTypeChanged()

    Emitted when the logging type has been changed.

    \since QXmpp 1.7
*/

/*! Returns the types of messages to log. */
QXmppLogger::MessageTypes QXmppLogger::messageTypes()
{
    return d->messageTypes;
}

/*! Sets the \a types of messages to log. */
void QXmppLogger::setMessageTypes(QXmppLogger::MessageTypes types)
{
    if (d->messageTypes != types) {
        d->messageTypes = types;
        Q_EMIT messageTypesChanged();
    }
}

/*!
    \fn QXmppLogger::messageTypesChanged()

    Emitted when the message types have been changed.

    \since QXmpp 1.7
*/

/*!
    Add a logging message.

    \a type and \a text.
*/
void QXmppLogger::log(QXmppLogger::MessageType type, const QString &text)
{
    // filter messages
    if (!d->messageTypes.testFlag(type)) {
        return;
    }

    // filter out XEP-0198 Stream Management ack stanzas (very frequent, noisy)
    if (d->filterStreamManagementAcks &&
        (type == SentMessage || type == ReceivedMessage) &&
        isStreamManagementAck(text)) {
        return;
    }

    const bool pretty = d->prettyOutput();

    bool colorize = false;
    if (pretty) {
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
    if (pretty && (type == SentMessage || type == ReceivedMessage) && !text.isEmpty()) {
        payload = QXmpp::Private::formatXmlForDebug(text, { true, 2, colorize, d->elideXmlTextAbove });
        if (d->elideLogMessagesAbove) {
            payload = QXmpp::Private::elideMiddle(payload, *d->elideLogMessagesAbove, colorize, true);
        }
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

/*!
    Returns how logged Sent/Received XML is formatted. See OutputMode.

    \since QXmpp 1.17
*/
QXmppLogger::OutputMode QXmppLogger::outputMode() const
{
    return d->outputMode;
}

/*!
    Sets how logged Sent/Received XML is formatted (\a mode). See OutputMode.

    Formatting for humans is only enabled by default for StdoutLogging. Set OutputMode::Pretty
    to format the output of the other logging types too, e.g. when logging via signals and
    printing the messages yourself.

    \since QXmpp 1.17
*/
void QXmppLogger::setOutputMode(OutputMode mode)
{
    if (d->outputMode != mode) {
        const auto pretty = prettyXml();
        d->outputMode = mode;
        Q_EMIT outputModeChanged();
        if (prettyXml() != pretty) {
            Q_EMIT prettyXmlChanged();
        }
    }
}

/*!
    Returns whether Sent/Received XML stanzas should be pretty-printed.

    \deprecated This method is deprecated since QXmpp 1.17. Use outputMode() instead.

    \since QXmpp 1.16
*/
bool QXmppLogger::prettyXml() const
{
    return d->outputMode == OutputMode::Pretty;
}

/*!
    Sets whether Sent/Received XML stanzas should be pretty-printed (\a enable).

    \deprecated This method is deprecated since QXmpp 1.17. Use setOutputMode() instead.

    \since QXmpp 1.16
*/
void QXmppLogger::setPrettyXml(bool enable)
{
    setOutputMode(enable ? OutputMode::Pretty : OutputMode::Raw);
}

/*!
    Returns the current ANSI color mode for pretty-printed XML.

    \since QXmpp 1.16
*/
QXmppLogger::ColorMode QXmppLogger::colorMode() const
{
    return d->colorMode;
}

/*!
    Sets the ANSI color \a mode for pretty-printed XML. See ColorMode.

    \since QXmpp 1.16
*/
void QXmppLogger::setColorMode(QXmppLogger::ColorMode mode)
{
    if (d->colorMode != mode) {
        d->colorMode = mode;
        Q_EMIT colorModeChanged();
    }
}

/*!
    Returns whether \xep{0198}{Stream Management} ack stanzas (\c{<r/>} and \c{<a/>}) are
    filtered out of Sent/Received logging.

    These tiny stanzas are exchanged very frequently and otherwise flood the log.
    Enabled by default.

    \since QXmpp 1.17
*/
bool QXmppLogger::filterStreamManagementAcks() const
{
    return d->filterStreamManagementAcks;
}

/*!
    Sets whether \xep{0198}{Stream Management} ack stanzas (\c{<r/>} and \c{<a/>}) are
    filtered out of Sent/Received logging (\a enable).

    \since QXmpp 1.17
*/
void QXmppLogger::setFilterStreamManagementAcks(bool enable)
{
    d->filterStreamManagementAcks = enable;
}

/*!
    Returns the maximum length of an XML text node in logged stanzas, or std::nullopt if
    text nodes are never elided.

    \sa setElideXmlTextAbove()
    \since QXmpp 1.17
*/
std::optional<qsizetype> QXmppLogger::elideXmlTextAbove() const
{
    return d->elideXmlTextAbove;
}

/*!
    Sets the maximum \a length of an XML text node in logged stanzas.

    Text nodes longer than \a length are elided in the middle, so the beginning and the end of
    the content stay visible while the XML structure around them is preserved. This is useful
    for large base64 payloads (avatars, file previews, encrypted content).

    Only applies to Sent/Received messages. Pass std::nullopt to never elide text nodes. Only
    has an effect in OutputMode::Pretty.

    \sa setElideLogMessagesAbove(), enableEliding()
    \since QXmpp 1.17
*/
void QXmppLogger::setElideXmlTextAbove(std::optional<qsizetype> length)
{
    d->elideXmlTextAbove = length;
}

/*!
    Returns the maximum length of a logged message, or std::nullopt if messages are never
    elided.

    \sa setElideLogMessagesAbove()
    \since QXmpp 1.17
*/
std::optional<qsizetype> QXmppLogger::elideLogMessagesAbove() const
{
    return d->elideLogMessagesAbove;
}

/*!
    Sets the maximum \a length of a logged message.

    Messages longer than \a length are elided in the middle, so the beginning and the end stay
    visible. Unlike setElideXmlTextAbove() this also shortens stanzas that are large because of
    the sheer number of elements they contain, as well as payloads that are not valid XML.

    Only applies to Sent/Received messages. Pass std::nullopt to never elide messages. Only has
    an effect in OutputMode::Pretty.

    \sa setElideXmlTextAbove(), enableEliding()
    \since QXmpp 1.17
*/
void QXmppLogger::setElideLogMessagesAbove(std::optional<qsizetype> length)
{
    d->elideLogMessagesAbove = length;
}

/*!
    Enables (\a enable) eliding of overly long Sent/Received messages using default limits.

    Equivalent to calling setElideXmlTextAbove(DefaultElideXmlTextAbove) and
    setElideLogMessagesAbove(DefaultElideLogMessagesAbove), or, if disabling, setting both to
    std::nullopt. Use those setters directly to pick your own limits.

    Eliding is enabled by default, so this is mainly useful as enableEliding(false) to log full
    stanzas in OutputMode::Pretty. It has no effect in OutputMode::Raw.

    \sa setElideXmlTextAbove(), setElideLogMessagesAbove()
    \since QXmpp 1.17
*/
void QXmppLogger::enableEliding(bool enable)
{
    if (enable) {
        setElideXmlTextAbove(DefaultElideXmlTextAbove);
        setElideLogMessagesAbove(DefaultElideLogMessagesAbove);
    } else {
        setElideXmlTextAbove(std::nullopt);
        setElideLogMessagesAbove(std::nullopt);
    }
}

/*!
    Enables (\a enable) pretty-printing of Sent/Received XML stanzas, sets ColorAuto so ANSI
    escapes appear on a TTY and elides overly long messages.

    \deprecated This method is deprecated since QXmpp 1.17. Use
    setOutputMode(OutputMode::Pretty) instead, which does the same.

    \since QXmpp 1.16
*/
void QXmppLogger::enablePrettyXml(bool enable)
{
    setOutputMode(enable ? OutputMode::Pretty : OutputMode::Raw);
    if (enable) {
        setColorMode(ColorAuto);
        enableEliding();
    }
}

/*!
    Sets the given \a gauge to \a value.

    NOTE: the base implementation does nothing.
*/
void QXmppLogger::setGauge(const QString &gauge, double value)
{
    Q_UNUSED(gauge)
    Q_UNUSED(value)
}

/*!
    Updates the given \a counter by \a amount.

    NOTE: the base implementation does nothing.
*/
void QXmppLogger::updateCounter(const QString &counter, qint64 amount)
{
    Q_UNUSED(counter)
    Q_UNUSED(amount)
}

/*!
    Returns the path to which logging messages should be written.

    \sa loggingType()
*/
QString QXmppLogger::logFilePath()
{
    return d->logFilePath;
}

/*!
    Sets the path to which logging messages should be written to \a path.

    \sa setLoggingType()
*/
void QXmppLogger::setLogFilePath(const QString &path)
{
    if (d->logFilePath != path) {
        d->logFilePath = path;
        reopen();
        Q_EMIT logFilePathChanged();
    }
}

/*!
    \fn QXmppLogger::logFilePathChanged()

    Emitted when the log file path has been changed.

    \since QXmpp 1.7
*/

/*! If logging to a file, causes the file to be re-opened. */
void QXmppLogger::reopen()
{
    if (d->logFile) {
        delete d->logFile;
        d->logFile = nullptr;
    }
}

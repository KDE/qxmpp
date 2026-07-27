// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2012 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2020 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef TESTS_UTIL_H
#define TESTS_UTIL_H

#include "QXmppError.h"
#include "QXmppTask.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <algorithm>
#include <any>
#include <memory>
#include <variant>
#include <vector>

// Note: do not include <QtTest> here. That umbrella header pulls in all of
// <QtCore/QtCore> and is included by every test, which costs noticeably more
// compile time than naming the few headers actually needed.
#include <QBuffer>
#include <QDomDocument>
#include <QFutureWatcher>
#include <QMetaMethod>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QXmlStreamWriter>

struct QXmppError;

// QVERIFY2 with empty return value (return {};)
#define QVERIFY_RV(statement, description)                                       \
    if (!QTest::qVerify(statement, #statement, description, __FILE__, __LINE__)) \
        return {};

#define VERIFY2(statement, description)                                                                           \
    if (!QTest::qVerify(bool(statement), #statement, static_cast<const char *>(description), __FILE__, __LINE__)) \
        throw std::runtime_error(description);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto TimeZoneUTC = QTimeZone::Initialization::UTC;
#else
constexpr auto TimeZoneUTC = Qt::UTC;
#endif

template<typename String>
inline std::variant<QDomDocument, QString> internalParseDomDocument(const String &xml, bool namespaceProcessing)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QDomDocument doc;
    auto options = namespaceProcessing ? QDomDocument::ParseOption::UseNamespaceProcessing : QDomDocument::ParseOption::Default;
    if (auto result = doc.setContent(xml, options); !result) {
        return result.errorMessage;
    }
    return doc;
#else
    QDomDocument doc;
    QString errorMessage;
    if (!doc.setContent(xml, namespaceProcessing, &errorMessage)) {
        return errorMessage;
    }
    return doc;
#endif
}

template<typename String>
inline QDomDocument xmlToDomDoc(const String &xml, bool namespaceProcessing)
{
    std::variant<QDomDocument, QString> result;

    if constexpr (std::is_same_v<String, QString> || std::is_same_v<String, QByteArray>) {
        result = internalParseDomDocument(xml, namespaceProcessing);
    } else {
        result = internalParseDomDocument(QString(xml), namespaceProcessing);
    }

    // error
    if (std::holds_alternative<QString>(result)) {
        qDebug() << "Parsing error:";
        qDebug().noquote() << xml;
        qDebug().noquote() << "Error:" << std::get<QString>(result);
        QTest::qFail("Invalid XML", __FILE__, __LINE__);
        return {};
    }
    return std::get<QDomDocument>(result);
}

template<typename String>
inline QDomElement xmlToDom(const String &xml)
{
    return xmlToDomDoc(xml, true).documentElement();
}

template<typename String>
inline QByteArray xmlToFormattedByteArray(const String &xml)
{
    return xmlToDomDoc(xml, false).toByteArray(4);
}

template<typename String>
QString rewriteXml(const String &inputXml)
{
    QString outputXml;
    QXmlStreamReader reader(inputXml);
    QXmlStreamWriter writer(&outputXml);
    while (reader.readNext() != QXmlStreamReader::EndDocument) {
        if (reader.hasError()) {
            qDebug() << "Parsing error:";
            qDebug().noquote() << inputXml;
            qDebug().noquote() << reader.error() << reader.errorString();
            throw std::exception();
        }

        // do not generate '<?xml version="1.0"?>'
        if (reader.tokenType() == QXmlStreamReader::StartDocument) {
            continue;
        }
        writer.writeCurrentToken(reader);
    }
    return outputXml;
}

template<typename String>
std::tuple<QString, QString> rewriteXmlWithoutStanzaId(const String &inputXml)
{
    QString outputXml;
    QString id;
    QXmlStreamReader reader(inputXml);
    QXmlStreamWriter writer(&outputXml);

    // find start
    reader.readNextStartElement();
    Q_ASSERT(reader.isStartElement());

    // write element, but without 'id' attribute
    writer.writeStartElement(reader.name().toString());
    const auto attributes = reader.attributes();
    for (const auto &attribute : attributes) {
        if (attribute.name() == u"id") {
            id = attribute.value().toString();
        } else {
            writer.writeAttribute(attribute);
        }
    }

    // copy rest of the xml
    while (reader.readNext() != QXmlStreamReader::EndDocument) {
        if (reader.hasError()) {
            qDebug() << "Parsing error:";
            qDebug().noquote() << inputXml;
            qDebug().noquote() << reader.error() << reader.errorString();
            throw std::exception();
        }

        // do not generate '<?xml version="1.0"?>'
        if (reader.tokenType() == QXmlStreamReader::StartDocument) {
            continue;
        }
        writer.writeCurrentToken(reader);
    }
    return { outputXml, id };
}

template<typename T, typename... Args>
static QByteArray packetToXml(const T &packet, Args &&...args)
{
    using namespace QXmpp::Private;

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter writer(&buffer);
    XmlWriter xmlWriter(&writer);
    packet.toXml(xmlWriter, std::forward<Args>(args)...);
    auto data = buffer.data();
    data.replace(u'\'', "&apos;");
    return data;
}

template<class T>
static void parsePacket(T &packet, const QByteArray &xml)
{
    // qDebug() << "parsing" << xml;
    packet.parse(xmlToDom(xml));
}

template<typename T>
static T parseInto(const QDomElement &el)
{
    T packet;
    packet.parse(el);
    return packet;
}

template<class T>
static void serializePacket(T &packet, const QByteArray &xml)
{
    auto processedXml = xml;
    processedXml.replace(u'\'', u'"');

    // Remove newlines and needless spaces from raw strings.
    processedXml = processedXml.simplified();
    processedXml.replace("> <", "><");

    const auto data = packetToXml(packet);
    if (data != processedXml) {
        qDebug() << "expect " << processedXml;
        qDebug() << "writing" << data;
    }
    QCOMPARE(data, processedXml);
}

template<class T>
QDomElement writePacketToDom(T packet)
{
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter qtWriter(&buffer);
    QXmpp::Private::XmlWriter writer(&qtWriter);
    packet.toXml(writer);

    QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    doc.setContent(buffer.data(), QDomDocument::ParseOption::UseNamespaceProcessing);
#else
    doc.setContent(buffer.data(), true);
#endif

    return doc.documentElement();
}

template<typename T, typename Variant>
T expectVariant(Variant var)
{
    using namespace std::string_literals;
    std::string message =
        "Variant ("s + typeid(Variant).name() +
        ") contains wrong type ("s + std::to_string(var.index()) +
        "); expected '"s + typeid(T).name() + "'."s;
    VERIFY2(std::holds_alternative<T>(var), message.c_str());
    return std::get<T>(std::move(var));
}

template<typename T, typename Input>
T expectFutureVariant(const QFuture<Input> &future)
{
    VERIFY2(future.isFinished(), "Future is still running!");
    return expectVariant<T>(future.result());
}

template<typename T, typename Input>
T expectFutureVariant(QXmppTask<Input> &task)
{
    VERIFY2(task.isFinished(), "Task is still running!");
    return expectVariant<T>(task.result());
}

template<typename T>
const T &unwrap(const std::optional<T> &v)
{
    VERIFY2(v.has_value(), "Expected value, got empty optional");
    return *v;
}

template<typename T>
T unwrap(std::optional<T> &&v)
{
    VERIFY2(v.has_value(), "Expected value, got empty optional");
    return *v;
}

template<typename T>
T unwrap(std::variant<T, QXmppError> &&v)
{
    if (std::holds_alternative<QXmppError>(v)) {
        auto message = u"Expected value, got error: %1."_s.arg(std::get<QXmppError>(v).description);
        VERIFY2(v.index() == 1, message.toLocal8Bit().constData());
    }
    return std::get<T>(std::move(v));
}

template<typename T>
const T &unwrap(const std::variant<T, QXmppError> &v)
{
    if (std::holds_alternative<QXmppError>(v)) {
        auto message = u"Expected value, got error: %1."_s.arg(std::get<QXmppError>(v).description);
        VERIFY2(v.index() == 1, message.toLocal8Bit().constData());
    }
    return std::get<T>(v);
}

template<typename T>
const T &unwrap(const std::any &v)
{
    VERIFY2(v.has_value(), "Expected non-empty std::any");
    VERIFY2(v.type() == typeid(T), "Got std::any with wrong type");
    return std::any_cast<T>(v);
}

template<typename T>
T unwrap(std::any &&v)
{
    VERIFY2(v.has_value(), "Expected non-empty std::any");
    VERIFY2(v.type() == typeid(T), "Got std::any with wrong type");
    return std::any_cast<T>(std::move(v));
}

template<typename T>
T wait(const QFuture<T> &future)
{
    auto watcher = std::make_unique<QFutureWatcher<T>>();
    QSignalSpy spy(watcher.get(), &QFutureWatcherBase::finished);
    watcher->setFuture(future);
    [&]() { QVERIFY(spy.wait()); }();
    if constexpr (!std::is_same_v<T, void>) {
        return future.result();
    }
}

// Names of the test functions QTest::qExec() can select in the given class.
inline QStringList testFunctionNames(const QMetaObject &metaObject)
{
    QStringList names;
    for (int i = metaObject.methodOffset(); i < metaObject.methodCount(); ++i) {
        auto method = metaObject.method(i);
        if (method.methodType() != QMetaMethod::Slot || method.access() != QMetaMethod::Private ||
            method.parameterCount() != 0) {
            continue;
        }
        auto name = QString::fromUtf8(method.name());
        // data functions and the fixture slots cannot be selected on the command line
        if (name.endsWith(u"_data") || name == u"initTestCase" || name == u"cleanupTestCase" ||
            name == u"init" || name == u"cleanup") {
            continue;
        }
        names.append(name);
    }
    return names;
}

// Runs each of the given test classes in turn and returns a non-zero exit
// status if any of them failed.
//
// A test function name on the command line only exists in one of the classes,
// so the classes that do not have it are skipped and the names that select a
// function of another class are removed from their arguments. Without that,
// running a single function would make every other class in the binary fail
// with "Unknown test function".
template<typename... TestClasses>
int runTests(int argc, char *argv[])
{
    const QList<QStringList> testFunctions { testFunctionNames(TestClasses::staticMetaObject)... };

    QStringList allTestFunctions;
    for (const auto &names : testFunctions) {
        allTestFunctions += names;
    }

    // Arguments that select a test function, optionally with a ":datatag" suffix.
    // Options and their values never match a test function name and are passed through.
    QStringList selectors;
    for (int i = 1; i < argc; ++i) {
        auto arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith(u'-') && allTestFunctions.contains(arg.split(u':').first())) {
            selectors.append(arg);
        }
    }

    int status = 0;
    qsizetype index = 0;
    // each test object is destroyed before the next one is created
    ([&] {
        const auto &names = testFunctions[index++];

        auto selectsThisClass = [&](const QString &selector) {
            return names.contains(selector.split(u':').first());
        };
        if (!selectors.isEmpty() && std::none_of(selectors.cbegin(), selectors.cend(), selectsThisClass)) {
            return;
        }

        std::vector<char *> args { argv[0] };
        for (int i = 1; i < argc; ++i) {
            auto arg = QString::fromLocal8Bit(argv[i]);
            if (selectors.contains(arg) && !selectsThisClass(arg)) {
                continue;
            }
            args.push_back(argv[i]);
        }

        TestClasses testCase;
        status |= QTest::qExec(&testCase, int(args.size()), args.data());
    }(),
     ...);
    return status;
}

// main() for test files that contain more than one test class; QTEST_MAIN()
// only handles a single one. Pass the test classes in the order they should
// run, e.g.:
//
//     QXMPP_TEST_MAIN(tst_QXmppFoo, Bar::tst_QXmppBar)
#define QXMPP_TEST_MAIN(...)                      \
    int main(int argc, char *argv[])              \
    {                                             \
        QCoreApplication app(argc, argv);         \
        return runTests<__VA_ARGS__>(argc, argv); \
    }

#endif  // TESTS_UTIL_H

// SPDX-FileCopyrightText: 2023 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2024 Filipe Azevedo <pasnox@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppAccountMigrationManager.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppPromise.h"
#include "QXmppTask.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <array>

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

// Utilities for account data extensions

struct XmlElementId {
    QString tagName;
    QString xmlns;

    static XmlElementId fromDom(const QDomElement &el) { return { el.tagName(), el.namespaceURI() }; }

    bool operator==(const XmlElementId &other) const
    {
        return tagName == other.tagName && xmlns == other.xmlns;
    }

    bool operator<(const XmlElementId &other) const
    {
        const auto result = xmlns.compare(other.xmlns);

        if (result == 0) {
            return tagName.compare(other.tagName) < 0;
        }

        return result < 0;
    }
};

#ifndef QXMPP_DOC
template<>
struct std::hash<XmlElementId> {
    std::size_t operator()(const XmlElementId &m) const noexcept
    {
        std::size_t h1 = std::hash<QString> {}(m.tagName);
        std::size_t h2 = std::hash<QString> {}(m.xmlns);
        return h1 ^ (h2 << 1);
    }
};
#endif

using AnyParser = QXmppExportData::ExtensionParser<std::any>;
using AnySerializer = QXmppExportData::ExtensionSerializer<std::any>;
using Format = QXmppExportData::Format;

// Number of QXmppExportData::Format values; the registries below are indexed by format.
inline constexpr std::size_t FormatCount = 2;
inline std::size_t formatIndex(Format format) { return static_cast<std::size_t>(format); }

// Each registry is kept per output format. A C++ type may thus carry one entry for the
// QXmpp format and another for the XEP-0227 format (e.g. roster serializes as <roster/>
// vs <query xmlns='jabber:iq:roster'/>).
static std::unordered_map<std::type_index, XmlElementId> &accountDataMapping(Format format)
{
    thread_local static std::array<std::unordered_map<std::type_index, XmlElementId>, FormatCount> registry;
    return registry[formatIndex(format)];
}

static std::unordered_map<XmlElementId, AnyParser> &accountDataParsers(Format format)
{
    thread_local static std::array<std::unordered_map<XmlElementId, AnyParser>, FormatCount> registry;
    return registry[formatIndex(format)];
}

static std::unordered_map<std::type_index, AnySerializer> &accountDataSerializers(Format format)
{
    thread_local static std::array<std::unordered_map<std::type_index, AnySerializer>, FormatCount> registry;
    return registry[formatIndex(format)];
}

struct QXmppExportDataPrivate : QSharedData {
    QString accountJid;
    std::unordered_map<std::type_index, std::any> extensions;
};

QXmppExportData::QXmppExportData()
    : d(new QXmppExportDataPrivate())
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppExportData)

// Parses all registered child extension elements of `parent` using the parser table for
// `format`, adding the results to `extensions`. Already present types are kept untouched
// (so a value parsed from a preferred format is not overwritten). Returns an error if a
// registered parser fails.
static std::optional<QXmppError> parseExtensionChildren(const QDomElement &parent, Format format, std::unordered_map<std::type_index, std::any> &extensions)
{
    const auto &parsers = accountDataParsers(format);
    for (const auto &extension : iterChildElements(parent)) {
        const auto parser = parsers.find(XmlElementId::fromDom(extension));
        if (parser == parsers.end()) {
            continue;
        }

        auto result = parser->second(extension);
        if (hasError(result)) {
            return getError(std::move(result));
        }

        auto value = getValue(std::move(result));
        extensions.emplace(value.type(), std::move(value));
    }
    return {};
}

/*!
    Parses account data from \a el.

    Both the QXmpp format (\c {<account-data/>}) and \xep{0227, Portable Import/Export
    Format for XMPP-IM Servers} (\c {<server-data/>}) are accepted; the format is detected
    from the root element. When an \xep{0227} document carries the same data both as a
    native element and as an embedded QXmpp fallback, the native \xep{0227} element is
    preferred.

    \note \xep{0227} documents may contain multiple hosts and users, but as this is used for
    single-account client-side migration only the first host and user are read.
*/
std::variant<QXmppExportData, QXmppError> QXmppExportData::fromDom(const QDomElement &el)
{
    QXmppExportData data;

    // QXmpp's own format.
    if (el.tagName() == u"account-data" && el.namespaceURI() == ns_qxmpp_export) {
        data.setAccountJid(el.attribute(u"jid"_s));
        if (auto error = parseExtensionChildren(el, Format::QXmpp, data.d->extensions)) {
            return std::move(*error);
        }
        return data;
    }

    // XEP-0227: <server-data><host jid><user name>...</user></host></server-data>.
    if (el.tagName() == u"server-data" && el.namespaceURI() == ns_pie) {
        // XEP-0227 documents may contain multiple hosts and users, but as this is used for
        // single-account client-side migration we only read the first host and user.
        const auto host = firstChildElement(el, u"host", ns_pie);
        const auto user = firstChildElement(host, u"user", ns_pie);

        const auto hostJid = host.attribute(u"jid"_s);
        const auto userName = user.attribute(u"name"_s);
        data.setAccountJid(userName.isEmpty() ? hostJid : userName + u'@' + hostJid);

        // Pass 1: native XEP-0227 elements (preferred).
        if (auto error = parseExtensionChildren(user, Format::Xep0227, data.d->extensions)) {
            return std::move(*error);
        }
        // Pass 2: QXmpp-only data embedded directly under <user/> as foreign elements.
        // Types already parsed natively in pass 1 are kept.
        if (auto error = parseExtensionChildren(user, Format::QXmpp, data.d->extensions)) {
            return std::move(*error);
        }
        return data;
    }

    return QXmppError { u"Invalid XML document provided."_s, {} };
}

/*!
    Serializes the account data to \a writer using \a format.

    By default the QXmpp format (\c {<account-data/>}) is written. Pass Format::Xep0227 to
    write \xep{0227, Portable Import/Export Format for XMPP-IM Servers} instead; data types
    without a native \xep{0227} representation (e.g. MIX) are then embedded as a
    QXmpp-specific fallback so no data is lost, and standard servers ignore those elements.
*/
void QXmppExportData::toXml(QXmlStreamWriter *writer, Format format) const
{
    // Resolves, for each present extension, the element id and serializer to use for the
    // requested format. A type with no serializer for that format falls back to its QXmpp
    // serializer, so QXmpp-only data (e.g. MIX) is never dropped.
    struct ResolvedExtension {
        XmlElementId id;
        AnySerializer serialize;
        const std::any *value;
    };

    const auto &mapping = accountDataMapping(format);
    const auto &serializers = accountDataSerializers(format);
    const auto &qxmppMapping = accountDataMapping(Format::QXmpp);
    const auto &qxmppSerializers = accountDataSerializers(Format::QXmpp);

    std::vector<ResolvedExtension> nativeExtensions;
    std::vector<ResolvedExtension> fallbackExtensions;

    for (const auto &[typeIndex, value] : d->extensions) {
        if (const auto serializer = serializers.find(typeIndex); serializer != serializers.end()) {
            nativeExtensions.push_back({ mapping.at(typeIndex), serializer->second, &value });
        } else if (const auto fallback = qxmppSerializers.find(typeIndex); fallback != qxmppSerializers.end()) {
            fallbackExtensions.push_back({ qxmppMapping.at(typeIndex), fallback->second, &value });
        }
    }

    // We need to generate the xml file with nodes always in the same order.
    // This is needed for our unit tests which are based on xml generation.
    const auto byId = [](const ResolvedExtension &left, const ResolvedExtension &right) {
        return left.id < right.id;
    };
    std::ranges::stable_sort(nativeExtensions, byId);
    std::ranges::stable_sort(fallbackExtensions, byId);

    const auto writeExtensions = [writer](const std::vector<ResolvedExtension> &extensions) {
        for (const auto &extension : extensions) {
            extension.serialize(*extension.value, *writer);
        }
    };

    // Writes the QXmpp-only fallback extensions directly under <user/>. Their serializers
    // emit elements without an explicit namespace (they rely on the org.qxmpp.export
    // default namespace of the QXmpp <account-data/> root). Under <user/> the default
    // namespace is urn:xmpp:pie:0, so we declare org.qxmpp.export as the default namespace
    // for each fallback element. writeDefaultNamespace() only applies to the next child
    // element once the <user/> start tag is finalized, hence the empty writeCharacters().
    const auto writeFallbackExtensions = [&] {
        if (fallbackExtensions.empty()) {
            return;
        }
        writer->writeCharacters(QString());
        for (const auto &extension : fallbackExtensions) {
            writer->writeDefaultNamespace(ns_qxmpp_export.toString());
            extension.serialize(*extension.value, *writer);
        }
    };

    XmlWriter w(writer);
    writer->writeStartDocument();

    switch (format) {
    case Format::QXmpp:
        w.write(Element {
            { u"account-data", ns_qxmpp_export },
            Attribute { u"jid", d->accountJid },
            [&] { writeExtensions(nativeExtensions); },
        });
        break;
    case Format::Xep0227:
        w.write(Element {
            { u"server-data", ns_pie },
            Element {
                u"host",
                Attribute { u"jid", QXmppUtils::jidToDomain(d->accountJid) },
                Element {
                    u"user",
                    Attribute { u"name", QXmppUtils::jidToUser(d->accountJid) },
                    [&] { writeExtensions(nativeExtensions); },
                    // QXmpp-only extensions (e.g. MIX) are embedded directly under <user/>
                    // as foreign elements; standard servers ignore them, while QXmpp
                    // re-reads them as a fallback (see fromDom()).
                    writeFallbackExtensions,
                },
            },
        });
        break;
    }

    writer->writeEndDocument();
}

const QString &QXmppExportData::accountJid() const
{
    return d->accountJid;
}

void QXmppExportData::setAccountJid(const QString &jid)
{
    d->accountJid = jid;
}

const std::unordered_map<std::type_index, std::any> &QXmppExportData::extensions() const
{
    return d->extensions;
}

void QXmppExportData::setExtension(std::any value)
{
    d->extensions.emplace(std::type_index(value.type()), std::move(value));
}

void QXmppExportData::registerExtensionInternal(Format format, std::type_index type, ExtensionParser<std::any> parse, ExtensionSerializer<std::any> serialize, QStringView tagName, QStringView xmlns)
{
    const auto id = XmlElementId { tagName.toString(), xmlns.toString() };
    accountDataMapping(format).emplace(type, id);
    accountDataParsers(format).emplace(id, parse);
    accountDataSerializers(format).emplace(type, serialize);
}

bool QXmppExportData::isExtensionRegistered(std::type_index type)
{
    return accountDataSerializers(Format::QXmpp).contains(type) ||
        accountDataSerializers(Format::Xep0227).contains(type);
}

struct QXmppAccountMigrationManagerPrivate {
    template<typename T = Success>
    using Result = std::variant<T, QXmppError>;

    struct ExtensionData {
        std::function<QXmppTask<Result<>>(std::any)> importFunction;
        std::function<QXmppTask<Result<std::any>>()> exportFunction;
    };

    std::unordered_map<std::type_index, ExtensionData> extensions;
};

/*!
    \class QXmppAccountMigrationManager
    \inmodule QXmpp

    \brief Allows to export and import account data.

    This manager helps migrating a user account into another server.

    You can use exportData() to start a data export. Afterwards you can use the exported data to
    start a data import on another account using importData().

    The data that is exported (or imported) is determined by the other registered client
    extensions. They can register callbacks for export and import using registerExtension().

    \ingroup Managers
    \since QXmpp 1.8
*/

/*!
    \typealias QXmppAccountMigrationManager::Result

    Contains T or QXmppError.
*/

/*!
    \fn QXmppAccountMigrationManager::errorOccurred(const QXmppError &error)

    Emitted when an error occured during export or import. \a error is the occured error.
*/

/*!
    \fn template <typename DataType, typename ImportFunc, typename ExportFunc> void QXmppAccountMigrationManager::registerExportData(ImportFunc importFunc, ExportFunc exportFunc)

    Registers a data type that can be imported to an account using \a importFunc and generated using
    \a exportFunc.

    The functions are used when importData() or exportData() is called. You can unregister the
    functions using unregisterExportData().

    The data type MUST also be registered via QXmppExportData::registerExtension() so it can be
    serialized.
*/

/*!
    \fn template <typename DataType> void QXmppAccountMigrationManager::unregisterExportData()

    Unregisters a data type.
*/

/*!
    Constructs an account migration manager.

    \note You would need the \c {QXmppClient(QXmppClient::NoExtensions, this)} approach to use this manager
    because it needs to be instantiated before others using it.
*/
QXmppAccountMigrationManager::QXmppAccountMigrationManager()
    : d(std::make_unique<QXmppAccountMigrationManagerPrivate>())
{
}

QXmppAccountMigrationManager::~QXmppAccountMigrationManager() = default;

/*! Imports QXmppExportData to the currently connected \a account. */
QXmppTask<QXmppAccountMigrationManager::Result<>> QXmppAccountMigrationManager::importData(const QXmppExportData &account)
{
    if (account.extensions().empty()) {
        return makeReadyTask<Result<>>({});
    }

    auto promise = std::make_shared<QXmppPromise<Result<>>>();
    auto task = promise->task();

    auto counter = std::make_shared<int>(account.extensions().size());

    for (const auto &[typeIndex, value] : account.extensions()) {
        auto extensionType = d->extensions.find(typeIndex);

        // There is no registered extension to import this data type
        if (extensionType == d->extensions.cend()) {
            if (--(*counter) == 0) {
                promise->finish(Success());
            }
            continue;
        }

        auto &[_, extension] = *extensionType;
        extension.importFunction(value).then(this, [promise, counter](auto &&result) mutable {
            if (promise->task().isFinished()) {
                return;
            }

            if (hasError(result)) {
                return promise->finish(getError(std::move(result)));
            }

            if ((--(*counter)) == 0) {
                promise->finish(Success());
            }
        });
    }

    return task;
}

/*! Creates a data export of the current account. */
QXmppTask<QXmppAccountMigrationManager::Result<QXmppExportData>> QXmppAccountMigrationManager::exportData()
{
    struct State {
        QXmppPromise<Result<QXmppExportData>> p;
        QXmppExportData data;
        size_t counter = 0;
    };

    auto state = std::make_shared<State>();
    state->data.setAccountJid(client()->configuration().jidBare());
    state->counter = d->extensions.size();

    // early exit
    if (d->extensions.empty()) {
        return makeReadyTask<Result<QXmppExportData>>(std::move(state->data));
    }

    for (const auto &[dataTypeIndex, extension] : std::as_const(d->extensions)) {
        extension.exportFunction().then(this, [state](auto &&result) mutable {
            auto &[p, data, counter] = *state;

            if (p.task().isFinished()) {
                return;
            }

            if (hasError(result)) {
                return p.finish(getError(std::move(result)));
            }

            data.setExtension(getValue(std::move(result)));

            if (--counter == 0) {
                p.finish(std::move(data));
            }
        });
    }

    return state->p.task();
}

void QXmppAccountMigrationManager::registerMigrationDataInternal(
    std::type_index dataType,
    std::function<QXmppTask<Result<>>(std::any)> importFunc,
    std::function<QXmppTask<Result<std::any>>()> exportFunc)
{
    d->extensions.emplace(dataType, QXmppAccountMigrationManagerPrivate::ExtensionData { importFunc, exportFunc });
}

void QXmppAccountMigrationManager::unregisterMigrationDataInternal(std::type_index dataType)
{
    d->extensions.erase(dataType);
}

// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppMucManagerV2.h"

#include "QXmppClient.h"
#include "QXmppPubSubManager.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

using namespace QXmpp;
using namespace QXmpp::Private;

//
// Serialization
//

namespace QXmpp::Private {

struct Bookmarks2Conference {
    bool autojoin = false;
    QString name;
    QString nick;
    QString password;
    // extensions (none supported)
};

struct Bookmarks2ConferenceItem : public QXmppPubSubBaseItem {
    Bookmarks2Conference payload;

    void parsePayload(const QDomElement &el) override
    {
        payload.autojoin = parseBoolean(el.attribute(u"autojoin"_s)).value_or(false);
        payload.name = el.attribute(u"name"_s);
        payload.nick = firstChildElement(el, u"nick", ns_bookmarks2).text();
        payload.password = firstChildElement(el, u"password", ns_bookmarks2).text();
    }
    void serializePayload(QXmlStreamWriter *writer) const override
    {
        XmlWriter(writer).write(Element {
            { u"conference", ns_bookmarks2 },
            OptionalAttribute { u"autojoin", DefaultedBool { payload.autojoin, false } },
            OptionalAttribute { u"name", payload.name },
            OptionalTextElement { u"nick", payload.nick },
            OptionalTextElement { u"password", payload.password },
        });
    }
};

}  // namespace QXmpp::Private

class QXmppMucBookmarkPrivate : public QSharedData
{
public:
    QString jid;
    Bookmarks2Conference payload;
};

///
/// \class QXmppMucBookmark
///
/// Bookmark data for a MUC room, retrieved via \xep{0402, PEP Native Bookmarks}.
///
/// \since QXmpp 1.12
///

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppMucBookmark)

QXmppMucBookmark::QXmppMucBookmark()
    : d(new QXmppMucBookmarkPrivate)
{
}

QXmppMucBookmark::QXmppMucBookmark(QString &&jid, QXmpp::Private::Bookmarks2Conference &&conference)
    : d(new QXmppMucBookmarkPrivate { {}, std::move(jid), std::move(conference) })
{
}

/// Returns the (bare) JID of the MUC.
const QString &QXmppMucBookmark::jid() const { return d->jid; }
/// Sets the (bare) JID of the MUC.
void QXmppMucBookmark::setJid(const QString &jid) { d->jid = jid; }

/// Returns the user-defined display name of the MUC.
const QString &QXmppMucBookmark::name() const { return d->payload.name; }
/// Sets the user-defined display name of the MUC.
void QXmppMucBookmark::setName(const QString &name) { d->payload.name = name; }

/// Returns the user's preferred nick for this MUC.
const QString &QXmppMucBookmark::nick() const { return d->payload.nick; }
/// Sets the user's preferred nick for this MUC.
void QXmppMucBookmark::setNick(const QString &nick) { d->payload.nick = nick; }

/// Returns the required password for the MUC.
const QString &QXmppMucBookmark::password() const { return d->payload.password; }
/// Sets the required password for the MUC.
void QXmppMucBookmark::setPassword(const QString &password) { d->payload.password = password; }

/// Returns whether to automatically join this MUC on connection.
bool QXmppMucBookmark::autojoin() const { return d->payload.autojoin; }
/// Sets whether to automatically join this MUC on connection.
void QXmppMucBookmark::setAutojoin(bool autojoin) { d->payload.autojoin = autojoin; }

//
// Manager
//

using EmptyResult = std::variant<Success, QXmppError>;

struct QXmppMucManagerV2Private {
    QXmppMucManagerV2 *q = nullptr;
    std::optional<QList<QXmppMucBookmark>> bookmarks;

    void setBookmarks(QVector<Bookmarks2ConferenceItem> &&items);
    void resetBookmarks();
    QXmppTask<EmptyResult> setBookmark(QXmppMucBookmark &&bookmark);
    QXmppTask<EmptyResult> removeBookmark(const QString &jid);
};

QXmppMucManagerV2::QXmppMucManagerV2()
    : d(std::make_unique<QXmppMucManagerV2Private>(this))
{
}
QXmppMucManagerV2::~QXmppMucManagerV2() = default;

/// Supported service discovery features.
QStringList QXmppMucManagerV2::discoveryFeatures() const { return { ns_bookmarks2 + u"+notify" }; }
/// Retrieved bookmarks.
const std::optional<QList<QXmppMucBookmark>> &QXmppMucManagerV2::bookmarks() const { return d->bookmarks; }

/// Adds or updates the bookmark for a MUC.
QXmppTask<QXmppMucManagerV2::EmptyResult> QXmppMucManagerV2::setBookmark(QXmppMucBookmark &&bookmark)
{
    return d->setBookmark(std::move(bookmark));
}

QXmppTask<QXmppMucManagerV2::EmptyResult> QXmppMucManagerV2::removeBookmark(const QString &jid)
{
}

void QXmppMucManagerV2::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, &QXmppMucManagerV2::onConnected);
    connect(client, &QXmppClient::disconnected, this, &QXmppMucManagerV2::onDisconnected);
}

void QXmppMucManagerV2::onUnregistered(QXmppClient *client)
{
    disconnect(client, nullptr, this, nullptr);
}

void QXmppMucManagerV2::onConnected()
{
    using PubSub = QXmppPubSubManager;

    if (client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->resetBookmarks();

        client()
            ->findExtension<PubSub>()
            ->requestItems<Bookmarks2ConferenceItem>({}, ns_bookmarks2.toString())
            .then(this, [this](auto &&result) {
                if (std::holds_alternative<QXmppError>(result)) {
                    warning(u"Could not fetch MUC Bookmarks: " + std::get<QXmppError>(result).description);
                    return;
                }

                auto items = std::get<PubSub::Items<Bookmarks2ConferenceItem>>(std::move(result));
                d->setBookmarks(std::move(items.items));
            });
    }
}

void QXmppMucManagerV2::onDisconnected()
{
}

auto QXmppMucManagerV2Private::setBookmark(QXmppMucBookmark &&bookmark) -> QXmppTask<EmptyResult>
{
    auto opts = QXmppPubSubPublishOptions();
    opts.setPersistItems(true);
    opts.setMaxItems(QXmppPubSubNodeConfig::Max());
    opts.setSendLastItem(QXmppPubSubNodeConfig::SendLastItemType::Never);
    opts.setAccessModel(QXmppPubSubNodeConfig::AccessModel::Allowlist);

    auto item = Bookmarks2ConferenceItem();
    item.setId(bookmark.jid());
    item.payload = bookmark.d->payload;

    return chain<EmptyResult>(
        q->client()
            ->findExtension<QXmppPubSubManager>()
            ->publishItem({}, ns_bookmarks2.toString(), item, opts),
        q,
        [this, bookmark](auto &&result) mutable -> EmptyResult {
            if (std::holds_alternative<QXmppError>(result)) {
                return std::get<QXmppError>(std::move(result));
            }

            if (bookmarks) {
                bookmarks->append(std::move(bookmark));
            }
        });
}

QXmppTask<EmptyResult> QXmppMucManagerV2Private::removeBookmark(const QString &jid)
{
    return chain<EmptyResult>
}

void QXmppMucManagerV2Private::setBookmarks(QVector<Bookmarks2ConferenceItem> &&items)
{
    Q_ASSERT(!bookmarks.has_value());
    bookmarks = transform<QList<QXmppMucBookmark>>(items, [](auto &&item) {
        return QXmppMucBookmark { item.id(), std::move(item.payload) };
    });
    Q_EMIT q->bookmarksReset();
}

void QXmppMucManagerV2Private::resetBookmarks()
{
    if (bookmarks) {
        bookmarks.reset();
        Q_EMIT q->bookmarksReset();
    }
}

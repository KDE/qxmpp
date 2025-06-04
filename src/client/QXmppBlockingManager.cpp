// SPDX-FileCopyrightText: 2023 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppBlockingManager.h"

#include "QXmppConstants_p.h"
#include "QXmppIqHandling.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "Iq.h"
#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;
using StanzaError = QXmppStanza::Error;

// utility function
template<typename T>
static void makeUnique(T &vec)
{
    std::sort(vec.begin(), vec.end());
    vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

// IQs
enum BlockingAction {
    List,
    Block,
    Unblock,
};

template<>
struct Enums::Data<BlockingAction> {
    static constexpr auto Values = makeValues<BlockingAction>({
        { List, u"blocklist" },
        { Block, u"block" },
        { Unblock, u"unblock" },
    });
};

template<BlockingAction Action>
struct Blocking {
    QVector<QString> jids;

    static constexpr std::tuple XmlTag = { Enums::toString(Action), ns_blocking };

    bool parse(const QDomElement &el)
    {
        if (elementXmlTag(el) == XmlTag) {
            jids = parseSingleAttributeElements<QVector<QString>>(el, u"item", ns_blocking, u"jid"_s);
            return true;
        }
        return false;
    }
    void toXml(XmlWriter &w) const
    {
        w.write(Element { XmlTag, SingleAttributeElements { u"item", u"jid", jids } });
    }
};

// Manager data
struct QXmppBlockingManagerPrivate {
    std::optional<QVector<QString>> blocklist;
    std::vector<QXmppPromise<QXmppBlockingManager::BlocklistResult>> openFetchBlocklistPromises;
};

///
/// \class QXmppBlockingManager
///
/// \brief Uses \xep{0191, Blocking Command} to manage blocked accounts and services.
///
/// ## Use Cases
///
///  * listing blocked devices, accounts and servers
///  * blocking and unblocking JIDs
///  * getting notified when a new JID has been blocked or unblocked
///
/// ### Listing blocked devices
///
/// You can receive a list of blocked JIDs by using fetchBlocklist().
/// ```
/// manager->fetchBlocklist().then(this, [](auto result) {
///     if (auto *blocklist = std::get_if<QXmppBlocklist>(&result)) {
///         qDebug() << "Blocked JIDs:" << blocklist->entries();
///     } else if (auto *err = std::get_if<QXmppError>(&result)) {
///         qDebug() << "Error fetching blocklist:" << err->description;
///     }
/// });
/// ```
///
/// The server will send updates to us for the rest of the stream. You can listen to the updates by
/// connecting to blocked() and unblocked().
///
/// \note The manager caches the blocklist, so after the first time the task will finish instantly.
///
/// ### Blocking and Unblocking
///
/// You can use block() and unblock() for this purpose.
/// ```
/// manager->block("baduser@spam.im").then(this, [](auto result) {
///     if (std::holds_alternative<QXmpp::Success>(result)) {
///         qDebug() << "Blocked baduser@spam.im!";
///     } else if (auto *err = std::get_if<QXmppError>(&result)) {
///         qDebug() << "Error:" << err->description;
///     }
/// });
/// ```
/// unblock() works likewise.
///
/// \note This will also trigger blocked() or unblocked() if you are subscribed to the blocklist.
///
/// ### Blocklist Subscription
///
/// You will automatically receive blocklist updates after you requested the blocklist. You can
/// connect to the blocked() and unblocked() signals.
///
/// ## Format
///
/// It is important to notice that the blocked JIDs are not limited to accounts, allowed are the
/// following formats:
///  * `user@domain/resource`
///  * `user@domain`
///  * `domain/resource`
///  * `domain`
///
/// It is not possible to block a domain without blocking a specific account though (or another
/// combination).
///
/// ## Setup
///
/// The blocking manager is not enabled by default and needs to be registered with your
/// QXmppClient.
/// ```
/// auto *blockingManager = client->addNewExtension<QXmppBlockingManager>();
/// ```
///
/// \ingroup Managers
/// \sa QXmppBlocklist
/// \since QXmpp 1.6
///

///
/// \typedef QXmppBlockingManager::BlocklistResult
///
/// Contains a QXmppBlocklist or an error.
///

///
/// \typedef QXmppBlockingManager::Result
///
/// Contains QXmpp::Success or an error.
///

///
/// \fn QXmppBlockingManager::subscribedChanged()
///
/// Called whenever the state of the subscription to blocklist updates has changed.
///

///
/// \fn QXmppBlockingManager::blocked
///
/// Emitted when a blocklist update with new blocked JIDs has been received.
///
/// This is also emitted when you call block().
///

///
/// \fn QXmppBlockingManager::unblocked
///
/// Emitted when a blocklist update with new unblocked JIDs has been received.
///
/// This is also emitted when you call unblock().
///

// Manager
QXmppBlockingManager::QXmppBlockingManager()
    : d(new QXmppBlockingManagerPrivate)
{
}

QXmppBlockingManager::~QXmppBlockingManager() = default;

///
/// \brief Returns whether the blocking manager currently receives updates of the blocklist.
///
/// The subscription is enabled automatically after fetching the blocklist using fetchBlocklist().
///
bool QXmppBlockingManager::isSubscribed() const
{
    return d->blocklist.has_value();
}

///
/// \brief Fetches the list of blocked JIDs and subscribes to blocklist updates.
///
/// The manager will cache the blocklist and keep track of updates for the rest of the session.
/// Later calls of this function will report the cached result immediately. Even calling this
/// function multiple times before the first request has finished, will not trigger more than one
/// IQ request being sent.
///
QXmppTask<QXmppBlockingManager::BlocklistResult> QXmppBlockingManager::fetchBlocklist()
{
    // use cached blocklist if possible
    if (d->blocklist) {
        return makeReadyTask<BlocklistResult>(QXmppBlocklist(d->blocklist.value()));
    }

    // This function is designed so that you can call it multiple times and the actual IQ request
    // to the server is only done once.
    //
    // If there's no data yet, we cache all open promises. When the IQ request finishes, the result
    // is reported to all promises.

    // Create promise and cache it
    QXmppPromise<BlocklistResult> promise;
    auto task = promise.task();

    d->openFetchBlocklistPromises.push_back(std::move(promise));

    // send IQ request, if this is the first request
    if (d->openFetchBlocklistPromises.size() == 1) {
        auto iq = CompatIq {
            GetIq<Blocking<List>> {
                generateSequentialStanzaId(),
            },
        };
        client()->sendIq(std::move(iq)).then(this, [this](QXmppClient::IqResult &&result) {
            // parse into QXmppBlocklist/Error variant
            auto blocklistResult = map<BlocklistResult>(
                [](Blocking<List> &&list) { return QXmppBlocklist(list.jids); },
                parseIqResponse<Blocking<List>>(std::move(result)));

            // store blocklist on success
            if (!d->blocklist) {
                if (auto *blocklist = std::get_if<QXmppBlocklist>(&blocklistResult)) {
                    d->blocklist = blocklist->entries();
                    Q_EMIT subscribedChanged();
                }
            }

            // report result to all promises
            for (auto &promise : d->openFetchBlocklistPromises) {
                auto copy = blocklistResult;
                promise.finish(std::move(copy));
            }
            // delete cached promises
            d->openFetchBlocklistPromises.clear();
        });
    }

    return task;
}

///
/// \fn QXmppBlockingManager::block(QString jid)
///
/// Blocks a JID.
///
/// \sa unblock()
///

///
/// Blocks a list of JIDs.
///
/// \sa unblock()
///
QXmppTask<QXmppBlockingManager::Result> QXmppBlockingManager::block(QVector<QString> jids)
{
    return client()->sendGenericIq(CompatIq {
        SetIq<Blocking<Block>> {
            generateSequentialStanzaId(), {}, {}, {}, { std::move(jids) } },
    });
}

///
/// \fn QXmppBlockingManager::unblock(QString jid)
///
/// Unblocks a JID.
///
/// \sa block()
///

///
/// Unblocks a list of JIDs.
///
/// \sa block()
///
QXmppTask<QXmppBlockingManager::Result> QXmppBlockingManager::unblock(QVector<QString> jids)
{
    return client()->sendGenericIq(CompatIq {
        SetIq<Blocking<Unblock>> {
            generateSequentialStanzaId(), {}, {}, {}, { std::move(jids) } },
    });
}

/// \cond
QStringList QXmppBlockingManager::discoveryFeatures() const
{
    return { ns_blocking.toString() };
}

void QXmppBlockingManager::onRegistered(QXmppClient *client)
{
    connect(client, &QXmppClient::connected, this, &QXmppBlockingManager::onConnected);
}

void QXmppBlockingManager::onUnregistered(QXmppClient *oldClient)
{
    disconnect(oldClient, &QXmppClient::connected, this, &QXmppBlockingManager::onConnected);
}

bool QXmppBlockingManager::handleStanza(const QDomElement &stanza, const std::optional<QXmppE2eeMetadata> &)
{
    auto checkIqValidity = [this](QStringView from) -> std::optional<StanzaError> {
        // check permissions
        // only server (user's account) is allowed to do blocklist pushes
        if (!from.isEmpty() && from != client()->configuration().jidBare()) {
            // deny
            return StanzaError {
                StanzaError::Cancel,
                StanzaError::Forbidden,
                u"Forbidden."_s
            };
        }

        if (!d->blocklist) {
            return StanzaError {
                StanzaError::Wait,
                StanzaError::UnexpectedRequest,
                u"Client is not subscribed to blocklist."_s
            };
        }

        // allow
        return {};
    };

    auto handleBlock = [this, checkIqValidity](SetIq<Blocking<Block>> &&iq) -> std::variant<QXmppIq, StanzaError> {
        if (auto err = checkIqValidity(iq.from)) {
            return std::move(*err);
        }

        // store new jids
        d->blocklist->append(iq.payload.jids);
        // assure blocklist has no duplicates
        makeUnique(*d->blocklist);

        Q_EMIT blocked(iq.payload.jids);
        return QXmppIq(QXmppIq::Result);
    };
    auto handleUnblock = [this, checkIqValidity](SetIq<Blocking<Unblock>> &&iq) -> std::variant<QXmppIq, StanzaError> {
        if (auto err = checkIqValidity(iq.from)) {
            return std::move(*err);
        }

        // remove jids
        for (const auto &jid : std::as_const(iq.payload.jids)) {
            d->blocklist->removeOne(jid);
        }

        Q_EMIT unblocked(iq.payload.jids);
        return QXmppIq(QXmppIq::Result);
    };

    // e2ee is not supported (not needed with local server)
    return handleIqRequests<SetIq<Blocking<Block>>, SetIq<Blocking<Unblock>>>(stanza, client(), overloaded { handleBlock, handleUnblock });
}
/// \endcond

void QXmppBlockingManager::onConnected()
{
    if (d->blocklist && client()->streamManagementState() != QXmppClient::ResumedStream) {
        d->blocklist.reset();
        Q_EMIT subscribedChanged();
    }
}

///
/// \class QXmppBlocklist
///
/// \brief List of blocked entries according to \xep{0191, Blocking Command} with helper functions
/// to check the blocking state of JIDs.
///
/// \sa QXmppBlockingManager
/// \since QXmpp 1.6
///

///
/// \typedef QXmppBlocklist::BlockingState
///
/// Whether a JID is completely blocked (Blocked), partially blocked (PartiallyBlocked) or not
/// blocked (NotBlocked).
///

QXmppBlocklist::QXmppBlocklist() = default;

/// Constructs with given entries.
QXmppBlocklist::QXmppBlocklist(QVector<QString> entries)
    : m_blocklist(std::move(entries))
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppBlocklist)

///
/// Returns a list of blocked entires.
///
/// Entries may be full JIDs, bare JIDs, domains or domains with resource, as in
/// \xep{0191, Blocking Command}.
///
QVector<QString> QXmppBlocklist::entries() const
{
    return m_blocklist;
}

///
/// Checks whether the blocklist contains an entry.
///
/// \note This does not check whether a JID may be blocked or blocked partially by other entries.
/// E.g. `containsEntry("user@domain.tld")` will return false even if `domain.tld` is blocked
/// completely.
///
bool QXmppBlocklist::containsEntry(QStringView entry) const
{
    // Can be replaced with .contains() with Qt 6
    return std::find(m_blocklist.begin(), m_blocklist.end(), entry) != m_blocklist.end();
}

///
/// Checks the blocking state of a JID.
///
/// A JID can be a full JID, a bare JID, a domain or a domain with a resource, as in
/// \xep{0191, Blocking Command}.
///
/// \returns BlockingState of the JID.
///
QXmppBlocklist::BlockingState QXmppBlocklist::blockingState(const QString &jid) const
{
    auto user = QXmppUtils::jidToUser(jid);
    auto domain = QXmppUtils::jidToDomain(jid);
    auto resource = QXmppUtils::jidToResource(jid);

    Q_ASSERT(!jid.isEmpty());
    Q_ASSERT(!domain.isEmpty());

    // determine type of given JID
    enum { FullJid,
           BareJid,
           Domain,
           DomainResource } jidType = [&] {
        // domain is always set
        if (user.isEmpty()) {
            // no user set
            if (!resource.isEmpty()) {
                return DomainResource;
            }
            return Domain;
        }
        // user is set
        if (!resource.isEmpty()) {
            return FullJid;
        }
        return BareJid;
    }();

    // cause the given jid to be blocked (completely)
    QVector<QString> blockingJids;
    // cause parts of the given JID to be blocked
    QVector<QString> partiallyBlockingJids;

    auto checkBlockingJid = [this, &blockingJids](const QString &jid) {
        if (m_blocklist.contains(jid)) {
            blockingJids.append(jid);
        }
    };
    auto checkPartiallyBlockingJid = [this, &partiallyBlockingJids](const QString &jid) {
        if (m_blocklist.contains(jid)) {
            partiallyBlockingJids.append(jid);
        }
    };

    switch (jidType) {
    case FullJid:
        // Blocking:
        //  * full jid
        //  * bare jid
        //  * domain
        //  * domain + resource
        // Partially blocked:
        //  not possible
        checkBlockingJid(jid);
        checkBlockingJid(user + u'@' + domain);
        checkBlockingJid(domain);
        checkBlockingJid(domain + u'/' + resource);
        break;
    case BareJid: {
        // Blocking:
        //  * bare jid
        //  * domain
        // Partially blocking:
        //  * full jids
        //  * domain resource
        checkBlockingJid(jid);
        checkBlockingJid(domain);

        // look for full jids blocking the bare jid partially
        QString fullJidStart = jid + u'/';
        for (const auto &blockedJid : m_blocklist) {
            if (blockedJid.startsWith(fullJidStart)) {
                partiallyBlockingJids.append(blockedJid);
            }
        }

        checkPartiallyBlockingJid(domain + u'/' + resource);
        break;
    }
    case Domain: {
        // Blocking:
        //  * domain
        // Partially blocking:
        //  * full jids
        //  * bare jids
        //  * domain + resource
        checkBlockingJid(jid);

        // look for full/bare jids and domain+resource jids
        QString userJidSubstring = u'@' + domain;
        QString domainResourceSubstring = domain + u'/';

        for (const auto &blockedJid : m_blocklist) {
            if (blockedJid.contains(userJidSubstring) || blockedJid.contains(domainResourceSubstring)) {
                partiallyBlockingJids.append(blockedJid);
            }
        }

        break;
    }
    case DomainResource: {
        // Blocking:
        //  * domain
        //  * domain + resource
        // Partially blocking:
        //  * full jid
        //  * bare jid
        checkBlockingJid(jid);
        checkBlockingJid(domain);

        // look for full/bare jids
        QString userJidSubstring = u'@' + domain;
        for (const auto &blockedJid : m_blocklist) {
            if (blockedJid.contains(userJidSubstring)) {
                partiallyBlockingJids.append(blockedJid);
            }
        }
        break;
    }
    }

    if (!blockingJids.isEmpty()) {
        return Blocked {
            blockingJids,
            partiallyBlockingJids,
        };
    }
    if (!partiallyBlockingJids.isEmpty()) {
        return PartiallyBlocked {
            partiallyBlockingJids
        };
    }
    return NotBlocked {};
}

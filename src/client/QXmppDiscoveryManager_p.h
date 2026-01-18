// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPDISCOVERYMANAGER_P_H
#define QXMPPDISCOVERYMANAGER_P_H

#include "QXmppDiscoveryManager.h"
#include "QXmppPromise.h"

#include "Iq.h"

#include <QCache>

using namespace QXmpp::Private;

namespace QXmpp::Private {

template<typename Params, typename Response>
struct AttachableRequests {
    struct Request {
        Params params;
        std::vector<QXmppPromise<Response>> promises;
    };

    std::vector<Request> requests;

    /// Find existing request and attach if found.
    std::optional<QXmppTask<Response>> attach(const Params &key)
    {
        auto itr = std::ranges::find(requests, key, &Request::params);
        if (itr != requests.end()) {
            QXmppPromise<Response> p;
            auto task = p.task();
            itr->promises.push_back(std::move(p));
            return task;
        }

        return std::nullopt;
    }

    QXmppTask<Response> makeNew(Params key)
    {
        Q_ASSERT(!contains(requests, key, &Request::params));

        QXmppPromise<Response> p;
        auto task = p.task();
        requests.push_back(Request { key, { std::move(p) } });
        return task;
    }

    void finish(const Params &key, Response &&response)
    {
        auto itr = std::ranges::find(requests, key, &Request::params);
        Q_ASSERT(itr != requests.end());
        if (itr == requests.end()) {
            return;
        }

        auto promises = std::move(itr->promises);
        requests.erase(itr);

        for (auto it = promises.begin(); it != promises.end(); ++it) {
            // copy unless this is the last iteration (then do move)
            it->finish(std::next(it) == promises.end() ? std::move(response) : response);
        }
    }

    QXmppTask<Response> produce(Params key, std::function<QXmppTask<Response>(Params)> requestFunction, QObject *context)
    {
        if (auto task = attach(key)) {
            return std::move(*task);
        }
        auto task = makeNew(key);
        requestFunction(key).then(context, [this, key](auto &&response) {
            finish(key, std::move(response));
        });
        return task;
    }
};

}  // namespace QXmpp::Private

class QXmppDiscoveryManagerPrivate
{
public:
    using StanzaError = QXmppStanza::Error;

    QXmppDiscoveryManager *q = nullptr;
    QString clientCapabilitiesNode;
    QList<QXmppDiscoIdentity> identities;
    QList<QXmppDataForm> dataForms;

    // cached data
    QCache<std::tuple<QString, QString>, QXmppDiscoInfo> infoCache;
    QCache<std::tuple<QString, QString>, QList<QXmppDiscoItem>> itemsCache;

    // outgoing requests
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QXmppDiscoInfo>> infoRequests;
    AttachableRequests<std::tuple<QString, QString>, QXmpp::Result<QList<QXmppDiscoItem>>> itemsRequests;

    explicit QXmppDiscoveryManagerPrivate(QXmppDiscoveryManager *q) : q(q) { }

    static QString defaultApplicationName();
    static QXmppDiscoIdentity defaultIdentity();

    std::variant<CompatIq<QXmppDiscoInfo>, StanzaError> handleIq(GetIq<QXmppDiscoInfo> &&iq);
    std::variant<CompatIq<QXmppDiscoItems>, StanzaError> handleIq(GetIq<QXmppDiscoItems> &&iq);
};

#endif  // QXMPPDISCOVERYMANAGER_P_H

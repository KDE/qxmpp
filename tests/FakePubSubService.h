// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FAKEPUBSUBSERVICE_H
#define FAKEPUBSUBSERVICE_H

#include "StringLiterals.h"
#include "TestClient.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QMap>
#include <QSet>
#include <QTextStream>

// Answers the PubSub requests TestClients send, like the PEP services of their and other
// accounts. Supports getting items, publishing with publish options, configuring and deleting
// nodes and retracting items. Publishing sends an event to all clients, as if they had subscribed
// to each other.
class FakePubSubService
{
public:
    struct Item {
        QString id;
        QString payload;
    };

    struct Node {
        // default of XEP-0163
        QString accessModel = u"presence"_s;
        QString maxItems = u"1"_s;
        QList<Item> items;
    };

    FakePubSubService() = default;
    FakePubSubService(TestClient &client) { addClient(client); }

    void addClient(TestClient &client) { m_clients.append(&client); }
    void removeClient(TestClient &client) { m_clients.removeAll(&client); }

    // Answers the sent requests until no packet is left. Other packets than PubSub requests are
    // kept, see takeOtherPackets().
    void process()
    {
        bool handled = true;
        while (handled) {
            handled = false;
            for (auto *client : std::as_const(m_clients)) {
                while (client->hasPacket()) {
                    handle(*client, client->takePacket());
                    QCoreApplication::processEvents();
                    handled = true;
                }
            }
        }
    }

    // sent packets of the client that are no PubSub requests, e.g. messages
    QList<QString> takeOtherPackets(TestClient &client) { return m_otherPackets.take(&client); }

    void setNotificationsEnabled(bool enabled) { m_notify = enabled; }
    // fails the next request to the node with the error condition
    void failNextRequest(const QString &node, const QString &condition) { m_failures.insert(node, condition); }

    Node *node(const QString &jid, const QString &name)
    {
        auto it = m_nodes.find({ jid, name });
        return it == m_nodes.end() ? nullptr : &*it;
    }
    Node &createNode(const QString &jid, const QString &name, const QString &accessModel = u"open"_s, const QString &maxItems = u"max"_s)
    {
        auto &node = m_nodes[{ jid, name }];
        node.accessModel = accessModel;
        node.maxItems = maxItems;
        return node;
    }
    void setItem(const QString &jid, const QString &nodeName, const QString &itemId, const QString &payload)
    {
        if (!node(jid, nodeName)) {
            createNode(jid, nodeName);
        }
        storeItem(*node(jid, nodeName), { itemId, payload });
    }
    QString itemPayload(const QString &jid, const QString &nodeName, const QString &itemId)
    {
        if (auto *n = node(jid, nodeName)) {
            for (const auto &item : std::as_const(n->items)) {
                if (item.id == itemId) {
                    return item.payload;
                }
            }
        }
        return {};
    }
    // JIDs subscribed explicitly to the node
    QSet<QString> subscribers(const QString &jid, const QString &nodeName) const { return m_subscriptions.value({ jid, nodeName }); }
    QList<QString> nodeNames(const QString &jid) const
    {
        QList<QString> names;
        for (auto it = m_nodes.keyBegin(); it != m_nodes.keyEnd(); ++it) {
            if (it->first == jid) {
                names.append(it->second);
            }
        }
        return names;
    }

    // number of requests per type, e.g. "publish" or "items"
    int requestCount(const QString &type) const { return m_requestCounts.value(type); }
    void resetRequestCounts() { m_requestCounts.clear(); }

private:
    static QString serialize(const QDomElement &element)
    {
        QString xml;
        QTextStream stream(&xml);
        element.save(stream, -1);
        return xml;
    }

    static QMap<QString, QString> formFields(const QDomElement &parent)
    {
        QMap<QString, QString> fields;
        const auto form = parent.firstChildElement(u"x"_s);
        for (auto field = form.firstChildElement(u"field"_s); !field.isNull(); field = field.nextSiblingElement(u"field"_s)) {
            fields.insert(field.attribute(u"var"_s), field.firstChildElement(u"value"_s).text());
        }
        return fields;
    }

    static void storeItem(Node &node, Item item)
    {
        node.items.removeIf([&](const Item &existing) { return existing.id == item.id; });
        node.items.append(std::move(item));
        if (node.maxItems != u"max") {
            const auto max = node.maxItems.toInt();
            while (node.items.size() > max) {
                node.items.removeFirst();
            }
        }
    }

    void handle(TestClient &client, const QString &packet)
    {
        // without namespace processing, so payloads can be serialized as they are
        const auto iq = xmlToDomDoc(packet, false).documentElement();
        const auto pubsub = iq.firstChildElement(u"pubsub"_s);
        if (iq.tagName() != u"iq" || pubsub.isNull()) {
            m_otherPackets[&client].append(packet);
            return;
        }

        m_current = &client;
        const auto id = iq.attribute(u"id"_s);
        const auto jid = iq.attribute(u"to"_s, client.configuration().jidBare());
        const auto request = pubsub.firstChildElement();
        const auto nodeName = request.attribute(u"node"_s);
        m_requestCounts[request.tagName()]++;

        if (auto failure = m_failures.take(nodeName); !failure.isEmpty()) {
            replyError(id, jid, failure);
            return;
        }

        const auto type = request.tagName();
        if (type == u"items") {
            handleItems(id, jid, nodeName, request);
        } else if (type == u"publish") {
            handlePublish(id, jid, nodeName, request, pubsub.firstChildElement(u"publish-options"_s));
        } else if (type == u"configure") {
            if (auto *n = node(jid, nodeName)) {
                applyConfig(*n, formFields(request));
                replyResult(id, jid);
            } else {
                replyError(id, jid, u"item-not-found"_s);
            }
        } else if (type == u"delete") {
            if (m_nodes.remove({ jid, nodeName })) {
                replyResult(id, jid);
                notify(jid, u"<delete node='"_s + nodeName + u"'/>"_s);
            } else {
                replyError(id, jid, u"item-not-found"_s);
            }
        } else if (type == u"retract") {
            const auto itemId = request.firstChildElement(u"item"_s).attribute(u"id"_s);
            if (auto *n = node(jid, nodeName)) {
                n->items.removeIf([&](const Item &item) { return item.id == itemId; });
                replyResult(id, jid);
            } else {
                replyError(id, jid, u"item-not-found"_s);
            }
        } else if (type == u"subscribe") {
            m_subscriptions[{ jid, nodeName }].insert(request.attribute(u"jid"_s));
            replyResult(id, jid);
        } else if (type == u"unsubscribe") {
            m_subscriptions[{ jid, nodeName }].remove(request.attribute(u"jid"_s));
            replyResult(id, jid);
        } else {
            replyError(id, jid, u"feature-not-implemented"_s);
        }
    }

    void handleItems(const QString &id, const QString &jid, const QString &nodeName, const QDomElement &request)
    {
        auto *n = node(jid, nodeName);
        if (!n) {
            replyError(id, jid, u"item-not-found"_s);
            return;
        }

        QList<QString> requestedIds;
        for (auto item = request.firstChildElement(u"item"_s); !item.isNull(); item = item.nextSiblingElement(u"item"_s)) {
            requestedIds.append(item.attribute(u"id"_s));
        }

        QString items;
        for (const auto &item : std::as_const(n->items)) {
            if (requestedIds.isEmpty() || requestedIds.contains(item.id)) {
                items += u"<item id='"_s + item.id + u"'>"_s + item.payload + u"</item>"_s;
            }
        }
        if (!requestedIds.isEmpty() && items.isEmpty()) {
            replyError(id, jid, u"item-not-found"_s);
            return;
        }
        m_current->inject(u"<iq id='"_s + id + u"' from='"_s + jid + u"' type='result'>"
                                                                     "<pubsub xmlns='http://jabber.org/protocol/pubsub'><items node='"_s +
                          nodeName + u"'>"_s + items + u"</items></pubsub></iq>"_s);
    }

    void handlePublish(const QString &id, const QString &jid, const QString &nodeName, const QDomElement &request, const QDomElement &publishOptions)
    {
        const auto options = formFields(publishOptions);
        auto *n = node(jid, nodeName);
        if (n) {
            const auto accessModel = options.value(u"pubsub#access_model"_s);
            const auto maxItems = options.value(u"pubsub#max_items"_s);
            if ((!accessModel.isEmpty() && accessModel != n->accessModel) || (!maxItems.isEmpty() && maxItems != n->maxItems)) {
                m_current->inject(u"<iq id='"_s + id + u"' from='"_s + jid + u"' type='error'><error type='cancel'>"
                                                                             "<conflict xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
                                                                             "<precondition-not-met xmlns='http://jabber.org/protocol/pubsub#errors'/>"
                                                                             "</error></iq>"_s);
                return;
            }
        } else {
            n = &m_nodes[{ jid, nodeName }];
            applyConfig(*n, options);
        }

        const auto itemElement = request.firstChildElement(u"item"_s);
        Item item { itemElement.attribute(u"id"_s), serialize(itemElement.firstChildElement()) };
        storeItem(*n, item);
        replyResult(id, jid);
        notify(jid, u"<items node='"_s + nodeName + u"'><item id='"_s + item.id + u"'>"_s + item.payload + u"</item></items>"_s);
    }

    static void applyConfig(Node &node, const QMap<QString, QString> &fields)
    {
        if (const auto accessModel = fields.value(u"pubsub#access_model"_s); !accessModel.isEmpty()) {
            node.accessModel = accessModel;
        }
        if (const auto maxItems = fields.value(u"pubsub#max_items"_s); !maxItems.isEmpty()) {
            node.maxItems = maxItems;
        }
    }

    void notify(const QString &jid, const QString &event)
    {
        if (!m_notify) {
            return;
        }
        for (auto *client : std::as_const(m_clients)) {
            client->inject(u"<message from='"_s + jid + u"' to='"_s + client->configuration().jid() +
                           u"' type='headline'><event xmlns='http://jabber.org/protocol/pubsub#event'>"_s + event + u"</event></message>"_s);
        }
    }

    void replyResult(const QString &id, const QString &jid)
    {
        m_current->inject(u"<iq id='"_s + id + u"' from='"_s + jid + u"' type='result'/>"_s);
    }

    void replyError(const QString &id, const QString &jid, const QString &condition)
    {
        m_current->inject(u"<iq id='"_s + id + u"' from='"_s + jid + u"' type='error'><error type='cancel'><"_s + condition +
                          u" xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></iq>"_s);
    }

    QList<TestClient *> m_clients;
    TestClient *m_current = nullptr;
    QHash<TestClient *, QList<QString>> m_otherPackets;
    QMap<std::pair<QString, QString>, Node> m_nodes;
    QMap<std::pair<QString, QString>, QSet<QString>> m_subscriptions;
    QMap<QString, QString> m_failures;
    QMap<QString, int> m_requestCounts;
    bool m_notify = true;
};

#endif  // FAKEPUBSUBSERVICE_H

// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPOMEMOMEMORYSTORAGE_H
#define QXMPPOMEMOMEMORYSTORAGE_H

#include "QXmppOmemoStorage.h"
#include "QXmppTask.h"
#include "qxmppomemo_export.h"

#include <memory>

class QXmppOmemoMemoryStoragePrivate;

#if QXMPP_DEPRECATED_SINCE(1, 18)
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED

class [[deprecated("Use QXmpp::OmemoMemoryStorage")]]
QXMPPOMEMO_EXPORT QXmppOmemoMemoryStorage : public QXmppOmemoStorage
{
public:
    QXmppOmemoMemoryStorage();
    ~QXmppOmemoMemoryStorage() override;

    QXmppTask<OmemoData> allData() override;

    QXmppTask<void> setOwnDevice(const std::optional<OwnDevice> &device) override;

    QXmppTask<void> addSignedPreKeyPair(uint32_t keyId, const SignedPreKeyPair &keyPair) override;
    QXmppTask<void> removeSignedPreKeyPair(uint32_t keyId) override;

    QXmppTask<void> addPreKeyPairs(const QHash<uint32_t, QByteArray> &keyPairs) override;
    QXmppTask<void> removePreKeyPair(uint32_t keyId) override;

    QXmppTask<void> addDevice(const QString &jid, uint32_t deviceId, const Device &device) override;
    QXmppTask<void> removeDevice(const QString &jid, uint32_t deviceId) override;
    QXmppTask<void> removeDevices(const QString &jid) override;

    QXmppTask<void> resetAll() override;

private:
    std::unique_ptr<QXmppOmemoMemoryStoragePrivate> d;
};

QT_WARNING_POP
#endif

namespace QXmpp::Omemo::Private {
struct OmemoMemoryStoragePrivate;
}

namespace QXmpp {

class QXMPPOMEMO_EXPORT OmemoMemoryStorage : public OmemoStorage
{
public:
    OmemoMemoryStorage();
    ~OmemoMemoryStorage() override;

    QXmppTask<Data> allData() override;
    QXmppTask<void> setOwnDevice(const std::optional<OwnDevice> &device) override;
    QXmppTask<void> addSignedPreKey(const SignedPreKey &key) override;
    QXmppTask<void> removeSignedPreKey(uint32_t id) override;
    QXmppTask<void> addPreKeys(const QList<PreKey> &keys) override;
    QXmppTask<void> removePreKey(Omemo::Version version, uint32_t id) override;
    QXmppTask<void> addDevice(const Device &device) override;
    QXmppTask<void> removeDevice(Omemo::Version version, const QString &jid, uint32_t id) override;
    QXmppTask<void> removeDevices(const QString &jid) override;
    QXmppTask<void> addSubscription(const Subscription &subscription) override;
    QXmppTask<void> removeSubscription(const Subscription &subscription) override;
    QXmppTask<void> resetAll() override;

private:
    std::unique_ptr<Omemo::Private::OmemoMemoryStoragePrivate> d;
};

}  // namespace QXmpp

#endif  // QXMPPOMEMOMEMORYSTORAGE_H

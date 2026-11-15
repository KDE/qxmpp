// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPOMEMOMANAGER_H
#define QXMPPOMEMOMANAGER_H

#include "QXmppClientExtension.h"
#include "QXmppE2eeExtension.h"
#include "QXmppError.h"
#include "QXmppJid.h"
#include "QXmppOmemoStorage.h"
#include "QXmppTask.h"
#include "QXmppTrustLevel.h"
#include "QXmppTrustSecurityPolicy.h"
#include "qxmppomemo_export.h"

class QXmppOmemoDevicePrivate;
class QXmppOmemoManagerPrivate;
class QXmppOmemoOwnDevicePrivate;
class QXmppOmemoStorage;
class tst_OmemoManager2;

class QXMPPOMEMO_EXPORT QXmppOmemoOwnDevice
{
public:
    QXmppOmemoOwnDevice();
    QXmppOmemoOwnDevice(const QXmppOmemoOwnDevice &other);
    QXmppOmemoOwnDevice(QXmppOmemoOwnDevice &&) noexcept;
    ~QXmppOmemoOwnDevice();

    QXmppOmemoOwnDevice &operator=(const QXmppOmemoOwnDevice &);
    QXmppOmemoOwnDevice &operator=(QXmppOmemoOwnDevice &&);

    QString label() const;
    void setLabel(const QString &label);

    QByteArray keyId() const;
    void setKeyId(const QByteArray &keyId);

private:
    QSharedDataPointer<QXmppOmemoOwnDevicePrivate> d;
};

class QXMPPOMEMO_EXPORT QXmppOmemoDevice
{
public:
    QXmppOmemoDevice();
    QXmppOmemoDevice(const QXmppOmemoDevice &other);
    QXmppOmemoDevice(QXmppOmemoDevice &&) noexcept;
    ~QXmppOmemoDevice();

    QXmppOmemoDevice &operator=(const QXmppOmemoDevice &);
    QXmppOmemoDevice &operator=(QXmppOmemoDevice &&);

    QString jid() const;
    void setJid(const QString &jid);

    QString label() const;
    void setLabel(const QString &label);

    QByteArray keyId() const;
    void setKeyId(const QByteArray &keyId);

    QXmpp::TrustLevel trustLevel() const;
    void setTrustLevel(QXmpp::TrustLevel trustLevel);

private:
    QSharedDataPointer<QXmppOmemoDevicePrivate> d;
};

class QXMPPOMEMO_EXPORT QXmppOmemoManager : public QXmppClientExtension, public QXmppE2eeExtension
{
    Q_OBJECT

public:
    using Result = std::variant<QXmpp::Success, QXmppError>;

    /*!
        \struct QXmppOmemoManager::DevicesResult
        \inmodule QXmpp
        \brief Result of a per-JID OMEMO device list operation.
    */
    struct DevicesResult {
        QString jid;
        Result result;
    };

    explicit QXmppOmemoManager(QXmppOmemoStorage *omemoStorage);
    ~QXmppOmemoManager() override;

    QXmppTask<bool> load();
    QXmppTask<bool> setUp(QString deviceLabel = {});

    QXmppTask<QByteArray> ownKey();
    QXmppTask<QHash<QXmpp::TrustLevel, QMultiHash<QString, QByteArray>>> keys(QXmpp::TrustLevels trustLevels = {});
    QXmppTask<QHash<QString, QHash<QByteArray, QXmpp::TrustLevel>>> keys(const QList<QString> &jids, QXmpp::TrustLevels trustLevels = {});

    QXmppTask<bool> changeDeviceLabel(const QString &deviceLabel = {});

    int maximumDevicesPerJid() const;
    void setMaximumDevicesPerJid(int maximum);

    int maximumDevicesPerStanza() const;
    void setMaximumDevicesPerStanza(int maximum);

    QXmppTask<QList<DevicesResult>> requestDeviceLists(QList<QString> jids);
    QXmppTask<QList<DevicesResult>> subscribeToDeviceLists(QList<QString> jids);
    QXmppTask<QList<DevicesResult>> unsubscribeFromDeviceLists();

    QXmppOmemoOwnDevice ownDevice();
    QXmppTask<QList<QXmppOmemoDevice>> devices();
    QXmppTask<QList<QXmppOmemoDevice>> devices(QList<QString> jids);
    QXmppTask<Result> removeContactDevices(QString jid);

    void setAcceptedSessionBuildingTrustLevels(QXmpp::TrustLevels trustLevels);
    QXmpp::TrustLevels acceptedSessionBuildingTrustLevels();

    void setNewDeviceAutoSessionBuildingEnabled(bool isNewDeviceAutoSessionBuildingEnabled);
    bool isNewDeviceAutoSessionBuildingEnabled();

    QXmppTask<void> buildMissingSessions(QList<QString> jids);

    QXmppTask<bool> resetOwnDevice();
    QXmppTask<void> resetOwnDeviceLocally();
    QXmppTask<bool> resetAll();

    QXmppTask<void> setSecurityPolicy(QXmpp::TrustSecurityPolicy securityPolicy);
    QXmppTask<QXmpp::TrustSecurityPolicy> securityPolicy();

    QXmppTask<void> setTrustLevel(const QMultiHash<QString, QByteArray> &keyIds, QXmpp::TrustLevel trustLevel);
    QXmppTask<QXmpp::TrustLevel> trustLevel(const QString &keyOwnerJid, const QByteArray &keyId);

    QXmppTask<MessageEncryptResult> encryptMessage(QXmppMessage &&message, const std::optional<QXmppSendStanzaParams> &params) override;
    QXmppTask<MessageDecryptResult> decryptMessage(QXmppMessage &&message) override;

    QXmppTask<IqEncryptResult> encryptIq(QXmppIq &&iq, const std::optional<QXmppSendStanzaParams> &params) override;
    QXmppTask<IqDecryptResult> decryptIq(const QDomElement &element) override;

    bool isEncrypted(const QDomElement &) override;
    bool isEncrypted(const QXmppMessage &) override;

    Q_SIGNAL void trustLevelsChanged(const QMultiHash<QString, QByteArray> &modifiedKeys);

    Q_SIGNAL void deviceAdded(const QString &jid, uint32_t deviceId);
    Q_SIGNAL void deviceChanged(const QString &jid, uint32_t deviceId);
    Q_SIGNAL void deviceRemoved(const QString &jid, uint32_t deviceId);
    Q_SIGNAL void devicesRemoved(const QString &jid);
    Q_SIGNAL void allDevicesRemoved();

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    bool handleIncomingStanza(const QDomElement &stanza);
    QXmpp::HookResult handlePubSubEvent(const QDomElement &element, const QXmpp::Jid &pubSubService, const QString &nodeName);

    std::unique_ptr<QXmppOmemoManagerPrivate> d;

    friend class QXmppOmemoManagerPrivate;
    friend class tst_QXmppOmemoManager;
};

namespace QXmpp::Omemo::Private {
class OmemoManagerPrivate;
}

namespace QXmpp::Omemo {

inline constexpr QStringView Automatic = u"org.qxmpp.omemo-auto";

struct Device {
    Version version = Version::Omemo2;
    QXmpp::Jid jid {};
    uint32_t id = 0;
    QString label {};
    QByteArray keyId {};
    QXmpp::TrustLevel trustLevel = QXmpp::TrustLevel::Undecided;
    QByteArray omemo0KeyId {};

    bool hasKey(const QByteArray &key) const { return !key.isEmpty() && (key == keyId || key == omemo0KeyId); }

    bool operator==(const Device &) const = default;
};

struct EncryptionInfo {
    enum class SkipReason {
        Untrusted,
        NoSession,
        Unresponsive,
        DeviceLimit,
        BodyOnlyUnsupported,
    };

    struct SkippedDevice {
        Device device {};
        SkipReason reason = SkipReason::NoSession;

        bool operator==(const SkippedDevice &) const = default;
    };

    QList<Device> devices {};
    QList<SkippedDevice> skippedDevices {};
};

class QXMPPOMEMO_EXPORT DeviceWatch
{
public:
    enum class State {
        Loading,
        Stale,
        Loaded,
        Error,
    };

    DeviceWatch();

    QXmpp::Jid jid() const;
    QBindable<State> state() const;
    QBindable<QList<Device>> devices() const;
    QBindable<QList<Version>> supportedVersions() const;

private:
    friend class Private::OmemoManagerPrivate;
    struct Data;
    explicit DeviceWatch(std::shared_ptr<Data> d);

    std::shared_ptr<Data> d;
};

}  // namespace QXmpp::Omemo

namespace QXmpp {

class QXMPPOMEMO_EXPORT OmemoManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    enum class State {
        Loading,
        NotSetUp,
        SettingUp,
        Ready,
    };
    Q_ENUM(State)

    explicit OmemoManager(std::shared_ptr<OmemoStorage> storage, QList<Omemo::Version> versions = { Omemo::Version::Omemo0, Omemo::Version::Omemo2 });
    ~OmemoManager() override;

    QBindable<QList<Omemo::Version>> enabledVersions() const;
    void setEnabledVersions(QList<Omemo::Version> versions);

    QString deviceLabel() const;
    void setDeviceLabel(const QString &label);
    static QString defaultDeviceLabel();

    QBindable<State> state() const;
    QBindable<uint32_t> ownDeviceId() const;
    QBindable<QByteArray> ownKey(Omemo::Version version = Omemo::Version::Omemo2) const;

    Omemo::DeviceWatch watchDevices(const QXmpp::Jid &jid);
    QXmppTask<void> setTrustLevel(QList<Omemo::Device> devices, QXmpp::TrustLevel trustLevel);
    QXmppTask<void> removeContactDevices(QXmpp::Jid jid);

    QXmppTask<QXmpp::Result<>> resetOwnDevice();
    QXmppTask<void> resetOwnDeviceLocally();
    QXmppTask<QXmpp::Result<>> resetAll();

    Q_SIGNAL void errorOccurred(const QXmppError &error);

protected:
    void onRegistered(QXmppClient *client) override;
    void onUnregistered(QXmppClient *client) override;

private:
    QXmpp::HookResult handlePubSubEvent(const QDomElement &element, const QXmpp::Jid &service, const QString &node);

    std::unique_ptr<Omemo::Private::OmemoManagerPrivate> d;

    friend class Omemo::Private::OmemoManagerPrivate;
    friend class ::tst_OmemoManager2;
};

}  // namespace QXmpp

#endif  // QXMPPOMEMOMANAGER_H

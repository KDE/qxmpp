// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCFORMS_H
#define QXMPPMUCFORMS_H

#include "QXmppDataFormBase.h"

struct QXmppMucRoomInfoPrivate;

class QXMPP_EXPORT QXmppMucRoomInfo : public QXmppExtensibleDataFormBase
{
public:
    /// FORM_TYPE of this data form
    static constexpr QStringView DataFormType = QXmpp::Private::ns_muc_roominfo;
    static std::optional<QXmppMucRoomInfo> fromDataForm(const QXmppDataForm &);

    QXmppMucRoomInfo();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucRoomInfo)

    std::optional<bool> subjectChangeable() const;
    void setSubjectChangeable(std::optional<bool> newSubjectChangeable);

    QString subject() const;
    void setSubject(const QString &newSubject);

    std::optional<quint32> occupants() const;
    void setOccupants(std::optional<quint32> newOccupants);

    QString language() const;
    void setLanguage(const QString &newLanguage);

    QString description() const;
    void setDescription(const QString &newDescription);

    QStringList contactJids() const;
    void setContactJids(const QStringList &newContactJids);

    std::optional<quint32> maxHistoryFetch() const;
    void setMaxHistoryFetch(std::optional<quint32> newMaxHistoryFetch);

    QStringList avatarHashes() const;
    void setAvatarHashes(const QStringList &hashes);

    bool operator==(const QXmppMucRoomInfo &other) const;

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucRoomInfoPrivate> d;
};

struct QXmppMucVoiceRequestPrivate;

class QXMPP_EXPORT QXmppMucVoiceRequest : public QXmppExtensibleDataFormBase
{
public:
    /// FORM_TYPE of this data form
    static constexpr QStringView DataFormType = QXmpp::Private::ns_muc_request;
    static std::optional<QXmppMucVoiceRequest> fromDataForm(const QXmppDataForm &);

    QXmppMucVoiceRequest();
    QXMPP_PRIVATE_DECLARE_RULE_OF_SIX(QXmppMucVoiceRequest)

    QString jid() const;
    void setJid(const QString &jid);

    QString nick() const;
    void setNick(const QString &nick);

    std::optional<bool> requestAllow() const;
    void setRequestAllow(std::optional<bool> allow);

    QXmppDataForm toDataForm() const override;

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucVoiceRequestPrivate> d;
};

Q_DECLARE_METATYPE(QXmppMucVoiceRequest)

#endif  // QXMPPMUCFORMS_H

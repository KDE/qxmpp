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

protected:
    QString formType() const override;
    bool parseField(const QXmppDataForm::Field &) override;
    void serializeForm(QXmppDataForm &) const override;

private:
    QSharedDataPointer<QXmppMucRoomInfoPrivate> d;
};

#endif  // QXMPPMUCFORMS_H

// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPVCARDIQ_H
#define QXMPPVCARDIQ_H

#include "QXmppIq.h"

#include <QDate>
#include <QDomElement>
#include <QMap>

namespace QXmpp::Private {
struct VCardData;
}

class QXmppVCardAddressPrivate;
class QXmppVCardEmailPrivate;
class QXmppVCardPhonePrivate;
class QXmppVCardOrganizationPrivate;
class QXmppVCardIqPrivate;

/*!
    \inmodule QXmpp

    \brief Represent a vCard address.
*/

class QXMPP_EXPORT QXmppVCardAddress
{
public:
    /*!
        \brief Describes e-mail address types.

        \value None
        \value Home
        \value Work
        \value Postal
        \value Preferred
    */
    enum TypeFlag {
        None = 0x0,
        Home = 0x1,
        Work = 0x2,
        Postal = 0x4,
        Preferred = 0x8
    };
    Q_DECLARE_FLAGS(Type, TypeFlag)

    QXmppVCardAddress();
    QXmppVCardAddress(const QXmppVCardAddress &other);
    QXmppVCardAddress(QXmppVCardAddress &&);
    ~QXmppVCardAddress();

    QXmppVCardAddress &operator=(const QXmppVCardAddress &other);
    QXmppVCardAddress &operator=(QXmppVCardAddress &&);

    QString country() const;
    void setCountry(const QString &country);

    QString locality() const;
    void setLocality(const QString &locality);

    QString postcode() const;
    void setPostcode(const QString &postcode);

    QString region() const;
    void setRegion(const QString &region);

    QString street() const;
    void setStreet(const QString &street);

    Type type() const;
    void setType(Type type);

    static constexpr std::tuple XmlTag = { u"ADR", QXmpp::Private::ns_vcard };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *stream) const;

private:
    QSharedDataPointer<QXmppVCardAddressPrivate> d;
};

/*!
    \relates QXmppVCardAddress
    Returns \c true if \a left and \a right are equal.
*/
QXMPP_EXPORT bool operator==(const QXmppVCardAddress &left, const QXmppVCardAddress &right);
/*!
    \relates QXmppVCardAddress
    Returns \c true if \a left and \a right are not equal.
*/
QXMPP_EXPORT bool operator!=(const QXmppVCardAddress &left, const QXmppVCardAddress &right);

/*!
    \inmodule QXmpp

    \brief Represents a vCard e-mail address.
*/

class QXMPP_EXPORT QXmppVCardEmail
{
public:
    /*!
        \brief Describes e-mail address types.

        \value None
        \value Home
        \value Work
        \value Internet
        \value Preferred
        \value X400
    */
    enum TypeFlag {
        None = 0x0,
        Home = 0x1,
        Work = 0x2,
        Internet = 0x4,
        Preferred = 0x8,
        X400 = 0x10
    };
    Q_DECLARE_FLAGS(Type, TypeFlag)

    QXmppVCardEmail();
    QXmppVCardEmail(const QXmppVCardEmail &other);
    ~QXmppVCardEmail();

    QXmppVCardEmail &operator=(const QXmppVCardEmail &other);

    QString address() const;
    void setAddress(const QString &address);

    Type type() const;
    void setType(Type type);

    static constexpr std::tuple XmlTag = { u"EMAIL", QXmpp::Private::ns_vcard };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *stream) const;

private:
    QSharedDataPointer<QXmppVCardEmailPrivate> d;
};

/*!
    \relates QXmppVCardEmail
    Returns \c true if \a left and \a right are equal.
*/
QXMPP_EXPORT bool operator==(const QXmppVCardEmail &left, const QXmppVCardEmail &right);
/*!
    \relates QXmppVCardEmail
    Returns \c true if \a left and \a right are not equal.
*/
QXMPP_EXPORT bool operator!=(const QXmppVCardEmail &left, const QXmppVCardEmail &right);

/*!
    \inmodule QXmpp

    \brief Represents a vCard phone number.
*/

class QXMPP_EXPORT QXmppVCardPhone
{
public:
    /*!
        \brief Describes phone number types.

        \value None
        \value Home
        \value Work
        \value Voice
        \value Fax
        \value Pager
        \value Messaging
        \value Cell
        \value Video
        \value BBS
        \value Modem
        \value ISDN
        \value PCS
        \value Preferred
    */
    enum TypeFlag {
        None = 0x0,
        Home = 0x1,
        Work = 0x2,
        Voice = 0x4,
        Fax = 0x8,
        Pager = 0x10,
        Messaging = 0x20,
        Cell = 0x40,
        Video = 0x80,
        BBS = 0x100,
        Modem = 0x200,
        ISDN = 0x400,
        PCS = 0x800,
        Preferred = 0x1000
    };
    Q_DECLARE_FLAGS(Type, TypeFlag)

    QXmppVCardPhone();
    QXmppVCardPhone(const QXmppVCardPhone &other);
    ~QXmppVCardPhone();

    QXmppVCardPhone &operator=(const QXmppVCardPhone &other);

    QString number() const;
    void setNumber(const QString &number);

    Type type() const;
    void setType(Type type);

    static constexpr std::tuple XmlTag = { u"TEL", QXmpp::Private::ns_vcard };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *stream) const;

private:
    QSharedDataPointer<QXmppVCardPhonePrivate> d;
};

/*!
    \relates QXmppVCardPhone
    Returns \c true if \a left and \a right are equal.
*/
QXMPP_EXPORT bool operator==(const QXmppVCardPhone &left, const QXmppVCardPhone &right);
/*!
    \relates QXmppVCardPhone
    Returns \c true if \a left and \a right are not equal.
*/
QXMPP_EXPORT bool operator!=(const QXmppVCardPhone &left, const QXmppVCardPhone &right);

/*!
    \inmodule QXmpp

    \brief Represents organization information in XMPP vCards.

    This contains both information about organization itself and
    information about job position in the organization.
*/

class QXMPP_EXPORT QXmppVCardOrganization
{
public:
    QXmppVCardOrganization();
    QXmppVCardOrganization(const QXmppVCardOrganization &other);
    ~QXmppVCardOrganization();

    QXmppVCardOrganization &operator=(const QXmppVCardOrganization &other);

    QString organization() const;
    void setOrganization(const QString &);

    QString unit() const;
    void setUnit(const QString &);

    QString title() const;
    void setTitle(const QString &);

    QString role() const;
    void setRole(const QString &);

    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *stream) const;

private:
    QSharedDataPointer<QXmppVCardOrganizationPrivate> d;
};

/*!
    \relates QXmppVCardOrganization
    Returns \c true if \a left and \a right are equal.
*/
QXMPP_EXPORT bool operator==(const QXmppVCardOrganization &left, const QXmppVCardOrganization &right);
/*!
    \relates QXmppVCardOrganization
    Returns \c true if \a left and \a right are not equal.
*/
QXMPP_EXPORT bool operator!=(const QXmppVCardOrganization &left, const QXmppVCardOrganization &right);

/*!
    \inmodule QXmpp

    \brief Represents the XMPP vCard.

    The functions names are self explanatory.
    Look at QXmppVCardManager and \xep{0054}{vcard-temp} for more details.

    There are many field of XMPP vCard which are not present in
    this class. File a issue for the same. We will add the requested
    field to this class.
*/

class QXMPP_EXPORT QXmppVCardIq : public QXmppIq
{
public:
    QXmppVCardIq(const QString &bareJid = QString());
    QXmppVCardIq(const QXmppVCardIq &other);
    ~QXmppVCardIq() override;

    QXmppVCardIq &operator=(const QXmppVCardIq &other);

    QDate birthday() const;
    void setBirthday(const QDate &birthday);

    QString description() const;
    void setDescription(const QString &description);

    QString email() const;
    void setEmail(const QString &);

    QString firstName() const;
    void setFirstName(const QString &);

    QString fullName() const;
    void setFullName(const QString &);

    QString lastName() const;
    void setLastName(const QString &);

    QString middleName() const;
    void setMiddleName(const QString &);

    QString nickName() const;
    void setNickName(const QString &);

    QByteArray photo() const;
    void setPhoto(const QByteArray &);

    QString photoType() const;
    void setPhotoType(const QString &type);

    QString url() const;
    void setUrl(const QString &);

    QList<QXmppVCardAddress> addresses() const;
    void setAddresses(const QList<QXmppVCardAddress> &addresses);

    QList<QXmppVCardEmail> emails() const;
    void setEmails(const QList<QXmppVCardEmail> &emails);

    QList<QXmppVCardPhone> phones() const;
    void setPhones(const QList<QXmppVCardPhone> &phones);

    QXmppVCardOrganization organization() const;
    void setOrganization(const QXmppVCardOrganization &);

    static constexpr std::tuple PayloadXmlTag = { u"vCard", QXmpp::Private::ns_vcard };
    [[deprecated("Use QXmpp::isIqElement()")]]
    static bool isVCard(const QDomElement &element);
    [[deprecated]]
    static bool checkIqType(const QString &tagName, const QString &xmlNamespace);

protected:
    friend struct QXmpp::Private::VCardData;

    void parseElementFromChild(const QDomElement &) override;
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override;

private:
    QSharedDataPointer<QXmppVCardIqPrivate> d;
};

/*!
    \relates QXmppVCardIq
    Returns \c true if \a left and \a right are equal.
*/
QXMPP_EXPORT bool operator==(const QXmppVCardIq &left, const QXmppVCardIq &right);
/*!
    \relates QXmppVCardIq
    Returns \c true if \a left and \a right are not equal.
*/
QXMPP_EXPORT bool operator!=(const QXmppVCardIq &left, const QXmppVCardIq &right);

#endif  // QXMPPVCARDIQ_H

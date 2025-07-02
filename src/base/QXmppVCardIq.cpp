// SPDX-FileCopyrightText: 2010 Manjeet Dahiya <manjeetdahiya@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppVCardIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QBuffer>
#include <QXmlStreamWriter>

using namespace QXmpp::Private;

static QString getImageType(const QByteArray &contents)
{
    if (contents.startsWith("\x89PNG\x0d\x0a\x1a\x0a")) {
        return u"image/png"_s;
    } else if (contents.startsWith("\x8aMNG")) {
        return u"video/x-mng"_s;
    } else if (contents.startsWith("GIF8")) {
        return u"image/gif"_s;
    } else if (contents.startsWith("BM")) {
        return u"image/bmp"_s;
    } else if (contents.contains("/* XPM */")) {
        return u"image/x-xpm"_s;
    } else if (contents.contains("<?xml") && contents.contains("<svg")) {
        return u"image/svg+xml"_s;
    } else if (contents.startsWith("\xFF\xD8\xFF\xE0")) {
        return u"image/jpeg"_s;
    }
    return u"image/unknown"_s;
}

struct VCardDate {
    QDate date;
};

template<>
struct QXmpp::Private::StringSerializer<VCardDate> {
    static auto serialize(auto v) { return v.date.toString(u"yyyy-MM-dd"_s); }
    static auto hasValue(auto v) { return v.date.isValid(); }
};

struct VCardPhoto {
    const QByteArray &data;
    const QString &type;

    void toXml(XmlWriter &w) const
    {
        if (!data.isEmpty()) {
            w.write(Element {
                u"PHOTO",
                TextElement { u"TYPE", type.isEmpty() ? getImageType(data) : type },
                TextElement { u"BINVAL", Base64 { data } },
            });
        }
    }
};

class QXmppVCardAddressPrivate : public QSharedData
{
public:
    QXmppVCardAddressPrivate() : type(QXmppVCardAddress::None) { };
    QString country;
    QString locality;
    QString postcode;
    QString region;
    QString street;
    QXmppVCardAddress::Type type;
};

/// Constructs an empty address.
QXmppVCardAddress::QXmppVCardAddress()
    : d(new QXmppVCardAddressPrivate)
{
}

/// Copy-constructor
QXmppVCardAddress::QXmppVCardAddress(const QXmppVCardAddress &other) = default;
/// Move-constructor
QXmppVCardAddress::QXmppVCardAddress(QXmppVCardAddress &&) = default;
QXmppVCardAddress::~QXmppVCardAddress() = default;
/// Assignment operator.
QXmppVCardAddress &QXmppVCardAddress::operator=(const QXmppVCardAddress &other) = default;
/// Move-assignment operator.
QXmppVCardAddress &QXmppVCardAddress::operator=(QXmppVCardAddress &&) = default;

/// \brief Checks if two address objects represent the same address.
bool operator==(const QXmppVCardAddress &left, const QXmppVCardAddress &right)
{
    return left.type() == right.type() &&
        left.country() == right.country() &&
        left.locality() == right.locality() &&
        left.postcode() == right.postcode() &&
        left.region() == right.region() &&
        left.street() == right.street();
}

/// \brief Checks if two address objects represent different addresses.
bool operator!=(const QXmppVCardAddress &left, const QXmppVCardAddress &right)
{
    return !(left == right);
}

/// Returns the country.
QString QXmppVCardAddress::country() const
{
    return d->country;
}

/// Sets the country.
void QXmppVCardAddress::setCountry(const QString &country)
{
    d->country = country;
}

/// Returns the locality.
QString QXmppVCardAddress::locality() const
{
    return d->locality;
}

/// Sets the locality.
void QXmppVCardAddress::setLocality(const QString &locality)
{
    d->locality = locality;
}

/// Returns the postcode.

QString QXmppVCardAddress::postcode() const
{
    return d->postcode;
}

/// Sets the postcode.
void QXmppVCardAddress::setPostcode(const QString &postcode)
{
    d->postcode = postcode;
}

/// Returns the region.
QString QXmppVCardAddress::region() const
{
    return d->region;
}

/// Sets the region.
void QXmppVCardAddress::setRegion(const QString &region)
{
    d->region = region;
}

/// Returns the street address.
QString QXmppVCardAddress::street() const
{
    return d->street;
}

/// Sets the street address.
void QXmppVCardAddress::setStreet(const QString &street)
{
    d->street = street;
}

/// Returns the address type, which is a combination of TypeFlag.
QXmppVCardAddress::Type QXmppVCardAddress::type() const
{
    return d->type;
}

/// Sets the address \a type, which is a combination of TypeFlag.
void QXmppVCardAddress::setType(QXmppVCardAddress::Type type)
{
    d->type = type;
}

/// \cond
void QXmppVCardAddress::parse(const QDomElement &element)
{
    if (!element.firstChildElement(u"HOME"_s).isNull()) {
        d->type |= Home;
    }
    if (!element.firstChildElement(u"WORK"_s).isNull()) {
        d->type |= Work;
    }
    if (!element.firstChildElement(u"POSTAL"_s).isNull()) {
        d->type |= Postal;
    }
    if (!element.firstChildElement(u"PREF"_s).isNull()) {
        d->type |= Preferred;
    }

    d->country = element.firstChildElement(u"CTRY"_s).text();
    d->locality = element.firstChildElement(u"LOCALITY"_s).text();
    d->postcode = element.firstChildElement(u"PCODE"_s).text();
    d->region = element.firstChildElement(u"REGION"_s).text();
    d->street = element.firstChildElement(u"STREET"_s).text();
}

void QXmppVCardAddress::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"ADR",
        OptionalContent { d->type & Home, Element { u"HOME" } },
        OptionalContent { d->type & Work, Element { u"WORK" } },
        OptionalContent { d->type & Postal, Element { u"POSTAL" } },
        OptionalContent { d->type & Preferred, Element { u"PREF" } },
        OptionalTextElement { u"CTRY", d->country },
        OptionalTextElement { u"LOCALITY", d->locality },
        OptionalTextElement { u"PCODE", d->postcode },
        OptionalTextElement { u"REGION", d->region },
        OptionalTextElement { u"STREET", d->street },
    });
}
/// \endcond

class QXmppVCardEmailPrivate : public QSharedData
{
public:
    QXmppVCardEmailPrivate() : type(QXmppVCardEmail::None) { };
    QString address;
    QXmppVCardEmail::Type type;
};

/// Constructs an empty e-mail address.
QXmppVCardEmail::QXmppVCardEmail()
    : d(new QXmppVCardEmailPrivate)
{
}

/// Copy-constructor
QXmppVCardEmail::QXmppVCardEmail(const QXmppVCardEmail &) = default;

QXmppVCardEmail::~QXmppVCardEmail() = default;

/// Copy-assignment operator.
QXmppVCardEmail &QXmppVCardEmail::operator=(const QXmppVCardEmail &other) = default;

/// \brief Checks if two email objects represent the same email address.
bool operator==(const QXmppVCardEmail &left, const QXmppVCardEmail &right)
{
    return left.type() == right.type() &&
        left.address() == right.address();
}

/// \brief Checks if two email objects represent different email addresses.
bool operator!=(const QXmppVCardEmail &left, const QXmppVCardEmail &right)
{
    return !(left == right);
}

/// Returns the e-mail address.
QString QXmppVCardEmail::address() const
{
    return d->address;
}

/// Sets the e-mail \a address.
void QXmppVCardEmail::setAddress(const QString &address)
{
    d->address = address;
}

/// Returns the e-mail type, which is a combination of TypeFlag.
QXmppVCardEmail::Type QXmppVCardEmail::type() const
{
    return d->type;
}

/// Sets the e-mail \a type, which is a combination of TypeFlag.
void QXmppVCardEmail::setType(QXmppVCardEmail::Type type)
{
    d->type = type;
}

/// \cond
void QXmppVCardEmail::parse(const QDomElement &element)
{
    if (!element.firstChildElement(u"HOME"_s).isNull()) {
        d->type |= Home;
    }
    if (!element.firstChildElement(u"WORK"_s).isNull()) {
        d->type |= Work;
    }
    if (!element.firstChildElement(u"INTERNET"_s).isNull()) {
        d->type |= Internet;
    }
    if (!element.firstChildElement(u"PREF"_s).isNull()) {
        d->type |= Preferred;
    }
    if (!element.firstChildElement(u"X400"_s).isNull()) {
        d->type |= X400;
    }
    d->address = element.firstChildElement(u"USERID"_s).text();
}

void QXmppVCardEmail::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"EMAIL",
        OptionalContent { d->type & Home, Element { u"HOME" } },
        OptionalContent { d->type & Work, Element { u"WORK" } },
        OptionalContent { d->type & Internet, Element { u"INTERNET" } },
        OptionalContent { d->type & Preferred, Element { u"PREF" } },
        OptionalContent { d->type & X400, Element { u"X400" } },
        TextElement { u"USERID", d->address },
    });
}
/// \endcond

class QXmppVCardPhonePrivate : public QSharedData
{
public:
    QXmppVCardPhonePrivate() : type(QXmppVCardPhone::None) { };
    QString number;
    QXmppVCardPhone::Type type;
};

/// Constructs an empty phone number.
QXmppVCardPhone::QXmppVCardPhone()
    : d(new QXmppVCardPhonePrivate)
{
}

/// Copy-constructor
QXmppVCardPhone::QXmppVCardPhone(const QXmppVCardPhone &other) = default;

QXmppVCardPhone::~QXmppVCardPhone() = default;

/// Copy-assignment operator
QXmppVCardPhone &QXmppVCardPhone::operator=(const QXmppVCardPhone &other) = default;

/// Returns the phone number.
QString QXmppVCardPhone::number() const
{
    return d->number;
}

/// \brief Checks if two phone objects represent the same phone number.
bool operator==(const QXmppVCardPhone &left, const QXmppVCardPhone &right)
{
    return left.type() == right.type() &&
        left.number() == right.number();
}

/// \brief Checks if two phone objects represent different phone numbers.
bool operator!=(const QXmppVCardPhone &left, const QXmppVCardPhone &right)
{
    return !(left == right);
}

/// Sets the phone \a number.
void QXmppVCardPhone::setNumber(const QString &number)
{
    d->number = number;
}

/// Returns the phone number type, which is a combination of TypeFlag.
QXmppVCardPhone::Type QXmppVCardPhone::type() const
{
    return d->type;
}

/// Sets the phone number \a type, which is a combination of TypeFlag.
void QXmppVCardPhone::setType(QXmppVCardPhone::Type type)
{
    d->type = type;
}

/// \cond
void QXmppVCardPhone::parse(const QDomElement &element)
{
    if (!element.firstChildElement(u"HOME"_s).isNull()) {
        d->type |= Home;
    }
    if (!element.firstChildElement(u"WORK"_s).isNull()) {
        d->type |= Work;
    }
    if (!element.firstChildElement(u"VOICE"_s).isNull()) {
        d->type |= Voice;
    }
    if (!element.firstChildElement(u"FAX"_s).isNull()) {
        d->type |= Fax;
    }
    if (!element.firstChildElement(u"PAGER"_s).isNull()) {
        d->type |= Pager;
    }
    if (!element.firstChildElement(u"MSG"_s).isNull()) {
        d->type |= Messaging;
    }
    if (!element.firstChildElement(u"CELL"_s).isNull()) {
        d->type |= Cell;
    }
    if (!element.firstChildElement(u"VIDEO"_s).isNull()) {
        d->type |= Video;
    }
    if (!element.firstChildElement(u"BBS"_s).isNull()) {
        d->type |= BBS;
    }
    if (!element.firstChildElement(u"MODEM"_s).isNull()) {
        d->type |= Modem;
    }
    if (!element.firstChildElement(u"ISDN"_s).isNull()) {
        d->type |= ISDN;
    }
    if (!element.firstChildElement(u"PCS"_s).isNull()) {
        d->type |= PCS;
    }
    if (!element.firstChildElement(u"PREF"_s).isNull()) {
        d->type |= Preferred;
    }
    d->number = element.firstChildElement(u"NUMBER"_s).text();
}

void QXmppVCardPhone::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"TEL",
        OptionalContent { d->type & Home, Element { u"HOME" } },
        OptionalContent { d->type & Work, Element { u"WORK" } },
        OptionalContent { d->type & Voice, Element { u"VOICE" } },
        OptionalContent { d->type & Fax, Element { u"FAX" } },
        OptionalContent { d->type & Pager, Element { u"PAGER" } },
        OptionalContent { d->type & Messaging, Element { u"MSG" } },
        OptionalContent { d->type & Cell, Element { u"CELL" } },
        OptionalContent { d->type & Video, Element { u"VIDEO" } },
        OptionalContent { d->type & BBS, Element { u"BBS" } },
        OptionalContent { d->type & Modem, Element { u"MODEM" } },
        OptionalContent { d->type & ISDN, Element { u"ISDN" } },
        OptionalContent { d->type & PCS, Element { u"PCS" } },
        OptionalContent { d->type & Preferred, Element { u"PREF" } },
        TextElement { u"NUMBER", d->number },
    });
}
/// \endcond

class QXmppVCardOrganizationPrivate : public QSharedData
{
public:
    QString organization;
    QString unit;
    QString role;
    QString title;
};

/// Constructs an empty organization information.
QXmppVCardOrganization::QXmppVCardOrganization()
    : d(new QXmppVCardOrganizationPrivate)
{
}

/// Constructs a copy of \a other.
QXmppVCardOrganization::QXmppVCardOrganization(const QXmppVCardOrganization &other)
    : d(other.d)
{
}

QXmppVCardOrganization::~QXmppVCardOrganization() = default;

/// Assigns \a other to this organization info.
QXmppVCardOrganization &QXmppVCardOrganization::operator=(const QXmppVCardOrganization &other)
{
    d = other.d;
    return *this;
}

/// \brief Checks if two organization objects represent the same organization.
bool operator==(const QXmppVCardOrganization &left, const QXmppVCardOrganization &right)
{
    return left.organization() == right.organization() &&
        left.unit() == right.unit() &&
        left.title() == right.title() &&
        left.role() == right.role();
}

/// \brief Checks if two organization objects represent different organizations.
bool operator!=(const QXmppVCardOrganization &left, const QXmppVCardOrganization &right)
{
    return !(left == right);
}

/// Returns the name of the organization.
QString QXmppVCardOrganization::organization() const
{
    return d->organization;
}

/// Sets the organization \a name.
void QXmppVCardOrganization::setOrganization(const QString &name)
{
    d->organization = name;
}

/// Returns the organization unit (also known as department).
QString QXmppVCardOrganization::unit() const
{
    return d->unit;
}

/// Sets the \a unit within the organization.
void QXmppVCardOrganization::setUnit(const QString &unit)
{
    d->unit = unit;
}

/// Returns the job role within the organization.
QString QXmppVCardOrganization::role() const
{
    return d->role;
}

/// Sets the job \a role within the organization.
void QXmppVCardOrganization::setRole(const QString &role)
{
    d->role = role;
}

/// Returns the job title within the organization.
QString QXmppVCardOrganization::title() const
{
    return d->title;
}

/// Sets the job \a title within the organization.
void QXmppVCardOrganization::setTitle(const QString &title)
{
    d->title = title;
}

/// \cond
void QXmppVCardOrganization::parse(const QDomElement &cardElem)
{
    d->title = cardElem.firstChildElement(u"TITLE"_s).text();
    d->role = cardElem.firstChildElement(u"ROLE"_s).text();

    const QDomElement &orgElem = cardElem.firstChildElement(u"ORG"_s);
    d->organization = orgElem.firstChildElement(u"ORGNAME"_s).text();
    d->unit = orgElem.firstChildElement(u"ORGUNIT"_s).text();
}

void QXmppVCardOrganization::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter w(writer);

    if (!d->unit.isEmpty() || !d->organization.isEmpty()) {
        w.write(Element {
            u"ORG",
            TextElement { u"ORGNAME", d->organization },
            TextElement { u"ORGUNIT", d->unit },
        });
    }

    w.write(TextElement { u"TITLE", d->title });
    w.write(TextElement { u"ROLE", d->role });
}
/// \endcond

class QXmppVCardIqPrivate : public QSharedData
{
public:
    QDate birthday;
    QString description;
    QString firstName;
    QString fullName;
    QString lastName;
    QString middleName;
    QString nickName;
    QString url;

    // not as 64 base
    QByteArray photo;
    QString photoType;

    QList<QXmppVCardAddress> addresses;
    QList<QXmppVCardEmail> emails;
    QList<QXmppVCardPhone> phones;
    QXmppVCardOrganization organization;
};

/// Constructs a QXmppVCardIq for the specified recipient.
QXmppVCardIq::QXmppVCardIq(const QString &jid)
    : QXmppIq(), d(new QXmppVCardIqPrivate)
{
    // for self jid should be empty
    setTo(jid);
}

/// Constructs a copy of \a other.
QXmppVCardIq::QXmppVCardIq(const QXmppVCardIq &other)
    : QXmppIq(other), d(other.d)
{
}

QXmppVCardIq::~QXmppVCardIq()
{
}

/// Assigns \a other to this vCard IQ.
QXmppVCardIq &QXmppVCardIq::operator=(const QXmppVCardIq &other)
{
    QXmppIq::operator=(other);
    d = other.d;
    return *this;
}

/// \brief Checks if two VCard objects represent the same VCard.
bool operator==(const QXmppVCardIq &left, const QXmppVCardIq &right)
{
    return left.birthday() == right.birthday() &&
        left.description() == right.description() &&
        left.firstName() == right.firstName() &&
        left.fullName() == right.fullName() &&
        left.lastName() == right.lastName() &&
        left.middleName() == right.middleName() &&
        left.nickName() == right.nickName() &&
        left.photo() == right.photo() &&
        left.photoType() == right.photoType() &&
        left.url() == right.url() &&
        left.addresses() == right.addresses() &&
        left.emails() == right.emails() &&
        left.phones() == right.phones() &&
        left.organization() == right.organization();
}

/// \brief Checks if two VCard objects represent different VCards.
bool operator!=(const QXmppVCardIq &left, const QXmppVCardIq &right)
{
    return !(left == right);
}

/// Returns the date of birth of the individual associated with the vCard.
QDate QXmppVCardIq::birthday() const
{
    return d->birthday;
}

/// Sets the date of birth of the individual associated with the vCard.
void QXmppVCardIq::setBirthday(const QDate &birthday)
{
    d->birthday = birthday;
}

/// Returns the free-form descriptive text.
QString QXmppVCardIq::description() const
{
    return d->description;
}

/// Sets the free-form descriptive text.
void QXmppVCardIq::setDescription(const QString &description)
{
    d->description = description;
}

/// Returns the email address.
QString QXmppVCardIq::email() const
{
    if (d->emails.isEmpty()) {
        return QString();
    } else {
        return d->emails.first().address();
    }
}

/// Sets the email address.
void QXmppVCardIq::setEmail(const QString &email)
{
    QXmppVCardEmail first;
    first.setAddress(email);
    first.setType(QXmppVCardEmail::Internet);
    d->emails = QList<QXmppVCardEmail>() << first;
}

/// Returns the first name.
QString QXmppVCardIq::firstName() const
{
    return d->firstName;
}

/// Sets the first name.
void QXmppVCardIq::setFirstName(const QString &firstName)
{
    d->firstName = firstName;
}

/// Returns the full name.
QString QXmppVCardIq::fullName() const
{
    return d->fullName;
}

/// Sets the full name.
void QXmppVCardIq::setFullName(const QString &fullName)
{
    d->fullName = fullName;
}

/// Returns the last name.
QString QXmppVCardIq::lastName() const
{
    return d->lastName;
}

/// Sets the last name.
void QXmppVCardIq::setLastName(const QString &lastName)
{
    d->lastName = lastName;
}

/// Returns the middle name.
QString QXmppVCardIq::middleName() const
{
    return d->middleName;
}

/// Sets the middle name.
void QXmppVCardIq::setMiddleName(const QString &middleName)
{
    d->middleName = middleName;
}

/// Returns the nickname.
QString QXmppVCardIq::nickName() const
{
    return d->nickName;
}

/// Sets the nickname.
void QXmppVCardIq::setNickName(const QString &nickName)
{
    d->nickName = nickName;
}

///
/// Returns the URL associated with the vCard. It can represent the user's
/// homepage or a location at which you can find real-time information about
/// the vCard.
///
QString QXmppVCardIq::url() const
{
    return d->url;
}

///
/// Sets the URL associated with the vCard. It can represent the user's
/// homepage or a location at which you can find real-time information about
/// the vCard.
///
void QXmppVCardIq::setUrl(const QString &url)
{
    d->url = url;
}

///
/// Returns the photo's binary contents.
///
/// If you want to use the photo as a QImage you can use:
///
/// \code
/// QBuffer buffer;
/// buffer.setData(myCard.photo());
/// buffer.open(QIODevice::ReadOnly);
/// QImageReader imageReader(&buffer);
/// QImage myImage = imageReader.read();
/// \endcode
///
QByteArray QXmppVCardIq::photo() const
{
    return d->photo;
}

/// Sets the photo's binary contents.
void QXmppVCardIq::setPhoto(const QByteArray &photo)
{
    d->photo = photo;
}

/// Returns the photo's MIME type.
QString QXmppVCardIq::photoType() const
{
    return d->photoType;
}

/// Sets the photo's MIME type.
void QXmppVCardIq::setPhotoType(const QString &photoType)
{
    d->photoType = photoType;
}

/// Returns the addresses.
QList<QXmppVCardAddress> QXmppVCardIq::addresses() const
{
    return d->addresses;
}

/// Sets the addresses.
void QXmppVCardIq::setAddresses(const QList<QXmppVCardAddress> &addresses)
{
    d->addresses = addresses;
}

/// Returns the e-mail addresses.
QList<QXmppVCardEmail> QXmppVCardIq::emails() const
{
    return d->emails;
}

/// Sets the e-mail addresses.
void QXmppVCardIq::setEmails(const QList<QXmppVCardEmail> &emails)
{
    d->emails = emails;
}

/// Returns the phone numbers.
QList<QXmppVCardPhone> QXmppVCardIq::phones() const
{
    return d->phones;
}

/// Sets the phone numbers.
void QXmppVCardIq::setPhones(const QList<QXmppVCardPhone> &phones)
{
    d->phones = phones;
}

/// Returns the organization info.
QXmppVCardOrganization QXmppVCardIq::organization() const
{
    return d->organization;
}

/// Sets the organization info.
void QXmppVCardIq::setOrganization(const QXmppVCardOrganization &org)
{
    d->organization = org;
}

/// \cond
void QXmppVCardIq::parseElementFromChild(const QDomElement &nodeRecv)
{
    // vCard
    QDomElement cardElement = nodeRecv.firstChildElement(u"vCard"_s);
    d->birthday = QDate::fromString(cardElement.firstChildElement(u"BDAY"_s).text(), u"yyyy-MM-dd"_s);
    d->description = cardElement.firstChildElement(u"DESC"_s).text();
    d->fullName = cardElement.firstChildElement(u"FN"_s).text();
    d->nickName = cardElement.firstChildElement(u"NICKNAME"_s).text();
    QDomElement nameElement = cardElement.firstChildElement(u"N"_s);
    d->firstName = nameElement.firstChildElement(u"GIVEN"_s).text();
    d->lastName = nameElement.firstChildElement(u"FAMILY"_s).text();
    d->middleName = nameElement.firstChildElement(u"MIDDLE"_s).text();
    d->url = cardElement.firstChildElement(u"URL"_s).text();
    QDomElement photoElement = cardElement.firstChildElement(u"PHOTO"_s);
    QByteArray base64data = photoElement.firstChildElement(u"BINVAL"_s).text().toLatin1();
    d->photo = QByteArray::fromBase64(base64data);
    d->photoType = photoElement.firstChildElement(u"TYPE"_s).text();
    d->addresses = parseChildElements<QList<QXmppVCardAddress>>(cardElement);
    d->emails = parseChildElements<QList<QXmppVCardEmail>>(cardElement);
    d->phones = parseChildElements<QList<QXmppVCardPhone>>(cardElement);
    d->organization.parse(cardElement);
}

void QXmppVCardIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        d->addresses,
        OptionalTextElement { u"BDAY", VCardDate { d->birthday } },
        OptionalTextElement { u"DESC", d->description },
        d->emails,
        OptionalTextElement { u"FN", d->fullName },
        OptionalTextElement { u"NICKNAME", d->nickName },
        OptionalContent {
            !d->firstName.isEmpty() || !d->lastName.isEmpty() || !d->middleName.isEmpty(),
            Element {
                u"N",
                OptionalTextElement { u"GIVEN", d->firstName },
                OptionalTextElement { u"FAMILY", d->lastName },
                OptionalTextElement { u"MIDDLE", d->middleName },
            },
        },
        d->phones,
        VCardPhoto { d->photo, d->photoType },
        OptionalTextElement { u"URL", d->url },
        d->organization,
    });
}
/// \endcond

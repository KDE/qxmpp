// SPDX-FileCopyrightText: 2012 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppRegisterIq.h"

#include "QXmppBitsOfBinaryDataList.h"
#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QSharedData>

using namespace QXmpp::Private;

class QXmppRegisterIqPrivate : public QSharedData
{
public:
    QXmppRegisterIqPrivate();

    QXmppDataForm form;
    QString email;
    QString instructions;
    QString password;
    QString username;
    bool isRegistered;
    bool isRemove;
    QXmppBitsOfBinaryDataList bitsOfBinaryData;
    QString outOfBandUrl;
};

QXmppRegisterIqPrivate::QXmppRegisterIqPrivate()
    : isRegistered(false),
      isRemove(false)
{
}

QXmppRegisterIq::QXmppRegisterIq()
    : d(new QXmppRegisterIqPrivate)
{
}

/// Default copy-constructor
QXmppRegisterIq::QXmppRegisterIq(const QXmppRegisterIq &other) = default;
/// Default move-constructor
QXmppRegisterIq::QXmppRegisterIq(QXmppRegisterIq &&) = default;
QXmppRegisterIq::~QXmppRegisterIq() = default;
/// Default assignment operator
QXmppRegisterIq &QXmppRegisterIq::operator=(const QXmppRegisterIq &other) = default;
/// Default move-assignment operator
QXmppRegisterIq &QXmppRegisterIq::operator=(QXmppRegisterIq &&) = default;

///
/// Constructs a regular change password request.
///
/// \param username The username of the account of which the password should be
/// changed.
/// \param newPassword The new password that should be set.
/// \param to Optional JID of the registration service. If this is omitted, the
/// IQ is automatically addressed to the local server.
///
/// \since QXmpp 1.2
///
QXmppRegisterIq QXmppRegisterIq::createChangePasswordRequest(const QString &username, const QString &newPassword, const QString &to)
{
    QXmppRegisterIq iq;
    iq.setType(QXmppIq::Set);
    iq.setTo(to);
    iq.setUsername(username);
    iq.setPassword(newPassword);
    return iq;
}

///
/// Constructs a regular unregistration request.
///
/// \param to Optional JID of the registration service. If this is omitted, the
/// IQ is automatically addressed to the local server.
///
/// \since QXmpp 1.2
///
QXmppRegisterIq QXmppRegisterIq::createUnregistrationRequest(const QString &to)
{
    QXmppRegisterIq iq;
    iq.setType(QXmppIq::Set);
    iq.setTo(to);
    iq.setIsRemove(true);
    return iq;
}

/// Returns the email for this registration IQ.
QString QXmppRegisterIq::email() const
{
    return d->email;
}

/// Sets the \a email for this registration IQ.
void QXmppRegisterIq::setEmail(const QString &email)
{
    d->email = email;
}

/// Returns the QXmppDataForm for this registration IQ.
QXmppDataForm QXmppRegisterIq::form() const
{
    return d->form;
}

/// Sets the QXmppDataForm for this registration IQ.
void QXmppRegisterIq::setForm(const QXmppDataForm &form)
{
    d->form = form;
}

/// Returns the instructions for this registration IQ.
QString QXmppRegisterIq::instructions() const
{
    return d->instructions;
}

/// Sets the \a instructions for this registration IQ.
void QXmppRegisterIq::setInstructions(const QString &instructions)
{
    d->instructions = instructions;
}

/// Returns the password for this registration IQ.
QString QXmppRegisterIq::password() const
{
    return d->password;
}

/// Sets the \a password for this registration IQ.
void QXmppRegisterIq::setPassword(const QString &password)
{
    d->password = password;
}

/// Returns the username for this registration IQ.
QString QXmppRegisterIq::username() const
{
    return d->username;
}

/// Sets the \a username for this registration IQ.
void QXmppRegisterIq::setUsername(const QString &username)
{
    d->username = username;
}

///
/// Returns whether the account is registered.
///
/// By default this is set to false.
///
/// \since QXmpp 1.2
///
bool QXmppRegisterIq::isRegistered() const
{
    return d->isRegistered;
}

///
/// Sets whether the account is registered.
///
/// By default this is set to false.
///
/// \since QXmpp 1.2
///
void QXmppRegisterIq::setIsRegistered(bool isRegistered)
{
    d->isRegistered = isRegistered;
}

///
/// Returns whether to remove (unregister) the account.
///
/// By default this is set to false.
///
/// \since QXmpp 1.2
///
bool QXmppRegisterIq::isRemove() const
{
    return d->isRemove;
}

///
/// Sets whether to remove (unregister) the account.
///
/// By default this is set to false.
///
/// \since QXmpp 1.2
///
void QXmppRegisterIq::setIsRemove(bool isRemove)
{
    d->isRemove = isRemove;
}

///
/// Returns a list of data packages attached using \xep{0231, Bits of Binary}.
///
/// This could be used to resolve a \c cid: URL of an CAPTCHA field of the
/// form.
///
/// \since QXmpp 1.2
///
QXmppBitsOfBinaryDataList QXmppRegisterIq::bitsOfBinaryData() const
{
    return d->bitsOfBinaryData;
}

///
/// Returns a list of data attached using \xep{0231, Bits of Binary}.
///
/// This could be used to resolve a \c cid: URL of an CAPTCHA field of the
/// form.
///
/// \since QXmpp 1.2
///
QXmppBitsOfBinaryDataList &QXmppRegisterIq::bitsOfBinaryData()
{
    return d->bitsOfBinaryData;
}

///
/// Sets a list of \xep{0231, Bits of Binary} attachments to be included.
///
/// \since QXmpp 1.2
///
void QXmppRegisterIq::setBitsOfBinaryData(const QXmppBitsOfBinaryDataList &bitsOfBinaryData)
{
    d->bitsOfBinaryData = bitsOfBinaryData;
}

///
/// Returns a \xep{0066, Out of Band Data} URL used for out-of-band registration.
///
/// \since QXmpp 1.5
///
QString QXmppRegisterIq::outOfBandUrl() const
{
    return d->outOfBandUrl;
}

///
/// Sets a \xep{0066, Out of Band Data} URL used for out-of-band registration.
///
/// \since QXmpp 1.5
///
void QXmppRegisterIq::setOutOfBandUrl(const QString &outOfBandUrl)
{
    d->outOfBandUrl = outOfBandUrl;
}

/// \cond
void QXmppRegisterIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement queryElement = element.firstChildElement(u"query"_s);
    d->instructions = queryElement.firstChildElement(u"instructions"_s).text();
    d->username = queryElement.firstChildElement(u"username"_s).text();
    d->password = queryElement.firstChildElement(u"password"_s).text();
    d->email = queryElement.firstChildElement(u"email"_s).text();

    if (auto formEl = firstChildElement(queryElement, u"x", ns_data); !formEl.isNull()) {
        d->form.parse(formEl);
    }
    if (auto oobEl = firstChildElement(queryElement, u"x", ns_oob); !oobEl.isNull()) {
        d->outOfBandUrl = oobEl.firstChildElement(u"url"_s).text();
    }

    d->isRegistered = !queryElement.firstChildElement(u"registered"_s).isNull();
    d->isRemove = !queryElement.firstChildElement(u"remove"_s).isNull();
    d->bitsOfBinaryData.parse(queryElement);
}

void QXmppRegisterIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        OptionalTextElement { u"instructions", d->instructions },
        OptionalContent { d->isRegistered, Element { u"registered" } },
        OptionalContent { d->isRemove, Element { u"remove" } },
        OptionalContent { !d->username.isNull(), TextElement { u"username", d->username } },
        OptionalContent { !d->password.isNull(), TextElement { u"password", d->password } },
        OptionalContent { !d->email.isNull(), TextElement { u"email", d->email } },
        d->form,
        d->bitsOfBinaryData,
        OptionalContent {
            !d->outOfBandUrl.isEmpty(),
            Element { { u"x", ns_oob }, TextElement { u"url", d->outOfBandUrl } },
        },
    });
}

/// \endcond

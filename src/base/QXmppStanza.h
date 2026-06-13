// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2015 Georg Rudoy <0xd34df00d@gmail.com>
// SPDX-FileCopyrightText: 2019 Linus Jahn <lnj@kaidan.im>
// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPSTANZA_H
#define QXMPPSTANZA_H

#include <optional>

#include <QByteArray>
#include <QSharedData>

// forward declarations of QXmlStream* classes will not work on Mac, we need to
// include the whole header.
// See http://lists.trolltech.com/qt-interest/2008-07/thread00798-0.html
// for an explanation.
#include "QXmppConstants_p.h"
#include "QXmppElement.h"
#include "QXmppNonza.h"

#include <QXmlStreamWriter>

class QXmppE2eeMetadata;
class QXmppExtendedAddressPrivate;

namespace QXmpp {

/*!
    \inmodule QXmpp

    \brief \xep{0166}{Jingle}-specific error condition, used in QXmppStanza::Error.

    \since QXmpp 1.14

    \value OutOfOrder
    \value TieBreak
    \value UnknownSession
    \value UnsupportedInfo
*/
enum class JingleErrorCondition {
    OutOfOrder,
    TieBreak,
    UnknownSession,
    UnsupportedInfo,
};

}  // namespace QXmpp

/*!
    \inmodule QXmpp

    \brief Represents an extended address as defined by \xep{0033}{Extended
    Stanza Addressing}.

    Extended addresses maybe of different types: some are defined by \xep{0033}{Extended Stanza Addressing},
    others are defined in separate XEPs (for instance \xep{0146}{Remote
    Controlling Clients}). That is why the "type" property is a string rather
    than an enumerated type.
*/
class QXMPP_EXPORT QXmppExtendedAddress
{
public:
    QXmppExtendedAddress();
    QXmppExtendedAddress(const QXmppExtendedAddress &);
    QXmppExtendedAddress(QXmppExtendedAddress &&);
    ~QXmppExtendedAddress();

    QXmppExtendedAddress &operator=(const QXmppExtendedAddress &);
    QXmppExtendedAddress &operator=(QXmppExtendedAddress &&);

    QString description() const;
    void setDescription(const QString &description);

    QString jid() const;
    void setJid(const QString &jid);

    QString type() const;
    void setType(const QString &type);

    bool isDelivered() const;
    void setDelivered(bool);

    bool isValid() const;

    static constexpr std::tuple XmlTag = { u"address", QXmpp::Private::ns_extended_addressing };
    void parse(const QDomElement &element);
    void toXml(QXmlStreamWriter *writer) const;

private:
    QSharedDataPointer<QXmppExtendedAddressPrivate> d;
};

class QXmppStanzaPrivate;
class QXmppStanzaErrorPrivate;

/*!
    \inmodule QXmpp

    \brief The QXmppStanza class is the base class for all XMPP stanzas.

    \ingroup Stanzas
*/
class QXMPP_EXPORT QXmppStanza : public QXmppNonza
{
public:
    /*!
        \inmodule QXmpp

        \brief The Error class represents a stanza error.
    */
    class QXMPP_EXPORT Error
    {
    public:
        /*!
            The type represents the error type of stanza errors.

            The error descriptions are not detailed in here. The exact meaning
            can be found in the particular protocols using them.

            \value Cancel The error is not temporary.
            \value Continue The error was only a warning.
            \value Modify The request needs to be changed and resent.
            \value Auth The request needs to be resent after authentication.
            \value Wait The error is temporary, you should wait and resend.

            \value NoType
        */
        enum Type {
            NoType = -1,
            Cancel,
            Continue,
            Modify,
            Auth,
            Wait
        };

        /*!
            A detailed condition of the error

            \value NoCondition No specific error condition was provided.
            \value BadRequest The request does not contain a valid schema.
            \value Conflict The request conflicts with another.
            \value FeatureNotImplemented The feature is not implemented.
            \value Forbidden The requesting entity does not posses the necessary privileges to perform the request.
            \value Gone The user or server can not be contacted at the address. This is used in combination with a redirection URI.
            \value InternalServerError The server has expierienced an internal error and can not process the request.
            \value ItemNotFound The requested item could not be found.
            \value JidMalformed The given JID is not valid.
            \value NotAcceptable The request does not meet the defined critera.
            \value NotAllowed No entity is allowed to perform the request.
            \value NotAuthorized The request should be resent after authentication.
            \value RecipientUnavailable The recipient is unavailable.
            \value Redirect The requested resource is available elsewhere. This is used in combination with a redirection URI.
            \value RegistrationRequired The requesting entity needs to register first.
            \value RemoteServerNotFound The remote server could not be found.
            \value RemoteServerTimeout The connection to the server could not be established or timed out.
            \value ResourceConstraint The recipient lacks system resources to perform the request.
            \value ServiceUnavailable The service is currently not available.
            \value SubscriptionRequired The requester needs to subscribe first.
            \value UndefinedCondition An undefined condition was hit.
            \value UnexpectedRequest The request was unexpected.
            \value PaymentRequired Payment is required to perform the request. \deprecated Removed in RFC 6120.
            \value PolicyViolation The entity has violated a local server policy. \since QXmpp 1.3.
        */
        enum Condition {
            NoCondition = -1,
            BadRequest,
            Conflict,
            FeatureNotImplemented,
            Forbidden,
            Gone,
            InternalServerError,
            ItemNotFound,
            JidMalformed,
            NotAcceptable,
            NotAllowed,
            NotAuthorized,
#if QXMPP_DEPRECATED_SINCE(1, 3)
            // PaymentRequired: payment is required to perform the request.
            // Deprecated since QXmpp 1.3 as it was not adopted in RFC6120.
            PaymentRequired Q_DECL_ENUMERATOR_DEPRECATED_X("The <payment-required/> error was removed in RFC6120"),
#endif
            RecipientUnavailable = 12,
            Redirect,
            RegistrationRequired,
            RemoteServerNotFound,
            RemoteServerTimeout,
            ResourceConstraint,
            ServiceUnavailable,
            SubscriptionRequired,
            UndefinedCondition,
            UnexpectedRequest,
            PolicyViolation
        };

        Error();
        Error(const Error &);
        Error(Error &&);
        Error(Type type, Condition cond, const QString &text = QString());
        Error(const QString &type, const QString &cond, const QString &text = QString());
        Error(QSharedDataPointer<QXmppStanzaErrorPrivate> d);
        ~Error();

        Error &operator=(const Error &);
        Error &operator=(Error &&);

        int code() const;
        void setCode(int code);

        QString text() const;
        void setText(const QString &text);

        Condition condition() const;
        void setCondition(Condition cond);

        Type type() const;
        void setType(Type type);

        QString by() const;
        void setBy(const QString &by);

        QString redirectionUri() const;
        void setRedirectionUri(const QString &redirectionUri);

        // XEP-0166: Jingle
        std::optional<QXmpp::JingleErrorCondition> jingleErrorCondition() const;
        void setJingleErrorCondition(std::optional<QXmpp::JingleErrorCondition>);

        // XEP-0363: HTTP File Upload
        bool fileTooLarge() const;
        void setFileTooLarge(bool);

        qint64 maxFileSize() const;
        void setMaxFileSize(qint64);

        QDateTime retryDate() const;
        void setRetryDate(const QDateTime &);

        void parse(const QDomElement &element);
        void toXml(QXmlStreamWriter *writer) const;

    private:
        friend class QXmppStanza;

        QSharedDataPointer<QXmppStanzaErrorPrivate> d;
    };

    QXmppStanza(const QString &from = QString(), const QString &to = QString());
    QXmppStanza(const QXmppStanza &other);
    QXmppStanza(QXmppStanza &&);
    ~QXmppStanza() override;

    QXmppStanza &operator=(const QXmppStanza &other);
    QXmppStanza &operator=(QXmppStanza &&);

    QString to() const;
    void setTo(const QString &);

    QString from() const;
    void setFrom(const QString &);

    QString id() const;
    void setId(const QString &);

    QString lang() const;
    void setLang(const QString &);

    QXmppStanza::Error error() const;
    std::optional<Error> errorOptional() const;
    void setError(const QXmppStanza::Error &error);
    void setError(const std::optional<Error> &error);

    QXmppElementList extensions() const;
    void setExtensions(const QXmppElementList &elements);

    QList<QXmppExtendedAddress> extendedAddresses() const;
    void setExtendedAddresses(const QList<QXmppExtendedAddress> &extendedAddresses);

    std::optional<QXmppE2eeMetadata> e2eeMetadata() const;
    void setE2eeMetadata(const std::optional<QXmppE2eeMetadata> &e2eeMetadata);

    void parse(const QDomElement &element) override;

protected:
    void extensionsToXml(QXmlStreamWriter *writer, QXmpp::SceMode = QXmpp::SceAll) const;
    void generateAndSetNextId();

private:
    QSharedDataPointer<QXmppStanzaPrivate> d;
    friend class TestClient;
};

Q_DECLARE_METATYPE(QXmppStanza::Error::Type);
Q_DECLARE_METATYPE(QXmppStanza::Error::Condition);

#endif  // QXMPPSTANZA_H

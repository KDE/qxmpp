// SPDX-FileCopyrightText: 2023 Tibor Csötönyi <work@taibsu.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppExternalServiceDiscoveryIq.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDateTime>
#include <QDomElement>

using namespace QXmpp::Private;

using Action = QXmppExternalService::Action;
using Transport = QXmppExternalService::Transport;

template<>
struct Enums::Data<Action> {
    using enum Action;
    static constexpr auto Values = makeValues<Action>({
        { Add, u"add" },
        { Delete, u"delete" },
        { Modify, u"modify" },
    });
};

template<>
struct Enums::Data<Transport> {
    using enum Transport;
    static constexpr auto Values = makeValues<Transport>({
        { Tcp, u"tcp" },
        { Udp, u"udp" },
    });
};

class QXmppExternalServicePrivate : public QSharedData
{
public:
    QString host;
    QString type;
    std::optional<Action> action;
    std::optional<QDateTime> expires;
    std::optional<QString> name;
    std::optional<QString> password;
    std::optional<quint16> port;  // recommended
    std::optional<bool> restricted;
    std::optional<Transport> transport;  // recommended
    std::optional<QString> username;
};

class QXmppExternalServiceDiscoveryIqPrivate : public QSharedData
{
public:
    QVector<QXmppExternalService> externalServices;
};

///
/// \class QXmppExternalService
///
/// QXmppExternalService represents a related XMPP entity that can be queried using \xep{0215,
/// External Service Discovery}.
///
/// \since QXmpp 1.6
///

QXmppExternalService::QXmppExternalService()
    : d(new QXmppExternalServicePrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppExternalService)

///
/// Returns the host of the external service.
///
QString QXmppExternalService::host() const
{
    return d->host;
}

///
/// Sets the host of the external service.
///
void QXmppExternalService::setHost(const QString &host)
{
    d->host = host;
}

///
/// Returns the type of the external service.
///
QString QXmppExternalService::type() const
{
    return d->type;
}

///
/// Sets the type of the external service.
///
void QXmppExternalService::setType(const QString &type)
{
    d->type = type;
}

///
/// Returns the action of the external service.
///
std::optional<Action> QXmppExternalService::action() const
{
    return d->action;
}

///
/// Sets the action of the external service.
///
void QXmppExternalService::setAction(std::optional<Action> action)
{
    d->action = action;
}

///
/// Returns the expiration date of the external service.
///
std::optional<QDateTime> QXmppExternalService::expires() const
{
    return d->expires;
}

///
/// Sets the expiration date of the external service.
///
void QXmppExternalService::setExpires(std::optional<QDateTime> expires)
{
    d->expires = std::move(expires);
}

///
/// Returns the name of the external service.
///
std::optional<QString> QXmppExternalService::name() const
{
    return d->name;
}

///
/// Sets the name of the external service.
///
void QXmppExternalService::setName(std::optional<QString> name)
{
    d->name = std::move(name);
}

///
/// Returns the password of the external service.
///
std::optional<QString> QXmppExternalService::password() const
{
    return d->password;
}

///
/// Sets the password of the external service.
///
void QXmppExternalService::setPassword(std::optional<QString> password)
{
    d->password = std::move(password);
}

/// Returns the port of the external service.
/// \note quint16 since QXmpp 1.11
std::optional<quint16> QXmppExternalService::port() const { return d->port; }

/// Sets the port of the external service.
/// \note quint16 since QXmpp 1.11
void QXmppExternalService::setPort(std::optional<quint16> port) { d->port = port; }

///
/// Returns the restricted mode of the external service.
///
std::optional<bool> QXmppExternalService::restricted() const
{
    return d->restricted;
}
///
/// Sets the restricted mode of the external service.
///
void QXmppExternalService::setRestricted(std::optional<bool> restricted)
{
    d->restricted = restricted;
}

///
/// Returns the transport type of the external service.
///
std::optional<QXmppExternalService::Transport> QXmppExternalService::transport() const
{
    return d->transport;
}

///
/// Sets the transport type of the external service.
///
void QXmppExternalService::setTransport(std::optional<Transport> transport)
{
    d->transport = transport;
}

///
/// Returns the username of the external service.
///
std::optional<QString> QXmppExternalService::username() const
{
    return d->username;
}

///
/// Sets the username of the external service.
///
void QXmppExternalService::setUsername(std::optional<QString> username)
{
    d->username = std::move(username);
}

///
/// Returns true if the element is a valid external service and can be parsed.
///
bool QXmppExternalService::isExternalService(const QDomElement &element)
{
    if (element.tagName() != u"service") {
        return false;
    }

    return !element.attribute(u"host"_s).isEmpty() &&
        !element.attribute(u"type"_s).isEmpty();
}

///
/// Parses given DOM element as an external service.
///
void QXmppExternalService::parse(const QDomElement &el)
{
    QDomNamedNodeMap attributes = el.attributes();

    setHost(el.attribute(u"host"_s));
    setType(el.attribute(u"type"_s));

    d->action = Enums::fromString<Action>(el.attribute(u"action"_s));

    if (attributes.contains(u"expires"_s)) {
        setExpires(QXmppUtils::datetimeFromString(el.attribute(u"expires"_s)));
    }

    if (attributes.contains(u"name"_s)) {
        setName(el.attribute(u"name"_s));
    }

    if (attributes.contains(u"password"_s)) {
        setPassword(el.attribute(u"password"_s));
    }

    if (attributes.contains(u"port"_s)) {
        setPort(el.attribute(u"port"_s).toInt());
    }

    if (attributes.contains(u"restricted"_s)) {
        auto restrictedStr = el.attribute(u"restricted"_s);
        setRestricted(restrictedStr == u"true" || restrictedStr == u"1");
    }

    d->transport = Enums::fromString<Transport>(el.attribute(u"transport"_s));

    if (attributes.contains(u"username"_s)) {
        setUsername(el.attribute(u"username"_s));
    }
}

///
/// \brief QXmppExternalService::toXml
///
/// Translates the external service to XML using the provided XML stream writer.
///
void QXmppExternalService::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        u"service",
        Attribute { u"host", d->host },
        Attribute { u"type", d->type },
        OptionalAttribute { u"action", d->action },
        OptionalAttribute { u"expires", d->expires },
        OptionalAttribute { u"name", d->name },
        OptionalAttribute { u"password", d->password },
        OptionalAttribute { u"port", d->port },
        OptionalAttribute { u"restricted", d->restricted },
        OptionalAttribute { u"transport", d->transport },
        OptionalAttribute { u"username", d->username },
    });
}

///
/// \brief The QXmppExternalServiceDiscoveryIq class represents an IQ used to discover external
/// services as defined by \xep{0215, External Service Discovery}.
///
/// \ingroup Stanzas
///
/// \since QXmpp 1.6
///

///
/// Constructs an external service discovery IQ.
///
QXmppExternalServiceDiscoveryIq::QXmppExternalServiceDiscoveryIq()
    : d(new QXmppExternalServiceDiscoveryIqPrivate)
{
}

QXMPP_PRIVATE_DEFINE_RULE_OF_SIX(QXmppExternalServiceDiscoveryIq)

///
/// Returns the external services of the IQ.
///
QVector<QXmppExternalService> QXmppExternalServiceDiscoveryIq::externalServices() const
{
    return d->externalServices;
}

///
/// Sets the external services of the IQ.
///
void QXmppExternalServiceDiscoveryIq::setExternalServices(const QVector<QXmppExternalService> &externalServices)
{
    d->externalServices = externalServices;
}

///
/// Adds an external service to the list of external services in the IQ.
///
void QXmppExternalServiceDiscoveryIq::addExternalService(const QXmppExternalService &externalService)
{
    d->externalServices.append(externalService);
}

///
/// Returns true if the IQ is a valid external service discovery IQ.
///
bool QXmppExternalServiceDiscoveryIq::checkIqType(const QString &tagName, const QString &xmlNamespace)
{
    return tagName == u"services" && (xmlNamespace == ns_external_service_discovery);
}

/// \cond
void QXmppExternalServiceDiscoveryIq::parseElementFromChild(const QDomElement &element)
{
    d->externalServices = parseChildElements<QVector<QXmppExternalService>>(element.firstChildElement());
}

void QXmppExternalServiceDiscoveryIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { { u"services", ns_external_service_discovery }, d->externalServices });
}
/// \endcond

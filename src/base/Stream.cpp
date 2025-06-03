// SPDX-FileCopyrightText: 2009 Manjeet Dahiya <manjeetdahiya@gmail.com>
// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
// SPDX-FileCopyrightText: 2021 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Stream.h"

#include "QXmppConstants_p.h"
#include "QXmppUtils_p.h"
#include "QXmppVisitHelper_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"
#include "XmppSocket.h"

#include <QDomDocument>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSslSocket>

using namespace QXmpp;
using namespace QXmpp::Private;

namespace QXmpp::Private {

StreamOpen StreamOpen::fromXml(QXmlStreamReader &reader)
{
    Q_ASSERT(reader.isStartElement());
    Q_ASSERT(reader.name() == u"stream");
    Q_ASSERT(reader.namespaceUri() == ns_stream);

    StreamOpen out;
    const auto attributes = reader.attributes();
    auto attribute = [&](QStringView ns, QStringView name) {
        for (const auto &a : attributes) {
            if (a.name() == name && a.namespaceUri() == ns) {
                return a.value().toString();
            }
        }
        return QString();
    };

    out.from = attribute({}, u"from");
    out.to = attribute({}, u"to");
    out.id = attribute({}, u"id");
    out.version = attribute({}, u"version");

    const auto namespaceDeclarations = reader.namespaceDeclarations();
    for (const auto &ns : namespaceDeclarations) {
        if (ns.prefix().isEmpty()) {
            out.xmlns = ns.namespaceUri().toString();
        }
    }

    return out;
}

void StreamOpen::toXml(XmlWriter &w) const
{
    w.writer()->writeStartDocument();
    w.writer()->writeStartElement(u"stream:stream"_s);
    w.write(OptionalAttribute { u"from", from });
    w.write(OptionalAttribute { u"to", to });
    w.write(OptionalAttribute { u"id", id });
    w.write(OptionalAttribute { u"version", version });
    w.write(DefaultNamespace { xmlns });
    w.write(Namespace { u"stream", ns_stream });
    w.write(Characters { QString() });
}

std::optional<StarttlsRequest> StarttlsRequest::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"starttls" || el.namespaceURI() != ns_tls) {
        return {};
    }
    return StarttlsRequest {};
}

void StarttlsRequest::toXml(XmlWriter &w) const
{
    w.write(Element { XmlTag });
}

std::optional<StarttlsProceed> StarttlsProceed::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"proceed" || el.namespaceURI() != ns_tls) {
        return {};
    }
    return StarttlsProceed {};
}

void StarttlsProceed::toXml(XmlWriter &w) const
{
    w.write(Element { XmlTag });
}

void CsiActive::toXml(XmlWriter &w) const
{
    w.write(Element { XmlTag });
}

void CsiInactive::toXml(XmlWriter &w) const
{
    w.write(Element { XmlTag });
}

std::variant<StreamErrorElement, QXmppError> StreamErrorElement::fromDom(const QDomElement &el)
{
    if (el.tagName() != u"error" || el.namespaceURI() != ns_stream) {
        return QXmppError { u"Invalid dom element."_s, {} };
    }

    std::optional<StreamErrorElement::Condition> condition;

    auto conditionEl = el.firstChildElement();
    if (conditionEl.namespaceURI() != ns_stream_error) {
        return QXmppError { u"Invalid xmlns on stream error condition."_s, {} };
    }
    if (auto conditionEnum = Enums::fromString<StreamError>(conditionEl.tagName())) {
        condition = conditionEnum;
    } else if (conditionEl.tagName() == u"see-other-host") {
        if (auto [host, port] = parseHostAddress(conditionEl.text()); !host.isEmpty()) {
            condition = SeeOtherHost { host, quint16(port > 0 ? port : XMPP_DEFAULT_PORT) };
        } else {
            return QXmppError { u"Stream error condition of <see-other-host/> requires valid redirection host."_s, {} };
        }
    } else {
        return QXmppError { u"Stream error is missing valid error condition."_s, {} };
    }

    return StreamErrorElement {
        std::move(*condition),
        firstChildElement(el, u"text", ns_stream_error).text(),
    };
}

void StreamErrorElement::toXml(XmlWriter &w) const
{
    w.write(Element {
        u"stream:error",
        [&] {
            if (const auto *streamError = std::get_if<StreamError>(&condition)) {
                w.write(Element { Tag { *streamError, ns_stream_error } });
            } else if (const auto *seeOtherHost = std::get_if<SeeOtherHost>(&condition)) {
                w.write(Element {
                    { u"see-other-host", ns_stream_error },
                    Characters<QString> { seeOtherHost->host + u':' + QString::number(seeOtherHost->port) },
                });
            }
        },
        OptionalTextElement { u"text", text },
    });
}

static QString restrictedXmlErrorText(QXmlStreamReader::TokenType token)
{
    switch (token) {
    case QXmlStreamReader::Comment:
        return u"XML comments are not allowed in XMPP."_s;
    case QXmlStreamReader::DTD:
        return u"XML DTDs are not allowed in XMPP."_s;
    case QXmlStreamReader::EntityReference:
        return u"XML entity references are not allowed in XMPP."_s;
    case QXmlStreamReader::ProcessingInstruction:
        return u"XML processing instructions are not allowed in XMPP."_s;
    default:
        return {};
    }
}

DomReader::Result DomReader::process(QXmlStreamReader &r)
{
    while (true) {
        switch (r.tokenType()) {
        case QXmlStreamReader::Invalid:
            // error received
            if (r.error() == QXmlStreamReader::PrematureEndOfDocumentError) {
                return Unfinished {};
            }
            return Error { NotWellFormed, r.errorString() };
        case QXmlStreamReader::StartElement: {
            auto child = r.prefix().isNull()
                ? doc.createElement(r.name().toString())
                : doc.createElementNS(r.namespaceUri().toString(), r.qualifiedName().toString());

            // xmlns attribute
            const auto nsDeclarations = r.namespaceDeclarations();
            for (const auto &ns : nsDeclarations) {
                if (ns.prefix().isEmpty()) {
                    child.setAttribute(u"xmlns"_s, ns.namespaceUri().toString());
                } else {
                    // namespace declarations are not supported in XMPP
                    return Error { UnsupportedXmlFeature, u"XML namespace declarations are not allowed in XMPP."_s };
                }
            }

            // other attributes
            const auto attributes = r.attributes();
            for (const auto &a : attributes) {
                child.setAttribute(a.name().toString(), a.value().toString());
            }

            if (currentElement.isNull()) {
                doc.appendChild(child);
            } else {
                currentElement.appendChild(child);
            }
            depth++;
            currentElement = child;
            break;
        }
        case QXmlStreamReader::EndElement:
            Q_ASSERT(depth > 0);
            if (depth == 0) {
                return Error { InvalidState, u"Invalid state: Received element end instead of element start."_s };
            }

            currentElement = currentElement.parentNode().toElement();
            depth--;
            // if top-level element is complete: return
            if (depth == 0) {
                return doc.documentElement();
            }
            break;
        case QXmlStreamReader::Characters:
            // DOM reader must only be used on element start: characters on level 0 are not allowed
            Q_ASSERT(depth > 0);
            if (depth == 0) {
                return Error { InvalidState, u"Invalid state: Received top-level character data instead of element begin."_s };
            }

            currentElement.appendChild(doc.createTextNode(r.text().toString()));
            break;
        case QXmlStreamReader::NoToken:
            // skip
            break;
        case QXmlStreamReader::StartDocument:
        case QXmlStreamReader::EndDocument:
            Q_ASSERT_X(false, "DomReader", "Received document begin or end.");
            return Error { InvalidState, u"Invalid state: Received document begin or end."_s };
            break;
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            return Error { UnsupportedXmlFeature, restrictedXmlErrorText(r.tokenType()) };
        }
        r.readNext();
    }
}

XmppSocket::XmppSocket(QObject *parent)
    : QXmppLoggable(parent)
{
    setSocket(new QSslSocket(this));
}

XmppSocket::XmppSocket(QSslSocket *socket, QObject *parent)
    : QXmppLoggable(parent)
{
    socket->setParent(this);
    setSocket(socket);
}

void XmppSocket::resetInternalSocket()
{
    setSocket(new QSslSocket(this));
}

void XmppSocket::setSocket(QSslSocket *socket)
{
    if (m_socket) {
        // disconnect all signals from us
        QObject::disconnect(m_socket, nullptr, this, nullptr);
        m_socket->deleteLater();
    }

    m_socket = socket;
    if (!m_socket) {
        return;
    }

    connect(socket, &QAbstractSocket::connected, this, [this]() {
        info(u"Socket connected to %1 %2"_s
                 .arg(m_socket->peerAddress().toString(),
                      QString::number(m_socket->peerPort())));

        resetStream();

        // do not emit started() with direct TLS (this happens in encrypted())
        if (!m_directTls) {
            Q_EMIT started();
        }
    });
    connect(socket, &QSslSocket::encrypted, this, [this]() {
        debug(u"Socket encrypted"_s);
        // this happens with direct TLS or STARTTLS
        resetStream();
        Q_EMIT started();
    });
    connect(socket, &QAbstractSocket::disconnected, this, &XmppSocket::disconnected);
    connect(socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        warning(u"Socket error: "_s + m_socket->errorString());
        Q_EMIT errorOccurred(m_socket->errorString(), error);
    });
    connect(socket, QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors), this, &XmppSocket::sslErrorsOccurred);
    connect(socket, &QSslSocket::readyRead, this, [this]() {
        processData(QString::fromUtf8(m_socket->readAll()));
    });
    connect(socket, &QSslSocket::stateChanged, this, &XmppSocket::internalSocketStateChanged);
}

bool XmppSocket::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void XmppSocket::connectToHost(const ServerAddress &address)
{
    m_directTls = address.type == ServerAddress::Tls;

    // connect to host
    switch (address.type) {
    case ServerAddress::Tcp:
        info(u"Connecting to %1:%2 (TCP)"_s.arg(address.host, QString::number(address.port)));
        m_socket->connectToHost(address.host, address.port);
        break;
    case ServerAddress::Tls:
        info(u"Connecting to %1:%2 (TLS)"_s.arg(address.host, QString::number(address.port)));
        Q_ASSERT(QSslSocket::supportsSsl());
        m_socket->connectToHostEncrypted(address.host, address.port);
        break;
    }
}

void XmppSocket::disconnectFromHost()
{
    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            sendData(QByteArrayLiteral("</stream:stream>"));
            m_socket->flush();
        }
        // FIXME: according to RFC 6120 section 4.4, we should wait for
        // the incoming stream to end before closing the socket
        m_socket->disconnectFromHost();
    }
    m_acceptInput = false;
}

bool XmppSocket::sendData(const QByteArray &data)
{
    logSent(QString::fromUtf8(data));
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }
    return m_socket->write(data) == data.size();
}

void XmppSocket::resetStream()
{
    m_reader.clear();
    m_domReader.reset();
    m_streamReceived = false;
    m_acceptInput = true;
}

void XmppSocket::throwError(const QString &text, StreamError condition)
{
    Q_ASSERT(m_acceptInput);
    m_acceptInput = false;

    sendData(serializeXml(StreamErrorElement { condition, text }));
    Q_EMIT errorOccurred(text, condition);

    disconnectFromHost();
}

void XmppSocket::processData(const QString &data)
{
    // stop parsing after an error has occurred
    if (!m_acceptInput) {
        return;
    }

    // Check for whitespace pings
    if (data.isEmpty()) {
        logReceived({});
        Q_EMIT stanzaReceived(QDomElement());
        return;
    }

    // log data received and process
    logReceived(data);
    m_reader.addData(data);

    // 'm_reader' parses the XML stream and 'm_domReader' creates DOM elements with the parsed XML
    // from it. 'm_domReader' lives as long as one stanza element is parsed.

    auto readDomElement = [this]() {
        return std::visit(
            overloaded {
                [this](const QDomElement &element) {
                    m_domReader.reset();
                    Q_EMIT stanzaReceived(element);
                    return true;
                },
                [](DomReader::Unfinished) {
                    return false;
                },
                [this](const DomReader::Error &error) {
                    switch (error.type) {
                    case DomReader::InvalidState:
                        throwError(u"Experienced internal error while parsing XML."_s,
                                   StreamError::InternalServerError);
                        break;
                    case DomReader::NotWellFormed:
                        throwError(u"Not well-formed: "_s + error.text,
                                   StreamError::NotWellFormed);
                        break;
                    case DomReader::UnsupportedXmlFeature:
                        throwError(error.text, StreamError::RestrictedXml);
                        break;
                    }
                    return false;
                },
            },
            m_domReader->process(m_reader));
    };

    // we're still reading a previously started top-level element
    if (m_domReader) {
        m_reader.readNext();
        if (!readDomElement()) {
            return;
        }
    }

    // reading new elements at top-level
    do {
        switch (m_reader.readNext()) {
        case QXmlStreamReader::Invalid:
            // error received
            if (m_reader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
                throwError(m_reader.errorString(), StreamError::NotWellFormed);
            }
            break;
        case QXmlStreamReader::StartDocument:
            // pre-stream open
            break;
        case QXmlStreamReader::EndDocument:
            // post-stream close
            break;
        case QXmlStreamReader::StartElement:
            // stream open or stream-level element
            if (m_reader.name() == u"stream" && m_reader.namespaceUri() == ns_stream) {
                // check for 'stream:stream' (this is required by the spec)
                if (m_reader.prefix() != u"stream") {
                    throwError(
                        u"Top-level stream element must have a namespace prefix of 'stream'."_s,
                        StreamError::BadNamespacePrefix);
                    break;
                }

                m_streamReceived = true;
                Q_EMIT streamReceived(StreamOpen::fromXml(m_reader));
            } else if (!m_streamReceived) {
                throwError(
                    u"Invalid element received. Expected 'stream' element qualified by 'http://etherx.jabber.org/streams' namespace."_s,
                    StreamError::BadFormat);
            } else {
                // parse top-level stream element
                m_domReader = DomReader();
                if (!readDomElement()) {
                    return;
                }
            }
            break;
        case QXmlStreamReader::EndElement:
            // end of stream
            Q_EMIT streamClosed();
            break;
        case QXmlStreamReader::Characters:
            if (m_reader.isWhitespace()) {
                logReceived({});
                Q_EMIT stanzaReceived(QDomElement());
            } else {
                // invalid: emit error
                throwError(
                    u"Top-level, non-whitespace character data is not allowed in XMPP."_s,
                    StreamError::BadFormat);
            }
            break;
        case QXmlStreamReader::NoToken:
            // skip
            break;
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::DTD:
        case QXmlStreamReader::EntityReference:
        case QXmlStreamReader::ProcessingInstruction:
            throwError(restrictedXmlErrorText(m_reader.tokenType()), StreamError::RestrictedXml);
            break;
        }
    } while (!m_reader.hasError() && m_acceptInput);
}

}  // namespace QXmpp::Private

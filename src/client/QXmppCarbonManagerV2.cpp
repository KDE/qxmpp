// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppCarbonManagerV2.h"

#include "QXmppClient.h"
#include "QXmppConstants_p.h"
#include "QXmppDiscoveryManager_p.h"
#include "QXmppMessage.h"
#include "QXmppOutgoingClient.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp;
using namespace QXmpp::Private;

class CarbonEnableIq : public QXmppIq
{
public:
    CarbonEnableIq()
    {
        setType(QXmppIq::Set);
    }

    // parsing not implemented
    void parseElementFromChild(const QDomElement &) override
    {
    }
    void toXmlElementFromChild(QXmlStreamWriter *writer) const override
    {
        XmlWriter(writer).write(Element { { u"enable", ns_carbons } });
    }
};

auto parseIq(std::variant<QDomElement, QXmppError> &&sendResult) -> std::optional<QXmppError>
{
    if (hasValue(sendResult)) {
        auto &el = getValue(sendResult);

        auto iqType = el.attribute(u"type"_s);
        if (iqType == u"result") {
            return {};
        }
        QXmppIq iq;
        iq.parse(el);
        if (auto error = iq.errorOptional()) {
            return QXmppError { error->text(), std::move(*error) };
        }
        // Only happens with IQs with type=error, but no <error/> element
        return QXmppError { u"Unknown error received."_s, QXmppStanza::Error() };
    } else if (hasError(sendResult)) {
        return getError(std::move(sendResult));
    }
    return {};
}

/*!
    \class QXmppCarbonManagerV2
    \inmodule QXmpp

    \brief The QXmppCarbonManagerV2 class handles message carbons as described in \xep{0280}{Message Carbons}.

    The manager automatically enables carbons when a connection is established. Either by using
    \xep{0386}{Bind 2} if available or by sending a normal IQ request on connection, if the server
    supports carbons. The server support is checked using the QXmppDiscoveryManager. If it is not
    registered with the client, carbons are enabled without checking the support.
    Carbon copied messages from other devices of the same account and carbon copied messages from
    other accounts are injected into the QXmppClient. This way you can handle them like any other
    incoming message by implementing QXmppMessageHandler or using QXmppClient::messageReceived().

    Checks are done to ensure that the entity sending the carbon copy is allowed to send the
    forwarded message.

    You don't need to do anything other than adding the extension to the client to use it.
    \code
    QXmppClient client;
    client.addNewExtension<QXmppCarbonManagerV2>();
    \endcode

    To distinguish carbon messages, you can use QXmppMessage::isCarbonMessage().

    \note Enabling via Bind 2 has been added in QXmpp 1.8.

    \ingroup Managers

    \since QXmpp 1.5
*/

QXmppCarbonManagerV2::QXmppCarbonManagerV2() = default;
QXmppCarbonManagerV2::~QXmppCarbonManagerV2() = default;

/*! Returns whether message carbons are currently enabled on the server. */
QBindable<bool> QXmppCarbonManagerV2::enabled() const
{
    return &m_enabled;
}

/*!
    Returns a watch on whether the own server supports \xep{0280}{Message Carbons}.

    The information is requested as long as a copy of the watch exists and shared with all other
    watches on the server information. This requires the QXmppDiscoveryManager to be registered
    with the client. If the manager is not registered with a client, a watch that never loads is
    returned.

    \since QXmpp 1.17
*/
QXmppDiscoFeatureWatch QXmppCarbonManagerV2::watchServerSupport() const
{
    return watchServerFeature(client(), QXmpp::Namespace::Carbons2);
}

bool QXmppCarbonManagerV2::handleStanza(const QDomElement &element, const std::optional<QXmppE2eeMetadata> &)
{
    if (element.tagName() != u"message") {
        return false;
    }

    auto carbon = firstChildElement(element, {}, ns_carbons);
    if (carbon.isNull() || (carbon.tagName() != u"sent" && carbon.tagName() != u"received")) {
        return false;
    }

    // carbon copies must always come from our bare JID
    auto from = element.attribute(u"from"_s);
    if (from != client()->configuration().jidBare()) {
        info(u"Received carbon copy from attacker or buggy client '" + from + u"' trying to use CVE-2017-5603.");
        return false;
    }

    auto forwarded = firstChildElement(carbon, u"forwarded", ns_forwarding);
    auto messageElement = firstChildElement(forwarded, u"message", ns_client);
    if (messageElement.isNull()) {
        return false;
    }

    QXmppMessage message;
    message.parse(messageElement);
    message.setCarbonForwarded(true);

    injectMessage(std::move(message));
    return true;
}

// Kept by the connection to QXmppClient::connected.
struct QXmppCarbonManagerV2::ConnectionState {
    // Keeps the support known on reconnection. Created on connection, since the
    // QXmppDiscoveryManager may be registered after this manager.
    std::optional<QXmppDiscoFeatureWatch> serverSupport;
    // Counts the new streams, so that attempts of previous streams can be dropped.
    uint64_t streamCount = 0;
};

void QXmppCarbonManagerV2::onRegistered(QXmppClient *client)
{
    client->stream()->carbonManager().setEnableViaBind2(true);
    connect(client, &QXmppClient::connected, this, [this, state = std::make_shared<ConnectionState>()] {
        enableCarbons(state);
    });
}

void QXmppCarbonManagerV2::onUnregistered(QXmppClient *client)
{
    client->stream()->carbonManager().setEnableViaBind2(false);
    disconnect(client, &QXmppClient::connected, this, nullptr);
}

QXmppTask<void> QXmppCarbonManagerV2::enableCarbons(std::shared_ptr<ConnectionState> state)
{
    // stream resumed: carbons state is preserved from the previous session
    if (client()->streamManagementState() == QXmppClient::ResumedStream) {
        co_return;
    }
    const auto stream = ++state->streamCount;

    // carbons enabled via bind2 already
    if (client()->stream()->carbonManager().enabled()) {
        m_enabled = true;
        co_return;
    }

    // new session: reset until IQ succeeds
    m_enabled = false;

    if (!state->serverSupport && client()->findExtension<QXmppDiscoveryManager>()) {
        state->serverSupport = watchServerSupport();
    }
    // without QXmppDiscoveryManager, carbons are enabled without checking the support
    if (state->serverSupport) {
        const auto supported = co_await state->serverSupport->resolve().withContext(this);
        // a new stream has been started while waiting
        if (stream != state->streamCount) {
            co_return;
        }
        if (!supported) {
            info(u"Message Carbons are not supported by the server."_s);
            co_return;
        }
    }

    auto result = co_await client()->sendIq(CarbonEnableIq()).withContext(this);
    if (auto err = parseIq(std::move(result))) {
        warning(u"Could not enable message carbons: " + err->description);
    } else {
        m_enabled = true;
        info(u"Message Carbons enabled."_s);
    }
}

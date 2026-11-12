// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppE2eeProvider.h"

#include "QXmppIqStanza.h"

namespace QXmpp::E2ee {

namespace {

// Providers are stored as handlers of this hook, so they share the lifetime rules of hooks.
struct ProviderLookup {
    static constexpr QStringView Name = u"org.qxmpp.e2ee.provider";
    // namespace of the encryption
    using Key = QStringView;
    using Signature = Provider();
};

}  // namespace

void EncryptIqRequest::writeContent(QXmlStreamWriter *writer) const
{
    Private::writeIqContent(writer, m_payload, m_error);
}

/*!
    \class QXmpp::E2ee::Providers
    \inmodule QXmpp

    Registry of the end-to-end encryption providers of a client, one per encryption.

    It is a view on the hooks of the client (see QXmpp::Hooks): extensions add their providers
    via \c {QXmpp::E2ee::Providers(hooks()).add()}, also in their constructor. A provider is
    removed when its owner is destroyed or, for extensions, removed from the client.

    \since QXmpp 1.18
*/

Providers::Providers(Hooks hooks)
    : m_hooks(std::move(hooks))
{
}

/*!
    Adds \a provider. If another provider for the same encryption exists, the one added first is
    used and a warning is logged.
*/
void Providers::add(Provider provider) const
{
    const auto encryption = provider.encryption;
    m_hooks.add<ProviderLookup>(encryption, [provider = std::move(provider)] { return provider; });
}

/*!
    Returns the provider for the encryption with the namespace \a encryption.
*/
std::optional<Provider> Providers::find(QStringView encryption) const
{
    return m_hooks.run<ProviderLookup>(encryption);
}

}  // namespace QXmpp::E2ee

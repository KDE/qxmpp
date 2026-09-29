// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppHash.h"

#include "QXmppConstants_p.h"
#include "QXmppHashing_p.h"
#include "QXmppUtils.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>
#include <QXmlStreamWriter>

using namespace QXmpp;
using namespace QXmpp::Private;

/*!
    \class QXmppHash
    \inmodule QXmpp

    Contains a hash value and its algorithm.

    \since QXmpp 1.5
*/

QXmppHash::QXmppHash() = default;

bool QXmppHash::parse(const QDomElement &el)
{
    if (elementXmlTag(el) == XmlTag) {
        m_algorithm = Enums::fromString<HashAlgorithm>(el.attribute(u"algo"_s))
                          .value_or(HashAlgorithm::Unknown);
        if (auto hashResult = parseBase64(el.text())) {
            m_hash = std::move(*hashResult);
        } else {
            return false;
        }
        return true;
    }
    return false;
}

void QXmppHash::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        XmlTag,
        Attribute { u"algo", m_algorithm },
        Characters { Base64 { m_hash } },
    });
}

/*!
    \class QXmppHashUsed
    \inmodule QXmpp

    Annotates the used hashing algorithm.

    \since QXmpp 1.5
*/

QXmppHashUsed::QXmppHashUsed() = default;

/*! Creates an object that tells other XMPP entities to use this hash algorithm. */
QXmppHashUsed::QXmppHashUsed(QXmpp::HashAlgorithm algorithm)
    : m_algorithm(algorithm)
{
}

bool QXmppHashUsed::parse(const QDomElement &el)
{
    if (elementXmlTag(el) != XmlTag) {
        return false;
    }
    m_algorithm = Enums::fromString<HashAlgorithm>(el.attribute(u"algo"_s))
                      .value_or(HashAlgorithm::Unknown);
    return true;
}

void QXmppHashUsed::toXml(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element { XmlTag, Attribute { u"algo", m_algorithm } });
}

/*! Returns the algorithm used to create the hash. */
HashAlgorithm QXmppHash::algorithm() const
{
    return m_algorithm;
}

/*! Sets the \a algorithm that was used to create the hashed data */
void QXmppHash::setAlgorithm(QXmpp::HashAlgorithm algorithm)
{
    m_algorithm = algorithm;
}

/*! Returns the binary data of the hash. */
QByteArray QXmppHash::hash() const
{
    return m_hash;
}

/*! Sets the hashed \a data. */
void QXmppHash::setHash(const QByteArray &data)
{
    m_hash = data;
}

/*! Returns the algorithm that is supposed to be used for hashing. */
HashAlgorithm QXmppHashUsed::algorithm() const
{
    return m_algorithm;
}

/*! Sets the \a algorithm that was used to create the hashed data */
void QXmppHashUsed::setAlgorithm(QXmpp::HashAlgorithm algorithm)
{
    m_algorithm = algorithm;
}

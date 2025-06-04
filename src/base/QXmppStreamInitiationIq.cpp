// SPDX-FileCopyrightText: 2010 Jeremy Lainé <jeremy.laine@m4x.org>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppConstants_p.h"
#include "QXmppStreamInitiationIq_p.h"
#include "QXmppUtils_p.h"

#include "StringLiterals.h"
#include "XmlWriter.h"

#include <QDomElement>

using namespace QXmpp::Private;

/// \cond
QXmppDataForm QXmppStreamInitiationIq::featureForm() const
{
    return m_featureForm;
}

void QXmppStreamInitiationIq::setFeatureForm(const QXmppDataForm &form)
{
    m_featureForm = form;
}

QXmppTransferFileInfo QXmppStreamInitiationIq::fileInfo() const
{
    return m_fileInfo;
}

void QXmppStreamInitiationIq::setFileInfo(const QXmppTransferFileInfo &fileInfo)
{
    m_fileInfo = fileInfo;
}

QString QXmppStreamInitiationIq::mimeType() const
{
    return m_mimeType;
}

void QXmppStreamInitiationIq::setMimeType(const QString &mimeType)
{
    m_mimeType = mimeType;
}

QXmppStreamInitiationIq::Profile QXmppStreamInitiationIq::profile() const
{
    return m_profile;
}

void QXmppStreamInitiationIq::setProfile(QXmppStreamInitiationIq::Profile profile)
{
    m_profile = profile;
}

QString QXmppStreamInitiationIq::siId() const
{
    return m_siId;
}

void QXmppStreamInitiationIq::setSiId(const QString &id)
{
    m_siId = id;
}

void QXmppStreamInitiationIq::parseElementFromChild(const QDomElement &element)
{
    QDomElement siElement = element.firstChildElement(u"si"_s);
    m_siId = siElement.attribute(u"id"_s);
    m_mimeType = siElement.attribute(u"mime-type"_s);
    if (siElement.attribute(u"profile"_s) == ns_stream_initiation_file_transfer) {
        m_profile = FileTransfer;
    } else {
        m_profile = None;
    }

    auto featureElement = firstChildElement(siElement, u"feature", ns_feature_negotiation);
    m_featureForm = parseOptionalChildElement<QXmppDataForm>(featureElement).value_or(QXmppDataForm());
    m_fileInfo = parseOptionalChildElement<QXmppTransferFileInfo>(siElement).value_or(QXmppTransferFileInfo());
}

void QXmppStreamInitiationIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    XmlWriter(writer).write(Element {
        PayloadXmlTag,
        OptionalAttribute { u"id", m_siId },
        OptionalAttribute { u"mime-type", m_mimeType },
        OptionalContent {
            m_profile == FileTransfer,
            Attribute { u"profile", ns_stream_initiation_file_transfer },
        },
        OptionalContent {
            !m_fileInfo.isNull(),
            m_fileInfo,
        },
        OptionalContent {
            !m_featureForm.isNull(),
            Element { { u"feature", ns_feature_negotiation }, m_featureForm },
        },
    });
}
/// \endcond

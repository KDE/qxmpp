// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppError.h"

#include "QXmppStanza.h"

#include "StringLiterals.h"

#include <QFileDevice>
#include <QNetworkReply>

/*!
    \variable QXmppError::description

    Human readable description of the error.
*/

/*!
    \variable QXmppError::error

    More specific details on the error. It may be of any type. Functions returning QXmppError
    should tell you which types are used.

    \a device.
*/

/*!
    Constructs a QXmppError from an QIODevice

    It tries to cast the IO device to different known IO devices to get a useful more specific
    error, i.e. it returns a QXmppError with QFileDevice::FileError for QFileDevices.

    \a device.
*/
QXmppError QXmppError::fromIoDevice(const QIODevice &device)
{
    if (const auto *file = dynamic_cast<const QFileDevice *>(&device)) {
        return QXmppError::fromFileDevice(*file);
    }
    if (const auto *networkReply = dynamic_cast<const QNetworkReply *>(&device)) {
        return QXmppError::fromNetworkReply(*networkReply);
    }
    return QXmppError { device.errorString(), std::any() };
}

/*!
    \brief Constructs a QXmppError from a QNetworkReply

    It creates a QXmppError with the error string and network error from the reply.

    \a reply.
*/
QXmppError QXmppError::fromNetworkReply(const QNetworkReply &reply)
{
    return { reply.errorString(), reply.error() };
}

/*!
    \brief Constructs a QXmppError from a QFileDevice

    It creates a QXmppError with the error string and a QFileDevice::FileError.

    \a file.
*/
QXmppError QXmppError::fromFileDevice(const QFileDevice &file)
{
    return QXmppError { file.errorString(), file.error() };
}

/*! Returns whether the error is a QNetworkReply::NetworkError. */
bool QXmppError::isFileError() const
{
    return holdsType<QFileDevice::FileError>();
}

/*! Returns whether the error is a QNetworkReply::NetworkError. */
bool QXmppError::isNetworkError() const
{
    return holdsType<QNetworkReply::NetworkError>();
}

/*! Returns whether the error is a QXmppStanza::Error. */
bool QXmppError::isStanzaError() const
{
    return holdsType<QXmppStanza::Error>();
}

/*!
    \fn template <typename T> bool QXmppError::holdsType() const

    Returns true if the error is of type T.
*/

/*!
    \fn template <typename T> std::optional<T> QXmppError::value() const

    Copies the value if it has type T, returns empty optional otherwise.
*/

/*!
    \fn template <typename T> std::optional<T> QXmppError::takeValue()

    Moves out the value if it has type T, leaves the stored error intact and returns an empty
    optional otherwise.
*/

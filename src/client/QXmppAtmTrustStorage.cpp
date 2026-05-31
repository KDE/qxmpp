// SPDX-FileCopyrightText: 2022 Melvin Keskin <melvo@olomono.de>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
    \class QXmppAtmTrustStorage
    \inmodule QXmpp

    \brief The QXmppAtmTrustStorage class stores trust data for
    \xep{0450}{Automatic Trust Management (ATM)}.

    \since QXmpp 1.5
*/

/*!
    \fn QXmppAtmTrustStorage::addKeysForPostponedTrustDecisions(const QString &encryption, const QByteArray &senderKeyId, const QList<QXmppTrustMessageKeyOwner> &keyOwners)

    Adds keys that cannot be authenticated or distrusted directly because the
    key of the trust message's sender is not yet authenticated.

    Those keys are being authenticated or distrusted once the sender's key is
    authenticated.
    Each element of keyOwners (i.e., keyOwner) can contain keys for postponed
    authentication as trustedKeys or for postponed distrusting as
    distrustedKeys.

    If keys of keyOwner.trustedKeys() are already stored for postponed
    distrusting, they are changed to be used for postponed authentication.
    If keys of keyOwner.distrustedKeys() are already stored for postponed
    authentication, they are changed to be used for postponed distrusting.
    If the same keys are in keyOwner.trustedKeys() and
    keyOwner.distrustedKeys(), they are used for postponed distrusting.

    \a encryption is the encryption protocol namespace. \a senderKeyId is the key ID of the
    trust message's sender. \a keyOwners contains key owners with key IDs for postponed trust
    decisions.
*/

/*!
    \fn QXmppAtmTrustStorage::removeKeysForPostponedTrustDecisions(const QString &encryption, const QList<QByteArray> &keyIdsForAuthentication, const QList<QByteArray> &keyIdsForDistrusting)

    Removes keys for postponed authentication or distrusting.

    \a encryption is the encryption protocol namespace. \a keyIdsForAuthentication contains
    the IDs of the keys for postponed authentication. \a keyIdsForDistrusting contains the IDs
    of the keys for postponed distrusting.
*/

/*!
    \fn QXmppAtmTrustStorage::removeKeysForPostponedTrustDecisions(const QString &encryption, const QList<QByteArray> &senderKeyIds)

    Removes keys for postponed authentication or distrusting by the trust
    message sender's key ID.

    \a encryption is the encryption protocol namespace. \a senderKeyIds contains the key IDs
    of the trust messages' senders.
*/

/*!
    \fn QXmppAtmTrustStorage::removeKeysForPostponedTrustDecisions(const QString &encryption)

    Removes all keys for postponed authentication or distrusting for the encryption protocol
    namespace \a encryption.
*/

/*!
    \fn QXmppAtmTrustStorage::keysForPostponedTrustDecisions(const QString &encryption, const QList<QByteArray> &senderKeyIds = {})

    Returns the JIDs of key owners mapped to the IDs of their keys stored for
    postponed authentication (true) or postponed distrusting (false).

    If \a senderKeyIds is empty, all keys for the encryption protocol namespace \a encryption
    are returned. \a senderKeyIds contains the key IDs of the trust messages' senders.

    Returns the key owner JIDs mapped to their keys.
*/

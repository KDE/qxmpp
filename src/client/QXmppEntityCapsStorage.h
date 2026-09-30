// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPENTITYCAPSSTORAGE_H
#define QXMPPENTITYCAPSSTORAGE_H

#include "QXmppHash.h"

#include <optional>

#include <QByteArray>
#include <QList>
#include <QString>

template<typename T>
class QXmppTask;

/*!
    \class QXmppEntityCapsStorage
    \inmodule QXmpp
    \brief Storage backend used by QXmppDiscoveryManager to keep verified service discovery
    information of entity capabilities between sessions.

    Entities announce a hash of their service discovery information in their presence
    (\xep{0115}{Entity Capabilities}). Once the information of a hash has been received and
    verified, it is the same for all entities and accounts, so it can be stored permanently and
    shared between multiple clients. With a storage, the information of most contacts is known
    without any request after the start of the application.

    The information is stored as the XML of the received disco#info \c{<query/>} element.
    QXmppDiscoveryManager verifies it again after loading it, so information that does not
    match its key is requested from the entity instead.

    QXmppDiscoveryManager has no storage by default. Implement this interface and pass an
    instance via QXmppDiscoveryManager::setEntityCapsStorage(). The returned tasks must always
    finish, also on errors: entities with the same capabilities wait for load() to finish.

    \section1 Removing unused entries

    QXmppDiscoveryManager never removes entries from the storage. The implementation is
    responsible for deleting old and unused entries, otherwise the storage grows without limit:
    entities announce new capabilities whenever their features change, e.g. after a software
    update, so entries that are no longer used accumulate over time.

    The best indicator for that is the time of the last use of an entry. QXmppDiscoveryManager
    keeps recently used information in memory and only calls load() when it needs the
    information of a key that is not in memory, and store() after it has requested and verified
    new information. Implementations should record the time of the last load() or store() of
    each key and regularly remove entries that have not been used for a while, e.g. for 90 days.
    Information that has been removed although it is still in use is requested from the entities
    again and stored anew, so removing too much only causes additional requests.

    \ingroup Managers
    \since QXmpp 1.17
*/
class QXMPP_EXPORT QXmppEntityCapsStorage
{
public:
    /*!
        \enum QXmppEntityCapsStorage::Method
        \brief The specification the hash of stored information has been generated with.

        Other methods may be added in the future. The values are stable, so they can be
        stored as integers.

        \value EntityCapabilities \xep{0115}{Entity Capabilities}
    */
    enum class Method : uint8_t {
        EntityCapabilities,
    };

    /*!
        \struct QXmppEntityCapsStorage::Key
        \inmodule QXmpp
        \brief Identifies stored information by the hash it has been verified with.

        Other methods and hash algorithms may be used in the future, so implementations
        should store all fields.
    */
    struct Key {
        /*! The method the hash has been generated with. */
        Method method = Method::EntityCapabilities;
        /*! The hash algorithm. Its values are stable, so it can be stored as an integer. */
        QXmpp::HashAlgorithm algorithm = QXmpp::HashAlgorithm::Unknown;
        /*! The hash. */
        QByteArray hash;

        /*! Returns true if the keys are equal. */
        bool operator==(const Key &) const = default;
    };

    virtual ~QXmppEntityCapsStorage();

    /*!
        Loads the information stored under \a key, i.e. the XML of the disco#info
        \c{<query/>} element, or nothing if there is none.

        Implementations should update the time of the last use of the entry, see
        \l{Removing unused entries}.
    */
    virtual QXmppTask<std::optional<QString>> load(const Key &key) = 0;
    /*!
        Stores the information \a xml, i.e. the XML of the disco#info \c{<query/>} element,
        under all \a keys, the hashes of the information using different methods or hash
        algorithms.

        Implementations should record the time of the last use of the entry, see
        \l{Removing unused entries}.
    */
    virtual QXmppTask<void> store(const QList<Key> &keys, const QString &xml) = 0;
};

#endif  // QXMPPENTITYCAPSSTORAGE_H

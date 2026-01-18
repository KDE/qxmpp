// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCDATA_H
#define QXMPPMUCDATA_H

namespace QXmpp {

namespace Muc {

enum class Affiliation {
    Outcast,
    None,
    Member,
    Admin,
    Owner,
};

enum class Role {
    None,
    Visitor,
    Participant,
    Moderator,
};

}  // namespace Muc

}  // namespace QXmpp

#endif  // QXMPPMUCDATA_H

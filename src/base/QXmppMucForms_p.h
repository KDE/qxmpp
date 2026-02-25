// SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef QXMPPMUCFORMS_P_H
#define QXMPPMUCFORMS_P_H

#include "QXmppMucForms.h"

#include "Enums.h"

namespace QXmpp::Private {

template<>
struct Enums::Data<QXmppMucRoomConfig::AllowPrivateMessages> {
    using enum QXmppMucRoomConfig::AllowPrivateMessages;
    static constexpr auto Values = makeValues<QXmppMucRoomConfig::AllowPrivateMessages>({
        { Anyone, u"anyone" },
        { Participants, u"participants" },
        { Moderators, u"moderators" },
        { Nobody, u"none" },
    });
};

template<>
struct Enums::Data<QXmppMucRoomConfig::WhoCanDiscoverJids> {
    using enum QXmppMucRoomConfig::WhoCanDiscoverJids;
    static constexpr auto Values = makeValues<QXmppMucRoomConfig::WhoCanDiscoverJids>({
        { Moderators, u"moderators" },
        { Anyone, u"anyone" },
    });
};

}  // namespace QXmpp::Private

#endif  // QXMPPMUCFORMS_P_H

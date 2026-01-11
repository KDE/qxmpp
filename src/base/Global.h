// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef GLOBAL_H
#define GLOBAL_H

// Always asserts, not limited to debug builds
#define QX_ALWAYS_ASSERT(cond) ((cond) ? static_cast<void>(0) : qt_assert(#cond, __FILE__, __LINE__))

#endif  // GLOBAL_H

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

namespace Kadunce
{
// Who may steer Spread's guest over D-Bus (TETTEGOUCHE-CONTEXT.md). A call
// Kadunce makes itself is its own and always may.
//
// A guest begins by naming the connection Kadunce answers and later calls back:
// only that connection may name itself, so no program can make Kadunce call
// another on its behalf.
[[nodiscard]] inline bool guestMayBegin(bool fromBus, const QString &caller, const QString &owner)
{
    return !fromBus || (!owner.isEmpty() && caller == owner);
}

// Every later call, to move, finish, launch from or end the guest, comes from
// the guest that began. With no guest, no caller on the bus may.
[[nodiscard]] inline bool guestMayAct(bool fromBus, const QString &caller, const QString &guest)
{
    return !fromBus || (!guest.isEmpty() && caller == guest);
}
} // namespace Kadunce

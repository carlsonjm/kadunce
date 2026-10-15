/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

namespace Kadunce
{
// What removing a workspace does to its ownership session. KWin moves the
// workspace's windows to a neighbour first, and may announce the removal
// before the switch away from it, or with no switch at all. A session left
// holding those windows hides them where they now stand, so it always lets
// them go, as a release would.
struct DesktopRemovalPlan {
    // The workspace shown now becomes the current session before the removed
    // one lets go, so its windows are taken by the session that shows them.
    bool switchFirst = false;
    bool release = false;
    // The session is forgotten, except while KWin still shows its workspace:
    // it stays current, holding nothing, until the switch arrives.
    bool forget = false;
};

// `removed` is the removed workspace's id, `current` the current session's,
// and `shown` the workspace KWin shows now, empty when it names none.
inline DesktopRemovalPlan planDesktopRemoval(bool hasSession, const QString &removed,
    const QString &current, const QString &shown)
{
    if (!hasSession) return {};
    if (removed != current) return {false, true, true};
    if (!shown.isEmpty() && shown != removed) return {true, true, true};
    return {false, true, false};
}
} // namespace Kadunce

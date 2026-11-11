/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardTuning.h"

#include <QPointF>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a window carried over another pane joins
// that pane's Stack once it has been held over it this long, as a held card
// joins a Stack in Spread once the card under it has risen; let go sooner,
// the two panes swap. The hand may move while it waits: the time runs for as
// long as the carry stays inside that pane's outline, and starts again only
// on another pane or none. The ring shows it filling, so a carry crossing a
// pane on its way elsewhere is seen and never let go of there by chance.
inline constexpr long long PaneStackDwell = CardTuning::DefaultStackPauseMs;
// That rest as a person has set it (CardTuning.h).
inline long long paneStackDwell() { return cardTuning().stackPauseMs; }

template<class Handle>
struct PaneStackAim {
    Handle pane{};
    long long since = -1;

    // The pane under the carry now, at `now` in milliseconds. True once the
    // carry has stayed over the same pane for the dwell, however it moved
    // there. `at` is where the hand is, kept for the callers' one signature.
    bool aim(const Handle &over, long long now, const QPointF &at = {})
    {
        (void)at;
        if (!(over == pane)) {
            pane = over;
            since = over ? now : -1;
        }
        return ready(now);
    }
    [[nodiscard]] bool ready(long long now) const
    {
        return pane && since >= 0 && now - since >= paneStackDwell();
    }
    // How long until it would be ready, so the aim can be read again then.
    [[nodiscard]] long long remaining(long long now) const
    {
        if (!pane || since < 0) return -1;
        const long long left = paneStackDwell() - (now - since);
        return left > 0 ? left : 0;
    }
    void clear()
    {
        pane = Handle{};
        since = -1;
    }
};
} // namespace Kadunce

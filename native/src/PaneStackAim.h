/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardTuning.h"

#include <QLineF>
#include <QPointF>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a window carried over another pane joins
// that pane's Stack once it has rested there this long, as a held card joins
// a Stack in Spread once the card under it has risen; let go sooner, the two
// panes swap. A rest is the hand held still: a carry passing over a wide pane
// on its way elsewhere is moving, however long it takes to cross.
inline constexpr long long PaneStackDwell = CardTuning::DefaultStackPauseMs;
// How far the hand may drift, in logical pixels, and still be resting.
inline constexpr double PaneRestSlack = 12.0;
// That rest as a person has set it (CardTuning.h).
inline long long paneStackDwell() { return cardTuning().stackPauseMs; }

template<class Handle>
struct PaneStackAim {
    Handle pane{};
    long long since = -1;
    QPointF restingAt;

    // The pane under the carry now, with the hand at `at`, at `now` in
    // milliseconds. True once the hand has rested on the same pane, within
    // the slack of where it stopped, for the dwell.
    bool aim(const Handle &over, long long now, const QPointF &at)
    {
        if (!(over == pane) || QLineF(restingAt, at).length() > PaneRestSlack) {
            pane = over;
            since = over ? now : -1;
            restingAt = at;
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
        restingAt = {};
    }
};
} // namespace Kadunce

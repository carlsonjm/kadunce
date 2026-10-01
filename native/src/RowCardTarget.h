// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "SpreadLayout.h"

#include <cstdlib>
#include <functional>

namespace Kadunce {

// The entry `slot` places from the centred one, in a row where every Stack
// keeps the room its fan takes (CARD-LIFECYCLE.md §9). envelopeAt(offset)
// gives each entry's reach, nothing for a lone card or past an end. The
// centred entry's whole fan is centred, and each step out adds the pitch and
// the two neighbours' reach toward each other, so the gap between two entries
// never depends on which is centred and the row lands on a Stack without
// anything moving.
inline CardRect makeRowCardTarget(const SpreadLayout &layout, int slot,
                                  const std::function<CardStackEnvelope(int)> &envelopeAt)
{
    const CardRect &center = layout.cards[1];
    const double pitch = center.width + layout.gutter;
    CardStackEnvelope previous = envelopeAt(0);
    double x = center.x - (previous.left + previous.right) / 2.0;
    const int side = slot < 0 ? -1 : 1;
    for (int step = 1; step <= std::abs(slot); ++step) {
        const CardStackEnvelope next = envelopeAt(side * step);
        x += side > 0 ? pitch + previous.right - next.left
                      : -pitch + previous.left - next.right;
        previous = next;
    }
    return {x, center.y, center.width, center.height};
}

} // namespace Kadunce

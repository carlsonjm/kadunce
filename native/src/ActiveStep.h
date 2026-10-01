// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>

namespace Kadunce {

// Where a side step from the open card goes (INPUT.md § Active card). A Stack
// is a ring: from one of its cards a step goes round it, its last card giving
// way to its first, and never leaves it; Spread or the dock leave it. A card
// on its own steps along the row, which stops at its ends.
struct ActiveStep {
    enum class Along { None, Stack, Row };
    Along along = Along::None;
    int delta = 0;
};

inline ActiveStep activeStep(int stackSize, int selectedIndex, int count, int delta)
{
    if (delta == 0) return {};
    if (stackSize > 1) return {ActiveStep::Along::Stack, delta};
    const int along = std::clamp(delta, -selectedIndex, count - 1 - selectedIndex);
    if (along == 0) return {};
    return {ActiveStep::Along::Row, along};
}

} // namespace Kadunce

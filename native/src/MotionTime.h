/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>
namespace Kadunce {
// Plasma's animation speed reaches the compositor as KWin's animation time
// factor: 1 at the default speed, smaller when faster, and 0 for instant.
// Every custom motion's duration passes through this, so one setting governs
// all of it. Instant still takes one millisecond, which ends a motion on its
// next frame and keeps every progress division defined.
[[nodiscard]] inline int motionDuration(int base, double factor)
{
    if (base <= 0) return 1;
    if (!std::isfinite(factor) || factor <= 0.0) return 1;
    return std::max(1, static_cast<int>(std::lround(base * factor)));
}

// Motion that carries on from the hand, such as a flicked row gliding to its
// card or a row parting under a carried one, keeps the hand's own pace at
// every speed but instant, where it lands at once.
[[nodiscard]] inline bool motionInstant(double factor)
{
    return !std::isfinite(factor) || factor <= 0.0;
}
}

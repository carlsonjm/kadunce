/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>

namespace Kadunce {
// How far a card rises so the text cursor clears the keyboard by one gutter:
// the least that reveals it, and never below zero. The card's own top may
// leave the display, as a page panned under a keyboard does; the cursor may
// not, so the lift stops where the cursor's top would meet the display's.
[[nodiscard]] inline double keyboardRevealLift(
    double cursorTop, double cursorBottom, double keyboardTop, double gutter,
    double displayTop)
{
    return std::clamp(cursorBottom + gutter - keyboardTop, 0.0,
                      std::max(0.0, cursorTop - displayTop));
}

}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "BentoCrossings.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    // Four panes in two columns of two: one crossing, at the shared corner.
    const auto four = bentoCrossings({{0, 0, .5, .5}, {0, .5, .5, .5}, {.5, 0, .5, .5}, {.5, .5, .5, .5}});
    check(four.size() == 1, "four panes do not meet at one crossing");
    check(four[0].topLeft == 0 && std::abs(four[0].x - .5) < 1e-9 && std::abs(four[0].y - .5) < 1e-9,
          "the crossing is not at the top-left pane's corner");
    // Moved off centre, the lines still cross.
    const auto moved = bentoCrossings({{0, 0, .6, .3}, {0, .3, .6, .7}, {.6, 0, .4, .3}, {.6, .3, .4, .7}});
    check(moved.size() == 1 && moved[0].topLeft == 0, "moved lines no longer cross");
    // Columns whose rows split at different heights meet in T-joints only.
    check(bentoCrossings({{0, 0, .5, .6}, {0, .6, .5, .4}, {.5, 0, .5, .5}, {.5, .5, .5, .5}}).empty(),
          "rows split at different heights cross");
    // Three panes, one column whole: no crossing.
    check(bentoCrossings({{0, 0, .5, 1}, {.5, 0, .5, .5}, {.5, .5, .5, .5}}).empty(), "three panes cross");
    // Two side by side: no crossing.
    check(bentoCrossings({{0, 0, .5, 1}, {.5, 0, .5, 1}}).empty(), "two panes cross");
    // Six panes in three columns of two: two crossings.
    const double t = 1.0 / 3.0;
    check(bentoCrossings({{0, 0, t, .5}, {0, .5, t, .5}, {t, 0, t, .5}, {t, .5, t, .5},
                          {2 * t, 0, t, .5}, {2 * t, .5, t, .5}}).size() == 2,
          "three columns of two do not cross twice");
    std::cout << "PASS: bento crossings\n";
}

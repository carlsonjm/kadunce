// SPDX-License-Identifier: GPL-2.0-or-later
#include "BentoCrossings.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool at(const BentoCrossing &c, double x, double y) { return std::abs(c.x - x) < 1e-9 && std::abs(c.y - y) < 1e-9; }
int main() {
    // Four panes in two columns of two: one full crossing, held by the top-left pane.
    const auto four = bentoCrossings({{0, 0, .5, .5}, {0, .5, .5, .5}, {.5, 0, .5, .5}, {.5, .5, .5, .5}});
    check(four.size() == 1, "four panes do not meet at one crossing");
    check(four[0].full && four[0].left == 0 && four[0].above == 0 && at(four[0], .5, .5),
          "the crossing is not the top-left pane's corner");
    // Listed in another order, the same crossing and holder.
    const auto shuffled = bentoCrossings({{.5, .5, .5, .5}, {.5, 0, .5, .5}, {0, .5, .5, .5}, {0, 0, .5, .5}});
    check(shuffled.size() == 1 && shuffled[0].full && shuffled[0].left == 3 && shuffled[0].above == 3,
          "the crossing's holder depends on order");
    // A whole column beside two rows: a three-way joint, held by the whole
    // pane for the column line and the upper row pane for the row line.
    const auto right = bentoCrossings({{0, 0, .5, 1}, {.5, 0, .5, .4}, {.5, .4, .5, .6}});
    check(right.size() == 1 && !right[0].full && right[0].left == 0 && right[0].above == 1 && at(right[0], .5, .4),
          "rows beside a whole column do not meet it at a joint");
    const auto left = bentoCrossings({{0, 0, .5, .3}, {0, .3, .5, .7}, {.5, 0, .5, 1}});
    check(left.size() == 1 && !left[0].full && left[0].left == 0 && left[0].above == 0 && at(left[0], .5, .3),
          "rows left of a whole column do not meet it at a joint");
    // Rows split at different heights: two three-way joints.
    const auto apart = bentoCrossings({{0, 0, .5, .6}, {0, .6, .5, .4}, {.5, 0, .5, .5}, {.5, .5, .5, .5}});
    check(apart.size() == 2 && !apart[0].full && !apart[1].full, "rows apart do not make two joints");
    // Two side by side, or one pane: none.
    check(bentoCrossings({{0, 0, .5, 1}, {.5, 0, .5, 1}}).empty(), "two panes meet at a joint");
    check(bentoCrossings({{0, 0, 1, 1}}).empty(), "one pane meets at a joint");
    // Two rows of one pane each: no line between columns.
    check(bentoCrossings({{0, 0, 1, .5}, {0, .5, 1, .5}}).empty(), "two rows meet at a joint");
    // Six panes in three columns of two: two full crossings.
    const double t = 1.0 / 3.0;
    const auto six = bentoCrossings({{0, 0, t, .5}, {0, .5, t, .5}, {t, 0, t, .5}, {t, .5, t, .5},
                                     {2 * t, 0, t, .5}, {2 * t, .5, t, .5}});
    check(six.size() == 2 && six[0].full && six[1].full, "three columns of two do not cross twice");
    std::cout << "PASS: bento crossings\n";
}

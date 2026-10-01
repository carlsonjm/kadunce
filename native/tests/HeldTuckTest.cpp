// SPDX-License-Identifier: GPL-2.0-or-later
#include "HeldTuck.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool close(double a, double b) { return std::abs(a - b) < 0.0001; }
int main() {
    const CardRect group{100, 50, 600, 400};
    const CardRect left{100, 50, 360, 400}, right{470, 50, 230, 400};
    // Tucked, the card is the pane's own shape, slid out past the group's edge
    // on the pane's side by a Stack shoulder's step, and tipped that way.
    const auto underLeft = heldTuckPose(left, group);
    check(close(underLeft.rect.width, left.width) && close(underLeft.rect.height, left.height),
          "a tucked card did not take its pane's shape");
    check(underLeft.rect.x < group.x && close(underLeft.rect.x, left.x - left.width * 0.08),
          "a card under the left pane did not show past the group's left edge");
    check(underLeft.rotation < 0, "a card under the left pane tipped the wrong way");
    const auto underRight = heldTuckPose(right, group);
    check(underRight.rect.x + underRight.rect.width > group.x + group.width
              && close(underRight.rect.x, right.x + right.width * 0.08),
          "a card under the right pane did not show past the group's right edge");
    check(underRight.rotation > 0, "a card under the right pane tipped the wrong way");
    // The blend runs from the finger's card to the tucked one and no further.
    const CardRect held{800, 300, 300, 200};
    const auto start = heldTuckBlend(held, underRight.rect, 0.0);
    const auto end = heldTuckBlend(held, underRight.rect, 1.5);
    check(close(start.x, held.x) && close(start.width, held.width), "the blend did not start at the finger");
    check(close(end.x, underRight.rect.x) && close(end.height, underRight.rect.height),
          "the blend went past the tucked card");
    // The card goes under only once the pane has given way part of the way,
    // and is wholly under before it has finished.
    check(heldTuckUnder(0.3) == 0.0 && heldTuckUnder(0.6) > 0.0 && heldTuckUnder(0.6) < 1.0
              && heldTuckUnder(0.85) == 1.0,
          "the card went under at the wrong time");
    return 0;
}

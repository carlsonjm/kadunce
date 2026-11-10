// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneResizePicture.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 0.001; }
int main() {
    // The left column of a 2x2 layout on a 2226x1176 work area: a quarter,
    // and the half it grows to when the pane under it closes.
    const PaneResizeRect quarter{0, 24, 1109, 580};
    const PaneResizeRect half{0, 24, 1109, 1168};

    // Growing, its client already draws the half: the picture keeps its own
    // scale and the box reveals it from the top, never shifted up.
    PaneResizeRect drawn = paneResizePicture(quarter, 1109, 1168);
    check(near(drawn.x, 0) && near(drawn.y, 24) && near(drawn.width, 1109) && near(drawn.height, 1168),
          "a grown picture was moved or scaled inside a smaller box");
    check(!paneResizeLeavesRoom(quarter, drawn), "a picture larger than its box left room");

    // Shrinking, its client already draws the quarter: the picture is not
    // blown up to fill the taller box, and the rest of the box is left.
    drawn = paneResizePicture(half, 1109, 580);
    check(near(drawn.width, 1109) && near(drawn.height, 580), "a smaller picture was magnified");
    check(paneResizeLeavesRoom(half, drawn), "a smaller picture claimed to cover its box");

    // A picture larger both ways shrinks only as far as covering the box.
    drawn = paneResizePicture(quarter, 2218, 2336);
    check(near(drawn.width, 1109) && near(drawn.height, 1168), "a larger picture did not shrink to cover");
    check(drawn.width >= quarter.width && drawn.height >= quarter.height, "a shrunk picture left room");

    // Nothing to draw keeps the box.
    drawn = paneResizePicture(quarter, 0, 0);
    check(near(drawn.width, quarter.width) && near(drawn.height, quarter.height), "an empty picture lost its box");
    std::cout << "A pane resized by a layout keeps its own scale, anchored at its top-left corner\n";
}

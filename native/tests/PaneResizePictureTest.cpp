// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneResizePicture.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    // A pane that only moves already has its size.
    check(paneResizeCaughtUp(1098, 573, 1098, 573, 1098, 573), "a moved pane waited for its client");
    // Shrinking from a half to a quarter, the client has not drawn yet.
    check(!paneResizeCaughtUp(1098, 1156, 1098, 1156, 1098, 573), "a pane stopped before its client drew");
    // The client has drawn the quarter, part of a pixel off at a fractional scale.
    check(paneResizeCaughtUp(1098, 1156, 1097.4, 572.6, 1098, 573), "a drawn quarter was not seen");
    // A terminal keeping whole rows stops short of its place: that is its answer.
    check(paneResizeCaughtUp(1098, 1156, 1098, 560, 1098, 573), "a pane short of its place waited");
    // A card that was not a pane before starts at another size than its place.
    check(!paneResizeCaughtUp(2540, 1420, 2540, 1420, 1265, 705), "a card took its old size as an answer");
    std::cout << "pane resize picture: ok\n";
}

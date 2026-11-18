// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneReveal.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool same(const RevealBox &a, const RevealBox &b) {
    const auto near = [](double x, double y) { return std::abs(x - y) < 0.001; };
    return near(a.x, b.x) && near(a.y, b.y) && near(a.width, b.width) && near(a.height, b.height);
}
int main() {
    // A monitor's two columns and the panes of the right one, a gutter apart.
    const double gutter = 10;
    const RevealBox whole{1290, 34, 2174, 1367}, left{1290, 34, 1082, 1367}, right{2382, 34, 1082, 1367};
    const RevealBox upper{2382, 34, 1082, 679}, lower{2382, 723, 1082, 678};

    // A terminal opens at its own size in the middle and is asked for the
    // right column. Until it stands there it has not drawn its place.
    const RevealBox opened{1857, 371, 1040, 693};
    check(!openedStandsInPlace(1040, 693, opened, right), "a window still where it opened had drawn its pane");
    check(openedStandsInPlace(1040, 693, right, right), "a window standing in its pane had not drawn it");
    check(openedStandsInPlace(1040, 693, RevealBox{2382, 34, 1076, 1358}, right),
          "a terminal keeping whole rows never drew its pane");
    check(!openedStandsInPlace(1040, 693, RevealBox{2382, 34, 1040, 693}, right),
          "a window moved but not yet redrawn had drawn its pane");
    check(openedStandsInPlace(1082, 1367, right, right), "a window opened at its pane's size waited");

    // It is unseen until then, and never longer than the cap.
    check(openedStaysUnseen(false, 0) && openedStaysUnseen(false, OpenedUnseenCap - 1),
          "the opened window showed before its client drew the pane");
    check(!openedStaysUnseen(false, OpenedUnseenCap), "the opened window stayed unseen past the cap");
    check(!openedStaysUnseen(true, 0), "a window that had drawn its pane stayed unseen");
    check(OpenedUnseenCap <= 150, "a slow client could hold a window unseen for longer than a blink");

    // With no pane over its place it comes up there, quickly at first.
    check(openedAloneOpacity(0) == 0.0 && openedAloneOpacity(OpenedAloneFade) == 1.0
          && openedAloneOpacity(10 * OpenedAloneFade) == 1.0, "a window opening alone did not come up from nothing to whole");
    check(openedAloneOpacity(OpenedAloneFade / 2) > 0.7 && openedAloneOpacity(OpenedAloneFade / 2) < 1.0,
          "a window opening alone did not come up quickly at first");

    // The pane that filled the display draws back to the left column. While
    // it still covers the right column nothing of the new window shows; then
    // the window shows from a gutter past the box's edge.
    check(revealedBeside(right, whole, gutter).empty(), "the new window showed through a pane still covering it");
    const RevealBox halfway{1290, 34, 1600, 1367};
    check(same(revealedBeside(right, halfway, gutter), RevealBox{2900, 34, 564, 1367}),
          "the new window did not show from a gutter past the box drawing back");
    check(same(revealedBeside(right, left, gutter), right), "the new window was not whole once the pane had drawn back");

    // A pane drawing up from over the one opening under it.
    const RevealBox drawingUp{2382, 34, 1082, 1000};
    check(same(revealedBeside(lower, drawingUp, gutter), RevealBox{2382, 1044, 1082, 357}),
          "the window under a pane drawing up did not show from a gutter below it");
    check(same(revealedBeside(lower, upper, gutter), lower), "the lower window was not whole once the pane above stood");

    // A pane drawing back to the right, and one drawing down.
    check(same(revealedBeside(left, RevealBox{1900, 34, 1564, 1367}, gutter), RevealBox{1290, 34, 600, 1367}),
          "the window left of a pane drawing right did not show up to a gutter before it");
    check(same(revealedBeside(upper, RevealBox{2382, 400, 1082, 1001}, gutter), RevealBox{2382, 34, 1082, 356}),
          "the window above a pane drawing down did not show down to a gutter above it");

    // A pane beside the new window's, never over it, hides nothing; nor does
    // an empty box.
    check(same(revealedBeside(lower, left, gutter), lower), "a pane in the other column hid the new window");
    check(same(revealedBeside(lower, RevealBox{}, gutter), lower), "an empty box hid the new window");

    // Over a corner, the cut that keeps more of the window.
    const RevealBox corner{1290, 34, 1300, 900};
    check(same(revealedBeside(lower, corner, gutter), RevealBox{2600, 723, 864, 678}),
          "a box over the window's corner took more of it than it covered");
    std::cout << "A window opening into a layout stays unseen until it has drawn its pane, then shows where the panes have drawn back\n";
}

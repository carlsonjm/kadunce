// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneReveal.h"
#include <cmath>
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

    // Opening as the card a display shows, it comes up from nothing and 90%
    // to whole and its own size, its opacity rising evenly; from the dock, it
    // grows out of its icon.
    check(openedCardRise(0) == 0.0 && openedCardRise(OpenedCardRise) == 1.0 && openedCardRise(5000) == 1.0,
          "a card opening did not come up from nothing to whole");
    check(std::abs(openedCardRise(OpenedCardRise / 2) - 0.5) < 0.01, "a card opening did not come up evenly");
    check(std::abs(openedCardScale(0.0) - 0.9) < 0.001 && openedCardScale(1.0) == 1.0 && openedCardScale(2.0) == 1.0
          && openedCardScale(0.5) > 0.95, "a card opening did not grow from 90% to its own size, quickly at first");
    check(OpenedCardRise == 150 && OpenedCardGrow == 300 && OpenedCardGrowFade == 60, "a card opening kept other timings");
    const RevealBox icon{1000, 1420, 64, 64};
    check(same(openedCardFromIcon(icon, whole, 0.0), icon) && same(openedCardFromIcon(icon, whole, 1.0), whole),
          "a card opened from the dock did not grow from its icon to its place");

    // Its keys are watched for as long as it takes to come into view.
    check(OpenedKeysWatched >= 220 && OpenedKeysWatched >= OpenedCardRise && OpenedKeysWatched >= OpenedCardGrow - 50,
          "an opened window's keys were watched for less than its coming into view");

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

    // Pushed in by the panes drawing back, from the side they end on: beside
    // a pane drawing left it comes from the right; under one drawing up, from
    // the bottom, even while that pane still covers it wholly.
    const NeighbourPush besideLeft = neighbourPush(right, whole, halfway, left, gutter);
    check(besideLeft.side == PushSide::Left && besideLeft.covered && std::abs(besideLeft.over - 518) < 0.001,
          "a window opening beside a pane drawing left was not pushed from the right by what it still covers");
    const OpenedPush fromRight = pushedFrom(besideLeft.side, besideLeft.over);
    check(fromRight.pushes && std::abs(fromRight.dx - 518) < 0.001 && fromRight.dy == 0.0,
          "a window opening beside a pane drawing left did not push in from the right");
    const RevealBox rightColumn{2382, 34, 1082, 1367};
    const NeighbourPush underUpper = neighbourPush(lower, rightColumn, rightColumn, upper, gutter);
    check(underUpper.side == PushSide::Top && underUpper.covered && std::abs(underUpper.over - lower.height) < 0.001,
          "a window opening under a pane still covering it did not wait wholly below");
    const OpenedPush fromBottom = pushedFrom(underUpper.side, underUpper.over);
    check(fromBottom.pushes && fromBottom.dx == 0.0 && fromBottom.dy > 0.0, "a window opening under a pane did not rise from the bottom");
    check(neighbourPush(lower, drawingUp, drawingUp, upper, gutter).over < lower.height,
          "a window under a pane drawing up did not rise as it drew up");
    // A pane that never stood over the place, however it moves, makes no room.
    const RevealBox zenFrom{1290, 723, 1082.29, 678.29}, zenTo{1290, 723, 1082, 678};
    check(!neighbourPush(upper, zenFrom, zenFrom, zenTo, gutter).covered, "a pane in the other column made room for the new window");
    check(pushedFrom(PushSide::None, 100).pushes == false, "a window with no side pushed in");
    // Taking the place of a pane that gives way, it pushes in from the screen
    // edge nearest that place.
    const OpenedPush start = openedFromNearestEdge(right, whole, 0.0), end = openedFromNearestEdge(right, whole, 1.0);
    check(start.pushes && std::abs(start.dx - (whole.right() - right.x)) < 0.001 && start.dy == 0.0,
          "a window taking a pane's place did not start past the nearest edge");
    check(end.dx == 0.0 && end.dy == 0.0, "a window taking a pane's place did not end in it");
    const OpenedPush leftStart = openedFromNearestEdge(left, whole, 0.0);
    check(leftStart.dx < -left.width + 0.5, "a window taking the left pane's place did not come from the left");
    std::cout << "A window opening into a layout stays unseen until it has drawn its pane, then shows where the panes have drawn back\n";
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "PanePlaces.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
using Columns = BentoColumns<int>;
Columns two() { Columns l; l.columns = {{{1, false}}, {{2, false}}}; return l; }
int main() {
    // A wide right-hand pane of a two-pane layout.
    const BentoRect pane{0.5, 0.0, 0.5, 1.0};
    check(!panePlaceAt(pane, 0.4, 0.5), "a contact beside the pane named one of its places");
    check(panePlaceAt(pane, 0.75, 0.5) == PanePlace::Middle, "the pane's centre did not join its Stack");
    check(panePlaceAt(pane, 0.52, 0.5) == PanePlace::Left, "its left side was not Left");
    check(panePlaceAt(pane, 0.98, 0.5) == PanePlace::Right, "its right side was not Right");
    check(panePlaceAt(pane, 0.75, 0.05) == PanePlace::Top, "its top was not Top");
    check(panePlaceAt(pane, 0.75, 0.95) == PanePlace::Bottom, "its bottom was not Bottom");
    // A tall narrow pane still offers each side along its whole length.
    const BentoRect tall{0.0, 0.0, 0.25, 1.0};
    check(panePlaceAt(tall, 0.01, 0.3) == PanePlace::Left, "a narrow pane's left was not reachable");
    check(panePlaceAt(tall, 0.125, 0.02) == PanePlace::Top, "a narrow pane's top was not reachable");
    // Pixels: a pane of 1000 by 1000, for the settle band.
    const BentoRect px{0, 0, 1000, 1000};
    check(panePlaceAt(px, 500, 295, PanePlace::Middle) == PanePlace::Middle,
          "a contact just past the middle's edge dropped the Stack");
    check(panePlaceAt(px, 500, 250, PanePlace::Middle) == PanePlace::Top,
          "a contact well past the middle's edge kept the Stack");
    check(panePlaceAt(px, 100, 95, PanePlace::Left) == PanePlace::Left,
          "a contact on the diagonal flickered to another side");

    // Under the right pane: it joins that column, each taking half its height.
    auto r = bentoColumnsPlaceByRect(two(), 2, PanePlace::Bottom, 9);
    check(r && near(r->x, 0.5) && near(r->y, 0.5) && near(r->width, 0.5) && near(r->height, 0.5),
          "a card under a pane did not take the lower half of its column");
    r = bentoColumnsPlaceByRect(two(), 1, PanePlace::Top, 9);
    check(r && near(r->x, 0) && near(r->y, 0) && near(r->height, 0.5), "a card over a pane was not on top");
    // Left of the right pane: a new column between the two, a third each.
    Columns placed = two();
    check(bentoColumnsPlaceBy(placed, 2, PanePlace::Left, 9) && placed.columns.size() == 3
          && placed.columns[1].front().window == 9, "a card left of a pane was not a column before it");
    r = bentoColumnsPlaceByRect(two(), 2, PanePlace::Right, 9);
    check(r && near(r->x, 2.0 / 3.0) && near(r->width, 1.0 / 3.0) && near(r->height, 1.0),
          "a card right of the last pane was not the last full-height column");
    // Open room above a pane is taken, not kept beside the card.
    Columns open; open.columns = {{{0, true}, {1, false}}, {{2, false}}};
    check(bentoColumnsPlaceBy(open, 1, PanePlace::Top, 9) && open.columns[0].size() == 2
          && open.columns[0][0].window == 9 && !open.columns[0][0].open, "open room above was not filled");
    // A lone column on the left: right of it fills the open half.
    Columns lone; lone.columns = {{{1, false}}};
    r = bentoColumnsPlaceByRect(lone, 1, PanePlace::Right, 9);
    check(r && near(r->x, 0.5) && near(r->width, 0.5), "right of a lone column did not fill the open half");
    // The middle, a pane not in the layout, or the card itself place nothing.
    Columns same = two();
    check(!bentoColumnsPlaceBy(same, 2, PanePlace::Middle, 9), "the middle placed a card");
    check(!bentoColumnsPlaceBy(same, 7, PanePlace::Left, 9), "a pane not in the layout placed a card");
    check(!bentoColumnsPlaceBy(same, 2, PanePlace::Left, 2), "a pane was placed beside itself");
    // A card already in the layout moves rather than standing twice.
    Columns moved; moved.columns = {{{1, false}, {9, false}}, {{2, false}}};
    check(bentoColumnsPlaceBy(moved, 2, PanePlace::Top, 9) && moved.columns[0].size() == 1
          && moved.columns[1].size() == 2, "a card already placed stood twice");
    std::cout << "A card over a pane names one of five places, placed in that pane's column or beside it\n";
}

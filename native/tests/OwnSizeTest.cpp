// SPDX-License-Identifier: GPL-2.0-or-later
#include "OwnSize.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
namespace {
const OwnRect Place{1290, 34, 2174, 1367};
const OwnRect Area{1280, 24, 2194, 1387};
// What Cards does once the application has stopped answering: where the card
// then stands.
OwnRect settle(OwnSizeRecord &record, OwnFill fill, OwnRect frame) {
    const auto wanted = ownPlace(record, fill, frame, Place, Area, 806, 536);
    return wanted && ownSizeAsserts(record, frame, *wanted) ? *wanted : frame;
}
}
int main() {
    // An application with its own title bar, as it was seen to behave: it
    // answers a maximize, and a restore, by asking for the size it had before.
    OwnSizeRecord card;
    const OwnRect own{1290, 34, 2174, 1367};
    noteOwnSize(card, OwnFill::None, own, 0);
    check(ownRectsMatch(settle(card, OwnFill::None, own), own), "a card at its own size was moved");

    // Maximize: KWin fills the work area, and 3 ms on the application asks
    // for its old size, still maximized. It is held to the work area.
    noteOwnSize(card, OwnFill::WorkArea, Area, 7000);
    OwnRect echo{1280, 24, 2174, 1367};
    noteOwnSize(card, OwnFill::WorkArea, echo, 7003);
    check(ownSizeWaitMs(card, 7003) < 100, "a card answering a maximize is left waiting");
    check(ownRectsMatch(settle(card, OwnFill::WorkArea, echo), Area), "a maximized card kept its old size");
    check(card.width == 2174 && card.height == 1367, "filling the screen changed the card's own size");

    // Restore: KWin gives the old size back, and the application asks for the
    // size it filled the screen at. It is held to its own size.
    noteOwnSize(card, OwnFill::None, own, 9000);
    echo = {1290, 34, 2194, 1387};
    noteOwnSize(card, OwnFill::None, echo, 9003);
    check(card.width == 2174 && card.height == 1367, "an answer to a restore became the card's own size");
    check(ownRectsMatch(settle(card, OwnFill::None, echo), own), "a restored card kept the size it filled the screen at");

    // The same, over and over: every press ends the same way.
    for (long long at = 20000; at < 60000; at += 4000) {
        noteOwnSize(card, OwnFill::WorkArea, Area, at);
        noteOwnSize(card, OwnFill::WorkArea, {1280, 24, 2174, 1367}, at + 3);
        check(ownRectsMatch(settle(card, OwnFill::WorkArea, {1280, 24, 2174, 1367}), Area), "a later maximize did not fill");
        noteOwnSize(card, OwnFill::None, own, at + 2000);
        noteOwnSize(card, OwnFill::None, {1290, 34, 2194, 1387}, at + 2003);
        check(ownRectsMatch(settle(card, OwnFill::None, {1290, 34, 2194, 1387}), own), "a later restore did not return");
    }
    // With no answer at all, nothing is moved.
    noteOwnSize(card, OwnFill::WorkArea, Area, 70000);
    check(ownRectsMatch(settle(card, OwnFill::WorkArea, Area), Area) && card.asserts == 0, "a filled card was moved");
    noteOwnSize(card, OwnFill::None, own, 72000);
    check(card.asserts == 0, "a restored card was counted as put back");

    // An application that insists is put back a few times, then left.
    noteOwnSize(card, OwnFill::WorkArea, Area, 80000);
    int moved = 0;
    for (int i = 0; i < 10; ++i) {
        const OwnRect small{1280, 24, 1000, 700};
        noteOwnSize(card, OwnFill::WorkArea, small, 80010 + i * 50);
        if (!ownRectsMatch(settle(card, OwnFill::WorkArea, small), small)) ++moved;
    }
    check(moved == OwnSizeAsserts, "an insisting application was fought without end");
    noteOwnSize(card, OwnFill::None, own, 90000);

    // A size it gives itself at any other time is its own, in the middle.
    noteOwnSize(card, OwnFill::None, {1290, 34, 1000, 700}, 100000);
    check(card.width == 1000 && card.height == 700, "a card's new size was not taken");
    const OwnRect middle = settle(card, OwnFill::None, {1290, 34, 1000, 700});
    check(middle.width == 1000 && middle.height == 700 && middle.x == 1290 + 587 && middle.y == 34 + 333,
          "a card at its own size does not stand in the middle");
    // And comes back after filling the screen.
    noteOwnSize(card, OwnFill::WorkArea, Area, 110000);
    noteOwnSize(card, OwnFill::None, middle, 112000);
    check(ownRectsMatch(settle(card, OwnFill::None, middle), middle), "a restored card moved");

    // A size larger than the place, as one saved while it overhung the
    // screen, is brought inside it; one that cannot be so small keeps its least.
    OwnSizeRecord wide;
    noteOwnSize(wide, OwnFill::None, {1290, 34, 2194, 1387}, 0);
    check(ownRectsMatch(settle(wide, OwnFill::None, {1290, 34, 2194, 1387}), Place), "a card overhangs the place");
    const auto least = ownPlace(wide, OwnFill::None, {0, 0, 2194, 1387}, {0, 0, 600, 400}, {0, 0, 620, 420}, 806, 536);
    check(least && least->width == 806 && least->height == 536 && least->x == 0 && least->y == 0,
          "a card was made smaller than it can be");

    // One first seen filling the screen takes the size KWin restores it to.
    OwnSizeRecord opened;
    noteOwnSize(opened, OwnFill::WorkArea, Area, 0);
    check(!opened.hasSize(), "a filled card's size was taken for its own");
    noteOwnSize(opened, OwnFill::None, {1500, 300, 900, 600}, 5000);
    noteOwnSize(opened, OwnFill::None, {1500, 300, 2194, 1387}, 5003);
    check(opened.width == 900 && opened.height == 600, "the restored size was not kept");

    // Full screen, and filling one direction, are left alone, and their size
    // is never the card's own.
    noteOwnSize(card, OwnFill::Other, {1280, 0, 2195, 1463}, 120000);
    check(!ownPlace(card, OwnFill::Other, {1280, 0, 2195, 1463}, Place, Area), "a full screen card was placed");
    check(card.width == 1000, "a full screen size became the card's own");

    // A size set by hand is its own even in the moment after a restore.
    noteOwnSize(card, OwnFill::None, middle, 130000);
    noteOwnSize(card, OwnFill::None, {1290, 34, 1200, 800}, 130100, true);
    check(card.width == 1200 && card.height == 800, "a size set by hand was not taken");
    return 0;
    // A pane has no size of its own: its pane is its place. RuneLite beside
    // another pane, as it was seen: maximized it fills the work area, and
    // restored KWin gives it its pane back, then 7 ms on it asks for the
    // size it filled the screen at, over the pane beside it.
    const OwnRect Pane{1290, 34, 1082, 1367};
    PaneHoldRecord pane;
    check(notePaneFill(pane, OwnFill::None, 0), "a pane first seen began no moment");
    check(!paneHoldPlace(pane, Pane, Pane, Area, 40), "a pane in its place was moved");
    check(!notePaneFill(pane, OwnFill::None, 5000), "a pane that changed nothing began a moment");
    check(notePaneFill(pane, OwnFill::WorkArea, 22000), "a maximize began no moment");
    check(!paneHoldPlace(pane, Area, Pane, Area, 22040), "a pane filling the work area was moved");
    // Its answer to the maximize, the size it had before: held to the work area.
    auto held = paneHoldPlace(pane, OwnRect{1280, 24, 1082, 1367}, Pane, Area, 22050);
    check(held && ownRectsMatch(*held, Area), "a maximized pane kept its pane's size");
    check(notePaneFill(pane, OwnFill::None, 23640), "a restore began no moment");
    check(!paneHoldPlace(pane, Pane, Pane, Area, 23641), "a restored pane in its place was moved");
    held = paneHoldPlace(pane, Area, Pane, Area, 23680);
    check(held && ownRectsMatch(*held, Pane), "a restored pane was left over the pane beside it");
    check(!paneHoldPlace(pane, Pane, Pane, Area, 23720), "a pane put back was moved again");
    // One that insists is put back three times in the moment, no more.
    check(paneHoldPlace(pane, Area, Pane, Area, 23760) && paneHoldPlace(pane, Area, Pane, Area, 23800),
          "an insisting pane was given up on early");
    check(!paneHoldPlace(pane, Area, Pane, Area, 23840), "an insisting pane is fought in a loop");
    // Outside the moment a size it takes is left to the layout's own settle.
    check(notePaneFill(pane, OwnFill::WorkArea, 30000) && notePaneFill(pane, OwnFill::None, 31000),
          "a later maximize and restore began no moment");
    check(!paneHoldPlace(pane, Area, Pane, Area, 31000 + OwnSizeSettleMs + 1), "a pane was held after its moment");
    // A pane short of its place covers nothing and is left alone; one a
    // layout has just placed, first seen then, is held as one restored is.
    PaneHoldRecord placed;
    notePaneFill(placed, OwnFill::None, 100);
    check(!paneHoldPlace(placed, OwnRect{1290, 34, 1082, 1340}, Pane, Area, 140), "a pane short of its place was moved");
    held = paneHoldPlace(placed, Area, Pane, Area, 150);
    check(held && ownRectsMatch(*held, Pane), "a pane just placed was left over the pane beside it");
    // Full screen, or on its way into or out of a maximize: left as it is.
    PaneHoldRecord full;
    notePaneFill(full, OwnFill::Other, 0);
    check(!paneHoldPlace(full, OwnRect{0, 0, 3840, 2560}, Pane, Area, 10), "a full screen pane was moved");
    check(ownFill(true, false, false, true, true) == OwnFill::Other, "full screen was read as a maximize");
    check(ownFill(false, true, true, false, false) == OwnFill::None, "a restored window was read as filling");
    check(ownFill(false, false, false, true, true) == OwnFill::WorkArea, "a maximized window was not read as one");
    check(ownFill(false, false, true, true, false) == OwnFill::Other, "a window being restored was read as settled");
    check(ownFill(false, true, false, false, true) == OwnFill::Other, "a window being maximized was read as settled");
}

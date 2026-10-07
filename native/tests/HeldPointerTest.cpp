// SPDX-License-Identifier: GPL-2.0-or-later
#include "HeldPointer.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    // Nowhere Kadunce's, nothing is held and the window has the pointer.
    check(heldPointer({}) == HeldPointer::None, "a point no gutter holds was held");
    // The gap beside the Active card shows which way a press pages.
    check(heldPointer({.activeSide = -1, .cardGap = true}) == HeldPointer::PageLeft, "the left gap is not a page tab");
    check(heldPointer({.activeSide = 1}) == HeldPointer::PageRight, "the right gap is not a page tab");
    check(heldPointerName(HeldPointer::PageLeft) == "kadunce-page-left"
              && heldPointerName(HeldPointer::PageRight) == "kadunce-page-right",
          "the page tabs lost their theme names");
    // A divider's reach is one resize shape, whatever lies under it.
    check(heldPointer({.divider = 1, .activeSide = 1, .cardGap = true}) == HeldPointer::ResizeColumns,
          "a divider between side-by-side panes is not a column resize");
    check(heldPointer({.divider = 2}) == HeldPointer::ResizeRows, "a divider between stacked panes is not a row resize");
    check(heldPointerName(HeldPointer::ResizeColumns) == "col-resize", "a column resize lost its name");
    // Any other gutter is an arrow.
    check(heldPointer({.cardGap = true}) == HeldPointer::Arrow, "a gutter is not an arrow");
    // An open Table holds the pointer everywhere, grabbing while a card is carried.
    check(heldPointer({.tableOpen = true, .divider = 1}) == HeldPointer::Arrow, "an open Table is not an arrow");
    check(heldPointer({.tableOpen = true, .tableCarrying = true}) == HeldPointer::Carrying,
          "a card carried in Table is not grabbing");
    check(heldPointerName(HeldPointer::Carrying) == "grabbing", "grabbing lost its name");
    std::cout << "PASS: held pointer\n";
}

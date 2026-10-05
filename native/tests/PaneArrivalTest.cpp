// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneArrival.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    // The tablet's left pane, and the card-shaped frame a client still shows
    // when it has not yet drawn the pane's size.
    const double paneWidth = 892, paneHeight = 1010;
    check(paneArrivalShaped(892, 1010, paneWidth, paneHeight), "the pane's own size was not its shape");
    check(paneArrivalShaped(891.6, 1010.4, paneWidth, paneHeight), "a fraction of a pixel off was not the pane");
    check(paneArrivalShaped(446, 505, paneWidth, paneHeight), "the pane's proportions at another size were not its shape");
    check(!paneArrivalShaped(1463, 914, paneWidth, paneHeight), "a card-shaped frame passed for the pane");
    check(!paneArrivalShaped(0, 0, paneWidth, paneHeight) && !paneArrivalShaped(892, 1010, 0, 1010),
          "an empty frame or pane had a shape");

    // It waits for that shape, never longer than the cap, and not at all
    // once the client has drawn it.
    check(paneArrivalWaits(false, 0) && paneArrivalWaits(false, PaneArrivalWaitCap - 1),
          "the card moved before its client drew the pane");
    check(!paneArrivalWaits(false, PaneArrivalWaitCap), "the card waited past the cap");
    check(!paneArrivalWaits(true, 0), "the card waited for a client that had drawn the pane");
    check(PaneArrivalWaitCap <= 150 && PaneArrivalAbandon > PaneArrivalWaitCap + PaneArrivalDuration,
          "the wait was not short, or a slow display could drop the move");

    // Its move counts from the first frame that draws it, so time before that
    // frame, a stall included, takes none of it.
    check(paneArrivalProgress(false, 5000, PaneArrivalDuration) == 0.0, "a move not yet drawn had begun");
    check(paneArrivalProgress(true, 0, PaneArrivalDuration) == 0.0
              && paneArrivalProgress(true, PaneArrivalDuration / 2, PaneArrivalDuration) == 0.5
              && paneArrivalProgress(true, PaneArrivalDuration + 40, PaneArrivalDuration) == 1.0,
          "the move did not run its length from its first frame");
    check(paneArrivalProgress(true, 0, 0) == 1.0, "a move with no time did not finish");
    std::cout << "A card let go on a pane waits briefly for its client and then moves the whole way\n";
}

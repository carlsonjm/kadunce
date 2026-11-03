/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "PaneStackAim.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace
{
int failures = 0;
void expect(bool condition, const char *what)
{
    if (condition) return;
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
}
} // namespace

int main()
{
    PaneStackAim<int> aim;
    const QPointF here(500, 400);
    expect(!aim.aim(0, 0, here), "no pane under the carry is never ready");
    expect(aim.remaining(0) == -1, "no pane has nothing to wait for");
    expect(!aim.aim(7, 100, here), "a pane just reached is not ready");
    expect(aim.remaining(100) == PaneStackDwell, "it waits the whole dwell");
    expect(!aim.aim(7, 100 + PaneStackDwell - 1, here), "just short of the dwell it still swaps");
    expect(aim.aim(7, 100 + PaneStackDwell, here), "rested the dwell over one pane, it stacks");
    expect(!aim.aim(8, 100 + PaneStackDwell + 10, here), "another pane starts the wait again");
    expect(aim.remaining(100 + PaneStackDwell + 10) == PaneStackDwell, "from its full length");
    expect(!aim.aim(0, 2000, here), "leaving every pane forgets the aim");
    expect(!aim.aim(8, 2001, here), "coming back starts again");
    expect(aim.aim(8, 2001 + PaneStackDwell, here), "and is ready after the dwell");
    aim.clear();
    expect(!aim.ready(5000), "a cleared aim is not ready");

    // A steady drag across a wide pane, 1100 px in half a second, never rests.
    PaneStackAim<int> crossing;
    bool stacked = false;
    for (int ms = 0; ms <= 1000; ms += 16)
        stacked = crossing.aim(7, ms, QPointF(1600 - ms * 1.1, 400)) || stacked;
    expect(!stacked, "a carry crossing a pane without stopping does not stack");
    // Stopped within the slack, a small drift still rests.
    expect(!crossing.aim(7, 2000, QPointF(300, 400)), "stopping starts the rest");
    expect(!crossing.aim(7, 2000 + PaneStackDwell / 2, QPointF(300 + PaneRestSlack - 1, 401)),
           "a drift inside the slack keeps resting");
    expect(crossing.aim(7, 2000 + PaneStackDwell, QPointF(304, 398)), "rested the dwell, it stacks");
    // Moved past the slack, the rest begins again where it stopped.
    PaneStackAim<int> moved;
    expect(!moved.aim(7, 0, here), "a rest begins");
    expect(!moved.aim(7, PaneStackDwell - 50, here + QPointF(PaneRestSlack + 1, 0)),
           "moving past the slack starts the rest again");
    expect(!moved.aim(7, PaneStackDwell + 10, here + QPointF(PaneRestSlack + 1, 0)),
           "so the first stop's time does not count");
    expect(moved.aim(7, 2 * PaneStackDwell - 50, here + QPointF(PaneRestSlack + 1, 0)),
           "the new rest stacks after its own dwell");
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: pane stack aim\n";
    return EXIT_SUCCESS;
}

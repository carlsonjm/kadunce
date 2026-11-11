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

    // Held over one pane, the hand may move anywhere inside it: the time runs on.
    PaneStackAim<int> moving;
    bool early = false;
    for (int ms = 0; ms < PaneStackDwell; ms += 16)
        early = moving.aim(7, ms, QPointF(1600 - ms * 1.1, 400 + ms % 50)) || early;
    expect(!early, "moving over a pane is not ready before the dwell");
    expect(moving.aim(7, PaneStackDwell, QPointF(200, 900)), "kept over it the dwell, it stacks however it moved");
    // Leaving for another pane, even briefly, starts it again.
    PaneStackAim<int> left;
    expect(!left.aim(7, 0, here), "a wait begins");
    expect(!left.aim(8, PaneStackDwell / 2, here), "another pane starts it again");
    expect(!left.aim(7, PaneStackDwell / 2 + 10, here), "and coming back starts it once more");
    expect(!left.aim(7, PaneStackDwell + 10, here), "so the first pane's time does not count");
    expect(left.aim(7, PaneStackDwell * 3 / 2 + 10, here), "the new wait stacks after its own dwell");
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: pane stack aim\n";
    return EXIT_SUCCESS;
}

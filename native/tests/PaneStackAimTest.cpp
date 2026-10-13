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
    expect(!aim.aim(0, 0), "no pane under the carry is never ready");
    expect(aim.remaining(0) == -1, "no pane has nothing to wait for");
    expect(!aim.aim(7, 100), "a pane just reached is not ready");
    expect(aim.remaining(100) == PaneStackDwell, "it waits the whole dwell");
    expect(!aim.aim(7, 100 + PaneStackDwell - 1), "just short of the dwell it still swaps");
    expect(aim.aim(7, 100 + PaneStackDwell), "rested the dwell over one pane, it stacks");
    expect(!aim.aim(8, 100 + PaneStackDwell + 10), "another pane starts the wait again");
    expect(aim.remaining(100 + PaneStackDwell + 10) == PaneStackDwell, "from its full length");
    expect(!aim.aim(0, 2000), "leaving every pane forgets the aim");
    expect(!aim.aim(8, 2001), "coming back starts again");
    expect(aim.aim(8, 2001 + PaneStackDwell), "and is ready after the dwell");
    aim.clear();
    expect(!aim.ready(5000), "a cleared aim is not ready");
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: pane stack aim\n";
    return EXIT_SUCCESS;
}

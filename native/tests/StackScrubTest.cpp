/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "StackScrub.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using Kadunce::StackScrub;
    StackScrub scrub;
    scrub.begin(12, 0, 600.0);
    require(scrub.follow(600.0) == 0, "No travel passes nothing");
    require(scrub.follow(600.0 - Kadunce::StackScrubStep * 3 - 1) == 3, "Three steps up pass three cards");
    require(scrub.follow(800.0) == 0, "Travel down returns it to the front");
    require(scrub.follow(-5000.0) == 11, "It stops at the back");
    require(scrub.ladderStart() == 8 && scrub.ladderCount() == 4, "At the back the last four show");
    scrub.follow(600.0);
    require(scrub.ladderStart() == 0, "At the front the first four show");
    require(scrub.wheel(2, 600.0) == 2, "Two notches pass two cards");
    require(scrub.ladderStart() == 1, "The card last passed shows above the held one");
    require(scrub.wheel(-5, 600.0) == 0, "The wheel stops at the front");

    StackScrub small;
    small.begin(3, 0, 0.0);
    require(small.ladderCount() == 3 && small.ladderStart() == 0, "A small Stack shows whole");

    // A face of five let go two back: the two it passed come forward.
    const std::vector<int> behind{2, 3, 4, 5};
    require(Kadunce::stackScrubOrder(1, behind, 2) == std::vector<int>{2, 3, 1, 4, 5},
            "Face not at its new place");
    require(Kadunce::stackScrubOrder(1, behind, 0) == std::vector<int>{1, 2, 3, 4, 5},
            "Let go where it began, nothing moves");
    require(Kadunce::stackScrubOrder(1, behind, 9).back() == 1, "Past the back it goes last");

    using Kadunce::StackScrubStart;
    require(Kadunce::stackScrubStart(2, -3, 900) == StackScrubStart::Undecided, "A tremor decides nothing");
    require(Kadunce::stackScrubStart(0, -20, 400) == StackScrubStart::Scrub, "Held, then up, scrubs");
    require(Kadunce::stackScrubStart(0, -20, 100) == StackScrubStart::Carry, "Up at once carries");
    require(Kadunce::stackScrubStart(30, -10, 900) == StackScrubStart::Carry, "Held, then sideways, carries");
    require(Kadunce::stackScrubStart(0, 20, 900) == StackScrubStart::Carry, "Held, then down, carries");

    const auto front = Kadunce::stackLadderRung(100, 50, 800, 600, 0, 4);
    const auto back = Kadunce::stackLadderRung(100, 50, 800, 600, 3, 4);
    require(front.y == 50 + 3 * Kadunce::StackLadderRung && front.x == 100 && front.width == 800,
            "The card in front stands lowest, full width");
    require(back.y == 50 && back.x > 100 && back.width < 800, "The last shown stands at the top, narrower");
    require(front.height == back.height && front.y + front.height == 650, "Every rung ends at the pane's foot");
    const auto lone = Kadunce::stackLadderRung(0, 0, 400, 300, 0, 2);
    require(lone.y == Kadunce::StackLadderRung, "Two shown take one rung");
    return EXIT_SUCCESS;
}

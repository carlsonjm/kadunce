/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "SpreadModel.h"
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

    // A face of five moved two back: the two it passed come forward.
    Kadunce::SpreadModel model(5);
    for (int id = 2; id <= 5; ++id) {
        model.selectIndex(1);
        require(model.stackSelectedWith(1), "Stack setup failed");
    }
    const std::vector<int> before = model.stackFrontFirstForId(1);
    require(before.size() == 5, "Stack of five");
    const int face = before[0];
    require(model.moveStackMember(face, 2), "Move failed");
    const std::vector<int> after = model.stackFrontFirstForId(face);
    require(after == std::vector<int>{before[1], before[2], face, before[3], before[4]},
            "Face not at its new place");
    require(model.moveStackMember(face, 4), "Move to the back failed");
    require(model.stackFrontFirstForId(face).back() == face, "Not at the back");
    require(!model.moveStackMember(face, 5), "A place past the back is refused");
    return EXIT_SUCCESS;
}

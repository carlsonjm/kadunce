// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActiveStep.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    using Along = ActiveStep::Along;
    // A row of five entries, the second a three-card Stack.
    // From a Stack's card, every step goes round the Stack, at either end too.
    for (int delta : {-1, 1}) {
        const auto step = activeStep(3, 1, 5, delta);
        check(step.along == Along::Stack && step.delta == delta, "a Stack's card stepped out of its ring");
    }
    check(activeStep(3, 0, 5, -1).along == Along::Stack, "the first entry's Stack stopped at the row's end");
    check(activeStep(3, 4, 5, 1).along == Along::Stack, "the last entry's Stack stopped at the row's end");
    // A card on its own walks the row, and stops at its ends.
    check(activeStep(1, 2, 5, 1).along == Along::Row && activeStep(1, 2, 5, 1).delta == 1, "a lone card did not step on");
    check(activeStep(1, 0, 5, -1).along == Along::None, "the row's first card stepped past its end");
    check(activeStep(1, 4, 5, 1).along == Along::None, "the row's last card stepped past its end");
    check(activeStep(1, 3, 5, 3).delta == 1, "a long step left the row");
    // Nothing asked, nothing happens.
    check(activeStep(3, 1, 5, 0).along == Along::None, "a zero step moved");
    return 0;
}

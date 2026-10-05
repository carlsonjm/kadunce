/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "MotionTime.h"
#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace
{
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    check(motionDuration(220, 1.0) == 220, "the default speed changed a duration");
    // Four times the default speed is a quarter of each duration.
    check(motionDuration(220, 0.25) == 55 && motionDuration(460, 0.25) == 115,
        "a faster Plasma speed did not shorten motion in proportion");
    check(motionDuration(220, 2.0) == 440, "a slower Plasma speed did not lengthen motion");
    // Instant ends a motion on its next frame and never divides by zero.
    check(motionDuration(220, 0.0) == 1 && motionDuration(220, -1.0) == 1
        && motionDuration(220, std::nan("")) == 1,
        "instant motion was not one millisecond");
    check(motionDuration(1, 0.25) == 1 && motionDuration(0, 1.0) == 1,
        "a duration fell below one millisecond");
    std::cout << "Motion time checks passed\n";
}

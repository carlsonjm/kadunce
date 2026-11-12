// SPDX-License-Identifier: GPL-2.0-or-later
#include "StackStepMotion.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool rests(const StackStepPose &p) {
    return near(p.dx, 0) && near(p.dy, 0) && near(p.scale, 1) && near(p.opacity, 1)
        && near(p.brightness, 1) && near(p.rotation, 0);
}
int main() {
    const double width = 1000.0;
    for (bool forward : {true, false}) {
        // The leaving card starts in place; the arriving one ends in place.
        check(rests(stackStepPose(0.0, forward, false, width)), "the leaving card does not start in place");
        check(rests(stackStepPose(1.0, forward, true, width)), "the arriving card does not end in place");
        check(near(stackStepPose(0.0, forward, true, width).opacity, 0.0), "the arriving card shows before the step");
        for (int step = 0; step <= 40; ++step) {
            const double p = step / 40.0;
            for (bool arriving : {false, true}) {
                const auto pose = stackStepPose(p, forward, arriving, width);
                // Nothing crosses the room beside the Stack.
                check(pose.dx <= 0.0 && pose.dx >= -width * StackStepReach - 1e-9, "a card travels too far");
                check(pose.scale <= 1.0 && pose.scale >= 1.0 - StackStepSink - 1e-9, "a card grows or sinks too far");
                check(pose.opacity >= 0.0 && pose.opacity <= 1.0, "opacity leaves its range");
            }
        }
    }
    // Stepping on, the front card goes round; stepping back, the card before does.
    check(stackStepPose(0.5, true, false, width).dx < 0.0, "stepping on, the front card stays put");
    check(near(stackStepPose(0.5, true, true, width).dx, 0.0), "stepping on, the next card slides");
    check(stackStepPose(0.5, false, true, width).dx < 0.0, "stepping back, the card before does not come round");
    check(near(stackStepPose(0.5, false, false, width).dx, 0.0), "stepping back, the front card slides");
    // Stepping on, the card that went round ends behind, smaller and darker.
    const auto behind = stackStepPose(1.0, true, false, width);
    check(near(behind.dx, 0.0) && behind.scale < 1.0 && behind.brightness < 1.0, "the front card does not tuck behind");
    return 0;
}

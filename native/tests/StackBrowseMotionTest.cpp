// SPDX-License-Identifier: GPL-2.0-or-later
#include "StackBrowseMotion.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
int main() {
    for (double height : {0.0, 300.0, 700.0, 1500.0}) {
        for (int direction : {-1, 1}) {
            for (double p : {-1.0, 0.0, 1.0, 2.0}) {
                const auto endpoint = stackBrowseAccent(p, height, direction, 1);
                check(endpoint.y == 0 && endpoint.rotation == 0, "fan endpoint changed");
            }
            for (int i=0; i<=100; ++i) {
                const double p = i/100.0;
                const auto incoming = stackBrowseAccent(p, height, direction, 1);
                const auto outgoing = stackBrowseAccent(p, height, direction, -1);
                const auto other = stackBrowseAccent(p, height, direction, 0);
                if (direction < 0) {
                    check(incoming.y == -outgoing.y && incoming.rotation == -outgoing.rotation,
                        "accepted upward exchange changed");
                    check(outgoing.y <= 0, "upward flick does not send front card up");
                } else {
                    check(incoming.y >= 0, "downward flick does not pull incoming card down");
                    check(outgoing.y == 0 && outgoing.rotation == 0,
                        "reverse still bumps the departing face");
                }
                check(std::abs(incoming.y) <= 24 && std::abs(incoming.rotation) <= .25,
                    "depth cue exceeded bounds");
                check(other.y == 0 && other.rotation == 0, "unrelated member moved");
            }
            const auto restart = stackBrowseAccent(0, height, -direction, 1);
            check(restart.y == 0 && restart.rotation == 0, "captured pose jumps on restart");
        }
    }
    check(StackBrowseDuration == 220, "stack browse diverged from scoped duration");
}

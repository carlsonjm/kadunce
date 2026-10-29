// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActiveSlide.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
int main() {
    const double pitch = 1200.0 + ActiveSlideGap;
    // The two cards are one strip: always one pitch apart, whatever the position.
    for (int side : {-1, 1}) {
        for (int step = 0; step <= 20; ++step) {
            const double p = step / 20.0;
            const double gap = activeSlideArrivingShift(p, side, pitch) - activeSlideLeavingShift(p, side, pitch);
            check(near(gap, side * pitch), "the strip's cards drift apart");
        }
        check(near(activeSlideLeavingShift(0.0, side, pitch), 0.0), "the leaving card does not start in place");
        check(near(activeSlideArrivingShift(1.0, side, pitch), 0.0), "the arriving card does not end in place");
    }
    // The card after comes in from the right, so the leaving card goes left.
    check(activeSlideLeavingShift(0.5, 1, pitch) < 0.0, "the card after pushes the wrong way");
    check(activeSlideArrivingShift(0.0, -1, pitch) < 0.0, "the card before starts on the right");
    // The hand: 1:1, capped at the arriving card's place.
    check(near(activeSlideTracked(pitch * 0.4, pitch, true), 0.4), "the strip does not follow the finger 1:1");
    check(near(activeSlideTracked(pitch * 2.0, pitch, true), 1.0), "the strip runs past the arriving card");
    check(near(activeSlideTracked(-50.0, pitch, true), 0.0), "the strip moves against the swipe");
    // A row end resists, never giving more than the give.
    check(activeSlideTracked(pitch, pitch, false) < ActiveSlideGive, "a row end gives too far");
    check(activeSlideTracked(10.0, pitch, false) > 0.0, "a row end does not give at all");
    // Let go past a quarter, or flicked, it goes on; otherwise back.
    check(activeSlideCommits(0.3, 0.0), "past a quarter springs back");
    check(!activeSlideCommits(0.2, 0.0), "short of a quarter goes on");
    check(activeSlideCommits(0.05, ActiveSlideFlick), "a flick springs back");
    // The motions start and end where asked, and stay between.
    for (auto shape : {ActiveSlideMotion::Shape::InOut, ActiveSlideMotion::Shape::Out}) {
        const ActiveSlideMotion motion{0.3, 1.0, 260, shape};
        check(near(activeSlideAt(motion, 0.0), 0.3), "a motion jumps at its start");
        check(near(activeSlideAt(motion, 260.0), 1.0), "a motion stops short");
        double previous = 0.3;
        for (int ms = 0; ms <= 260; ms += 10) {
            const double now = activeSlideAt(motion, ms);
            check(now + 1e-12 >= previous && now <= 1.0 + 1e-12, "a motion goes back or overshoots");
            previous = now;
        }
        check(activeSlideDone(motion, 260.0) && !activeSlideDone(motion, 259.0), "a motion ends at the wrong time");
    }
    // A give goes out and comes home.
    const ActiveSlideMotion give{0.0, ActiveSlideGive, 260, ActiveSlideMotion::Shape::Give};
    check(near(activeSlideAt(give, 0.0), 0.0) && near(activeSlideAt(give, 260.0), 0.0), "a give does not come home");
    check(near(activeSlideAt(give, 130.0), ActiveSlideGive), "a give does not reach its reach at half time");
    return 0;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include "RowPageMotion.h"
#include <cstdlib>
#include <iostream>
#include <cmath>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    for (double offset : {0.0, -1600.0}) {
        const QRectF area(offset, 30, 1200, 800);
        const RowPageFrame left{{offset-500, 180, 700, 440}, .6, 1};
        const RowPageFrame center{{offset+250, 170, 700, 440}, 0, 1};
        const RowPageFrame right{{offset+1000, 180, 700, 440}, -.6, 1};
        for (int side : {-1, 1}) {
            const auto destination = side < 0 ? left : right;
            const auto origin = rowNeighborOrigin(destination, area, side);
            check(side < 0 ? origin.rect.right() < area.left() : origin.rect.left() > area.right(),
                "opaque entry begins on screen");
            check(origin.rect.size() == destination.rect.size() && origin.rotation == destination.rotation,
                "incoming neighbor changes canonical shape");
            double previousOpacity = 1;
            for (int frame=0; frame<=100; ++frame) {
                const auto pose = rowPageFrame(origin, destination, area, false, frame/100.0);
                check(pose.opacity >= previousOpacity && pose.opacity <= 1, "entry opacity overshot");
                check(pose.rect.x() >= std::min(origin.rect.x(), destination.rect.x())
                    && pose.rect.x() <= std::max(origin.rect.x(), destination.rect.x()), "entry overshot");
                previousOpacity = pose.opacity;
            }
            check(rowPageFrame(origin, destination, area, false, .01).opacity == 1,
                "incoming neighbor still fades");
            check(rowPageFrame(origin, destination, area, false, 1).rect == destination.rect,
                "incoming neighbor misses endpoint");
        }
        check(rowPageFrame(left, center, area, false, 0).rect == left.rect, "start jumped");
        check(rowPageFrame(left, center, area, false, 1).rect == center.rect, "endpoint missed");
        double previousX = left.rect.x();
        for (int frame=0; frame<=100; ++frame) {
            const auto pose = rowPageFrame(left, center, area, false, frame/100.0);
            check(pose.rect.x() >= previousX && pose.rect.x() <= center.rect.x(), "overshoot/reversal");
            check(pose.opacity == 1 && pose.rect.size() == center.rect.size(), "paging changed size/opacity");
            previousX = pose.rect.x();
        }
        const auto interrupted = rowPageFrame(left, center, area, false, .31);
        const auto reversed = rowPageFrame(interrupted, left, area, false, 0);
        check(reversed.rect == interrupted.rect && reversed.rotation == interrupted.rotation,
            "reversal does not start at current pose");
        const auto relocated = rowPageFrame(left, right, area, true, .5);
        check(relocated.rect.left() > area.right() && relocated.opacity == 1,
            "wrap relocation is not clipped offscreen and opaque");
        for (double t : {.1, .25, .49, .51, .75, .9}) {
            const auto pose = rowPageFrame(left, right, area, true, t);
            check(pose.rect.center().x() < area.left() || pose.rect.center().x() > area.right(),
                "wrapped shoulder crossed center");
        }
        check(rowPageFrame(relocated, center, area, false, 0).rect == relocated.rect,
            "interrupt jumped offscreen origin");
    }
    check(RowPageDuration < 300, "paging cannot outlast fastest held repeat");
    for (double direction : {-1.0, 1.0}) {
        const QRectF work(0, 0, 1200, 800);
        const double pitch = 660, displacement = direction*pitch;
        const RowPageFrame destination{{280, 150, 640, 440}, 0, 1};
        auto centerStart = destination;
        centerStart.rect.translate(displacement, 0);
        auto neighborEnd = destination;
        neighborEnd.rect.translate(direction*pitch, 0);
        auto neighborStart = neighborEnd;
        neighborStart.rect.translate(displacement, 0);
        for (int i=0; i<=100; ++i) {
            const double p = i/100.0;
            const auto center = sharedRowPageFrame(centerStart, destination, work, false, displacement, p);
            const auto neighbor = sharedRowPageFrame(neighborStart, neighborEnd, work, false, displacement, p);
            check(std::abs(std::abs(neighbor.rect.x()-center.rect.x())-pitch) < .0001,
                "neighbor and center compress their gutter during paging");
            auto wrapStart = destination;
            wrapStart.rect.translate(-direction*pitch, 0);
            const auto wrapped = sharedRowPageFrame(wrapStart, neighborEnd, work, true, displacement, p);
            const double t = 1-std::pow(1-p, 3);
            const double exitingX = wrapStart.rect.x()-displacement*t;
            const double enteringX = neighborEnd.rect.x()+displacement*(1-t);
            check(std::abs(wrapped.rect.x()-exitingX)<.0001 || std::abs(wrapped.rect.x()-enteringX)<.0001,
                "wrap used an independent clock/distance");
            check(wrapped.opacity == 1, "unified row faded");
        }
    }
}

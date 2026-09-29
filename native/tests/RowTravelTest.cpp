// SPDX-License-Identifier: GPL-2.0-or-later
#include "RowTravel.h"
#include "SpreadStroke.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
// A row under the finger at `position`.
RowMotion held(double position) {
    RowMotion motion;
    motion.mode = RowMotion::Mode::Drag;
    motion.position = position;
    return motion;
}
// Run a released row to rest at 120 frames a second.
RowMotion settle(RowMotion motion, const RowStops &row, double *furthest = nullptr) {
    for (int frame = 0; frame < 2000 && stepRow(motion, row, 1.0 / 120.0); ++frame) {
        if (furthest) *furthest = std::max(*furthest, std::abs(motion.position));
    }
    return motion;
}
int main() {
    // Five cards 872 apart with the second centred: one card to its left.
    const RowStops row{{-872, 0, 872, 1744, 2616}};

    // Between the ends the row is exactly where the finger puts it.
    check(rowStretch(400, row) == 400, "row does not follow the finger");
    // Past an end it gives, less and less, and never more than its limit.
    const double a = rowStretch(row.last() + 100, row) - row.last();
    const double b = rowStretch(row.last() + 1000, row) - row.last();
    const double c = rowStretch(row.last() + 100000, row) - row.last();
    check(a > 0 && a < 100 && b > a && b < 1000 - 100 && c <= RowEndGive, "end does not give like a spring");
    check(rowStretch(row.first() - 300, row) > row.first() - 300, "first end does not resist");
    for (double raw : {-3000.0, -900.0, 300.0, 2700.0, 4000.0})
        check(std::abs(rowUnstretch(rowStretch(raw, row), row) - raw) < 0.01, "caught row jumps");

    // A slow release springs onto the nearest card.
    RowMotion slow = held(500);
    releaseRow(slow, row, 100);
    check(slow.mode == RowMotion::Mode::Spring && slow.target == 872, "slow release did not aim at the nearest card");
    check(settle(slow, row).position == 872, "slow release did not settle on a card");

    // A flick glides past the nearest card and still lands exactly on one.
    RowMotion flick = held(100);
    releaseRow(flick, row, 2400);
    check(flick.mode == RowMotion::Mode::Glide, "flick does not glide");
    const auto flung = settle(flick, row);
    check(flung.mode == RowMotion::Mode::Rest && flung.position >= 872
          && rowStopIndex(row, flung.position) >= 2, "flick did not glide onto a later card");
    // A harder flick travels further.
    RowMotion harder = held(100);
    releaseRow(harder, row, 4800);
    check(settle(harder, row).position > flung.position, "harder flick did not travel further");

    // Physical review, 27 September: a gentle flick sprang back to the card it
    // left, reaching the end of the row took too much force, and the row
    // braked too hard. On the tablet's 872px pitch, from the first of eight:
    const RowStops eight{{0, 872, 1744, 2616, 3488, 4360, 5232, 6104}};
    const auto landsOn = [&eight](double speed) {
        RowMotion thrown = held(0);
        releaseRow(thrown, eight, speed);
        return rowStopIndex(eight, settle(thrown, eight).position);
    };
    // any flick moves on at least one card, in its direction,
    check(landsOn(450) == 1 && landsOn(1500) == 1, "gentle flick did not reach the next card");
    // a brisk one crosses a few,
    check(landsOn(3000) == 2 && landsOn(4000) == 3, "brisk flick did not cross a few cards");
    // and a hard one reaches the end of a row of eight.
    check(landsOn(7000) == 7, "hard flick did not reach the end");
    for (double speed = 450; speed <= 9000; speed += 50) {
        RowMotion thrown = held(0);
        releaseRow(thrown, eight, speed);
        double last = 0;
        int frames = 0;
        bool backward = false;
        while (stepRow(thrown, eight, 1.0 / 120.0) && ++frames < 2000) {
            // Short of the end's spring, a thrown row never comes back.
            if (thrown.position < last - 0.01 && thrown.position < eight.last()) backward = true;
            last = std::max(last, thrown.position);
        }
        check(!backward, "thrown row came back against the hand");
        check(frames < 120 * 3, "thrown row took too long to rest");
        check(rowStopIndex(eight, thrown.position) >= 1, "thrown row did not land on a card");
    }
    // A fifth of a second after a brisk flick the row still has most of its speed.
    RowMotion brisk = held(0);
    releaseRow(brisk, eight, 3000);
    for (int frame = 0; frame < 24; ++frame) stepRow(brisk, eight, 1.0 / 120.0);
    check(brisk.velocity > 3000 * 0.7, "row braked too hard");
    // A flick back against a half-dragged row returns it.
    RowMotion back = held(600);
    releaseRow(back, eight, -900);
    check(settle(back, eight).position == 0, "flick back did not return the row");

    // A flick toward an end springs back to that end without passing far.
    RowMotion toEnd = held(2000);
    releaseRow(toEnd, row, 6000);
    double furthest = 0;
    const auto ended = settle(toEnd, row, &furthest);
    check(ended.position == row.last(), "row did not stop at its end");
    check(furthest > row.last() && furthest < row.last() + RowEndGive, "end did not spring");
    RowMotion toFirst = held(-1200);
    settleRow(toFirst, row);
    check(settle(toFirst, row).position == row.first(), "stretched first end did not come back");

    // A row with one card goes nowhere.
    const RowStops lone{{0}};
    RowMotion alone = held(rowStretch(-400, lone));
    releaseRow(alone, lone, -3000);
    check(settle(alone, lone).position == 0, "single card row moved");

    // Lifting: up closes past a long lift or a short flick, and not otherwise.
    check(liftOutcome(-300, 0, false) == LiftOutcome::Close, "long lift did not close");
    check(liftOutcome(-60, -1.2, false) == LiftOutcome::Close, "flick up did not close");
    check(liftOutcome(-120, -0.2, false) == LiftOutcome::Return, "short slow lift closed");
    // A Bento group never closes by a flick: up only gives a little and comes
    // back, however hard; down takes the pane out as it does a Stack's card.
    check(groupLiftShown(-900) > -60.5 && groupLiftShown(-900) < 0, "a group rose past its give");
    check(groupLiftShown(200) == 200, "a group pane did not follow a pull down");
    check(groupLiftOutcome(groupLiftShown(-900), -5.0) == LiftOutcome::Return, "a flick up closed a group");
    check(groupLiftOutcome(200, 0.2) == LiftOutcome::PullOut, "a pull down did not take the pane out");
    check(groupLiftOutcome(60, 0.1) == LiftOutcome::Return, "a short pull took the pane out");
    check(liftOutcome(-30, -3.0, true) == LiftOutcome::Return, "twitch closed");
    // Down takes a card out of a Stack, and nothing else.
    check(liftOutcome(150, 0, true) == LiftOutcome::PullOut, "long pull did not take card out");
    check(liftOutcome(50, 1.0, true) == LiftOutcome::PullOut, "quick pull did not take card out");
    check(liftOutcome(100, 0.2, true) == LiftOutcome::Return, "short pull took card out");
    check(liftOutcome(liftShown(400, false), 2.0, false) == LiftOutcome::Return, "pull down on a lone card did something");
    check(liftShown(400, false) < 60 && liftShown(400, true) == 400 && liftShown(-200, false) == -200,
          "lift is not drawn under the finger");

    // Strokes lock once they have a direction, and never before.
    check(lockStroke(6, 5, false, 0) == StrokeAxis::Undecided, "stroke locked before it moved");
    check(lockStroke(12, 3, false, 2) == StrokeAxis::Row, "sideways stroke did not move the row");
    check(lockStroke(-12, 3, true, 0.2) == StrokeAxis::Scrub, "slow stroke on the Stack did not scrub");
    check(lockStroke(-12, 3, true, 1.5) == StrokeAxis::Row, "quick stroke on the Stack did not move the row");
    check(lockStroke(3, -12, true, 0) == StrokeAxis::Lift && lockStroke(8, 9, false, 0) == StrokeAxis::Lift,
          "vertical stroke did not lift");
    check(scrubSteps(-89) == 0 && scrubSteps(-91) == 1 && scrubSteps(185) == -2, "scrub steps wrong");

    // Release speed comes from the stroke's last moments, not its start.
    StrokeVelocity velocity;
    for (int t = 0; t <= 300; t += 10) velocity.add(t, t < 200 ? 0.0 : (t - 200) * 2.0, 0);
    check(std::abs(velocity.speed().x - 2.0) < 0.05, "release speed does not reflect the flick");
    StrokeVelocity still;
    still.add(0, 5, 5);
    check(still.speed().x == 0 && still.speed().y == 0, "single sample has speed");
    return 0;
}

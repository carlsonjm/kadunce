// SPDX-License-Identifier: GPL-2.0-or-later
#include "PaneLeaving.h"
#include <cstdlib>
#include <iostream>
using namespace Kadunce;
void check(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b) { return std::abs(a - b) < 0.001; }
int main() {
    const double d = PaneLeavingDuration;
    // Covered, it dims from whole to its covered opacity and stays there.
    check(near(paneLeavingOpacity(0, d, true), 1.0), "a covered pane starts whole");
    check(near(paneLeavingOpacity(d, d, true), PaneLeavingCoveredOpacity), "a covered pane ends dimmed");
    check(near(paneLeavingOpacity(2 * d, d, true), PaneLeavingCoveredOpacity), "and stays dimmed");
    const double half = paneLeavingOpacity(d / 2, d, true);
    check(half < 1.0 && half > PaneLeavingCoveredOpacity, "it dims as the move goes");
    // Uncovered, it fades out where it stood.
    check(near(paneLeavingOpacity(0, d, false), 1.0), "an uncovered pane starts whole");
    check(near(paneLeavingOpacity(d, d, false), 0.0), "an uncovered pane fades out");
    check(paneLeavingOpacity(d / 2, d, false) < half, "faster than a covered one");
    // Animations off: nothing to draw.
    check(near(paneLeavingOpacity(0, 0, true), 0.0), "no duration, no picture");

    // Four panes: the left column's lower pane closes and the upper grows down over it.
    const QRectF upper(1290, 34, 1082, 679), lower(1290, 723, 1082, 678), column(1290, 34, 1082, 1367);
    const QRectF right(2382, 34, 1082, 1367);
    check(paneLeavingCovered(lower, {column}), "the grown column covers the closed pane");
    check(!paneLeavingCovered(lower, {upper}), "a pane that does not reach it does not");
    check(!paneLeavingCovered(lower, {right}), "nor one in another column");
    // Down to one: the last pane grows across the whole display.
    check(paneLeavingCovered(right, {QRectF(1290, 34, 2174, 1367)}), "the whole display covers it");
    check(!paneLeavingCovered(QRectF(), {column}), "no place, nothing covered");
    // The gap between panes does not stop it counting as covered.
    check(paneLeavingCovered(QRectF(1288, 721, 1086, 682), {column}), "a few pixels past the edge still count");
    std::cout << "pane leaving checks passed\n";
}

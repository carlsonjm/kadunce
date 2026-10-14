/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a pane's face held up and scrubbed passes
// the cards behind it one at a time, and goes in at the place it is let go.
// Four title bars show at a time, as Spread shows four cards of a Stack.
inline constexpr int StackLadderShown = 4;
// Upward travel, in logical pixels, that passes one card.
inline constexpr double StackScrubStep = 56.0;
// Height of each title bar the ladder shows above the card in front of it.
inline constexpr double StackLadderRung = 30.0;

struct StackScrub {
    int size = 0;
    int from = 0;
    int place = 0;
    double startY = 0.0;
    int stepped = 0;

    void begin(int stackSize, int depth, double y)
    {
        size = stackSize;
        from = place = std::clamp(depth, 0, std::max(stackSize - 1, 0));
        startY = y;
        stepped = 0;
    }
    [[nodiscard]] bool active() const { return size > 1; }
    // The hand at `y`: every step of upward travel is one card passed, plus
    // the wheel's steps. Travel back down returns it toward where it began.
    int follow(double y)
    {
        const int travelled = int(std::max(0.0, startY - y) / StackScrubStep);
        place = std::clamp(from + travelled + stepped, 0, size - 1);
        return place;
    }
    // One wheel notch away from the person passes one card; toward, back one.
    int wheel(int notches, double y)
    {
        stepped = std::clamp(stepped + notches, -from, size - 1);
        return follow(y);
    }
    // The first place the ladder shows: the card last passed, the held card,
    // then those next behind it.
    [[nodiscard]] int ladderStart() const
    {
        return std::clamp(place - 1, 0, std::max(size - StackLadderShown, 0));
    }
    [[nodiscard]] int ladderCount() const { return std::min(size, StackLadderShown); }
    void clear() { *this = StackScrub{}; }
};
} // namespace Kadunce

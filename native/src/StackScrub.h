/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <cmath>

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

// How long the title bar is held before the hand moves for the drag to scrub
// rather than carry the pane, and how far the hand moves before that is told.
inline constexpr long long StackScrubHoldMs = 350;
inline constexpr double StackScrubSlop = 8.0;

enum class StackScrubStart { Undecided, Scrub, Carry };
// A hold, then travel mostly upward, scrubs; a drag begun at once, or one
// leaving sideways or down, carries the pane as before.
inline StackScrubStart stackScrubStart(double dx, double dy, long long heldMs)
{
    if (std::hypot(dx, dy) < StackScrubSlop) return StackScrubStart::Undecided;
    return heldMs >= StackScrubHoldMs && dy < 0.0 && -dy >= std::abs(dx)
        ? StackScrubStart::Scrub : StackScrubStart::Carry;
}

// Where the ladder draws the card `rung` places from its front (0 is the one
// in front, drawn whole) of `count` shown, over a pane at x, y, width, height.
// Each card behind stands one rung higher and a little narrower, so its
// title bar shows above the card in front of it.
struct StackRungBox { double x = 0, y = 0, width = 0, height = 0; };
inline StackRungBox stackLadderRung(double x, double y, double width, double height,
                                    int rung, int count)
{
    const int n = std::max(count, 1);
    const int r = std::clamp(rung, 0, n - 1);
    const double inset = width * 0.01 * r;
    return {x + inset, y + (n - 1 - r) * StackLadderRung, width - 2 * inset,
            std::max(0.0, height - (n - 1) * StackLadderRung)};
}

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
    // A key passes one card a press; past the back it comes round to the
    // front, as a Stack is a ring. Back toward the front it stops there.
    int stepKey(int delta)
    {
        if (size <= 1) return place;
        place = delta > 0 ? (place + delta) % size : std::max(place + delta, 0);
        stepped = place - from;
        return place;
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

// A pane's whole Stack, face first, with the face let go at `place`: the
// cards it passed come forward, and the first is the pane's face after.
template<class List>
List stackScrubOrder(const typename List::value_type &face, const List &behind, int place)
{
    List order = behind;
    const int at = std::clamp(place, 0, int(behind.size()));
    order.insert(order.begin() + at, face);
    return order;
}
} // namespace Kadunce

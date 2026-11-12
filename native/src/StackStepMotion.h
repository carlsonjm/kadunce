// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace Kadunce {
// A Stack standing narrowed in the middle steps in place, as a deck: only the
// two cards trading places show. Stepping on, the front card slides a short
// way toward the room on its left, sinks and tucks behind the card coming
// forward; stepping back, the card before comes out from behind and settles
// in front. Nothing crosses the room beside the Stack.
inline constexpr int StackStepDuration = 300;
// How far the card going round the deck slides out, as a share of its width.
inline constexpr double StackStepReach = 0.16;
// How much smaller, and darker, a card stands once behind.
inline constexpr double StackStepSink = 0.05;
inline constexpr double StackStepShade = 0.18;
// How far it rises, and turns, on its way round.
inline constexpr double StackStepLift = 10.0;
inline constexpr double StackStepTurn = -1.2;

struct StackStepPose {
    double dx = 0.0;
    double dy = 0.0;
    double scale = 1.0;
    double opacity = 1.0;
    double brightness = 1.0;
    double rotation = 0.0;
};

namespace StackStepDetail {
inline double easeOut(double t) { t = std::clamp(t, 0.0, 1.0); return 1.0 - std::pow(1.0 - t, 3.0); }
inline double easeInOut(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
}
}

// Where a card of the step stands at `progress`, 0 to 1 in time. `forward`
// steps on to the card after; `arriving` is the card that ends in front, drawn
// over the one leaving, so it fades in rather than passing through it.
[[nodiscard]] inline StackStepPose stackStepPose(double progress, bool forward, bool arriving, double cardWidth)
{
    using namespace StackStepDetail;
    const double p = std::clamp(progress, 0.0, 1.0);
    const double eased = easeInOut(p);
    // 0 in front, 1 behind, for the card that goes round the deck.
    const double depth = forward ? eased : 1.0 - eased;
    const bool goesRound = forward != arriving;
    StackStepPose pose;
    if (goesRound) {
        const double away = p < 0.5 ? easeOut(p / 0.5) : 1.0 - easeInOut((p - 0.5) / 0.5);
        const double arc = std::sin(M_PI * p);
        pose.dx = -away * cardWidth * StackStepReach;
        pose.dy = -StackStepLift * arc;
        pose.rotation = StackStepTurn * arc;
        pose.scale = 1.0 - StackStepSink * depth;
        pose.brightness = 1.0 - StackStepShade * depth;
    } else {
        pose.scale = 1.0 - StackStepSink * (1.0 - depth);
        pose.brightness = 1.0 - StackStepShade * (1.0 - depth);
    }
    if (arriving) pose.opacity = easeInOut((p - 0.1) / 0.45);
    return pose;
}
}

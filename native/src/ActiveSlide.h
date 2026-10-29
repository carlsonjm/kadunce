// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace Kadunce {
// The Active card's turn along the row: the leaving card and the one arriving
// move as one strip, at full size, a gap apart. Its position runs from 0, the
// leaving card in place, to 1, the arriving card in place. At a row end there
// is no arriving card, and the one card gives a little and settles back.
inline constexpr int ActiveSlideDuration = 260;
inline constexpr double ActiveSlideGap = 10.0;
// Let go past this share of the card's width, or flicked, the turn goes on.
inline constexpr double ActiveSlideCommitShare = 0.25;
// A flick: the hand leaving at this many card widths a second, or faster.
inline constexpr double ActiveSlideFlick = 1.2;
// How far a card gives at a row end, as a share of its width.
inline constexpr double ActiveSlideGive = 0.06;

[[nodiscard]] inline double easeOutCubic(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
}

[[nodiscard]] inline double easeInOutCubic(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

struct ActiveSlideMotion {
    enum class Shape { InOut, Out, Give };
    double from = 0.0;
    double to = 0.0;
    int durationMs = 1;
    Shape shape = Shape::InOut;
};

// Where a motion stands after so many milliseconds. A give goes out and back
// on one clock: out with the hand's ease, home with the same.
[[nodiscard]] inline double activeSlideAt(const ActiveSlideMotion &motion, double elapsedMs)
{
    const double t = std::clamp(elapsedMs / std::max(1, motion.durationMs), 0.0, 1.0);
    if (motion.shape == ActiveSlideMotion::Shape::Give) {
        const double out = t < 0.5 ? easeOutCubic(t * 2.0) : 1.0 - easeInOutCubic((t - 0.5) * 2.0);
        return motion.from + (motion.to - motion.from) * out;
    }
    const double eased = motion.shape == ActiveSlideMotion::Shape::Out ? easeOutCubic(t) : easeInOutCubic(t);
    return motion.from + (motion.to - motion.from) * eased;
}

[[nodiscard]] inline bool activeSlideDone(const ActiveSlideMotion &motion, double elapsedMs)
{
    return elapsedMs >= motion.durationMs;
}

// The hand's travel toward the arriving card, as a position. Past the row end
// there is nothing to bring in, and the card resists, reaching at most a give.
[[nodiscard]] inline double activeSlideTracked(double travel, double pitch, bool hasNext)
{
    if (pitch <= 0.0) return 0.0;
    const double share = std::max(0.0, travel / pitch);
    if (hasNext) return std::min(share, 1.0);
    return ActiveSlideGive * (1.0 - std::exp(-share / ActiveSlideGive));
}

// Let go: whether the turn completes, from where the strip stands and how
// fast the hand left, in card widths a second toward the arriving card.
[[nodiscard]] inline bool activeSlideCommits(double position, double speed)
{
    return position >= ActiveSlideCommitShare || speed >= ActiveSlideFlick;
}

// How far each card of the strip stands from its place. `side` is where the
// arriving card comes from: 1 from the right, the card after; -1 from the
// left, the card before.
[[nodiscard]] inline double activeSlideLeavingShift(double position, int side, double pitch)
{
    return -side * position * pitch;
}

[[nodiscard]] inline double activeSlideArrivingShift(double position, int side, double pitch)
{
    return side * (1.0 - position) * pitch;
}
}

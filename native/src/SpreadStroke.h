// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <deque>

namespace Kadunce {

// A one-finger stroke in Spread means one thing, decided once it has moved far
// enough to have a direction: sideways moves the row, up or down lifts the card
// under the finger. A slow sideways start on the centred Stack thumbs through
// its cards instead, so a quick stroke always moves the row.
enum class StrokeAxis { Undecided, Row, Scrub, Lift };

inline constexpr double StrokeLockDistance = 10.0;
inline constexpr double StrokeSidewaysRatio = 1.15;
// Pixels per millisecond: a sideways start slower than this on the centred
// Stack scrubs it.
inline constexpr double ScrubSpeed = 0.5;
// Sideways travel that brings the next card of a Stack forward.
inline constexpr double ScrubStep = 90.0;

inline StrokeAxis lockStroke(double dx, double dy, bool onCentredStack, double sidewaysSpeed)
{
    if (std::hypot(dx, dy) < StrokeLockDistance) return StrokeAxis::Undecided;
    if (std::abs(dx) > std::abs(dy) * StrokeSidewaysRatio)
        return onCentredStack && std::abs(sidewaysSpeed) < ScrubSpeed
            ? StrokeAxis::Scrub : StrokeAxis::Row;
    return StrokeAxis::Lift;
}

// Cards a scrub has stepped for its sideways travel since it began. A finger
// moving left brings later cards forward, as the row does.
inline int scrubSteps(double travel)
{
    return static_cast<int>(std::trunc(-travel / ScrubStep));
}

// Recent finger samples, for the speed a stroke is let go at.
class StrokeVelocity
{
public:
    struct Speed { double x = 0.0; double y = 0.0; };

    void clear() { m_samples.clear(); }
    void add(double milliseconds, double x, double y)
    {
        m_samples.push_back({milliseconds, x, y});
        while (m_samples.size() > 40) m_samples.pop_front();
    }
    // Pixels per millisecond over the last 70 ms or so of the stroke.
    [[nodiscard]] Speed speed() const
    {
        if (m_samples.size() < 2) return {};
        const Sample &latest = m_samples.back();
        Sample earlier = m_samples.front();
        for (auto it = m_samples.rbegin() + 1; it != m_samples.rend(); ++it) {
            earlier = *it;
            if (latest.t - it->t >= 70.0) break;
        }
        const double elapsed = std::max(8.0, latest.t - earlier.t);
        return {(latest.x - earlier.x) / elapsed, (latest.y - earlier.y) / elapsed};
    }

private:
    struct Sample { double t; double x; double y; };
    std::deque<Sample> m_samples;
};

} // namespace Kadunce

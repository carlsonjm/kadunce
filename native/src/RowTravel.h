// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace Kadunce {

// The Spread row as something a hand moves. A position is logical pixels along
// the row: zero centres the selected entry, and each stop is the position that
// centres another entry. The row has two ends, and past either one it gives a
// little and springs back.
inline constexpr double RowEndGive = 260.0;
inline constexpr double RowEndGiveScale = 520.0;
// A thrown row slides like a deck on a table: it loses the same speed every
// second, so a flick twice as fast travels four times as far, and a hard one
// reaches the end of a long row. Pixels per second squared.
inline constexpr double RowFriction = 3000.0;
// Pixels per second: a release faster than this is a flick.
inline constexpr double RowCoastRelease = 400.0;
// Seconds: the longest a gentle flick takes to glide onto the next card.
inline constexpr double RowGlideLongest = 0.6;
// Critically damped springs, per second.
inline constexpr double RowSpring = 15.0;
inline constexpr double LiftSpring = 15.0;
// A touch landing on a row moving faster than this stops it where it is.
inline constexpr double RowCatchSpeed = 220.0;

struct RowStops {
    std::vector<double> stops;
    [[nodiscard]] double first() const { return stops.empty() ? 0.0 : stops.front(); }
    [[nodiscard]] double last() const { return stops.empty() ? 0.0 : stops.back(); }
};

inline double rowEndGive(double beyond)
{
    return RowEndGive * (1.0 - std::exp(-std::max(0.0, beyond) / RowEndGiveScale));
}

// Where the row stands when a finger asks for `raw`.
inline double rowStretch(double raw, const RowStops &row)
{
    if (raw < row.first()) return row.first() - rowEndGive(row.first() - raw);
    if (raw > row.last()) return row.last() + rowEndGive(raw - row.last());
    return raw;
}

// The finger position that shows `shown`, so a row caught past an end is
// picked up where it stands rather than jumping.
inline double rowUnstretch(double shown, const RowStops &row)
{
    const auto undo = [](double give) {
        const double fraction = std::clamp(give / RowEndGive, 0.0, 0.999);
        return -RowEndGiveScale * std::log(1.0 - fraction);
    };
    if (shown < row.first()) return row.first() - undo(row.first() - shown);
    if (shown > row.last()) return row.last() + undo(shown - row.last());
    return shown;
}

inline double nearestRowStop(const RowStops &row, double position)
{
    double best = position;
    double distance = -1.0;
    for (double stop : row.stops) {
        const double d = std::abs(stop - position);
        if (distance < 0.0 || d < distance) {
            distance = d;
            best = stop;
        }
    }
    return best;
}

inline int rowStopIndex(const RowStops &row, double stop)
{
    for (std::size_t i = 0; i < row.stops.size(); ++i)
        if (std::abs(row.stops[i] - stop) < 0.5) return static_cast<int>(i);
    return -1;
}

// The first stop past `position` in `direction`, or the end the row stops at.
inline double nextRowStop(const RowStops &row, double position, double direction)
{
    if (direction > 0) {
        for (double stop : row.stops)
            if (stop > position + 0.5) return stop;
        return row.last();
    }
    for (auto it = row.stops.rbegin(); it != row.stops.rend(); ++it)
        if (*it < position - 0.5) return *it;
    return row.first();
}

// One step of a critically damped spring toward `target`, in substeps short
// enough to stay stable on a slow frame.
inline void springToward(double &position, double &velocity, double target,
                         double omega, double seconds)
{
    const int steps = std::max(1, static_cast<int>(std::ceil(seconds / 0.008)));
    const double h = seconds / steps;
    for (int i = 0; i < steps; ++i) {
        const double acceleration = -omega * omega * (position - target) - 2.0 * omega * velocity;
        velocity += acceleration * h;
        position += velocity * h;
    }
}

struct RowMotion {
    // A glide lands a flick on the card it chose when let go; a coast carries
    // one thrown past an end until the end's spring takes it.
    enum class Mode { Rest, Drag, Glide, Coast, Spring };
    Mode mode = Mode::Rest;
    double position = 0.0;
    // Pixels per second along the row.
    double velocity = 0.0;
    double target = 0.0;
    struct Glide {
        double from = 0.0;
        // The release velocity times the glide's duration.
        double reach = 0.0;
        double seconds = 0.0;
        double elapsed = 0.0;
    } glide;

    [[nodiscard]] bool moving() const
    {
        return mode == Mode::Glide || mode == Mode::Coast || mode == Mode::Spring;
    }
    [[nodiscard]] bool still() const { return mode == Mode::Rest && position == 0.0; }
};

// The finger let go of the row at `velocity` pixels per second along it. A
// flick chooses its card now, where the slide would stop, and always at least
// the next card in its direction; it never comes back against the hand.
inline void releaseRow(RowMotion &motion, const RowStops &row, double velocity)
{
    motion.velocity = velocity;
    const double speed = std::abs(velocity);
    if (speed <= RowCoastRelease) {
        motion.mode = RowMotion::Mode::Spring;
        motion.target = nearestRowStop(row, motion.position);
        return;
    }
    const double direction = velocity > 0 ? 1.0 : -1.0;
    const double slide = motion.position + direction * speed * speed / (2.0 * RowFriction);
    if (slide < row.first() || slide > row.last()) {
        motion.mode = RowMotion::Mode::Coast;
        return;
    }
    double target = nearestRowStop(row, slide);
    const double next = nextRowStop(row, motion.position, direction);
    if ((target - next) * direction < 0.0) target = next;
    const double distance = std::abs(target - motion.position);
    if (distance < 0.5 || (target - motion.position) * direction < 0.0) {
        motion.mode = RowMotion::Mode::Spring;
        motion.target = target;
        return;
    }
    // Slowing evenly lands in 2d/v. A card further than the slide would reach
    // is glided onto a little faster, and a nearer one a little sooner, never
    // so soon that the row passes it and comes back.
    double seconds = std::min(2.0 * distance / speed,
                              std::max(RowGlideLongest, speed / RowFriction));
    seconds = std::min(seconds, 3.0 * distance / speed);
    motion.mode = RowMotion::Mode::Glide;
    motion.target = target;
    motion.glide = {motion.position, velocity * seconds, seconds, 0.0};
}

inline void settleRow(RowMotion &motion, const RowStops &row)
{
    motion.mode = RowMotion::Mode::Spring;
    motion.target = nearestRowStop(row, motion.position);
}

// Advance a released row by `seconds`. Returns whether it is still moving;
// once it is not, its position is exactly one of the stops.
inline bool stepRow(RowMotion &motion, const RowStops &row, double seconds)
{
    if (motion.mode == RowMotion::Mode::Glide) {
        // One cubic from the release to the card: it leaves at the finger's
        // speed and arrives at rest, exactly on the card.
        RowMotion::Glide &glide = motion.glide;
        glide.elapsed += seconds;
        const double s = std::clamp(glide.elapsed / glide.seconds, 0.0, 1.0);
        const double span = motion.target - glide.from;
        motion.position = glide.from + glide.reach * (s * s * s - 2.0 * s * s + s)
            + span * (3.0 * s * s - 2.0 * s * s * s);
        motion.velocity = (glide.reach * (3.0 * s * s - 4.0 * s + 1.0)
            + span * (6.0 * s - 6.0 * s * s)) / glide.seconds;
        if (s < 1.0) return true;
        motion.position = motion.target;
        motion.velocity = 0.0;
        motion.mode = RowMotion::Mode::Rest;
        return false;
    }
    if (motion.mode == RowMotion::Mode::Coast) {
        const double direction = motion.velocity > 0 ? 1.0 : -1.0;
        const double slowed = std::max(0.0, std::abs(motion.velocity) - RowFriction * seconds);
        motion.position += direction * (std::abs(motion.velocity) + slowed) / 2.0 * seconds;
        motion.velocity = direction * slowed;
        if (motion.position < row.first() || motion.position > row.last()) {
            motion.mode = RowMotion::Mode::Spring;
            motion.target = motion.position < row.first() ? row.first() : row.last();
        } else if (slowed == 0.0) {
            motion.mode = RowMotion::Mode::Spring;
            motion.target = nearestRowStop(row, motion.position);
        }
        return true;
    }
    if (motion.mode != RowMotion::Mode::Spring) return false;
    springToward(motion.position, motion.velocity, motion.target, RowSpring, seconds);
    // Within a pixel and nearly still is at rest: the last creep is invisible,
    // and a Stack waits for rest to open its fan.
    if (std::abs(motion.position - motion.target) < 1.0 && std::abs(motion.velocity) < 20.0) {
        motion.position = motion.target;
        motion.velocity = 0.0;
        motion.mode = RowMotion::Mode::Rest;
        return false;
    }
    return true;
}

// Where a released row comes to rest, at once: the card a glide or spring is
// bound for, or for a coast, where its slide stops or the end it reaches.
inline void finishRow(RowMotion &motion, const RowStops &row)
{
    if (motion.mode == RowMotion::Mode::Coast) {
        const double speed = std::abs(motion.velocity);
        const double direction = motion.velocity > 0 ? 1.0 : -1.0;
        const double slide = motion.position + direction * speed * speed / (2.0 * RowFriction);
        motion.target = slide < row.first() ? row.first()
            : slide > row.last() ? row.last() : nearestRowStop(row, slide);
    } else if (motion.mode != RowMotion::Mode::Glide && motion.mode != RowMotion::Mode::Spring) {
        return;
    }
    motion.position = motion.target;
    motion.velocity = 0.0;
    motion.mode = RowMotion::Mode::Rest;
}

// A card lifted off the row by an upward or downward stroke. Travel is the
// finger's vertical distance, negative upward; velocity is pixels per
// millisecond, negative upward. Up closes; down takes a card out of its Stack.
inline constexpr double CloseTravel = 280.0;
inline constexpr double CloseFlickTravel = 50.0;
inline constexpr double CloseFlickSpeed = 0.8;
inline constexpr double PullTravel = 140.0;
inline constexpr double PullFlickTravel = 40.0;
inline constexpr double PullFlickSpeed = 0.7;
// A thrown card leaves at least this fast, in pixels per second.
inline constexpr double CloseThrowSpeed = 1200.0;

enum class LiftOutcome { Return, Close, PullOut };

// Where the card is drawn for the finger's travel. A card that is not in a
// Stack has nothing to come out of, so a pull down only gives a little.
inline double liftShown(double travel, bool inStack)
{
    if (travel <= 0.0 || inStack) return travel;
    return 60.0 * (1.0 - std::exp(-travel / 120.0));
}

inline LiftOutcome liftOutcome(double shown, double velocity, bool inStack)
{
    if (shown < 0.0 && (-shown > CloseTravel
            || (velocity < -CloseFlickSpeed && -shown > CloseFlickTravel)))
        return LiftOutcome::Close;
    if (inStack && shown > 0.0 && (shown > PullTravel
            || (velocity > PullFlickSpeed && shown > PullFlickTravel)))
        return LiftOutcome::PullOut;
    return LiftOutcome::Return;
}

// A pane of a Bento group comes out by a pull down, and a flick up never
// closes a group, so up only gives a little and springs back.
inline double groupLiftShown(double travel)
{
    if (travel >= 0.0) return travel;
    return -60.0 * (1.0 - std::exp(travel / 120.0));
}

inline LiftOutcome groupLiftOutcome(double shown, double velocity)
{
    return liftOutcome(shown, velocity, true) == LiftOutcome::PullOut
        ? LiftOutcome::PullOut : LiftOutcome::Return;
}

} // namespace Kadunce

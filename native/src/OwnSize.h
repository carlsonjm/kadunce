/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace Kadunce
{
// CARD-LIFECYCLE.md §2, A card that sizes itself: where such a card stands.
//
// The application chooses its size and whether it fills the screen; Cards
// holds it to what it chose. An application that draws its own title bar may
// answer a maximize or a restore by asking again for the size it had a moment
// before, and KWin gives a window with no frame of KWin's what it asks for,
// maximized or not. So the size the card had before it filled the screen is
// kept here, and a size asked for in the moment after it starts or stops
// filling the screen is not taken for a new size of its own.

struct OwnRect {
    double x = 0, y = 0, width = 0, height = 0;
};

[[nodiscard]] inline bool ownRectsMatch(const OwnRect &a, const OwnRect &b)
{
    return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1
        && std::abs(a.width - b.width) <= 1 && std::abs(a.height - b.height) <= 1;
}

// How the window fills its display: not at all, the whole work area, or some
// other way (full screen, one direction only) that is left as it is.
enum class OwnFill { None, WorkArea, Other };

// From what KWin says of a window, what it has and what was asked of it: one
// on its way into or out of filling the screen, or filling one direction only,
// fills it some other way.
[[nodiscard]] constexpr OwnFill ownFill(bool fullScreen, bool restored, bool restoreAsked, bool whole, bool wholeAsked)
{
    if (fullScreen) return OwnFill::Other;
    if (restored && restoreAsked) return OwnFill::None;
    return whole && wholeAsked ? OwnFill::WorkArea : OwnFill::Other;
}

// The moment, in milliseconds, after a card starts or stops filling the
// screen in which a size it asks for is an answer to that change.
constexpr long long OwnSizeSettleMs = 700;
// How often a card is put back each time it starts or stops filling the
// screen, or is shown, so an application that insists is not fought in a loop.
constexpr int OwnSizeAsserts = 3;

struct OwnSizeRecord {
    // The size the card has when it does not fill the screen; none known yet
    // for one first seen filling it.
    double width = 0, height = 0;
    OwnFill fill = OwnFill::None;
    bool seen = false;
    // When it last started or stopped filling the screen.
    long long changedAt = 0;
    // When its size or state last changed, and how often it was put back.
    long long lastChangeAt = 0;
    int asserts = 0;

    [[nodiscard]] bool hasSize() const { return width > 0 && height > 0; }
    [[nodiscard]] bool settling(long long now) const { return seen && now - changedAt <= OwnSizeSettleMs; }
};

// One change of the window's size or state, as KWin reports it. `sizedByHand`
// is a person dragging its edge, whose size is always its own.
inline void noteOwnSize(OwnSizeRecord &record, OwnFill fill, const OwnRect &frame, long long now,
                        bool sizedByHand = false)
{
    record.lastChangeAt = now;
    if (!record.seen) {
        record.seen = true;
        record.fill = fill;
        record.changedAt = now - OwnSizeSettleMs - 1;
        if (fill == OwnFill::None) {
            record.width = frame.width;
            record.height = frame.height;
        }
        return;
    }
    if (fill != record.fill) {
        const bool restored = fill == OwnFill::None;
        record.fill = fill;
        record.changedAt = now;
        record.asserts = 0;
        // One first seen filling the screen takes the size KWin restores it to.
        if (restored && !record.hasSize()) {
            record.width = frame.width;
            record.height = frame.height;
        }
        return;
    }
    if (fill != OwnFill::None) return;
    if (sizedByHand || !record.settling(now)) {
        record.width = frame.width;
        record.height = frame.height;
    }
}

// How long after a change the card is looked at: at once while it settles
// into filling the screen or out of it, later otherwise, so an application
// sizing itself step by step is left to finish.
[[nodiscard]] inline int ownSizeWaitMs(const OwnSizeRecord &record, long long now)
{
    return record.settling(now) ? 40 : 200;
}

// Where the card stands: the whole work area while it fills it, else its own
// size in the middle of the Active place and no larger than it, unless it
// cannot be that small. None while it fills the screen another way.
[[nodiscard]] inline std::optional<OwnRect> ownPlace(const OwnSizeRecord &record, OwnFill fill, const OwnRect &frame,
                                                     const OwnRect &place, const OwnRect &workArea,
                                                     double leastWidth = 0, double leastHeight = 0)
{
    if (fill == OwnFill::Other) return std::nullopt;
    if (fill == OwnFill::WorkArea) return workArea;
    const double ownWidth = record.hasSize() ? record.width : frame.width;
    const double ownHeight = record.hasSize() ? record.height : frame.height;
    const double width = std::max({1.0, leastWidth, std::min(ownWidth, place.width)});
    const double height = std::max({1.0, leastHeight, std::min(ownHeight, place.height)});
    // Whole points, its top and left kept inside the place.
    return OwnRect{place.x + std::max(0.0, std::floor((place.width - width) / 2)),
                   place.y + std::max(0.0, std::floor((place.height - height) / 2)), width, height};
}

// Whether the card is moved to `wanted` now: it stands elsewhere and has not
// been put back too often since it last changed how it fills the screen.
[[nodiscard]] inline bool ownSizeAsserts(OwnSizeRecord &record, const OwnRect &frame, const OwnRect &wanted)
{
    if (ownRectsMatch(frame, wanted) || record.asserts >= OwnSizeAsserts) return false;
    ++record.asserts;
    return true;
}

// CARD-LIFECYCLE.md §5, a pane that fills the screen: a pane has no size of
// its own, so in the same moment it is held to the work area while it fills
// it, and to its pane once it does not. A pane first seen is one just placed,
// and is held the same way.
struct PaneHoldRecord {
    OwnFill fill = OwnFill::None;
    bool seen = false;
    long long changedAt = 0;
    int asserts = 0;

    [[nodiscard]] bool settling(long long now) const { return seen && now - changedAt <= OwnSizeSettleMs; }
};

// One reading of how the pane fills the screen. Whether the moment began.
inline bool notePaneFill(PaneHoldRecord &record, OwnFill fill, long long now)
{
    if (record.seen && fill == record.fill) return false;
    record = {fill, true, now, 0};
    return true;
}

// Whether `frame` reaches past `place` on any side, and so over what stands
// beside it; one short of its place covers nothing.
[[nodiscard]] inline bool ownRectReachesPast(const OwnRect &frame, const OwnRect &place)
{
    return frame.x < place.x - 1.5 || frame.y < place.y - 1.5
        || frame.x + frame.width > place.x + place.width + 1.5
        || frame.y + frame.height > place.y + place.height + 1.5;
}

// Where the pane is put now, if anywhere: the work area while it fills it and
// stands elsewhere, its pane while it does not and reaches past it. Nothing
// outside the moment, or once it has been put back too often in it.
[[nodiscard]] inline std::optional<OwnRect> paneHoldPlace(PaneHoldRecord &record, const OwnRect &frame,
                                                          const OwnRect &pane, const OwnRect &workArea, long long now)
{
    if (!record.settling(now) || record.fill == OwnFill::Other || record.asserts >= OwnSizeAsserts) return std::nullopt;
    const bool fills = record.fill == OwnFill::WorkArea;
    if (fills ? ownRectsMatch(frame, workArea) : !ownRectReachesPast(frame, pane)) return std::nullopt;
    ++record.asserts;
    return fills ? workArea : pane;
}
}

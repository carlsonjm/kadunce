// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "SpreadStroke.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Kadunce {

// A card held in Spread and the row beneath it. The held card is out of the
// row; the rest keep their order, parted by a gap where it would land. The row
// stays three across. Leaned off the middle it slides under the card, faster
// the further out, and closes up while it slides; stopped, it parts under the
// card. Pulled down, the card takes the row to one set zoom and back up it
// returns. Positions are logical pixels along the row at full size: item k
// stands k pitches along, a gap further once it is past the gap, and the row's
// position is the row coordinate drawn at the screen's centre. A card held in
// its own Stack is lifted out of it instead: the Stack parts front to back at
// the place it would take, and sideways travel moves that place a card at a
// time.

// Travel from the pickup that makes a hold a carry.
inline constexpr double CarryMoveDistance = 16.0;
// Carried off the middle, the card leans the row: the middle of the screen,
// this fraction of its width either side of the centre, holds it still; past
// that it slides, reaching full speed this far out, in logical pixels per
// second on screen. The speed grows as the lean to this power, so a small
// lean creeps.
inline constexpr double CarrySlideBand = 0.07;
inline constexpr double CarrySlideFull = 0.3;
inline constexpr double CarrySlideSpeed = 1500.0;
inline constexpr double CarrySlideCurve = 1.5;
// Slower than this on screen, in logical pixels per second, a leaned row only
// creeps: it stays parted under the card, so a small lean does not pass the
// card beside it.
inline constexpr double CarrySlideShown = 150.0;
// Zoomed out to show whole, the row keeps this margin clear across the screen.
inline constexpr double CarryStepBackMargin = 170.0;
// Carried, the card's place in the row opens this fraction of a pitch; a
// card only picked up keeps its whole place.
inline constexpr double CarryGapWidth = 0.7;
// Pulled this far below where it was picked up, in logical pixels, a held card
// takes the row to one set zoom, no further out than shows all of it; back
// above `CarryZoomRelease` it returns to three across. Either way it glides
// for `CarryZoomDuration` milliseconds.
inline constexpr double CarryZoomScale = 0.32;
inline constexpr double CarryZoomEngage = 120.0;
inline constexpr double CarryZoomRelease = 60.0;
inline constexpr int CarryZoomDuration = 320;
// Let go, a carried card lands in one move: it drops into its place in the
// line, and the row grows back to three across around it, over this long.
inline constexpr int CarryLandDuration = 460;
// Picked up or carried, a card stands a little larger than the row's cards.
inline constexpr double CarryHeldRise = 1.05;
// Within this fraction of a card's width of its centre, the held card is
// over that card rather than a gap, and joins it once it has rested there.
// Once it would join, it keeps the card until it is carried a little further
// off than that.
inline constexpr double CarryJoinReach = 0.32;
inline constexpr double CarryJoinKeep = 1.3;
// Over a Bento group the held card names the pane under the finger, so the
// group is reached across nearly its whole width, and kept no further off
// than its own edge.
inline constexpr double CarryGroupReach = 0.45;
inline constexpr double CarryGroupKeep = 0.5;
inline constexpr int CarryJoinDwell = 250;
// A new gap waits this long, so a card passing over another does not open
// and close the row behind it.
inline constexpr int CarryGapDwell = 60;
// The card a held one rests on rises while it waits, and this far once it
// would take it.
inline constexpr double CarryJoinRise = 1.1;
// Come to rest over a card, the row settles it under the held one, easing with
// this time constant in seconds.
inline constexpr double CarrySettleTime = 0.07;
// A card held in its own Stack follows the finger only this far, easing to a
// stop, in logical pixels, so it never looks free to leave, and stands lifted
// this fraction of its height above the place it would take.
inline constexpr double CarryStackLeash = 40.0;
inline constexpr double CarryStackLift = 0.16;

// Row coordinate of item k, given how far it has moved aside for the gap:
// zero before it, one after it, and in between while the gap moves, for a
// gap `gap` pitches wide.
inline double carryEntryPosition(int k, double shift, double pitch, double gap = 1.0)
{
    return (k + std::clamp(shift, 0.0, 1.0) * gap) * pitch;
}

// A carried row of `entries` and its gap, from the outer edge of its first
// card to the outer edge of its last.
inline double carryRowSpan(int entries, double pitch, double cardWidth)
{
    return (std::max(entries, 1) - 1 + CarryGapWidth) * pitch + cardWidth;
}

// Whether the whole row and its gap fit on screen at `scale`.
inline bool carryRowFits(int entries, double pitch, double cardWidth, double screenWidth,
                         double scale)
{
    return carryRowSpan(entries, pitch, cardWidth) * scale <= screenWidth - CarryStepBackMargin + 0.5;
}

// The set zoom a pulled-down card takes a row of `entries` to, no further out
// than shows all of it.
inline double carryZoomLevel(int entries, double pitch, double cardWidth, double screenWidth)
{
    if (entries <= 0 || pitch <= 0.0) return 1.0;
    const double fit = (screenWidth - CarryStepBackMargin) / carryRowSpan(entries, pitch, cardWidth);
    return std::clamp(std::max(fit, CarryZoomScale), 0.05, 1.0);
}

// Whether a card pulled `pull` logical pixels below where it was picked up
// holds the row zoomed out, given whether it did.
inline bool carryZoomed(bool zoomed, double pull)
{
    return zoomed ? pull > CarryZoomRelease : pull > CarryZoomEngage;
}

// The zoom `progress` of the way from `from` to `to`, eased in and out. It
// changes by the same ratio each moment, so it reads as one steady move.
inline double carryZoomAt(double from, double to, double progress)
{
    if (from <= 0.0 || to <= 0.0) return to;
    const double p = std::clamp(progress, 0.0, 1.0);
    const double eased = p < 0.5 ? 4.0 * p * p * p : 1.0 - std::pow(-2.0 * p + 2.0, 3.0) / 2.0;
    return from * std::pow(to / from, eased);
}

// A landing's progress for the row, given its eased time: the share of the
// way from where each card stood at `fromScale` to where it rests, such that
// the row's scale changes by the same ratio each moment.
inline double carryLandProgress(double fromScale, double eased)
{
    const double e = std::clamp(eased, 0.0, 1.0);
    if (fromScale <= 0.0 || fromScale >= 0.999) return e;
    return (std::pow(fromScale, 1.0 - e) - fromScale) / (1.0 - fromScale);
}

// Where the lean starts on each side. A card picked up out beyond the middle
// leans nothing until it is carried further out than where it was picked
// up; carried back in, that point follows it to the middle's edge.
struct CarryLean {
    double low = 0.0;
    double high = 0.0;
};

inline CarryLean carryLeanStart(double pickupX, double screenLeft, double screenWidth)
{
    const double centre = screenLeft + screenWidth / 2.0;
    const double band = CarrySlideBand * screenWidth;
    return {std::min(pickupX, centre - band), std::max(pickupX, centre + band)};
}

inline CarryLean carryLeanFollow(CarryLean lean, double fingerX, double screenLeft, double screenWidth)
{
    const double centre = screenLeft + screenWidth / 2.0;
    const double band = CarrySlideBand * screenWidth;
    lean.low = std::max(lean.low, std::min(fingerX, centre - band));
    lean.high = std::min(lean.high, std::max(fingerX, centre + band));
    return lean;
}

// How far the finger at `fingerX` leans the row: nothing inside where the
// lean starts, to all the way out, negative toward the left.
inline double carryLeanDepth(double fingerX, const CarryLean &lean, double screenLeft,
                             double screenWidth)
{
    const double centre = screenLeft + screenWidth / 2.0;
    const double full = CarrySlideFull * screenWidth;
    if (fingerX > lean.high) {
        const double room = centre + full - lean.high;
        return room > 1.0 ? std::clamp((fingerX - lean.high) / room, 0.0, 1.0) : 1.0;
    }
    if (fingerX < lean.low) {
        const double room = lean.low - (centre - full);
        return room > 1.0 ? -std::clamp((lean.low - fingerX) / room, 0.0, 1.0) : -1.0;
    }
    return 0.0;
}

// Row travel per second for a finger at `fingerX`. The row passes under the
// card at the same speed on screen however far it has stepped back, so a row
// drawn smaller covers more of itself.
inline double carrySlideSpeed(double fingerX, const CarryLean &lean, double screenLeft,
                              double screenWidth, double scale)
{
    const double depth = carryLeanDepth(fingerX, lean, screenLeft, screenWidth);
    return (depth < 0.0 ? -1.0 : 1.0) * std::pow(std::abs(depth), CarrySlideCurve) * CarrySlideSpeed
        / std::max(scale, 0.05);
}

// The width a held card is drawn at, as a fraction of a full-size card's: a
// little larger than the row it is over, at whatever zoom the row stands.
inline double carryHeldFraction(double scale)
{
    return scale * CarryHeldRise;
}

struct CarryAim {
    enum class Kind { Gap, Card };
    Kind kind = Kind::Gap;
    // The gap before item `index`, or the item under the card.
    int index = 0;
    // Over a Bento group, the pane under the finger; otherwise -1.
    int part = -1;
    bool operator==(const CarryAim &other) const = default;
};

// The gap the held card's centre, at row coordinate `centre`, is nearest,
// given where each item stands, kept between items `low` and `high`.
inline int carryGapAt(double centre, const std::vector<double> &positions, int low, int high)
{
    int before = 0;
    for (int k = 0; k < static_cast<int>(positions.size()); ++k)
        if (positions[static_cast<std::size_t>(k)] < centre) before = k + 1;
    return std::clamp(before, low, std::max(low, high));
}

// What the held card's centre, at row coordinate `centre`, is over, given
// where each item stands. `reach` says, in card widths from each item's
// centre, how near the held card must be to be over it, nothing where it
// cannot be joined; a gap is kept between items `low` and `high`.
inline CarryAim carryAim(double centre, const std::vector<double> &positions, double cardWidth,
                         const std::vector<double> &reach, int low, int high)
{
    const int items = static_cast<int>(positions.size());
    int nearest = -1;
    double distance = 0.0;
    for (int k = 0; k < items; ++k) {
        const double d = std::abs(centre - positions[static_cast<std::size_t>(k)]);
        if (nearest < 0 || d < distance) {
            nearest = k;
            distance = d;
        }
    }
    if (nearest >= 0 && static_cast<std::size_t>(nearest) < reach.size()
        && distance < reach[static_cast<std::size_t>(nearest)] * cardWidth)
        return {CarryAim::Kind::Card, nearest};
    return {CarryAim::Kind::Gap, carryGapAt(centre, positions, low, high)};
}

// The same, where every item a card may join is reached alike.
inline CarryAim carryAim(double centre, const std::vector<double> &positions, double cardWidth,
                         const std::vector<bool> &joinable, int low, int high)
{
    std::vector<double> reach(joinable.size(), 0.0);
    for (std::size_t k = 0; k < joinable.size(); ++k) reach[k] = joinable[k] ? CarryJoinReach : 0.0;
    return carryAim(centre, positions, cardWidth, reach, low, high);
}

// The row's reach: it slides until either end of the items from `low` to
// `high`, or a gap past the last, is a little beyond the centre.
inline double carryClampPosition(double position, int low, int high, double pitch)
{
    return std::clamp(position, (low - 0.6) * pitch, (std::max(high, low) + 0.6) * pitch);
}

// A card held in its own Stack: where it would go among the Stack's places,
// and where the finger stood when it last took a place.
struct CarryStackPaging {
    int place = 0;
    double anchor = 0.0;
};

// Sideways travel of one slow-stroke step moves the card one place, a finger
// moving left toward the later cards, which cross the place to its left. At
// either end the place holds, and the way back counts from where the finger
// turns.
inline CarryStackPaging carryPageStack(CarryStackPaging paging, double fingerX, int count)
{
    const int last = std::max(count - 1, 0);
    while (paging.anchor - fingerX >= ScrubStep && paging.place < last) {
        ++paging.place;
        paging.anchor -= ScrubStep;
    }
    while (fingerX - paging.anchor >= ScrubStep && paging.place > 0) {
        --paging.place;
        paging.anchor += ScrubStep;
    }
    if (paging.place >= last && paging.anchor > fingerX) paging.anchor = fingerX;
    if (paging.place <= 0 && paging.anchor < fingerX) paging.anchor = fingerX;
    return paging;
}

// A Stack's cards in the order they would have with `held` let go at `place`.
inline std::vector<int> carryStackOrder(std::vector<int> members, int held, int place)
{
    members.erase(std::remove(members.begin(), members.end(), held), members.end());
    members.insert(members.begin() + std::clamp(place, 0, static_cast<int>(members.size())), held);
    return members;
}

// A Stack's cards from front to back, given its cards in storage order and
// the one in front. The fan shows the card before the front one just behind
// it, and so on round (SpreadLayout.cpp).
inline std::vector<int> carryStackDeck(const std::vector<int> &members, int face)
{
    const int count = static_cast<int>(members.size());
    std::vector<int> deck;
    deck.reserve(members.size());
    for (int depth = 0; depth < count; ++depth)
        deck.push_back(members[static_cast<std::size_t>(((face - depth) % count + count) % count)]);
    return deck;
}

// Where a card held in its own Stack goes when let go at `place` from the
// front of `deck`: the index to insert it at among the Stack's other cards,
// `others` in storage order, and the card then in front, which is the one the
// Stack shows. Only the held card moves; the rest keep their order round.
struct CarryStackLanding {
    int insertion = 0;
    int face = 0;
};

inline CarryStackLanding carryStackLanding(const std::vector<int> &deck, int held, int place,
                                           const std::vector<int> &others)
{
    const auto order = carryStackOrder(deck, held, place);
    const auto at = static_cast<int>(std::find(order.cbegin(), order.cend(), held) - order.cbegin());
    const auto indexOf = [&others](int card) {
        const auto it = std::find(others.cbegin(), others.cend(), card);
        return it == others.cend() ? 0 : static_cast<int>(it - others.cbegin());
    };
    CarryStackLanding landing;
    landing.face = order.empty() ? held : order.front();
    if (order.size() < 2) return landing;
    // Behind a card, it goes just below it in storage; in front, just above
    // the card behind it.
    landing.insertion = at > 0 ? indexOf(order[static_cast<std::size_t>(at - 1)])
                               : indexOf(order[1]) + 1;
    return landing;
}

// Where a card held in its own Stack is drawn for the finger's travel: a
// little way after it, and no further.
inline double carryStackLeash(double travel)
{
    return CarryStackLeash * std::tanh(travel / CarryStackLeash);
}

} // namespace Kadunce

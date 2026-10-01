// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "SpreadLayout.h"

#include <algorithm>

namespace Kadunce {

// A card held over a pane of the Bento group slides under the group on that
// pane's side as the pane gives way (CARD-LIFECYCLE.md §5). It takes the pane's
// shape and stands behind the group as a Stack's shoulder stands behind its
// face: slid outward by the same share of its width, and tipped the same
// little way, so it shows through the cutout and past the group's edge.
inline constexpr double HeldTuckShift = 0.24 / 3.0;
inline constexpr double HeldTuckTilt = 0.6;

struct HeldTuckPose {
    CardRect rect;
    double rotation = 0.0;
};

inline HeldTuckPose heldTuckPose(const CardRect &pane, const CardRect &group)
{
    const int side = pane.x + pane.width / 2.0 < group.x + group.width / 2.0 ? -1 : 1;
    return {{pane.x + side * pane.width * HeldTuckShift, pane.y, pane.width, pane.height},
            side * HeldTuckTilt};
}

// Between the finger's card and the tucked one, as the pane gives way.
inline CardRect heldTuckBlend(const CardRect &free, const CardRect &tucked, double progress)
{
    const double t = std::clamp(progress, 0.0, 1.0);
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return {mix(free.x, tucked.x), mix(free.y, tucked.y),
            mix(free.width, tucked.width), mix(free.height, tucked.height)};
}

// How far under the group the card has gone: none until the pane has given
// way part of the way, then all of it by the time it has gone, so the part of
// the card over the group fades out of sight rather than vanishing.
inline double heldTuckUnder(double progress)
{
    return std::clamp((progress - 0.4) / 0.4, 0.0, 1.0);
}

} // namespace Kadunce

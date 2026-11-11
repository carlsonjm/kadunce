/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <cmath>

namespace Kadunce {

// A pane a layout moves or resizes (CARD-LIFECYCLE.md §11, Connected) is
// drawn in a box that travels from its old place to its new one. Its client
// draws the new size about 40 ms into the 220 ms move, long before the box
// arrives, so whichever picture is current is fitted to the box exactly:
// its frame's edges are the box's edges every frame. The picture is
// stretched a little for the length of the move, and never shown larger or
// smaller than its box, so the pane can neither flash magnified nor jump.
//
// The box stands in its place until the client has drawn there: a client
// that has not yet drawn the new size by the end of the move would
// otherwise jump the moment the move ends.

// Whether a client has drawn a pane's new place: its frame has the size the
// pane goes to, or another size than it had when the move began (a terminal
// keeping whole rows stops short of its place, and that is its answer).
inline bool paneResizeCaughtUp(double startWidth, double startHeight, double frameWidth,
                               double frameHeight, double toWidth, double toHeight)
{
    const auto same = [](double a, double b) { return std::abs(a - b) < 1.0; };
    if (same(frameWidth, toWidth) && same(frameHeight, toHeight)) return true;
    return !same(frameWidth, startWidth) || !same(frameHeight, startHeight);
}

} // namespace Kadunce

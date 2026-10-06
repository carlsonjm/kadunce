/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>

namespace Kadunce {

// A card let go on a pane of the Bento group grows into that pane from where
// it was let go (CARD-LIFECYCLE.md §5). Its client draws the pane's size a
// little after KWin asks for it, so the card first waits where it was let go
// until the client's frame has the pane's shape, or for `PaneArrivalWaitCap`
// milliseconds at most, and then moves the whole way in
// `PaneArrivalDuration` milliseconds, counted from the first frame that
// draws it moving, so a slow frame never takes the move with it.
inline constexpr int PaneArrivalDuration = 220;
inline constexpr int PaneArrivalWaitCap = 150;
// A move that no frame has drawn this long after it was asked for is dropped,
// so nothing waits on a display that is not being drawn.
inline constexpr int PaneArrivalAbandon = 1000;
// A frame within this share of the pane's aspect has the pane's shape.
inline constexpr double PaneArrivalAspectTolerance = 0.02;

// Whether a frame of `width` by `height` has the shape of a pane of
// `paneWidth` by `paneHeight`: its size, or its proportions.
inline bool paneArrivalShaped(double width, double height, double paneWidth, double paneHeight)
{
    if (width <= 0.0 || height <= 0.0 || paneWidth <= 0.0 || paneHeight <= 0.0) return false;
    if (std::abs(width - paneWidth) < 1.0 && std::abs(height - paneHeight) < 1.0) return true;
    const double aspect = (width / height) / (paneWidth / paneHeight);
    return std::abs(aspect - 1.0) <= PaneArrivalAspectTolerance;
}

// Whether the card still waits where it was let go, `waited` milliseconds
// after the move was asked for.
inline bool paneArrivalWaits(bool shaped, long long waited)
{
    return !shaped && waited < PaneArrivalWaitCap;
}

// How far through its move the card is, `elapsed` milliseconds after the
// first frame drew it moving, none before (`started` false), for a move of
// `duration` milliseconds.
inline double paneArrivalProgress(bool started, long long elapsed, int duration)
{
    if (!started) return 0.0;
    if (duration <= 0) return 1.0;
    return std::clamp(double(elapsed) / duration, 0.0, 1.0);
}

} // namespace Kadunce

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QList>
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace Kadunce {

// A window closing in a layout (CARD-LIFECYCLE.md §11) does not shrink toward
// its centre as KDE's closing animation draws it. It stays still and dims,
// and the pane growing into its room draws across it on its backing. Once
// that pane covers it, it dims to `PaneLeavingCoveredOpacity` and is gone
// under it as the move ends; left uncovered, as when its room is offered to
// the display's other cards, it fades out where it stood.
inline constexpr int PaneLeavingDuration = 220;
inline constexpr double PaneLeavingCoveredOpacity = 0.55;

[[nodiscard]] inline double paneLeavingOpacity(double elapsed, double duration, bool covered)
{
    if (!(duration > 0.0)) return 0.0;
    const double t = std::clamp(elapsed / duration, 0.0, 1.0);
    // Covered, it dims early, as the pane draws across it; left alone, it
    // fades evenly, so its going is seen rather than a cut.
    if (covered) return 1.0 - (1.0 - PaneLeavingCoveredOpacity) * (1.0 - std::pow(1.0 - t, 3.0));
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

// Whether a pane growing to any of `growing` will cover the closing window
// standing at `leaving`, its edges given a few pixels for the gap between panes.
[[nodiscard]] inline bool paneLeavingCovered(const QRectF &leaving, const QList<QRectF> &growing)
{
    if (!leaving.isValid()) return false;
    const QRectF inside = leaving.adjusted(4.0, 4.0, -4.0, -4.0);
    return std::any_of(growing.cbegin(), growing.cend(), [&inside](const QRectF &to) { return to.contains(inside); });
}

} // namespace Kadunce

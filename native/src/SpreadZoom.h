/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "SpreadLayout.h"

#include <algorithm>

namespace Kadunce
{

// How much of the work area each card takes across and down in the ordinary
// row: three across, the neighbours partly shown (makeSpreadLayout).
inline constexpr double SpreadCardShare = 0.54;
// The ordinary gutter, as a share of the work area's width.
inline constexpr double SpreadGutterShare = 0.056;
// How much of each neighbour past the three whole cards a display sized
// Connected shows at its edges.
inline constexpr double SpreadConnectedEdge = 0.4;

// Spread zoomed out, as it opens on a display sized Connected: three whole
// cards, the selected one centred, and an edge of the card past each side, as
// the ordinary row shows one whole card and its neighbours' edges.
[[nodiscard]] constexpr double spreadConnectedShare()
{
    // Three cards, their four gutters and two edges span the work area.
    constexpr double gutterPerCard = SpreadGutterShare / SpreadCardShare;
    return 1.0 / (3.0 + 4.0 * gutterPerCard + 2.0 * SpreadConnectedEdge);
}

// The row at a card share; the ordinary share is makeSpreadLayout itself. The
// gutter keeps its proportion to the card.
[[nodiscard]] inline SpreadLayout makeSpreadLayoutAt(
    double workX, double workY, double workWidth, double workHeight, double share)
{
    if (share >= SpreadCardShare) return makeSpreadLayout(workX, workY, workWidth, workHeight);
    const double cardWidth = workWidth * share;
    const double cardHeight = workHeight * share;
    const double gutter = workWidth * SpreadGutterShare * share / SpreadCardShare;
    const double centerX = workX + (workWidth - cardWidth) / 2.0;
    const double cardY = workY + (workHeight - cardHeight) / 2.0;
    return {
        .cards = {
            CardRect{centerX - cardWidth - gutter, cardY, cardWidth, cardHeight},
            CardRect{centerX, cardY, cardWidth, cardHeight},
            CardRect{centerX + cardWidth + gutter, cardY, cardWidth, cardHeight},
        },
        .gutter = gutter,
    };
}

} // namespace Kadunce

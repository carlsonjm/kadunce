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
// Zoomed out, the share never falls below this, so a card stays readable.
inline constexpr double SpreadZoomedOutShareMinimum = 0.16;
// The ordinary gutter, as a share of the work area's width.
inline constexpr double SpreadGutterShare = 0.056;

// Spread zoomed out, as it opens on a display sized Connected: cards small
// enough that `entries` stand side by side across the work area, between the
// floor above and the ordinary share.
[[nodiscard]] inline double spreadZoomedOutShare(int entries)
{
    // n cards and n - 1 gutters across all but a margin of the work area.
    constexpr double across = 0.96;
    constexpr double gutterPerCard = SpreadGutterShare / SpreadCardShare;
    const int n = std::max(entries, 1);
    const double share = across / (n + gutterPerCard * (n - 1));
    return std::clamp(share, SpreadZoomedOutShareMinimum, SpreadCardShare);
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

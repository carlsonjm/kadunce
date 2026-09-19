/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <vector>
#include <optional>

namespace Kadunce
{

struct BentoRect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct BentoPixelRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct BentoCandidate
{
    double minimumWidth = 0.0;
    double minimumHeight = 0.0;
    double width = 1.0;
    double height = 1.0;
    bool preferred = false;
};

struct BentoAdmission
{
    std::vector<int> candidateIndices;
    std::vector<BentoRect> rects;
};

// One maximum visible pane count per display, read by every admission path.
// A work area this small has room for the three-pane grammar only; a larger
// one uses the curated library. The bound is the work area, never the
// hardware the work area belongs to.
inline constexpr int BentoCompactAreaWidth = 1800;
inline constexpr int BentoCompactAreaHeight = 1000;
inline constexpr int BentoCompactPaneCap = 2;
// The pane count the side-contact grammar in BentoSidePlacement.h describes.
// It is deliberately not BentoCompactPaneCap: the compact display's maximum is
// a product decision and this is the shape vocabulary's own size. No display
// caps at this today, so that grammar is dormant; raising a cap to it revives
// the grammar without touching the mapping.
inline constexpr int BentoContactGrammarPaneCap = 3;
inline constexpr int BentoCuratedPaneCap = 8;

[[nodiscard]] constexpr int bentoPaneCap(int areaWidth, int areaHeight)
{
    return areaWidth < BentoCompactAreaWidth || areaHeight < BentoCompactAreaHeight
        ? BentoCompactPaneCap : BentoCuratedPaneCap;
}

// Orientation is a proportion of the work area. A landscape-shaped work area
// is landscape on every path that lays panes out inside it.
[[nodiscard]] constexpr bool bentoLandscapeArea(int areaWidth, int areaHeight)
{
    return areaWidth >= areaHeight;
}

[[nodiscard]] std::vector<BentoRect> makeBentoLayout(
    int count, bool landscape);
[[nodiscard]] std::vector<BentoRect> makeAlternateTwoPaneBentoLayout(
    bool landscape);
// The second permitted three-pane shape: three panes across the long axis,
// beside the one column and top/bottom split that makeBentoLayout(3) gives.
[[nodiscard]] std::vector<BentoRect> makeAlternateThreePaneBentoLayout(
    bool landscape);
[[nodiscard]] std::vector<BentoPixelRect> makePixelBentoLayout(
    const std::vector<BentoRect> &rects,
    int areaX, int areaY, int areaWidth, int areaHeight,
    int gap = 14);
// maximumVisible is the caller's own further limit; the display's pane cap
// bounds it either way.
[[nodiscard]] BentoAdmission chooseBentoAdmission(
    const std::vector<BentoCandidate> &candidates,
    int areaWidth, int areaHeight, int maximumVisible = BentoCuratedPaneCap);

// A transfer is accepted only if the arriving candidate has a visible pane.
// Unlike ordinary reflow, silently parking this candidate is rejection.
[[nodiscard]] std::optional<BentoAdmission> chooseBentoTransferAdmission(
    const std::vector<BentoCandidate> &candidates, int arrivingIndex,
    int areaWidth, int areaHeight, int maximumVisible = BentoCuratedPaneCap);

} // namespace Kadunce

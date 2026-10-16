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
// hardware the work area belongs to, where the display has no sizing to
// name it (the overload below).
inline constexpr int BentoCompactAreaWidth = 1800;
inline constexpr int BentoCompactAreaHeight = 1000;
inline constexpr int BentoCompactPaneCap = 2;
// The pane count the side-contact grammar in BentoSidePlacement.h describes.
// It is deliberately not BentoCompactPaneCap: the compact display's maximum is
// a product decision and this is the shape vocabulary's own size. No display
// caps at this today, so that grammar is dormant; raising a cap to it revives
// the grammar without touching the mapping.
inline constexpr int BentoContactGrammarPaneCap = 3;
// The curated library's own size. Past it the library is an even grid, and
// the windows' minimum sizes decide how many panes a larger display shows.
inline constexpr int BentoCuratedPaneCap = 8;
// The curated library considers two alternates beyond its eight panes, which
// is enough to resolve minimum-size conflicts without the subset search
// growing with a desktop's whole window history.
inline constexpr int BentoCuratedCandidates = 10;
// Not a pane count anyone sees: minimum sizes stop a grid long before it. It
// keeps the candidates any one solve reads finite.
inline constexpr int BentoPaneBound = 24;

[[nodiscard]] constexpr int bentoPaneCap(int areaWidth, int areaHeight)
{
    return areaWidth < BentoCompactAreaWidth || areaHeight < BentoCompactAreaHeight
        ? BentoCompactPaneCap : BentoPaneBound;
}

// The cap a solve reads. A display that plays cards has one from its sizing
// (DisplaySizing.h), which panels and docks cannot change and kaduncerc's
// [Sizing] choice can; a solve given it reads it in place of the work area's,
// or a display sized Connected would cap at two whenever panels left its work
// area below the compact bound. A display without cards has no sizing, and
// its work area decides.
[[nodiscard]] constexpr int bentoPaneCap(int areaWidth, int areaHeight,
                                         std::optional<int> paneCap)
{
    return paneCap ? *paneCap : bentoPaneCap(areaWidth, areaHeight);
}

// Orientation is a proportion of the work area. A landscape-shaped work area
// is landscape on every path that lays panes out inside it.
[[nodiscard]] constexpr bool bentoLandscapeArea(int areaWidth, int areaHeight)
{
    return areaWidth >= areaHeight;
}

[[nodiscard]] std::vector<BentoRect> makeBentoLayout(
    int count, bool landscape);
// Past the curated eight: an even grid, row by row, with any empty cells at
// the end of the last row. bentoGridColumns is the library's own proportion.
[[nodiscard]] int bentoGridColumns(int count, bool landscape);
[[nodiscard]] std::vector<BentoRect> makeGridBentoLayout(int count, int columns);
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
// bounds it either way. paneCap is that cap where the display's sizing names
// it; left out, the work area's own stands in. connected is a display sized
// Connected (CARD-LIFECYCLE.md §11), whose columns share its width evenly:
// after the curated shapes its two panes may also take even halves the other
// way, side by side on a landscape display, so two windows too wide for the
// narrow pane and too tall for half its height still stand together. Nothing
// else changes with it.
[[nodiscard]] BentoAdmission chooseBentoAdmission(
    const std::vector<BentoCandidate> &candidates,
    int areaWidth, int areaHeight, int maximumVisible = BentoPaneBound,
    std::optional<int> paneCap = std::nullopt, bool connected = false);

// A transfer is accepted only if the arriving candidate has a visible pane.
// Leaving any candidate out is not a private matter either way: the caller
// compares the result against what it owned, and CARD-LIFECYCLE.md §8 makes an
// admission that would drop a resident a refusal rather than a rearrangement.
[[nodiscard]] std::optional<BentoAdmission> chooseBentoTransferAdmission(
    const std::vector<BentoCandidate> &candidates, int arrivingIndex,
    int areaWidth, int areaHeight, int maximumVisible = BentoPaneBound,
    std::optional<int> paneCap = std::nullopt, bool connected = false);

// CARD-LIFECYCLE.md §8: a layout that cannot grow gives the arrival one slot,
// and only that slot's occupant leaves. The slot is the smallest whose pixel
// size satisfies the arrival's minimum, so a window that fits only the wide
// pane takes the wide pane and a small one leaves the wide pane alone. Fit is
// the whole rule: no interaction history is consulted, and the slots the
// arrival does not take keep their windows, their size and their places.
// Returns -1 when no slot fits, which is the arrival §8 answers with a card.
[[nodiscard]] int bentoSlotForArrival(const std::vector<BentoPixelRect> &slots,
                                      double minimumWidth, double minimumHeight);

// CARD-LIFECYCLE.md §11, Connected: on the display that holds Spread, a full
// layout gives the arrival the slot of the window used longest ago, among the
// slots whose pixel size satisfies the arrival's minimum. lastUsed holds each
// slot's last use, later uses larger; a tie goes to the earlier slot. Only that
// slot's occupant leaves, as for bentoSlotForArrival. `kept`, when not -1, is
// the slot of the card a side snap placed, which never gives way. Returns -1
// when no slot fits or the uses do not name every slot.
[[nodiscard]] int bentoSlotUsedLongestAgo(const std::vector<BentoPixelRect> &slots,
                                          const std::vector<long long> &lastUsed,
                                          double minimumWidth, double minimumHeight,
                                          int kept = -1);

// CARD-LIFECYCLE.md §5: the pane a full layout gives up to a side release is
// the one occupying the side released into. A pane occupies a side when it
// reaches the outermost edge on that side, so gaps and margins do not matter;
// where a stack shares the edge, an upper contact names its highest pane and a
// lower contact its lowest. Returns -1 when there are no panes.
[[nodiscard]] int bentoPaneOnSide(const std::vector<BentoPixelRect> &panes,
                                  bool right, bool upper);

// Zones drawn for a display with KDE's tile editor, which give a display
// without cards its shape in place of the curated library (CARD-LIFECYCLE.md
// §11). Each chosen candidate goes to one zone whose pixel size satisfies its
// minimum; as many candidates are shown as fit, earlier candidates first, and
// a zone nobody fits stays empty. A candidate keeps the zone currentZones
// gives it while it still fits, so an arrival never moves the windows
// already there; otherwise the first candidate takes the largest zone it
// fits. required, when not -1, must be among those shown.
struct ZoneAdmission
{
    std::vector<int> candidateIndices;
    std::vector<int> zoneIndices;
};
[[nodiscard]] std::optional<ZoneAdmission> chooseZoneAdmission(
    const std::vector<BentoCandidate> &candidates,
    const std::vector<BentoPixelRect> &zones,
    const std::vector<int> &currentZones = {}, int required = -1);

} // namespace Kadunce

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "BentoLayout.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Kadunce {
struct BentoSidePlacement {
    bool right = false;
    bool large = true;
};

// Contact chooses intent; geometry is solved separately with native minimums.
inline BentoSidePlacement bentoSideChoice(bool right, double y, double midpoint,
    std::optional<BentoSidePlacement> previous = {})
{
    const bool retain = previous && previous->right == right && std::abs(y - midpoint) <= 18.0;
    return {right, retain ? previous->large : y < midpoint};
}

// The arrangements a side snap can ask for. Proportional is the curated
// library placed beside one edge pane; the other two are the permitted
// three-pane shapes.
enum class BentoSideShape {
    Proportional,
    LargeBesideStack,
    VerticalPanes,
};

// CARD-LIFECYCLE.md §5. Where a display's maximum is three panes, the contact
// names the shape: the upper half asks for the larger placement, which is one
// of three panes across, and the lower half for the smaller one, which is the
// lower pane of a stack beside a full-height pane. Where the minimums forbid
// the requested shape the other one is used; a caller that can place neither
// stays at two panes. The order is a function of the gesture and the pane count
// alone, so it cannot depend on which admission attempt ran first.
inline std::vector<BentoSideShape> bentoSideShapeOrder(
    BentoSidePlacement choice, int paneCount, int width, int height)
{
    if (paneCount != BentoContactGrammarPaneCap
        || bentoPaneCap(width, height) != BentoContactGrammarPaneCap)
        return {BentoSideShape::Proportional};
    return choice.large
        ? std::vector<BentoSideShape>{BentoSideShape::VerticalPanes,
                                      BentoSideShape::LargeBesideStack}
        : std::vector<BentoSideShape>{BentoSideShape::LargeBesideStack,
                                      BentoSideShape::VerticalPanes};
}

constexpr double BentoSideHalfGap = 7.0; // Existing makePixelBentoLayout gap / 2.

// Rect order is arrival, then existing companions, so the two line up.
inline bool bentoPanesHoldMinimums(const std::vector<BentoRect> &rects,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions,
    int width, int height)
{
    if (rects.size() != companions.size() + 1) return false;
    const auto pixels = makePixelBentoLayout(rects, 0, 0, width, height);
    for (size_t i = 0; i < pixels.size(); ++i) {
        const BentoCandidate &candidate = i == 0 ? arrival : companions[i - 1];
        if (pixels[i].width < candidate.minimumWidth
            || pixels[i].height < candidate.minimumHeight) return false;
    }
    return true;
}

// One full-height pane beside two stacked panes, the stack on the contacted
// edge and the arrival in its lower half. A third is the starting share and a
// half the starting cut; both are clamped by the native minimums.
inline std::optional<std::vector<BentoRect>> bentoLargeBesideStackLayout(
    BentoSidePlacement choice, int width, int height,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions)
{
    if (companions.size() != 2) return std::nullopt;
    const BentoCandidate &whole = companions[0];
    const BentoCandidate &upper = companions[1];
    const double low = (std::max({1.0, arrival.minimumWidth, upper.minimumWidth})
        + BentoSideHalfGap) / width;
    const double high = 1.0
        - (std::max(1.0, whole.minimumWidth) + BentoSideHalfGap) / width;
    if (low > high) return std::nullopt;
    const double stack = std::clamp(1.0 / 3.0, low, high);
    const double lowCut = (upper.minimumHeight + BentoSideHalfGap) / height;
    const double highCut = 1.0 - (arrival.minimumHeight + BentoSideHalfGap) / height;
    if (lowCut > highCut) return std::nullopt;
    const double cut = std::clamp(0.5, lowCut, highCut);
    const double stackX = choice.right ? 1.0 - stack : 0.0;
    const double wholeX = choice.right ? 0.0 : stack;
    std::vector<BentoRect> rects{{stackX, cut, stack, 1.0 - cut},
                                 {wholeX, 0.0, 1.0 - stack, 1.0},
                                 {stackX, 0.0, stack, cut}};
    if (!bentoPanesHoldMinimums(rects, arrival, companions, width, height))
        return std::nullopt;
    return rects;
}

// Three panes across, the arrival holding the contacted edge. Each pane starts
// from an equal share; a wider minimum takes what it needs first and what is
// left over is shared out evenly.
inline std::optional<std::vector<BentoRect>> bentoVerticalPanesLayout(
    BentoSidePlacement choice, int width, int height,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions)
{
    if (companions.size() != 2) return std::nullopt;
    const std::array<const BentoCandidate *, 3> columns = choice.right
        ? std::array<const BentoCandidate *, 3>{&companions[0], &companions[1], &arrival}
        : std::array<const BentoCandidate *, 3>{&arrival, &companions[0], &companions[1]};
    std::array<double, 3> needed{};
    double total = 0.0;
    for (int i = 0; i < 3; ++i) {
        const double inset = BentoSideHalfGap * ((i > 0) + (i < 2));
        needed[i] = (std::max(1.0, columns[i]->minimumWidth) + inset) / width;
        total += needed[i];
    }
    if (total > 1.0) return std::nullopt;
    const double spare = (1.0 - total) / 3.0;
    std::array<BentoRect, 3> ordered{};
    double x = 0.0;
    for (int i = 0; i < 3; ++i) {
        const double share = i == 2 ? 1.0 - x : needed[i] + spare;
        ordered[i] = {x, 0.0, share, 1.0};
        x += share;
    }
    std::vector<BentoRect> rects = choice.right
        ? std::vector<BentoRect>{ordered[2], ordered[0], ordered[1]}
        : std::vector<BentoRect>{ordered[0], ordered[1], ordered[2]};
    if (!bentoPanesHoldMinimums(rects, arrival, companions, width, height))
        return std::nullopt;
    return rects;
}

// Reuse the curated presets inside the remainder beside the edge pane.
// Admission below chooses which residents can fit.
inline std::optional<std::vector<BentoRect>> bentoProportionalSideLayout(
    BentoSidePlacement choice, int width, int height,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions)
{
    constexpr double halfGap = BentoSideHalfGap;
    const double low = (std::max(1.0, arrival.minimumWidth) + halfGap) / width;
    const double desired = choice.large ? 2.0/3.0 : 1.0/3.0;
    std::optional<std::vector<BentoRect>> best;
    double bestDistance = 2.0;
    // The remainder is the area the companion preset has to fill, so that
    // area's own proportions decide which orientation is tried first.
    const bool landscape = bentoLandscapeArea(
        static_cast<int>(std::lround(width * (1 - desired))), height);
    std::vector<std::vector<BentoRect>> patterns{
        makeBentoLayout(companions.size(), landscape),
        makeBentoLayout(companions.size(), !landscape)};
    if (companions.size() == 2) {
        patterns.push_back(makeAlternateTwoPaneBentoLayout(landscape));
        patterns.push_back(makeAlternateTwoPaneBentoLayout(!landscape));
    }
    for (const auto &pattern : patterns) {
        double high = 1.0;
        bool fits = true;
        for (size_t i = 0; i < pattern.size(); ++i) {
            const auto &r = pattern[i];
            const double horizontalInset = halfGap * ((r.x > .001 || !choice.right)
                + (r.x+r.width < .999 || choice.right));
            const double verticalInset = halfGap * ((r.y > .001) + (r.y+r.height < .999));
            high = std::min(high, 1.0 - (std::max(1.0, companions[i].minimumWidth)
                + horizontalInset) / (width * r.width));
            if (height * r.height - verticalInset < companions[i].minimumHeight) fits = false;
        }
        if (!fits || low > high) continue;
        const double share = std::clamp(desired, low, high);
        std::vector<BentoRect> result{{choice.right ? 1-share : 0, 0, share, 1}};
        for (const auto &r : pattern)
            result.push_back({(choice.right ? 0 : share) + r.x*(1-share), r.y,
                r.width*(1-share), r.height});
        const auto pixels = makePixelBentoLayout(result, 0, 0, width, height);
        if (pixels[0].width < arrival.minimumWidth) continue;
        for (size_t i = 0; i < companions.size(); ++i)
            if (pixels[i+1].width < companions[i].minimumWidth
                || pixels[i+1].height < companions[i].minimumHeight) fits = false;
        if (fits && std::abs(share-desired) < bestDistance) {
            bestDistance = std::abs(share-desired); best = std::move(result);
        }
    }
    return best;
}

// Rect order is arrival, then existing companions. The contact names the shape
// and the shapes it names are tried in that order, so the same contact answers
// the same way whatever was on screen before.
inline std::optional<std::vector<BentoRect>> bentoSideLayout(
    BentoSidePlacement choice, int width, int height,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions)
{
    if (width <= 0 || height <= 0 || arrival.minimumHeight > height
        || arrival.minimumWidth > width) return std::nullopt;
    if (companions.empty()) return std::vector<BentoRect>{{0, 0, 1, 1}};
    if (companions.size() > 7) return std::nullopt;
    for (const BentoSideShape shape : bentoSideShapeOrder(
             choice, int(companions.size()) + 1, width, height)) {
        std::optional<std::vector<BentoRect>> rects;
        switch (shape) {
        case BentoSideShape::Proportional:
            rects = bentoProportionalSideLayout(choice, width, height, arrival, companions);
            break;
        case BentoSideShape::LargeBesideStack:
            rects = bentoLargeBesideStackLayout(choice, width, height, arrival, companions);
            break;
        case BentoSideShape::VerticalPanes:
            rects = bentoVerticalPanesLayout(choice, width, height, arrival, companions);
            break;
        }
        if (rects) return rects;
    }
    return std::nullopt;
}

inline std::optional<std::vector<BentoRect>> bentoSideLayout(BentoSidePlacement choice,
    int width, int height, const BentoCandidate &arrival, const std::optional<BentoCandidate> &companion)
{
    return bentoSideLayout(choice, width, height, arrival,
        companion ? std::vector<BentoCandidate>{*companion} : std::vector<BentoCandidate>{});
}

// Split an occupied full-height edge column without rebuilding unrelated panes.
// Arrival is appended, or moved out of a full-height third column. In that case
// the remaining adjacent column absorbs its old space before the target splits.
//
// The result is one full-height pane beside a stacked pair, so where the contact
// mapping names a shape this answers only when that is the shape named. Without
// that the same snap resolved differently depending on which path could run.
inline std::optional<std::vector<BentoRect>> splitBentoColumn(
    const std::vector<BentoRect> &existing, const std::vector<BentoCandidate> &candidates,
    int arrival, BentoSidePlacement choice, int width, int height)
{
    if (existing.size() < 2 || existing.size() >= 8 || arrival < 0
        || arrival > int(existing.size()) || candidates.size() != existing.size()+(arrival == int(existing.size()))
        || width <= 0 || height <= 0
        || int(candidates.size()) > bentoPaneCap(width, height)) return std::nullopt;
    const auto order = bentoSideShapeOrder(choice, int(candidates.size()), width, height);
    const bool mapped = order.front() != BentoSideShape::Proportional;
    if (mapped && order.front() != BentoSideShape::LargeBesideStack) return std::nullopt;
    int target = -1;
    for (int i = 0; i < int(existing.size()); ++i) {
        const auto &r = existing[i];
        if (i != arrival && std::abs(r.y) < .002 && std::abs(r.height-1) < .002
            && r.width < .999 && (!mapped || r.width <= .5)
            && (choice.right ? std::abs(r.x+r.width-1) < .002 : std::abs(r.x) < .002)) {
            target = i; break;
        }
    }
    if (target < 0) return std::nullopt;
    auto result = existing;
    if (arrival < int(existing.size())) {
        const auto old = existing[arrival];
        if (std::abs(old.y) > .002 || std::abs(old.height-1) > .002) return std::nullopt;
        int neighbor = -1;
        for (int i = 0; i < int(existing.size()); ++i) {
            const auto &r = existing[i];
            if (i != arrival && i != target && std::abs(r.y) < .002 && std::abs(r.height-1) < .002
                && (std::abs(r.x+r.width-old.x) < .002 || std::abs(old.x+old.width-r.x) < .002)) {
                neighbor = i; break;
            }
        }
        if (neighbor < 0) return std::nullopt;
        auto &r = result[neighbor];
        const double end = std::max(r.x+r.width,old.x+old.width);
        r.x = std::min(r.x,old.x); r.width = end-r.x;
    } else result.push_back({});
    const auto column = existing[target];
    const double low = (candidates[choice.large ? arrival : target].minimumHeight+7.0)/height;
    const double high = 1.0-(candidates[choice.large ? target : arrival].minimumHeight+7.0)/height;
    if (low > high) return std::nullopt;
    const double cut = std::clamp(.5,low,high);
    result[choice.large ? arrival : target] = {column.x,0,column.width,cut};
    result[choice.large ? target : arrival] = {column.x,cut,column.width,1-cut};
    const auto pixels = makePixelBentoLayout(result,0,0,width,height);
    for (size_t i = 0; i < pixels.size(); ++i)
        if (pixels[i].width < candidates[i].minimumWidth || pixels[i].height < candidates[i].minimumHeight)
            return std::nullopt;
    return result;
}

// Keep the edge-selected card visible and report the rest as unplaced. The
// display's own pane cap bounds the search, preserve resident order, prefer more
// panes. A resident this leaves out is not retained anywhere: CARD-LIFECYCLE.md
// §5 makes it an individual card, and the publisher is what gives it one.
inline std::optional<BentoAdmission> chooseBentoSideAdmission(BentoSidePlacement choice,
    int width, int height, const std::vector<BentoCandidate> &candidates, int required = 0)
{
    const int cap = bentoPaneCap(width, height);
    const int count = std::min(10, int(candidates.size()));
    if (count == 0 || required < 0 || required >= count) return std::nullopt;
    std::optional<BentoAdmission> best;
    for (unsigned mask = 0; mask < (1u << (count - 1)); ++mask) {
        std::vector<int> indices{0};
        std::vector<BentoCandidate> companions;
        for (int i = 1; i < count; ++i) if (mask & (1u << (i - 1))) {
            indices.push_back(i);
            companions.push_back(candidates[i]);
        }
        if (int(indices.size()) > cap || (best && indices.size() <= best->candidateIndices.size())
            || std::find(indices.begin(), indices.end(), required) == indices.end()) continue;
        const auto rects = bentoSideLayout(choice, width, height, candidates[0], companions);
        if (!rects) continue;
        BentoAdmission admission;
        admission.candidateIndices = std::move(indices);
        admission.rects = *rects;
        best = std::move(admission);
    }
    // A side proportion is a preference, not a reason to hide a window that the
    // curated library can fit. That library governs only where it owns the cap;
    // where the three-pane grammar governs, the contact chose the shape and the
    // layout stays at two panes rather than solving a third some other way.
    const auto ordinary = cap > BentoCompactPaneCap
        ? chooseBentoTransferAdmission(candidates, required, width, height, cap)
        : std::optional<BentoAdmission>{};
    if (ordinary && (!best || ordinary->candidateIndices.size() > best->candidateIndices.size())
        && std::find(ordinary->candidateIndices.begin(), ordinary->candidateIndices.end(), 0)
            != ordinary->candidateIndices.end())
        best = ordinary;
    return best;
}
}

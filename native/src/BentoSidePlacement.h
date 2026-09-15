/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "BentoLayout.h"
#include <algorithm>
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

// Rect order is arrival, then existing companions. Reuse the curated presets
// inside the remainder. Admission below chooses which residents can fit.
inline std::optional<std::vector<BentoRect>> bentoSideLayout(
    BentoSidePlacement choice, int width, int height,
    const BentoCandidate &arrival, const std::vector<BentoCandidate> &companions)
{
    if (width <= 0 || height <= 0 || arrival.minimumHeight > height
        || arrival.minimumWidth > width) return std::nullopt;
    if (companions.empty()) return std::vector<BentoRect>{{0, 0, 1, 1}};
    if (companions.size() > 7) return std::nullopt;
    constexpr double halfGap = 7.0; // Existing makePixelBentoLayout gap / 2.
    const double low = (std::max(1.0, arrival.minimumWidth) + halfGap) / width;
    const double desired = choice.large ? 2.0/3.0 : 1.0/3.0;
    std::optional<std::vector<BentoRect>> best;
    double bestDistance = 2.0;
    const bool landscape = width * (1-desired) >= height;
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

inline std::optional<std::vector<BentoRect>> bentoSideLayout(BentoSidePlacement choice,
    int width, int height, const BentoCandidate &arrival, const std::optional<BentoCandidate> &companion)
{
    return bentoSideLayout(choice, width, height, arrival,
        companion ? std::vector<BentoCandidate>{*companion} : std::vector<BentoCandidate>{});
}

// Split an occupied full-height edge column without rebuilding unrelated panes.
// Arrival is appended, or moved out of a full-height third column. In that case
// the remaining adjacent column absorbs its old space before the target splits.
inline std::optional<std::vector<BentoRect>> splitBentoColumn(
    const std::vector<BentoRect> &existing, const std::vector<BentoCandidate> &candidates,
    int arrival, BentoSidePlacement choice, int width, int height, bool allowLarge)
{
    if (existing.size() < 2 || existing.size() >= 8 || arrival < 0
        || arrival > int(existing.size()) || candidates.size() != existing.size()+(arrival == int(existing.size()))
        || width <= 0 || height <= 0 || (!allowLarge && choice.large)) return std::nullopt;
    int target = -1;
    for (int i = 0; i < int(existing.size()); ++i) {
        const auto &r = existing[i];
        if (i != arrival && std::abs(r.y) < .002 && std::abs(r.height-1) < .002
            && r.width < .999 && (allowLarge || r.width <= .5)
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

// Keep the edge-selected card visible; park only the residents that cannot fit.
// Bound search like ordinary admission, preserve resident order, prefer more panes.
inline std::optional<BentoAdmission> chooseBentoSideAdmission(BentoSidePlacement choice,
    int width, int height, const std::vector<BentoCandidate> &candidates, int required = 0,
    bool allowMonitorFallback = false)
{
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
        if (indices.size() > 8 || (best && indices.size() <= best->candidateIndices.size())
            || std::find(indices.begin(), indices.end(), required) == indices.end()) continue;
        const auto rects = bentoSideLayout(choice, width, height, candidates[0], companions);
        if (!rects) continue;
        BentoAdmission admission;
        admission.candidateIndices = std::move(indices);
        admission.rects = *rects;
        best = std::move(admission);
    }
    // A side proportion is a preference, not a reason to hide a window that
    // the normal monitor layout can fit. Keep the side plan on equal counts.
    const auto ordinary = allowMonitorFallback
        ? chooseBentoTransferAdmission(candidates, required, width, height, 8)
        : std::optional<BentoAdmission>{};
    if (ordinary && (!best || ordinary->candidateIndices.size() > best->candidateIndices.size())
        && std::find(ordinary->candidateIndices.begin(), ordinary->candidateIndices.end(), 0)
            != ordinary->candidateIndices.end())
        best = ordinary;
    return best;
}
}

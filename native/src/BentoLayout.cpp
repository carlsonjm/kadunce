/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "BentoLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace Kadunce
{

namespace
{

bool fits(const BentoCandidate &candidate, const BentoPixelRect &rect)
{
    return rect.width + 2 >= candidate.minimumWidth
        && rect.height + 2 >= candidate.minimumHeight;
}

double assignmentCost(const BentoCandidate &candidate,
                      const BentoPixelRect &rect,
                      int candidateRank, int slotRank)
{
    const double windowAspect = std::max(1.0, candidate.width)
        / std::max(1.0, candidate.height);
    const double slotAspect = std::max(1, rect.width)
        / static_cast<double>(std::max(1, rect.height));
    const double aspectCost = std::abs(std::log(
        std::max(0.01, windowAspect / slotAspect)));
    const double rankCost = std::abs(candidateRank - slotRank) * 0.34;
    const double preferredCost = candidate.preferred && slotRank == 0
        ? -2.0 : 0.0;
    return aspectCost + rankCost + preferredCost;
}

bool assignCandidates(const std::vector<BentoCandidate> &candidates,
                      const std::vector<int> &subset,
                      const std::vector<BentoRect> &rects,
                      int areaWidth, int areaHeight,
                      std::vector<int> &assignment)
{
    const std::vector<BentoPixelRect> pixels = makePixelBentoLayout(
        rects, 0, 0, areaWidth, areaHeight);
    std::vector<int> slotOrder(rects.size());
    std::iota(slotOrder.begin(), slotOrder.end(), 0);
    std::stable_sort(slotOrder.begin(), slotOrder.end(),
                     [&pixels](int first, int second) {
        return pixels[first].width * pixels[first].height
            > pixels[second].width * pixels[second].height;
    });

    std::vector<bool> used(subset.size(), false);
    std::vector<int> current(rects.size(), -1);
    std::vector<int> best;
    double bestCost = std::numeric_limits<double>::max();

    const auto search = [&](const auto &self, int slotRank,
                            double cost) -> void {
        if (slotRank == static_cast<int>(slotOrder.size())) {
            if (cost < bestCost) {
                bestCost = cost;
                best = current;
            }
            return;
        }
        if (cost >= bestCost) {
            return;
        }
        const int slot = slotOrder[slotRank];
        for (int candidateRank = 0;
             candidateRank < static_cast<int>(subset.size());
             ++candidateRank) {
            if (used[candidateRank]) {
                continue;
            }
            const int candidateIndex = subset[candidateRank];
            const BentoCandidate &candidate = candidates[candidateIndex];
            if (!fits(candidate, pixels[slot])) {
                continue;
            }
            used[candidateRank] = true;
            current[slot] = candidateIndex;
            self(self, slotRank + 1,
                 cost + assignmentCost(candidate, pixels[slot],
                                       candidateRank, slotRank));
            current[slot] = -1;
            used[candidateRank] = false;
        }
    };
    search(search, 0, 0.0);
    if (best.empty()) {
        return false;
    }
    assignment = std::move(best);
    return true;
}

bool findSubsetAssignment(const std::vector<BentoCandidate> &candidates,
                          int count, const std::vector<BentoRect> &rects,
                          int areaWidth, int areaHeight,
                          std::vector<int> &assignment, int required = -1)
{
    std::vector<int> subset;
    subset.reserve(count);
    const auto search = [&](const auto &self, int next) -> bool {
        if (static_cast<int>(subset.size()) == count) {
            if (required >= 0 && std::find(subset.begin(), subset.end(), required) == subset.end())
                return false;
            return assignCandidates(candidates, subset, rects,
                                    areaWidth, areaHeight, assignment);
        }
        const int needed = count - static_cast<int>(subset.size());
        for (int index = next;
             index <= static_cast<int>(candidates.size()) - needed;
             ++index) {
            subset.push_back(index);
            if (self(self, index + 1)) {
                return true;
            }
            subset.pop_back();
        }
        return false;
    };
    return search(search, 0);
}

} // namespace

std::vector<BentoRect> makeBentoLayout(int count, bool landscape)
{
    if (count <= 0) {
        return {};
    }
    if (count == 1) {
        return {{0.0, 0.0, 1.0, 1.0}};
    }
    if (count == 2) {
        return landscape
            ? std::vector<BentoRect>{{0.0, 0.0, 0.62, 1.0},
                                     {0.62, 0.0, 0.38, 1.0}}
            : std::vector<BentoRect>{{0.0, 0.0, 1.0, 0.62},
                                     {0.0, 0.62, 1.0, 0.38}};
    }
    if (count == 3) {
        return landscape
            ? std::vector<BentoRect>{{0.0, 0.0, 0.60, 1.0},
                                     {0.60, 0.0, 0.40, 0.50},
                                     {0.60, 0.50, 0.40, 0.50}}
            : std::vector<BentoRect>{{0.0, 0.0, 1.0, 0.60},
                                     {0.0, 0.60, 0.50, 0.40},
                                     {0.50, 0.60, 0.50, 0.40}};
    }
    if (count == 4) {
        return {{0.0, 0.0, 0.50, 0.62},
                {0.50, 0.0, 0.50, 0.38},
                {0.0, 0.62, 0.50, 0.38},
                {0.50, 0.38, 0.50, 0.62}};
    }
    if (count == 5) {
        return {{0.22, 0.0, 0.56, 1.0},
                {0.0, 0.0, 0.22, 0.50},
                {0.0, 0.50, 0.22, 0.50},
                {0.78, 0.0, 0.22, 0.50},
                {0.78, 0.50, 0.22, 0.50}};
    }
    if (count == 6) {
        return {{0.0, 0.0, 0.50, 1.0},
                {0.50, 0.0, 0.25, 0.50},
                {0.50, 0.50, 0.25, 0.50},
                {0.75, 0.0, 0.25, 1.0 / 3.0},
                {0.75, 1.0 / 3.0, 0.25, 1.0 / 3.0},
                {0.75, 2.0 / 3.0, 0.25, 1.0 / 3.0}};
    }
    if (count <= 8) {
        std::vector<BentoRect> rects{{0.0, 0.0, 0.50, 0.64},
                                     {0.0, 0.64, 0.50, 0.36}};
        const int remaining = count - 2;
        const int middle = remaining / 2;
        const int right = remaining - middle;
        for (int index = 0; index < middle; ++index) {
            rects.push_back({0.50, index / static_cast<double>(middle),
                             0.25, 1.0 / middle});
        }
        for (int index = 0; index < right; ++index) {
            rects.push_back({0.75, index / static_cast<double>(right),
                             0.25, 1.0 / right});
        }
        return rects;
    }

    const int columns = static_cast<int>(std::ceil(std::sqrt(
        count * (landscape ? 1.6 : 0.7))));
    const int rows = static_cast<int>(std::ceil(
        count / static_cast<double>(columns)));
    std::vector<BentoRect> rects;
    rects.reserve(count);
    for (int index = 0; index < count; ++index) {
        const int column = index % columns;
        const int row = index / columns;
        rects.push_back({column / static_cast<double>(columns),
                         row / static_cast<double>(rows),
                         1.0 / columns, 1.0 / rows});
    }
    return rects;
}

std::vector<BentoRect> makeAlternateTwoPaneBentoLayout(bool landscape)
{
    return landscape
        ? std::vector<BentoRect>{{0.0, 0.0, 1.0, 0.50},
                                 {0.0, 0.50, 1.0, 0.50}}
        : std::vector<BentoRect>{{0.0, 0.0, 0.50, 1.0},
                                 {0.50, 0.0, 0.50, 1.0}};
}

std::vector<BentoRect> makeAlternateThreePaneBentoLayout(bool landscape)
{
    const double third = 1.0 / 3.0;
    return landscape
        ? std::vector<BentoRect>{{0.0, 0.0, third, 1.0},
                                 {third, 0.0, third, 1.0},
                                 {2.0 * third, 0.0, third, 1.0}}
        : std::vector<BentoRect>{{0.0, 0.0, 1.0, third},
                                 {0.0, third, 1.0, third},
                                 {0.0, 2.0 * third, 1.0, third}};
}

std::vector<BentoPixelRect> makePixelBentoLayout(
    const std::vector<BentoRect> &rects,
    int areaX, int areaY, int areaWidth, int areaHeight, int gap)
{
    std::vector<BentoPixelRect> pixels;
    pixels.reserve(rects.size());
    for (const BentoRect &rect : rects) {
        const double left = areaX + rect.x * areaWidth
            + (rect.x > 0.001 ? gap / 2.0 : 0.0);
        const double top = areaY + rect.y * areaHeight
            + (rect.y > 0.001 ? gap / 2.0 : 0.0);
        const double right = areaX + (rect.x + rect.width) * areaWidth
            - (rect.x + rect.width < 0.999 ? gap / 2.0 : 0.0);
        const double bottom = areaY + (rect.y + rect.height) * areaHeight
            - (rect.y + rect.height < 0.999 ? gap / 2.0 : 0.0);
        pixels.push_back({static_cast<int>(std::lround(left)),
                          static_cast<int>(std::lround(top)),
                          std::max(1, static_cast<int>(std::lround(right - left))),
                          std::max(1, static_cast<int>(std::lround(bottom - top)))});
    }
    return pixels;
}

BentoAdmission chooseBentoAdmission(
    const std::vector<BentoCandidate> &candidates,
    int areaWidth, int areaHeight, int maximumVisible)
{
    const int visible = std::min({maximumVisible,
                                  bentoPaneCap(areaWidth, areaHeight),
                                  static_cast<int>(candidates.size())});
    const bool landscape = bentoLandscapeArea(areaWidth, areaHeight);
    for (int count = visible; count >= 1; --count) {
        std::vector<std::vector<BentoRect>> layouts{
            makeBentoLayout(count, landscape)};
        if (count == 2) {
            layouts.push_back(makeAlternateTwoPaneBentoLayout(landscape));
        }
        if (count == 3) {
            layouts.push_back(makeAlternateThreePaneBentoLayout(landscape));
        }
        for (const std::vector<BentoRect> &layout : layouts) {
            std::vector<int> assignment;
            if (findSubsetAssignment(candidates, count, layout,
                                     areaWidth, areaHeight, assignment)) {
                return {std::move(assignment), layout};
            }
        }
    }
    return {};
}

std::optional<BentoAdmission> chooseBentoTransferAdmission(
    const std::vector<BentoCandidate> &candidates, int arrivingIndex,
    int areaWidth, int areaHeight, int maximumVisible)
{
    if (arrivingIndex < 0 || arrivingIndex >= static_cast<int>(candidates.size())
        || areaWidth <= 0 || areaHeight <= 0 || maximumVisible <= 0)
        return std::nullopt;
    for (const auto &candidate : candidates) {
        if (!std::isfinite(candidate.minimumWidth) || !std::isfinite(candidate.minimumHeight)
            || !std::isfinite(candidate.width) || !std::isfinite(candidate.height)
            || candidate.minimumWidth < 0 || candidate.minimumHeight < 0
            || candidate.width <= 0 || candidate.height <= 0) return std::nullopt;
    }
    const bool landscape = bentoLandscapeArea(areaWidth, areaHeight);
    for (int count = std::min({maximumVisible, bentoPaneCap(areaWidth, areaHeight),
                               static_cast<int>(candidates.size())}); count >= 1; --count) {
        std::vector<std::vector<BentoRect>> layouts{makeBentoLayout(count, landscape)};
        if (count == 2) layouts.push_back(makeAlternateTwoPaneBentoLayout(landscape));
        if (count == 3) layouts.push_back(makeAlternateThreePaneBentoLayout(landscape));
        for (const auto &layout : layouts) {
            std::vector<int> assignment;
            if (findSubsetAssignment(candidates, count, layout, areaWidth, areaHeight,
                                     assignment, arrivingIndex))
                return BentoAdmission{std::move(assignment), layout};
        }
    }
    return std::nullopt;
}

} // namespace Kadunce

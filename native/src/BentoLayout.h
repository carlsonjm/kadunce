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

[[nodiscard]] std::vector<BentoRect> makeBentoLayout(
    int count, bool landscape);
[[nodiscard]] std::vector<BentoRect> makeAlternateTwoPaneBentoLayout(
    bool landscape);
[[nodiscard]] std::vector<BentoPixelRect> makePixelBentoLayout(
    const std::vector<BentoRect> &rects,
    int areaX, int areaY, int areaWidth, int areaHeight,
    int gap = 14);
[[nodiscard]] BentoAdmission chooseBentoAdmission(
    const std::vector<BentoCandidate> &candidates,
    int areaWidth, int areaHeight, int maximumVisible = 8);

// A transfer is accepted only if the arriving candidate has a visible pane.
// Unlike ordinary reflow, silently parking this candidate is rejection.
[[nodiscard]] std::optional<BentoAdmission> chooseBentoTransferAdmission(
    const std::vector<BentoCandidate> &candidates, int arrivingIndex,
    int areaWidth, int areaHeight, int maximumVisible = 8);

} // namespace Kadunce

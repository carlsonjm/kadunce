/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardLineLayout.h"

#include <algorithm>
#include <span>

namespace Kadunce
{

struct ProjectedStackPreviewPolicy {
    double commonHeight = 0.0;
    double widestAspect = 0.0;

    [[nodiscard]] bool valid() const
    {
        return commonHeight > 0.0 && widestAspect > 0.0;
    }
};

// Open projected stacks normalize to one live-preview height. The widest
// source aspect sets the height that lets every member remain proportional
// inside the canonical slot; layout and input continue to own the full slot.
[[nodiscard]] inline ProjectedStackPreviewPolicy
makeProjectedStackPreviewPolicy(
    const CardRect &slot, std::span<const double> memberAspects)
{
    if (slot.width <= 0.0 || slot.height <= 0.0) {
        return {};
    }
    double widestAspect = 0.0;
    for (const double aspect : memberAspects) {
        if (aspect > 0.0) {
            widestAspect = std::max(widestAspect, aspect);
        }
    }
    if (widestAspect <= 0.0) {
        return {};
    }
    return {
        .commonHeight = std::min(slot.height, slot.width / widestAspect),
        .widestAspect = widestAspect,
    };
}

// Fit the complete proportional source inside a fixed layout slot. The slot
// remains authoritative for Card Line pitch and input; this rectangle is only
// the maximum visual aperture available to a source with a different aspect.
[[nodiscard]] inline CardRect makeProjectedCardVisualRect(
    const CardRect &slot, double sourceWidth, double sourceHeight)
{
    if (slot.width <= 0.0 || slot.height <= 0.0
        || sourceWidth <= 0.0 || sourceHeight <= 0.0) {
        return slot;
    }

    const double scale = std::min(slot.width / sourceWidth,
                                  slot.height / sourceHeight);
    const double paintedWidth = sourceWidth * scale;
    const double paintedHeight = sourceHeight * scale;
    return CardRect{
        slot.x + (slot.width - paintedWidth) / 2.0,
        slot.y + (slot.height - paintedHeight) / 2.0,
        paintedWidth,
        paintedHeight,
    };
}

[[nodiscard]] inline CardRect makeNormalizedProjectedCardVisualRect(
    const CardRect &slot, double sourceWidth, double sourceHeight,
    const ProjectedStackPreviewPolicy &policy)
{
    if (!policy.valid() || sourceWidth <= 0.0 || sourceHeight <= 0.0) {
        return makeProjectedCardVisualRect(slot, sourceWidth, sourceHeight);
    }

    const double aspect = sourceWidth / sourceHeight;
    const double paintedWidth = aspect * policy.commonHeight;
    return {
        slot.x + (slot.width - paintedWidth) / 2.0,
        slot.y + (slot.height - policy.commonHeight) / 2.0,
        paintedWidth,
        policy.commonHeight,
    };
}

} // namespace Kadunce

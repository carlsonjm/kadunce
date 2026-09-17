/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BentoLayout.h"
#include "CardLineLayout.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Kadunce
{

constexpr double BentoStageSideInset = 10.0;
constexpr double BentoStageTopInset = 10.0;
constexpr double BentoStageBottomInset = 20.0;

struct BentoCompositeGeometry {
    CardRect sourceUnion;
    CardRect targetUnion;
    double scale = 0.0;

    [[nodiscard]] bool valid() const
    {
        return sourceUnion.width > 0.0 && sourceUnion.height > 0.0
            && targetUnion.width > 0.0 && targetUnion.height > 0.0
            && scale > 0.0;
    }
};

struct BentoProjectedPaneGeometry {
    CardRect authoritativeFrame;
    CardRect authoritativeSurface;
    CardRect targetSurface;
    CardRect targetClip;
};

[[nodiscard]] inline CardRect makeBentoStageArea(const CardRect &workspace)
{
    if (workspace.width <= BentoStageSideInset * 2.0
        || workspace.height <= BentoStageTopInset + BentoStageBottomInset) return {};
    return {workspace.x + BentoStageSideInset,
        workspace.y + BentoStageTopInset,
        workspace.width - BentoStageSideInset * 2.0,
        workspace.height - BentoStageTopInset - BentoStageBottomInset};
}

[[nodiscard]] inline BentoCompositeGeometry makeBentoCompositeGeometry(
    const CardRect &slot, const CardRect &workspace)
{
    if (slot.width <= 0.0 || slot.height <= 0.0
        || workspace.width <= 0.0 || workspace.height <= 0.0) {
        return {};
    }
    const double scale = std::min(slot.width / workspace.width,
                                  slot.height / workspace.height);
    const CardRect target{
        slot.x + (slot.width - workspace.width * scale) / 2.0,
        slot.y + (slot.height - workspace.height * scale) / 2.0,
        workspace.width * scale,
        workspace.height * scale,
    };
    return {workspace, target, scale};
}

[[nodiscard]] inline CardRect mapBentoCompositeRect(
    const BentoCompositeGeometry &geometry, const CardRect &source)
{
    if (!geometry.valid() || source.width <= 0.0 || source.height <= 0.0) {
        return {};
    }
    return {
        geometry.targetUnion.x
            + (source.x - geometry.sourceUnion.x) * geometry.scale,
        geometry.targetUnion.y
            + (source.y - geometry.sourceUnion.y) * geometry.scale,
        source.width * geometry.scale,
        source.height * geometry.scale,
    };
}

[[nodiscard]] inline CardRect intersectBentoCompositeRect(
    const CardRect &first, const CardRect &second)
{
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.right(), second.right());
    const double bottom = std::min(first.bottom(), second.bottom());
    return right > left && bottom > top
        ? CardRect{left, top, right - left, bottom - top} : CardRect{};
}

[[nodiscard]] inline std::optional<BentoProjectedPaneGeometry>
makeBentoProjectedPaneGeometry(const BentoCompositeGeometry &composite,
    const CardRect &workspace, const BentoRect &storedRect,
    const CardRect &liveFrame, const CardRect &liveExpanded)
{
    const auto stage = makeBentoStageArea(workspace);
    if (!composite.valid() || stage.width <= 0.0 || stage.height <= 0.0
        || liveFrame.width <= 0.0 || liveFrame.height <= 0.0
        || liveExpanded.width <= 0.0 || liveExpanded.height <= 0.0) return std::nullopt;
    const auto pixels = makePixelBentoLayout({storedRect}, int(std::lround(stage.x)),
        int(std::lround(stage.y)), int(std::lround(stage.width)),
        int(std::lround(stage.height)));
    if (pixels.size() != 1) return std::nullopt;
    const auto &pixel = pixels.front();
    const CardRect frame{double(pixel.x), double(pixel.y),
        double(pixel.width), double(pixel.height)};
    const double left = std::max(0.0, liveFrame.x - liveExpanded.x);
    const double top = std::max(0.0, liveFrame.y - liveExpanded.y);
    const double right = std::max(0.0, liveExpanded.right() - liveFrame.right());
    const double bottom = std::max(0.0, liveExpanded.bottom() - liveFrame.bottom());
    const CardRect surface{frame.x - left, frame.y - top,
        frame.width + left + right, frame.height + top + bottom};
    const auto target = mapBentoCompositeRect(composite, surface);
    // Decorations and shadows participate in the live transform, but the stored
    // Bento pane frame remains the hard aperture. This preserves client chrome
    // inside the frame without letting expanded pixels consume workspace gaps.
    const auto clip = intersectBentoCompositeRect(
        mapBentoCompositeRect(composite, frame), composite.targetUnion);
    if (target.width <= 0.0 || target.height <= 0.0
        || clip.width <= 0.0 || clip.height <= 0.0) return std::nullopt;
    return BentoProjectedPaneGeometry{frame, surface, target, clip};
}

} // namespace Kadunce

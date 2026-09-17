/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardLineLayout.h"

#include <algorithm>
#include <span>
#include <vector>

namespace Kadunce
{

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

[[nodiscard]] inline BentoCompositeGeometry makeBentoCompositeGeometry(
    const CardRect &slot, std::span<const CardRect> paneFrames)
{
    if (slot.width <= 0.0 || slot.height <= 0.0 || paneFrames.empty()) {
        return {};
    }
    double left = paneFrames.front().x;
    double top = paneFrames.front().y;
    double right = paneFrames.front().right();
    double bottom = paneFrames.front().bottom();
    for (const auto &frame : paneFrames) {
        if (frame.width <= 0.0 || frame.height <= 0.0) {
            return {};
        }
        left = std::min(left, frame.x);
        top = std::min(top, frame.y);
        right = std::max(right, frame.right());
        bottom = std::max(bottom, frame.bottom());
    }
    const CardRect source{left, top, right - left, bottom - top};
    const double scale = std::min(slot.width / source.width,
                                  slot.height / source.height);
    const CardRect target{
        slot.x + (slot.width - source.width * scale) / 2.0,
        slot.y + (slot.height - source.height * scale) / 2.0,
        source.width * scale,
        source.height * scale,
    };
    return {source, target, scale};
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

} // namespace Kadunce

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CardLineLayout.h"

#include <algorithm>
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

} // namespace Kadunce

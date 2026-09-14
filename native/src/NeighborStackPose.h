#pragma once

#include "CardLineLayout.h"
#include <algorithm>

namespace Kadunce {
// Reserve most of the side peek for the face; share the rest equally among
// shoulders. The inward edge stays at the compact deck's existing boundary.
inline CardStackPose neighborStackPose(int depth, int count, int side,
                                       double available, double compactExtent)
{
    const int shoulders = std::min(std::max(count - 1, 0), 3);
    if (!shoulders) return {0, 0, 0, true};
    if (depth > shoulders) return {0, 0, 0, false};
    const double spread = std::max(0.0, available) * 0.4;
    const double step = spread / shoulders;
    return {side > 0 ? -compactExtent + (shoulders - depth) * step
                     : -depth * step, 0, 0, true};
}
}

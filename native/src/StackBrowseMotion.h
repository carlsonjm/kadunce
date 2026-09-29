// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>

namespace Kadunce {
inline constexpr int StackBrowseDuration = 220;
struct StackBrowseAccent { double y; double rotation; };

// A small rigid depth cue, never a layout or input change. Zero value and
// slope at both endpoints let a new capture rebase without an initial kick.
inline StackBrowseAccent stackBrowseAccent(double progress, double height,
                                          int direction, int faceRole)
{
    const double p = std::clamp(progress, 0.0, 1.0);
    const double pulse = 16*p*p*(1-p)*(1-p);
    // Up sends the old face away. Down retrieves the new face: give the
    // incoming card the downward gesture, not another departing-card bump.
    // The outgoing face still follows its ordinary fan interpolation.
    const int sign = direction > 0 ? (faceRole > 0 ? -1 : 0)
        : direction < 0 ? -(faceRole < 0 ? -1 : faceRole > 0 ? 1 : 0) : 0;
    const double lift = std::clamp(height*0.035, 0.0, 24.0);
    return {-sign*lift*pulse, sign*0.25*pulse};
}
}

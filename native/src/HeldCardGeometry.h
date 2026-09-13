/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CardLineLayout.h"
#include <algorithm>
namespace Kadunce {
inline constexpr double HeldCardFraction = 0.44;
// Immutable pickup plus contact-relative scale; drag translation is applied once
// by the caller. This is the previously tried44% math without old paging code.
inline CardRect anchoredStackCarry(const CardRect &pickup, double contactX,
    double contactY, double targetWidth, double targetHeight, double progress)
{
    if (pickup.width <= 0 || pickup.height <= 0 || targetWidth <= 0 || targetHeight <= 0)
        return pickup;
    const double t = std::clamp(progress, 0.0, 1.0);
    const double w = pickup.width + (targetWidth - pickup.width) * t;
    const double h = pickup.height + (targetHeight - pickup.height) * t;
    return {contactX - (contactX - pickup.x) * w / pickup.width,
            contactY - (contactY - pickup.y) * h / pickup.height, w, h};
}
}

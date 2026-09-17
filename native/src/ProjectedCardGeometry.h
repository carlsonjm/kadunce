/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardLineLayout.h"

#include <algorithm>

namespace Kadunce
{

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

} // namespace Kadunce

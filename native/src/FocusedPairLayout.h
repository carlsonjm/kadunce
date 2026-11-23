/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "SpreadLayout.h"

namespace Kadunce
{
// A pair uses the same aspect ratio and shoulder gap as ordinary Spread.
// Only the center grows. Shoulders retain their standard three-card size.
inline SpreadLayout makeFocusedPairLayout(double x, double y, double width, double height)
{
    auto layout = makeSpreadLayout(x, y, width, height);
    const double w = width * 0.64;
    const double h = height * 0.64;
    const double cx = x + (width - w) / 2.0;
    layout.cards[1] = {cx, y + (height - h) / 2.0, w, h};
    layout.cards[0].x = cx - layout.gutter - layout.cards[0].width;
    layout.cards[2].x = cx + w + layout.gutter;
    return layout;
}

inline CardRect makeReservedFocusedPairTarget(const SpreadLayout &layout, int slot,
                                              const CardStackEnvelope &envelope)
{
    auto target = layout.cards[slot < 0 ? 0 : slot > 0 ? 2 : 1];
    target.x -= (envelope.left + envelope.right) / 2.0;
    target.x += slot < 0 ? envelope.left : slot > 0 ? envelope.right : 0.0;
    return target;
}

inline SpreadLayout makeLauncherGuestLayout(double x, double y, double width,
                                               double height, int neighborGroups)
{
    return neighborGroups <= 1 ? makeFocusedPairLayout(x, y, width, height)
                              : makeSpreadLayout(x, y, width, height);
}
}

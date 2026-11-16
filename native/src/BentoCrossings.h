/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "BentoLayout.h"

#include <cmath>
#include <vector>

namespace Kadunce
{

// A point inside a layout where four panes meet at their corners: the line
// between columns crosses the line between rows there, and both move at once
// from it.
struct BentoCrossing
{
    // The pane above and left of the point, whose right and bottom edges are
    // the two lines.
    int topLeft = -1;
    double x = 0.0;
    double y = 0.0;
};

inline std::vector<BentoCrossing> bentoCrossings(const std::vector<BentoRect> &rects)
{
    constexpr double Epsilon = 0.002;
    const auto near = [](double a, double b) { return std::abs(a - b) <= Epsilon; };
    std::vector<BentoCrossing> crossings;
    for (int i = 0; i < int(rects.size()); ++i) {
        const BentoRect &a = rects[std::size_t(i)];
        const double x = a.x + a.width;
        const double y = a.y + a.height;
        if (x < Epsilon || x > 1.0 - Epsilon || y < Epsilon || y > 1.0 - Epsilon) continue;
        bool topRight = false, bottomLeft = false, bottomRight = false;
        for (int j = 0; j < int(rects.size()); ++j) {
            if (j == i) continue;
            const BentoRect &b = rects[std::size_t(j)];
            topRight = topRight || (near(b.x, x) && near(b.y + b.height, y));
            bottomLeft = bottomLeft || (near(b.x + b.width, x) && near(b.y, y));
            bottomRight = bottomRight || (near(b.x, x) && near(b.y, y));
        }
        if (topRight && bottomLeft && bottomRight) crossings.push_back({i, x, y});
    }
    return crossings;
}

} // namespace Kadunce

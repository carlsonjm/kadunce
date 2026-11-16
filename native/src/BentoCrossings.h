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

// A point inside a layout where a line between rows meets a line between
// columns: four panes meeting at their corners, or three where a column's
// rows meet a pane beside them. Both lines move at once from it.
struct BentoCrossing
{
    // A pane whose right edge is the line between columns.
    int left = -1;
    // A pane whose bottom edge is the line between rows.
    int above = -1;
    // Four panes meet: the line between rows runs on both sides.
    bool full = false;
    double x = 0.0;
    double y = 0.0;
};

inline std::vector<BentoCrossing> bentoCrossings(const std::vector<BentoRect> &rects)
{
    constexpr double Epsilon = 0.002;
    const auto near = [](double a, double b) { return std::abs(a - b) <= Epsilon; };
    const auto inside = [](double v) { return v > Epsilon && v < 1.0 - Epsilon; };
    const int count = int(rects.size());
    std::vector<BentoCrossing> crossings;
    for (int i = 0; i < count; ++i) {
        const BentoRect &a = rects[std::size_t(i)];
        const double y = a.y + a.height;
        if (!inside(y)) continue;
        // `a` is above a line between rows; look at each of its sides.
        for (const bool right : {true, false}) {
            const double x = right ? a.x + a.width : a.x;
            if (!inside(x)) continue;
            bool below = false;
            for (int j = 0; j < count && !below; ++j) {
                const BentoRect &b = rects[std::size_t(j)];
                below = j != i && near(b.y, y) && near(right ? b.x + b.width : b.x, x);
            }
            if (!below) continue;
            // Across the line between columns: a pane spanning the point, or a
            // line between rows of its own there.
            int across = -1, acrossAbove = -1;
            bool acrossBelow = false;
            for (int j = 0; j < count; ++j) {
                const BentoRect &c = rects[std::size_t(j)];
                if (j == i || !near(right ? c.x : c.x + c.width, x)) continue;
                if (c.y < y - Epsilon && c.y + c.height > y + Epsilon) across = j;
                if (near(c.y + c.height, y)) acrossAbove = j;
                if (near(c.y, y)) acrossBelow = true;
            }
            const bool full = acrossAbove >= 0 && acrossBelow;
            if (across < 0 && !full) continue;
            bool seen = false;
            for (const auto &crossing : crossings) seen = seen || (near(crossing.x, x) && near(crossing.y, y));
            if (seen) continue;
            // A full crossing is held by its top-left pane, both lines its edges.
            const int above = full && !right ? acrossAbove : i;
            const int left = right ? i : full ? acrossAbove : across;
            crossings.push_back({left, above, full, x, y});
        }
    }
    return crossings;
}

} // namespace Kadunce

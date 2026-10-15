/* SPDX-License-Identifier: GPL-2.0-or-later */
// Behavioral coverage of where a touch starts against an edge. Spread can be
// on a monitor while the touchscreen drives another display; each edge
// gesture on the touchscreen is measured against that display's own edges,
// across as well as down, and the Spread display keeps its own.
#include "TouchEdge.h"

#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

constexpr double Band = 20.0;

TouchEdgeDisplay display(double x, double y, double width, double height)
{
    // A display's geometry names its last row, one above its logical bottom.
    return {QRectF(x, y, width, height), y + height - 1.0};
}

// Spread on a 2560x1440 monitor at the origin; the touchscreen is a 1280x800
// tablet to its right, its bottom far above the monitor's.
const TouchEdgeDisplay Monitor = display(0, 0, 2560, 1440);
const TouchEdgeDisplay Tablet = display(2560, 0, 1280, 800);

bool bottom(const QPointF &p, const TouchEdgeDisplay &spread, const TouchEdgeDisplay &touchscreen)
{
    return atTouchBottomEdge(p, touchEdgeDisplayAt(p, spread, touchscreen), Band);
}

bool top(const QPointF &p, const TouchEdgeDisplay &spread, const TouchEdgeDisplay &touchscreen)
{
    return atTouchTopEdge(p, touchEdgeDisplayAt(p, spread, touchscreen), Band);
}
} // namespace

int main()
{
    // The touchscreen's own edges, with Spread on the monitor.
    require(bottom(QPointF(3000, 795), Monitor, Tablet),
            "a swipe up from the touchscreen's bottom starts at its edge");
    require(top(QPointF(3000, 5), Monitor, Tablet),
            "a pull down from the touchscreen's top starts at its edge");
    require(!bottom(QPointF(3000, 400), Monitor, Tablet) && !top(QPointF(3000, 400), Monitor, Tablet),
            "the middle of the touchscreen is no edge");
    require(touchEdgeDisplayAt(QPointF(3000, 795), Monitor, Tablet)->area == Tablet.area,
            "a touch on the touchscreen is measured against the touchscreen");

    // Across as well as down: the touchscreen's bottom row is not the
    // monitor's, and the monitor keeps its own bottom.
    require(!bottom(QPointF(1000, 795), Monitor, Tablet),
            "the monitor at the touchscreen's bottom height is no edge");
    require(bottom(QPointF(1000, 1435), Monitor, Tablet),
            "the monitor's own bottom is still the Spread display's edge");
    require(top(QPointF(1000, 5), Monitor, Tablet),
            "the monitor's own top is still the Spread display's edge");

    // Below the tablet lies no display at all.
    require(!touchEdgeDisplayAt(QPointF(3000, 1000), Monitor, Tablet),
            "a point on no display has no edges");
    require(!bottom(QPointF(3000, 1435), Monitor, Tablet),
            "the monitor's bottom height beside it is no edge either");

    // The shared boundary belongs to the display on its right, half-open.
    require(touchEdgeDisplayAt(QPointF(2560, 795), Monitor, Tablet)->area == Tablet.area,
            "the boundary column is the touchscreen's");
    require(touchEdgeDisplayAt(QPointF(2559.5, 795), Monitor, Tablet)->area == Monitor.area,
            "the column left of it is the monitor's");

    // The touchscreen on the Spread display: one display, its own edges.
    const TouchEdgeDisplay Alone = display(0, 0, 1280, 800);
    require(bottom(QPointF(640, 790), Alone, Alone), "the tablet's bottom band");
    require(!bottom(QPointF(640, 770), Alone, Alone), "above the tablet's bottom band");
    require(top(QPointF(640, 19), Alone, Alone) && !top(QPointF(640, 20), Alone, Alone),
            "the tablet's top band");

    // No touchscreen: only the Spread display has edges.
    require(bottom(QPointF(640, 1435), Monitor, TouchEdgeDisplay{}),
            "without a touchscreen the Spread display keeps its bottom");
    require(!touchEdgeDisplayAt(QPointF(3000, 795), Monitor, TouchEdgeDisplay{}),
            "without a touchscreen nothing beside the Spread display has edges");

    std::cout << "touch-edge: ok\n";
    return 0;
}

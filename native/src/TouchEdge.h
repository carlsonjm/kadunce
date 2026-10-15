/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QPointF>
#include <QRectF>

#include <optional>

namespace Kadunce
{
// The edges a touch is measured against. Contacts arrive in global
// coordinates, and a touchscreen can drive a display other than the Spread
// display, so an edge belongs to one display: its band is that display's
// bottom or top, and the touch has to land on it, across as well as down.
struct TouchEdgeDisplay {
    QRectF area;
    // The display's last row, as its own geometry reports it, from which the
    // bottom band is measured.
    double bottomInclusive = 0.0;

    [[nodiscard]] bool isValid() const { return area.isValid(); }
    // Half-open and unrounded, as the Spread display's own test is: the last
    // physical rows map to within a pixel of the logical bottom.
    [[nodiscard]] bool contains(const QPointF &position) const
    {
        return isValid() && position.x() >= area.left() && position.x() < area.right()
            && position.y() >= area.top() && position.y() < area.bottom();
    }
};

// The display whose edges a touch at `position` starts against: the display a
// touchscreen drives where the touch lands on it, else the Spread display
// where it lands there, else none.
inline std::optional<TouchEdgeDisplay> touchEdgeDisplayAt(const QPointF &position,
    const TouchEdgeDisplay &spread, const TouchEdgeDisplay &touchscreen)
{
    if (touchscreen.contains(position)) return touchscreen;
    if (spread.contains(position)) return spread;
    return std::nullopt;
}

// Whether a touch at `position` starts within `band` of its display's bottom.
inline bool atTouchBottomEdge(const QPointF &position,
    const std::optional<TouchEdgeDisplay> &display, double band)
{
    return display && display->contains(position)
        && position.y() >= display->bottomInclusive - band;
}

// Whether a touch at `position` starts within `band` of its display's top.
inline bool atTouchTopEdge(const QPointF &position,
    const std::optional<TouchEdgeDisplay> &display, double band)
{
    return display && display->contains(position)
        && position.y() < display->area.top() + band;
}
} // namespace Kadunce

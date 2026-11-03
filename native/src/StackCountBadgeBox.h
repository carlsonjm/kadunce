/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QRectF>

#include <algorithm>
#include <cmath>

namespace Kadunce
{
// Where a Stack's count stands for a front card drawn at face, turned by
// rotation degrees (clockwise on screen) about its lower-right corner: centred
// a little inside the turned top-right corner, and kept on the display.
[[nodiscard]] inline QRectF stackCountBadgeBox(const QRectF &face, double rotation,
                                               const QRectF &display, double diameter)
{
    if (face.isEmpty() || diameter <= 0.0) return {};
    constexpr double Inset = 4.0;
    const double radians = rotation * M_PI / 180.0;
    // The top-right corner sits face.height() above the pivot.
    const double dx = face.height() * std::sin(radians);
    const double dy = -face.height() * std::cos(radians);
    const QPointF corner(face.right() + dx, face.bottom() + dy);
    QRectF box(corner.x() - diameter / 2.0 - Inset, corner.y() - diameter / 2.0 + Inset,
               diameter, diameter);
    if (!display.isEmpty()) {
        box.moveLeft(std::clamp(box.left(), display.left(),
                                std::max(display.left(), display.right() - diameter)));
        box.moveTop(std::clamp(box.top(), display.top(),
                               std::max(display.top(), display.bottom() - diameter)));
    }
    return box;
}
} // namespace Kadunce

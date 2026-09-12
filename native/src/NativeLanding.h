/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QRectF>
#include <algorithm>
namespace Kadunce {
// One release-time translation, never a resize. Oversized windows keep their
// title bar reachable rather than being pushed above the display.
inline QRectF safeNativeLanding(QRectF frame, QRectF area)
{
    if (!frame.isValid() || !area.isValid()) return frame;
    const double left = area.left() + 10, top = area.top() + 10;
    frame.moveLeft(std::clamp(frame.left(), left, std::max(left, area.right() - 10 - frame.width())));
    frame.moveTop(std::clamp(frame.top(), top, std::max(top, area.bottom() - 10 - frame.height())));
    return frame;
}
}

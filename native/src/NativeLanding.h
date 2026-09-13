/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QRectF>
#include <QList>
#include <algorithm>
namespace Kadunce {
// Floating bottom docks may reserve no work-area strut. Treat their visible
// frame as an additional landing boundary, never as an input interception.
inline QRectF nativeLandingArea(QRectF output, QRectF workArea, const QList<QRectF> &docks)
{
    auto area = workArea.intersected(output);
    for (const auto &dock : docks) {
        if (dock.intersects(output) && dock.width() > dock.height()
            && dock.center().y() > output.center().y()
            && dock.bottom() >= output.bottom() - 48)
            area.setBottom(std::min(area.bottom(), dock.top()));
    }
    return area;
}
inline bool inNativeDockReleaseZone(QPointF contact, QRectF output, QRectF area)
{
    return output.contains(contact) && area.isValid()
        && contact.y() >= std::min(output.bottom() - 24, area.bottom());
}
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

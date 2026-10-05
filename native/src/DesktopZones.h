/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QPointer>
#include <QRectF>

class QObject;

namespace KWin {
class LogicalOutput;
class Window;
}

namespace Kadunce {

// One zone drawn for a display with KDE's tile editor (Meta+T): KWin's own
// tile object and the rectangle a window placed in it takes.
struct DrawnZone {
    QPointer<QObject> tile;
    QRectF window;
};

// The zones drawn for a display on the current desktop, in KWin's order.
// KWin's untouched default layout, or a display with no layout, has none:
// a display without drawn zones keeps the curated library
// (CARD-LIFECYCLE.md §11).
[[nodiscard]] QList<DrawnZone> drawnZones(KWin::LogicalOutput *output);

// The rectangle a window placed in a zone takes now, which moves with the
// zone's edges; empty when the zone is gone.
[[nodiscard]] QRectF zoneWindowRect(QObject *tile);

// Gives a window to KWin's zone, so KWin owns its geometry and the edges it
// shares; false when KWin refuses it, as for a window that cannot resize.
bool placeInZone(QObject *tile, KWin::Window *window);

// Takes a window back out of whatever zone holds it.
void leaveZone(KWin::Window *window);

// The zone holding a window, if any.
[[nodiscard]] QObject *zoneOf(KWin::Window *window);

} // namespace Kadunce

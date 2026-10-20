/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "ReleasePlace.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>

#include <QRectF>
#include <QString>

namespace Kadunce
{
// Where a window stood before Cards placed it, and as what: the one page every
// restore record keeps (CARD-LIFECYCLE.md §13). The card stage, a layout and a
// carry each add only what they alone need, so a record handed from one to
// another is copied whole and never rebuilt field by field.
struct WindowPlace {
    KWin::RectF geometry;
    KWin::RectF floatingGeometry;
    KWin::RectF fullscreenRestoreGeometry;
    KWin::QuickTileMode quickTileMode;
    KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
    bool fullScreen = false;
    bool minimized = false;
    // The display the place was taken on, by name, so one unplugged and
    // plugged back in is still found, and its geometry then.
    QString outputName;
    QRectF outputGeometry;

    // Neither maximized, tiled nor full screen: the place is the window's own.
    [[nodiscard]] bool floating() const
    {
        return !fullScreen && maximizeMode == KWin::MaximizeRestore
            && quickTileMode == KWin::QuickTileMode{};
    }

    [[nodiscard]] ReleaseRecord releaseRecord() const
    {
        return {QRectF(geometry), outputName, outputGeometry};
    }

    // The place carried onto `work` from `from` (ReleasePlace.h), each of its
    // places alike, so un-maximizing it later stays on the display too.
    [[nodiscard]] WindowPlace carriedOnto(const QRectF &from, const QRectF &work) const
    {
        const auto carried = [&](const KWin::RectF &place) {
            return place.isValid() ? KWin::RectF(carriedPlace(QRectF(place), from, work)) : place;
        };
        WindowPlace moved = *this;
        moved.geometry = carried(geometry);
        moved.floatingGeometry = carried(floatingGeometry);
        moved.fullscreenRestoreGeometry = carried(fullscreenRestoreGeometry);
        return moved;
    }
};

// Every display there is now, with the room its panels leave.
inline QList<ReleaseDisplay> releaseDisplays()
{
    QList<ReleaseDisplay> displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens())
        if (output) displays.append({output->name(), QRectF(output->geometry()),
            QRectF(KWin::effects->clientArea(KWin::MaximizeArea, output))});
    return displays;
}

// The place `client` has now, at `geometry` on `output`.
inline WindowPlace placeOf(KWin::Window *client, const KWin::RectF &geometry,
                           const KWin::LogicalOutput *output)
{
    return {geometry, client->geometryRestore(), client->fullscreenGeometryRestore(),
        client->quickTileMode(), client->maximizeMode(), client->isFullScreen(),
        client->isMinimized(), output ? output->name() : QString(),
        output ? QRectF(output->geometry()) : QRectF()};
}
} // namespace Kadunce

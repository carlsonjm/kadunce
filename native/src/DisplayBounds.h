/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "NativeMoveProtocol.h"

#include <effect/effecthandler.h>
#include <window.h>

#include <algorithm>

namespace Kadunce {
// KWin tells a Wayland window how large it may grow (xdg_toplevel's
// configure_bounds) only when it lets the window choose its own size, so a
// window that opened on one display keeps that display's bounds after moving
// to a larger one. Some toolkits keep within them, and a window Kadunce sizes
// to a pane on the larger display then stops at the smaller one's height.
// Main compositor thread only; X11 windows have no bounds and are skipped.
inline bool sendDisplayBounds(KWin::Window *client, KWin::LogicalOutput *output)
{
    if (!client || !output || client->isDeleted()) return false;
    const QPointer<KWin::XdgToplevelInterface> toplevel = nativeMoveProtocol(client->surface());
    if (!toplevel) return false;
    const KWin::RectF area = KWin::effects->clientArea(KWin::PlacementArea, output);
    const double width = area.width() - client->borderLeft() - client->borderRight();
    const double height = area.height() - client->borderTop() - client->borderBottom();
    toplevel->sendConfigureBounds(QSizeF(std::max(1.0, width), std::max(1.0, height)));
    return true;
}
} // namespace Kadunce

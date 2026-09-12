/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <wayland/clientconnection.h>
#include <wayland/surface.h>
#include <wayland/xdgshell.h>
#include <wayland-server-core.h>
#include <QPointer>
#include <cstring>

namespace Kadunce {
// Main compositor thread only. Enumerate the surface owner's protocol resources
// once when attaching to a window, never per frame/input event. No QObject-tree
// assumptions or unexported XdgToplevelWindow symbols. Non-xdg surfaces reject.
inline QPointer<KWin::XdgToplevelInterface> nativeMoveProtocol(KWin::SurfaceInterface *surface)
{
    if (!surface || !surface->client() || !surface->client()->client()) return {};
    struct Lookup {
        KWin::SurfaceInterface *surface;
        QPointer<KWin::XdgToplevelInterface> result;
    } lookup{surface, {}};
    wl_client_for_each_resource(surface->client()->client(),
        [](wl_resource *resource, void *data) {
            auto &lookup = *static_cast<Lookup *>(data);
            if (std::strcmp(wl_resource_get_class(resource), "xdg_toplevel") != 0)
                return WL_ITERATOR_CONTINUE;
            auto *toplevel = KWin::XdgToplevelInterface::get(resource);
            if (!toplevel || toplevel->surface() != lookup.surface) return WL_ITERATOR_CONTINUE;
            lookup.result = toplevel;
            return WL_ITERATOR_STOP;
        }, &lookup);
    return lookup.result;
}
} // namespace Kadunce

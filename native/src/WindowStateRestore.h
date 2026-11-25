#pragma once
#include "ReleasePlace.h"

#include <effect/effecthandler.h>
#include <window.h>

namespace Kadunce {
// Mechanics only: callers retain output choice, focus/stacking order and
// mutation guards. Preserve the tested fullscreen restoration sequence.
template<class Client, class Snapshot, class Valid>
bool restoreWindowStateChecked(Client *client, const Snapshot &snapshot,
                        const KWin::RectF &geometry, bool preserveRestoreGeometry,
                        bool manageMinimized, bool minimized, Valid valid)
{
    const auto step = [&](auto action) {
        if (!valid()) return false;
        action();
        return valid();
    };
    if (!step([&] { if (client->isFullScreen()) client->setFullScreen(false); })) return false;
    if (!step([&] { if (client->maximizeMode() != KWin::MaximizeRestore) client->maximize(KWin::MaximizeRestore); })) return false;
    if (!step([&] { if (client->quickTileMode() != KWin::QuickTileMode{}) {
        client->setQuickTileMode(KWin::QuickTileMode{}, client->frameGeometry().center());
    } })) return false;
    if (!step([&] { if (manageMinimized) client->setMinimized(false); })) return false;
    if (!step([&] { client->moveResize(geometry); })) return false;
    // Given back maximized, it keeps a smaller place to restore to: never the
    // maximized room itself (ReleasePlace.h).
    const bool maximized = snapshot.quickTileMode == KWin::QuickTileMode{}
        && snapshot.maximizeMode != KWin::MaximizeRestore;
    const auto *room = client->moveResizeOutput();
    const QRectF work = maximized && room
        ? QRectF(KWin::effects->clientArea(KWin::MaximizeArea, room)) : QRectF();
    const KWin::RectF restoreBeneath = maximized
        ? KWin::RectF(restoreBeneathMaximize(
              preserveRestoreGeometry ? QRectF(snapshot.floatingGeometry) : QRectF(), QRectF(geometry), work))
        : geometry;
    if (!step([&] { if (snapshot.quickTileMode != KWin::QuickTileMode{}) {
        client->setQuickTileMode(snapshot.quickTileMode, geometry.center());
    } else if (snapshot.maximizeMode != KWin::MaximizeRestore) {
        client->maximize(snapshot.maximizeMode, restoreBeneath);
    } })) return false;
    if (!step([&] { if (snapshot.fullScreen) client->setFullScreen(true); })) return false;
    if (!step([&] { if (maximized) {
        client->setGeometryRestore(restoreBeneath);
    } else if (preserveRestoreGeometry && snapshot.floatingGeometry.isValid()) {
        client->setGeometryRestore(snapshot.floatingGeometry);
    } })) return false;
    if (!step([&] { if (preserveRestoreGeometry && snapshot.fullscreenRestoreGeometry.isValid()) {
        client->setFullscreenGeometryRestore(snapshot.fullscreenRestoreGeometry);
    } })) return false;
    return step([&] { if (manageMinimized) client->setMinimized(minimized); });
}

template<class Client, class Snapshot>
void restoreWindowState(Client *client, const Snapshot &snapshot,
                        const KWin::RectF &geometry, bool preserveRestoreGeometry,
                        bool manageMinimized = false, bool minimized = false)
{
    (void)restoreWindowStateChecked(client, snapshot, geometry, preserveRestoreGeometry,
                                   manageMinimized, minimized, [] { return true; });
}
}

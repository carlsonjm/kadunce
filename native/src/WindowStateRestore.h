#pragma once
#include <window.h>

namespace Kadunce {
// Mechanics only: callers retain output choice, focus/stacking order and
// mutation guards. Preserve the tested fullscreen restoration sequence.
template<class Client, class Snapshot>
void restoreWindowState(Client *client, const Snapshot &snapshot,
                        const KWin::RectF &geometry, bool preserveRestoreGeometry,
                        bool manageMinimized = false, bool minimized = false)
{
    if (client->isFullScreen()) client->setFullScreen(false);
    if (client->maximizeMode() != KWin::MaximizeRestore) client->maximize(KWin::MaximizeRestore);
    if (client->quickTileMode() != KWin::QuickTileMode{}) {
        client->setQuickTileMode(KWin::QuickTileMode{}, client->frameGeometry().center());
    }
    if (manageMinimized) client->setMinimized(false);
    client->moveResize(geometry);
    if (snapshot.quickTileMode != KWin::QuickTileMode{}) {
        client->setQuickTileMode(snapshot.quickTileMode, geometry.center());
    } else if (snapshot.maximizeMode != KWin::MaximizeRestore) {
        client->maximize(snapshot.maximizeMode,
            preserveRestoreGeometry && snapshot.floatingGeometry.isValid()
                ? snapshot.floatingGeometry : geometry);
    }
    if (snapshot.fullScreen) client->setFullScreen(true);
    if (preserveRestoreGeometry && snapshot.floatingGeometry.isValid()) {
        client->setGeometryRestore(snapshot.floatingGeometry);
    }
    if (preserveRestoreGeometry && snapshot.fullscreenRestoreGeometry.isValid()) {
        client->setFullscreenGeometryRestore(snapshot.fullscreenRestoreGeometry);
    }
    if (manageMinimized) client->setMinimized(minimized);
}
}

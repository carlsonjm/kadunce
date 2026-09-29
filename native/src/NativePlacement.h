#pragma once
#include <window.h>

namespace Kadunce {
// Every setter may synchronously close a client, remove an output or disable
// the effect. Never issue the next setter without rechecking the owner/handles.
template<class Client, class Output, class Valid>
bool applyNativePlacement(Client *client, Output *output,
                          const KWin::RectF &geometry, Valid valid)
{
    const auto step = [&](auto action) {
        if (!valid()) return false;
        action();
        return valid();
    };
    if (!step([&] { client->sendToOutput(output); })) return false;
    if (!step([&] { client->setMinimized(false); })) return false;
    if (!step([&] { if (client->isFullScreen()) client->setFullScreen(false); })) return false;
    if (!step([&] { if (client->maximizeMode() != KWin::MaximizeRestore) client->maximize(KWin::MaximizeRestore); })) return false;
    if (!step([&] {
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, client->frameGeometry().center());
    })) return false;
    return step([&] { client->moveResize(geometry); });
}
}

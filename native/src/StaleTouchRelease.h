/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <input.h>
#include <input_event.h>
#include <window.h>
#include <QPointer>
#include <chrono>

namespace Kadunce {
// KWin's move filter keeps the first contact of a touch move until a contact
// lifts while that move is still open, not for one move. A move cancelled
// with the finger still down leaves that contact behind, and the next touch
// move waits for it: its motion is ignored and its lift ends nothing.
//
// Before cancelling a touch move, a lift under an id no contact uses passes
// through the filters. KWin's move filter takes it as a lift of some other
// contact and forgets the one it kept; with none kept, it ends the move, which
// has not moved under any touch yet. This filter, after the move filter,
// takes the lift so nothing later sees it, and every Kadunce filter ahead of
// the move filter ignores the id.
class StaleTouchRelease final : public KWin::InputEventFilter {
public:
    static constexpr qint32 Id = -1;
    // Popup, after interactive move/resize in KWin's filter order.
    StaleTouchRelease() : InputEventFilter(KWin::InputFilterOrder::Popup) {
        KWin::input()->installInputEventFilter(this);
        s_installed = this;
    }
    ~StaleTouchRelease() override { if (s_installed == this) s_installed = nullptr; }
    bool touchUp(KWin::TouchUpEvent *event) override { return event->id == Id; }
    // Ends KWin's interactive move or resize of the window, cancelling it as
    // cancelInteractiveMoveResize does. touch says a contact drives it.
    // Without this filter installed the lift is not sent.
    static void cancel(KWin::Window *window, bool touch)
    {
        const QPointer<KWin::Window> guarded(window);
        if (touch && s_installed && KWin::input()) {
            KWin::TouchUpEvent lift{Id, std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())};
            KWin::input()->processFilters(&KWin::InputEventFilter::touchUp, &lift);
        }
        if (guarded && (guarded->isInteractiveMove() || guarded->isInteractiveResize()))
            guarded->cancelInteractiveMoveResize();
    }
private:
    static inline StaleTouchRelease *s_installed = nullptr;
};
} // namespace Kadunce

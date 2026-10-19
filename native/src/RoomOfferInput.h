/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <input.h>
#include <input_event.h>
#include <functional>
#include <utility>

namespace Kadunce {
// CARD-LIFECYCLE.md §11, Connected: the room a side snap left open offers the
// display's other cards (RoomOffer.h). While it is active, a press
// on a picture picks that card, and Escape or a press anywhere else leaves the
// room open as it was. A press inside the room is the offer's, all of it; one
// outside goes on to whatever it was meant for.
class RoomOfferInput final : public KWin::InputEventFilter {
public:
    // The picture under a point, -1 for the room around them, -2 outside it.
    std::function<int(QPointF)> pictureAt;
    std::function<void(int)> hovered;
    std::function<void(int)> picked;
    std::function<void()> dismissed;
    // Whether the room offers cards now; the filter stays installed between.
    bool active = false;
    RoomOfferInput() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        KWin::input()->installInputEventFilter(this);
    }
    bool pointerMotion(KWin::PointerMotionEvent *e) override {
        if (active && hovered && pictureAt) hovered(pictureAt(e->position));
        return false;
    }
    bool pointerButton(KWin::PointerButtonEvent *e) override {
        if (e->state == KWin::PointerButtonState::Released) return std::exchange(m_heldPress, false);
        return active && press(e->position);
    }
    bool touchDown(KWin::TouchDownEvent *e) override {
        m_heldTouch = active && press(e->pos);
        return m_heldTouch;
    }
    bool touchMotion(KWin::TouchMotionEvent *) override { return m_heldTouch; }
    bool touchUp(KWin::TouchUpEvent *) override { return std::exchange(m_heldTouch, false); }
    bool keyboardKey(KWin::KeyboardKeyEvent *e) override {
        if (e->key != Qt::Key_Escape) return false;
        if (e->state == KWin::KeyboardKeyState::Released) return std::exchange(m_heldEscape, false);
        if (!active) return false;
        m_heldEscape = true;
        if (dismissed) dismissed();
        return true;
    }
private:
    bool m_heldPress = false;
    bool m_heldTouch = false;
    bool m_heldEscape = false;
    bool press(QPointF position) {
        const int at = pictureAt ? pictureAt(position) : -2;
        if (at >= 0) { if (picked) picked(at); }
        else if (dismissed) dismissed();
        m_heldPress = at != -2;
        return m_heldPress;
    }
};
}

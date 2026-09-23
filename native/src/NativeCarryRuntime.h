/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "NativeCarryHandoff.h"
#include "NativeMoveObserver.h"
#include "CarryInputRoute.h"
#include <input_event.h>

namespace Kadunce {
// Effect-owned input adapter. Passive until a correlated native move is adopted.
// Lock screen/VT remain ahead of this filter. Other devices and unowned input pass.
class NativeCarryRuntime final : public QObject, public KWin::InputEventFilter {
public:
    NativeMoveObserver observer;
    NativeCarryHandoff handoff;
    CarryInputRoute route;
    std::function<void(KWin::Window *)> adopted;
    std::function<void(QPointF)> moved;
    std::function<void()> ended;
    // Presentation can retire without taking ownership of a new input stream.
    std::function<void()> interrupted;
    // Bounded diagnostics belong to the host; never log individual motion events.
    std::function<void(KWin::Window *, const char *)> observed;
    std::function<bool(const PreparedCarrySource &, QPointF)> entryRequested;
    std::function<void(KWin::Window *)> deferred;
    std::function<void(KWin::Window *, QPointF)> nativeReleased;
    NativeCarryRuntime() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        KWin::input()->installInputEventFilter(this);
        observer.identified = [this](KWin::Window *w, auto ticket, QPointF pos, auto proof) {
            if (observed) observed(w, proof == NativeMoveObserver::Proof::ApplicationRequest
                ? (ticket.owner().kind == CarryDevice::Touch ? "proof-app-touch" : "proof-app-pointer")
                : proof == NativeMoveObserver::Proof::XwaylandRequest
                    ? (ticket.owner().kind == CarryDevice::Touch ? "proof-xwayland-touch" : "proof-xwayland-pointer")
                    : (ticket.owner().kind == CarryDevice::Touch ? "proof-decoration-touch" : "proof-decoration-pointer"));
            if (route.busy()) return;
            if (handoff.deferOrdinary(w)) {
                m_deferred = Deferred{w, ticket, pos};
                if (observed) observed(w, "awaiting-entry");
                if (deferred) deferred(w);
                return;
            }
            adopt(w, ticket, pos);
        };
        observer.streamRemoved = [this](CarryDevice kind, quint64 device) {
            clearDeferred();
            dispatch(route.streamGone(kind, device), route.owner());
        };
    }
    void clearDeferred() {
        if (m_deferred) { m_deferred.reset(); handoff.cancel(); }
    }
    bool deferredFor(const KWin::Window *w) const { return m_deferred && m_deferred->window == w; }
    void adopt(KWin::Window *w, NativeMoveObserver::Candidate ticket, QPointF pos, QPointF pendingMotion = {}) {
            const auto result = handoff.identify(w, ticket.owner(), pos,
                [this, ticket] { return bool(observer.contacts().resolve(ticket)); }, pendingMotion);
            if (result == NativeMoveTakeover::Result::Rejected) {
                if (observed) observed(w, "adoption-rejected");
                return;
            }
            if (!observer.contacts().resolve(ticket)) { cancel(); return; }
            route.acquire(ticket.owner(), result == NativeMoveTakeover::Result::Carrying);
            if (ticket.owner().kind == CarryDevice::Touch)
                KWin::waylandServer()->seat()->notifyTouchCancel();
            if (result == NativeMoveTakeover::Result::Carrying && adopted) adopted(w);
            else cancel();
    }
    void cancel() {
        m_deferred.reset();
        if (interrupted) interrupted();
        route.cancel(); handoff.cancel(); handoff.takeOutcome();
        if (ended) ended();
    }
    bool dispatch(CarryInputRoute::Action action, CarryOwner owner, QPointF pos = {}) {
        using A = CarryInputRoute::Action;
        if (action == A::Move) {
            if (handoff.carry().move(owner, pos)) { if (moved) moved(pos); }
            else cancel();
        } else if (action == A::Release) {
            const auto result = handoff.releaseDrop(owner);
            if (observed) observed(nullptr, result && result->committed ? "drop-committed" : "drop-not-committed");
            if (observed && result && !result->committed && result->refusal) observed(nullptr, result->refusal);
            if (ended) ended();
        } else if (action == A::Cancel) cancel();
        return action != A::Pass;
    }
    bool pointerButton(KWin::PointerButtonEvent *e) override {
        if (m_deferred && e->state == KWin::PointerButtonState::Released
            && m_deferred->ticket.owner() == CarryOwner{CarryDevice::Pointer, observer.deviceToken(e->device), e->nativeButton}
            && nativeReleased) nativeReleased(m_deferred->window, e->position);
        clearDeferred(); // Extra press or final native release ends the reservation.
        if (e->state == KWin::PointerButtonState::Pressed && interrupted) interrupted();
        const CarryOwner owner{CarryDevice::Pointer, observer.deviceToken(e->device), e->nativeButton};
        return dispatch(e->state == KWin::PointerButtonState::Pressed ? route.down(owner) : route.up(owner), owner, e->position);
    }
    bool pointerMotion(KWin::PointerMotionEvent *e) override {
        if (m_deferred && m_deferred->ticket.owner().kind == CarryDevice::Pointer
            && m_deferred->ticket.owner().device == observer.deviceToken(e->device)
            && tryEntry(e->position)) return dispatch(route.motion(route.owner()), route.owner(), e->position);
        auto owner = route.owner(); owner.kind = CarryDevice::Pointer; owner.device = observer.deviceToken(e->device);
        return dispatch(route.motion(owner), owner, e->position);
    }
    bool touchDown(KWin::TouchDownEvent *e) override {
        clearDeferred();
        if (interrupted) interrupted();
        const CarryOwner owner{CarryDevice::Touch, 1, e->id}; return dispatch(route.down(owner), owner, e->pos);
    }
    bool touchMotion(KWin::TouchMotionEvent *e) override {
        if (m_deferred && m_deferred->ticket.owner() == CarryOwner{CarryDevice::Touch, 1, e->id}
            && tryEntry(e->pos)) return dispatch(route.motion(route.owner()), route.owner(), e->pos);
        const CarryOwner owner{CarryDevice::Touch, 1, e->id}; return dispatch(route.motion(owner), owner, e->pos);
    }
    bool touchUp(KWin::TouchUpEvent *e) override {
        if (m_deferred && m_deferred->ticket.owner() == CarryOwner{CarryDevice::Touch, 1, e->id}
            && nativeReleased) nativeReleased(m_deferred->window, m_deferred->lastPosition);
        clearDeferred();
        const CarryOwner owner{CarryDevice::Touch, 1, e->id}; return dispatch(route.up(owner), owner);
    }
    bool touchCancel() override {
        clearDeferred();
        observer.cancelTouch();
        dispatch(route.streamGone(CarryDevice::Touch, 1), route.owner());
        return false;
    }
    bool keyboardKey(KWin::KeyboardKeyEvent *) override { clearDeferred(); return false; }
private:
    struct Deferred { QPointer<KWin::Window> window; NativeMoveObserver::Candidate ticket; QPointF lastPosition; };
    std::optional<Deferred> m_deferred;
    bool tryEntry(QPointF position) {
        if (!m_deferred) return false;
        const auto waiting = *m_deferred;
        const auto *source = handoff.pendingSource();
        if (!waiting.window || !waiting.window->isInteractiveMove()
            || !observer.contacts().resolve(waiting.ticket) || !source) {
            clearDeferred(); return false;
        }
        if (!entryRequested || !entryRequested(*source, position)) {
            m_deferred->lastPosition = position;
            return false;
        }
        m_deferred.reset();
        // This filter runs before native movement consumes the current event.
        // Project its delta once; never write native geometry to catch up.
        adopt(waiting.window, waiting.ticket, position, position - waiting.lastPosition);
        if (!route.busy()) handoff.flushPending();
        return route.busy();
    }
};
}

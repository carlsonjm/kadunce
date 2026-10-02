/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "WorkspaceInputRouter.h"

#include "SpreadLayout.h"

#include <input_event.h>
#include <window.h>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace Kadunce
{

namespace
{
constexpr double SystemEdgeWidth = 36.0;
// Spread's swipe starts at the bezel. Measured on the tablet on 22 September,
// every bezel swipe first registered on the output's last row; the strip stays
// clear of most of the dock, whose touches are its own.
constexpr double BottomBezelWidth = 20.0;
// A sample older than this when the finger lifts means it had stopped, so the
// release is no flick.
constexpr std::chrono::milliseconds BezelStillAfter{80};
constexpr double CardHoldMotion = 12.0;
constexpr int CardHoldDelay = 300;
}

WorkspaceInputRouter::WorkspaceInputRouter(WorkspaceInputTarget *target,
                                           bool ownsSystemEdges)
    : KWin::InputEventFilter(KWin::InputFilterOrder::Effects)
    , m_target(target)
    , m_ownsSystemEdges(ownsSystemEdges)
{
    if (KWin::input()) KWin::input()->installInputEventSpy(&m_touchEvents);
    m_railHoldTimer.setSingleShot(true);
    m_railHoldTimer.setInterval(90);
    QObject::connect(&m_railHoldTimer, &QTimer::timeout, [this] {
        if (!m_railPointer && m_railTouch < 0) return;
        m_railReady = true;
        m_target->updateRailFromInput(m_railPosition);
    });
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(CardHoldDelay);
    QObject::connect(&m_holdTimer, &QTimer::timeout, [this]() {
        if (reconcileNativeInteraction()) return;
        const QPointF delta = holdCurrent() - holdStart();
        if (m_holdSource != HoldSource::None
            && m_target->presentationForInput()
                == WorkspacePresentation::Spread
            && std::hypot(delta.x(), delta.y()) <= CardHoldMotion) {
            m_target->beginCardGrab(holdCurrent());
            if (m_target->cardGrabActiveForInput()) m_stroke = {};
        }
    });
}

bool WorkspaceInputRouter::pointerMotion(KWin::PointerMotionEvent *event)
{
    m_target->pointerMovedForInput(event->position);
    if (m_railPointer) {
        m_railPosition = event->position;
        if (m_railReady) m_target->updateRailFromInput(event->position);
        else if (QLineF(m_railStart,event->position).length() > 12) m_railHoldTimer.stop();
        return true;
    }
    if (reconcileNativeInteraction()) return false;
    if (!m_forwardedPointerButtons.isEmpty()) return false;
    if (!m_pointerPressed && !m_launcherGuestNavigationPointer
        && !m_drainingPointerButtons.isEmpty()) return true;
    if (!m_panelPointerButtons.isEmpty()
        || (!m_pointerPressed && !m_launcherGuestNavigationPointer
            && m_target->isPanelPoint(event->position))) return false;
    if (m_launcherGuestNavigationPointer) {
        const auto delta = event->position - m_guestOutsidePointerStart;
        if (std::hypot(delta.x(), delta.y()) > CardHoldMotion)
            m_guestOutsidePointerMoved = true;
        return true;
    }
    if (m_target->launcherGuestActiveForInput()
        && m_target->launcherGuestContainsForInput(event->position)) {
        return false;
    }
    // A lifted card keeps this one pointer transaction after it crosses the
    // tablet boundary. Ordinary monitor input remains untouched.
    if (m_pointerPressed
        && m_holdSource == HoldSource::Pointer
        && m_target->cardGrabActiveForInput()) {
        m_pointerCurrent = event->position;
        m_target->updateCardGrab(m_pointerCurrent);
        return true;
    }
    // The row, like a lifted card, keeps its one pointer transaction until
    // the button comes up, even over another display.
    if (m_pointerPressed && m_stroke.live
        && m_stroke.source == HoldSource::Pointer) {
        m_pointerCurrent = event->position;
        cancelMovedHold(m_pointerStart, m_pointerCurrent,
                        HoldSource::Pointer);
        updateSpreadStroke(m_pointerCurrent);
        return true;
    }
    if (!m_target->isTabletPoint(event->position)) {
        return false;
    }
    if (m_pointerPressed) {
        m_pointerCurrent = event->position;
        cancelMovedHold(m_pointerStart, m_pointerCurrent,
                        HoldSource::Pointer);
        return true;
    }
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    if (presentation == WorkspacePresentation::Inactive) {
        return false;
    }
    if (presentation == WorkspacePresentation::Spread) {
        return true;
    }
    return m_target->activeSideForPoint(event->position) != 0;
}

bool WorkspaceInputRouter::pointerButton(KWin::PointerButtonEvent *event)
{
    if (event->state == KWin::PointerButtonState::Pressed && m_target->standsAsideForInput()
        && !m_pointerPressed && !m_railPointer) return false;
    if (m_railPointer) {
        const bool commit = event->button == Qt::LeftButton && event->state == KWin::PointerButtonState::Released;
        if (!commit) m_drainingPointerButtons.insert(Qt::LeftButton);
        m_railPointer = false;
        m_railHoldTimer.stop();
        // A click that never moved leaves the split as it was.
        m_target->finishRailFromInput(commit && m_railReady && m_railPosition != m_railStart);
        m_railReady = false;
        if (commit) return true;
    }
    if (event->state == KWin::PointerButtonState::Pressed && event->button == Qt::LeftButton
        && m_railTouch < 0 && m_observedTouchIds.isEmpty() && !m_pointerPressed
        && m_forwardedPointerButtons.isEmpty() && m_drainingPointerButtons.isEmpty()
        && m_panelPointerButtons.isEmpty() && !m_target->nativeWindowInteractionForInput()
        && !m_target->launcherGuestActiveForInput() && !m_target->isPanelPoint(event->position)
        && m_target->beginRailFromInput(event->position)) {
        // A finger rests on a divider before it drags; a mouse press there is
        // already deliberate, and a quick press and drag moved off before any
        // rest ended, so the pointer drags at once.
        m_railPointer = true;
        m_railStart = m_railPosition = event->position;
        m_railReady = true;
        return true;
    }
    if (reconcileNativeInteraction()) {
        if (event->state == KWin::PointerButtonState::Released) {
            m_forwardedPointerButtons.remove(event->button);
            m_panelPointerButtons.remove(event->button);
        }
        return false;
    }
    if (m_drainingPointerButtons.contains(event->button)) {
        if (event->state == KWin::PointerButtonState::Released)
            m_drainingPointerButtons.remove(event->button);
        return true;
    }
    if (!m_forwardedPointerButtons.isEmpty()) {
        if (event->state == KWin::PointerButtonState::Pressed)
            m_forwardedPointerButtons.insert(event->button);
        else
            m_forwardedPointerButtons.remove(event->button);
        return false;
    }
    // A transaction that begins on Plasma's panel stays with Plasma even
    // when it releases over a card. Never steal an already-owned card drag.
    if (!m_panelPointerButtons.isEmpty()
        || (!m_pointerPressed && !m_launcherGuestNavigationPointer
            && event->state == KWin::PointerButtonState::Pressed
            && m_target->isPanelPoint(event->position))) {
        if (event->state == KWin::PointerButtonState::Pressed)
            m_panelPointerButtons.insert(event->button);
        else
            m_panelPointerButtons.remove(event->button);
        return false;
    }
    if (m_launcherGuestNavigationPointer) {
        if (event->button != m_guestPointerButton) {
            if (event->state == KWin::PointerButtonState::Pressed) {
                m_drainingPointerButtons.insert(event->button);
                return true;
            }
            return false;
        }
        if (event->state == KWin::PointerButtonState::Released) {
            m_launcherGuestNavigationPointer = false;
            const auto delta = event->position - m_guestOutsidePointerStart;
            if (!m_guestOutsidePointerMoved && std::hypot(delta.x(), delta.y()) <= CardHoldMotion)
                m_target->dismissLauncherGuestFromInput();
        }
        return true;
    }
    if ((m_touchId >= 0 || !m_launcherGuestNavigationTouchIds.isEmpty()
         || !m_ownedTouchIds.isEmpty())
        && event->state == KWin::PointerButtonState::Pressed
        && m_target->isTabletPoint(event->position)
        && (m_target->presentationForInput() == WorkspacePresentation::Spread
            || m_target->activeSideForPoint(event->position) != 0)
        && !m_target->launcherGuestContainsForInput(event->position)) {
        m_drainingPointerButtons.insert(event->button);
        return true;
    }
    if (m_target->launcherGuestActiveForInput()
        && event->state == KWin::PointerButtonState::Pressed
        && m_target->isTabletPoint(event->position)) {
        if (m_target->launcherGuestContainsForInput(event->position)
            || m_target->inputPanelContainsForInput(event->position)) {
            m_forwardedPointerButtons.insert(event->button);
            return false;
        }
        m_launcherGuestNavigationPointer = true;
        m_guestPointerButton = event->button;
        m_guestOutsidePointerStart = event->position;
        m_guestOutsidePointerMoved = false;
        return true;
    }
    const bool finishingCrossOutputGrab = m_pointerPressed
        && m_holdSource == HoldSource::Pointer
        && m_target->cardGrabActiveForInput()
        && event->state == KWin::PointerButtonState::Released;
    const bool finishingStroke = m_pointerPressed && m_stroke.live
        && m_stroke.source == HoldSource::Pointer
        && event->state == KWin::PointerButtonState::Released;
    if (!m_target->isTabletPoint(event->position)
        && !finishingCrossOutputGrab && !finishingStroke) {
        return false;
    }
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    if (presentation == WorkspacePresentation::Inactive) {
        return false;
    }
    // A native drag may begin on a monitor and release over the tablet.
    // Spread must not consume that foreign release: KWin owns the matching
    // press and needs the release to end its pointer grab.
    if (event->state == KWin::PointerButtonState::Released
        && !m_pointerPressed) {
        return false;
    }
    const bool spread = presentation == WorkspacePresentation::Spread;
    const int activeSide = spread
        ? 0 : m_target->activeSideForPoint(event->position);
    if (event->state == KWin::PointerButtonState::Pressed
        && (!spread && activeSide == 0)) {
        m_forwardedPointerButtons.insert(event->button);
        return false;
    }
    if (event->button != Qt::LeftButton) {
        if (event->state == KWin::PointerButtonState::Pressed) {
            m_drainingPointerButtons.insert(event->button);
            return true;
        }
        return false;
    }
    if (event->state == KWin::PointerButtonState::Pressed) {
        m_pointerPressed = true;
        m_pointerStart = event->position;
        m_pointerCurrent = event->position;
        m_pointerActiveSide = activeSide;
        if (spread) {
            beginSpreadStroke(HoldSource::Pointer, event->position);
        }
    } else if (m_pointerPressed) {
        m_pointerPressed = false; // This release is already being consumed.
        m_pointerCurrent = event->position;
        if (m_holdSource == HoldSource::Pointer
            && m_target->cardGrabActiveForInput()) {
            stopCardHold(HoldSource::Pointer);
            if (!m_target->finishCardGrabOnOutput(m_pointerCurrent)) {
                m_target->finishCardGrab(true);
            }
        } else if (spread && m_stroke.live && m_stroke.source == HoldSource::Pointer) {
            finishSpreadStroke(m_pointerCurrent);
        } else if (spread) {
            finishSpreadGesture(m_pointerStart, m_pointerCurrent);
        } else if (m_pointerActiveSide < 0) {
            m_target->pageLeftFromInput();
        } else if (m_pointerActiveSide > 0) {
            m_target->pageRightFromInput();
        }
        m_pointerPressed = false;
        m_pointerActiveSide = 0;
        stopCardHold(HoldSource::Pointer);
    }
    return true;
}

bool WorkspaceInputRouter::pointerAxis(KWin::PointerAxisEvent *event)
{
    if (reconcileNativeInteraction()) return false;
    if (!m_pointerPressed && m_target->isPanelPoint(event->position)) return false;
    if (m_target->launcherGuestActiveForInput()
        && m_target->launcherGuestContainsForInput(event->position)) {
        return false;
    }
    if (m_target->isTabletPoint(event->position)
        && (m_touchId >= 0 || m_pointerPressed
            || !m_launcherGuestNavigationTouchIds.isEmpty())) return true;
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    if (!m_target->isTabletPoint(event->position)
        || presentation == WorkspacePresentation::Inactive) {
        return false;
    }
    const bool spread = presentation == WorkspacePresentation::Spread;
    const int activeSide = spread
        ? 0 : m_target->activeSideForPoint(event->position);
    if (!spread && activeSide == 0) {
        return false;
    }
    // Spread opened by fingers stays on the card they began on. A scroll that
    // arrives while they are on the glass comes with them, not from a wheel,
    // and taken as the row's it stepped a card on every event.
    if (spread && m_target->scrollFromFingersForInput()) {
        return true;
    }
    const qreal delta = event->deltaV120 != 0
        ? event->deltaV120 : event->delta;
    if (spread && m_target->selectedStackContains(event->position)) {
        m_target->pageStackFromInput(delta > 0.0 ? -1 : 1);
        return true;
    }
    if (delta > 0.0) {
        m_target->pageLeftFromInput();
    } else if (delta < 0.0) {
        m_target->pageRightFromInput();
    }
    return true;
}

void WorkspaceInputRouter::TouchEvents::touchDown(KWin::TouchDownEvent *event)
{
    window = KWin::input() ? KWin::input()->findToplevel(event->pos) : nullptr;
    counted();
}

const KWin::Window *WorkspaceInputRouter::latestTouchWindow() const
{
    return m_touchEvents.window.data();
}

bool WorkspaceInputRouter::keepTouch(bool kept)
{
    if (kept) m_keptTouchEvent = m_touchEvents.count;
    return kept;
}

bool WorkspaceInputRouter::touchDown(KWin::TouchDownEvent *event)
{
    return keepTouch(routeTouchDown(event));
}

bool WorkspaceInputRouter::routeTouchDown(KWin::TouchDownEvent *event)
{
    if (m_target->standsAsideForInput() && m_ownedTouchIds.isEmpty() && m_railTouch < 0) return false;
    m_observedTouchIds.insert(event->id);
    if (m_railTouch >= 0) {
        m_drainingTouchIds.insert(m_railTouch);
        m_drainingTouchIds.insert(event->id);
        m_railTouch = -1;
        m_railHoldTimer.stop(); m_railReady = false;
        m_target->finishRailFromInput(false);
        return true;
    }
    if (m_observedTouchIds.size() == 1 && !m_railPointer && !m_pointerPressed
        && m_forwardedPointerButtons.isEmpty() && m_panelPointerButtons.isEmpty()
        && !m_target->nativeWindowInteractionForInput() && !m_target->launcherGuestActiveForInput()
        && !m_target->isPanelPoint(event->pos) && m_target->beginRailFromInput(event->pos)) {
        m_railTouch = event->id;
        m_railStart = m_railPosition = event->pos;
        m_railReady = false;
        m_railHoldTimer.start();
        return true;
    }
    if (reconcileNativeInteraction()) return false;
    if (m_observedTouchIds.size() > 1) m_bottomCandidateId = -1;
    // Preserve the native bottom-edge swipe in Active/Inactive; only the
    // overview's blanket touch capture needs a panel exclusion, and the bezel,
    // whose swipe opens Spread, has nothing to do in it. Unowned IDs already
    // pass through motion/up, even after they leave the panel.
    if (m_touchId < 0
        && m_target->presentationForInput() == WorkspacePresentation::Spread
        && m_target->isPanelPoint(event->pos)) return false;
    if (!m_target->isTabletPoint(event->pos)) {
        return false;
    }
    if (!m_target->launcherGuestContainsForInput(event->pos)
        && touchModeAt(event->pos) != TouchMode::None
        && (m_pointerPressed || m_launcherGuestNavigationPointer
            || (m_touchId < 0 && !m_ownedTouchIds.isEmpty()))) {
        m_drainingTouchIds.insert(event->id);
        return true;
    }
    if (m_target->launcherGuestActiveForInput()) {
        // A touch on the keys is typing into the guest, not leaving it.
        if (m_target->launcherGuestContainsForInput(event->pos)
            || m_target->inputPanelContainsForInput(event->pos)) {
            m_launcherGuestTouchIds.insert(event->id);
            return false;
        }
        m_launcherGuestNavigationTouchIds.insert(event->id);
        m_guestOutsideTouchStarts.insert(event->id, event->pos);
        if (m_launcherGuestNavigationTouchIds.size() > 1)
            m_guestOutsideMovedTouches.unite(m_launcherGuestNavigationTouchIds);
        return true;
    }
    const TouchMode mode = touchModeAt(event->pos);
    if (mode == TouchMode::BottomEdge && m_target->surfaceOwnsTouchAt(event->pos)) {
        return false;
    }
    if (mode == TouchMode::BottomEdge
        && m_target->presentationForInput() != WorkspacePresentation::Spread
        && m_touchId < 0) {
        if (m_observedTouchIds.size() == 1) {
            m_bottomCandidateId = event->id;
            m_bottomCandidateStart = event->pos;
        }
        // A bottom-edge contact is not yet a gesture. Let the client receive
        // taps and small movements; claim only a deliberate single-finger swipe.
        return false;
    }
    if (mode == TouchMode::None) {
        return false;
    }
    if (m_touchId < 0) {
        m_touchId = event->id;
        m_touchStart = event->pos;
        m_touchCurrent = event->pos;
        m_touchCommitted = false;
        m_touchMode = mode;
        if (mode == TouchMode::Spread) {
            beginSpreadStroke(HoldSource::Touch, event->pos);
        }
    }
    m_ownedTouchIds.insert(event->id);
    return true;
}

bool WorkspaceInputRouter::touchMotion(KWin::TouchMotionEvent *event)
{
    return keepTouch(routeTouchMotion(event));
}

bool WorkspaceInputRouter::routeTouchMotion(KWin::TouchMotionEvent *event)
{
    if (event->id == m_railTouch) {
        m_railPosition = event->pos;
        if (m_railReady) m_target->updateRailFromInput(event->pos);
        else if (QLineF(m_railStart,event->pos).length() > 12) m_railHoldTimer.stop();
        return true;
    }
    const bool native = reconcileNativeInteraction();
    if (m_drainingTouchIds.contains(event->id)) return true;
    if (native) return false;
    if (event->id == m_bottomCandidateId) {
        const QPointF delta = event->pos - m_bottomCandidateStart;
        if (delta.y() > 40.0 || (std::abs(delta.x()) > 40.0
            && std::abs(delta.x()) > std::abs(delta.y()) * 1.2)) {
            m_bottomCandidateId = -1;
            return false;
        }
        if (m_observedTouchIds.size() == 1 && delta.y() < -40.0
            && std::abs(delta.y()) > std::abs(delta.x()) * 1.2) {
            m_bottomCandidateId = -1;
            if (!m_target->cancelForwardedTouchForInput()) return false;
            m_touchId = event->id;
            m_touchStart = m_bottomCandidateStart;
            m_touchCurrent = event->pos;
            m_touchMode = TouchMode::BottomEdge;
            m_touchCommitted = true;
            m_ownedTouchIds.insert(event->id);
            beginBezelSpread(event->pos, event->time);
            return true;
        }
        return false;
    }
    if (m_launcherGuestNavigationTouchIds.contains(event->id)) {
        const auto delta = event->pos - m_guestOutsideTouchStarts.value(event->id);
        if (std::hypot(delta.x(), delta.y()) > CardHoldMotion)
            m_guestOutsideMovedTouches.insert(event->id);
        return true;
    }
    if (m_launcherGuestTouchIds.contains(event->id)) {
        return false;
    }
    if (!m_ownedTouchIds.contains(event->id)) {
        return false;
    }
    if (event->id != m_touchId) {
        return true;
    }
    m_touchCurrent = event->pos;
    if (m_bezelSpread.live) {
        followBezelSpread(event->pos, event->time);
        return true;
    }
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
        m_target->updateCardGrab(m_touchCurrent);
    } else {
        cancelMovedHold(m_touchStart, m_touchCurrent, HoldSource::Touch);
    }
    if (m_target->cardGrabActiveForInput()) {
        return true;
    }
    if (m_touchMode == TouchMode::Spread && m_stroke.live
        && m_stroke.source == HoldSource::Touch) {
        updateSpreadStroke(m_touchCurrent);
    } else if (!m_touchCommitted) {
        updateTouchGesture();
    }
    return true;
}

bool WorkspaceInputRouter::touchUp(KWin::TouchUpEvent *event)
{
    return keepTouch(routeTouchUp(event));
}

bool WorkspaceInputRouter::routeTouchUp(KWin::TouchUpEvent *event)
{
    if (event->id == m_railTouch) {
        m_observedTouchIds.remove(event->id);
        m_railTouch = -1;
        m_railHoldTimer.stop();
        m_target->finishRailFromInput(m_railReady);
        m_railReady = false;
        return true;
    }
    reconcileNativeInteraction();
    m_observedTouchIds.remove(event->id);
    if (m_drainingTouchIds.remove(event->id)) return true;
    if (m_bottomCandidateId == event->id) m_bottomCandidateId = -1;
    if (m_launcherGuestNavigationTouchIds.remove(event->id)) {
        m_guestOutsideTouchStarts.remove(event->id);
        if (!m_guestOutsideMovedTouches.remove(event->id))
            m_target->dismissLauncherGuestFromInput();
        return true;
    }
    if (m_launcherGuestTouchIds.remove(event->id)) {
        return false;
    }
    if (!m_ownedTouchIds.remove(event->id)) {
        return false;
    }
    if (event->id != m_touchId) {
        return true;
    }
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
        stopCardHold(HoldSource::Touch);
        if (!m_target->finishCardGrabOnOutput(m_touchCurrent)) {
            m_target->finishCardGrab(true);
        }
        m_touchCommitted = true;
    } else if (!m_touchCommitted) {
        finishTouchGesture();
    }
    if (m_bezelSpread.live) finishBezelSpread(false, event->time);
    resetTouch();
    return true;
}

bool WorkspaceInputRouter::touchCancel()
{
    if (m_railTouch >= 0) {
        m_railHoldTimer.stop(); m_railReady = false;
        m_observedTouchIds.remove(m_railTouch);
        m_railTouch = -1;
        m_target->finishRailFromInput(false);
    }
    // Cancellation belongs to the touch stream, not the pointer stream.
    // Let it continue to clients if any contact was forwarded to them.
    QSet<qint32> forwarded = m_observedTouchIds;
    forwarded.subtract(m_ownedTouchIds);
    forwarded.subtract(m_launcherGuestNavigationTouchIds);
    forwarded.subtract(m_drainingTouchIds);
    const bool owned = !m_ownedTouchIds.isEmpty()
        || !m_launcherGuestNavigationTouchIds.isEmpty() || !m_drainingTouchIds.isEmpty();
    m_drainingTouchIds.clear();
    m_observedTouchIds.clear();
    m_bottomCandidateId = -1;
    m_launcherGuestTouchIds.clear();
    m_launcherGuestNavigationTouchIds.clear();
    m_guestOutsideTouchStarts.clear();
    m_guestOutsideMovedTouches.clear();
    if (m_stroke.source == HoldSource::Touch) cancelSpreadStroke();
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
            m_target->finishCardGrab(false);
    }
    if (m_bezelSpread.live) finishBezelSpread(true, m_bezelSpread.lastTime);
    m_ownedTouchIds.clear();
    resetTouch();
    return owned && forwarded.isEmpty();
}

bool WorkspaceInputRouter::reconcileNativeInteraction()
{
    if (!m_target->nativeWindowInteractionForInput()) return false;
    m_bottomCandidateId = -1;
    if (m_pointerPressed || m_touchId >= 0 || m_launcherGuestNavigationPointer
        || !m_launcherGuestNavigationTouchIds.isEmpty()
        || m_holdSource != HoldSource::None) {
        cancelWorkspaceInteraction();
    }
    // KWin's native pointer transaction takes priority over a stale card drain.
    // Touch contacts already consumed by us still need their own inert release.
    m_drainingPointerButtons.clear();
    return true;
}

void WorkspaceInputRouter::cancelWorkspaceInteraction()
{
    m_railHoldTimer.stop(); m_railReady = false;
    if (m_railPointer) m_drainingPointerButtons.insert(Qt::LeftButton);
    if (m_railTouch >= 0) m_drainingTouchIds.insert(m_railTouch);
    m_railPointer = false;
    m_railTouch = -1;
    m_target->finishRailFromInput(false);
    const bool rollback = m_holdSource != HoldSource::None
        && m_target->cardGrabActiveForInput();
    m_drainingTouchIds.unite(m_ownedTouchIds);
    m_drainingTouchIds.unite(m_launcherGuestNavigationTouchIds);
    if (m_pointerPressed) m_drainingPointerButtons.insert(Qt::LeftButton);
    if (m_launcherGuestNavigationPointer)
        m_drainingPointerButtons.insert(m_guestPointerButton);
    m_pointerPressed = false;
    m_pointerActiveSide = 0;
    m_launcherGuestNavigationPointer = false;
    m_ownedTouchIds.clear();
    m_launcherGuestNavigationTouchIds.clear();
    m_guestOutsideTouchStarts.clear();
    m_guestOutsideMovedTouches.clear();
    m_bottomCandidateId = -1;
    cancelSpreadStroke();
    // Clear ownership before calling the target, which may reenter lifecycle hooks.
    m_holdTimer.stop();
    m_holdSource = HoldSource::None;
    const auto bezel = m_bezelSpread;
    const qint32 bezelTouch = bezel.live && bezel.asking ? m_touchId : -1;
    resetTouch();
    if (bezelTouch >= 0) {
        // Spread opening under the bezel swipe goes on following its finger.
        m_drainingTouchIds.remove(bezelTouch);
        m_ownedTouchIds.insert(bezelTouch);
        m_touchId = bezelTouch;
        m_touchMode = TouchMode::BottomEdge;
        m_touchCommitted = true;
        m_bezelSpread = bezel;
    }
    if (rollback) m_target->finishCardGrab(false);
    // Forwarded panel, client and Tette contacts retain their original owner.
}

QPointF WorkspaceInputRouter::holdStart() const
{
    return m_holdSource == HoldSource::Pointer
        ? m_pointerStart : m_touchStart;
}

QPointF WorkspaceInputRouter::holdCurrent() const
{
    return m_holdSource == HoldSource::Pointer
        ? m_pointerCurrent : m_touchCurrent;
}

void WorkspaceInputRouter::startCardHold(HoldSource source,
                                         const QPointF &position)
{
    // A second device cannot take over an existing hold/grab transaction.
    if (m_holdSource != HoldSource::None && m_holdSource != source) {
        return;
    }
    const WorkspaceInputGeometry geometry = m_target->geometryForInput();
    // A hold picks up whichever card it is on, not only the centred one.
    if (!geometry.isValid() || !m_target->cardAtForInput(position)) {
        return;
    }
    m_holdSource = source;
    m_holdTimer.start();
}

void WorkspaceInputRouter::cancelMovedHold(const QPointF &start,
                                           const QPointF &current,
                                           HoldSource source)
{
    if (m_holdSource != source || m_target->cardGrabActiveForInput()) {
        return;
    }
    const QPointF delta = current - start;
    if (std::hypot(delta.x(), delta.y()) > CardHoldMotion) {
        stopCardHold(source);
    }
}

void WorkspaceInputRouter::stopCardHold(HoldSource source)
{
    if (m_holdSource == source) {
        m_holdTimer.stop();
        m_holdSource = HoldSource::None;
    }
}

WorkspaceInputRouter::TouchMode
WorkspaceInputRouter::touchModeAt(const QPointF &position) const
{
    const WorkspaceInputGeometry geometry = m_target->geometryForInput();
    if (!geometry.isValid()) {
        return TouchMode::None;
    }
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    // With the keys up, the bottom is theirs.
    const bool atBottom = !m_target->inputPanelContainsForInput(position)
        && position.y() >= geometry.tabletBottomInclusive - BottomBezelWidth;
    const bool atTop = position.y()
        < geometry.tablet.y() + SystemEdgeWidth;
    if (atBottom) {
        return m_ownsSystemEdges
            ? TouchMode::BottomEdge : TouchMode::None;
    }
    if (presentation == WorkspacePresentation::Spread && atTop) {
        return m_ownsSystemEdges
            ? TouchMode::TopEdge : TouchMode::None;
    }
    if (presentation == WorkspacePresentation::Inactive) {
        return TouchMode::None;
    }
    if (presentation == WorkspacePresentation::Spread) {
        return TouchMode::Spread;
    }
    const int side = m_target->activeSideForPoint(position);
    if (side < 0) {
        return TouchMode::ActiveLeft;
    }
    if (side > 0) {
        return TouchMode::ActiveRight;
    }
    return TouchMode::None;
}

void WorkspaceInputRouter::updateTouchGesture()
{
    const QPointF delta = m_touchCurrent - m_touchStart;
    const bool horizontal = std::abs(delta.x()) > std::abs(delta.y()) * 1.2;
    const bool vertical = std::abs(delta.y()) > std::abs(delta.x()) * 1.2;
    const int stackDirection = classifyStackGesture(
        m_touchStart.x(), m_touchStart.y(),
        m_touchCurrent.x(), m_touchCurrent.y(),
        m_touchMode == TouchMode::Spread
            && m_target->selectedStackContains(m_touchStart));
    if (stackDirection != 0) {
        m_target->pageStackFromInput(stackDirection);
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::Spread
               && std::abs(delta.x()) > 58.0 && horizontal) {
        delta.x() < 0.0
            ? m_target->pageRightFromInput()
            : m_target->pageLeftFromInput();
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::ActiveLeft
               && delta.x() > 30.0 && horizontal) {
        m_target->pageLeftFromInput();
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::ActiveRight
               && delta.x() < -30.0 && horizontal) {
        m_target->pageRightFromInput();
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::BottomEdge
               && delta.y() < -40.0 && vertical) {
        m_touchCommitted = true;
        beginBezelSpread(m_touchCurrent, std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()));
    } else if (m_touchMode == TouchMode::TopEdge
               && delta.y() > 40.0 && vertical) {
        if (m_target->presentationForInput()
            == WorkspacePresentation::Spread) {
            m_target->toggleFromInput();
        }
        m_touchCommitted = true;
    }
}

void WorkspaceInputRouter::finishSpreadGesture(const QPointF &start,
                                                 const QPointF &end)
{
    const WorkspaceInputGeometry geometry = m_target->geometryForInput();
    if (!geometry.isValid()) {
        return;
    }
    // A stroke that never moved far enough to choose a direction is a tap.
    const QPointF travel = end - start;
    if (std::hypot(travel.x(), travel.y()) <= CardHoldMotion) {
        m_target->tapSpreadFromInput(end);
        return;
    }
    const int stackDirection = classifyStackGesture(
        start.x(), start.y(), end.x(), end.y(),
        m_target->selectedStackContains(start));
    if (stackDirection != 0) {
        m_target->pageStackFromInput(stackDirection);
        return;
    }
    const SpreadAction action = classifySpreadGesture(
        start.x(), start.y(), end.x(), end.y(),
        geometry.centerCard.x(), geometry.centerRightInclusive);
    if (action == SpreadAction::Previous) {
        m_target->pageLeftFromInput();
    } else if (action == SpreadAction::Next) {
        m_target->pageRightFromInput();
    } else if (action == SpreadAction::Activate) {
        m_target->activateSelectedFromInput();
    }
}

void WorkspaceInputRouter::finishTouchGesture()
{
    if (m_touchMode == TouchMode::Spread && m_stroke.live
        && m_stroke.source == HoldSource::Touch) {
        finishSpreadStroke(m_touchCurrent);
    } else if (m_touchMode == TouchMode::Spread) {
        finishSpreadGesture(m_touchStart, m_touchCurrent);
    } else if (m_touchMode == TouchMode::ActiveLeft) {
        m_target->pageLeftFromInput();
    } else if (m_touchMode == TouchMode::ActiveRight) {
        m_target->pageRightFromInput();
    }
}

double WorkspaceInputRouter::strokeTime()
{
    if (!m_strokeClock.isValid()) m_strokeClock.start();
    return m_strokeClock.nsecsElapsed() / 1.0e6;
}

void WorkspaceInputRouter::beginSpreadStroke(HoldSource source, const QPointF &position)
{
    m_stroke = {};
    m_stroke.live = true;
    m_stroke.source = source;
    m_stroke.start = position;
    m_stroke.velocity.add(strokeTime(), position.x(), position.y());
    // A finger that stops a coasting row has done all it meant to.
    m_stroke.caught = m_target->catchRowFromInput();
    if (!m_stroke.caught && m_target->rowStillForInput()) {
        startCardHold(source, position);
    }
}

void WorkspaceInputRouter::updateSpreadStroke(const QPointF &position)
{
    m_stroke.velocity.add(strokeTime(), position.x(), position.y());
    if (m_stroke.ignored) return;
    const QPointF delta = position - m_stroke.start;
    if (m_stroke.axis == StrokeAxis::Undecided) {
        const bool onCentredStack = !m_stroke.caught && m_target->rowStillForInput()
            && m_target->selectedStackContains(m_stroke.start);
        StrokeAxis axis = lockStroke(delta.x(), delta.y(), onCentredStack,
                                     m_stroke.velocity.speed().x);
        if (axis == StrokeAxis::Undecided) return;
        stopCardHold(m_stroke.source);
        m_stroke.lock = delta;
        bool began = false;
        if (axis == StrokeAxis::Scrub) {
            began = m_target->beginScrubFromInput();
            if (!began) axis = StrokeAxis::Row;
        }
        if (axis == StrokeAxis::Row) began = m_target->beginRowFromInput();
        if (axis == StrokeAxis::Lift) began = m_target->beginLiftFromInput(m_stroke.start);
        m_stroke.axis = axis;
        if (!began) {
            m_stroke.ignored = true;
            return;
        }
    }
    const QPointF travel = delta - m_stroke.lock;
    switch (m_stroke.axis) {
    case StrokeAxis::Row: m_target->updateRowFromInput(travel.x()); break;
    case StrokeAxis::Scrub: m_target->updateScrubFromInput(travel.x()); break;
    case StrokeAxis::Lift: m_target->updateLiftFromInput(travel.y()); break;
    case StrokeAxis::Undecided: break;
    }
}

void WorkspaceInputRouter::finishSpreadStroke(const QPointF &position)
{
    m_stroke.velocity.add(strokeTime(), position.x(), position.y());
    const StrokeVelocity::Speed speed = m_stroke.velocity.speed();
    // Clear first: finishing can close a window, which cancels input.
    const StrokeState stroke = m_stroke;
    m_stroke = {};
    if (stroke.ignored) {
        if (stroke.caught) m_target->settleRowFromInput();
        return;
    }
    switch (stroke.axis) {
    case StrokeAxis::Row: m_target->finishRowFromInput(speed.x); break;
    case StrokeAxis::Scrub: m_target->finishScrubFromInput(); break;
    case StrokeAxis::Lift:
        m_target->finishLiftFromInput(speed.y);
        if (stroke.caught) m_target->settleRowFromInput();
        break;
    case StrokeAxis::Undecided:
        if (stroke.caught) m_target->settleRowFromInput();
        else finishSpreadGesture(stroke.start, position);
        break;
    }
}

void WorkspaceInputRouter::cancelSpreadStroke()
{
    if (!m_stroke.live) return;
    const bool moving = m_stroke.caught || (!m_stroke.ignored
        && m_stroke.axis != StrokeAxis::Undecided);
    m_stroke = {};
    if (moving) m_target->cancelStrokeFromInput();
}

void WorkspaceInputRouter::resetTouch()
{
    if (m_holdSource == HoldSource::Touch) {
        }
    stopCardHold(HoldSource::Touch);
    m_touchId = -1;
    m_touchCommitted = false;
    m_touchMode = TouchMode::None;
    m_touchStart = {};
    m_touchCurrent = {};
    m_bezelSpread = {};
}

void WorkspaceInputRouter::beginBezelSpread(const QPointF &position, std::chrono::microseconds time)
{
    m_bezelSpread = {true, position, position, time, 0.0, true};
    m_target->beginBezelSpreadFromInput();
    m_bezelSpread.asking = false;
}

void WorkspaceInputRouter::followBezelSpread(const QPointF &position, std::chrono::microseconds time)
{
    auto &swipe = m_bezelSpread;
    const double ms = std::chrono::duration<double, std::milli>(time - swipe.lastTime).count();
    if (ms > 0.0) {
        // Smoothed, so one uneven sample neither makes nor breaks a flick.
        const double speed = (swipe.last.y() - position.y()) / ms;
        swipe.speed = swipe.speed * 0.4 + speed * 0.6;
        swipe.last = position;
        swipe.lastTime = time;
    }
    swipe.asking = true;
    m_target->followBezelSpreadFromInput(std::max(0.0, swipe.from.y() - position.y()));
    m_bezelSpread.asking = false;
}

void WorkspaceInputRouter::finishBezelSpread(bool cancelled, std::chrono::microseconds time)
{
    auto &swipe = m_bezelSpread;
    const double speed = time - swipe.lastTime > BezelStillAfter ? 0.0 : swipe.speed;
    const double rise = std::max(0.0, swipe.from.y() - swipe.last.y());
    swipe = {};
    m_target->finishBezelSpreadFromInput(rise, speed, cancelled);
}

} // namespace Kadunce

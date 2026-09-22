/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "WorkspaceInputRouter.h"

#include "SpreadLayout.h"

#include <input_event.h>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace Kadunce
{

namespace
{
constexpr double SystemEdgeWidth = 36.0;
// Spread's swipe starts at the bezel. Measured on the tablet on 22 September,
// every bezel swipe first registered on the output's last row, and a pull on
// the Keyboard handle 60 to 70 above it; the strip stays well clear of both
// the handle and most of the dock, whose touches are their own.
constexpr double BottomBezelWidth = 20.0;
constexpr double CardHoldMotion = 12.0;
constexpr int CardHoldDelay = 300;
constexpr double CardEdgeZoneFraction = 0.08;
constexpr double CardEdgeZoneMinimum = 72.0;
constexpr int CardEdgeDwellDelay = 300;
constexpr int CardEdgeRepeatDelay = 350;
constexpr int CardStackDwellDelay = 350;
constexpr int CardStackInsertionDwellDelay = 300;
constexpr double CardStackIntentDistance = 36.0;
}

WorkspaceInputRouter::WorkspaceInputRouter(WorkspaceInputTarget *target,
                                           bool ownsSystemEdges)
    : KWin::InputEventFilter(KWin::InputFilterOrder::Effects)
    , m_target(target)
    , m_ownsSystemEdges(ownsSystemEdges)
{
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
            stopEdgePaging();
            m_target->beginCardGrab(holdCurrent());
        }
    });

    m_edgePageTimer.setSingleShot(true);
    QObject::connect(&m_edgePageTimer, &QTimer::timeout, [this]() {
        if (reconcileNativeInteraction()) return;
        if (m_edgePageDirection == 0
            || !m_target->cardGrabActiveForInput()
            || m_target->stackPreviewArmedForInput()
            || heldEdgeDirection(holdCurrent()) != m_edgePageDirection) {
            stopEdgePaging();
            return;
        }
        qInfo() << "Kadunce edge-repeat contact" << holdCurrent()
                << "direction" << m_edgePageDirection;
        m_target->pageCardGrab(m_edgePageDirection);
        // Each newly centered card receives a fresh complete dwell.
        m_edgePageTimer.start(m_edgePageDelay);
    });

    m_stackTargetTimer.setSingleShot(true);
    m_stackTargetTimer.setInterval(CardStackDwellDelay);
    QObject::connect(&m_stackTargetTimer, &QTimer::timeout, [this]() {
        if (reconcileNativeInteraction()) return;
        if (m_stackTargetId != 0
            && m_target->cardGrabActiveForInput()
            && m_target->cardStackCandidate() == m_stackTargetId) {
            m_target->setCardStackPreview(m_stackTargetId);
            stopEdgePaging();
            stopStackInsertion();
            m_stackInsertionAnchor = holdCurrent();
        }
    });

    m_stackInsertionTimer.setSingleShot(true);
    QObject::connect(&m_stackInsertionTimer, &QTimer::timeout, [this]() {
        if (reconcileNativeInteraction()) return;
        if (m_stackInsertionDirection == 0
            || !m_target->cardGrabActiveForInput()
            || !m_target->stackPreviewArmedForInput()
            || m_target->stackPreviewTargetForInput() != m_stackTargetId
            || m_target->cardStackCandidate() != m_stackTargetId) {
            stopStackInsertion();
            return;
        }
        const int direction = m_stackInsertionDirection;
        (void)m_target->pageCardStackInsertion(direction);
        // One movement requests one slot. An end seam is not a request
        // to leave the stack or repeat Spread navigation.
        m_stackInsertionAnchor = holdCurrent();
        stopStackInsertion();
    });
}

bool WorkspaceInputRouter::pointerMotion(KWin::PointerMotionEvent *event)
{
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
        if (m_target->isTabletPoint(m_pointerCurrent)) {
            updateEdgePaging(m_pointerCurrent);
            updateStackTarget();
        } else {
            stopEdgePaging();
            stopStackTarget();
        }
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
    if (m_railPointer) {
        const bool commit = event->button == Qt::LeftButton && event->state == KWin::PointerButtonState::Released;
        if (!commit) m_drainingPointerButtons.insert(Qt::LeftButton);
        m_railPointer = false;
        m_railHoldTimer.stop();
        m_target->finishRailFromInput(commit && m_railReady);
        m_railReady = false;
        if (commit) return true;
    }
    if (event->state == KWin::PointerButtonState::Pressed && event->button == Qt::LeftButton
        && m_railTouch < 0 && m_observedTouchIds.isEmpty() && !m_pointerPressed
        && m_forwardedPointerButtons.isEmpty() && m_drainingPointerButtons.isEmpty()
        && m_panelPointerButtons.isEmpty() && !m_target->nativeWindowInteractionForInput()
        && !m_target->launcherGuestActiveForInput() && !m_target->isPanelPoint(event->position)
        && m_target->beginRailFromInput(event->position)) {
        m_railPointer = true;
        m_railStart = m_railPosition = event->position;
        m_railReady = false;
        m_railHoldTimer.start();
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
        if (m_target->launcherGuestContainsForInput(event->position)) {
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
    if (!m_target->isTabletPoint(event->position)
        && !finishingCrossOutputGrab) {
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
            startCardHold(HoldSource::Pointer, event->position);
        }
    } else if (m_pointerPressed) {
        m_pointerPressed = false; // This release is already being consumed.
        m_pointerCurrent = event->position;
        if (m_holdSource == HoldSource::Pointer
            && m_target->cardGrabActiveForInput()) {
            stopEdgePaging();
            m_stackTargetTimer.stop();
            stopStackInsertion();
            stopCardHold(HoldSource::Pointer);
            if (!m_target->finishCardGrabOnOutput(m_pointerCurrent)) {
                m_target->finishCardGrab(true);
            }
            m_stackTargetId = 0;
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

bool WorkspaceInputRouter::touchDown(KWin::TouchDownEvent *event)
{
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
    // overview's blanket touch capture needs a panel exclusion. Unowned IDs
    // already pass through motion/up, even after they leave the panel.
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
        if (m_target->launcherGuestContainsForInput(event->pos)) {
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
            startCardHold(HoldSource::Touch, event->pos);
        }
    }
    m_ownedTouchIds.insert(event->id);
    return true;
}

bool WorkspaceInputRouter::touchMotion(KWin::TouchMotionEvent *event)
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
            if (m_target->presentationForInput() != WorkspacePresentation::Spread)
                m_target->toggleFromInput();
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
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
        m_target->updateCardGrab(m_touchCurrent);
        if (m_target->isTabletPoint(m_touchCurrent)) {
            updateEdgePaging(m_touchCurrent);
            updateStackTarget();
        } else {
            stopEdgePaging();
            stopStackTarget();
        }
    } else {
        cancelMovedHold(m_touchStart, m_touchCurrent, HoldSource::Touch);
    }
    if (!m_touchCommitted && !m_target->cardGrabActiveForInput()) {
        updateTouchGesture();
    }
    return true;
}

bool WorkspaceInputRouter::touchUp(KWin::TouchUpEvent *event)
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
        stopEdgePaging();
        m_stackTargetTimer.stop();
        stopStackInsertion();
        stopCardHold(HoldSource::Touch);
        if (!m_target->finishCardGrabOnOutput(m_touchCurrent)) {
            m_target->finishCardGrab(true);
        }
        m_stackTargetId = 0;
        m_touchCommitted = true;
    } else if (!m_touchCommitted) {
        finishTouchGesture();
    }
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
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
        stopEdgePaging();
        stopStackTarget();
        m_target->finishCardGrab(false);
    }
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
    // Clear ownership before calling the target, which may reenter lifecycle hooks.
    m_holdTimer.stop();
    m_holdSource = HoldSource::None;
    stopEdgePaging();
    stopStackTarget();
    resetTouch();
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
    if (!geometry.isValid()
        || !m_target->centerCardContainsForInput(position)) {
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

int WorkspaceInputRouter::heldEdgeDirection(const QPointF &position) const
{
    const auto geometry = m_target->geometryForInput();
    if (!geometry.isValid() || !geometry.tablet.contains(position)) return 0;
    const double edgeZone = std::max(CardEdgeZoneMinimum,
        geometry.tablet.width() * CardEdgeZoneFraction);
    const int edge = classifyCardEdge(position.x(), geometry.tablet.x(), geometry.tablet.width(), edgeZone);
    if (edge != 0) return edge;
    // An armed insertion owns its fan until the explicit physical edge exit.
    if (m_target->stackPreviewArmedForInput() || !geometry.centerCard.isValid()) return 0;
    return position.x() < geometry.centerCard.left() ? -1
        : position.x() > geometry.centerCard.right() ? 1 : 0;
}

void WorkspaceInputRouter::updateEdgePaging(const QPointF &position)
{
    const int direction = heldEdgeDirection(position);
    const auto geometry = m_target->geometryForInput();
    const double edgeZone = std::max(CardEdgeZoneMinimum,
        geometry.tablet.width() * CardEdgeZoneFraction);
    const bool fast = geometry.isValid() && geometry.tablet.contains(position)
        && classifyCardEdge(position.x(), geometry.tablet.x(), geometry.tablet.width(), edgeZone) != 0;
    const int delay = fast ? CardEdgeRepeatDelay : 500;
    if (direction != 0 && m_target->stackPreviewArmedForInput()) stopStackTarget();
    if (direction == m_edgePageDirection && delay == m_edgePageDelay) {
        return;
    }
    qInfo() << "Kadunce edge-intent contact" << position
            << "from" << m_edgePageDirection << "to" << direction;
    m_edgePageTimer.stop();
    m_edgePageDirection = direction;
    m_edgePageDelay = delay;
    if (direction != 0) {
        m_edgePageTimer.start(fast ? CardEdgeDwellDelay : delay);
    }
}

void WorkspaceInputRouter::updateStackTarget()
{
    if (m_target->stackPreviewArmedForInput()
        && m_stackTargetId == m_target->stackPreviewTargetForInput()
        && m_stackTargetId == m_target->cardStackCandidate()) {
        updateStackInsertion(holdCurrent());
        return;
    }
    const int target = m_edgePageDirection == 0
        ? m_target->cardStackCandidate() : 0;
    if (target == m_stackTargetId) {
        updateStackInsertion(holdCurrent());
        return;
    }
    m_stackTargetTimer.stop();
    stopStackInsertion();
    m_target->clearCardStackPreview();
    m_stackTargetId = target;
    if (target != 0) {
        m_stackTargetTimer.start();
    }
}

void WorkspaceInputRouter::stopStackTarget()
{
    m_stackTargetTimer.stop();
    stopStackInsertion();
    m_stackTargetId = 0;
    m_target->clearCardStackPreview();
}

void WorkspaceInputRouter::updateStackInsertion(const QPointF &position)
{
    if (!m_target->stackPreviewArmedForInput() || m_stackTargetId == 0) {
        stopStackInsertion();
        return;
    }
    const auto delta = position - m_stackInsertionAnchor;
    const int direction = std::abs(delta.x()) >= CardStackIntentDistance
        && std::abs(delta.x()) > std::abs(delta.y()) ? (delta.x() < 0 ? -1 : 1) : 0;
    if (direction == m_stackInsertionDirection) {
        return;
    }
    m_stackInsertionTimer.stop();
    m_stackInsertionDirection = direction;
    if (direction != 0) {
        m_stackInsertionTimer.start(CardStackInsertionDwellDelay);
    }
}

void WorkspaceInputRouter::stopStackInsertion()
{
    m_stackInsertionTimer.stop();
    m_stackInsertionDirection = 0;
}

void WorkspaceInputRouter::stopEdgePaging()
{
    m_edgePageTimer.stop();
    m_edgePageDirection = 0;
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
    const bool atBottom = position.y()
        >= geometry.tabletBottomInclusive - BottomBezelWidth;
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
        if (m_target->presentationForInput()
            != WorkspacePresentation::Spread) {
            m_target->toggleFromInput();
        }
        m_touchCommitted = true;
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
    if (m_touchMode == TouchMode::Spread) {
        finishSpreadGesture(m_touchStart, m_touchCurrent);
    } else if (m_touchMode == TouchMode::ActiveLeft) {
        m_target->pageLeftFromInput();
    } else if (m_touchMode == TouchMode::ActiveRight) {
        m_target->pageRightFromInput();
    }
}

void WorkspaceInputRouter::resetTouch()
{
    if (m_holdSource == HoldSource::Touch) {
        stopEdgePaging();
        stopStackTarget();
    }
    stopCardHold(HoldSource::Touch);
    m_touchId = -1;
    m_touchCommitted = false;
    m_touchMode = TouchMode::None;
    m_touchStart = {};
    m_touchCurrent = {};
}

} // namespace Kadunce

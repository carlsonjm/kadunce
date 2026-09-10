/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "WorkspaceInputRouter.h"

#include "CardLineLayout.h"

#include <input_event.h>

#include <algorithm>
#include <cmath>

namespace Kadunce
{

namespace
{
constexpr double SystemEdgeWidth = 36.0;
constexpr double CardHoldMotion = 12.0;
constexpr int CardHoldDelay = 300;
constexpr double CardEdgeZoneFraction = 0.08;
constexpr double CardEdgeZoneMinimum = 72.0;
constexpr int CardEdgeDwellDelay = 300;
constexpr int CardEdgeRepeatDelay = 350;
constexpr int CardStackDwellDelay = 350;
constexpr int CardStackInsertionDwellDelay = 300;
constexpr int CardStackInsertionRepeatDelay = 350;
}

WorkspaceInputRouter::WorkspaceInputRouter(WorkspaceInputTarget *target,
                                           bool ownsSystemEdges)
    : KWin::InputEventFilter(KWin::InputFilterOrder::Effects)
    , m_target(target)
    , m_ownsSystemEdges(ownsSystemEdges)
{
    m_holdTimer.setSingleShot(true);
    m_holdTimer.setInterval(CardHoldDelay);
    QObject::connect(&m_holdTimer, &QTimer::timeout, [this]() {
        const QPointF delta = holdCurrent() - holdStart();
        if (m_holdSource != HoldSource::None
            && m_target->presentationForInput()
                == WorkspacePresentation::CardLine
            && std::hypot(delta.x(), delta.y()) <= CardHoldMotion) {
            stopEdgePaging();
            m_target->beginCardGrab();
        }
    });

    m_edgePageTimer.setSingleShot(true);
    QObject::connect(&m_edgePageTimer, &QTimer::timeout, [this]() {
        if (m_edgePageDirection == 0
            || !m_target->cardGrabActiveForInput()) {
            return;
        }
        m_target->pageCardGrab(m_edgePageDirection);
        m_edgePageTimer.start(CardEdgeRepeatDelay);
    });

    m_stackTargetTimer.setSingleShot(true);
    m_stackTargetTimer.setInterval(CardStackDwellDelay);
    QObject::connect(&m_stackTargetTimer, &QTimer::timeout, [this]() {
        if (m_stackTargetId != 0
            && m_target->cardGrabActiveForInput()
            && m_target->cardStackCandidate() == m_stackTargetId) {
            m_target->setCardStackPreview(m_stackTargetId);
            updateStackInsertion(holdCurrent());
        }
    });

    m_stackInsertionTimer.setSingleShot(true);
    QObject::connect(&m_stackInsertionTimer, &QTimer::timeout, [this]() {
        if (m_stackInsertionDirection == 0
            || !m_target->stackPreviewArmedForInput()) {
            return;
        }
        const int direction = m_stackInsertionDirection;
        if (m_target->pageCardStackInsertion(direction)) {
            m_stackInsertionTimer.start(CardStackInsertionRepeatDelay);
            return;
        }

        // One extra outward dwell at the first or last seam releases the
        // destination deck without releasing the carried card. Continue the
        // same transaction through the detached Card Line row.
        m_stackTargetTimer.stop();
        m_stackTargetId = 0;
        stopStackInsertion();
        m_target->clearCardStackPreview();
        m_target->pageCardGrab(direction);
        m_edgePageDirection = direction;
        m_edgePageTimer.start(CardEdgeRepeatDelay);
    });
}

bool WorkspaceInputRouter::pointerMotion(KWin::PointerMotionEvent *event)
{
    if (!m_panelPointerButtons.isEmpty()
        || (!m_pointerPressed && !m_launcherGuestNavigationPointer
            && m_target->isPanelPoint(event->position))) return false;
    if (m_launcherGuestNavigationPointer) {
        const auto delta = event->position - m_guestOutsidePointerStart;
        if (std::hypot(delta.x(), delta.y()) > CardHoldMotion)
            m_guestOutsidePointerMoved = true;
        return true;
    }
    if (m_launcherGuestPointerPassthrough) {
        return false;
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
        m_target->updateCardGrabDestination(m_pointerCurrent);
        if (m_target->isTabletPoint(m_pointerCurrent)) {
            m_target->updateCardGrab(
                m_pointerCurrent.x() - m_pointerStart.x());
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
        if (m_holdSource == HoldSource::Pointer
            && m_target->cardGrabActiveForInput()) {
            m_target->updateCardGrab(
                m_pointerCurrent.x() - m_pointerStart.x());
            updateEdgePaging(m_pointerCurrent);
            updateStackTarget();
        } else {
            cancelMovedHold(m_pointerStart, m_pointerCurrent,
                            HoldSource::Pointer);
        }
        return true;
    }
    if (m_pointerPassthrough) {
        return false;
    }
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    if (presentation == WorkspacePresentation::Inactive) {
        return false;
    }
    if (presentation == WorkspacePresentation::CardLine) {
        return true;
    }
    return m_target->activeSideForPoint(event->position) != 0;
}

bool WorkspaceInputRouter::pointerButton(KWin::PointerButtonEvent *event)
{
    // A transaction that begins on Plasma's panel stays with Plasma even
    // when it releases over a card. Never steal an already-owned card drag.
    if (!m_panelPointerButtons.isEmpty()
        || (!m_pointerPressed && !m_launcherGuestNavigationPointer
            && !m_launcherGuestPointerPassthrough && !m_pointerPassthrough
            && event->state == KWin::PointerButtonState::Pressed
            && m_target->isPanelPoint(event->position))) {
        if (event->state == KWin::PointerButtonState::Pressed)
            m_panelPointerButtons.insert(event->button);
        else
            m_panelPointerButtons.remove(event->button);
        return false;
    }
    if (m_launcherGuestNavigationPointer) {
        if (event->state == KWin::PointerButtonState::Released) {
            m_launcherGuestNavigationPointer = false;
            const auto delta = event->position - m_guestOutsidePointerStart;
            if (!m_guestOutsidePointerMoved && std::hypot(delta.x(), delta.y()) <= CardHoldMotion)
                m_target->dismissLauncherGuestFromInput();
        }
        return true;
    }
    if (m_launcherGuestPointerPassthrough) {
        if (event->state == KWin::PointerButtonState::Released) {
            m_launcherGuestPointerPassthrough = false;
        }
        return false;
    }
    if (m_target->launcherGuestActiveForInput()
        && event->state == KWin::PointerButtonState::Pressed) {
        if (m_target->launcherGuestContainsForInput(event->position)) {
            m_launcherGuestPointerPassthrough = true;
            return false;
        }
        m_launcherGuestNavigationPointer = true;
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
    // Card Line must not consume that foreign release: KWin owns the matching
    // press and needs the release to end its pointer grab.
    if (event->state == KWin::PointerButtonState::Released
        && !m_pointerPressed && !m_pointerPassthrough) {
        return false;
    }
    const bool cardLine = presentation == WorkspacePresentation::CardLine;
    const int activeSide = cardLine
        ? 0 : m_target->activeSideForPoint(event->position);
    if (event->state == KWin::PointerButtonState::Pressed
        && (!cardLine && activeSide == 0)) {
        m_pointerPassthrough = true;
        return false;
    }
    if (m_pointerPassthrough) {
        if (event->state == KWin::PointerButtonState::Released) {
            m_pointerPassthrough = false;
        }
        return false;
    }
    if (event->button != Qt::LeftButton) {
        return cardLine || activeSide != 0;
    }
    if (event->state == KWin::PointerButtonState::Pressed) {
        m_pointerPressed = true;
        m_pointerStart = event->position;
        m_pointerCurrent = event->position;
        m_pointerActiveSide = activeSide;
        if (cardLine) {
            startCardHold(HoldSource::Pointer, event->position);
        }
    } else if (m_pointerPressed) {
        m_pointerCurrent = event->position;
        if (m_holdSource == HoldSource::Pointer
            && m_target->cardGrabActiveForInput()) {
            stopEdgePaging();
            m_stackTargetTimer.stop();
            stopStackInsertion();
            if (!m_target->finishCardGrabOnOutput(m_pointerCurrent)) {
                m_target->finishCardGrab(true);
            }
            m_stackTargetId = 0;
        } else if (cardLine) {
            finishCardLineGesture(m_pointerStart, m_pointerCurrent);
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
    if (!m_pointerPressed && m_target->isPanelPoint(event->position)) return false;
    if (m_target->launcherGuestActiveForInput()
        && m_target->launcherGuestContainsForInput(event->position)) {
        return false;
    }
    const WorkspacePresentation presentation =
        m_target->presentationForInput();
    if (!m_target->isTabletPoint(event->position)
        || presentation == WorkspacePresentation::Inactive) {
        return false;
    }
    const bool cardLine = presentation == WorkspacePresentation::CardLine;
    const int activeSide = cardLine
        ? 0 : m_target->activeSideForPoint(event->position);
    if (!cardLine && activeSide == 0) {
        return false;
    }
    const qreal delta = event->deltaV120 != 0
        ? event->deltaV120 : event->delta;
    if (cardLine && m_target->selectedStackContains(event->position)) {
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
    if (m_observedTouchIds.size() > 1) m_bottomCandidateId = -1;
    // Preserve the native bottom-edge swipe in Active/Inactive; only the
    // overview's blanket touch capture needs a panel exclusion. Unowned IDs
    // already pass through motion/up, even after they leave the panel.
    if (m_touchId < 0
        && m_target->presentationForInput() == WorkspacePresentation::CardLine
        && m_target->isPanelPoint(event->pos)) return false;
    if (!m_target->isTabletPoint(event->pos)) {
        return false;
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
    if (mode == TouchMode::BottomEdge
        && m_target->presentationForInput() != WorkspacePresentation::CardLine
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
        if (mode == TouchMode::CardLine) {
            startCardHold(HoldSource::Touch, event->pos);
        }
    }
    m_ownedTouchIds.insert(event->id);
    return true;
}

bool WorkspaceInputRouter::touchMotion(KWin::TouchMotionEvent *event)
{
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
            if (m_target->presentationForInput() != WorkspacePresentation::CardLine)
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
        m_target->updateCardGrab(m_touchCurrent.x() - m_touchStart.x());
        updateEdgePaging(m_touchCurrent);
        updateStackTarget();
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
    m_observedTouchIds.remove(event->id);
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
        m_target->finishCardGrab(true);
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
    m_observedTouchIds.clear();
    m_bottomCandidateId = -1;
    const bool owned = m_touchId >= 0;
    m_launcherGuestTouchIds.clear();
    m_launcherGuestNavigationTouchIds.clear();
    m_guestOutsideTouchStarts.clear();
    m_guestOutsideMovedTouches.clear();
    m_launcherGuestNavigationPointer = false;
    if (m_holdSource == HoldSource::Touch
        && m_target->cardGrabActiveForInput()) {
        stopEdgePaging();
        stopStackTarget();
        m_target->finishCardGrab(false);
    }
    m_ownedTouchIds.clear();
    resetTouch();
    return owned;
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

void WorkspaceInputRouter::updateEdgePaging(const QPointF &position)
{
    // Insertion owns ordinary horizontal travel. The insertion timer
    // explicitly hands the still-grabbed card back to the Card Line after
    // one extra outward dwell at the deck's first or last seam.
    if (m_target->stackPreviewArmedForInput()) {
        stopEdgePaging();
        return;
    }
    const WorkspaceInputGeometry geometry = m_target->geometryForInput();
    if (!geometry.isValid()) {
        stopEdgePaging();
        return;
    }
    const double edgeZone = std::max(
        CardEdgeZoneMinimum,
        geometry.tablet.width() * CardEdgeZoneFraction);
    const int direction = classifyCardEdge(
        position.x(), geometry.tablet.x(), geometry.tablet.width(), edgeZone);
    if (direction == m_edgePageDirection) {
        return;
    }
    m_edgePageTimer.stop();
    m_edgePageDirection = direction;
    if (direction != 0) {
        m_edgePageTimer.start(CardEdgeDwellDelay);
    }
}

void WorkspaceInputRouter::updateStackTarget()
{
    if (m_target->stackPreviewArmedForInput()
        && m_stackTargetId == m_target->stackPreviewTargetForInput()) {
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
    const WorkspaceInputGeometry geometry = m_target->geometryForInput();
    if (!geometry.isValid()) {
        stopStackInsertion();
        return;
    }
    const double leftBoundary =
        geometry.centerCard.x() + geometry.centerCard.width() * 0.34;
    const double rightBoundary =
        geometry.centerCard.x() + geometry.centerCard.width() * 0.66;
    const int direction = position.x() < leftBoundary ? -1
        : position.x() > rightBoundary ? 1 : 0;
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
        >= geometry.tabletBottomInclusive - SystemEdgeWidth;
    const bool atTop = position.y()
        < geometry.tablet.y() + SystemEdgeWidth;
    if (atBottom) {
        return m_ownsSystemEdges
            ? TouchMode::BottomEdge : TouchMode::None;
    }
    if (presentation == WorkspacePresentation::CardLine && atTop) {
        return m_ownsSystemEdges
            ? TouchMode::TopEdge : TouchMode::None;
    }
    if (presentation == WorkspacePresentation::Inactive) {
        return TouchMode::None;
    }
    if (presentation == WorkspacePresentation::CardLine) {
        return TouchMode::CardLine;
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
        m_touchMode == TouchMode::CardLine
            && m_target->selectedStackContains(m_touchStart));
    if (stackDirection != 0) {
        m_target->pageStackFromInput(stackDirection);
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::CardLine
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
            != WorkspacePresentation::CardLine) {
            m_target->toggleFromInput();
        }
        m_touchCommitted = true;
    } else if (m_touchMode == TouchMode::TopEdge
               && delta.y() > 40.0 && vertical) {
        if (m_target->presentationForInput()
            == WorkspacePresentation::CardLine) {
            m_target->toggleFromInput();
        }
        m_touchCommitted = true;
    }
}

void WorkspaceInputRouter::finishCardLineGesture(const QPointF &start,
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
    const CardLineAction action = classifyCardLineGesture(
        start.x(), start.y(), end.x(), end.y(),
        geometry.centerCard.x(), geometry.centerRightInclusive);
    if (action == CardLineAction::Previous) {
        m_target->pageLeftFromInput();
    } else if (action == CardLineAction::Next) {
        m_target->pageRightFromInput();
    } else if (action == CardLineAction::Activate) {
        m_target->activateSelectedFromInput();
    }
}

void WorkspaceInputRouter::finishTouchGesture()
{
    if (m_touchMode == TouchMode::CardLine) {
        finishCardLineGesture(m_touchStart, m_touchCurrent);
    } else if (m_touchMode == TouchMode::ActiveLeft) {
        m_target->pageLeftFromInput();
    } else if (m_touchMode == TouchMode::ActiveRight) {
        m_target->pageRightFromInput();
    }
}

void WorkspaceInputRouter::resetTouch()
{
    stopEdgePaging();
    stopStackTarget();
    stopCardHold(HoldSource::Touch);
    m_touchId = -1;
    m_touchCommitted = false;
    m_touchMode = TouchMode::None;
    m_touchStart = {};
    m_touchCurrent = {};
}

} // namespace Kadunce

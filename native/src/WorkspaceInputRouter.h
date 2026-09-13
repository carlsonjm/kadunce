/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <input.h>

#include <QPointF>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QTimer>

namespace Kadunce
{

enum class WorkspacePresentation {
    Inactive,
    CardLine,
    Active,
};

struct WorkspaceInputGeometry {
    QRectF tablet;
    QRectF centerCard;
    double tabletBottomInclusive = 0.0;
    double centerRightInclusive = 0.0;
    // Dock/work-area depth only broadens the idle swipe's starting band.
    // It does not move the held-card physical bottom departure target.
    double bottomGestureInset = 0.0;

    [[nodiscard]] bool isValid() const
    {
        return tablet.isValid();
    }
};

// The router owns physical gesture transactions. Its target exposes semantic
// workspace operations, so input policy does not depend on Effect internals.
class WorkspaceInputTarget
{
public:
    virtual ~WorkspaceInputTarget() = default;

    [[nodiscard]] virtual WorkspacePresentation presentationForInput() const = 0;
    // KWin owns the complete transaction for an ordinary window move/resize.
    [[nodiscard]] virtual bool nativeWindowInteractionForInput() const = 0;
    [[nodiscard]] virtual WorkspaceInputGeometry geometryForInput() const = 0;
    [[nodiscard]] virtual bool cardGrabActiveForInput() const = 0;
    [[nodiscard]] virtual bool stackPreviewArmedForInput() const = 0;
    [[nodiscard]] virtual int stackPreviewTargetForInput() const = 0;
    [[nodiscard]] virtual bool centerCardContainsForInput(
        const QPointF &position) const = 0;
    [[nodiscard]] virtual bool launcherGuestActiveForInput() const = 0;
    [[nodiscard]] virtual bool launcherGuestContainsForInput(
        const QPointF &position) const = 0;

    [[nodiscard]] virtual bool isTabletPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool isPanelPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool cancelForwardedTouchForInput() = 0;
    [[nodiscard]] virtual int activeSideForPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool selectedStackContains(
        const QPointF &position) const = 0;
    [[nodiscard]] virtual int cardStackCandidate() const = 0;

    virtual void toggleFromInput() = 0;
    virtual void dismissLauncherGuestFromInput() = 0;
    virtual void navigateLauncherGuestFromInput(
        const QPointF &position) = 0;
    virtual void pageLeftFromInput() = 0;
    virtual void pageRightFromInput() = 0;
    virtual void pageStackFromInput(int delta) = 0;
    virtual void activateSelectedFromInput() = 0;
    virtual void beginCardGrab(const QPointF &position) = 0;
    virtual void updateCardGrab(const QPointF &position) = 0;
    virtual void pageCardGrab(int direction) = 0;
    virtual void finishCardGrab(bool commit) = 0;
    [[nodiscard]] virtual bool finishCardGrabOnOutput(
        const QPointF &position) = 0;
    virtual void setCardStackPreview(int destinationId) = 0;
    virtual void clearCardStackPreview() = 0;
    [[nodiscard]] virtual bool pageCardStackInsertion(int direction) = 0;
};

class WorkspaceInputRouter final : public KWin::InputEventFilter
{
public:
    explicit WorkspaceInputRouter(WorkspaceInputTarget *target,
                                  bool ownsSystemEdges = true);

    bool pointerMotion(KWin::PointerMotionEvent *event) override;
    bool pointerButton(KWin::PointerButtonEvent *event) override;
    bool pointerAxis(KWin::PointerAxisEvent *event) override;
    bool touchDown(KWin::TouchDownEvent *event) override;
    bool touchMotion(KWin::TouchMotionEvent *event) override;
    bool touchUp(KWin::TouchUpEvent *event) override;
    bool touchCancel() override;
    // Invalidate actions, but retain consumed contacts until their release.
    void cancelWorkspaceInteraction();

private:
    bool reconcileNativeInteraction();
    enum class TouchMode {
        None,
        CardLine,
        ActiveLeft,
        ActiveRight,
        BottomEdge,
        TopEdge,
    };

    enum class HoldSource {
        None,
        Pointer,
        Touch,
    };

    [[nodiscard]] QPointF holdStart() const;
    [[nodiscard]] QPointF holdCurrent() const;
    void startCardHold(HoldSource source, const QPointF &position);
    void cancelMovedHold(const QPointF &start, const QPointF &current,
                         HoldSource source);
    void stopCardHold(HoldSource source);
    void updateEdgePaging(const QPointF &position);
    void updateStackTarget();
    void stopStackTarget();
    void updateStackInsertion(const QPointF &position);
    [[nodiscard]] int heldEdgeDirection(const QPointF &position) const;
    void stopStackInsertion();
    void stopEdgePaging();
    [[nodiscard]] TouchMode touchModeAt(const QPointF &position) const;
    void updateTouchGesture();
    void finishCardLineGesture(const QPointF &start, const QPointF &end);
    void finishTouchGesture();
    void resetTouch();

    WorkspaceInputTarget *m_target;
    bool m_ownsSystemEdges = true;
    QPointF m_pointerStart;
    QPointF m_pointerCurrent;
    QPointF m_touchStart;
    QPointF m_touchCurrent;
    QSet<qint32> m_ownedTouchIds;
    QSet<qint32> m_drainingTouchIds;
    QSet<Qt::MouseButton> m_drainingPointerButtons;
    Qt::MouseButton m_guestPointerButton = Qt::NoButton;
    QSet<qint32> m_observedTouchIds;
    qint32 m_bottomCandidateId = -1;
    QPointF m_bottomCandidateStart;
    QSet<Qt::MouseButton> m_panelPointerButtons;
    QSet<qint32> m_launcherGuestTouchIds;
    QSet<qint32> m_launcherGuestNavigationTouchIds;
    QHash<qint32, QPointF> m_guestOutsideTouchStarts;
    QSet<qint32> m_guestOutsideMovedTouches;
    QTimer m_holdTimer;
    QTimer m_edgePageTimer;
    QTimer m_stackTargetTimer;
    QTimer m_stackInsertionTimer;
    qint32 m_touchId = -1;
    bool m_pointerPressed = false;
    QSet<Qt::MouseButton> m_forwardedPointerButtons;
    bool m_launcherGuestNavigationPointer = false;
    QPointF m_guestOutsidePointerStart;
    bool m_guestOutsidePointerMoved = false;
    bool m_touchCommitted = false;
    int m_pointerActiveSide = 0;
    int m_edgePageDirection = 0;
    int m_stackTargetId = 0;
    int m_stackInsertionDirection = 0;
    QPointF m_stackInsertionAnchor;
    TouchMode m_touchMode = TouchMode::None;
    HoldSource m_holdSource = HoldSource::None;
};

} // namespace Kadunce

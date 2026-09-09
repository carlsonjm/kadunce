/*
    SPDX-FileCopyrightText: 2026 Warbler Studio contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <input.h>

#include <QPointF>
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
    [[nodiscard]] virtual WorkspaceInputGeometry geometryForInput() const = 0;
    [[nodiscard]] virtual bool cardGrabActiveForInput() const = 0;
    [[nodiscard]] virtual bool stackPreviewArmedForInput() const = 0;
    [[nodiscard]] virtual int stackPreviewTargetForInput() const = 0;
    [[nodiscard]] virtual bool centerCardContainsForInput(
        const QPointF &position) const = 0;

    [[nodiscard]] virtual bool isTabletPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual int activeSideForPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool selectedStackContains(
        const QPointF &position) const = 0;
    [[nodiscard]] virtual int cardStackCandidate() const = 0;

    virtual void toggleFromInput() = 0;
    virtual void pageLeftFromInput() = 0;
    virtual void pageRightFromInput() = 0;
    virtual void pageStackFromInput(int delta) = 0;
    virtual void activateSelectedFromInput() = 0;
    virtual void beginCardGrab() = 0;
    virtual void updateCardGrab(double horizontalDelta) = 0;
    virtual void updateCardGrabDestination(const QPointF &position) = 0;
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

private:
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
    QTimer m_holdTimer;
    QTimer m_edgePageTimer;
    QTimer m_stackTargetTimer;
    QTimer m_stackInsertionTimer;
    qint32 m_touchId = -1;
    bool m_pointerPressed = false;
    bool m_pointerPassthrough = false;
    bool m_touchCommitted = false;
    int m_pointerActiveSide = 0;
    int m_edgePageDirection = 0;
    int m_stackTargetId = 0;
    int m_stackInsertionDirection = 0;
    TouchMode m_touchMode = TouchMode::None;
    HoldSource m_holdSource = HoldSource::None;
};

} // namespace Kadunce

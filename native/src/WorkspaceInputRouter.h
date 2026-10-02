/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "SpreadStroke.h"

#include <input.h>
#include <input_event_spy.h>

#include <QPointF>
#include <QPointer>
#include <QHash>
#include <QElapsedTimer>
#include <QRectF>
#include <QSet>
#include <QTimer>

#include <chrono>
#include <limits>

namespace Kadunce
{

enum class WorkspacePresentation {
    Inactive,
    Spread,
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
    virtual bool beginRailFromInput(QPointF) { return false; }
    virtual void updateRailFromInput(QPointF) {}
    virtual void finishRailFromInput(bool) {}

    [[nodiscard]] virtual WorkspacePresentation presentationForInput() const = 0;
    // KWin owns the complete transaction for an ordinary window move/resize.
    [[nodiscard]] virtual bool nativeWindowInteractionForInput() const = 0;
    [[nodiscard]] virtual WorkspaceInputGeometry geometryForInput() const = 0;
    [[nodiscard]] virtual bool cardGrabActiveForInput() const = 0;
    [[nodiscard]] virtual bool centerCardContainsForInput(
        const QPointF &position) const = 0;
    [[nodiscard]] virtual bool launcherGuestActiveForInput() const = 0;
    [[nodiscard]] virtual bool launcherGuestContainsForInput(
        const QPointF &position) const = 0;

    [[nodiscard]] virtual bool isTabletPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool isPanelPoint(const QPointF &position) const = 0;
    // A surface that is neither an application nor a panel keeps a touch that
    // starts on it, even inside the bottom swipe's starting band. The band
    // reaches above the dock, where such a surface can have a pull of its own.
    [[nodiscard]] virtual bool surfaceOwnsTouchAt(const QPointF &) const { return false; }
    // Every pointer motion, before anything takes it.
    virtual void pointerMovedForInput(const QPointF &) {}
    // The keys serve whatever holds the text focus, so a touch on them is
    // never a touch away from it.
    [[nodiscard]] virtual bool inputPanelContainsForInput(const QPointF &) const { return false; }
    // Nothing on screen is Kadunce's: every new touch and press passes to the
    // desktop untouched, including the bottom edge.
    [[nodiscard]] virtual bool standsAsideForInput() const { return false; }
    [[nodiscard]] virtual bool cancelForwardedTouchForInput() = 0;
    [[nodiscard]] virtual int activeSideForPoint(const QPointF &position) const = 0;
    [[nodiscard]] virtual bool selectedStackContains(
        const QPointF &position) const = 0;
    // Spread's row and the card under one finger. Travel is the finger's
    // distance since its stroke locked; velocity is pixels per millisecond.
    [[nodiscard]] virtual bool rowMovingForInput() const { return false; }
    [[nodiscard]] virtual bool rowStillForInput() const { return true; }
    // A scroll now comes with fingers on the glass: Spread is forming under
    // them, going back short of halfway, or they have not all lifted.
    [[nodiscard]] virtual bool scrollFromFingersForInput() const { return false; }
    virtual bool catchRowFromInput() { return false; }
    virtual void settleRowFromInput() {}
    virtual bool beginRowFromInput() { return false; }
    virtual void updateRowFromInput(double) {}
    virtual void finishRowFromInput(double) {}
    virtual bool beginScrubFromInput() { return false; }
    virtual void updateScrubFromInput(double) {}
    virtual void finishScrubFromInput() {}
    virtual bool beginLiftFromInput(const QPointF &) { return false; }
    virtual void updateLiftFromInput(double) {}
    virtual void finishLiftFromInput(double) {}
    virtual void cancelStrokeFromInput() {}
    // Whether a card in Spread is under `position`, so a hold there picks it up.
    [[nodiscard]] virtual bool cardAtForInput(const QPointF &) const { return false; }
    // A tap in Spread: the card under it opens, or comes forward in its Stack,
    // and empty space goes back to where the person was.
    virtual void tapSpreadFromInput(const QPointF &) {}

    virtual void toggleFromInput() = 0;
    // A swipe up from the bottom bezel opens Spread under the finger: begun
    // once it commits, then told how far the finger has risen since, and on
    // release how fast it was rising, in logical pixels per millisecond. A
    // cancelled swipe lets Spread go back.
    virtual void beginBezelSpreadFromInput() {}
    virtual void followBezelSpreadFromInput(double) {}
    virtual void finishBezelSpreadFromInput(double, double, bool) {}
    virtual void dismissLauncherGuestFromInput() = 0;
    virtual void navigateLauncherGuestFromInput(
        const QPointF &position) = 0;
    virtual void pageLeftFromInput() = 0;
    virtual void pageRightFromInput() = 0;
    virtual void pageStackFromInput(int delta) = 0;
    virtual void activateSelectedFromInput() = 0;
    virtual void beginCardGrab(const QPointF &position) = 0;
    virtual void updateCardGrab(const QPointF &position) = 0;
    virtual void finishCardGrab(bool commit) = 0;
    [[nodiscard]] virtual bool finishCardGrabOnOutput(
        const QPointF &position) = 0;
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
    // The Z13 tablet kit can appear after the effect loads, handing the top and
    // bottom edges from Plasma to this router mid-session. Any interaction in
    // flight belongs to the previous backend and is cancelled rather than split.
    void setOwnsSystemEdges(bool owns) {
        if (m_ownsSystemEdges == owns) return;
        m_ownsSystemEdges = owns;
        cancelWorkspaceInteraction();
    }
    // Whether Kadunce kept the latest touch for itself, so no application
    // received it: the tap that chose a card, a stroke through Spread, a
    // swipe from the bezel. A touch a filter answered before this one, as the
    // lock screen does, was never Kadunce's.
    [[nodiscard]] bool latestTouchKept() const {
        return m_touchEvents.count != 0 && m_keptTouchEvent == m_touchEvents.count;
    }
    // Every touch event so far, whoever received it, how long ago the latest
    // one came, and the window under the latest finger to go down.
    [[nodiscard]] quint64 touchEvents() const { return m_touchEvents.count; }
    [[nodiscard]] const KWin::Window *latestTouchWindow() const;
    [[nodiscard]] qint64 msSinceLatestTouch() const {
        return m_touchEvents.latest.isValid() ? m_touchEvents.latest.elapsed()
                                              : std::numeric_limits<qint64>::max();
    }
    // Native carry now owns this previously forwarded stream, including its up.
    void retireNativePointer(Qt::MouseButton button) { m_forwardedPointerButtons.remove(button); }
    void retireNativeTouch(qint32 id) {
        m_observedTouchIds.remove(id);
        if (m_bottomCandidateId == id) m_bottomCandidateId = -1;
    }

private:
    // Counts every touch event before any filter sees it, so the count also
    // moves for touches Kadunce never receives.
    struct TouchEvents final : KWin::InputEventSpy {
        void touchDown(KWin::TouchDownEvent *event) override;
        void touchMotion(KWin::TouchMotionEvent *) override { counted(); }
        void touchUp(KWin::TouchUpEvent *) override { counted(); }
        void counted() { ++count; latest.start(); }
        quint64 count = 0;
        QElapsedTimer latest;
        QPointer<KWin::Window> window;
    };
    bool routeTouchDown(KWin::TouchDownEvent *event);
    bool routeTouchMotion(KWin::TouchMotionEvent *event);
    bool routeTouchUp(KWin::TouchUpEvent *event);
    bool keepTouch(bool kept);
    bool reconcileNativeInteraction();
    enum class TouchMode {
        None,
        Spread,
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
    [[nodiscard]] TouchMode touchModeAt(const QPointF &position) const;
    void updateTouchGesture();
    void finishSpreadGesture(const QPointF &start, const QPointF &end);
    // One finger or one mouse drag in Spread.
    void beginSpreadStroke(HoldSource source, const QPointF &position);
    void updateSpreadStroke(const QPointF &position);
    void finishSpreadStroke(const QPointF &position);
    void cancelSpreadStroke();
    [[nodiscard]] double strokeTime();
    void finishTouchGesture();
    void resetTouch();
    void beginBezelSpread(const QPointF &position, std::chrono::microseconds time);
    void followBezelSpread(const QPointF &position, std::chrono::microseconds time);
    void finishBezelSpread(bool cancelled, std::chrono::microseconds time);

    WorkspaceInputTarget *m_target;
    bool m_railPointer = false;
    qint32 m_railTouch = -1;
    QTimer m_railHoldTimer;
    bool m_railReady = false;
    QPointF m_railStart;
    QPointF m_railPosition;
    bool m_ownsSystemEdges = true;
    QPointF m_pointerStart;
    QPointF m_pointerCurrent;
    QPointF m_touchStart;
    QPointF m_touchCurrent;
    QSet<qint32> m_ownedTouchIds;
    TouchEvents m_touchEvents;
    quint64 m_keptTouchEvent = 0;
    QSet<qint32> m_drainingTouchIds;
    QSet<Qt::MouseButton> m_drainingPointerButtons;
    Qt::MouseButton m_guestPointerButton = Qt::NoButton;
    QSet<qint32> m_observedTouchIds;
    qint32 m_bottomCandidateId = -1;
    QPointF m_bottomCandidateStart;
    // The committed bezel swipe that is opening Spread: where it committed,
    // and its last sample and upward speed for telling a flick on release.
    struct BezelSpread {
        bool live = false;
        QPointF from;
        QPointF last;
        std::chrono::microseconds lastTime{0};
        double speed = 0.0;
        // Opening Spread cancels workspace input; while this swipe is the one
        // asking, that cancellation leaves it its finger.
        bool asking = false;
    } m_bezelSpread;
    QSet<Qt::MouseButton> m_panelPointerButtons;
    QSet<qint32> m_launcherGuestTouchIds;
    QSet<qint32> m_launcherGuestNavigationTouchIds;
    QHash<qint32, QPointF> m_guestOutsideTouchStarts;
    QSet<qint32> m_guestOutsideMovedTouches;
    QTimer m_holdTimer;
    qint32 m_touchId = -1;
    bool m_pointerPressed = false;
    QSet<Qt::MouseButton> m_forwardedPointerButtons;
    bool m_launcherGuestNavigationPointer = false;
    QPointF m_guestOutsidePointerStart;
    bool m_guestOutsidePointerMoved = false;
    bool m_touchCommitted = false;
    int m_pointerActiveSide = 0;
    TouchMode m_touchMode = TouchMode::None;
    HoldSource m_holdSource = HoldSource::None;
    struct StrokeState {
        bool live = false;
        // It landed on a row that was coasting, so it is not also a tap.
        bool caught = false;
        // It locked onto something that could not be done, and does nothing.
        bool ignored = false;
        HoldSource source = HoldSource::None;
        StrokeAxis axis = StrokeAxis::Undecided;
        QPointF start;
        QPointF lock;
        StrokeVelocity velocity;
    };
    StrokeState m_stroke;
    QElapsedTimer m_strokeClock;
};

} // namespace Kadunce

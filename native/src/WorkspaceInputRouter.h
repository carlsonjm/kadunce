/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "SpreadStroke.h"
#include "TouchEdge.h"

#include <input.h>
#include <input_event_spy.h>

#include <QPointF>
#include <QPointer>
#include <QHash>
#include <QElapsedTimer>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QTimer>

#include <chrono>
#include <limits>
#include <optional>

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
    // The display a touchscreen drives, whose own edges a touch on it starts
    // from; invalid when no touchscreen drives one.
    TouchEdgeDisplay touchscreen;

    [[nodiscard]] TouchEdgeDisplay spreadEdges() const
    {
        return {tablet, tabletBottomInclusive};
    }

    [[nodiscard]] bool isValid() const
    {
        return tablet.isValid();
    }
};

// What an event does to the strokes under way: a contact or button going
// down or up, or neither.
enum class InputReach { Within, Press, Release, Cancel };

// The router owns physical gesture transactions. Its target exposes semantic
// workspace operations, so input policy does not depend on Effect internals.
class WorkspaceInputTarget
{
public:
    virtual ~WorkspaceInputTarget() = default;
    // CARD-LIFECYCLE.md §11, Every display: each event is answered for the
    // cards of one display, the one it begins on; a stroke keeps the display
    // its first contact named until its last lifts. The router names every
    // event before routing it and lets it go after.
    virtual void enterInputScope(const QPointF &, InputReach, qint64) {}
    virtual void leaveInputScope(InputReach, qint64) {}
    virtual bool beginRailFromInput(QPointF) { return false; }
    virtual void updateRailFromInput(QPointF) {}
    virtual void finishRailFromInput(bool) {}

    [[nodiscard]] virtual WorkspacePresentation presentationForInput() const = 0;
    // A press arrives: a chosen card still growing into Active is there now,
    // so the press reaches it as it would a moment later.
    virtual void settleChosenCardForInput() {}
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
    // Whether a touch here would reach an application or panel surface. A
    // window's frame and resize border are KWin's own, and so is bare screen:
    // a contact there is never a client's to cancel, so the bottom swipe has
    // to claim it at once or not at all.
    [[nodiscard]] virtual bool clientReceivesTouchAt(const QPointF &) const { return true; }
    // The contacts down now, as seen ahead of every KWin handler, or nothing
    // where no one watches. A contact this router still holds that is not
    // among them was lifted where the router never saw it.
    [[nodiscard]] virtual std::optional<QSet<qint32>> touchesDownForInput() const { return std::nullopt; }
    // Each contact that reaches the router at all.
    virtual void touchReachedRouterForInput(qint32) {}
    // Whether the router kept a contact as it came down.
    virtual void touchRoutedForInput(qint32, bool) {}
    // Every pointer motion, before anything takes it.
    virtual void pointerMovedForInput(const QPointF &) {}
    // The keys serve whatever holds the text focus, so a touch on them is
    // never a touch away from it.
    [[nodiscard]] virtual bool inputPanelContainsForInput(const QPointF &) const { return false; }
    [[nodiscard]] virtual bool cancelForwardedTouchForInput() = 0;
    [[nodiscard]] virtual int activeSideForPoint(const QPointF &position) const = 0;
    // Off Spread's display, the gap beside a card shown alone that holds a
    // Stack: before (-1) or after (1), and 0 elsewhere.
    [[nodiscard]] virtual int stackSideForPoint(const QPointF &) const { return 0; }
    virtual void stepStackBesideFromInput(const QPointF &, int) {}
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
    // Notes on Spread's cards (INPUT.md § Notes on cards). A press the notes
    // take is theirs through release: it never moves the row, lifts a card or
    // taps one. Held still for a card's hold, it carries a note; `still` says
    // the contact never moved further than a tap may.
    [[nodiscard]] virtual bool pressNotesFromInput(const QPointF &) { return false; }
    virtual void moveNotesFromInput(const QPointF &) {}
    virtual void holdNotesFromInput() {}
    virtual void releaseNotesFromInput(const QPointF &, bool) {}
    virtual void cancelNotesFromInput() {}

    virtual void toggleFromInput() = 0;
    // A swipe up from the bottom bezel opens Spread under the finger: begun
    // once it commits, then told how far the finger has risen since, and on
    // release how fast it was rising, in logical pixels per millisecond. A
    // cancelled swipe lets Spread go back.
    virtual void beginBezelSpreadFromInput() {}
    virtual void followBezelSpreadFromInput(double) {}
    virtual void finishBezelSpreadFromInput(double, double, bool) {}
    virtual void dismissLauncherGuestFromInput() = 0;
    // A still tap outside Search and the keys, where it touched down.
    virtual void tapBesideLauncherGuestFromInput(
        const QPointF &position) = 0;
    virtual void pageLeftFromInput() = 0;
    virtual void pageRightFromInput() = 0;
    virtual void pageStackFromInput(int delta) = 0;
    // INPUT.md § Active card: a swipe in the gap beside the Active card
    // carries its strip under the finger. `side` is the gap, -1 the left;
    // travel is the finger's, rightward positive; speed in pixels per ms.
    virtual void dragActiveSlideFromInput(int side, double travel) = 0;
    virtual void releaseActiveSlideFromInput(int side, double travel, double speed) = 0;
    virtual void activateSelectedFromInput() = 0;
    virtual void beginCardGrab(const QPointF &position) = 0;
    virtual void updateCardGrab(const QPointF &position) = 0;
    virtual void finishCardGrab(bool commit) = 0;
    [[nodiscard]] virtual bool finishCardGrabOnOutput(
        const QPointF &position) = 0;
    // Table, pulled down from the top edge (TABLE.md).
    // While it is open every touch is Table's; a target without it never is.
    [[nodiscard]] virtual bool tableOpenForInput() const { return false; }
    // Above the Active card's top edge there is no application to reach, only
    // the edges of cards hidden behind it; a touch there is kept, unless it
    // lands on a panel.
    [[nodiscard]] virtual bool aboveActiveCardForInput(const QPointF &) const { return false; }
    // A strip a surface along the top edge keeps from Table's pull
    // (TETTEGOUCHE-CONTEXT.md § Top-edge claims): a contact starting there is
    // the surface's, however it moves.
    [[nodiscard]] virtual bool topEdgeClaimedForInput(const QPointF &) const { return false; }
    // Outside every window's frame, where only the invisible resize border of
    // a card or pane reaches.
    [[nodiscard]] virtual bool inCardGapForInput(const QPointF &) const { return false; }
    // Between the panes of a layout the card display presents, where the cards
    // it hides lie behind; a press there is kept so it never calls one.
    [[nodiscard]] virtual bool inPaneGutterForInput(const QPointF &) const { return false; }
    // A finger came down, before KWin's filters look for the window under it.
    virtual void touchBeganForInput() {}
    // One finger touched and lifted again, still and briefly, with no other
    // finger down meanwhile; `window` was under it. Told whoever received
    // the touch.
    virtual void tapEndedForInput(const KWin::Window *) {}
    virtual void beginTableFromInput(const QPointF &) {}
    // A finger, or else a pointer's button, pressed on an open Table.
    virtual void pressTableFromInput(const QPointF &, bool) {}
    // A right-click on an open Table.
    virtual void contextTableFromInput(const QPointF &) {}
    virtual void moveTableFromInput(const QPointF &) {}
    virtual void releaseTableFromInput(const QPointF &) {}
    virtual void cancelTableFromInput() {}
    // A pointer over an open Table with no button held, and its wheel.
    virtual void hoverTableFromInput(const QPointF &) {}
    virtual void wheelTableFromInput(int) {}
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
    // The lock screen takes the releases from here on, so a button held now
    // is never seen to lift: nothing waits for it, and the first click after
    // unlocking is an ordinary one.
    void cancelForLock() {
        cancelWorkspaceInteraction();
        m_drainingPointerButtons.clear();
    }
    // A touchscreen can appear after the effect loads, handing the top and
    // bottom edges from Plasma to this router mid-session. Any interaction in
    // flight belongs to the previous backend and is cancelled rather than split.
    // Whether a contact here would start in the bottom swipe's band.
    [[nodiscard]] bool inBottomBezel(const QPointF &position) const {
        return touchModeAt(position) == TouchMode::BottomEdge;
    }
    [[nodiscard]] bool ownsSystemEdges() const { return m_ownsSystemEdges; }
    // What the router still holds, for the log: a contact it never saw lift
    // would keep every later one from starting a swipe.
    [[nodiscard]] QString heldForLog() const {
        return QStringLiteral("touch %1, %2 down, %3 kept, rail %4, notes %5, table %6")
            .arg(m_touchId).arg(m_observedTouchIds.size()).arg(m_ownedTouchIds.size())
            .arg(m_railTouch).arg(m_notesTouch).arg(m_tableTouch);
    }
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
        void touchMotion(KWin::TouchMotionEvent *event) override;
        void touchUp(KWin::TouchUpEvent *event) override;
        void counted() { ++count; latest.start(); }
        WorkspaceInputTarget *target = nullptr;
        quint64 count = 0;
        QElapsedTimer latest;
        QPointer<KWin::Window> window;
        // The contacts down now, as KWin counts them, and whether the first of
        // them is a tap yet.
        int fingers = 0;
        bool tapping = false;
        QPointF tapStart;
        QElapsedTimer tapTime;
    };
    bool routePointerMotion(KWin::PointerMotionEvent *event);
    bool routePointerButton(KWin::PointerButtonEvent *event);
    bool routePointerAxis(KWin::PointerAxisEvent *event);
    bool routeTouchCancel();
    bool routeTouchDown(KWin::TouchDownEvent *event);
    bool routeTouchMotion(KWin::TouchMotionEvent *event);
    bool routeTouchUp(KWin::TouchUpEvent *event);
    bool keepTouch(bool kept);
    // The one contact the notes on Spread's cards hold, pointer or touch.
    [[nodiscard]] bool notesHeld() const { return m_notesPointer || m_notesTouch >= 0; }
    void moveNotes(const QPointF &position);
    void releaseNotes(const QPointF &position);
    void cancelNotes();
    bool reconcileNativeInteraction();
    void forgetLiftedTouches(qint32 arriving);
    enum class TouchMode {
        None,
        Spread,
        ActiveLeft,
        ActiveRight,
        StackBefore,
        StackAfter,
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
    void followActiveSwipe(std::chrono::microseconds time);
    [[nodiscard]] double activeSwipeSpeed() const;
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
    bool m_notesPointer = false;
    qint32 m_notesTouch = -1;
    QPointF m_notesStart;
    // Where the notes contact was last seen; a touch lifts with no position.
    QPointF m_notesLast;
    bool m_notesMoved = false;
    QTimer m_notesHoldTimer;
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
    // A contact in the top bezel strip reaches the client until it pulls down
    // far enough to be Table's; the contact Table owns from then on.
    qint32 m_topCandidateId = -1;
    QPointF m_topCandidateStart;
    QPointF m_topCandidateLast;
    // A candidate begun above the Active card is kept rather than forwarded.
    bool m_topCandidateOwned = false;
    qint32 m_tableTouch = -1;
    QPointF m_tablePosition;
    // The pointer's own stroke on an open Table, and the wheel travel not yet
    // a whole step.
    bool m_tablePointer = false;
    double m_tableWheel = 0.0;
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
    // A swipe in the Active card's gap, carrying the strip: the last two
    // samples of the finger give its speed when it lets go.
    struct ActiveSwipe {
        bool live = false;
        double lastX = 0.0, previousX = 0.0;
        std::chrono::microseconds lastTime{0}, previousTime{0};
    } m_activeSwipe;
    int m_pointerActiveSide = 0;
    int m_pointerStackSide = 0;
    QPointF m_pointerStackStart;
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

/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WorkspaceInputRouter.h"
#include "DeferredCommandGuard.h"
#include "CardWorkspaceState.h"
#include <input_event.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <optional>

using namespace Kadunce;
struct Target : WorkspaceInputTarget {
    bool guest = false;
    bool nativeInteraction = false;
    QRectF appletPopup;
    QRectF surface;
    QRectF keys;
    WorkspacePresentation presentation = WorkspacePresentation::Spread;
    int cancellations = 0;
    bool canCancel = true;
    int actions = 0;
    int toggles = 0;
    // A bezel swipe opening Spread: how many began and finished, and what the
    // last one reported.
    int bezelBegins = 0;
    int bezelFinishes = 0;
    double bezelRise = -1.0;
    double bezelSpeed = -1.0;
    bool bezelCancelled = false;
    int dismissals = 0;
    QPointF lastGuestTap;
    bool grabbed = false;
    int grabStarts = 0;
    int grabCancels = 0;
    std::function<void()> onActivate;
    std::function<void()> onToggle;
    std::function<void()> onPage;
    WorkspacePresentation presentationForInput() const override { return presentation; }
    bool nativeWindowInteractionForInput() const override { return nativeInteraction; }
    bool cancelForwardedTouchForInput() override { ++cancellations; return canCancel; }
    WorkspaceInputGeometry geometryForInput() const override { return {{0,0,1000,800},{200,100,600,500},799,799}; }
    bool cardGrabActiveForInput() const override { return grabbed; }
    bool centerCardContainsForInput(const QPointF &p) const override { return geometryForInput().centerCard.contains(p); }
    bool launcherGuestActiveForInput() const override { return guest; }
    bool launcherGuestContainsForInput(const QPointF &p) const override { return guest && centerCardContainsForInput(p); }
    bool isTabletPoint(const QPointF &p) const override { return geometryForInput().tablet.contains(p); }
    bool isPanelPoint(const QPointF &p) const override { return QRectF(100,740,800,60).contains(p) || appletPopup.contains(p); }
    bool surfaceOwnsTouchAt(const QPointF &p) const override { return surface.contains(p); }
    // Where KWin keeps a touch from every client: a window's frame, or bare screen.
    QRectF kwinOwn;
    bool clientReceivesTouchAt(const QPointF &p) const override { return !kwinOwn.contains(p); }
    // The contacts down as a witness ahead of every filter sees them.
    std::optional<QSet<qint32>> down;
    std::optional<QSet<qint32>> touchesDownForInput() const override { return down; }
    bool inputPanelContainsForInput(const QPointF &p) const override { return keys.contains(p); }
    int activeSideForPoint(const QPointF &) const override { return 0; }
    bool selectedStackContains(const QPointF &) const override { return false; }
    // The centred card and one beside it; a hold picks up either.
    QRectF sideCard{860,150,140,400};
    int taps = 0;
    QPointF lastTap;
    bool cardAtForInput(const QPointF &p) const override { return centerCardContainsForInput(p) || sideCard.contains(p); }
    // As the effect does: a tap on a card or on empty space ends in activation.
    void tapSpreadFromInput(const QPointF &p) override { ++taps; lastTap = p; activateSelectedFromInput(); }
    void toggleFromInput() override { ++actions; ++toggles; if (onToggle) onToggle(); }
    void beginBezelSpreadFromInput() override { ++actions; ++bezelBegins; }
    std::function<void()> onBezelFollow;
    bool scrollFromFingers = false;
    bool scrollFromFingersForInput() const override { return scrollFromFingers; }
    void followBezelSpreadFromInput(double rise) override { bezelRise = rise; if (onBezelFollow) onBezelFollow(); }
    void finishBezelSpreadFromInput(double rise, double speed, bool cancelled) override {
        ++actions; ++bezelFinishes; bezelRise = rise; bezelSpeed = speed; bezelCancelled = cancelled;
    }
    void dismissLauncherGuestFromInput() override { ++actions; ++dismissals; }
    void tapBesideLauncherGuestFromInput(const QPointF &p) override { ++actions; ++dismissals; lastGuestTap = p; }
    void pageLeftFromInput() override { ++actions; if (onPage) onPage(); }
    void pageRightFromInput() override { ++actions; if (onPage) onPage(); }
    void pageStackFromInput(int) override { ++actions; }
    void activateSelectedFromInput() override { ++actions; if (onActivate) onActivate(); }
    QPointF pickup, carried, dropped;
    int carryMoves = 0, drops = 0;
    bool acceptDrop = false;
    void beginCardGrab(const QPointF &p) override { ++actions; ++grabStarts; grabbed = true; pickup = p; }
    void updateCardGrab(const QPointF &p) override { ++actions; ++carryMoves; carried = p; }
    void finishCardGrab(bool commit) override { ++actions; grabbed = false; if (!commit) ++grabCancels; }
    bool finishCardGrabOnOutput(const QPointF &p) override {
        ++actions; ++drops; dropped = p;
        if (acceptDrop) grabbed = false;
        return acceptDrop;
    }
};
void require(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    {
        DeferredCommandGuard guard;
        const auto canceled = guard.issue();
        int activations = 0;
        QTimer::singleShot(0, &app, [&] { if (guard.accepts(canceled)) ++activations; });
        guard.invalidate();
        QCoreApplication::processEvents();
        require(activations == 0, "Canceled queued activation executed");
        const auto older = guard.issue();
        const auto newest = guard.issue();
        QTimer::singleShot(0, &app, [&] { if (guard.accepts(older)) ++activations; });
        QTimer::singleShot(0, &app, [&] { if (guard.accepts(newest)) ++activations; });
        QCoreApplication::processEvents();
        require(activations == 1, "Queued requests did not keep only the newest activation");
        guard.invalidate();
        require(!guard.accepts(newest), "Finished workspace retained an activation ticket");
    }
    const auto waitForHold = [] {
        QEventLoop loop;
        QTimer::singleShot(400, &loop, &QEventLoop::quit);
        loop.exec();
    };
    // A hold picks up any card, not only the centred one, and carrying it to
    // a side never pages: the row moves under the card in the compositor, on
    // no timer of the router's.
    for (bool touch : {false, true}) {
        Target target;
        WorkspaceInputRouter input(&target);
        const QPointF side(900,300);
        KWin::PointerButtonEvent button{};
        button.position = side; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        KWin::TouchDownEvent down{701,side,{}};
        require(touch ? input.touchDown(&down) : input.pointerButton(&button), "Side pickup escaped");
        waitForHold();
        require(target.grabbed && target.pickup == side, "A hold beside the centre did not pick that card up");
        for (const QPointF p : {QPointF(990,300), QPointF(10,300), QPointF(500,300)}) {
            KWin::TouchMotionEvent tm{701,p,{}};
            KWin::PointerMotionEvent pm{}; pm.position = p;
            require(touch ? input.touchMotion(&tm) : input.pointerMotion(&pm), "Carry escaped");
            waitForHold();
        }
        require(target.carryMoves == 3 && target.actions == 4, "Carrying to an edge paged or dwelt");
        button.position = {500,300}; button.state = KWin::PointerButtonState::Released;
        KWin::TouchUpEvent up{701,{}};
        require(touch ? input.touchUp(&up) : input.pointerButton(&button), "Carry release escaped");
        require(!target.grabbed, "Carry release did not finish the grab");
    }
    // A tap anywhere in Spread is the target's to answer: on a card, beside
    // the centre, or on empty space.
    for (bool touch : {false, true}) {
        for (const QPointF p : {QPointF(500,300), QPointF(900,300), QPointF(100,400)}) {
            Target target;
            WorkspaceInputRouter input(&target);
            KWin::PointerButtonEvent button{};
            button.position = p; button.button = Qt::LeftButton;
            button.state = KWin::PointerButtonState::Pressed;
            KWin::TouchDownEvent down{702,p,{}};
            KWin::TouchUpEvent up{702,{}};
            require(touch ? input.touchDown(&down) : input.pointerButton(&button), "Tap press escaped");
            button.state = KWin::PointerButtonState::Released;
            require(touch ? input.touchUp(&up) : input.pointerButton(&button), "Tap release escaped");
            require(target.taps == 1 && target.lastTap == p && target.grabStarts == 0,
                    "A tap did not reach the target where it landed");
        }
    }
    // Spread has no window menu: a right-click on a card is kept and does
    // nothing, neither a tap nor a hold.
    {
        Target target;
        WorkspaceInputRouter input(&target);
        KWin::PointerButtonEvent button{};
        button.position = {500,300}; button.button = Qt::RightButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(input.pointerButton(&button), "A right-click on a card escaped Spread");
        button.state = KWin::PointerButtonState::Released;
        require(input.pointerButton(&button), "A right-click's release escaped Spread");
        require(target.taps == 0 && target.grabStarts == 0 && target.actions == 0,
                "A right-click in Spread did something");
    }
    for (bool touch : {false, true}) {
        Target target;
        target.acceptDrop = true;
        WorkspaceInputRouter router(&target);
        const QPointF pickup(420,260), destination(1450,920);
        KWin::PointerButtonEvent button{};
        button.position = pickup; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        KWin::TouchDownEvent down{201,pickup,{}};
        require(touch ? router.touchDown(&down) : router.pointerButton(&button),
                "Carry pickup escaped");
        waitForHold();
        require(target.grabbed && target.pickup == pickup,
                "Carry did not receive actual pickup contact");
        KWin::TouchMotionEvent touchMove{201,destination,{}};
        KWin::PointerMotionEvent pointerMove{}; pointerMove.position = destination;
        require(touch ? router.touchMotion(&touchMove) : router.pointerMotion(&pointerMove),
                "Owned carry escaped at output boundary");
        require(target.carried == destination && target.carryMoves == 1,
                "Carry lost an axis, clamped, or double-delivered motion");
        if (touch) {
            KWin::TouchMotionEvent stranger{202,{100,100},{}};
            require(!router.touchMotion(&stranger) && target.carryMoves == 1,
                    "Foreign contact moved held card");
        }
        button.position = destination; button.state = KWin::PointerButtonState::Released;
        KWin::TouchUpEvent up{201,{}};
        require(touch ? router.touchUp(&up) : router.pointerButton(&button),
                "Carry release escaped");
        require(target.drops == 1 && target.dropped == destination && !target.grabbed,
                "Mouse/touch did not use the same output acceptance path");
        require(target.grabCancels == 0, "Accepted carry also canceled");
    }
    {
        Target target; target.presentation = WorkspacePresentation::Active;
        WorkspaceInputRouter router(&target);
        target.onToggle = [&] {
            router.cancelWorkspaceInteraction();
            target.presentation = target.presentation == WorkspacePresentation::Active
                ? WorkspacePresentation::Spread : WorkspacePresentation::Active;
        };
        target.onPage = [&] { router.cancelWorkspaceInteraction(); };
        KWin::TouchDownEvent down{101,{500,790},{}};
        KWin::TouchMotionEvent motion{101,{500,720},{}};
        KWin::TouchUpEvent up{101,{}};
        require(!router.touchDown(&down) && router.touchMotion(&motion),
                "Combined edge takeover failed");
        require(router.touchMotion(&motion) && router.touchUp(&up)
                    && target.bezelBegins == 1 && target.bezelFinishes == 1 && target.toggles == 0,
                "A bezel swipe leaked its release, began Spread twice or toggled it");
        // Spread opens by other means, as three fingers open it, and the
        // router starts afresh in it.
        router.cancelWorkspaceInteraction();
        target.presentation = WorkspacePresentation::Spread;
        down.pos = {500,300}; motion.pos = {350,300};
        require(router.touchDown(&down) && router.touchMotion(&motion),
                "Fresh swipe failed after view-change drain");
        require(router.touchUp(&up), "Paging callback lost its release");
        const int afterPage = target.actions;
        waitForHold();
        require(target.actions == afterPage && target.grabStarts == 0,
                "Paging cancellation left a delayed hold alive");
        down.pos = {500,300};
        require(router.touchDown(&down), "Contact failed after paging cancellation");
        waitForHold();
        require(target.grabbed, "New hold failed after combined transitions");
        router.cancelWorkspaceInteraction(); target.presentation = WorkspacePresentation::Inactive;
        require(router.touchUp(&up) && target.grabCancels == 1,
                "Release after workspace exit failed to drain canceled grab");
    }
    {
        Target target;
        {
            WorkspaceInputRouter router(&target);
            KWin::TouchDownEvent down{102,{500,300},{}};
            require(router.touchDown(&down), "Destruction test hold not started");
        }
        waitForHold();
        require(target.grabStarts == 0, "Destroyed router fired a hold callback");
        // This tests timer destruction only, not KWin delivery after unload.
    }
    for (bool guest : {false, true}) {
        for (auto firstReleased : {Qt::LeftButton, Qt::RightButton}) {
            Target target; target.guest = guest;
            target.presentation = guest ? WorkspacePresentation::Spread : WorkspacePresentation::Active;
            WorkspaceInputRouter router(&target);
            KWin::PointerButtonEvent button{};
            button.position = {500,300}; button.state = KWin::PointerButtonState::Pressed;
            for (auto held : {Qt::LeftButton, Qt::RightButton}) {
                button.button = held;
                require(!router.pointerButton(&button), "Client chord press stolen");
            }
            router.cancelWorkspaceInteraction();
            target.guest = false; target.presentation = WorkspacePresentation::Spread;
            button.position = {50,300}; button.state = KWin::PointerButtonState::Released;
            button.button = firstReleased;
            require(!router.pointerButton(&button), "First client chord release stolen");
            KWin::PointerMotionEvent motion{}; motion.position = button.position;
            require(!router.pointerMotion(&motion), "Remaining client button lost motion ownership");
            button.button = firstReleased == Qt::LeftButton ? Qt::RightButton : Qt::LeftButton;
            require(!router.pointerButton(&button), "Last client chord release stolen");
            button.position = {500,300}; button.button = Qt::LeftButton;
            button.state = KWin::PointerButtonState::Pressed;
            require(router.pointerButton(&button), "Fresh card click failed after client chord");
            button.state = KWin::PointerButtonState::Released;
            const int before = target.actions;
            require(router.pointerButton(&button) && target.actions > before,
                    "Client chord left a stale passthrough");
        }
    }
    {
        Target target; target.guest = true;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent button{};
        button.position = {50,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Outside primary press failed");
        button.button = Qt::RightButton;
        require(router.pointerButton(&button), "Outside secondary press leaked");
        KWin::PointerMotionEvent motion{}; motion.position = {50,400};
        require(router.pointerMotion(&motion), "Outside chord motion escaped");
        button.button = Qt::LeftButton; button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && target.dismissals == 0,
                "Secondary button hid primary movement, causing dismissal");
        target.guest = false; target.presentation = WorkspacePresentation::Inactive;
        button.position = {1200,300}; button.button = Qt::RightButton;
        require(router.pointerButton(&button), "Secondary outside release was not drained");
    }
    for (bool lifted : {false, true}) {
        Target target;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent button{};
        button.position = {500,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Native takeover initial press failed");
        if (lifted) waitForHold();
        target.nativeInteraction = true;
        // A native operation can start between events: the hold timer must
        // not create a card grab before the next pointer motion arrives.
        if (!lifted) waitForHold();
        KWin::PointerMotionEvent motion{}; motion.position = {1200,300};
        require(!router.pointerMotion(&motion), "Native move was intercepted");
        require(target.grabStarts == (lifted ? 1 : 0)
                    && target.grabCancels == (lifted ? 1 : 0) && !target.grabbed,
                "Native takeover left a hold or grab alive");
        target.nativeInteraction = false;
        button.state = KWin::PointerButtonState::Released;
        require(!router.pointerButton(&button), "Native final release was intercepted");
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "New click failed after native takeover");
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button), "New click release failed after takeover");
    }
    for (bool pointerFirst : {false, true}) {
        Target target;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent button{};
        button.position = {500,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        KWin::TouchDownEvent down{97,{500,300},{}};
        KWin::TouchUpEvent up{97,{}};
        if (pointerFirst) require(router.pointerButton(&button), "Primary pointer failed");
        else require(router.touchDown(&down), "Primary touch failed");
        waitForHold();
        const int beforeSecondary = target.actions;
        if (pointerFirst) {
            require(router.touchDown(&down) && router.touchUp(&up), "Secondary touch not drained");
        } else {
            require(router.pointerButton(&button), "Secondary pointer not drained");
            button.state = KWin::PointerButtonState::Released;
            require(router.pointerButton(&button), "Secondary pointer release leaked");
        }
        KWin::PointerAxisEvent wheel{}; wheel.position = {500,300}; wheel.deltaV120 = 120;
        require(router.pointerAxis(&wheel) && target.actions == beforeSecondary && target.grabbed,
                "Secondary input modified the primary grab");
        wheel.position = {1200,300};
        require(!router.pointerAxis(&wheel), "External desktop scrolling was intercepted");
        if (pointerFirst) {
            button.state = KWin::PointerButtonState::Released;
            require(router.pointerButton(&button), "Primary pointer release failed");
        } else require(router.touchUp(&up), "Primary touch release failed");
        require(!target.grabbed && target.grabCancels == 0, "Primary grab did not commit");
    }
    for (bool guest : {false, true}) {
        Target target; target.guest = guest;
        WorkspaceInputRouter router(&target);
        const QPointF point = guest ? QPointF(50,300) : QPointF(500,300);
        target.onActivate = [&router] { router.cancelWorkspaceInteraction(); };
        KWin::TouchDownEvent down{95,point,{}};
        KWin::TouchMotionEvent motion{95,{1200,300},{}};
        KWin::TouchUpEvent up{95,{}};
        require(router.touchDown(&down), "Lifecycle contact not owned");
        router.cancelWorkspaceInteraction();
        router.cancelWorkspaceInteraction();
        const int afterCancel = target.actions;
        target.presentation = WorkspacePresentation::Inactive; target.guest = false;
        waitForHold();
        require(router.touchMotion(&motion) && router.touchUp(&up)
                    && target.actions == afterCancel && target.grabStarts == 0,
                "Interrupted contact leaked release or fired an action");
        require(!router.touchUp(&up), "Drained contact retained ownership");
        target.presentation = WorkspacePresentation::Spread; target.guest = guest;
        KWin::PointerButtonEvent button{};
        button.position = point; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Lifecycle pointer not owned");
        router.cancelWorkspaceInteraction();
        const int pointerCanceled = target.actions;
        button.position = {1200,300};
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && target.actions == pointerCanceled,
                "Interrupted pointer leaked release or committed");
        button.position = point; button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Next pointer sequence failed");
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && target.actions > pointerCanceled,
                "Next pointer action remained canceled");
        button.state = KWin::PointerButtonState::Pressed;
        const int beforeFresh = target.actions;
        require(router.pointerButton(&button), "Post-activation press failed");
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && target.actions > beforeFresh,
                "Synchronous activation cancellation left a stale release drain");
    }
    {
        Target target; target.guest = true;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{96,{500,300},{}};
        KWin::TouchMotionEvent motion{96,{50,300},{}};
        KWin::TouchUpEvent up{96,{}};
        require(!router.touchDown(&down), "Client contact stolen");
        router.cancelWorkspaceInteraction(); target.guest = false;
        require(!router.touchMotion(&motion) && !router.touchUp(&up),
                "Lifecycle cancellation stole a client contact");
        down.pos = {500,300};
        require(router.touchDown(&down), "Grab contact failed");
        waitForHold();
        require(target.grabbed, "Lifecycle grab was not armed");
        router.cancelWorkspaceInteraction(); router.cancelWorkspaceInteraction();
        require(target.grabCancels == 1 && !target.grabbed && router.touchUp(&up),
                "Lifecycle rollback was not exactly once with release drain");
    }
    {
        Target target; target.guest = true;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent button{};
        button.position = {50,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Outside pointer press not owned");
        KWin::TouchDownEvent down{90,{50,300},{}};
        require(router.touchDown(&down) && router.touchCancel(),
                "Guest outside-touch cancellation not owned");
        require(!router.touchCancel(), "Repeated cancellation retained touch ownership");
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && target.dismissals == 1,
                "Touch cancellation destroyed the independent pointer transaction");
        require(router.touchDown(&down), "Fresh guest contact not owned");
        KWin::TouchDownEvent inside{91,{500,300},{}};
        require(!router.touchDown(&inside) && !router.touchCancel(),
                "Mixed cancellation did not reach the launcher-owned contact");
    }
    {
        Target target;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{92,{500,300},{}};
        require(router.touchDown(&down) && router.touchCancel(), "Card cancel not owned");
        waitForHold();
        require(target.grabStarts == 0, "Canceled hold fired later");
        require(router.touchDown(&down), "Fresh card contact failed after cancel");
        waitForHold();
        require(target.grabStarts == 1 && router.touchCancel()
                    && target.grabCancels == 1 && !target.grabbed,
                "Touch grab was not rolled back exactly once");
        router.touchCancel();
        require(target.grabCancels == 1, "Repeated cancel rolled back twice");
    }
    {
        Target target;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent button{};
        button.position = {500,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(router.pointerButton(&button), "Pointer hold not owned");
        KWin::TouchDownEvent down{93,{500,300},{}};
        require(router.touchDown(&down) && router.touchCancel(), "Concurrent touch not canceled");
        waitForHold();
        require(target.grabStarts == 1 && target.grabCancels == 0,
                "Touch stole or canceled the pointer hold");
        button.state = KWin::PointerButtonState::Released;
        require(router.pointerButton(&button) && !target.grabbed,
                "Pointer grab did not finish after touch cancellation");
    }
    // A foreign release must reach its original desktop owner even if KWin
    // has already cleared its interactive-move flag at the display seam.
    for (bool guest : {false, true}) {
        Target target; target.guest = guest;
        WorkspaceInputRouter router(&target);
        KWin::PointerButtonEvent release{};
        release.position = {500,300}; release.button = Qt::LeftButton;
        release.state = KWin::PointerButtonState::Released;
        require(!router.pointerButton(&release) && target.actions == 0,
                "Foreign desktop release activated a tablet card");
    }
    for (bool guest : {false, true}) {
        Target target; target.guest = guest; target.nativeInteraction = true;
        WorkspaceInputRouter router(&target);
        KWin::PointerMotionEvent motion{}; motion.position = {500,300};
        KWin::PointerButtonEvent button{}; button.position = motion.position;
        button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Released;
        KWin::PointerAxisEvent axis{}; axis.position = motion.position; axis.deltaV120 = 120;
        require(!router.pointerMotion(&motion) && !router.pointerButton(&button)
                    && !router.pointerAxis(&axis) && target.actions == 0,
                "Spread stole a native KWin move/resize transaction");
        target.nativeInteraction = false;
        motion.position = {1200,300}; button.position = motion.position;
        button.state = KWin::PointerButtonState::Pressed;
        require(!router.pointerButton(&button) && !router.pointerMotion(&motion),
                "External press became guest dismissal");
        button.state = KWin::PointerButtonState::Released;
        require(!router.pointerButton(&button) && target.actions == 0,
                "External release affected the tablet workspace");
    }
    for (auto presentation : {WorkspacePresentation::Active, WorkspacePresentation::Inactive}) {
        // A bezel swipe first registers on the output's last rows, and opens
        // Spread from the Active card or the desktop.
        Target target; target.presentation = presentation;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{10,{500,795},{}};
        KWin::TouchMotionEvent move{10,{502,787},{}};
        KWin::TouchUpEvent up{10,{}};
        require(!router.touchDown(&down) && !router.touchMotion(&move)
                    && !router.touchUp(&up), "Bottom tap did not reach the panel");
        require(target.actions == 0 && target.cancellations == 0, "Tap triggered or canceled a gesture");
        require(!router.touchDown(&down), "Swipe's initial contact was stolen");
        move.pos = {501,745};
        require(router.touchMotion(&move) && target.bezelBegins == 1 && target.cancellations == 1,
                "Deliberate upward swipe failed to cancel client delivery before takeover");
        require(router.touchMotion(&move) && router.touchUp(&up) && target.bezelBegins == 1
                    && target.bezelFinishes == 1 && target.toggles == 0,
                "Claimed swipe leaked release, began twice or toggled Spread");
        require(!router.touchDown(&down), "Multitouch first contact stolen");
        KWin::TouchDownEvent second{11,{520,797},{}};
        KWin::TouchUpEvent secondUp{11,{}};
        require(!router.touchDown(&second) && !router.touchMotion(&move)
                    && !router.touchUp(&secondUp) && !router.touchUp(&up),
                "Multiple client touches were stolen by bottom-edge candidate");
        require(target.cancellations == 1, "Multitouch canceled the client sequence");
        require(!router.touchDown(&down), "Horizontal gesture contact stolen");
        move.pos = {560,795};
        require(!router.touchMotion(&move), "Horizontal panel drag stolen");
        move.pos = {501,745};
        require(!router.touchMotion(&move) && !router.touchUp(&up),
                "Disqualified horizontal drag became an upward swipe");
        require(target.cancellations == 1, "Disqualified drag canceled client delivery");
        // A rise no application holds has nothing to cancel and is still the swipe.
        target.canCancel = false;
        require(!router.touchDown(&down) && router.touchMotion(&move) && router.touchUp(&up),
                "A rise no application held stayed where it landed");
        require(target.bezelBegins == 2 && target.bezelFinishes == 2,
                "A rise no application held did not open Spread");
        target.canCancel = true;
        require(!router.touchDown(&down), "Cancel test contact stolen");
        router.touchCancel();
        require(!router.touchMotion(&move) && !router.touchUp(&up), "Canceled candidate remained armed");
    }
    for (qint32 next : {21, 20}) {
        // A filter ahead of the router took a contact's release. The router
        // forgets it once the witness shows it lifted, whether the next
        // contact has a new id or the same one, and the swipe still opens Spread.
        Target target; target.presentation = WorkspacePresentation::Active;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent lost{20,{500,795},{}};
        target.down = QSet<qint32>{20};
        require(!router.touchDown(&lost), "The contact whose release goes missing was stolen");
        KWin::TouchDownEvent down{next,{500,795},{}};
        KWin::TouchMotionEvent move{next,{501,745},{}};
        KWin::TouchUpEvent up{next,{}};
        target.down = QSet<qint32>{next};
        require(!router.touchDown(&down) && router.touchMotion(&move) && router.touchUp(&up),
                "A contact whose release went missing kept the next swipe from Spread");
        require(target.bezelBegins == 1 && target.bezelFinishes == 1,
                "A swipe after a missing release did not open Spread");
    }
    for (auto presentation : {WorkspacePresentation::Active, WorkspacePresentation::Inactive}) {
        // A bezel contact on a window's frame, as a Bento pane's can reach,
        // never reaches a client, so there is nothing to cancel: the swipe is
        // claimed at once and still opens Spread. A tap there does nothing.
        Target target; target.presentation = presentation;
        target.kwinOwn = QRectF(0,700,1000,100);
        target.canCancel = false;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{13,{500,795},{}};
        KWin::TouchMotionEvent move{13,{501,745},{}};
        KWin::TouchUpEvent up{13,{}};
        require(router.touchDown(&down) && router.touchUp(&up) && target.actions == 0,
                "A tap on a frame at the bezel did something");
        require(router.touchDown(&down) && router.touchMotion(&move) && target.bezelBegins == 1,
                "A swipe from a frame at the bezel did not open Spread");
        require(router.touchUp(&up) && target.bezelFinishes == 1 && target.cancellations == 0,
                "A swipe from a frame at the bezel cancelled a delivery or did not finish");
        KWin::TouchDownEvent above{14,{500,770},{}};
        KWin::TouchUpEvent aboveUp{14,{}};
        require(!router.touchDown(&above) && !router.touchUp(&aboveUp),
                "A frame above the bezel lost its touch");
    }
    for (auto presentation : {WorkspacePresentation::Active, WorkspacePresentation::Inactive}) {
        // A swipe that starts on the dock, or in the gutter above it, is not an
        // edge: that ground is the dock's and the Keyboard handle's.
        Target target; target.presentation = presentation;
        WorkspaceInputRouter router(&target);
        for (double start : {770.0, 745.0, 735.0}) {
            KWin::TouchDownEvent down{12,{500,start},{}};
            KWin::TouchMotionEvent move{12,{501,start - 90},{}};
            KWin::TouchUpEvent up{12,{}};
            require(!router.touchDown(&down) && !router.touchMotion(&move) && !router.touchUp(&up),
                    "A swipe that did not start at the bezel was taken as the bottom edge");
        }
        require(target.actions == 0 && target.cancellations == 0,
                "A swipe from the dock or the gutter raised the keys");
    }
    for (auto presentation : {WorkspacePresentation::Active, WorkspacePresentation::Spread}) {
        // A handle is a surface of its own and keeps every touch that starts on
        // it, even where it could meet a bottom edge.
        Target target; target.presentation = presentation;
        target.surface = QRectF(400,785,200,10);
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{20,{500,790},{}};
        KWin::TouchMotionEvent move{20,{501,700},{}};
        KWin::TouchUpEvent up{20,{}};
        require(!router.touchDown(&down) && !router.touchMotion(&move) && !router.touchUp(&up),
                "A pull on the handle was taken for the bottom swipe");
        require(target.actions == 0 && target.cancellations == 0,
                "A pull on the handle raised the keys or canceled the handle");
        if (presentation != WorkspacePresentation::Active) continue;
        KWin::TouchDownEvent bezel{21,{500,798},{}};
        KWin::TouchMotionEvent bezelMove{21,{501,700},{}};
        KWin::TouchUpEvent bezelUp{21,{}};
        require(!router.touchDown(&bezel) && router.touchMotion(&bezelMove)
                    && router.touchUp(&bezelUp) && target.bezelBegins == 1,
                "A bezel swipe below a surface stopped opening Spread");
    }
    {
        // A scroll that comes with fingers on the glass never moves Spread's
        // row; a wheel with none on it moves one card.
        Target target; target.presentation = WorkspacePresentation::Spread;
        WorkspaceInputRouter router(&target);
        KWin::PointerAxisEvent scroll{}; scroll.position = {500,300}; scroll.delta = 6;
        target.scrollFromFingers = true;
        for (int i = 0; i < 5; ++i)
            require(router.pointerAxis(&scroll), "A scroll with the fingers reached the application");
        require(target.actions == 0, "A scroll with fingers on the glass moved the row");
        target.scrollFromFingers = false;
        KWin::PointerAxisEvent wheel{}; wheel.position = {500,300}; wheel.deltaV120 = 120;
        require(router.pointerAxis(&wheel) && target.actions == 1, "A wheel stopped moving Spread's row");
    }
    {
        // In Spread the bezel is the band's: a swipe there opens nothing more.
        Target target; target.presentation = WorkspacePresentation::Spread;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{40,{500,795},{}};
        KWin::TouchMotionEvent move{40,{501,700},{}};
        KWin::TouchUpEvent up{40,{}};
        require(!router.touchDown(&down) && !router.touchMotion(&move) && !router.touchUp(&up)
                    && target.actions == 0 && target.cancellations == 0,
                "A bezel swipe in Spread was taken from the band");
    }
    {
        // The swipe reports how far it has risen since it committed, and on
        // release how fast it was still rising: a flick, or a finger that had
        // stopped.
        using namespace std::chrono_literals;
        for (bool still : {false, true}) {
            Target target; target.presentation = WorkspacePresentation::Active;
            WorkspaceInputRouter router(&target);
            KWin::TouchDownEvent down{41,{500,798},std::chrono::microseconds(1000ms)};
            KWin::TouchMotionEvent commit{41,{500,750},std::chrono::microseconds(1050ms)};
            KWin::TouchMotionEvent rise{41,{500,650},std::chrono::microseconds(1100ms)};
            KWin::TouchUpEvent up{41,std::chrono::microseconds(still ? 1300ms : 1110ms)};
            require(!router.touchDown(&down) && router.touchMotion(&commit)
                        && target.bezelBegins == 1 && router.touchMotion(&rise),
                    "A bezel swipe did not commit");
            require(target.bezelRise == 100.0, "The rise is not measured from where the swipe committed");
            require(router.touchUp(&up) && target.bezelFinishes == 1 && !target.bezelCancelled
                        && target.bezelRise == 100.0,
                    "A released bezel swipe did not finish");
            require(still ? target.bezelSpeed == 0.0 : target.bezelSpeed > 1.0,
                    still ? "A finger that had stopped was read as a flick"
                          : "A flick was not read as one");
        }
        {
            // Spread opening cancels workspace input, which must not take the
            // finger from the swipe that is opening it.
            Target target; target.presentation = WorkspacePresentation::Active;
            WorkspaceInputRouter router(&target);
            bool opened = false;
            target.onBezelFollow = [&] {
                if (opened) return;
                opened = true;
                target.presentation = WorkspacePresentation::Spread;
                router.cancelWorkspaceInteraction();
            };
            KWin::TouchDownEvent down{43,{500,798},std::chrono::microseconds(2000ms)};
            KWin::TouchMotionEvent commit{43,{500,750},std::chrono::microseconds(2050ms)};
            KWin::TouchMotionEvent rise{43,{500,720},std::chrono::microseconds(2100ms)};
            KWin::TouchMotionEvent higher{43,{500,690},std::chrono::microseconds(2150ms)};
            KWin::TouchUpEvent up{43,std::chrono::microseconds(2400ms)};
            require(!router.touchDown(&down) && router.touchMotion(&commit) && router.touchMotion(&rise)
                        && opened && router.touchMotion(&higher) && target.bezelRise == 60.0,
                    "Opening Spread took the finger from the bezel swipe opening it");
            require(router.touchUp(&up) && target.bezelFinishes == 1 && !target.bezelCancelled,
                    "The bezel swipe opening Spread lost its release");
            router.cancelWorkspaceInteraction();
            require(target.bezelFinishes == 1, "A finished bezel swipe was finished again");
        }
        Target target; target.presentation = WorkspacePresentation::Active;
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent down{42,{500,798},{}};
        KWin::TouchMotionEvent commit{42,{500,740},{}};
        require(!router.touchDown(&down) && router.touchMotion(&commit), "A bezel swipe did not commit");
        router.touchCancel();
        require(target.bezelFinishes == 1 && target.bezelCancelled, "A cancelled bezel swipe was let open");
    }
    {
        // With the keys up the bottom is theirs, a swipe from the output's
        // last rows included.
        Target target; target.presentation = WorkspacePresentation::Active;
        target.keys = QRectF(10,560,980,240);
        WorkspaceInputRouter router(&target);
        KWin::TouchDownEvent low{30,{500,785},{}};
        KWin::TouchMotionEvent lowMove{30,{540,700},{}};
        KWin::TouchUpEvent lowUp{30,{}};
        require(!router.touchDown(&low) && !router.touchMotion(&lowMove) && !router.touchUp(&lowUp),
                "A slide low on the keys was taken as the bottom edge");
        require(target.actions == 0 && target.cancellations == 0,
                "A slide low on the keys raised them again or canceled them");
        KWin::TouchDownEvent bezel{31,{500,797},{}};
        KWin::TouchMotionEvent bezelMove{31,{501,700},{}};
        KWin::TouchUpEvent bezelUp{31,{}};
        require(!router.touchDown(&bezel) && !router.touchMotion(&bezelMove)
                    && !router.touchUp(&bezelUp) && target.actions == 0,
                "A swipe from the last rows under the keys was taken from them");
    }
    for (bool guest : {false, true}) {
        Target target;
        target.guest = guest;
        WorkspaceInputRouter router(&target);
        KWin::PointerMotionEvent motion{};
        motion.position = {500,770};
        require(!router.pointerMotion(&motion), "Panel hover was intercepted");
        KWin::PointerButtonEvent button{};
        button.position = motion.position;
        button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        require(!router.pointerButton(&button), "Panel press was intercepted");
        motion.position = {500,300};
        require(!router.pointerMotion(&motion), "Panel drag lost ownership over a card");
        button.position = motion.position;
        button.state = KWin::PointerButtonState::Released;
        require(!router.pointerButton(&button), "Panel release was intercepted");
        KWin::PointerAxisEvent axis{};
        axis.position = {500,770}; axis.deltaV120 = 120;
        require(!router.pointerAxis(&axis), "Panel scrolling paged cards");
        KWin::TouchDownEvent down{1,{500,770},{}};
        KWin::TouchMotionEvent move{1,{500,300},{}};
        KWin::TouchUpEvent up{1,{}};
        require(!router.touchDown(&down) && !router.touchMotion(&move)
                    && !router.touchUp(&up), "Panel touch lost ownership");
        require(target.actions == 0, "Panel input activated a card or dismissed Tette");
    }
    {
        // Typing a search: the keys are not outside the launcher, and their
        // touches are theirs. A touch outside both still dismisses it.
        Target guest; guest.guest = true; guest.keys = QRectF(0,620,1000,180);
        WorkspaceInputRouter input(&guest);
        KWin::TouchDownEvent key{43,{50,700},{}};
        KWin::TouchUpEvent keyUp{43,{}};
        require(!input.touchDown(&key) && !input.touchUp(&keyUp) && guest.actions == 0,
                "A touch on the keys dismissed the launcher");
        KWin::PointerButtonEvent click{};
        click.position = {50,700}; click.button = Qt::LeftButton;
        click.state = KWin::PointerButtonState::Pressed;
        require(!input.pointerButton(&click), "A click on the keys was taken from them");
        click.state = KWin::PointerButtonState::Released;
        require(!input.pointerButton(&click) && guest.actions == 0,
                "A click on the keys dismissed the launcher");
        KWin::TouchDownEvent outside{44,{50,300},{}};
        KWin::TouchUpEvent outsideUp{44,{}};
        require(input.touchDown(&outside) && input.touchUp(&outsideUp)
                    && guest.dismissals == 1, "A touch outside the keys stopped dismissing");
    }
    Target target;
    {
        Target guest; guest.guest = true;
        WorkspaceInputRouter input(&guest);
        KWin::TouchDownEvent down{42,{50,300},{}};
        KWin::TouchMotionEvent motion{42,{50,350},{}};
        KWin::TouchUpEvent up{42,{}};
        require(input.touchDown(&down) && guest.actions == 0, "Outside touch acted before release");
        require(input.touchMotion(&motion) && input.touchUp(&up) && guest.actions == 0,
                "Outside swipe became a tap");
        require(input.touchDown(&down) && input.touchUp(&up) && guest.dismissals == 1
                    && guest.lastGuestTap == QPointF(50,300), "Outside tap lost where it landed");
        require(input.touchDown(&down), "Cancel contact missing");
        input.touchCancel();
        require(!input.touchUp(&up) && guest.dismissals == 1, "Canceled touch dismissed guest");
        KWin::PointerButtonEvent pointer{};
        pointer.position = {50,300}; pointer.button = Qt::LeftButton;
        pointer.state = KWin::PointerButtonState::Pressed;
        require(input.pointerButton(&pointer) && guest.dismissals == 1, "Outside mouse press acted early");
        pointer.state = KWin::PointerButtonState::Released;
        require(input.pointerButton(&pointer) && guest.dismissals == 2
                    && guest.lastGuestTap == QPointF(50,300), "Outside mouse click lost where it landed");
        // Moving out and back is still a drag, not a fresh stationary click.
        pointer.state = KWin::PointerButtonState::Pressed;
        require(input.pointerButton(&pointer), "Outside drag press escaped");
        KWin::PointerMotionEvent drag{}; drag.position = {50,400};
        require(input.pointerMotion(&drag), "Outside drag lost ownership");
        drag.position = pointer.position;
        require(input.pointerMotion(&drag), "Returning outside drag lost ownership");
        pointer.state = KWin::PointerButtonState::Released;
        require(input.pointerButton(&pointer) && guest.dismissals == 2,
                "Returning outside drag became a dismissal");
        pointer.state = KWin::PointerButtonState::Pressed;
        require(input.pointerButton(&pointer), "Next outside click was blocked");
        pointer.state = KWin::PointerButtonState::Released;
        require(input.pointerButton(&pointer) && guest.dismissals == 3,
                "Completed outside drag left stale input state");

        // A gesture begun inside the launcher remains Tette's, even outside
        // its surface: Kadunce must not reinterpret the release as dismissal.
        pointer.position = {500,300};
        pointer.state = KWin::PointerButtonState::Pressed;
        require(!input.pointerButton(&pointer), "Tette press was captured");
        drag.position = {50,300};
        require(!input.pointerMotion(&drag), "Tette drag was captured outside its surface");
        pointer.position = drag.position;
        pointer.state = KWin::PointerButtonState::Released;
        require(!input.pointerButton(&pointer) && guest.dismissals == 3,
                "Tette-owned release became outside dismissal");
    }
    {
        Target held;
        WorkspaceInputRouter input(&held);
        KWin::TouchDownEvent down{}; down.id = 95; down.pos = {500,300};
        require(input.touchDown(&down), "Safety test hold escaped");
        waitForHold();
        require(held.grabbed, "Safety test card did not lift");
        KWin::PointerButtonEvent click{}; click.button = Qt::LeftButton;
        click.position = {500,770}; click.state = KWin::PointerButtonState::Pressed;
        require(!input.pointerButton(&click), "Held touch blocked dock press");
        click.state = KWin::PointerButtonState::Released;
        require(!input.pointerButton(&click), "Held touch blocked dock release");
        // Plasma opens a distinct applet surface above the panel.
        held.appletPopup = {650,450,300,250};
        KWin::PointerMotionEvent hover{}; hover.position = {700,500};
        require(!input.pointerMotion(&hover), "Held touch blocked tray popup hover");
        click.position = hover.position; click.state = KWin::PointerButtonState::Pressed;
        require(!input.pointerButton(&click), "Held touch blocked tray popup press");
        click.state = KWin::PointerButtonState::Released;
        require(!input.pointerButton(&click), "Held touch blocked tray popup release");
        require(held.grabbed && held.grabCancels == 0, "Tray input committed the held card");
        // The delivered Disable action cancels before the outstanding finger up.
        input.cancelWorkspaceInteraction();
        require(!held.grabbed && held.grabCancels == 1, "Disable did not cancel held card exactly once");
        const int afterDisable = held.actions;
        KWin::TouchUpEvent up{}; up.id = down.id;
        require(input.touchUp(&up) && held.actions == afterDisable,
                "Finger release after tray disable activated a card");
    }
    WorkspaceInputRouter router(&target);
    KWin::PointerButtonEvent button{};
    button.position = {500,300}; button.button = Qt::LeftButton;
    button.state = KWin::PointerButtonState::Pressed;
    require(router.pointerButton(&button), "Card press no longer captured");
    KWin::PointerMotionEvent move{}; move.position = {500,770};
    require(router.pointerMotion(&move), "Owned card gesture leaked into panel");
    button.position = move.position; button.state = KWin::PointerButtonState::Released;
    require(router.pointerButton(&button), "Owned card release leaked into panel");
    button.position = {500,300}; button.state = KWin::PointerButtonState::Pressed;
    require(router.pointerButton(&button), "Second card press not captured");
    button.state = KWin::PointerButtonState::Released;
    const int before = target.actions;
    require(router.pointerButton(&button) && target.actions == before + 1,
            "Real center-card click stopped activating");
}

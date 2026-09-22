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

using namespace Kadunce;
struct Target : WorkspaceInputTarget {
    bool guest = false;
    bool nativeInteraction = false;
    QRectF appletPopup;
    QRectF surface;
    WorkspacePresentation presentation = WorkspacePresentation::Spread;
    int cancellations = 0;
    bool canCancel = true;
    int actions = 0;
    int toggles = 0;
    int dismissals = 0;
    int guestNavigations = 0;
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
    bool stackPreviewArmedForInput() const override { return false; }
    int stackPreviewTargetForInput() const override { return 0; }
    bool centerCardContainsForInput(const QPointF &p) const override { return geometryForInput().centerCard.contains(p); }
    bool launcherGuestActiveForInput() const override { return guest; }
    bool launcherGuestContainsForInput(const QPointF &p) const override { return guest && centerCardContainsForInput(p); }
    bool isTabletPoint(const QPointF &p) const override { return geometryForInput().tablet.contains(p); }
    bool isPanelPoint(const QPointF &p) const override { return QRectF(100,740,800,60).contains(p) || appletPopup.contains(p); }
    bool surfaceOwnsTouchAt(const QPointF &p) const override { return surface.contains(p); }
    int activeSideForPoint(const QPointF &) const override { return 0; }
    bool selectedStackContains(const QPointF &) const override { return false; }
    int cardStackCandidate() const override { return 0; }
    void toggleFromInput() override { ++actions; ++toggles; if (onToggle) onToggle(); }
    void dismissLauncherGuestFromInput() override { ++actions; ++dismissals; }
    void navigateLauncherGuestFromInput(const QPointF &) override { ++actions; ++guestNavigations; }
    void pageLeftFromInput() override { ++actions; if (onPage) onPage(); }
    void pageRightFromInput() override { ++actions; if (onPage) onPage(); }
    void pageStackFromInput(int) override { ++actions; }
    void activateSelectedFromInput() override { ++actions; if (onActivate) onActivate(); }
    QPointF pickup, carried, dropped;
    int carryMoves = 0, drops = 0;
    bool acceptDrop = false;
    void beginCardGrab(const QPointF &p) override { ++actions; ++grabStarts; grabbed = true; pickup = p; }
    void updateCardGrab(const QPointF &p) override { ++actions; ++carryMoves; carried = p; }
    void pageCardGrab(int) override { ++actions; }
    void finishCardGrab(bool commit) override { ++actions; grabbed = false; if (!commit) ++grabCancels; }
    bool finishCardGrabOnOutput(const QPointF &p) override {
        ++actions; ++drops; dropped = p;
        if (acceptDrop) grabbed = false;
        return acceptDrop;
    }
    void setCardStackPreview(int) override { ++actions; }
    void clearCardStackPreview() override { ++actions; }
    bool pageCardStackInsertion(int) override { ++actions; return false; }
};
struct StackTarget final : Target {
    CardWorkspaceState<QString> workspace;
    std::optional<CardWorkspaceState<QString>::PreparedStackInsertion> insertion;
    int slot = 0, pages = 0, steps = 0;
    bool armed = false, valid = true, geometryValid = true;
    WorkspaceInputGeometry geometryForInput() const override {
        return geometryValid ? Target::geometryForInput() : WorkspaceInputGeometry{};
    }
    StackTarget() {
        workspace.reset({QStringLiteral("A"),QStringLiteral("B"),QStringLiteral("C")}, 0);
        workspace.stackSelectedWith(2, 0); // A,B
        workspace.page(1); // carry C
    }
    bool stackPreviewArmedForInput() const override { return armed; }
    int stackPreviewTargetForInput() const override { return armed ? 1 : 0; }
    int cardStackCandidate() const override { return grabbed && valid ? 1 : 0; }
    void setCardStackPreview(int) override {
        armed = true;
        insertion = workspace.prepareStackInsertionAtDepth(QStringLiteral("A"), slot);
    }
    void clearCardStackPreview() override { armed = false; insertion.reset(); }
    bool pageCardStackInsertion(int direction) override {
        ++steps;
        const int next = std::clamp(slot - direction, 0, 2);
        if (next == slot || !valid) return false;
        slot = next;
        insertion = workspace.prepareStackInsertionAtDepth(QStringLiteral("A"), slot);
        return bool(insertion);
    }
    void pageCardGrab(int) override { ++pages; }
    void finishCardGrab(bool commit) override {
        if (commit && valid && insertion) workspace.commitStackInsertion(*insertion);
        Target::finishCardGrab(commit);
        clearCardStackPreview();
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
    // Real router timers + actual immutable workspace insertion/order model.
    for (bool touch : {false, true}) {
        for (int side : {-1, 1}) {
            StackTarget target;
            WorkspaceInputRouter input(&target);
            KWin::PointerButtonEvent button{};
            button.position = {500,300}; button.button = Qt::LeftButton;
            button.state = KWin::PointerButtonState::Pressed;
            KWin::TouchDownEvent down{701,{500,300},{}};
            require(touch ? input.touchDown(&down) : input.pointerButton(&button), "Shoulder pickup escaped");
            waitForHold();
            auto move = [&](QPointF p) {
                KWin::TouchMotionEvent tm{701,p,{}};
                KWin::PointerMotionEvent pm{}; pm.position = p;
                require(touch ? input.touchMotion(&tm) : input.pointerMotion(&pm), "Shoulder move escaped");
            };
            move({side < 0 ? 150.0 : 850.0,300});
            waitForHold();
            require(target.pages == 0 && !target.armed, "Shoulder skipped deliberate dwell");
            waitForHold();
            require(target.pages == 1, "Shoulder did not page once after dwell");
            move({500,300});
            waitForHold(); waitForHold();
            require(target.pages == 1 && target.armed, "Inward contact retained travel timer");
            move({side < 0 ? 150.0 : 850.0,300});
            waitForHold(); waitForHold();
            require(target.pages == 1 && target.armed, "Shoulder stole armed stack slotting");
            move({side < 0 ? 20.0 : 980.0,300});
            waitForHold();
            require(target.pages == 2 && !target.armed, "Physical edge failed faster exit");
            input.cancelWorkspaceInteraction();
        }
    }
    // The adapter substitutes only compositor-facing target geometry/selection.
    for (bool touch : {false, true}) {
        for (int side : {-1, 1}) {
            StackTarget target;
            target.slot = side < 0 ? 2 : 0;
            WorkspaceInputRouter input(&target);
            QPointF contact(side < 0 ? 350 : 650, 300);
            KWin::PointerButtonEvent button{};
            button.position = contact; button.button = Qt::LeftButton;
            button.state = KWin::PointerButtonState::Pressed;
            KWin::TouchDownEvent down{601,contact,{}};
            require(touch ? input.touchDown(&down) : input.pointerButton(&button), "Stack pickup escaped");
            waitForHold();
            auto move = [&](QPointF p) {
                contact = p;
                KWin::TouchMotionEvent tm{601,p,{}};
                KWin::PointerMotionEvent pm{}; pm.position = p;
                require(touch ? input.touchMotion(&tm) : input.pointerMotion(&pm), "Stack carry escaped");
            };
            move(contact);
            waitForHold(); // arm insertion at an outer seam
            require(target.armed, "Stack preview did not arm");
            waitForHold(); waitForHold();
            require(target.pages == 0 && target.steps == 0, "Stationary stack entry caused slot/row paging");
            move(contact + QPointF(side * 45,0)); // outward at an end seam
            waitForHold(); waitForHold();
            require(target.pages == 0 && target.slot == (side < 0 ? 2 : 0)
                        && target.steps == 1, "End seam escaped into row paging");
            move(contact + QPointF(-side * 70,0)); // explicit inward slot request
            waitForHold();
            require(target.slot == 1 && target.steps == 2, "Fresh inward movement did not select middle slot");
            waitForHold();
            require(target.steps == 2 && target.pages == 0, "One slot request kept repeating while stationary");
            button.position = contact; button.state = KWin::PointerButtonState::Released;
            KWin::TouchUpEvent up{601,{}};
            require(touch ? input.touchUp(&up) : input.pointerButton(&button), "Stack release escaped");
            require(target.workspace.stackMembersForId(3) == std::vector<int>({3,1,2}),
                    "Preview/commit lost user-placed A,C,B order");
            require(target.workspace.selectedWindow() == QStringLiteral("A"), "Placement stole destination selection");
            target.workspace.pageStack(1);
            require(target.workspace.selectedWindow() == QStringLiteral("B"), "Stack browsing ignored placement");
            target.workspace.pageStack(1);
            require(target.workspace.selectedWindow() == QStringLiteral("C"), "Next member lost placed order");
            target.workspace.pageStack(-1);
            require(target.workspace.selectedWindow() == QStringLiteral("B")
                        && target.workspace.stackMembersForId(3) == std::vector<int>({3,1,2}),
                    "Reverse browsing reordered members");
        }
    }
    for (bool touch : {false, true}) {
        StackTarget target;
        WorkspaceInputRouter input(&target);
        KWin::PointerButtonEvent button{};
        button.position = {500,300}; button.button = Qt::LeftButton;
        button.state = KWin::PointerButtonState::Pressed;
        KWin::TouchDownEvent down{602,{500,300},{}};
        require(touch ? input.touchDown(&down) : input.pointerButton(&button), "Edge test pickup escaped");
        waitForHold();
        auto move = [&](QPointF p) {
            KWin::TouchMotionEvent tm{602,p,{}};
            KWin::PointerMotionEvent pm{}; pm.position = p;
            require(touch ? input.touchMotion(&tm) : input.pointerMotion(&pm), "Edge carry escaped");
        };
        move({500,300}); waitForHold();
        move({980,300}); waitForHold();
        require(target.pages > 0 && !target.armed, "Explicit screen edge did not leave stack and page");
        const int before = target.pages;
        target.geometryValid = false; // no motion event: timer must revalidate
        waitForHold();
        require(target.pages == before, "Edge repeat ignored invalid geometry");
        target.geometryValid = true;
        move({500,300}); waitForHold(); waitForHold();
        require(target.pages == before && target.armed, "Leaving edge failed to stop and return to insertion");
        move({570,300}); target.valid = false; waitForHold();
        require(target.steps == 0, "Stale destination consumed pending slot request");
        input.cancelWorkspaceInteraction();
        waitForHold();
        require(target.pages == before && !target.grabbed, "Cancellation retained held-row paging");
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
        const int afterToggle = target.actions;
        require(router.touchMotion(&motion) && router.touchUp(&up)
                    && target.actions == afterToggle && target.toggles == 1,
                "Synchronous view change leaked release or repeated toggle");
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
        // A bezel swipe first registers on the output's last rows.
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
        require(router.touchMotion(&move) && target.toggles == 1 && target.cancellations == 1,
                "Deliberate upward swipe failed to cancel client delivery before takeover");
        require(router.touchMotion(&move) && router.touchUp(&up) && target.toggles == 1,
                "Claimed swipe leaked release or triggered twice");
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
        target.canCancel = false;
        require(!router.touchDown(&down) && !router.touchMotion(&move) && !router.touchUp(&up),
                "Failed client cancellation still stole touch ownership");
        require(target.toggles == 1, "Failed client cancellation still opened Spread");
        require(!router.touchDown(&down), "Cancel test contact stolen");
        router.touchCancel();
        require(!router.touchMotion(&move) && !router.touchUp(&up), "Canceled candidate remained armed");
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
                "A swipe from the dock or the gutter opened Spread");
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
                "A pull on the handle opened Spread or canceled the handle");
        if (presentation != WorkspacePresentation::Active) continue;
        KWin::TouchDownEvent bezel{21,{500,798},{}};
        KWin::TouchMotionEvent bezelMove{21,{501,700},{}};
        KWin::TouchUpEvent bezelUp{21,{}};
        require(!router.touchDown(&bezel) && router.touchMotion(&bezelMove)
                    && router.touchUp(&bezelUp) && target.toggles == 1,
                "A bezel swipe below the handle stopped opening Spread");
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
                    && guest.guestNavigations == 0, "Outside tap navigated instead of dismissing");
        require(input.touchDown(&down), "Cancel contact missing");
        input.touchCancel();
        require(!input.touchUp(&up) && guest.dismissals == 1, "Canceled touch dismissed guest");
        KWin::PointerButtonEvent pointer{};
        pointer.position = {50,300}; pointer.button = Qt::LeftButton;
        pointer.state = KWin::PointerButtonState::Pressed;
        require(input.pointerButton(&pointer) && guest.dismissals == 1, "Outside mouse press acted early");
        pointer.state = KWin::PointerButtonState::Released;
        require(input.pointerButton(&pointer) && guest.dismissals == 2 && guest.guestNavigations == 0,
                "Outside mouse click navigated instead of dismissing");
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

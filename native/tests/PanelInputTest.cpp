/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WorkspaceInputRouter.h"
#include <input_event.h>
#include <QCoreApplication>
#include <cstdlib>
#include <iostream>

using namespace Kadunce;
struct Target final : WorkspaceInputTarget {
    bool guest = false;
    int actions = 0;
    WorkspacePresentation presentationForInput() const override { return WorkspacePresentation::CardLine; }
    WorkspaceInputGeometry geometryForInput() const override { return {{0,0,1000,800},{200,100,600,500},799,799}; }
    bool cardGrabActiveForInput() const override { return false; }
    bool stackPreviewArmedForInput() const override { return false; }
    int stackPreviewTargetForInput() const override { return 0; }
    bool centerCardContainsForInput(const QPointF &p) const override { return geometryForInput().centerCard.contains(p); }
    bool launcherGuestActiveForInput() const override { return guest; }
    bool launcherGuestContainsForInput(const QPointF &p) const override { return guest && centerCardContainsForInput(p); }
    bool isTabletPoint(const QPointF &p) const override { return geometryForInput().tablet.contains(p); }
    bool isPanelPoint(const QPointF &p) const override { return QRectF(100,740,800,60).contains(p); }
    int activeSideForPoint(const QPointF &) const override { return 0; }
    bool selectedStackContains(const QPointF &) const override { return false; }
    int cardStackCandidate() const override { return 0; }
    void toggleFromInput() override { ++actions; }
    void dismissLauncherGuestFromInput() override { ++actions; }
    void navigateLauncherGuestFromInput(const QPointF &) override { ++actions; }
    void pageLeftFromInput() override { ++actions; }
    void pageRightFromInput() override { ++actions; }
    void pageStackFromInput(int) override { ++actions; }
    void activateSelectedFromInput() override { ++actions; }
    void beginCardGrab() override { ++actions; }
    void updateCardGrab(double) override { ++actions; }
    void updateCardGrabDestination(const QPointF &) override { ++actions; }
    void pageCardGrab(int) override { ++actions; }
    void finishCardGrab(bool) override { ++actions; }
    bool finishCardGrabOnOutput(const QPointF &) override { ++actions; return false; }
    void setCardStackPreview(int) override { ++actions; }
    void clearCardStackPreview() override { ++actions; }
    bool pageCardStackInsertion(int) override { ++actions; return false; }
};
void require(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
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

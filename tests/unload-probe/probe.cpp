#include "WorkspaceInputRouter.h"
#include "BentoProbe.h"
#include "OwnershipTransitionProbe.h"
#include "SnapProbe.h"
#include "ContactProbe.h"
#include "NativeCarryHandoff.h"
#include "CarryWindowPaint.h"
#include <keyboard_input.h>
#include <effect/effect.h>
#include <input.h>
#include <touch_input.h>
#include <pointer_input.h>
#include <input_event.h>
#include <options.h>
#include <inputmethod.h>
#include <inputpanelv1window.h>
#include <main.h>
#include <window.h>
#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <memory>
#include <core/inputdevice.h>
using namespace Kadunce;
class Device final : public KWin::InputDevice {
public:
 QString name() const override { return "Isolated test input"; }
 bool isEnabled() const override { return true; }
 void setEnabled(bool) override {}
 bool isKeyboard() const override { return false; }
 bool isPointer() const override { return true; }
 bool isTouchpad() const override { return false; }
 bool isTouch() const override { return true; }
 bool isTabletTool() const override { return false; }
 bool isTabletPad() const override { return false; }
 bool isTabletModeSwitch() const override { return false; }
 bool isLidSwitch() const override { return false; }
};
struct Target final : WorkspaceInputTarget {
    bool guest = false;
    bool nativeInteraction = false;
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
    bool isPanelPoint(const QPointF &p) const override { return QRectF(100,740,800,60).contains(p); }
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
    void beginCardGrab(const QPointF &) override { ++actions; ++grabStarts; grabbed = true; }
    void updateCardGrab(const QPointF &) override { ++actions; }
    void pageCardGrab(int) override { ++actions; }
    void finishCardGrab(bool commit) override { ++actions; grabbed = false; if (!commit) ++grabCancels; }
    bool finishCardGrabOnOutput(const QPointF &) override { ++actions; return false; }
    void setCardStackPreview(int) override { ++actions; }
    void clearCardStackPreview() override { ++actions; }
    bool pageCardStackInsertion(int) override { ++actions; return false; }
};

// Passive ordering evidence only. The unload probe is loaded before Kadunce,
// while KWin inserts a later filter of equal weight before an existing one.
// Therefore a motion counted here was first offered to Kadunce's ScreenEdge
// filter and explicitly passed onward. Never consume or mutate input here.
struct EdgeOrderProbe final : KWin::InputEventFilter {
    EdgeOrderProbe() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge)
    {
        KWin::input()->installInputEventFilter(this);
    }
    bool pointerMotion(KWin::PointerMotionEvent *event) override
    {
        ++pointerMotions;
        pointerPosition = event->position;
        return false;
    }
    bool touchMotion(KWin::TouchMotionEvent *event) override
    {
        ++touchMotions;
        touchPosition = event->pos;
        return false;
    }
    QString state() const
    {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"pointerMotions", pointerMotions}, {"touchMotions", touchMotions},
            {"pointerX", pointerPosition.x()}, {"pointerY", pointerPosition.y()},
            {"touchX", touchPosition.x()}, {"touchY", touchPosition.y()}
        }).toJson(QJsonDocument::Compact));
    }
    int pointerMotions = 0;
    int touchMotions = 0;
    QPointF pointerPosition;
    QPointF touchPosition;
};

class UnloadProbe final : public KWin::Effect {
    Q_OBJECT
public:
    bool isActive() const override { return handoff && handoff->carry().busy(); }
    void prePaintScreen(KWin::ScreenPrePaintData &data) override {
        if (isActive()) data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        KWin::effects->prePaintScreen(data);
    }
    void prePaintWindow(KWin::RenderView *view, KWin::EffectWindow *window,
                        KWin::WindowPrePaintData &data) override {
        if (isActive() && handoff->carry().snapshot().window == window->window()) {
            data.setTransformed(); data.setTranslucent();
        }
        KWin::effects->prePaintWindow(view, window, data);
    }
    void paintScreen(const KWin::RenderTarget &target, const KWin::RenderViewport &viewport,
                     int mask, const KWin::Region &region, KWin::LogicalOutput *screen) override {
        paintingOutput = screen;
        KWin::effects->paintScreen(target, viewport, mask, region, screen);
        paintingOutput = nullptr;
    }
    void paintWindow(const KWin::RenderTarget &target, const KWin::RenderViewport &viewport,
                     KWin::EffectWindow *window, int mask, const KWin::Region &region,
                     KWin::WindowPaintData &data) override {
        if (isActive() && paintingOutput && handoff->carry().snapshot().window == window->window()) {
            const auto plan = Kadunce::carryPaintPlan(handoff->carry().snapshot().geometry,
                handoff->carry().position(), KWin::RectF(paintingOutput->geometry()));
            const auto before = window->frameGeometry();
            if (plan && Kadunce::paintCarryWindow(target, viewport, window, mask, region, data, *plan)) {
                ++carryPaints;
                carryPaintOutputs.insert(paintingOutput->name());
            }
            if (window->frameGeometry() != before) ++paintGeometryChanges;
            return;
        }
        KWin::effects->paintWindow(target, viewport, window, mask, region, data);
    }
    UnloadProbe() {
        KWin::input()->addInputDevice(&device);
        QDBusConnection::sessionBus().registerObject("/UnloadProbe", this, QDBusConnection::ExportAllSlots);
    }
    ~UnloadProbe() override { drop(); KWin::input()->removeInputDevice(&device); QDBusConnection::sessionBus().unregisterObject("/UnloadProbe"); }
public Q_SLOTS:
    QString edgeOptions() {
        return QString::number(KWin::options->electricBorderTiling()) + QLatin1Char('|')
            + QString::number(KWin::options->electricBorderMaximize());
    }
    void reloadEdgeOptions() { KWin::options->updateSettings(); }
    // What the keyboard covers and what the window holding text focus does
    // about it: the panel, the focused window's frame, the text cursor it
    // reports and the work area, read from the compositor that owns them.
    QString keyboardState() {
        auto rect = [](const KWin::RectF &r) {
            return QJsonObject{{"x", r.x()}, {"y", r.y()}, {"width", r.width()}, {"height", r.height()}};
        };
        auto *method = KWin::kwinApp()->inputMethod();
        QJsonObject state{{"overlayOption", KWin::options->overlayVirtualKeyboardOnWindows()}};
        if (!method) return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
        state.insert("visible", method->isVisible());
        if (auto *panel = KWin::effects->inputPanel()) state.insert("panel", rect(panel->frameGeometry()));
        if (auto *tracked = method->activeWindow()) {
            state.insert("tracked", tracked->caption());
            state.insert("trackedFrame", rect(tracked->frameGeometry()));
            state.insert("workArea", rect(KWin::workspace()->clientArea(KWin::MaximizeArea, tracked)));
        }
        state.insert("cursor", rect(method->cursorRectangle()));
        return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
    }
    // Every height the input panel reports while it is on screen, from now on,
    // so a scene can say whether the keys ever claimed more room as they left.
    void watchPanel() {
        panelHeights = {};
        QObject::disconnect(panelWatch);
        auto *method = KWin::kwinApp()->inputMethod();
        QPointer<KWin::Window> panel = method ? method->panel() : nullptr;
        if (!panel) return;
        panelWatch = connect(panel, &KWin::Window::frameGeometryChanged, this, [this, panel]() {
            const auto *shown = KWin::effects->inputPanel();
            if (panel && shown && shown->isVisible())
                panelHeights.append(panel->frameGeometry().height());
        });
    }
    QString panelHistory() {
        QJsonArray heights;
        for (double height : std::as_const(panelHeights)) heights.append(height);
        return QString::fromUtf8(QJsonDocument(heights).toJson(QJsonDocument::Compact));
    }
    QString frames() {
        QJsonObject frames;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (!w->isNormalWindow() || w->isDeleted()) continue;
            const auto r = w->frameGeometry();
            frames.insert(w->caption(), QJsonObject{{"x", r.x()}, {"y", r.y()}, {"width", r.width()}, {"height", r.height()}});
        }
        return QString::fromUtf8(QJsonDocument(frames).toJson(QJsonDocument::Compact));
    }
    // Which window a touch at this point is delivered to, and the stack above
    // the desktop, topmost last: what decides whether the keys or a surface
    // laid over them receive a finger.
    QString windowAt(int x, int y) {
        auto describe = [](const KWin::Window *w) {
            return QJsonObject{{"caption", w->caption()}, {"class", w->resourceClass()},
                {"inputMethod", w->isInputMethod()}, {"layer", int(w->layer())},
                {"hidden", w->isHidden()}};
        };
        QJsonObject state;
        if (auto *w = KWin::input()->findToplevel(QPointF(x, y))) state.insert("target", describe(w));
        QJsonArray stack;
        for (auto *w : KWin::workspace()->stackingOrder())
            if (!w->isDeleted() && w->layer() >= KWin::AboveLayer) stack.append(describe(w));
        state.insert("stack", stack);
        return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
    }
    // What KWin says about every client window, in stacking order, for the
    // sessions that measure which windows Kadunce should hold.
    QString windowFacts() {
        QJsonArray list;
        for (auto *w : KWin::workspace()->stackingOrder()) {
            if (w->isDeleted() || !w->isClient()) continue;
            const auto g = w->frameGeometry();
            list.append(QJsonObject{{"caption", w->caption()}, {"class", w->resourceClass()},
                {"id", w->internalId().toString(QUuid::WithoutBraces)},
                {"normal", w->isNormalWindow()}, {"dialog", w->isDialog()},
                {"transient", w->isTransient()}, {"modal", w->isModal()},
                {"parent", w->transientFor() ? w->transientFor()->caption() : QString()},
                {"parentId", w->transientFor() ? w->transientFor()->internalId().toString(QUuid::WithoutBraces) : QString()},
                {"attention", w->isDemandingAttention()},
                {"skipSwitcher", w->skipSwitcher()}, {"skipTaskbar", w->skipTaskbar()},
                {"minimized", w->isMinimized()}, {"hidden", w->isHidden()},
                {"active", w->isActive()}, {"layer", int(w->layer())},
                {"output", w->output() ? w->output()->name() : QString()},
                {"onCurrentDesktop", w->isOnCurrentDesktop()},
                {"x", g.x()}, {"y", g.y()}, {"width", g.width()}, {"height", g.height()}});
        }
        return QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
    }
    // What the dock or a task switcher does when a person picks a window.
    bool activateWindowId(const QString &id) {
        for (auto *w : KWin::workspace()->windows())
            if (w->internalId().toString(QUuid::WithoutBraces) == id) {
                KWin::workspace()->activateWindow(w);
                return true;
            }
        return false;
    }
    void setKeyboardOverlay(bool overlay) { KWin::options->setOverlayVirtualKeyboardOnWindows(overlay); }
    void hideKeyboard() { if (auto *method = KWin::kwinApp()->inputMethod()) method->hide(); }
    bool handoffArm(bool bentoSource, bool reject, bool interrupt) {
        if (!contact || !contact->client) return false;
        bento.client = contact->client->effectWindow();
        if (!bento.sourcePrepare(bentoSource)) return false;
        handoff = std::make_unique<Kadunce::NativeCarryHandoff>();
        dropResult.reset(); dropEnabled = false; dropCommits = 0;
        handoffFallbacks = handoffFinishes = 0; handoffResult = -1;
        carryPaints = 0;
        carryPaintOutputs.clear(); paintGeometryChanges = 0;
        contact->moves = contact->releases = contact->routeCancels = 0;
        contact->onRoute = [this](Kadunce::CarryInputRoute::Action action, Kadunce::CarryOwner owner, QPointF pos) {
            using A = Kadunce::CarryInputRoute::Action;
            if (action == A::Move) (void)handoff->carry().move(owner, pos);
            else if (action == A::Release) {
                if (dropEnabled) dropResult = handoff->releaseDrop(owner);
                else (void)handoff->carry().release(owner);
            }
            else if (action == A::Cancel) handoff->cancel();
            KWin::effects->addRepaintFull();
        };
        contact->onNativeStart = [this] {
            if (!bento.reservation || !handoff->stage(contact->client, *bento.reservation,
                    [this] { return bento.sourceValid(); }, [this] { ++handoffFallbacks; }))
                ++handoffFallbacks;
        };
        contact->onIdentified = [this, reject](Kadunce::CarryOwner owner, QPointF position) {
            const auto ticket = contact->contacts.soleCandidate();
            handoffResult = int(handoff->identify(contact->client, owner, position,
                [this, ticket, reject] { return !reject && ticket && bool(contact->contacts.resolve(*ticket)); }));
            if (handoffResult > 0) {
                if (!ticket || !contact->contacts.resolve(*ticket)) {
                    handoff->cancel();
                    return; // released during synchronous cancellation: nothing to drain
                }
                if (!contact->route.acquire(owner, handoffResult == 1)) qFatal("route already owned");
                if (owner.kind == Kadunce::CarryDevice::Touch)
                    KWin::waylandServer()->seat()->notifyTouchCancel();
                KWin::effects->addRepaintFull();
            }
        };
        QObject::disconnect(handoffFinishConnection);
        handoffFinishConnection = QObject::connect(contact->client, &KWin::Window::interactiveMoveResizeFinished,
            this, [this, interrupt] {
                if (handoff->ownsNativeFinish(contact->client)) {
                    ++handoffFinishes;
                    if (interrupt) handoff->cancel();
                } else handoff->flushPending();
            });
        return true;
    }
    QString handoffState() {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"fallbacks",handoffFallbacks},{"finishes",handoffFinishes},{"result",handoffResult},
            {"sourceValid",bento.sourceValid()},{"busy",handoff && handoff->carry().busy()},
            {"retained",handoff && bool(handoff->source())},
            {"paintCalls",carryPaints},
            {"paintOutputs",carryPaintOutputs.size()},
            {"paintGeometryChanges",paintGeometryChanges},
            {"inputBusy",contact && contact->route.busy()},
            {"moves",contact ? contact->moves : -1},
            {"releases",contact ? contact->releases : -1},
            {"routeCancels",contact ? contact->routeCancels : -1},
            {"moving",contact && contact->client && contact->client->isInteractiveMove()}
        }).toJson(QJsonDocument::Compact));
    }
    void handoffUnidentified(bool cancelPending) {
        if (!handoff || !contact || !contact->client) return;
        KWin::workspace()->performWindowOperation(contact->client, KWin::Options::MoveOp);
        if (cancelPending) handoff->cancel();
    }
    void handoffCancelInput() {
        if (contact) contact->dispatch(contact->route.cancel(), contact->route.owner());
    }
    bool handoffDisarm() {
        if (!handoff || !contact) return false;
        contact->onNativeStart = {}; contact->onIdentified = {};
        contact->onRoute = {};
        QObject::disconnect(handoffFinishConnection);
        handoff->cancel();
        const auto outcome = handoff->takeOutcome();
        const bool oneShot = !handoff->takeOutcome();
        const bool correctOutcome = dropResult ? !outcome : (handoffResult < 1) ? !outcome
            : outcome && bento.reservation && outcome->origin == bento.reservation->origin()
                && outcome->resolution == Kadunce::CarryResolution::ReturnToOrigin;
        if (auto *w = KWin::workspace()->moveResizeWindow()) w->cancelInteractiveMoveResize();
        const bool preserved = bento.sourceValid();
        if (preserved) bento.sourceRestoreWithoutMove();
        bento.controller.cancelRestoredMinimizations();
        if (contact->client) contact->client->setMinimized(false);
        handoff.reset();
        return (preserved || (dropResult && dropResult->committed))
            && oneShot && correctOutcome && !contact->route.busy();
    }
    void contactObserve() { contact = std::make_unique<ContactProbe>(); }
    bool contactStart() {
        if (!contact) return false;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->isNormalWindow() && !w->isDeleted() && w->window())
                return contact->watch(w->window());
        }
        return false;
    }
    QString contactState() { return contact ? contact->state() : QString(); }
    QString edgeOrderState() const { return edgeOrder.state(); }
    bool contactFocus() {
        if (!contact || !contact->client) return false;
        KWin::workspace()->activateWindow(contact->client, true);
        return true;
    }
    bool releaseRuntime() {
        auto *effect = QDBusConnection::sessionBus().objectRegisteredAt(QStringLiteral("/Kadunce"));
        return effect && QMetaObject::invokeMethod(effect, "release", Qt::DirectConnection);
    }
    bool entryClientsOnTablet() {
        KWin::LogicalOutput *target = nullptr;
        for (auto *o : KWin::effects->screens())
            if (o->name() == QStringLiteral("Virtual-0")) target = o;
        if (!target) return false;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (!w->isNormalWindow() || w->isDeleted() || !w->window()) continue;
            w->window()->sendToOutput(target);
        }
        return true;
    }
    // Leaves one window on a named display, as a person who had put it there
    // would, for the sessions that start from a window on the monitor.
    bool sendCaptionToOutput(const QString &caption, const QString &outputName) {
        KWin::LogicalOutput *target = nullptr;
        for (auto *o : KWin::effects->screens())
            if (o->name() == outputName) target = o;
        if (!target) return false;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->isDeleted() || !w->window() || w->caption() != caption) continue;
            w->window()->sendToOutput(target);
            return true;
        }
        return false;
    }
    bool maximizeCaption(const QString &caption) {
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->isDeleted() || !w->window() || w->caption() != caption) continue;
            w->window()->maximize(KWin::MaximizeFull);
            return true;
        }
        return false;
    }
    bool contactPrepareDecoration() { return contact && contact->prepareDecoration(); }
    bool contactLookupAll() {
        QSet<KWin::XdgToplevelInterface *> found;
        int count = 0;
        if (Kadunce::nativeMoveProtocol(nullptr)) return false;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (!w->isNormalWindow() || w->isDeleted() || !w->window()) continue;
            const auto surface = w->window()->surface();
            const auto protocol = Kadunce::nativeMoveProtocol(surface);
            if (!protocol || protocol->surface() != surface || found.contains(protocol)) return false;
            if (!contact || !contact->observer.watch(w->window())
                || !contact->observer.watch(w->window())
                || contact->observer.protocolFor(w->window()) != protocol) return false;
            found.insert(protocol); ++count;
        }
        return count >= 2;
    }
    void contactDrop() { contact.reset(); }
    void contactButton(bool pressed) {
        KWin::input()->pointer()->processButton(272, pressed ? KWin::PointerButtonState::Pressed
            : KWin::PointerButtonState::Released, now(), &device);
        KWin::input()->pointer()->processFrame();
    }
    void contactMotion(int x, int y) {
        KWin::input()->pointer()->processMotionAbsolute({double(x),double(y)},now(), &device);
        KWin::input()->pointer()->processFrame();
    }
    void contactCancel() { KWin::input()->touch()->cancel(); }
    void contactKeyboardMove() {
        if (contact && contact->client)
            KWin::workspace()->performWindowOperation(contact->client, KWin::Options::MoveOp);
    }
    void contactEndMove() {
        if (auto *w = KWin::workspace()->moveResizeWindow()) w->cancelInteractiveMoveResize();
    }
    void contactRemoveDevice() { KWin::input()->removeInputDevice(&device); }
    void contactAddDevice() { KWin::input()->addInputDevice(&device); }
    bool snapStart(bool intercept, bool touch) {
        if (!snap) {
            snap = std::make_unique<SnapProbe>();
            KWin::input()->installInputEventFilter(snap.get());
        }
        return snap->start(intercept, touch);
    }
    bool snapCapture() { return snap && snap->capture(); }
    bool snapBeginMove() { return snap && snap->beginMove(); }
    void snapInterrupt(const QString &mode) { if (snap) snap->interruptMode = mode; }
    bool snapForeignOwnerRejected() { return snap && snap->foreignOwnerRejected(); }
    QString snapOutcome() { return snap ? snap->outcome() : QString(); }
    QString snapState() { return snap ? snap->state() : QString(); }
    void snapDrop() { snap.reset(); }
    void shift(bool pressed) { KWin::input()->keyboard()->processKey(42,
        pressed ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released, now()); }
    void motion(int id, int x, int y) { KWin::input()->touch()->processMotion(id, {double(x), double(y)}, now()); KWin::input()->touch()->frame(); }
    bool bentoInterrupt() { return bento.interrupt(); }
    bool sourcePrepare(bool bentoSource) { return bento.sourcePrepare(bentoSource); }
    bool nativeRestoreControlPrepare() { return bento.nativeRestoreControlPrepare(); }
    bool nativeRestoreControlApply(bool together) { return bento.nativeRestoreControlApply(together); }
    bool nativeRestoreControlMinimize() { return bento.nativeRestoreControlMinimize(); }
    bool nativeRestoreControlShow() { return bento.nativeRestoreControlShow(); }
    bool sourceRestoreWithoutMove() { return bento.sourceRestoreWithoutMove(); }
    bool sourceCancelPendingRestore(int reason) { return bento.sourceCancelPendingRestore(reason); }
    bool sourceCanceledRestoreVisible() { return bento.sourceCanceledRestoreVisible(); }
    bool beginOwnedRestore() { return bento.beginOwnedRestore(); }
    bool destroyOwnedRestore() { return bento.destroyOwnedRestore(); }
    bool setSourceFullScreen(bool full) { return bento.setSourceFullScreen(full); }
    bool sourceBeginMove() { return bento.sourceBeginMove(); }
    bool sourceAdopt(bool interrupt) { return bento.sourceAdopt(interrupt); }
    bool sourceRestored() { return bento.sourceRestored(); }
    bool sourceVisibleRestored() { return bento.sourceVisibleRestored(); }
    QString sourceEvidence() {
        QString evidence = bento.reservationEvidence;
        QDebug(&evidence) << "native" << (bento.client ? bento.client->frameGeometry() : KWin::RectF{})
            << "original" << bento.reservationOriginal
            << "requested" << (bento.client ? bento.client->window()->moveResizeGeometry() : KWin::RectF{})
            << "saved-max" << (bento.reservation ? int(bento.reservation->restoreSnapshot().maximizeMode) : -1)
            << "saved-full" << (bento.reservation && bento.reservation->restoreSnapshot().fullScreen)
            << "minimized" << (bento.client && bento.client->isMinimized())
            << "expected" << bento.reservationIsBento;
        return evidence;
    }
    bool bentoRestored() { return bento.restored(); }
    bool bentoFresh() { return bento.fresh(); }
    bool bentoRestoreReentry() { return bento.restoreReentry(); }
    bool bentoOutputLostDuringRestore() { return bento.outputLostDuringRestore(); }
    bool cardAdmissionOrdering() { return bento.cardAdmissionOrdering(); }
    bool a2Setup() { return ownershipTransitions.setup(); }
    bool a2Begin() { return ownershipTransitions.begin(); }
    bool a2Arrival(bool oversized) { return ownershipTransitions.arrival(oversized); }
    bool a2Refused() { return ownershipTransitions.refused(); }
    bool a2CrossPrepare() { return ownershipTransitions.crossPrepare(); }
    bool a2CrossAdmit() { return ownershipTransitions.crossAdmit(); }
    bool a2Project() { return ownershipTransitions.project(); }
    bool a2OrdinaryNeighbor() { return ownershipTransitions.ordinaryNeighbor(); }
    bool a2PaneDrift() { return ownershipTransitions.paneDrift(); }
    bool a2PanesReplaced() { return ownershipTransitions.panesReplaced(); }
    bool a2Return() { return ownershipTransitions.returnToBento(); }
    bool a2Reactivate() { return ownershipTransitions.reactivate(); }
    bool a2ImmediatePlace() { return ownershipTransitions.immediatePlace(); }
    bool a2ImmediateRelease() { return ownershipTransitions.immediateRelease(); }
    bool a2Release() { return ownershipTransitions.release(); }
    bool a2Restored() { return ownershipTransitions.restored(); }
    QString a2Evidence() const { return ownershipTransitions.evidence; }
    bool bentoActiveAdmission() { return bento.bentoActiveAdmission(); }
    QString bentoActiveEvidence() const { return bento.extractionEvidence; }
    bool ownershipEntry() { return bento.ownershipEntry(); }
    bool ownershipRestored() { return bento.ownershipRestored(); }
    bool ownershipTransferPrepare() { return bento.ownershipTransferPrepare(); }
    bool ownershipTransfer() { return bento.ownershipTransfer(); }
    QString ownershipEvidence() const { return bento.ownershipEvidence; }
    bool tabletAdmissionOrdering() { return bento.tabletAdmissionOrdering(); }
    bool productionTabletAdmission() { return bento.productionTabletAdmission(); }
    bool prepareProductionTablet() { return bento.prepareProductionTablet(); }
    bool productionTabletPlaced() { return bento.productionTabletPlaced(); }
    QString tabletAdmissionEvidence() { return bento.tabletEvidence; }
    bool bentoBeginStableGeometry() { return bento.beginGeometryTest(false); }
    bool bentoStableGeometry() {
        const bool active = bento.controller.hasActiveSession();
        bento.controller.restoreAllSessions();
        return active;
    }
    void arm() { drop(); target = Target{}; router = std::make_unique<WorkspaceInputRouter>(&target); KWin::input()->installInputEventFilter(router.get()); }
    void drop() { if(router) router->cancelWorkspaceInteraction(); router.reset(); }
    void guest(bool value) { target.guest = value; }
    void down(int id, int x, int y) { KWin::input()->touch()->processDown(id, {double(x), double(y)}, now()); KWin::input()->touch()->frame(); }
    void up(int id) { KWin::input()->touch()->processUp(id, now()); KWin::input()->touch()->frame(); }
    void pointer(int x, int y) { KWin::input()->pointer()->processMotionAbsolute({double(x),double(y)},now()); KWin::input()->pointer()->processFrame(); }
    void button(bool pressed) { KWin::input()->pointer()->processButton(272, pressed ? KWin::PointerButtonState::Pressed : KWin::PointerButtonState::Released,now()); KWin::input()->pointer()->processFrame(); }
    QString state() { return QString::fromUtf8(QJsonDocument(QJsonObject{{"starts",target.grabStarts},{"cancels",target.grabCancels},{"grabbed",target.grabbed}}).toJson(QJsonDocument::Compact)); }
    QString windowGeometry(const QString &id) {
        for (auto *window : KWin::effects->stackingOrder()) {
            if (window->internalId().toString(QUuid::WithoutBraces) != id) continue;
            const auto r = window->frameGeometry();
            return QString::fromUtf8(QJsonDocument(QJsonObject{{"x",r.x()},{"y",r.y()},
                {"width",r.width()},{"height",r.height()}}).toJson(QJsonDocument::Compact));
        }
        return QStringLiteral("{}");
    }
    bool minimizeWindow(const QString &id, bool minimized) {
        for (auto *window : KWin::effects->stackingOrder()) {
            if (window->internalId().toString(QUuid::WithoutBraces) != id || !window->window()) continue;
            window->window()->setMinimized(minimized);
            return true;
        }
        return false;
    }
    bool windowMinimized(const QString &id) {
        for (auto *window : KWin::effects->stackingOrder())
            if (window->internalId().toString(QUuid::WithoutBraces) == id)
                return window->isMinimized();
        return false;
    }
private:
    KWin::LogicalOutput *paintingOutput = nullptr;
    int carryPaints = 0;
    QString destinationEvidence;
    bool dropEnabled = false;
    int dropCommits = 0;
    std::optional<Kadunce::NativeCarryHandoff::DropResult> dropResult;
    QPointer<KWin::LogicalOutput> destinationUnderTest;
    int paintGeometryChanges = 0;
    QList<double> panelHeights;
    QMetaObject::Connection panelWatch;
    QSet<QString> carryPaintOutputs;
    std::chrono::microseconds now() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()); }
    Target target;
    std::unique_ptr<SnapProbe> snap;
    std::unique_ptr<ContactProbe> contact;
    std::unique_ptr<Kadunce::NativeCarryHandoff> handoff;
    QMetaObject::Connection handoffFinishConnection;
    int handoffFallbacks = 0, handoffFinishes = 0, handoffResult = -1;
    BentoProbe bento;
    OwnershipTransitionProbe ownershipTransitions;
    Device device;
    EdgeOrderProbe edgeOrder;
    std::unique_ptr<WorkspaceInputRouter> router;
};
KWIN_EFFECT_FACTORY_SUPPORTED(UnloadProbe, "metadata.json", return true;)
#include "probe.moc"

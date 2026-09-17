#pragma once
#include "DesktopStageController.h"
#include "CardStageController.h"
#include "BentoCompositeGeometry.h"
#include <effect/effecthandler.h>
#include <core/output.h>
#include <window.h>

// Test-only host. Never installed; all virtual outputs are composition stages.
struct BentoProbeHost final : Kadunce::DesktopStageHost {
    QPointer<KWin::LogicalOutput> tablet;
    std::function<void(KWin::LogicalOutput *)> prepare;
    std::function<std::optional<Kadunce::NativeMoveSnapshot>(KWin::EffectWindow *)> restore;
    std::function<bool(KWin::EffectWindow *, const std::function<bool()> &)> admission;
    std::function<bool(KWin::EffectWindow *, const Kadunce::NativeMoveSnapshot &,
        const std::function<bool()> &)> independentAdmission;
    bool isTabletOutputForDesktopStage(const KWin::LogicalOutput *o) const override { return tablet && o == tablet; }
    bool allowsDesktopStageOnOutput(const KWin::LogicalOutput *) const override { return true; }
    bool isManagedWindowForDesktopStage(const KWin::EffectWindow *w) const override {
        return w && !w->isDeleted() && w->isNormalWindow() && w->window();
    }
    KWin::LogicalOutput *tabletOutputForDesktopStage() const override { return tablet; }
    KWin::Rect activeTargetForDesktopStage(KWin::LogicalOutput *o) const override { return o->geometry(); }
    void prepareOutputForDesktopStage(KWin::LogicalOutput *o) override { if (prepare) prepare(o); }
    std::optional<Kadunce::NativeMoveSnapshot> activeRestoreForDesktopStage(KWin::EffectWindow *w) const override {
        return restore ? restore(w) : std::nullopt;
    }
    bool admitTransferredWindowToTablet(KWin::EffectWindow *w, const std::function<bool()> &commit) override {
        return admission && admission(w, commit);
    }
    bool admitIndependentWindowToCardWorkspace(KWin::EffectWindow *w,
        const Kadunce::NativeMoveSnapshot &restore, const std::function<bool()> &commit) override {
        return independentAdmission && independentAdmission(w, restore, commit);
    }
};
struct TabletProbeHost final : Kadunce::CardStageHost {
    QPointer<KWin::LogicalOutput> tablet;
    std::function<void()> canceled;
    std::function<void(KWin::EffectWindow *)> connected;
    std::function<void(const QList<QPointer<KWin::EffectWindow>> &)> projectionRetired;
    std::function<bool(KWin::EffectWindow *, KWin::LogicalOutput *, const KWin::RectF &,
        const std::function<bool()> &, const std::function<void()> &)> desktopAdmission;
    std::function<bool(const Kadunce::BentoProjectionSession &,
        const std::function<bool()> &, const std::function<void()> &)> projectionResume;
    KWin::LogicalOutput *tabletOutputForCardStage() const override { return tablet; }
    bool isTabletOutputForCardStage(const KWin::LogicalOutput *o) const override { return o == tablet; }
    bool isManagedWindowForCardStage(const KWin::EffectWindow *w) const override {
        return w && !w->isDeleted() && w->isNormalWindow() && w->window();
    }
    void setPagingShortcutsForCardStage(bool) override {}
    void cancelInputForCardStage() override { if (canceled) canceled(); }
    void connectManagedWindowForCardStage(KWin::EffectWindow *w) override { if (connected) connected(w); }
    void unredirectForCardStage(KWin::EffectWindow *) override {}
    void retireBentoProjectionForCardStage(
        const QList<QPointer<KWin::EffectWindow>> &windows) override {
        if (projectionRetired) projectionRetired(windows);
    }
    bool admitCardToDesktopStage(KWin::EffectWindow *w, KWin::LogicalOutput *o, const KWin::RectF &g,
        const std::function<bool()> &commit, const std::function<void()> &release) override {
        return desktopAdmission && desktopAdmission(w,o,g,commit,release);
    }
    bool resumeBentoProjectionForCardStage(const Kadunce::BentoProjectionSession &projection,
        const std::function<bool()> &commit, const std::function<void()> &release) override {
        return projectionResume && projectionResume(projection, commit, release);
    }
};
struct BentoProbe {
    TabletProbeHost ownershipHost;
    std::unique_ptr<Kadunce::CardStageController> ownershipCards;
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> ownershipExpected;
    QPointer<KWin::EffectWindow> ownershipArrival;
    QPointer<KWin::LogicalOutput> ownershipSource;
    Kadunce::NativeMoveSnapshot ownershipSourceSnapshot;
    QString ownershipEvidence;
    bool ownershipEntry() {
        QList<KWin::EffectWindow *> windows;
        for (auto *w : KWin::effects->stackingOrder())
            if (host.isManagedWindowForDesktopStage(w)) windows.append(w);
        if (windows.size() < 2) return false;
        ownershipHost.tablet = windows.first()->screen();
        ownershipExpected.clear();
        for (auto *w : windows) {
            if (w->screen() != ownershipHost.tablet) return false;
            ownershipExpected.append({w, w->window()->moveResizeGeometry()});
        }
        ownershipCards = std::make_unique<Kadunce::CardStageController>(&ownershipHost);
        ownershipCards->toggle();
        if (!ownershipCards->isActive()
            || ownershipCards->presentation() != Kadunce::CardPresentation::CardLine) return false;
        for (const auto &[w, geometry] : ownershipExpected) {
            const auto saved = ownershipCards->managedRestore(w);
            if (!saved || saved->geometry != geometry || w->window()->moveResizeGeometry() != geometry) {
                ownershipEvidence = QStringLiteral("Initial member has no retained origin or entry changed native geometry");
                return false;
            }
        }
        // Later visits cannot overwrite the shared-entry records.
        ownershipCards->toggle();
        ownershipCards->pageHorizontal(1);
        for (const auto &[w, geometry] : ownershipExpected) {
            const auto saved = ownershipCards->managedRestore(w);
            if (!saved || saved->geometry != geometry) return false;
        }
        ownershipCards->release();
        return true;
    }
    bool ownershipRestored() const {
        for (const auto &[w, geometry] : ownershipExpected)
            if (!w || !w->window() || w->frameGeometry() != geometry) return false;
        return !ownershipExpected.isEmpty();
    }
    bool ownershipTransferPrepare() {
        ownershipCards.reset();
        ownershipArrival.clear();
        for (auto *w : KWin::effects->stackingOrder())
            if (host.isManagedWindowForDesktopStage(w)) { ownershipArrival = w; break; }
        if (!ownershipArrival) return false;
        ownershipHost.tablet = ownershipArrival->screen();
        for (auto *o : KWin::effects->screens())
            if (o != ownershipHost.tablet) { ownershipSource = o; break; }
        if (!ownershipSource) return false;
        ownershipArrival->window()->sendToOutput(ownershipSource);
        return true;
    }
    bool ownershipTransfer() {
        auto *w = ownershipArrival.data();
        if (!w || !w->window() || w->screen() != ownershipSource) return false;
        auto *native = w->window();
        ownershipSourceSnapshot = {native, ownershipSource, native->moveResizeGeometry(),
            native->geometryRestore(), native->fullscreenGeometryRestore(), native->maximizeMode(),
            native->quickTileMode(), native->isFullScreen(), native->isMinimized()};
        ownershipCards = std::make_unique<Kadunce::CardStageController>(&ownershipHost);
        int calls = 0;
        if (ownershipCards->admitTransferredWindowToTablet(w, [&] { ++calls; return false; }, {}, &ownershipSourceSnapshot)
            || calls != 1 || ownershipCards->isActive() || ownershipCards->liveCardIndex(w) >= 0
            || native->moveResizeGeometry() != ownershipSourceSnapshot.geometry
            || w->screen() != ownershipSource) return false;
        // Cancellation/rejection keeps the source; acceptance establishes the
        // tablet ordinary origin before the receiver's Active geometry write.
        const bool accepted = ownershipCards->admitTransferredWindowToTablet(w, [&] {
            ++calls;
            return native->moveResizeGeometry() == ownershipSourceSnapshot.geometry;
        }, {}, &ownershipSourceSnapshot);
        const auto saved = ownershipCards->managedRestore(w);
        if (!accepted || calls != 2 || !saved || saved->geometry == ownershipSourceSnapshot.geometry
            || !ownershipHost.tablet->geometry().contains(saved->geometry.center().toPoint())
            || saved->geometry.size() != ownershipSourceSnapshot.geometry.size()) {
            ownershipEvidence = QStringLiteral("Committed tablet origin still points to source or changed ordinary size");
            return false;
        }
        ownershipExpected = {{w, saved->geometry}};
        ownershipCards->release();
        return native->moveResizeGeometry() == saved->geometry;
    }
    TabletProbeHost reservationHost;
    std::unique_ptr<Kadunce::CardStageController> reservationCards;
    std::optional<Kadunce::PreparedCarrySource> reservation;
    Kadunce::NativeMoveTakeover reservationTakeover;
    bool reservationIsBento = false;
    KWin::RectF reservationOriginal;
    KWin::RectF reservationFloating;
    KWin::RectF reservationFullscreenRestore;
    QString reservationEvidence;
    std::unique_ptr<Kadunce::DesktopStageController> shortLivedRestoreOwner;
    bool beginOwnedRestore() {
        if (!client || !client->window()) return false;
        controller.restoreAllSessions();
        reservationOriginal = client->frameGeometry();
        client->window()->setMinimized(true);
        shortLivedRestoreOwner = std::make_unique<Kadunce::DesktopStageController>(&host);
        return shortLivedRestoreOwner->toggleOnOutput(client->screen()->name());
    }
    bool destroyOwnedRestore() {
        if (!shortLivedRestoreOwner) return false;
        shortLivedRestoreOwner->restoreAllSessions();
        const bool pending = client && !client->isMinimized();
        shortLivedRestoreOwner.reset();
        return pending;
    }
    bool setSourceFullScreen(bool full) {
        if (!client || !client->window()) return false;
        client->window()->setFullScreen(full);
        return true;
    }
    bool nativeRestoreControlPrepare() {
        for (auto *w : KWin::effects->stackingOrder()) {
            if (host.isManagedWindowForDesktopStage(w)) { client = w; break; }
        }
        if (!client || !client->window()) return false;
        reservationOriginal = client->frameGeometry();
        client->window()->maximize(KWin::MaximizeRestore);
        client->window()->moveResize({0, 0, 1260, 770});
        return true;
    }
    bool nativeRestoreControlApply(bool minimizeTogether) {
        if (!client || !client->window()) return false;
        client->window()->maximize(KWin::MaximizeFull);
        if (minimizeTogether) client->window()->setMinimized(true);
        return true;
    }
    bool nativeRestoreControlMinimize() {
        if (!sourceVisibleRestored()) return false;
        client->window()->setMinimized(true);
        return true;
    }
    bool nativeRestoreControlShow() {
        if (!client || !client->isMinimized()) return false;
        client->window()->setMinimized(false);
        return true;
    }
    bool sourcePrepare(bool bento) {
        controller.restoreAllSessions();
        if (reservationCards) reservationCards->release();
        reservationTakeover.cancel(); reservationTakeover.takeOutcome();
        reservation.reset();
        if (!client || !client->window()) return false;
        client->window()->setMinimized(bento);
        reservationOriginal = client->frameGeometry();
        reservationFloating = client->window()->geometryRestore();
        reservationFullscreenRestore = client->window()->fullscreenGeometryRestore();
        reservationIsBento = bento;
        if (bento) {
            if (!controller.toggleOnOutput(client->screen()->name())) return false;
            reservation = controller.prepareNativeCarrySource(client);
        } else {
            reservationHost.tablet = client->screen();
            reservationCards = std::make_unique<Kadunce::CardStageController>(&reservationHost);
            KWin::workspace()->activateWindow(client->window(), true);
            reservationCards->toggle();
            reservationCards->toggle();
            reservation = reservationCards->prepareNativeCarrySource(client);
        }
        if (!reservation) return false;
        const auto before = client->frameGeometry();
        const bool valid = sourceValid();
        // Cross-controller tokens are never interchangeable, even for one client.
        const bool crossRejected = bento
            ? (!reservationCards || !reservationCards->nativeCarrySourceValid(*reservation))
            : !controller.nativeCarrySourceValid(*reservation);
        return valid && crossRejected && client->frameGeometry() == before
            && reservation->restoreSnapshot().geometry == reservationOriginal
            && reservation->restoreSnapshot().floatingGeometry == reservationFloating
            && reservation->restoreSnapshot().fullscreenRestoreGeometry == reservationFullscreenRestore
            && reservation->restoreSnapshot().minimized == bento
            && reservation->origin().restoreToken != 0;
    }
    bool sourceValid() const {
        return reservation && (reservationIsBento
            ? controller.nativeCarrySourceValid(*reservation)
            : reservationCards->nativeCarrySourceValid(*reservation));
    }
    bool sourceBeginMove() {
        if (!sourceValid() || KWin::workspace()->moveResizeWindow()) return false;
        KWin::workspace()->performWindowOperation(client->window(), KWin::Options::MoveOp);
        return true;
    }
    bool sourceRestoreWithoutMove() {
        if (!sourceValid()) return false;
        if (reservationIsBento) controller.restoreAllSessions();
        else reservationCards->release();
        reservationEvidence = QStringLiteral("baseline restore without native move");
        return !sourceValid();
    }
    bool sourceCancelPendingRestore(int reason) {
        if (!sourceValid() || !reservationIsBento || !sourceRestoreWithoutMove()) return false;
        if (!client || client->isMinimized()) return false; // Must exercise the pending path.
        if (reason == 1) controller.cancelRestoredMinimizations();
        else if (reason == 2) {
            auto *output = client->screen();
            controller.handleScreenRemoved(output);
            controller.handleScreenAdded(output);
        } else if (reason == 3) {
            KWin::workspace()->performWindowOperation(client->window(), KWin::Options::MoveOp);
            client->window()->cancelInteractiveMoveResize();
        } else return false;
        return !client->isMinimized();
    }
    bool sourceCanceledRestoreVisible() const {
        return client && !client->isMinimized() && !sourceValid();
    }
    bool sourceAdopt(bool interrupt) {
        if (!sourceValid() || !client || !client->window()->isInteractiveMove()) return false;
        const auto native = client->frameGeometry();
        bool guarded = false;
        const auto connection = QObject::connect(client->window(), &KWin::Window::interactiveMoveResizeFinished,
            client->window(), [&] {
                guarded = reservationTakeover.ownsNativeFinish(client->window()) && sourceValid();
                // Coordinator test seam: takeover finish is NOT delivered to
                // legacy manual-change/native-drop handlers. Full Effect pending.
                if (interrupt) {
                    if (reservationIsBento) controller.restoreAllSessions();
                    else reservationCards->release();
                }
            });
        const Kadunce::CarryOwner owner{Kadunce::CarryDevice::Pointer, 1, 272};
        const auto result = reservationTakeover.adopt(client->window(), owner,
            reservation->origin(), KWin::effects->cursorPos(), native.topLeft(),
            [this] { return sourceValid(); });
        QObject::disconnect(connection);
        bool correct = guarded;
        if (interrupt) {
            correct &= result == Kadunce::NativeMoveTakeover::Result::Interrupted
                && !reservationTakeover.busy() && !sourceValid();
        } else {
            correct &= result == Kadunce::NativeMoveTakeover::Result::Carrying
                && sourceValid() && client->frameGeometry() == native;
            correct &= reservationTakeover.move(owner, KWin::effects->cursorPos() + QPointF(800, -300))
                && client->frameGeometry() == native;
            reservationTakeover.cancel();
        }
        const auto outcome = reservationTakeover.takeOutcome();
        correct &= outcome && outcome->origin == reservation->origin()
            && outcome->resolution == (interrupt ? Kadunce::CarryResolution::NeedsRecovery
                                                : Kadunce::CarryResolution::ReturnToOrigin);
        if (reservationIsBento) controller.restoreAllSessions();
        else reservationCards->release();
        correct &= !sourceValid();
        reservationEvidence = QStringLiteral("bento=%1 interrupt=%2 guarded=%3 correct=%4")
            .arg(reservationIsBento).arg(interrupt).arg(guarded).arg(correct);
        return correct;
    }
    bool sourceRestored() {
        // Check state flags while hidden, then require exact presented geometry
        // after showing it. The second check is mandatory: maximized state alone
        // does not prove that restoration actually filled the output.
        const bool restored = client && client->window()
            && reservation
            && client->window()->maximizeMode() == reservation->restoreSnapshot().maximizeMode
            && client->window()->isFullScreen() == reservation->restoreSnapshot().fullScreen
            && client->window()->quickTileMode() == reservation->restoreSnapshot().quickTileMode
            && (reservationIsBento && reservation->restoreSnapshot().maximizeMode != KWin::MaximizeRestore
                ? true : client->window()->moveResizeGeometry() == reservationOriginal)
            && client->isMinimized() == reservationIsBento && !sourceValid();
        if (restored && reservationIsBento) client->window()->setMinimized(false);
        return restored;
    }
    bool sourceVisibleRestored() const {
        return client && client->frameGeometry() == reservationOriginal && !sourceValid();
    }
    BentoProbeHost host;
    Kadunce::DesktopStageController controller{&host};
    QPointer<KWin::EffectWindow> client;
    KWin::RectF original;
    int interruptions = 0;
    QTimer geometryContention;
    int contestedFrames = 0;
    int unwantedCorrections = 0;
    QString tabletEvidence = QStringLiteral("not started");
    QString activeEvidence = QStringLiteral("not started");
    QPointer<KWin::LogicalOutput> expectedTablet;
    bool productionTabletPlaced() const { return client && expectedTablet && client->screen() == expectedTablet; }
    bool beginGeometryTest(bool contest) {
        for (auto *w : KWin::effects->stackingOrder()) {
            if (host.isManagedWindowForDesktopStage(w)) { client = w; break; }
        }
        if (!client || !client->window()) return false;
        original = client->frameGeometry();
        const bool activated = controller.toggleOnOutput(client->screen()->name());
        contestedFrames = 0;
        unwantedCorrections = 0;
        QObject::disconnect(&geometryContention, nullptr, nullptr, nullptr);
        QObject::connect(&geometryContention, &QTimer::timeout, &geometryContention, [this] {
            if (!client || !client->window() || !controller.hasActiveSession()) {
                geometryContention.stop();
                return;
            }
            if (contestedFrames && client->frameGeometry().toRect() != KWin::Rect(80,80,600,400))
                ++unwantedCorrections;
            ++contestedFrames;
            client->window()->moveResize({80,80,600,400});
        });
        if (contest) geometryContention.start(50);
        return activated && controller.hasActiveSession();
    }
    bool geometryRecovered() const {
        return contestedFrames > 3 && unwantedCorrections == 0 && client && !controller.hasActiveSession() && !client->isMinimized()
            && client->frameGeometry().toRect() == original.toRect();
    }
    bool interrupt() {
        for (auto *w : KWin::effects->stackingOrder()) {
            if (host.isManagedWindowForDesktopStage(w)) { client = w; break; }
        }
        if (!client) return false;
        original = client->frameGeometry();
        client->window()->setMinimized(true);
        interruptions = 0;
        const auto connection = QObject::connect(client->window(), &KWin::Window::minimizedChanged,
            client->window(), [this] {
                if (interruptions || !client || !client->window() || client->isMinimized()) return;
                ++interruptions;
                controller.restoreAllSessions();
            }, Qt::DirectConnection);
        controller.toggleOnOutput(client->screen()->name());
        QObject::disconnect(connection);
        return interruptions == 1 && !controller.hasActiveSession() && client && client->isMinimized();
    }
    bool restored() const {
        return interruptions == 1 && !controller.hasActiveSession() && client
            && client->isMinimized() && client->frameGeometry().toRect() == original.toRect();
    }
    bool fresh() {
        if (!client || !client->window()) return false;
        client->window()->setMinimized(false);
        const bool activated = controller.toggleOnOutput(client->screen()->name()) && controller.hasActiveSession();
        controller.restoreAllSessions();
        return activated && !controller.hasActiveSession() && client && !client->isMinimized();
    }
    bool restoreReentry() {
        if (!client || !client->window()) return false;
        client->window()->setMinimized(true);
        const QString key = client->screen()->name();
        if (!controller.toggleOnOutput(key)) return false;
        int attempts = 0;
        bool admitted = false;
        const auto connection = QObject::connect(client->window(), &KWin::Window::minimizedChanged,
            client->window(), [&] {
                if (!client || !client->isMinimized()) return;
                ++attempts;
                admitted = controller.toggleOnOutput(key);
            }, Qt::DirectConnection);
        controller.restoreAllSessions();
        QObject::disconnect(connection);
        return attempts == 1 && !admitted && !controller.hasActiveSession()
            && client && client->isMinimized();
    }
    bool outputLostDuringRestore() {
        if (!client || !client->window() || KWin::effects->screens().size() < 2) return false;
        QPointer<KWin::LogicalOutput> lost = client->screen();
        client->window()->setMinimized(true);
        if (!controller.toggleOnOutput(lost->name())) return false;
        int removals = 0;
        const auto connection = QObject::connect(client->window(), &KWin::Window::minimizedChanged,
            client->window(), [&] {
                if (removals || !client || !client->isMinimized()) return;
                ++removals;
                // Inject the removal notification while KWin's output list still
                // contains it. Physical hot-unplug is a separate acceptance test.
                controller.handleScreenRemoved(lost);
            }, Qt::DirectConnection);
        controller.restoreAllSessions();
        QObject::disconnect(connection);
        const bool recovered = removals == 1 && client && client->screen() != lost
            && client->isMinimized() && !controller.hasActiveSession();
        controller.handleScreenAdded(lost);
        return recovered;
    }
    bool prepareProductionTablet() {
        if (!client || !client->window()) return false;
        controller.restoreAllSessions();
        auto *origin = client->screen();
        for (auto *o : KWin::effects->screens()) if (o != origin) { host.tablet = o; break; }
        if (!host.tablet) return false;
        for (auto *w : KWin::effects->stackingOrder())
            if (w != client && host.isManagedWindowForDesktopStage(w)) {
                w->window()->sendToOutput(host.tablet);
                w->window()->moveResize(KWin::RectF(host.tablet->geometry()));
            }
        return true;
    }
    bool productionTabletAdmission() {
        tabletEvidence = QStringLiteral("production setup");
        if (!client || !client->window() || !host.tablet) return false;
        auto *origin = client->screen();
        TabletProbeHost tabletHost;
        tabletHost.tablet = host.tablet;
        Kadunce::CardStageController cards(&tabletHost);
        cards.toggle();
        if (!cards.isActive() || !cards.selectedWindow()) return false;
        const auto selected = QPointer<KWin::EffectWindow>(cards.selectedWindow());
        const auto nativeGeometry = selected->frameGeometry();
        const auto pickupTarget = cards.previewTargetForWindow(host.tablet, selected);
        const QPointF pickup(pickupTarget.x() + 53, pickupTarget.y() + 91);
        cards.beginCardGrab(pickup);
        cards.updateCardGrab(pickup + QPointF(1800, -900));
        const bool freeCarry = cards.cardGrabActive()
            && cards.cardGrabTarget() == pickupTarget
            && cards.cardGrabOffset() == QPointF(1800, -900)
            && selected->frameGeometry() == nativeGeometry
            && cards.cardStackCandidate() == 0;
        cards.finishCardGrab(false);
        const bool canceled = !cards.cardGrabActive()
            && cards.cardGrabOffset().isNull()
            && cards.cardGrabTarget().isEmpty()
            && cards.selectedWindow() == selected
            && selected->frameGeometry() == nativeGeometry;
        if (!freeCarry || !canceled) {
            tabletEvidence = QStringLiteral("free-carry=%1 canceled=%2").arg(freeCarry).arg(canceled);
            cards.release();
            return false;
        }
        qInfo() << "PASS: production Card Line 2D carry preserves pickup and native geometry; cancel resets pose";
        tabletEvidence = QStringLiteral("active=%1 members=%2 source=%3 tablet=%4")
            .arg(cards.isActive()).arg(cards.liveCards().size()).arg(origin->name()).arg(host.tablet->name());
        if (!cards.isActive() || !controller.toggleOnOutput(origin->name())) return false;
        bool ordered = false;
        tabletHost.connected = [&](auto *w) {
            if (w == client) ordered = !controller.managesWindow(w)
                && cards.liveCardIndex(w) >= 0 && w->screen() == origin;
        };
        host.admission = [&](auto *w, const auto &commit) {
            return cards.admitTransferredWindowToTablet(w, commit);
        };
        const bool accepted = controller.handoffLeadToOutput(origin->name(), host.tablet->name());
        expectedTablet = host.tablet;
        const bool result = accepted && ordered && cards.liveCardIndex(client) >= 0
            && cards.liveCards().size() == 2;
        tabletEvidence = QStringLiteral("accepted=%1 ordered=%2 members=%3 screen=%4 tablet=%5")
            .arg(accepted).arg(ordered).arg(cards.liveCards().size()).arg(client->screen()->name()).arg(host.tablet->name());
        host.admission = {};
        host.tablet.clear();
        cards.release();
        controller.restoreAllSessions();
        return result;
    }
    bool tabletAdmissionOrdering() {
        tabletEvidence = QStringLiteral("setup");
        if (!client || !client->window()) return false;
        auto *origin = client->screen();
        for (auto *o : KWin::effects->screens()) if (o != origin) { host.tablet = o; break; }
        if (!host.tablet) return false;
        // Leave exactly one candidate on the source; prior restoration may
        // have returned the companion here, making it the stage lead.
        for (auto *w : KWin::effects->stackingOrder())
            if (w != client && host.isManagedWindowForDesktopStage(w))
                w->window()->sendToOutput(host.tablet);
        if (!controller.toggleOnOutput(origin->name())) return false;
        const QString source = origin->name(), target = host.tablet->name();
        host.admission = [](auto *, const auto &) { return false; };
        const bool rejected = !controller.handoffLeadToOutput(source, target)
            && controller.managesWindow(client) && client->screen() == origin;
        bool staleRejected = false;
        host.admission = [&](auto *, const auto &commit) {
            controller.stopPendingSettle();
            staleRejected = !commit();
            return false;
        };
        const bool stale = !controller.handoffLeadToOutput(source, target)
            && staleRejected && controller.managesWindow(client);
        bool ordered = false;
        host.admission = [&](auto *w, const auto &commit) {
            if (!commit()) return false;
            ordered = !controller.managesWindow(w) && !commit();
            w->window()->sendToOutput(host.tablet);
            w->window()->moveResize(KWin::RectF(host.tablet->geometry()));
            return true;
        };
        const bool accepted = controller.handoffLeadToOutput(source, target)
            && ordered && client->screen() == host.tablet && !controller.managesWindow(client);
        tabletEvidence = QStringLiteral("rejected=%1 stale=%2 accepted=%3 ordered=%4 state=%5")
            .arg(rejected).arg(stale).arg(accepted).arg(ordered).arg(controller.outputStageState().join(';'));
        host.admission = {};
        host.tablet.clear();
        controller.restoreAllSessions();
        return rejected && stale && accepted;
    }
    bool bentoActiveAdmission() {
        activeEvidence = QStringLiteral("collect");
        controller.restoreAllSessions();
        QList<QPointer<KWin::EffectWindow>> windows;
        for (auto *w : KWin::effects->stackingOrder())
            if (host.isManagedWindowForDesktopStage(w)) windows.append(w);
        if (windows.size() < 3 || KWin::effects->screens().size() < 2) return false;
        activeEvidence = QStringLiteral("choose-output");
        KWin::LogicalOutput *tablet = nullptr;
        KWin::LogicalOutput *other = nullptr;
        for (auto *candidate : KWin::effects->screens()) {
            const int count = std::count_if(windows.cbegin(), windows.cend(),
                [candidate](const auto &window) { return window->screen() == candidate; });
            if (count >= 2) { tablet = candidate; break; }
        }
        if (!tablet) return false;
        activeEvidence = QStringLiteral("activate-bento");
        for (auto *candidate : KWin::effects->screens())
            if (candidate != tablet) { other = candidate; break; }
        if (!other) return false;
        host.tablet = tablet;
        windows.removeIf([tablet](const auto &window) { return window->screen() != tablet; });
        for (auto &window : windows) window->window()->setMinimized(false);
        QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> sourceOrigins;
        for (const auto &window : windows) sourceOrigins.append({window, window->frameGeometry()});
        const bool toggled = controller.toggleOnOutput(tablet->name());
        if (!toggled) return false;
        activeEvidence = QStringLiteral("prepare");
        const auto otherBefore = controller.outputStageState();
        TabletProbeHost cardsHost;
        cardsHost.tablet = tablet;
        Kadunce::CardStageController cards(&cardsHost);
        auto prepare = [&](KWin::EffectWindow *window) {
            const auto source = controller.prepareNativeCarrySource(window);
            const auto drop = controller.prepareCardDrop(window, tablet,
                host.activeTargetForDesktopStage(tablet),
                Kadunce::DesktopStageController::CardDropIntent::ActiveCard);
            return std::pair(source, drop);
        };
        auto [rejectedSource, rejectedDrop] = prepare(windows[0]);
        if (!rejectedSource || !rejectedDrop) return false;
        activeEvidence = QStringLiteral("transactions");
        const auto exactBefore = controller.outputStageState();
        host.admission = [](auto *, const auto &) { return false; };
        const bool rejected = !controller.transferBentoCarryToActive(*rejectedSource, *rejectedDrop)
            && controller.outputStageState() == exactBefore && controller.managesWindow(windows[0]);

        auto [staleSource, staleDrop] = prepare(windows[0]);
        bool staleCommitRejected = false;
        host.admission = [&](auto *, const auto &commit) {
            controller.stopPendingSettle();
            staleCommitRejected = !commit();
            return false;
        };
        const bool stale = staleSource && staleDrop
            && !controller.transferBentoCarryToActive(*staleSource, *staleDrop)
            && staleCommitRejected && controller.outputStageState() == exactBefore
            && controller.managesWindow(windows[0]);

        auto [firstSource, firstDrop] = prepare(windows[0]);
        bool admissionCalled = false, sourceCommitAccepted = false;
        if (firstSource) {
            const auto restore = firstSource->restoreSnapshot();
            host.admission = [&, restore](KWin::EffectWindow *window, const auto &commit) {
                admissionCalled = true;
                return cards.admitTransferredWindowToTablet(window, [&] {
                    sourceCommitAccepted = commit();
                    return sourceCommitAccepted;
                }, {}, &restore);
            };
        }
        const bool firstCommit = firstSource && firstDrop
            && controller.transferBentoCarryToActive(*firstSource, *firstDrop);
        const bool firstReplay = firstSource && firstDrop
            && !controller.transferBentoCarryToActive(*firstSource, *firstDrop);
        const bool firstCards = cards.isActive() && cards.liveCardIndex(windows[0]) >= 0;
        const bool firstDeparted = !controller.managesWindow(windows[0]);
        const bool firstRemaining = controller.managesWindow(windows[1]);
        const bool firstSession = controller.hasSessionOnOutput(tablet->name());
        const bool first = firstCommit && firstReplay && firstCards && firstDeparted
            && firstRemaining && firstSession;
        QList<QPointer<KWin::EffectWindow>> projected;
        const bool grouped = first && controller.transferTabletSessionToCardLine(tablet,
            [&](const auto &projection, const auto &commit) {
                for (const auto &member : projection.panes) projected.append(member.window);
                for (const auto &member : projection.overflow) projected.append(member.window);
                return cards.admitBentoStackToCardLine(projection, commit);
            });
        const bool neighbors = grouped
            && cards.presentation() == Kadunce::CardPresentation::CardLine
            && cards.model().count() == 2 && cards.selectedWindow() == windows[0]
            && cards.bentoProjectionPanes() == projected
            && std::all_of(projected.cbegin(), projected.cend(), [&](const auto &window) {
                const int slotId = cards.paintSlot(window);
                const auto slot = cards.cardTargetForSlot(tablet, slotId);
                const auto work = cards.bentoProjectionWorkspace();
                const auto stored = cards.bentoProjectionRect(window);
                const auto composite = Kadunce::makeBentoCompositeGeometry(
                    {double(slot.x()), double(slot.y()), double(slot.width()), double(slot.height())},
                    {double(work.x()), double(work.y()), double(work.width()), double(work.height())});
                const auto frame = window->frameGeometry();
                const auto expanded = window->expandedGeometry();
                const auto pane = stored ? Kadunce::makeBentoProjectedPaneGeometry(composite,
                    {double(work.x()), double(work.y()), double(work.width()), double(work.height())},
                    *stored,
                    {frame.x(), frame.y(), frame.width(), frame.height()},
                    {expanded.x(), expanded.y(), expanded.width(), expanded.height()}) : std::nullopt;
                return cards.usesBentoProjectionAperture(window)
                    && cards.visibleSlot(window) != 0
                    && slotId != 99 && slotId == cards.visibleSlot(window)
                    && composite.valid() && pane
                    && pane->targetSurface.width > 0 && pane->targetSurface.height > 0
                    && pane->targetClip.width > 0 && pane->targetClip.height > 0;
            });
        cards.release();
        const bool repeatedSetup = neighbors && controller.toggleOnOutput(tablet->name());
        bool lastCommit = repeatedSetup;
        for (int i = 0; i < windows.size(); ++i) {
            auto [nextSource, nextDrop] = prepare(windows[i]);
            if (nextSource) {
                const auto restore = nextSource->restoreSnapshot();
                host.admission = [&, restore](KWin::EffectWindow *window, const auto &commit) {
                    return cards.admitTransferredWindowToTablet(window, commit, {}, &restore);
                };
            }
            lastCommit &= nextSource && nextDrop
                && controller.transferBentoCarryToActive(*nextSource, *nextDrop);
        }
        const bool lastCard = cards.liveCardIndex(windows.last()) >= 0;
        const bool lastSessionGone = !controller.hasSessionOnOutput(tablet->name());
        const bool lastDeparted = !controller.managesWindow(windows.last());
        const bool last = lastCommit && lastCard && lastSessionGone && lastDeparted;
        const bool isolated = controller.outputStageState().filter(other->name() + QLatin1Char('|'))
                == otherBefore.filter(other->name() + QLatin1Char('|'));
        host.admission = {};
        cards.release();
        controller.restoreAllSessions();
        bool restored = true;
        for (const auto &[window, geometry] : sourceOrigins)
            restored &= window && !window->isMinimized() && window->frameGeometry() == geometry;
        host.tablet.clear();
        activeEvidence = QStringLiteral("rejected=%1 stale=%2 first=%3 neighbors=%4 repeated=%5 last=%6 isolated=%7 restored=%8")
            .arg(rejected).arg(stale).arg(first).arg(neighbors).arg(repeatedSetup)
            .arg(last).arg(isolated).arg(restored);
        if (!first) activeEvidence += QStringLiteral(" commit=%1 replay=%2 called=%3 source=%4 cards=%5 index=%6 departed=%7 remaining=%8 session=%9")
            .arg(firstCommit).arg(firstReplay).arg(admissionCalled)
            .arg(sourceCommitAccepted).arg(firstCards).arg(cards.liveCardIndex(windows[0]))
            .arg(firstDeparted).arg(firstRemaining).arg(firstSession);
        if (!last) activeEvidence += QStringLiteral(" lastCommit=%1 lastIndex=%2 lastDeparted=%3 lastSession=%4")
            .arg(lastCommit).arg(lastCard).arg(lastDeparted).arg(lastSessionGone);
        return rejected && stale && first && neighbors && repeatedSetup
            && last && isolated && restored;
    }
    bool edgeBatchAdmission() {
        controller.restoreAllSessions();
        if (!client || !client->window()) return false;
        QList<QPointer<KWin::EffectWindow>> others;
        for (auto *w : KWin::effects->stackingOrder())
            if (w != client && host.isManagedWindowForDesktopStage(w)) others.append(w);
        if (others.size() < 2) return false;
        auto resident = others[0];
        auto witness = others[1];
        auto *origin = client->screen();
        KWin::LogicalOutput *target = nullptr;
        for (auto *o : KWin::effects->screens()) if (o != origin) { target = o; break; }
        if (!target) return false;
        resident->window()->sendToOutput(target);
        witness->window()->sendToOutput(origin);
        const auto residentGeometry = resident->frameGeometry();
        const auto witnessGeometry = witness->frameGeometry();
        const auto clientGeometry = client->frameGeometry();
        const KWin::RectF arrivalGeometry(target->geometry());
        using Intent = Kadunce::DesktopStageController::CardDropIntent;
        int commits = 0, releases = 0;
        const auto rejectedCommit = [&] { ++commits; return false; };
        // Even an explicit activation request must not turn the tablet into a
        // monitor composition session through this transfer entry point.
        host.tablet = target;
        const bool forbidden = controller.transferCardWindow(client, target, arrivalGeometry,
            rejectedCommit, [&] { ++releases; }, Intent::ActivateBento);
        host.tablet = nullptr;
        if (forbidden || commits || releases || controller.hasActiveSession()) return false;
        const bool rejected = controller.transferCardWindow(client, target, arrivalGeometry,
            rejectedCommit, [&] { ++releases; }, Intent::ActivateBento);
        if (rejected || commits != 1 || releases || controller.hasActiveSession()
            || client->screen() != origin || client->frameGeometry() != clientGeometry
            || resident->frameGeometry() != residentGeometry) return false;
        bool ordered = false;
        const bool accepted = controller.transferCardWindow(client, target, arrivalGeometry,
            [&] {
                ++commits;
                return !controller.hasActiveSession() && client->screen() == origin;
            }, [&] {
                ++releases;
                ordered = commits == 2 && controller.hasSessionOnOutput(target->name())
                    && controller.managesWindow(client) && controller.managesWindow(resident)
                    && !controller.managesWindow(witness) && client->screen() == origin;
            }, Intent::ActivateBento);
        const bool correct = accepted && ordered && releases == 1
            && client->screen() == target && !controller.managesWindow(witness)
            && witness->screen() == origin && witness->frameGeometry() == witnessGeometry;
        controller.restoreAllSessions();
        if (!correct) return false;
        // Arrival already belongs to destination output: merge, don't duplicate.
        const bool local = controller.transferCardWindow(client, target, arrivalGeometry,
            [] { return true; }, [] {}, Intent::ActivateBento);
        const bool localCorrect = local && controller.managesWindow(client)
            && controller.managesWindow(resident) && !controller.managesWindow(witness);
        controller.restoreAllSessions();
        return localCorrect;
    }
    bool cardAdmissionOrdering() {
        if (!client || !client->window()) return false;
        QPointer<KWin::EffectWindow> companion;
        for (auto *w : KWin::effects->stackingOrder())
            if (w != client && host.isManagedWindowForDesktopStage(w)) { companion = w; break; }
        if (!companion) return false;
        auto *origin = client->screen();
        KWin::LogicalOutput *target = nullptr;
        for (auto *o : KWin::effects->screens()) if (o != origin) { target = o; break; }
        if (!target) return false;
        companion->window()->sendToOutput(target);
        if (!controller.toggleOnOutput(target->name())) return false;
        const KWin::RectF geometry(target->geometry());
        int commits = 0, releases = 0;
        const bool rejected = controller.transferCardWindow(client, target, geometry,
            [&] { ++commits; return false; }, [&] { ++releases; });
        if (rejected || commits != 1 || releases || controller.managesWindow(client)
            || client->screen() != origin) return false;
        bool ordered = false;
        const bool accepted = controller.transferCardWindow(client, target, geometry,
            [&] { ++commits; return true; }, [&] {
                ++releases;
                ordered = commits == 2 && controller.managesWindow(client);
            });
        const bool managed = controller.managesWindow(client) && client->screen() == target;
        controller.restoreAllSessions();
        if (!accepted || !ordered || !managed || releases != 1) return false;
        // No Bento on origin: the same commit boundary yields a native window.
        const bool native = controller.transferCardWindow(client, origin, KWin::RectF(origin->geometry()),
            [] { return true; }, [&] { ++releases; });
        return native && releases == 2 && !controller.hasActiveSession() && client->screen() == origin;
    }
};

#pragma once
#include "DesktopStageController.h"
#include "CardStageController.h"
#include <effect/effecthandler.h>
#include <core/output.h>
#include <window.h>

// Test-only host. Never installed; all virtual outputs are composition stages.
struct BentoProbeHost final : Kadunce::DesktopStageHost {
    QPointer<KWin::LogicalOutput> tablet;
    std::function<void(KWin::LogicalOutput *)> prepare;
    std::function<std::optional<Kadunce::NativeMoveSnapshot>(KWin::EffectWindow *)> restore;
    std::function<bool(KWin::EffectWindow *, const std::function<bool()> &,
        const Kadunce::NativeMoveSnapshot *)> admission;
    bool isTabletOutputForDesktopStage(const KWin::LogicalOutput *o) const override { return tablet && o == tablet; }
    bool allowsDesktopStageOnOutput(const KWin::LogicalOutput *) const override { return true; }
    bool isManagedWindowForDesktopStage(const KWin::EffectWindow *w) const override {
        return w && !w->isDeleted() && w->isNormalWindow() && w->window();
    }
    KWin::LogicalOutput *tabletOutputForDesktopStage() const override { return tablet; }
    // Matches Effect, which answers this from CardStageController::canOwnCards:
    // the one display that can hold cards is the one card ownership is bound to.
    bool outputCanOwnCards(const KWin::LogicalOutput *o) const override {
        return tablet && o == tablet;
    }
    KWin::Rect activeTargetForDesktopStage(KWin::LogicalOutput *o) const override { return o->geometry(); }
    void prepareOutputForDesktopStage(KWin::LogicalOutput *o) override { if (prepare) prepare(o); }
    std::optional<Kadunce::NativeMoveSnapshot> activeRestoreForDesktopStage(KWin::EffectWindow *w) const override {
        return restore ? restore(w) : std::nullopt;
    }
    bool admitTransferredWindowToTablet(KWin::EffectWindow *w, const std::function<bool()> &commit,
        const Kadunce::NativeMoveSnapshot *restore = nullptr) override {
        return admission && admission(w, commit, restore);
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
            || ownershipCards->presentation() != Kadunce::CardPresentation::Spread) return false;
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
    // CARD-LIFECYCLE.md §5 and §10: a pane carried to the top edge leaves Bento
    // for card ownership as one independent Active card, and §5 ends a layout
    // that falls to one visible pane by giving that pane to card ownership too.
    // Both selection paths, rollback, repeated transitions, release and the
    // other display's isolation are asserted against the production controllers.
    BentoProbeHost extractionDesktopHost;
    TabletProbeHost extractionCardHost;
    std::unique_ptr<Kadunce::DesktopStageController> extractionDesktop;
    std::unique_ptr<Kadunce::CardStageController> extractionCards;
    QPointer<KWin::LogicalOutput> extractionTablet, extractionMonitor;
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> extractionOrigins;
    QString extractionEvidence = QStringLiteral("not started");
    bool extractionFail(const QString &message) { extractionEvidence = message; return false; }

    bool bentoActiveWire() {
        const auto outputs = KWin::effects->screens();
        if (outputs.size() != 2) return extractionFail("Expected two private outputs");
        // The card-owning display is the one the session script gathered the
        // clients onto, so the two names must agree.
        for (auto *output : outputs) {
            if (output->name() == QStringLiteral("Virtual-0")) extractionTablet = output;
            else extractionMonitor = output;
        }
        if (!extractionTablet || !extractionMonitor)
            return extractionFail("Private outputs are not the expected pair");
        extractionDesktopHost.tablet = extractionTablet;
        extractionCardHost.tablet = extractionTablet;
        extractionDesktop = std::make_unique<Kadunce::DesktopStageController>(&extractionDesktopHost);
        extractionCards = std::make_unique<Kadunce::CardStageController>(&extractionCardHost);
        // A sweep of the display releases whatever card ownership holds on it
        // first, exactly as Effect::prepareOutputForDesktopStage does.
        extractionDesktopHost.prepare = [this](auto *output) {
            if (output == extractionTablet) extractionCards->release();
        };
        extractionDesktopHost.restore = [this](auto *w) { return extractionCards->managedRestore(w); };
        extractionDesktopHost.admission = [this](KWin::EffectWindow *window,
            const std::function<bool()> &commit,
            const Kadunce::NativeMoveSnapshot *restore) {
            const auto source = restore ? std::nullopt
                : extractionDesktop->prepareNativeCarrySource(window);
            const auto derived = source
                ? std::optional<Kadunce::NativeMoveSnapshot>(source->restoreSnapshot())
                : std::nullopt;
            if (!restore && derived) restore = &*derived;
            return extractionCards->admitTransferredWindowToTablet(window, commit, QRectF(), restore);
        };
        return true;
    }

    // Whatever the tablet's live session is currently showing.
    QList<QPointer<KWin::EffectWindow>> extractionPanes() const {
        QList<QPointer<KWin::EffectWindow>> panes;
        for (auto *w : KWin::effects->stackingOrder())
            if (w->screen() == extractionTablet && extractionDesktop->managesWindow(w))
                panes.append(w);
        return panes;
    }

    bool bentoActivePair() {
        // The sweep releases card ownership on this display for itself, through
        // the same hook Effect::prepareOutputForDesktopStage uses. Restoring
        // every session instead would take the other display's layout with it.
        if (extractionDesktop->hasSessionOnOutput(extractionTablet->name()))
            return extractionFail("The tablet still had a layout to pair into");
        if (!extractionDesktop->toggleOnOutput(extractionTablet->name()))
            return extractionFail("Tablet Bento activation failed");
        if (extractionPanes().size() != 2)
            return extractionFail(QStringLiteral("Expected two tablet panes, found %1")
                .arg(extractionPanes().size()));
        return true;
    }

    bool bentoActiveAdmission() {
        if (!bentoActiveWire()) return false;
        QList<KWin::EffectWindow *> windows;
        for (auto *w : KWin::effects->stackingOrder())
            if (extractionDesktopHost.isManagedWindowForDesktopStage(w)) windows.append(w);
        if (windows.size() != 3)
            return extractionFail(QStringLiteral("Expected three clients, found %1").arg(windows.size()));
        // One client keeps the other display so its own layout can witness that
        // nothing here reaches across §11's independent ownership sessions.
        QPointer<KWin::EffectWindow> isolated = windows.last();
        isolated->window()->sendToOutput(extractionMonitor);
        for (auto *w : windows) extractionOrigins.append({w, w->window()->moveResizeGeometry()});
        if (!extractionDesktop->toggleOnOutput(extractionMonitor->name())
            || !extractionDesktop->managesWindow(isolated))
            return extractionFail("Monitor layout did not take its own window");
        const auto isolatedTile = isolated->window()->moveResizeGeometry();

        // Two transitions, so a second extraction is proved to work on a layout
        // the first one already tore down and rebuilt.
        for (int round = 0; round < 2; ++round) {
            if (!bentoActivePair()) return false;
            const auto panes = extractionPanes();
            QPointer<KWin::EffectWindow> carried = panes.first();
            QPointer<KWin::EffectWindow> remaining = panes.last();
            const auto carriedTile = carried->window()->moveResizeGeometry();
            const auto remainingTile = remaining->window()->moveResizeGeometry();

            // §14: a refused gesture preserves the exact prior state. The carry
            // is the last thing that can refuse, so nothing may be published.
            if (extractionDesktop->extractPaneToCards(carried, [] { return false; }))
                return extractionFail("A refused extraction reported success");
            if (extractionPanes() != panes
                || carried->window()->moveResizeGeometry() != carriedTile
                || remaining->window()->moveResizeGeometry() != remainingTile
                || extractionCards->managedRestore(carried))
                return extractionFail("A refused extraction changed the layout");

            // The real seam validates a prepared carry source, whose identity
            // is stamped with the application guard's generation. Emulating it
            // here is what makes this an assertion about extraction rather
            // than about a stub: an eviction that claims the guard before
            // reading the carry invalidates the very carry it is committing,
            // which is exactly how a top-edge drag refused itself in the field
            // while this probe passed on a lambda that always agreed.
            const auto prepared = extractionDesktop->prepareNativeCarrySource(carried);
            if (!prepared)
                return extractionFail("No carry source for a live pane");
            if (!extractionDesktop->extractPaneToCards(carried,
                    [&] { return extractionDesktop->nativeCarrySourceValid(*prepared); }))
                return extractionFail("Extraction refused the carry it was committing");
            if (!extractionCards->promoteToActive(carried))
                return extractionFail("The extracted window did not become Active");

            // §5: the layout fell to one visible pane, so Bento ended and that
            // pane became an individual card rather than a desktop window.
            if (extractionDesktop->hasSessionOnOutput(extractionTablet->name()))
                return extractionFail("The tablet kept a layout after its last pane was extracted");
            for (const auto &window : {carried, remaining}) {
                if (!extractionCards->managedRestore(window)
                    || extractionCards->liveCardIndex(window) < 0)
                    return extractionFail(QStringLiteral("%1 did not reach card ownership")
                        .arg(window ? window->caption() : "deleted"));
                if (window->isMinimized())
                    return extractionFail("Leaving Bento put a window to sleep");
            }
            if (extractionCards->presentation() != Kadunce::CardPresentation::Active
                || extractionCards->selectedWindow() != carried)
                return extractionFail("§6: selecting the extracted card did not present it Active");

            // §6's other selection path: the neighbour is its own entry, and
            // choosing it leaves the extracted card owned and hidden.
            if (!extractionCards->promoteToActive(remaining)
                || extractionCards->selectedWindow() != remaining
                || extractionCards->liveCardIndex(carried) < 0
                || !extractionCards->managedRestore(carried))
                return extractionFail("§6: selecting the neighbour released the extracted card");

            // §11: the other display kept its own session and its own geometry.
            if (!extractionDesktop->hasSessionOnOutput(extractionMonitor->name())
                || !extractionDesktop->managesWindow(isolated)
                || isolated->window()->moveResizeGeometry() != isolatedTile)
                return extractionFail("Extraction reached the other display");
        }

        // A window no layout holds has no pane to give up.
        if (extractionDesktop->extractPaneToCards(extractionOrigins.first().first,
                [] { return true; }))
            return extractionFail("Extraction accepted a window no layout owns");

        // §13: release and unload return every window exactly once, to the
        // record it had before any of this, on both displays.
        extractionCards->release();
        extractionDesktop->restoreAllSessions();
        extractionDesktop.reset();
        extractionCards.reset();
        for (const auto &[window, geometry] : extractionOrigins) {
            if (!window || window->isDeleted() || !window->window())
                return extractionFail("Release lost a window");
            if (window->isMinimized() || window->window()->moveResizeGeometry() != geometry)
                return extractionFail(QStringLiteral("Release differs for %1: %2,%3 %4x%5 expected %6,%7 %8x%9")
                    .arg(window->caption())
                    .arg(window->window()->moveResizeGeometry().x())
                    .arg(window->window()->moveResizeGeometry().y())
                    .arg(window->window()->moveResizeGeometry().width())
                    .arg(window->window()->moveResizeGeometry().height())
                    .arg(geometry.x()).arg(geometry.y())
                    .arg(geometry.width()).arg(geometry.height()));
        }
        extractionEvidence = QStringLiteral("Bento-to-Active extraction, teardown, rollback and isolation held");
        return true;
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
        qInfo() << "PASS: production Spread 2D carry preserves pickup and native geometry; cancel resets pose";
        tabletEvidence = QStringLiteral("active=%1 members=%2 source=%3 tablet=%4")
            .arg(cards.isActive()).arg(cards.liveCards().size()).arg(origin->name()).arg(host.tablet->name());
        if (!cards.isActive() || !controller.toggleOnOutput(origin->name())) return false;
        bool ordered = false;
        tabletHost.connected = [&](auto *w) {
            if (w == client) ordered = !controller.managesWindow(w)
                && cards.liveCardIndex(w) >= 0 && w->screen() == origin;
        };
        host.admission = [&](auto *w, const auto &commit, const auto *record) {
            return cards.admitTransferredWindowToTablet(w, commit, {}, record);
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
        host.admission = [](auto *, const auto &, const auto *) { return false; };
        const bool rejected = !controller.handoffLeadToOutput(source, target)
            && controller.managesWindow(client) && client->screen() == origin;
        bool staleRejected = false;
        host.admission = [&](auto *, const auto &commit, const auto *) {
            controller.stopPendingSettle();
            staleRejected = !commit();
            return false;
        };
        const bool stale = !controller.handoffLeadToOutput(source, target)
            && staleRejected && controller.managesWindow(client);
        bool ordered = false;
        host.admission = [&](auto *w, const auto &commit, const auto *) {
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

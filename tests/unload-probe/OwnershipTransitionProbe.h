#pragma once
#include "BentoProbe.h"

// Actual production controllers/native clients, on the private virtual outputs.
struct OwnershipTransitionProbe {
    BentoProbeHost desktopHost;
    TabletProbeHost cardHost;
    Kadunce::DesktopStageController desktop{&desktopHost};
    Kadunce::CardStageController cards{&cardHost};
    QPointer<KWin::LogicalOutput> tablet, monitor;
    QPointer<KWin::EffectWindow> monitorWindow, oversized, cross, immediate, ordinary, projectedLead;
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> origins;
    KWin::RectF monitorTile;
    QString evidence;

    bool fail(const QString &message) { evidence = message; return false; }
    bool setup() {
        const auto outputs = KWin::effects->screens();
        if (outputs.size() != 2) return fail("Expected two private outputs");
        tablet = outputs.first(); monitor = outputs.last();
        desktopHost.tablet = tablet; cardHost.tablet = tablet;
        desktopHost.prepare = [this](auto *output) { if (output == tablet) cards.release(); };
        desktopHost.restore = [this](auto *w) { return cards.managedRestore(w); };
        cardHost.projectionResume = [this](const auto &projection,
            const auto &commit, const auto &release) {
            return desktop.resumeProjectedSession(projection, commit, release);
        };
        QList<KWin::EffectWindow *> windows;
        for (auto *w : KWin::effects->stackingOrder())
            if (desktopHost.isManagedWindowForDesktopStage(w)) windows.append(w);
        if (windows.size() != 3) return fail("Expected three initial clients");
        monitorWindow = windows.last();
        for (int i = 0; i < windows.size(); ++i) {
            auto *client = windows[i]->window();
            auto *output = i == 2 ? monitor.data() : tablet.data();
            client->sendToOutput(output);
            client->maximize(KWin::MaximizeRestore);
            client->moveResize(KWin::RectF(output->geometry().x() + 80 + i * 25, 90, 500, 400));
        }
        return true;
    }
    bool begin() {
        for (auto *w : KWin::effects->stackingOrder()) {
            if (!desktopHost.isManagedWindowForDesktopStage(w)) continue;
            origins.append({w, w->window()->moveResizeGeometry()});
            // Wire the native minimized transition just like Effect.
            QObject::connect(w->window(), &KWin::Window::minimizedChanged, w,
                [this, w] { desktop.handleWindowMinimizedChanged(w); });
        }
        if (!desktop.toggleOnOutput(monitor->name()) || !desktop.toggleOnOutput(tablet->name()))
            return fail("Initial Bento activation failed");
        monitorTile = monitorWindow->window()->moveResizeGeometry();
        // Rejected projection must leave both owners untouched.
        if (desktop.transferTabletSessionToCardLine(tablet, [](const auto &, const auto &) { return false; })
            || !desktop.hasSessionOnOutput(tablet->name())) return fail("Rejected projection destroyed Bento");
        if (desktop.transferTabletSessionToCardLine(monitor, [](const auto &, const auto &commit) { return commit(); }))
            return fail("Monitor exposed Card Line projection");
        return true;
    }
    bool arrival(bool tooLarge) {
        KWin::EffectWindow *arrival = nullptr;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->caption().contains(tooLarge ? "Oversized ownership" : "Large admission")) arrival = w;
        }
        if (!arrival || !arrival->window() || arrival->screen() != tablet) return fail("Arrival missing/wrong output");
        origins.append({arrival, arrival->window()->moveResizeGeometry()});
        QObject::connect(arrival->window(), &KWin::Window::minimizedChanged, arrival,
            [this, arrival] { desktop.handleWindowMinimizedChanged(arrival); });
        if (!desktop.handleWindowAdded(arrival) || !desktop.ownsWindow(arrival))
            return fail("Bento reported handled without owning arrival");
        if (!desktop.handleWindowAdded(arrival)) return fail("Duplicate arrival not idempotent");
        if (tooLarge) oversized = arrival;
        else if (arrival->isMinimized() || arrival->window()->moveResizeGeometry().width() < 1000)
            return fail("Constrained arrival did not receive large pane");
        return true;
    }
    bool prepared() {
        if (!oversized || !oversized->isMinimized() || !desktop.ownsWindow(oversized))
            return fail("Oversized arrival did not become owned prepared overflow");
        const auto accepted = oversized->window()->moveResizeGeometry();
        if (accepted != KWin::RectF(desktopHost.activeTargetForDesktopStage(tablet))
            || oversized->frameGeometry() != accepted)
            return fail("Prepared overflow did not acknowledge the established Active target");
        return true;
    }
    bool crossPrepare() {
        for (auto *w : KWin::effects->stackingOrder())
            if (w->caption().contains("Cross ownership")) cross = w;
        if (!cross) return fail("Cross arrival missing");
        cross->window()->sendToOutput(monitor);
        return true;
    }
    bool crossAdmit() {
        const auto source = desktop.prepareNativeCarrySource(cross);
        if (!source || !source->isDesktopWindow()) return fail("Ordinary monitor source missing");
        const auto saved = source->restoreSnapshot();
        const auto target = KWin::RectF(tablet->geometry());
        if (desktop.transferCardWindow(cross, tablet, target, [] { return false; }, [] {},
                Kadunce::DesktopStageController::CardDropIntent::OpenSpace, &saved)
            || cross->screen() != monitor || cross->window()->moveResizeGeometry() != saved.geometry
            || desktop.ownsWindow(cross)) return fail("Rejected cross admission mutated source");
        const auto drop = desktop.prepareCardDrop(cross, tablet, target);
        if (!drop || !desktop.transferNativeCarryToDesktop(*source, *drop)
            || !desktop.ownsWindow(cross)) return fail("Committed cross admission failed");
        return true;
    }
    bool project() {
        QPointer<KWin::EffectWindow> expectedLead;
        double largest = 0;
        QList<QPointer<KWin::EffectWindow>> visible;
        for (auto *w : KWin::effects->stackingOrder())
            if (w->screen() == tablet && desktop.managesWindow(w)) visible.append(w);
        for (const auto &w : visible) {
            if (w->isMinimized()) continue;
            const auto frame = w->window()->moveResizeGeometry();
            const double area = frame.width() * frame.height();
            if (area > largest) { largest = area; expectedLead = w; }
        }
        Kadunce::BentoProjectionSession transferred;
        if (!desktop.transferTabletSessionToCardLine(tablet, [this, &transferred](const auto &projection, const auto &commit) {
                transferred = projection;
                if (cross) {
                    QList<Kadunce::BentoProjectionMember> members = projection.panes;
                    members.append(projection.overflow);
                    const auto incoming = std::find_if(members.cbegin(), members.cend(),
                        [this](const auto &s) { return s.window == cross; });
                    if (incoming == members.cend() || incoming->restore.output != tablet
                        || !tablet->geometry().contains(incoming->restore.geometry.center().toPoint())
                        || incoming->restore.geometry.width() != 400 || incoming->restore.geometry.height() != 300)
                        return fail("Bento cross admission retained monitor origin/changed ordinary size");
                    if (std::none_of(origins.cbegin(), origins.cend(), [this](const auto &p) { return p.first == cross; }))
                        origins.append({cross, incoming->restore.geometry});
                }
                return cards.admitBentoStack(projection, commit);
            })) return fail("Tablet projection rejected");
        if (desktop.hasSessionOnOutput(tablet->name()) || !desktop.hasSessionOnOutput(monitor->name())
            || monitorWindow->window()->moveResizeGeometry() != monitorTile)
            return fail("Projection retained duplicate owner or changed monitor");
        if (cards.model().count() != 1
            || cards.liveCards().size() != transferred.panes.size() + transferred.overflow.size()
            || cards.selectedWindow() != transferred.lead
            || cards.selectedWindow() != expectedLead)
            return fail("Projection lost membership/large-pane selection");
        projectedLead = transferred.lead;
        QList<Kadunce::BentoProjectionMember> members = transferred.panes;
        members.append(transferred.overflow);
        for (const auto &saved : members) {
            if (!cards.usesBentoProjectionAperture(saved.window))
                return fail("Projection member lost Bento presentation provenance");
            const auto retained = cards.managedRestore(saved.window);
            if (!retained || retained->geometry != saved.restore.geometry
                || retained->minimized != saved.restore.minimized)
                return fail("Projection replaced authoritative restore record");
            if (saved.window->isMinimized() != saved.minimized)
                return fail("Projection changed pane/overflow minimization");
        }
        for (const auto &[w, geometry] : origins) {
            if (w == monitorWindow || w == ordinary) continue;
            const auto retained = cards.managedRestore(w);
            if (!retained || retained->geometry != geometry) return fail("Round trip lost ordinary origin");
        }
        const auto selectedBeforeBrowse = cards.selectedWindow();
        cards.pageStack(1);
        cards.pageStack(-1);
        if (cards.selectedWindow() != selectedBeforeBrowse)
            return fail("Bento group allowed member paging");
        return true;
    }
    bool ordinaryNeighbor() {
        for (auto *w : KWin::effects->stackingOrder())
            if (w->caption().contains("Ordinary neighbor")) ordinary = w;
        if (!ordinary || !ordinary->window() || ordinary->screen() != tablet)
            return fail("Ordinary Card Line neighbor missing/wrong output");
        origins.append({ordinary, ordinary->window()->moveResizeGeometry()});
        if (!cards.handleWindowAdded(ordinary) || cards.model().count() != 2)
            return fail("Ordinary neighbor did not coexist with Bento group card");
        if (cards.selectedWindow() != projectedLead) cards.pageHorizontal(-1);
        if (cards.selectedWindow() != projectedLead) cards.pageHorizontal(1);
        if (cards.selectedWindow() != projectedLead
            || cards.visibleSlot(ordinary) == 0
            || cards.visibleSlot(projectedLead) != 0)
            return fail("Bento group did not remain one ordinary Card Line neighbor");
        const auto ordinaryRestore = cards.managedRestore(ordinary);
        if (!ordinaryRestore) return fail("Ordinary neighbor lost its Card Line restore");
        cardHost.projectionResume = [this](const auto &projection,
            const auto &, const auto &release) {
            return desktop.resumeProjectedSession(projection, [] { return false; }, release);
        };
        if (cards.resumeSelectedBentoProjection() || !cards.isActive()
            || cards.model().count() != 2 || !cards.managedRestore(ordinary)
            || cards.managedRestore(ordinary)->geometry != ordinaryRestore->geometry
            || desktop.hasSessionOnOutput(tablet->name()))
            return fail("Rejected exact resume mutated group or ordinary neighbor ownership");
        cardHost.projectionResume = [this](const auto &projection,
            const auto &commit, const auto &release) {
            return desktop.resumeProjectedSession(projection, commit, release);
        };
        return true;
    }
    bool returnToBento() {
        if (!cards.resumeSelectedBentoProjection() || cards.isActive()
            || !desktop.hasSessionOnOutput(tablet->name())) return fail("Return to Bento failed");
        for (const auto &[w, geometry] : origins) {
            if (w == ordinary) {
                if (desktop.ownsWindow(w) || w->frameGeometry() != geometry)
                    return fail("Exact Bento resume consumed or displaced ordinary neighbor");
                continue;
            }
            if (!desktop.ownsWindow(w)) return fail("Return dropped an owned member");
            if (cards.usesBentoProjectionAperture(w))
                return fail("Released Card Line retained projected presentation provenance");
        }
        return true;
    }
    bool release() {
        cards.release();
        desktop.restoreAllSessions();
        return true;
    }
    bool immediatePlace() {
        for (auto *w : KWin::effects->stackingOrder())
            if (w->caption().contains("Immediate ownership")) immediate = w;
        if (!immediate) return fail("Immediate arrival missing");
        immediate->window()->moveResize(KWin::RectF(desktopHost.activeTargetForDesktopStage(tablet)));
        return true;
    }
    bool immediateRelease() {
        const auto ordinary = immediate->window()->moveResizeGeometry();
        origins.append({immediate, ordinary});
        if (!desktop.handleWindowAdded(immediate) || !desktop.ownsWindow(immediate))
            return fail("Immediate arrival unowned");
        desktop.restoreAllSessions();
        return true;
    }
    bool reactivate() { return desktop.toggleOnOutput(tablet->name()); }
    bool restored() {
        for (const auto &[w, geometry] : origins) {
            if (!w || w->isMinimized() || w->frameGeometry() != geometry)
                return fail(QStringLiteral("Release differs for %1: %2,%3 %4x%5 expected %6,%7 %8x%9 minimized %10")
                    .arg(w ? w->caption() : "deleted")
                    .arg(w ? w->frameGeometry().x() : 0).arg(w ? w->frameGeometry().y() : 0)
                    .arg(w ? w->frameGeometry().width() : 0).arg(w ? w->frameGeometry().height() : 0)
                    .arg(geometry.x()).arg(geometry.y()).arg(geometry.width()).arg(geometry.height())
                    .arg(w && w->isMinimized()));
        }
        return true;
    }
};

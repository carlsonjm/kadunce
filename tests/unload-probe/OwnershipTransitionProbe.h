#pragma once
#include "BentoProbe.h"

// Actual production controllers/native clients, on the private virtual outputs.
struct OwnershipTransitionProbe {
    BentoProbeHost desktopHost;
    TabletProbeHost cardHost;
    Kadunce::DesktopStageController desktop{&desktopHost};
    Kadunce::CardStageController cards{&cardHost};
    QPointer<KWin::LogicalOutput> tablet, monitor;
    QPointer<KWin::EffectWindow> monitorWindow, tabletResident, oversized, cross, immediate, ordinary, projectedLead;
    QPointer<KWin::EffectWindow> displaced;
    int entriesWithGroup = 0;
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> origins;
    QList<QPointer<KWin::EffectWindow>> currentProjectionWindows, lastRetiredProjection;
    QList<QPair<QPointer<KWin::EffectWindow>, KWin::RectF>> projectedPaneFrames;
    int projectionRetirements = 0;
    bool projectionStateClearedAtRetirement = false;
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
        // Without this the host refuses every eviction, which reads as §5's
        // "where no display can hold a card, nothing leaves" and makes a full
        // layout untestable. Effect::admitTransferredWindowToTablet resolves
        // the restore record the same way: a caller still holding the window's
        // session record supplies it, and otherwise the desktop stage is asked
        // before it publishes a plan that no longer names the window.
        desktopHost.admission = [this](KWin::EffectWindow *window,
            const std::function<bool()> &commit,
            const Kadunce::NativeMoveSnapshot *restore) {
            const auto source = restore ? std::nullopt
                : desktop.prepareNativeCarrySource(window);
            const auto derived = source
                ? std::optional<Kadunce::NativeMoveSnapshot>(source->restoreSnapshot())
                : std::nullopt;
            if (!restore && derived) restore = &*derived;
            return cards.admitTransferredWindowToTablet(window, commit, QRectF(), restore);
        };
        cardHost.projectionResume = [this](const auto &projection,
            const auto &commit, const auto &release) {
            return desktop.resumeProjectedSession(projection, commit, release);
        };
        cardHost.projectionRetired = [this](const auto &windows) {
            ++projectionRetirements;
            lastRetiredProjection = windows;
            projectionStateClearedAtRetirement = std::all_of(
                windows.cbegin(), windows.cend(), [this](const auto &window) {
                    return !cards.usesBentoProjectionAperture(window);
                });
        };
        QList<KWin::EffectWindow *> windows;
        for (auto *w : KWin::effects->stackingOrder())
            if (desktopHost.isManagedWindowForDesktopStage(w)) windows.append(w);
        if (windows.size() != 3) return fail("Expected three initial clients");
        monitorWindow = windows.last();
        for (int i = 0; i < windows.size(); ++i) {
            auto *client = windows[i]->window();
            // CARD-LIFECYCLE.md §8 admits a launch only where the layout can
            // grow to show it. The tablet's maximum is two panes, so one
            // resident leaves exactly one pane for growth: the constrained
            // launch is admitted and the oversized one is refused, which is
            // §8 in both directions against one layout. Two residents would
            // put the tablet at its maximum before either launch arrived and
            // admitting one would mean displacing the other.
            auto *output = i == 0 ? tablet.data() : monitor.data();
            if (i == 0) tabletResident = windows[i];
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
        if (desktop.transferTabletSessionToSpread(tablet, [](const auto &, const auto &) { return false; })
            || !desktop.hasSessionOnOutput(tablet->name())) return fail("Rejected projection destroyed Bento");
        if (desktop.transferTabletSessionToSpread(monitor, [](const auto &, const auto &commit) { return commit(); }))
            return fail("Monitor exposed Spread projection");
        return true;
    }
    bool arrival(bool tooLarge) {
        KWin::EffectWindow *arrival = nullptr;
        for (auto *w : KWin::effects->stackingOrder()) {
            if (w->caption().contains(tooLarge ? "Oversized ownership" : "Pane admission")) arrival = w;
        }
        if (!arrival || !arrival->window() || arrival->screen() != tablet) return fail("Arrival missing/wrong output");
        origins.append({arrival, arrival->window()->moveResizeGeometry()});
        QObject::connect(arrival->window(), &KWin::Window::minimizedChanged, arrival,
            [this, arrival] { desktop.handleWindowMinimizedChanged(arrival); });
        if (tooLarge) {
            // CARD-LIFECYCLE.md §8: a layout that cannot grow to show a launch
            // refuses it and leaves it for card ownership. Refusal takes
            // nothing, so it is idempotent the way admission is.
            if (desktop.handleWindowAdded(arrival) || desktop.ownsWindow(arrival))
                return fail("Bento admitted a launch it cannot show");
            if (desktop.handleWindowAdded(arrival))
                return fail("Refusing an unshowable launch was not idempotent");
            oversized = arrival;
            return true;
        }
        // §8 the other way: the layout has one pane and a cap of two, so it can
        // grow to show this launch beside the resident. Growth is the whole
        // claim, so the resident must still be a pane afterwards.
        if (!desktop.handleWindowAdded(arrival) || !desktop.ownsWindow(arrival))
            return fail("Bento refused a launch the layout could grow to show");
        if (!desktop.handleWindowAdded(arrival)) return fail("Duplicate arrival not idempotent");
        if (!tabletResident || !desktop.ownsWindow(tabletResident))
            return fail("Growth displaced the resident instead of growing beside it");
        if (arrival->isMinimized() || tabletResident->isMinimized())
            return fail("Growth put a pane to sleep rather than showing both");
        const auto grown = arrival->window()->moveResizeGeometry();
        const auto resident = tabletResident->window()->moveResizeGeometry();
        // Taking the display is what displacement looked like before §8 became
        // growth-only, so the pane must be narrower than the output it sits on.
        if (grown.width() >= tablet->geometry().width())
            return fail("Constrained arrival took the display instead of a pane");
        // Only the larger pane satisfies this launch's minimum, so assignment
        // followed the minimum rather than arrival order.
        if (grown.width() <= resident.width())
            return fail("Constrained arrival did not receive the larger pane");
        return true;
    }
    bool refused() {
        if (!oversized) return fail("Oversized arrival missing");
        // §8 forbids parking: the window stays awake and unowned here, which is
        // what lets the host give it to card ownership as an Active card.
        if (oversized->isMinimized())
            return fail("A launch Bento cannot show was minimized rather than left awake");
        if (desktop.ownsWindow(oversized) || desktop.managesWindow(oversized))
            return fail("A launch Bento cannot show was retained beside the session");
        if (!desktop.hasSessionOnOutput(tablet->name()))
            return fail("A refused launch ended the layout it could not join");
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
        // The layout is at its two-pane cap, so §5's displacement is what this
        // arrival exercises. Record the panes first; the one that stops being
        // owned afterwards is the pane that yielded.
        QList<QPointer<KWin::EffectWindow>> paneesBefore;
        for (auto *w : KWin::effects->stackingOrder())
            if (w->screen() == tablet && desktop.ownsWindow(w)) paneesBefore.append(w);
        const auto drop = desktop.prepareCardDrop(cross, tablet, target);
        if (!drop || !desktop.transferNativeCarryToDesktop(*source, *drop)
            || !desktop.ownsWindow(cross)) return fail("Committed cross admission failed");
        for (const auto &w : paneesBefore) {
            if (!w || desktop.ownsWindow(w)) continue;
            if (displaced) return fail("A full layout displaced more than one pane");
            displaced = w;
        }
        if (!displaced) return fail("Arriving at a full layout displaced nothing");
        // §5: leaving Bento is not a minimize, and the pane that yields becomes
        // an individual card rather than an unowned desktop window.
        if (displaced->isMinimized())
            return fail("A displaced pane was minimized rather than left awake");
        if (!cards.managedRestore(displaced))
            return fail("A displaced pane did not reach card ownership");
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
        // §5 already gave the displaced pane to card ownership, so the card
        // stage holds individual cards before the group card arrives. Each is
        // ungrouped, so its entry count and live-card count agree, and the
        // projection must add exactly one entry on top of them.
        const int individualsBefore = cards.model().count();
        if (cards.liveCards().size() != individualsBefore)
            return fail("Displaced panes did not reach card ownership as ungrouped cards");
        Kadunce::BentoProjectionSession transferred;
        if (!desktop.transferTabletSessionToSpread(tablet, [this, &transferred](const auto &projection, const auto &commit) {
                transferred = projection;
                if (cross) {
                    QList<Kadunce::BentoProjectionMember> members = projection.panes;
                    members.append(projection.sleeping);
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
        if (cards.model().count() != individualsBefore + 1
            || cards.liveCards().size() != individualsBefore + transferred.panes.size()
                + transferred.sleeping.size()
            || cards.selectedWindow() != transferred.lead
            || cards.selectedWindow() != expectedLead)
            return fail("Projection lost membership/large-pane selection");
        entriesWithGroup = cards.model().count();
        projectedLead = transferred.lead;
        QList<Kadunce::BentoProjectionMember> members = transferred.panes;
        members.append(transferred.sleeping);
        projectedPaneFrames.clear();
        for (const auto &pane : transferred.panes)
            projectedPaneFrames.append({pane.window, pane.window->frameGeometry()});
        currentProjectionWindows.clear();
        for (const auto &saved : members) {
            currentProjectionWindows.append(saved.window);
            if (!cards.usesBentoProjectionAperture(saved.window))
                return fail("Projection member lost Bento presentation provenance");
            const auto retained = cards.managedRestore(saved.window);
            if (!retained || retained->geometry != saved.restore.geometry
                || retained->minimized != saved.restore.minimized)
                return fail("Projection replaced authoritative restore record");
            if (saved.window->isMinimized() != saved.minimized)
                return fail("Projection changed a member's minimization");
        }
        // §5 keeps a restore record so release returns a window where it began.
        // Only what card ownership holds carries one: the projected members and
        // the pane the cross arrival displaced. A launch Bento refused and the
        // monitor's own panes are not card-owned, and a tablet projection that
        // gave either one a record would be reaching across the boundary.
        for (const auto &[w, geometry] : origins) {
            if (w == monitorWindow || w == ordinary) continue;
            const auto retained = cards.managedRestore(w);
            const bool cardOwned = currentProjectionWindows.contains(w) || w == displaced;
            if (!cardOwned) {
                if (retained)
                    return fail(QStringLiteral("Card ownership retained %1, which it does not own")
                        .arg(w ? w->caption() : "deleted"));
                continue;
            }
            if (!retained)
                return fail(QStringLiteral("Round trip lost the origin of %1")
                    .arg(w ? w->caption() : "deleted"));
            if (retained->geometry != geometry)
                return fail(QStringLiteral("Round trip changed %1's origin to %2,%3 %4x%5 from %6,%7 %8x%9")
                    .arg(w ? w->caption() : "deleted")
                    .arg(retained->geometry.x()).arg(retained->geometry.y())
                    .arg(retained->geometry.width()).arg(retained->geometry.height())
                    .arg(geometry.x()).arg(geometry.y())
                    .arg(geometry.width()).arg(geometry.height()));
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
            return fail("Ordinary Spread neighbor missing/wrong output");
        origins.append({ordinary, ordinary->window()->moveResizeGeometry()});
        if (!cards.handleWindowAdded(ordinary) || cards.model().count() != entriesWithGroup + 1)
            return fail("Ordinary neighbor did not coexist with Bento group card");
        if (cards.selectedWindow() != projectedLead) cards.pageHorizontal(-1);
        if (cards.selectedWindow() != projectedLead) cards.pageHorizontal(1);
        if (cards.selectedWindow() != projectedLead
            || cards.visibleSlot(ordinary) == 0
            || cards.visibleSlot(projectedLead) != 0)
            return fail("Bento group did not remain one ordinary Spread neighbor");
        const auto ordinaryRestore = cards.managedRestore(ordinary);
        if (!ordinaryRestore) return fail("Ordinary neighbor lost its Spread restore");
        const int retirementCount = projectionRetirements;
        cardHost.projectionResume = [this](const auto &projection,
            const auto &, const auto &release) {
            return desktop.resumeProjectedSession(projection, [] { return false; }, release);
        };
        if (cards.resumeSelectedBentoProjection() || !cards.isActive()
            || cards.model().count() != entriesWithGroup + 1 || !cards.managedRestore(ordinary)
            || cards.managedRestore(ordinary)->geometry != ordinaryRestore->geometry
            || desktop.hasSessionOnOutput(tablet->name())
            || projectionRetirements != retirementCount
            || std::any_of(currentProjectionWindows.cbegin(), currentProjectionWindows.cend(),
                [this](const auto &window) {
                    return !cards.usesBentoProjectionAperture(window);
                }))
            return fail("Rejected exact resume mutated group or ordinary neighbor ownership");
        cardHost.projectionResume = [this](const auto &projection,
            const auto &commit, const auto &release) {
            return desktop.resumeProjectedSession(projection, commit, release);
        };
        return true;
    }
    // A pane's own client can change its frame while the group is projected: a
    // terminal reflowing, or a scale change re-rounding one edge. §6 resumes the
    // group whatever the panes did meanwhile, so this states the drift really
    // happened before the resume is asked to survive it.
    bool paneDrift() {
        if (projectedPaneFrames.isEmpty()) return fail("No projected panes to drift");
        int moved = 0;
        for (const auto &[window, frame] : projectedPaneFrames) {
            if (!window || window->isDeleted()) return fail("Projected pane disappeared");
            if (window->frameGeometry() != frame) ++moved;
        }
        if (moved != 1) {
            QStringList seen;
            for (const auto &[window, frame] : projectedPaneFrames)
                seen.append(QStringLiteral("%1 %2x%3 -> %4x%5").arg(window->caption())
                    .arg(frame.width()).arg(frame.height())
                    .arg(window->frameGeometry().width()).arg(window->frameGeometry().height()));
            return fail(QStringLiteral("Expected one pane to leave the layout; %1 did: %2")
                .arg(moved).arg(seen.join(QStringLiteral("; "))));
        }
        return true;
    }
    // Resuming a drifted pane is only worth anything if the pane is also put
    // back: a session whose panes do not match its rects is torn down by the
    // next settle, which would trade a refusal for a layout that vanishes.
    bool panesReplaced() {
        for (const auto &[window, frame] : projectedPaneFrames) {
            if (!window || window->isDeleted()) return fail("Resumed pane disappeared");
            if (window->frameGeometry() != frame)
                return fail(QStringLiteral("%1 stayed off the layout at %2x%3 instead of %4x%5")
                    .arg(window->caption())
                    .arg(window->frameGeometry().width()).arg(window->frameGeometry().height())
                    .arg(frame.width()).arg(frame.height()));
        }
        return true;
    }
    bool returnToBento() {
        const int retirementCount = projectionRetirements;
        if (!cards.resumeSelectedBentoProjection())
            return fail("Selecting the Bento group did not resume it");
        if (!desktop.hasSessionOnOutput(tablet->name()))
            return fail("Resume did not return the layout to the tablet");
        // §6: only the group's own members leave this stage. Cards beside it
        // keep their ownership, so the stage stops presenting only when the
        // group was all it held. §2 then hides what remains behind Bento
        // rather than painting it over the resumed panes.
        const bool holdsCards = cards.model().count() > 0;
        if (cards.isActive() != holdsCards)
            return fail(QStringLiteral("Card stage presentation did not follow what it still owns; entries %1 active %2")
                .arg(cards.model().count()).arg(cards.isActive()));
        if (holdsCards && cards.presentation() != Kadunce::CardPresentation::Bento)
            return fail("Cards beside the group kept presenting Spread over the resumed panes");
        if (projectionRetirements != retirementCount + 1
            || !projectionStateClearedAtRetirement
            || lastRetiredProjection.size() != currentProjectionWindows.size()
            || std::any_of(currentProjectionWindows.cbegin(), currentProjectionWindows.cend(),
                [this](const auto &window) {
                    return !lastRetiredProjection.contains(window);
                })) {
            return fail("Successful exact resume did not retire every projected paint source");
        }
        for (const auto &[w, geometry] : origins) {
            // §6 keeps a card beside the group owned by this stage, with its
            // parked record intact, rather than returning it to the desktop.
            // §5 additionally gives a displaced pane no retained association
            // with the group, so a resume must not pull it back into Bento.
            if (w == ordinary || w == displaced) {
                if (desktop.ownsWindow(w))
                    return fail(QStringLiteral("Exact Bento resume reclaimed %1")
                        .arg(w ? w->caption() : "deleted"));
                if (!cards.managedRestore(w))
                    return fail(QStringLiteral("%1 lost its card restore record across the resume")
                        .arg(w ? w->caption() : "deleted"));
                continue;
            }
            // §8 left the refused launch awake and unowned. Resuming a layout
            // is not an admission, so nothing here may sweep it in.
            if (w == oversized) {
                if (desktop.ownsWindow(w))
                    return fail("Exact Bento resume adopted the launch the layout refused");
                if (w->isMinimized())
                    return fail("The refused launch was put to sleep by a later transition");
                continue;
            }
            if (!desktop.ownsWindow(w)) return fail("Return dropped an owned member");
            if (cards.usesBentoProjectionAperture(w))
                return fail("Released Spread retained projected presentation provenance");
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
        // This launch's minimum is larger than the display, so no layout can
        // grow to show it and §8 refuses it. What made release dangerous here
        // was the parking minimize that used to follow such an arrival: it was
        // scheduled against a session that release was about to retire. With
        // overflow deleted nothing schedules one, so the window must be awake
        // both before and after a release that lands immediately after it.
        if (desktop.handleWindowAdded(immediate) || desktop.ownsWindow(immediate))
            return fail("Bento admitted a launch larger than the display");
        if (immediate->isMinimized())
            return fail("A refused launch was minimized rather than left awake");
        // Reactivating swept a display holding more windows than its two-pane
        // cap, so §5 gave the remainder to card ownership. Both owners hold
        // windows here, and only releasing both returns the display, in the
        // order Effect::release uses.
        desktop.restoreAllSessions();
        cards.release();
        if (immediate->isMinimized())
            return fail("A delayed minimize outlived the session that scheduled it");
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

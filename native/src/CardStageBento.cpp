/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Bento and Stacks on the card stage, release, and cards moving between
// displays (docs/CARD-LIFECYCLE.md).
#include "CardStageInternal.h"

namespace Kadunce
{

void CardStageController::toggle(bool)
{
    stopOpeningSpread();
    m_host->cancelInputForCardStage();
    dropRowMotion();
    if (m_lift.phase != Lift::Phase::None && m_lift.phase != Lift::Phase::Throw) endLift();
    if (m_active) {
        finishCardGrab(false);
        if (m_presentation == CardPresentation::Desktop) {
            // The cards come back into the row, and going back returns here.
            m_presentation = CardPresentation::Spread;
            m_returnToDesktop = true;
            bringCardsBack();
            m_host->setPagingShortcutsForCardStage(true);
        } else if (m_presentation == CardPresentation::Spread) {
            // The chosen card grows to its Active place first, as a new app's
            // card does, and the arrival timer enters Active when it is there.
            if (growSelectedToActive()) return;
            if (!enterActive()) {
                return;
            }
        } else {
            parkActiveSnapshot();
            m_presentation = CardPresentation::Spread;
            formRowFromActive();
        }
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce" << Revision << "changed to"
                << (m_presentation == CardPresentation::Active
                        ? "Active" : "Spread");
        return;
    }

    m_restoredMinimizations.clear();
    rebuildLiveCards();
    if (m_workspace.windows().isEmpty()) {
        qWarning() << "Kadunce" << Revision
                   << "has no eligible live window on the tablet";
        return;
    }
    m_active = true;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "activated with"
            << m_workspace.windows().size() << "live tablet cards; selected"
            << m_workspace.selectedIndex() + 1;
}

bool CardStageController::admitBentoStack(const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    // §2: the layout's cards join whatever individual cards this stage
    // already owns, so an active stage is not a reason to refuse them.
    if (m_cardGrabActive || m_launcherGuestActive
        || !tablet || projection.output != tablet
        || projection.outputName != tablet->name()
        || !tablet->geometry().contains(projection.workspaceArea)
        || !validBentoProjectionShape(projectionShape(projection))) return false;
    QList<QPointer<KWin::EffectWindow>> windows;
    QList<ActiveRestoreSnapshot> snapshots;
    const auto append = [&](const BentoProjectionMember &member, bool visible) {
        const auto &saved = member.restore;
        auto *client = saved.window.data();
        if (!client || client->isDeleted() || client->output() != tablet
            || !client->effectWindow() || !saved.geometry.isValid()
            || client->effectWindow() != member.window
            || member.window->isMinimized() != member.minimized
            || (visible && member.minimized)
            || (!visible && !member.minimized)) return false;
        auto *window = client->effectWindow();
        windows.append(window);
        snapshots.append({.window = window, .geometry = saved.geometry,
            .floatingGeometry = saved.floatingGeometry,
            .fullscreenRestoreGeometry = saved.fullscreenRestoreGeometry,
            .quickTileMode = saved.quickTileMode, .maximizeMode = saved.maximizeMode,
            .fullScreen = saved.fullScreen, .minimized = saved.minimized, .valid = true,
            .outputName = outputNameOf(saved.output.data()),
            .outputGeometry = outputGeometryOf(saved.output.data())});
        return true;
    };
    // Each pane is a card of its own in the row (§11, A layout in Spread),
    // standing beside the others in the order the layout shows them, left to
    // right and then top to bottom, so a pair reads as it stood.
    QList<int> paneOrder;
    for (int i = 0; i < projection.panes.size(); ++i) paneOrder.append(i);
    if (projection.rects.size() == std::size_t(projection.panes.size())) {
        std::stable_sort(paneOrder.begin(), paneOrder.end(), [&](int a, int b) {
            const auto &first = projection.rects[std::size_t(a)];
            const auto &second = projection.rects[std::size_t(b)];
            return std::abs(first.x - second.x) > 1e-3 ? first.x < second.x : first.y < second.y;
        });
    }
    QList<QPointer<KWin::EffectWindow>> ordered;
    for (const int i : std::as_const(paneOrder)) ordered.append(projection.panes.at(i).window);
    for (const auto &member : projection.sleeping) ordered.append(member.window);
    for (const auto &window : std::as_const(ordered)) {
        const auto pane = std::find_if(projection.panes.cbegin(), projection.panes.cend(),
            [&](const auto &member) { return member.window == window; });
        if (pane != projection.panes.cend()) {
            if (!append(*pane, true)) return false;
            continue;
        }
        const auto sleeping = std::find_if(projection.sleeping.cbegin(), projection.sleeping.cend(),
            [&](const auto &member) { return member.window == window; });
        if (sleeping == projection.sleeping.cend() || !append(*sleeping, false)) return false;
    }
    const auto admission = m_workspace.prepareRowAdmission(windows);
    if (!admission) return false;
    // Both membership owners publish before signals/native calls. Original
    // restore records cross directly; no frame acknowledgement is required.
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitAdmission(*admission, commitSource); },
            [&] {
                for (const auto &snapshot : std::as_const(snapshots)) {
                    m_parkedRestores.removeIf([&](const auto &existing) {
                        return !existing.window || existing.window == snapshot.window; });
                    m_parkedRestores.append(snapshot);
                }
                m_bentoProjectionWindows = windows;
                m_bentoProjectionPaneWindows.clear();
                for (const auto &member : projection.panes)
                    m_bentoProjectionPaneWindows.append(member.window);
                m_bentoProjectionSession = projection;
                ++m_restoreGeneration;
                m_active = true;
                // The stage can already present an individual Active card here:
                // §5 gives a displaced pane to card ownership, and the group is
                // admitted beside it. Discarding the record instead of parking
                // it would leave that window with the geometry Bento gave it,
                // so release could no longer return it where it began.
                parkActiveSnapshot();
                m_presentation = CardPresentation::Spread;
                // The person came from the layout, so going back resumes it.
                m_returnToGroup = true;
                for (const auto &window : std::as_const(projection.stackingOrder)) {
                    m_originalCardStackingOrder.removeAll(window);
                    m_originalCardStackingOrder.append(window);
                }
                m_restoredMinimizations.clear();
            })) {
        return false;
    }
    m_host->cancelInputForCardStage();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const auto &window : windows) {
        if (!m_active) return true;
        if (window && !window->isDeleted() && window->window()) {
            m_host->connectManagedWindowForCardStage(window);
            // Each member's own visibility is part of the transferred session.
            // Spread never wakes a sleeping member merely to present the group.
        }
    }
    if (!m_active) return true;
    m_host->setPagingShortcutsForCardStage(true);
    // The row opens on the pane that was in front, each pane a card of its
    // own, and each pane's Stack whole around it.
    settlePaneStacks();
    if (projection.lead) (void)selectCardEntry(projection.lead);
    // The pane in front shrinks from where it stood into its card in the row,
    // as the Active card does, and the cards beside it slide in.
    formRowFromActive();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::hasBentoGroup() const
{
    return m_active && m_bentoProjectionSession
        && std::any_of(m_bentoProjectionPaneWindows.cbegin(), m_bentoProjectionPaneWindows.cend(),
               [](const auto &pane) { return pane && !pane->isDeleted(); });
}

bool CardStageController::resumeBentoGroup()
{
    if (!hasBentoGroup() || m_presentation != CardPresentation::Spread
        || m_cardGrabActive || m_launcherGuestActive) return false;
    if (selectedIsBentoGroup()) return resumeSelectedBentoProjection();
    const QPointer<KWin::EffectWindow> before = selectedWindow();
    KWin::EffectWindow *pane = nullptr;
    for (const auto &member : std::as_const(m_bentoProjectionPaneWindows))
        if (member && !member->isDeleted()) { pane = member; break; }
    if (!pane || !selectCardEntry(pane) || !selectedIsBentoGroup()) {
        if (before) (void)selectCardEntry(before);
        return false;
    }
    if (resumeSelectedBentoProjection()) return true;
    if (before) (void)selectCardEntry(before);
    return false;
}

bool CardStageController::stepActiveBackToLayout()
{
    if (!hasBentoGroup() || m_presentation != CardPresentation::Active
        || !activeCardIdentity() || m_cardGrabActive || m_launcherGuestActive) return false;
    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    parkActiveSnapshot();
    m_presentation = CardPresentation::Spread;
    syncSelectedElevation();
    return true;
}

QList<KWin::EffectWindow *> CardStageController::paneStack(const KWin::EffectWindow *pane) const
{
    QList<KWin::EffectWindow *> result;
    for (const auto &member : m_paneStacks.value(pane))
        if (member && !member->isDeleted() && liveCardIndex(member) >= 0) result.append(member.data());
    return result;
}

void CardStageController::notePaneStack(KWin::EffectWindow *pane,
                                        const QList<KWin::EffectWindow *> &behind)
{
    QList<QPointer<KWin::EffectWindow>> members;
    for (auto *member : behind)
        if (member && member != pane && !member->isDeleted()) members.append(member);
    if (!pane || members.isEmpty()) m_paneStacks.remove(pane);
    else m_paneStacks.insert(pane, members);
}

KWin::EffectWindow *CardStageController::paneHolding(const KWin::EffectWindow *card) const
{
    for (auto it = m_paneStacks.cbegin(); it != m_paneStacks.cend(); ++it)
        for (const auto &member : it.value())
            if (member && member == card) return const_cast<KWin::EffectWindow *>(it.key());
    return nullptr;
}

void CardStageController::forgetPaneStack(const KWin::EffectWindow *pane)
{
    m_paneStacks.remove(pane);
}

void CardStageController::forgetFromPaneStacks(const KWin::EffectWindow *window)
{
    for (auto it = m_paneStacks.begin(); it != m_paneStacks.end();) {
        it.value().removeIf([window](const auto &member) { return !member || member == window; });
        if (it.value().isEmpty()) it = m_paneStacks.erase(it);
        else ++it;
    }
}

bool CardStageController::stackCardOnto(KWin::EffectWindow *card, KWin::EffectWindow *onto)
{
    if (!card || !onto || card == onto || liveCardIndex(card) < 0 || liveCardIndex(onto) < 0) return false;
    QList<KWin::EffectWindow *> behind = stackFrontToBack(onto);
    if (behind.isEmpty()) behind.append(onto);
    behind.removeAll(card);
    if (!restack(card, behind)) return false;
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::restack(KWin::EffectWindow *face, const QList<KWin::EffectWindow *> &behind)
{
    if (!face || liveCardIndex(face) < 0 || m_cardGrabActive || m_launcherGuestActive) return false;
    QList<KWin::EffectWindow *> members{face};
    for (auto *member : behind)
        if (member && !member->isDeleted() && liveCardIndex(member) >= 0 && !members.contains(member))
            members.append(member);
    if (members.size() < 2) return false;
    // Already the Stack it should be: nothing moves.
    if (stackFrontToBack(face) == members) return true;
    for (auto *member : std::as_const(members)) {
        const int id = liveCardIndex(member) + 1;
        for (int guard = 0; m_workspace.stackSizeForId(id) > 1 && guard < 64; ++guard)
            if (!m_workspace.releaseMember(id)) return false;
    }
    QList<QPointer<KWin::EffectWindow>> rest;
    for (int i = 1; i < members.size(); ++i) rest.append(members.at(i));
    if (!m_workspace.stackBehind(face, rest)) return false;
    syncSelectedStackingOrder();
    standStackWhole(face);
    return true;
}

void CardStageController::settlePaneStacks()
{
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || m_paneStacks.isEmpty()) return;
    for (auto it = m_paneStacks.begin(); it != m_paneStacks.end();) {
        auto *pane = const_cast<KWin::EffectWindow *>(it.key());
        const auto behind = paneStack(pane);
        const bool paneGone = !std::any_of(KWin::effects->stackingOrder().cbegin(),
            KWin::effects->stackingOrder().cend(), [pane](auto *w) { return w == pane; });
        if (behind.isEmpty()) { it = m_paneStacks.erase(it); continue; }
        if (paneGone) {
            // Its pane closed: the cards that waited behind it stay together.
            if (behind.size() > 1) (void)restack(behind.first(), behind.mid(1));
            it = m_paneStacks.erase(it);
            continue;
        }
        if (liveCardIndex(pane) >= 0) {
            // The pane is a card again: its Stack is whole in the row.
            (void)restack(pane, behind);
            it = m_paneStacks.erase(it);
            continue;
        }
        // Still a pane: the cards behind it wait together, in order.
        if (behind.size() > 1) (void)restack(behind.first(), behind.mid(1));
        ++it;
    }
}

bool CardStageController::resumeSelectedBentoProjection()
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || !selectedIsBentoGroup() || !m_bentoProjectionSession
        || m_cardGrabActive || m_launcherGuestActive) return false;
    BentoProjectionSession projection = *m_bentoProjectionSession;
    QList<QPointer<KWin::EffectWindow>> projectionWindows = m_bentoProjectionWindows;
    QHash<const KWin::EffectWindow *, QList<QPointer<KWin::EffectWindow>>> paneStacks;
    // The layout opens on the pane whose card was chosen, and that pane has
    // the keys, so nothing has to be activated after it opens.
    if (KWin::EffectWindow *chosen = selectedWindow(); chosen
        && std::any_of(projection.panes.cbegin(), projection.panes.cend(),
               [chosen](const auto &member) { return member.window == chosen; }))
        projection.lead = chosen;
    {
        // CARD-LIFECYCLE.md §11: a Stack holding a pane fills that pane with
        // its front card, and the rest of it waits behind the pane.
        QList<const KWin::EffectWindow *> faces;
        for (auto &member : projection.panes) {
            KWin::EffectWindow *pane = member.window.data();
            const auto stack = stackFrontToBack(pane);
            if (stack.size() < 2) continue;
            KWin::EffectWindow *face = stack.first();
            if (faces.contains(face)) continue;
            if (face != pane) {
                const auto restore = managedRestore(face);
                if (inProjectedLayout(face) || !isEligiblePartner(face) || !restore) continue;
                if (projection.lead == pane) projection.lead = face;
                if (projection.sideWindow == pane) projection.sideWindow = face;
                if (projection.snappedWindow == pane) projection.snappedWindow = face;
                for (auto &stacked : projection.stackingOrder)
                    if (stacked == pane) stacked = face;
                member = {face, *restore, false};
                const int at = projectionWindows.indexOf(pane);
                if (at >= 0) projectionWindows[at] = face;
            }
            faces.append(face);
            QList<QPointer<KWin::EffectWindow>> behind;
            for (auto *window : stack)
                if (window != face) behind.append(window);
            paneStacks.insert(face, behind);
        }
        if (!validBentoProjectionShape(projectionShape(projection))) return false;
    }
    if (auto *tablet = m_host->tabletOutputForCardStage()) {
        for (const auto &member : std::as_const(projection.panes)) {
            // Each pane grows from its own card.
            const QRectF card(previewTargetForWindow(tablet, member.window.data()));
            projection.drawnPanes.append(card.isEmpty() ? QRectF() : card);
        }
    }
    const auto allWindows = m_workspace.windows();
    const auto originalStackingOrder = m_originalCardStackingOrder;
    // CARD-LIFECYCLE.md §6: choosing a pane's card resumes its layout. Only its own
    // members leave this stage; the cards around it keep their ownership and
    // their parked restore records, so none of them reaches the native desktop.
    const auto departure = m_workspace.prepareGroupRemoval(projectionWindows,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    // Each pane goes back to its own size in the layout; its picture is kept
    // until its application has drawn itself there.
    for (const auto &member : std::as_const(projection.panes)) {
        KWin::EffectWindow *pane = member.window.data();
        if (pane && !pane->isDeleted() && !pane->isMinimized())
            (void)m_host->holdPictureBeforeResizeForCardStage(pane, [] {});
    }
    bool committed = false;
    return m_host->resumeBentoProjectionForCardStage(projection,
        [this, projectionWindows, paneStacks, &departure, &committed] {
            committed = commitResumeHandback(
                [&] {
                    if (!m_active || !selectedIsBentoGroup()
                        || !m_workspace.commitGroupRemoval(*departure)) return false;
                    m_transferGuard.invalidate();
                    m_host->cancelInputForCardStage();
                    clearCardTransition();
                    m_activeSettleTimer.stop();
                    m_activeSettleRemaining = 0;
                    m_activeRestore = {};
                    for (const auto &window : projectionWindows) {
                        retireActiveIdentity(window);
                        m_parkedRestores.removeIf([&](const auto &saved) {
                            return !saved.window || saved.window == window; });
                        m_originalCardStackingOrder.removeAll(window);
                    }
                    m_bentoProjectionWindows.clear();
                    m_bentoProjectionPaneWindows.clear();
                    m_paneStacks = paneStacks;
                    // §2: the display now presents Bento. Whatever this stage
                    // still owns stays owned and hidden behind it.
                    m_presentation = CardPresentation::Bento;
                    m_host->setPagingShortcutsForCardStage(false);
                    if (m_workspace.windows().isEmpty()) {
                        m_active = false;
                        m_presentation = CardPresentation::Spread;
                        m_originalCardStackingOrder.clear();
                    }
                    return true;
                },
                [&] {
                    m_bentoProjectionSession.reset();
                    for (const auto &window : projectionWindows) {
                        if (window && !window->isDeleted())
                            KWin::effects->setElevatedWindow(window, false);
                    }
                    m_host->retireBentoProjectionForCardStage(projectionWindows);
                },
                [] {});
            return committed;
        },
        [this, allWindows, projectionWindows, originalStackingOrder, &committed] {
            if (!committed) return;
            for (const auto &window : allWindows) {
                if (window && !window->isDeleted()) {
                    KWin::effects->setElevatedWindow(window, false);
                    m_host->unredirectForCardStage(window);
                }
            }
            QScopedValueRollback<bool> applying(m_applyingWindowState, true);
            m_restoredMinimizations.clear();
            // Raise only what resumed. A retained card is hidden rather than
            // restored, so leaving it above the panes would occlude them in
            // KWin's own stacking while Kadunce paints nothing for it.
            for (const auto &window : originalStackingOrder) {
                if (window && !window->isDeleted() && window->window()
                    && projectionWindows.contains(window))
                    KWin::workspace()->raiseWindow(window->window());
            }
            KWin::effects->addRepaintFull();
        });
}

void CardStageController::release()
{
    forgetCloseAsk(nullptr);
    m_heldInPlace.clear();
    m_liftedInPlace.clear();
    bringCardsBack();
    m_returnToDesktop = false;
    m_returnedToDesktop.clear();
    stopOpeningSpread();
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    m_row = {};
    finishScrub();
    m_lift = {};
    m_motionClock.invalidate();
    // A window still closing stays out of sight; one that did not close is
    // going back to the desktop and must be seen there.
    m_thrown.removeIf([](const Thrown &thrown) {
        return !thrown.window || !thrown.window->isDeleted();
    });
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    if (!m_active) {
        return;
    }
    finishCardGrab(false);
    endLauncherGuest();
    clearCardTransition(); // Going away: a cancelled grab glides nowhere.
    const QPointer<KWin::EffectWindow> releasedWindow = selectedWindow();
    // Stop filtering the scene before fullscreen restoration changes layers,
    // activation or geometry. Those operations can synchronously reenter KWin.
    m_active = false;
    m_presentation = CardPresentation::Spread;
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_workspace.windows())) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
            KWin::effects->setElevatedWindow(window, false);
        }
    }
    restoreActiveSnapshot();
    restoreOriginalStackingOrder();
    // Replaying the old stack must not leave the released fullscreen client
    // underneath another application or without active-fullscreen treatment.
    // Only on the desktop shown: activating a window elsewhere would switch
    // desktops under the person while every desktop is being released.
    if (releasedWindow && !releasedWindow->isDeleted()
        && releasedWindow->window() && releasedWindow->window()->isFullScreen()
        && releasedWindow->isOnCurrentDesktop()
        && KWin::effects->sessionState() == KWin::SessionState::Normal) {
        KWin::workspace()->raiseWindow(releasedWindow->window());
        KWin::workspace()->activateWindow(releasedWindow->window(), true);
        qInfo() << "Kadunce fullscreen release: direct scene, restored focus"
                << logNameOf(releasedWindow) << releasedWindow->frameGeometry();
    }
    m_host->setPagingShortcutsForCardStage(false);
    m_workspace.clear();
    m_presentedActive = nullptr;
    m_bentoProjectionWindows.clear();
    m_bentoProjectionPaneWindows.clear();
    m_bentoProjectionSession.reset();
    m_originalCardStackingOrder.clear();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "released";
}

void CardStageController::standPanesInCardPlace()
{
    // §11: on Connected each pane is drawn as a whole card, so it stands in
    // the card's place and draws itself at that size, as a pane let go of
    // does. Resuming puts every pane back on its stored layout.
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !m_bentoProjectionSession || !tablet || m_presentation != CardPresentation::Spread) return;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const auto &window : std::as_const(m_bentoProjectionWindows)) {
        KWin::Window *client = window && !window->isDeleted() ? window->window() : nullptr;
        if (!client || window->isMinimized()) continue;
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore)
            client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        const KWin::RectF target(activeTarget(tablet));
        if (client->moveResizeGeometry() == target) continue;
        // Spread is opening over it, so its picture is kept until its
        // application has drawn itself at the card's size.
        const QPointer<KWin::EffectWindow> held(window);
        const auto resize = [this, held, target] {
            // A pick in the moment before may already have put it back.
            if (!held || held->isDeleted() || !held->window() || m_presentation != CardPresentation::Spread
                || !m_bentoProjectionWindows.contains(held)) return;
            QScopedValueRollback<bool> resizing(m_applyingWindowState, true);
            held->window()->moveResize(target);
        };
        if (!m_host->holdPictureBeforeResizeForCardStage(window, resize)) client->moveResize(target);
    }
}

bool CardStageController::presentSelectedNow()
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_cardGrabActive || m_launcherGuestActive || selectedIsBentoGroup()) return false;
    if (!enterActive()) return false;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::adoptDisplayWithActive(KWin::EffectWindow *carried,
    const std::function<bool()> &commitSource)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    // §3 adopts a display Kadunce does not yet own. The plan tests the same
    // thing, so both must read ownership rather than the stage's active flag.
    if (ownsDisplay(tablet) || !commitSource || !tablet || !carried || carried->isDeleted()
        || !carried->window() || carried->screen() != tablet
        || carried->isUserResize() || carried->isMinimized()
        || !m_host->isManagedWindowForCardStage(carried)
        || !KWin::effects->screens().contains(tablet)) {
        qInfo() << "Kadunce refused display adoption:"
                << "owns" << ownsDisplay(tablet) << "tablet" << bool(tablet)
                << "window" << bool(carried && !carried->isDeleted() && carried->window())
                << "sameOutput" << (carried && carried->screen() == tablet)
                << "resizing" << (carried && carried->isUserResize())
                << "minimized" << (carried && carried->isMinimized())
                << "managed" << (carried && m_host->isManagedWindowForCardStage(carried));
        return false;
    }
    // CARD-LIFECYCLE.md §3: the first deliberate action adopts the display as
    // one batch. Nothing is published until the carry commits, so a refusal
    // leaves every window Native rather than half of them owned.
    if (!commitSource()) {
        qInfo() << "Kadunce refused display adoption: the carried source changed";
        return false;
    }
    m_restoredMinimizations.clear();
    rebuildLiveCards();
    const int carriedIndex = liveCardIndex(carried);
    if (carriedIndex < 0) {
        qInfo() << "Kadunce refused display adoption: the carried window is not a live card";
        m_workspace.clear();
        m_presentedActive = nullptr;
        m_parkedRestores.clear();
        m_originalCardStackingOrder.clear();
        return false;
    }
    m_active = true;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    (void)selectCardEntry(carried);
    // Adoption produces individual cards only. The carried window is Active and
    // every other eligible window is a nonselected card; no pane is filled.
    if (!enterActive()) m_presentation = CardPresentation::Spread;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "adopted the display as"
            << m_workspace.windows().size() << "individual cards; Active is"
            << logNameOf(carried);
    return true;
}

void CardStageController::leaveBentoPresentation()
{
    if (!m_active || m_presentation != CardPresentation::Bento) return;
    m_presentation = CardPresentation::Spread;
    m_host->setPagingShortcutsForCardStage(true);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::promoteToActive(KWin::EffectWindow *window)
{
    if (!m_active || m_launcherGuestActive || !window || window->isDeleted()
        || !window->window() || m_presentation == CardPresentation::Bento) return false;
    finishCardGrab(false);
    const int index = liveCardIndex(window);
    if (index < 0) return false;
    if (m_presentation == CardPresentation::Active) {
        if (selectedWindow() == window) return true;
        parkActiveSnapshot();
    }
    if (!selectCardEntry(window)) return false;
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
        return false;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

bool CardStageController::releasePairToBento(KWin::EffectWindow *carried,
                                             KWin::EffectWindow *partner)
{
    // §11, Connected: with no partner the carried card is placed alone.
    if (!m_active || m_launcherGuestActive || !carried
        || carried == partner || m_presentation == CardPresentation::Bento
        || liveCardIndex(partner ? partner : carried) < 0) return false;
    finishCardGrab(false);
    QList<QPointer<KWin::EffectWindow>> pair;
    if (partner) pair.append(partner);
    // A window the user snapped in from the desktop is not a card yet, so it
    // has no individual ownership here to give up; the destination admits it.
    if (liveCardIndex(carried) >= 0) pair.append(carried);
    // §3: exactly these leave individual card ownership, in one step, so the
    // destination can publish two panes and nothing else.
    const auto departure = m_workspace.prepareGroupRemoval(pair,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitGroupRemoval(*departure); },
            [&] {
                ++m_restoreGeneration;
                m_activeSettleTimer.stop();
                m_activeSettleRemaining = 0;
                for (const auto &window : pair) {
                    retireActiveIdentity(window);
                    if (m_activeRestore.window == window) m_activeRestore = {};
                    m_parkedRestores.removeIf([&](const auto &saved) {
                        return !saved.window || saved.window == window; });
                    m_originalCardStackingOrder.removeAll(window);
                }
                // §2: the display presents Bento; the remaining cards stay
                // owned and hidden behind it.
                m_presentation = CardPresentation::Bento;
                m_host->setPagingShortcutsForCardStage(false);
                if (m_workspace.windows().isEmpty()) {
                    m_active = false;
                    m_presentation = CardPresentation::Spread;
                    m_originalCardStackingOrder.clear();
                }
            })) return false;
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    // §11, Connected: a Stack fills one pane. Two cards that are now two
    // panes wait behind neither, or Spread would join them again.
    for (const auto &window : pair) forgetFromPaneStacks(window);
    for (const auto &window : pair) {
        if (window && !window->isDeleted()) {
            KWin::effects->setElevatedWindow(window, false);
            m_host->unredirectForCardStage(window);
        }
    }
    // A card left behind the layout is drawn nowhere, so the panes stand above
    // it in KWin's own stacking too, or a press on a pane would reach it.
    for (KWin::EffectWindow *pane : {partner, carried}) {
        if (pane && !pane->isDeleted() && pane->window())
            KWin::workspace()->raiseWindow(pane->window());
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << (partner ? "paired two cards into Bento:" : "placed a card alone:")
            << (carried && carried->window() ? carried->window()->resourceClass() : QString())
            << "with"
            << (partner && partner->window() ? partner->window()->resourceClass() : QStringLiteral("nothing"))
            << ";" << m_workspace.windows().size() << "individual cards remain";
    return true;
}

bool CardStageController::releaseCardToLiveBento(KWin::EffectWindow *card)
{
    if (!m_active || m_launcherGuestActive || !card || card->isDeleted()
        || !card->window() || m_presentation != CardPresentation::Bento
        || liveCardIndex(card) < 0) return false;
    finishCardGrab(false);
    // §8: exactly this card leaves individual ownership, in one step, so the
    // layout never publishes a pane this stage still names as a card.
    const QList<QPointer<KWin::EffectWindow>> leaving{card};
    const auto departure = m_workspace.prepareGroupRemoval(leaving,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitGroupRemoval(*departure); },
            [&] {
                ++m_restoreGeneration;
                m_activeSettleTimer.stop();
                m_activeSettleRemaining = 0;
                retireActiveIdentity(card);
                if (m_activeRestore.window == card) m_activeRestore = {};
                m_parkedRestores.removeIf([&](const auto &saved) {
                    return !saved.window || saved.window == card; });
                m_originalCardStackingOrder.removeAll(card);
                // The display was already presenting Bento and still is; only
                // the membership behind it changed. An empty card stage still
                // has a live layout in front of it, so this stage stays active
                // rather than releasing the display it no longer draws.
            })) return false;
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    // A card held aside is hidden from KWin, and a hidden window draws nothing
    // and so never takes the size its pane asks of it: it comes back first.
    if (m_aside.removeAll(card) > 0) {
        QObject::disconnect(m_asideListed.take(card));
        card->setData(CardAsideRole, QVariant());
        if (card->window()) card->window()->setHidden(false);
    }
    KWin::effects->setElevatedWindow(card, false);
    m_host->unredirectForCardStage(card);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "admitted a called card into live Bento;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

bool CardStageController::admitCardFromOtherDisplay(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (m_cardGrabActive || m_launcherGuestActive || !tablet || !window
        || window->isDeleted() || !window->window() || !window->isNormalWindow()
        || !m_host->mayHoldWindowForCardStage(window)
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    // §5 keeps the record the window had before a layout placed it, so
    // release still returns it where it began.
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : client->moveResizeGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry
                                             : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
        .outputName = outputNameOf(restore ? restore->output.data() : window->screen()),
        .outputGeometry = outputGeometryOf(restore ? restore->output.data() : window->screen()),
    };
    const bool started = !m_active;
    const int previousSelection = m_workspace.selectedIndex();
    const auto admission = m_workspace.prepareAdmission(arrival, false);
    if (!admission || !m_workspace.commitAdmission(*admission, commitSource)) return false;
    // Admission selects the arrival; the entry this display had stays selected.
    if (!started && previousSelection >= 0) m_workspace.selectIndex(previousSelection);
    if (started) {
        // A stage that held nothing shows the desktop, with this card aside.
        m_active = true;
        m_presentation = CardPresentation::Desktop;
        m_host->setPagingShortcutsForCardStage(false);
    }
    m_originalCardStackingOrder.append(arrival);
    if (!arrival || arrival->isDeleted() || arrival->window() != client) return true;
    m_host->connectManagedWindowForCardStage(arrival);
    if (!managedRestore(arrival)) m_parkedRestores.append(incoming);
    KWin::effects->setElevatedWindow(arrival, false);
    if (!arrival->isMinimized() && client) {
        // It stands in the Active card's place here, as every card does, so
        // Spread can draw it on this display.
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore)
            client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, arrival->frameGeometry().center());
        const KWin::RectF target(activeTarget(tablet));
        // Taken in while the Active card shows, it is drawn in the row as
        // soon as Spread opens: its picture is kept until its application has drawn itself
        // at the new size, so a slow one shows no blank card.
        const QPointer<KWin::EffectWindow> held(arrival);
        const auto resize = [this, held, target] {
            if (!held || held->isDeleted() || !held->window() || liveCardIndex(held) < 0) return;
            QScopedValueRollback<bool> resizing(m_applyingWindowState, true);
            held->window()->moveResize(target);
        };
        if (m_presentation != CardPresentation::Active
            || !m_host->holdPictureBeforeResizeForCardStage(arrival, resize))
            client->moveResize(target);
        // Behind a layout or the Active card it is drawn nowhere, so it must
        // also be under them, or the touches on them would reach it.
        if (m_presentation == CardPresentation::Bento || m_presentation == CardPresentation::Active)
            KWin::workspace()->lowerWindow(client);
    }
    if (m_presentation == CardPresentation::Desktop) holdCardsAside();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took" << logNameOf(arrival)
            << "as a card from another display;" << m_workspace.windows().size() << "individual cards";
    return true;
}

bool CardStageController::giveCardToDisplay(KWin::EffectWindow *card,
    KWin::LogicalOutput *destination)
{
    if (!m_active || m_launcherGuestActive || m_cardGrabActive || !card || !destination
        || !m_host->tabletOutputForCardStage() || liveCardIndex(card) < 0) return false;
    if (!stepAsideFrom(card)) return false;
    return sendCardToDisplay(card, destination);
}

bool CardStageController::stepAsideFrom(KWin::EffectWindow *card)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Active || selectedWindow() != card)
        return true;
    // A display that is gone shows nothing, so nothing comes back in front.
    if (!tablet) {
        parkActiveSnapshot();
        showDesktop();
        return true;
    }
    // Only a card of this display's own comes back in front of it; one
    // waiting for another display is never shown here.
    const auto shown = [&](KWin::EffectWindow *other) {
        KWin::LogicalOutput *own = other ? m_host->releaseDisplayForCardStage(other) : nullptr;
        return other && other != card && !other->isDeleted() && other->window()
            && !other->isMinimized() && other->screen() == tablet && liveCardIndex(other) >= 0
            && (!own || own == tablet);
    };
    KWin::EffectWindow *back = shown(m_usedBefore) ? m_usedBefore.data() : nullptr;
    if (back) {
        if (!selectCardEntry(back)) return false;
        m_presentation = CardPresentation::Spread;
        return enterActive();
    }
    parkActiveSnapshot();
    showDesktop();
    return true;
}

bool CardStageController::releaseCardToStage(KWin::EffectWindow *card)
{
    if (!m_active || m_launcherGuestActive || m_cardGrabActive || !card || liveCardIndex(card) < 0)
        return false;
    return stepAsideFrom(card) && releaseCardToDisplay(card);
}

bool CardStageController::presentCardActive(KWin::EffectWindow *card, bool focus)
{
    if (!m_active || m_launcherGuestActive || m_cardGrabActive || !card || card->isDeleted()
        || card->isMinimized() || liveCardIndex(card) < 0) return false;
    if (m_presentation == CardPresentation::Active && selectedWindow() == card) return true;
    if (!selectCardEntry(card)) return false;
    if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
    m_presentation = CardPresentation::Spread;
    if (!focus) m_focusAfterActive = KWin::effects->activeWindow();
    if (enterActive()) return true;
    m_focusAfterActive.clear();
    showDesktop();
    return false;
}

bool CardStageController::releaseCardToDisplay(KWin::EffectWindow *card)
{
    if (!m_active || m_launcherGuestActive || m_cardGrabActive || !card || card->isDeleted()
        || !card->window() || liveCardIndex(card) < 0) return false;
    // The card in front of this display stays in front of it.
    if (m_presentation == CardPresentation::Active && selectedWindow() == card) return false;
    const QList<QPointer<KWin::EffectWindow>> leaving{card};
    const auto departure = m_workspace.prepareGroupRemoval(leaving,
        OwnershipTransition::CardToBento);
    if (!departure) return false;
    if (!publishOwnershipThenRecord(
            [&] { return m_workspace.commitGroupRemoval(*departure); },
            [&] {
                ++m_restoreGeneration;
                forgetManagedRestore(card);
                m_originalCardStackingOrder.removeAll(card);
            })) return false;
    m_transferGuard.invalidate();
    m_host->cancelInputForCardStage();
    clearCardTransition();
    if (m_aside.removeAll(card) > 0) {
        QObject::disconnect(m_asideListed.take(card));
        card->setData(CardAsideRole, QVariant());
        if (card->window()) card->window()->setHidden(false);
    }
    KWin::effects->setElevatedWindow(card, false);
    m_host->unredirectForCardStage(card);
    // A stage left with nothing to hold stops, unless a layout is in front of
    // it, which it still answers for.
    if (m_workspace.windows().isEmpty() && m_presentation != CardPresentation::Bento) {
        m_active = false;
        m_presentation = CardPresentation::Spread;
        m_originalCardStackingOrder.clear();
        m_host->setPagingShortcutsForCardStage(false);
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "gave a card to the display it shows on;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

bool CardStageController::sendCardToDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *destination)
{
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || !card || card->isDeleted()
        || !card->window() || card->isMinimized() || !destination
        || m_host->isTabletOutputForCardStage(destination) || liveCardIndex(card) < 0
        || (m_presentation == CardPresentation::Active && selectedWindow() == card)) return false;
    const bool accepted = m_host->admitCardToDesktopStage(card, destination,
        KWin::RectF(activeTarget(destination)),
        [this, card] { return releaseCardToDisplay(card); }, [] {});
    if (accepted)
        qInfo() << "Kadunce" << Revision << "showed card" << logNameOf(card) << "on" << destination->name();
    return accepted;
}

bool CardStageController::admitSleepingPaneAsCard(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (m_cardGrabActive || m_launcherGuestActive || !tablet || !window
        || window->isDeleted() || !window->window() || !window->isNormalWindow()
        || !window->isMinimized()
        || !m_host->mayHoldWindowForCardStage(window)
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const auto ticket = m_transferGuard.issue();
    // §5 keeps the record the window had before Bento placed it, so release
    // still returns it where it began. The caller supplies it because the
    // session that held it has already published a plan that no longer names
    // this window.
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : client->moveResizeGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry
                                             : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
        .outputName = outputNameOf(restore ? restore->output.data() : window->screen()),
        .outputGeometry = outputGeometryOf(restore ? restore->output.data() : window->screen()),
    };
    const auto admission = m_workspace.prepareAdmission(arrival, false);
    if (!admission) return false;
    if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
    m_originalCardStackingOrder.append(arrival);
    if (!m_active) {
        // A pair takes both cards, so the stage that gave them up owns nothing
        // and is not active. It has to own this one, and §2 gives it the only
        // presentation that fits: the display is showing the panes this window
        // just left, and a sleeping card sits behind them like every other
        // card this stage holds. Entering Spread would draw a stage whose one
        // member is asleep over a live layout.
        m_active = true;
        m_presentation = CardPresentation::Bento;
        m_host->setPagingShortcutsForCardStage(false);
    }
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data());
    };
    // Membership is published. Everything below is presentation cleanup, so an
    // invalidation stops it rather than failing a transfer that already
    // happened.
    if (!valid()) return true;
    m_host->connectManagedWindowForCardStage(arrival);
    if (!valid()) return true;
    if (!managedRestore(arrival)) m_parkedRestores.append(incoming);
    // §7: sleeping and nonselected. It is given no geometry, is not raised,
    // and keeps the minimized state the user asked for.
    KWin::effects->setElevatedWindow(arrival, false);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took a minimized pane as a sleeping card;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

int CardStageController::returnCardsToDisplay(bool every)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !tablet || !KWin::effects->screens().contains(tablet)) return 0;
    // Where KWin intends a window to be, not where its last acknowledged frame
    // was: a Wayland client acknowledges a move later than KWin decides it.
    const KWin::EffectWindow *presented =
        m_presentation == CardPresentation::Active ? selectedWindow() : nullptr;
    int returned = 0;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const QPointer<KWin::EffectWindow> &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || !window->window()) continue;
        KWin::Window *client = window->window();
        if ((!every && client->moveResizeOutput() == tablet)
            || client->isInteractiveMove() || client->isInteractiveResize()) continue;
        // §11, Every display: the card a display Spread has left still shows
        // stays where it stands, as that display's own, rather than standing
        // over whatever the new display shows.
        if (window == presented) {
            const KWin::LogicalOutput *own = m_host->releaseDisplayForCardStage(window);
            if (own && own != tablet) continue;
        }
        // KWin may have given the window back a state it held on that layout.
        // A card holds none of them; its restore record keeps the originals.
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore)
            client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        // Every card stands where the Active card does. Only the presented one
        // is drawn there; the rest are drawn in Spread's slots or not at all.
        const KWin::Rect target = window == presented
            ? activePlacement(tablet) : activeTarget(tablet);
        client->moveResize(KWin::RectF(target));
        ++returned;
        qInfo() << "Kadunce" << Revision << "returned card" << logNameOf(window)
                << "to" << tablet->name() << "target" << target;
    }
    if (returned == 0) return 0;
    if (presented) m_activeSettleRemaining = 2;
    KWin::effects->addRepaintFull();
    return returned;
}

int CardStageController::applyGutter()
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !tablet || !KWin::effects->screens().contains(tablet)) return 0;
    const KWin::EffectWindow *presented =
        m_presentation == CardPresentation::Active ? selectedWindow() : nullptr;
    // Keys up over the Active card: the room is made from the new target.
    if (m_keyboardRoom && m_keyboardRoom->window && m_keyboardRoom->window == presented) {
        m_keyboardRoom->base = KWin::RectF(activeTargetFor(tablet, presented));
        refreshKeyboardRoom();
    }
    int moved = 0;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    for (const QPointer<KWin::EffectWindow> &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || !window->window() || window->isMinimized()
            || isBentoProjectionPane(window)) continue;
        KWin::Window *client = window->window();
        // A Bento pane keeps its layout's own place. A card held, carried or being resized by hand is left where it is;
        // it takes the gutter when it is set down.
        if (client->moveResizeOutput() != tablet || client->isInteractiveMove()
            || client->isInteractiveResize() || client->isFullScreen()
            || client->maximizeMode() != KWin::MaximizeRestore
            || client->quickTileMode() != KWin::QuickTileMode{}
            || (m_cardGrabActive && window == selectedWindow())) continue;
        const KWin::Rect target = window == presented
            ? activePlacement(tablet) : activeTarget(tablet);
        if (client->moveResizeGeometry().toRect() == target) continue;
        client->moveResize(KWin::RectF(target));
        ++moved;
    }
    if (moved == 0) return 0;
    if (presented) m_activeSettleRemaining = 2;
    KWin::effects->addRepaintFull();
    return moved;
}

bool CardStageController::admitArrivalAsCard(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || !tablet || !window
        || window->isDeleted() || !window->window() || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->window()->moveResizeOutput() != tablet
        || window->isUserMove() || window->isUserResize()
        || liveCardIndex(window) >= 0) return false;
    // Admission appends the card after every other and selects it; the
    // selection this display had is put back until the caller chooses.
    const int previousSelection = m_workspace.selectedIndex();
    const auto admission = m_workspace.prepareAdmission(window, false);
    if (!admission || !m_workspace.commitAdmission(*admission, [] { return true; }))
        return false;
    m_workspace.selectIndex(previousSelection);
    // Published first, then described, as every other admission does. The
    // record is where KWin put it on this display, which is a place release
    // can always return it to.
    retainManagedOwnership(window);
    m_originalCardStackingOrder.append(window);
    m_host->connectManagedWindowForCardStage(window);
    KWin::effects->setElevatedWindow(window, false);
    // Behind a layout the card is drawn nowhere, so it must also be under the
    // panes, or the touches on them would reach it.
    if (m_presentation == CardPresentation::Bento) KWin::workspace()->lowerWindow(window->window());
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "took" << logNameOf(window)
            << "as a card after its display went away;"
            << m_workspace.windows().size() << "individual cards";
    return true;
}

bool CardStageController::startBehindLayout()
{
    if (m_active || !m_workspace.windows().isEmpty()) return false;
    // As for a sleeping pane: the display is showing panes, and a card behind
    // them is drawn nowhere until the person calls it or opens Spread.
    m_active = true;
    m_presentation = CardPresentation::Bento;
    m_host->setPagingShortcutsForCardStage(false);
    return true;
}

void CardStageController::stopBehindLayoutIfEmpty()
{
    if (!m_active || !m_workspace.windows().isEmpty() || m_presentation != CardPresentation::Bento) return;
    m_active = false;
    m_presentation = CardPresentation::Spread;
    m_originalCardStackingOrder.clear();
}

bool CardStageController::admitDisplacedPaneAsHiddenCard(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    // Each refusal says which rule refused, so a pane left plain on hardware
    // can be traced to it.
    const char *refusal = !m_active ? "cards are not active"
        : m_presentation != CardPresentation::Bento ? "the display is not presenting Bento"
        : m_cardGrabActive ? "a card is held"
        : m_launcherGuestActive ? "the launcher guest is open"
        : !tablet ? "there is no card display"
        : !window || window->isDeleted() || !window->window() ? "the window is gone"
        : !window->isNormalWindow() ? "the window is not a normal window"
        : !m_host->isManagedWindowForCardStage(window) ? "the window is not one cards hold"
        : window->isUserMove() || window->isUserResize() ? "the window is being moved or resized"
        : liveCardIndex(window) >= 0 ? "the window is already a card"
        : nullptr;
    if (refusal) {
        qInfo() << "Kadunce" << Revision << "did not take a displaced pane as a hidden card:" << refusal
                << (window ? logNameOf(window) : QString());
        return false;
    }
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const auto ticket = m_transferGuard.issue();
    // §5 keeps the record the window had before Bento placed it, so release
    // still returns it where it began rather than to a pane rectangle. The
    // caller supplies it because the session that held it has already
    // published a plan that no longer names this window.
    const ActiveRestoreSnapshot incoming{
        .window = window,
        .geometry = restore ? restore->geometry : client->moveResizeGeometry(),
        .floatingGeometry = restore ? restore->floatingGeometry : client->geometryRestore(),
        .fullscreenRestoreGeometry = restore ? restore->fullscreenRestoreGeometry
                                             : client->fullscreenGeometryRestore(),
        .quickTileMode = restore ? restore->quickTileMode : client->quickTileMode(),
        .maximizeMode = restore ? restore->maximizeMode : client->maximizeMode(),
        .fullScreen = restore ? restore->fullScreen : client->isFullScreen(),
        .minimized = restore ? restore->minimized : client->isMinimized(),
        .valid = true,
        .outputName = outputNameOf(restore ? restore->output.data() : window->screen()),
        .outputGeometry = outputGeometryOf(restore ? restore->output.data() : window->screen()),
    };
    const auto admission = m_workspace.prepareAdmission(arrival, false);
    if (!admission) {
        qInfo() << "Kadunce" << Revision << "did not take a displaced pane as a hidden card:"
                << "the card workspace refused it" << logNameOf(window);
        return false;
    }
    if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
    m_originalCardStackingOrder.append(arrival);
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data());
    };
    // Membership is published. Everything below is presentation cleanup, so an
    // invalidation stops it rather than failing a transfer that already
    // happened.
    if (!valid()) return true;
    m_host->connectManagedWindowForCardStage(arrival);
    if (!valid()) return true;
    if (!managedRestore(arrival)) m_parkedRestores.append(incoming);
    // §2: the panes are the display and this card is one of the ones behind
    // them. It does not become selected.
    KWin::effects->setElevatedWindow(arrival, false);
    KWin::effects->addRepaintFull();
    // §3: once it has faded where it stood, it stands in the Active card's
    // place as every other card does. Left at its pane's size, Spread would
    // show a pane's strip between bands of backing until it was next opened.
    QTimer::singleShot(motion(PaneArrivalDuration), &m_activeSettleTimer, [this, arrival] {
        if (!m_active || !arrival || arrival->isDeleted() || !arrival->window()
            || liveCardIndex(arrival) < 0 || arrival->isMinimized() || arrival->window()->isHidden()
            || m_bentoProjectionWindows.contains(arrival) || m_activeRestore.window == arrival
            || m_cardGrabActive || arrival->isUserMove() || arrival->isUserResize()) return;
        // Behind a layout or the Active card it is drawn nowhere, so it must
        // also be under them, or the touches on them would reach it.
        if (m_presentation == CardPresentation::Bento || m_presentation == CardPresentation::Active)
            KWin::workspace()->lowerWindow(arrival->window());
        settleCardsInActivePlace({arrival});
    });
    qInfo() << "Kadunce" << Revision << "took a displaced pane as a hidden card;"
            << m_workspace.windows().size() << "individual cards remain";
    return true;
}

} // namespace Kadunce

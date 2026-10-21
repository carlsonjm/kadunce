/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Bento pairing and Stacks, held pictures, and paging between cards.
#include "EffectInternal.h"

namespace Kadunce
{

bool Effect::mayHoldWindowForCardStage(
    const KWin::EffectWindow *window) const
{
    // §7 keeps a minimized window owned as an individual card, so what card
    // ownership may hold cannot exclude the one state the section is about.
    // `isCardWindow` answers what this effect can present, and excludes it.
    return isApplicationWindow(window)
        && (!window->isHidden() || window->data(CardAsideRole).toBool());
}

void Effect::setPagingShortcutsForCardStage(bool active)
{
    // The shortcuts page the cards on screen; a desktop not shown has none.
    if (scopedToCurrent()) setPagingShortcutsActive(active);
}

void Effect::cancelInputForCardStage()
{
    // Input and motion belong to the desktop on screen. A session on another
    // desktop changing its cards has none to cancel.
    if (!scopedToCurrent()) return;
    // A Table stroke taken away leaves the menu bar, not a half-finished pull.
    if (m_table.isOpen() && m_table.down()) {
        m_table.interrupt();
        relayoutTable();
        refreshTable();
    }
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    cancelCarryForCardStage();
    m_inputActivationGuard.invalidate();
    if (m_inputRouter) m_inputRouter->cancelWorkspaceInteraction();
}

void Effect::cancelCarryForCardStage()
{
    if (!m_carryRuntime) return;
    // Motion under way carries on: each glide checks on every frame that its
    // window is still presented where it draws it, and stops when not. A carry
    // this ends goes back with a presentation changing under it, so it does
    // not glide home.
    const bool carrying = bool(m_carriedWindow);
    {
        QScopedValueRollback<bool> outlives(m_motionOutlivesInterrupt, !carrying);
        m_carryRuntime->cancel();
    }
    if (carrying) clearDropSettle();
}

void Effect::cancelInputOnStageDisplay()
{
    if (!scopedToCurrent()) return;
    // Every display: a stage changing its cards cancels only what is under way
    // on its own display. A stroke or a carry another display's stage owns
    // goes on, so a window opening on the monitor drops nothing held on the
    // tablet.
    const bool strokeElsewhere = m_cardsOnEveryDisplay && !m_strokeContacts.isEmpty()
        && m_strokeKey != m_scopedKey;
    const bool carryElsewhere = m_cardsOnEveryDisplay && m_carriedWindow
        && stageKeyOf(m_carriedWindow) != m_scopedKey;
    if (!strokeElsewhere && !carryElsewhere) {
        cancelInputForCardStage();
        return;
    }
    if (!carryElsewhere && m_carriedWindow) cancelCarryForCardStage();
    if (!strokeElsewhere) {
        if (m_table.isOpen() && m_table.down()) {
            m_table.interrupt();
            relayoutTable();
            refreshTable();
        }
        m_lineDestination.reset(); m_lineDestinationWindow.clear();
        m_inputActivationGuard.invalidate();
        if (m_inputRouter) m_inputRouter->cancelWorkspaceInteraction();
    }
}

void Effect::connectManagedWindowForCardStage(KWin::EffectWindow *window)
{
    connectManagedWindow(window);
}

namespace
{
// The longest a resized card keeps its earlier picture, should its app be
// slow to draw at the new size.
constexpr int HeldPictureLimitMs = 300;
}

bool Effect::holdPictureBeforeResizeForCardStage(KWin::EffectWindow *window,
                                                 const std::function<void()> &resize)
{
    if (!window || !resize || !KWin::effects->isOpenGLCompositing()
        || m_heldPictures.count(window)) return false;
    auto &held = m_heldPictures[window];
    held.resize = resize;
    held.damaged = connect(window, &KWin::EffectWindow::windowDamaged, this, [this](KWin::EffectWindow *damaged) {
        const auto it = m_heldPictures.find(damaged);
        if (it == m_heldPictures.end() || it->second.resize || !it->second.picture.taken()) return;
        if (QSizeF(damaged->bufferGeometry().size()) != it->second.bufferAtCapture) ++it->second.drawnSince;
    });
    KWin::effects->addRepaintFull();
    // Whatever the paints do, the card is resized and its picture let go.
    const QPointer<KWin::EffectWindow> guard(window);
    QTimer::singleShot(HeldPictureLimitMs, this, [this, guard] {
        if (!guard) return;
        const auto it = m_heldPictures.find(guard.data());
        if (it == m_heldPictures.end()) return;
        auto resize = std::move(it->second.resize);
        releaseHeldPicture(guard.data());
        if (resize) resize();
        KWin::effects->addRepaintFull();
    });
    return true;
}

void Effect::releaseHeldPicture(KWin::EffectWindow *window)
{
    const auto it = m_heldPictures.find(window);
    if (it == m_heldPictures.end()) return;
    disconnect(it->second.damaged);
    m_heldPictures.erase(it);
}

void Effect::captureOfferPictures()
{
    for (const auto &card : std::as_const(m_roomOffer->cards)) {
        if (!card || card->isDeleted() || m_offerPictures.contains(card.data())) continue;
        KWin::EffectWindow *window = card.data();
        m_capturingHeld = window;
        KeptPicture picture;
        const bool taken = picture.take(window,
            [&](const KWin::RenderTarget &target, const KWin::RenderViewport &viewport) {
                KWin::WindowPaintData data;
                data.setOpacity(1.0);
                (void)painted([&] { return KWin::effects->drawWindow(target, viewport, window,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, KWin::Region::infinite(), data); });
            }, true);
        m_capturingHeld = nullptr;
        if (!taken) continue;
        m_offerPictures.emplace(window, std::move(picture));
        // The next frame draws it.
        KWin::effects->addRepaintFull();
    }
}

void Effect::captureHeldPictures()
{
    for (auto &[window, held] : m_heldPictures) {
        if (!held.resize || held.picture.taken() || window->isDeleted()) continue;
        m_capturingHeld = window;
        const bool taken = held.picture.take(window,
            [&](const KWin::RenderTarget &target, const KWin::RenderViewport &viewport) {
                KWin::WindowPaintData data;
                data.setOpacity(1.0);
                (void)painted([&] { return KWin::effects->drawWindow(target, viewport, window,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, KWin::Region::infinite(), data); });
            });
        m_capturingHeld = nullptr;
        if (!taken) continue;
        held.bufferAtCapture = window->bufferGeometry().size();
        // Resized once the picture is kept, outside the paint.
        QTimer::singleShot(0, this, [this, guard = QPointer<KWin::EffectWindow>(window)] {
            if (!guard) return;
            const auto it = m_heldPictures.find(guard.data());
            if (it == m_heldPictures.end() || !it->second.resize) return;
            auto resize = std::move(it->second.resize);
            it->second.resize = nullptr;
            resize();
        });
    }
}

void Effect::unredirectForCardStage(KWin::EffectWindow *window)
{
    unredirect(window);
    m_previewSourceBounds.remove(window);
}

void Effect::retireBentoProjectionForCardStage(
    const QList<QPointer<KWin::EffectWindow>> &windows)
{
    // Exact resume can synchronously return these surfaces to Desktop Stage.
    // Drop every Spread paint latch before that native scene becomes visible.
    m_fanApertureWindow = nullptr;
    m_fanPaintSize = {};
    m_fanApertureOrigin = {};
    m_fanApertureSize = {};
    m_fanApertureRadius = 0.0F;
    m_preparationNeighbors.removeIf([&](const auto &window) {
        return windows.contains(window);
    });
    m_neighborPreparedThisFrame = false;
    m_neighborPreparationFrames = m_preparationNeighbors.size();
    for (const auto &window : windows) {
        if (!window || window->isDeleted()) continue;
        unredirect(window);
        m_previewSourceBounds.remove(window);
    }
    KWin::effects->addRepaintFull();
}

bool Effect::admitCardToDesktopStage(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    // §11, Every display: a card leaving for another display holding its own
    // cards and showing no layout is that display's card, shown in front.
    if (isOtherCardDisplay(output) && !m_desktopStage->hasSessionOnOutput(output->name())) {
        const auto restore = m_cardStage->managedRestore(window);
        const QList<KWin::EffectWindow *> behind = cardsBehind(*m_cardStage, window);
        SessionScope there(this, m_scopedSession, stageKeyFor(output));
        if (!admitTransferredWindowToTablet(window, output, commitSource, restore ? &*restore : nullptr))
            return false;
        releaseSource();
        rejoinStack(window, behind);
        return true;
    }
    if (m_carryDestination && window == m_carriedWindow) {
        // §11: carried into the layout another display shows, its Stack
        // follows it there.
        const QList<KWin::EffectWindow *> behind = isOtherCardDisplay(output)
            ? cardsBehind(*m_cardStage, window) : QList<KWin::EffectWindow *>{};
        if (!m_desktopStage->transferPreparedCard(*m_carryDestination, commitSource, releaseSource)) return false;
        if (!behind.isEmpty()) {
            SessionScope there(this, m_scopedSession, stageKeyFor(output));
            rejoinStack(window, behind);
        }
        return true;
    }
    if (m_placementDrop && window == m_placementDrop->card())
        return m_desktopStage->transferPreparedCard(*m_placementDrop, commitSource, releaseSource);
    if (m_cardStage->cardGrabActive()) {
        if (!m_lineDestination || m_lineDestinationWindow != window) return false;
        const auto reserved = *m_lineDestination;
        const auto target = m_linePreview;
        const auto from = QRectF(m_cardStage->cardGrabTarget()).translated(m_cardStage->cardGrabOffset());
        QPointer<KWin::EffectWindow> arrival = window;
        QPointer<KWin::LogicalOutput> destination = output;
        const bool committed = m_desktopStage->transferPreparedCard(reserved, commitSource, releaseSource);
        if (committed && target) startDropSettle(arrival, destination, from, QRectF(*target));
        return committed;
    }
    return m_desktopStage->transferCardWindow(window, output, geometry, commitSource, releaseSource);
}

bool Effect::resumeBentoProjectionForCardStage(
    const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    return m_desktopStage->resumeProjectedSession(
        projection, commitSource, releaseSource);
}

bool Effect::pairActiveCardIntoBento(KWin::LogicalOutput *output)
{
    // CARD-LIFECYCLE.md §3 places by the gesture, and this gesture contacts no
    // edge, so the side is stated rather than read: the Active card keeps the
    // left pane and its partner is named by walking right from it. §11,
    // Connected: the key lays out every card there, so a Stack's front is a
    // partner too, its Stack waiting behind the pane it takes.
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active) return false;
    return pairCardsIntoBento(output, active,
        m_cardStage->partnerForSideSnap(active, false, nullptr, connectedOutputForDesktopStage(output)),
        BentoSidePlacement{false, true});
}

bool Effect::pairActiveCardIntoBento(KWin::LogicalOutput *output,
    BentoSidePlacement side, bool walkLeft)
{
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active) return false;
    return pairCardsIntoBento(output, active, m_cardStage->partnerForSideSnap(active, walkLeft), side);
}

bool Effect::placeActiveCardAlone(KWin::LogicalOutput *output, BentoSidePlacement side)
{
    auto *active = m_cardStage->activeCardIdentity();
    if (!output || !active || !isTabletOutput(output)) return false;
    const auto reserved = m_desktopStage->prepareCardDrop(active, output,
        KWin::RectF(active->frameGeometry()),
        DesktopStageController::CardDropIntent::ActivateBento, side, nullptr, true);
    if (!reserved) return false;
    // Its Stack waits behind the pane it becomes, so the keys carry it whole.
    QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(active);
    behind.removeAll(active);
    QPointer<KWin::EffectWindow> carried = active;
    const bool placed = m_desktopStage->activatePreparedTabletDrop(*reserved, nullptr,
        [this, carried] { return m_cardStage->releasePairToBento(carried, nullptr); });
    if (placed && carried && !behind.isEmpty()) {
        m_cardStage->notePaneStack(carried, behind);
        m_cardStage->settlePaneStacks();
        qInfo() << "Kadunce" << Revision << "placed" << logNameOf(carried) << "alone with"
                << behind.size() << "behind it";
    }
    // The room left open shows the desktop, not the cards.
    if (placed) m_cardStage->syncSelectedElevation();
    return placed;
}

void Effect::layOutHeldLayoutBeside(KWin::EffectWindow *card, KWin::LogicalOutput *output,
    BentoSidePlacement side, const QRectF &drawn)
{
    if (!card || card->isDeleted() || !output || !isTabletOutput(output)) return;
    m_aloneOnPurpose.removeAll(card);
    const QPointer<KWin::EffectWindow> placed = card;
    if (m_cardStage->activeCardIdentity() == card && m_cardStage->stepActiveBackToLayout()
        && playPickedCardIntoLayout(card) && placed && m_desktopStage->managesWindow(card)) {
        (void)m_desktopStage->placePaneBySide(card, output, side);
        startPaneArrival(card, output, drawn);
        qInfo() << "Kadunce" << Revision << "laid the layout on" << output->name() << "out again with"
                << logNameOf(card) << "at the" << (side.right ? "right" : "left") << "edge";
    } else if (placed && m_cardStage->activeCardIdentity() == card && !m_cardStage->hasBentoGroup()
               && placeActiveCardAlone(output, side)) {
        // The layout had no room for it and stays in Spread.
        startPaneArrival(card, output, drawn);
    } else {
        (void)m_cardStage->presentSelectedNow();
        qInfo() << "Kadunce" << Revision << "could not lay the layout on" << output->name()
                << "out again with" << (placed ? logNameOf(card) : QStringLiteral("a closed card"));
    }
    observeCardOwnership();
}

bool Effect::pairCardsIntoBento(KWin::LogicalOutput *output, KWin::EffectWindow *active,
    KWin::EffectWindow *partner, BentoSidePlacement side)
{
    if (!output || !active || !partner || partner == active) return false;
    const auto reserved = m_desktopStage->prepareCardDrop(active, output,
        KWin::RectF(active->frameGeometry()),
        DesktopStageController::CardDropIntent::ActivateBento, side, partner);
    if (!reserved) return false;
    QPointer<KWin::EffectWindow> carried = active;
    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
    // Each card's Stack waits behind the pane it becomes, as a card placed
    // alone keeps its own (placeActiveCardAlone), so no card of it is laid
    // out as a pane of its own after.
    const auto behindOf = [this](KWin::EffectWindow *card) {
        QList<KWin::EffectWindow *> behind = card ? m_cardStage->stackFrontToBack(card) : QList<KWin::EffectWindow *>{};
        behind.removeAll(card);
        return behind;
    };
    const QList<KWin::EffectWindow *> activeBehind = behindOf(active);
    const QList<KWin::EffectWindow *> partnerBehind = behindOf(named);
    if (!m_desktopStage->activatePreparedTabletDrop(*reserved, nullptr,
            [this, carried, named] { return m_cardStage->releasePairToBento(carried, named); }))
        return false;
    if (carried && !activeBehind.isEmpty()) m_cardStage->notePaneStack(carried, activeBehind);
    if (named && !partnerBehind.isEmpty()) m_cardStage->notePaneStack(named, partnerBehind);
    if (!activeBehind.isEmpty() || !partnerBehind.isEmpty()) m_cardStage->settlePaneStacks();
    return true;
}

void Effect::toggleBento()
{
    m_letGo = false;
    // §10: Meta+B lays a card the top edge left alone out with the rest.
    m_aloneOnPurpose.clear();
    if (m_carryRuntime) m_carryRuntime->cancel();
    // CARD-LIFECYCLE.md §3: external Bento while docked, tablet Bento while
    // undocked. This read the pointer before, which on a touch tablet is
    // wherever the pointer was last left rather than where the user is working.
    // With cards on every display, the other displays show one card each, so
    // the key belongs to Spread's display.
    KWin::LogicalOutput *target = m_cardsOnEveryDisplay ? nullptr : externalDesktopOutput();
    if (!target) target = tabletOutput();
    if (!target) return;
    // §11, Connected: in Spread the key never ends a layout it does not show;
    // it closes Spread on what is centred and lays out every card.
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
        && target == tabletOutput() && connectedOutputForDesktopStage(target)
        && m_cardStage->canOwnCards(target)) {
        layOutFromSpread(target);
        return;
    }
    // §3 names a pair on a display that can own cards, so a Bento action there
    // composes two named windows rather than sweeping the display. A display
    // that cannot own cards reaches neither pairing rule and composes across
    // itself, which is what an external desktop stage is for.
    if (!m_desktopStage->hasSessionOnOutput(target->name())
        && m_cardStage->canOwnCards(target)) {
        // §11, Every display: windows given back there are cards again first.
        if (m_cardsOnEveryDisplay && !m_cardStage->isActive()) (void)startTabletInCards(nullptr);
        qInfo() << "Kadunce" << Revision << "Meta+B on" << target->name() << "with no layout";
        const QPointer<KWin::EffectWindow> active = m_cardStage->activeCardIdentity();
        if (!pairActiveCardIntoBento(target)) {
            // §11, Connected: the layout Spread holds, standing behind the
            // Active card, is laid out again with that card in it, as picking
            // the card from Spread would; its panes are no partners for it.
            if (active && connectedOutputForDesktopStage(target)
                && m_cardStage->stepActiveBackToLayout()) {
                if (playPickedCardIntoLayout(active)) {
                    qInfo() << "Kadunce" << Revision << "laid the layout on" << target->name()
                            << "out again with" << logNameOf(active) << "in it";
                } else {
                    (void)m_cardStage->presentSelectedNow();
                    qInfo() << "Kadunce" << Revision << "could not lay the layout on" << target->name()
                            << "out again with" << logNameOf(active) << "in it";
                }
                observeCardOwnership();
                return;
            }
            qInfo() << "Kadunce" << Revision << "found no pair for Bento on" << target->name()
                    << "with cards" << (m_cardStage->isActive() ? "shown" : "not shown")
                    << "and" << (active ? "an" : "no") << "Active card";
            // The key did nothing, so the line says why, once.
            sayOnContextLine(QStringLiteral("hint"), gestureRefusalText(GestureRefusal::NoPartner));
        } else if (connectedOutputForDesktopStage(target)) {
            // §11, Connected: Bento auto-fill is the layout. The pair grows
            // to hold every open card of the display, as far as it has room.
            QTimer::singleShot(0, this, [this, target = QPointer(target)] { fillLayoutWithCards(target); });
        }
        observeCardOwnership();
        return;
    }
    // The panes a layout the key ends had, and the one in use.
    QList<QPointer<KWin::EffectWindow>> former = m_desktopStage->panesOn(target->name());
    QPointer<KWin::EffectWindow> inUse = KWin::effects->activeWindow();
    if (!former.contains(inUse)) inUse = former.isEmpty() ? nullptr : former.first();
    // §11, Connected: a layout standing behind the Active card, as one does
    // until Spread can hold what the top edge left, is not what the display
    // shows. The key lays it out again with that card in it, at
    // one press, rather than ending what was not shown.
    const QPointer<KWin::EffectWindow> inFront = m_cardsOnEveryDisplay && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active && connectedOutputForDesktopStage(target)
        ? m_cardStage->activeCardIdentity() : nullptr;
    qInfo() << "Kadunce" << Revision << "Meta+B on" << target->name() << "with a layout of"
            << former.size() << (inFront ? QString(QStringLiteral("behind ") + logNameOf(inFront)) : QStringLiteral("shown"));
    (void)m_desktopStage->toggleOnOutput(target->name());
    observeCardOwnership();
    // §11, Every display: a layout ended by the key leaves its windows as
    // cards, the one in use Active, so the key pairs them again. With cards
    // already held behind the layout, its panes join them.
    if (!m_cardsOnEveryDisplay || !m_cardStage->canOwnCards(target)) return;
    // Each pane's Stack is a Stack in the row again, whole, so the key lays
    // out only its face.
    onThisStageLater(0, [this, former, inUse, inFront, target = QPointer(target)] {
        if (!m_cardStage->isActive()) {
            (void)startTabletInCards(inUse);
            m_cardStage->settlePaneStacks();
            return;
        }
        if (!target || m_desktopStage->hasSessionOnOutput(target->name())) return;
        for (const auto &window : former)
            if (window && !window->isDeleted() && window->screen() == target
                && m_cardStage->liveCardIndex(window) < 0)
                (void)m_cardStage->admitArrivalAsCard(window);
        m_cardStage->settlePaneStacks();
        KWin::EffectWindow *front = inFront && !inFront->isDeleted()
            && m_cardStage->liveCardIndex(inFront) >= 0 ? inFront.data() : nullptr;
        KWin::EffectWindow *active = front ? front : inUse.data();
        if (active && m_cardStage->liveCardIndex(active) >= 0) (void)m_cardStage->promoteToActive(active);
        qInfo() << "Kadunce" << Revision << "ended the layout on" << target->name()
                << "into cards," << (active ? logNameOf(active) : QString()) << "Active";
        if (front && pairActiveCardIntoBento(target)) {
            qInfo() << "Kadunce" << Revision << "laid the layout on" << target->name()
                    << "out again with" << logNameOf(front) << "in it";
            QTimer::singleShot(0, this, [this, target] { fillLayoutWithCards(target); });
        }
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    });
}

void Effect::layOutFromSpread(KWin::LogicalOutput *output)
{
    const QPointer<KWin::EffectWindow> picked = selectedWindow();
    if (!picked || picked->isDeleted() || nativeWindowInteractionForInput()
        || m_cardStage->cardGrabActive() || m_cardStage->launcherGuestActive()) return;
    qInfo() << "Kadunce" << Revision << "Meta+B in Spread on" << output->name()
            << "with" << logNameOf(picked) << "centred";
    // What is centred opens as picking it would: a pane's card on its layout,
    // any other card joining the layout Spread holds, or paired with the card
    // in use, or shown alone; a sleeping card wakes as the Active card.
    if (m_cardStage->selectedIsBentoGroup()) {
        if (!m_cardStage->resumeSelectedBentoProjection()) return;
    } else if (picked->isMinimized()) {
        KWin::effects->activateWindow(picked);
    } else if (!playPickedCardIntoLayout(picked)) {
        if (!m_cardStage->presentSelectedNow()) return;
    }
    observeCardOwnership();
    onThisStageLater(0, [this, picked, target = QPointer(output)] {
        if (!target || m_releasing || !m_cardStage->isActive()) return;
        if (!m_desktopStage->hasSessionOnOutput(target->name())) {
            if (!pairActiveCardIntoBento(target)) {
                qInfo() << "Kadunce" << Revision << "found no pair for Meta+B from Spread on" << target->name();
                sayOnContextLine(QStringLiteral("hint"), gestureRefusalText(GestureRefusal::NoPartner));
                return;
            }
            observeCardOwnership();
        }
        if (picked && !picked->isDeleted() && m_desktopStage->managesWindow(picked))
            KWin::effects->activateWindow(picked);
        QTimer::singleShot(0, this, [this, target] { fillLayoutWithCards(target); });
    });
}

void Effect::fillLayoutWithCards(KWin::LogicalOutput *output)
{
    if (!output || m_releasing || !m_cardStage->presentsBento()
        || !m_desktopStage->hasSessionOnOutput(output->name())) return;
    // The front of each Stack takes a pane; the cards behind it wait there.
    QList<KWin::EffectWindow *> fronts;
    for (const auto &card : m_cardStage->liveCards()) {
        if (!card || card->isDeleted() || card->isMinimized()
            || card->screen() != output || m_cardStage->paneHolding(card)) continue;
        const auto stack = m_cardStage->stackFrontToBack(card);
        if (!stack.isEmpty() && stack.first() != card) continue;
        fronts.append(card);
    }
    int filled = 0;
    for (KWin::EffectWindow *card : std::as_const(fronts)) {
        const QPointer<KWin::EffectWindow> front = card;
        if (!front || front->isDeleted() || m_cardStage->liveCardIndex(front) < 0) continue;
        const QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(front).mid(1);
        if (!m_desktopStage->admitCardToLiveBento(front,
                [this, front] { return m_cardStage->releaseCardToLiveBento(front); }, true)) break;
        if (!behind.isEmpty()) m_cardStage->notePaneStack(front, behind);
        ++filled;
    }
    if (filled == 0) return;
    m_cardStage->settlePaneStacks();
    qInfo() << "Kadunce" << Revision << "filled the layout on" << output->name() << "with"
            << filled << "more cards";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

bool Effect::toggleBentoOnOutput(const QString &outputName)
{
    if (!callerMayDrive()) return false;
    const bool toggled = m_desktopStage->toggleOnOutput(outputName);
    observeCardOwnership();
    return toggled;
}

bool Effect::handoffBentoLeadToOutput(
    const QString &sourceName, const QString &destinationName)
{
    if (!callerMayDrive()) return false;
    return m_desktopStage->handoffLeadToOutput(
        sourceName, destinationName);
}

void Effect::cardGutterChangedForCardStage()
{
    m_desktopStage->relayAllSessions();
    KWin::effects->addRepaintFull();
}

void Effect::startPaneArrival(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                              const QRectF &from)
{
    if (!window || window->isDeleted() || !window->window() || !output || !from.isValid()
        || window->window()->moveResizeOutput() != output) return;
    const QRectF to(window->window()->moveResizeGeometry());
    if (to.isEmpty() || to == from) return;
    // The window is already on its pane, sized once; only its picture travels,
    // replacing whatever motion the layout gave it. It waits where it was let
    // go for its client to draw the pane's shape, a short while at most, and
    // then moves the whole way (PaneArrival.h).
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (it->window != window) { ++it; continue; }
        unredirect(window);
        it = m_bentoMotions.erase(it);
    }
    BentoMotion arrival{window, output, from, to, QRectF(output->geometry())};
    arrival.arrival = true;
    arrival.asked.start();
    m_bentoMotions.append(arrival);
    KWin::effects->addRepaintFull();
}

void Effect::toggle()
{
    toggleOwnedPresentation();
}

void Effect::toggleOwnedPresentation(bool growToActive)
{
    m_letGo = false;
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    {
        // §2: the live session's panes become cards in Spread, whether or not
        // this stage already owns individual cards beside them.
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && m_desktopStage->hasSessionOnOutput(tablet->name())) {
            m_desktopStage->transferTabletSessionToSpread(tablet,
                [this](const auto &projection, const auto &commit) {
                    return m_cardStage->admitBentoStack(projection, commit);
                });
            m_cardStage->standPanesInCardPlace();
            // Rejection retains Bento; never fall through to rediscovery.
            observeCardOwnership();
            return;
        }
    }
    // From the desktop, Spread opens whatever is selected in it.
    if (m_cardStage->selectedIsBentoGroup()
        && m_cardStage->presentation() != CardPresentation::Desktop) {
        if (!m_cardStage->resumeSelectedBentoProjection()) {
            qWarning() << "Kadunce" << Revision
                       << "declined to resume the selected Bento layout";
        }
        observeCardOwnership();
        return;
    }
    m_cardStage->toggle(growToActive);
    observeCardOwnership();
}

void Effect::release()
{
    // Windows given back here may come back from minimized; that is no pick.
    const bool wasReleasing = std::exchange(m_releasing, true);
    const auto settled = qScopeGuard([this, wasReleasing] { m_releasing = wasReleasing; });
    m_letGo = true;
    m_letGoOnDisplay.clear();
    qInfo() << "Kadunce" << Revision << "lets every window go";
    if (m_carryRuntime) m_carryRuntime->cancel();
    // §13 returns every managed window, whichever desktop holds it.
    bool hadBento = false;
    bool hadCards = false;
    forEachSession([&] {
        if (hasActiveDesktopStage()) {
            hadBento = true;
            m_desktopStage->stopPendingSettle();
            m_desktopStage->restoreAllSessions();
        }
        // Each display's cards go back where their records hold, on that display.
        forEachDisplayStage([&](CardStageController &stage) {
            hadCards = hadCards || stage.isActive();
            stage.release();
            // Kept, a Stack would stand behind that window again when cards
            // next start, though every window is its own now.
            stage.forgetAllPaneStacks();
        });
    });
    // Stacks carried to desktops not entered yet stay windows.
    m_pendingDecks.clear();
    // Every window is its own now: none remembers a layout it left, or being
    // kept alone, when cards next start.
    m_leftLayouts.clear();
    m_aloneOnPurpose.clear();
    returnDependentWindows();
    m_paintingOutput = nullptr;
    if (!hadCards && hadBento) {
        qInfo() << "Kadunce" << Revision
                << "released output-local Bento";
    }
    observeCardOwnership();
}

void Effect::pageLeft()
{
    pageHorizontal(-1);
}

void Effect::pageRight()
{
    pageHorizontal(1);
}

void Effect::pageHorizontal(int delta)
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    m_cardStage->pageHorizontal(delta);
}

void Effect::pageStackUp()
{
    pageStack(-1);
}

void Effect::pageStackDown()
{
    pageStack(1);
}

void Effect::pageStack(int delta)
{
    if (m_cardStage->launcherGuestActive()) {
        dismissLauncherGuestFromInput();
    }
    if (stepPaneStack(delta)) return;
    if (m_cardStage->stepActiveStack(delta)) {
        if (auto *shown = m_cardStage->activeCardIdentity()) {
            appearInPlace(shown);
            stepInUnseen(shown);
            KWin::effects->activateWindow(shown);
        }
        return;
    }
    m_cardStage->pageStack(delta);
}

bool Effect::stepPaneStack(int delta)
{
    // CARD-LIFECYCLE.md §11: the focused pane's Stack steps in place. The
    // pane's slot shows the next card, and the layout keeps its shape.
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    // Its own display's layout, Kadunce showing it as Bento or as the desktop.
    if (pane->screen() != tabletOutput() || !connectedForCardStage()
        || (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)) return false;
    return stepPane(pane, delta);
}

bool Effect::stackPaneOnto(KWin::EffectWindow *pane, KWin::EffectWindow *onto)
{
    if (!pane || !onto || pane == onto || !m_desktopStage->managesWindow(pane)
        || !m_desktopStage->managesWindow(onto)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(onto));
    const QList<KWin::EffectWindow *> paneBehind = m_cardStage->paneStack(pane);
    const QList<KWin::EffectWindow *> ontoBehind = m_cardStage->paneStack(onto);
    KWin::EffectWindow *next = paneBehind.isEmpty() ? nullptr : paneBehind.first();
    if (!m_desktopStage->stackPaneOnto(pane, onto, next,
            [this, next] {
                return !next || m_cardStage->releaseCardToLiveBento(next);
            })) return false;
    // The pane is the Stack's front, the one it covered next, and that one's
    // Stack after it; its own Stack stays with the slot it left.
    QList<KWin::EffectWindow *> stack{onto};
    stack.append(ontoBehind);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->forgetPaneStack(onto);
    m_cardStage->notePaneStack(pane, stack);
    if (next) {
        m_cardStage->notePaneStack(next, paneBehind.mid(1));
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    KWin::effects->activateWindow(pane);
    qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(pane) << "on" << logNameOf(onto);
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::followStackScrub(QPointF contact)
{
    if (m_stackScrub.active()) {
        // A card closed or the pane gone mid-scrub leaves nothing to order.
        if (!m_stackScrubPane
            || int(m_cardStage->paneStack(m_stackScrubPane).size()) + 1 != m_stackScrub.size) {
            if (m_carryRuntime) m_carryRuntime->cancel();
            return true;
        }
        // Shown on the hold alone: drawn aside or down, it folds away and
        // the pane is carried as before.
        if (m_stackScrubHeldStill && !stackScrubHeldStillGoesOn()) return false;
        m_stackScrub.follow(stackScrubHand(contact.y()));
        refreshStackScrubShown();
        KWin::effects->addRepaintFull();
        return true;
    }
    if (!m_stackScrubArmed || !m_carryRuntime) return false;
    const auto held = m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner());
    if (!held || !m_stackScrubPane) { m_stackScrubArmed = false; return false; }
    const QPointF travel = held->travel;
    const auto heldMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - held->pressedAt).count();
    switch (stackScrubStart(travel.x(), travel.y(), heldMs)) {
    case StackScrubStart::Undecided: return false;
    case StackScrubStart::Carry: m_stackScrubArmed = false; return false;
    case StackScrubStart::Scrub: break;
    }
    beginStackScrub(held->pressedPosition.y());
    m_stackScrub.follow(stackScrubHand(contact.y()));
    refreshStackScrubShown();
    KWin::effects->addRepaintFull();
    return true;
}

double Effect::stackScrubHand(double pointerY) const
{
    // A pane in the top row has its title bar at the screen's edge, where the
    // pointer stops; the hand's own travel still counts.
    if (!m_carryRuntime) return pointerY;
    const auto held = m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner());
    return held ? held->pressedPosition.y() + held->travel.y() : pointerY;
}

bool Effect::stackScrubHeldStillGoesOn()
{
    const auto held = m_carryRuntime
        ? m_carryRuntime->observer.contacts().held(m_carryRuntime->route.owner()) : std::nullopt;
    if (!held) return true;
    switch (stackScrubStart(held->travel.x(), held->travel.y(), StackScrubHoldMs)) {
    case StackScrubStart::Undecided: return true;
    case StackScrubStart::Scrub: m_stackScrubHeldStill = false; return true;
    case StackScrubStart::Carry: break;
    }
    qInfo() << "Kadunce" << Revision << "folds the ladder of" << logNameOf(m_stackScrubPane)
            << "and carries it";
    endStackScrub();
    return false;
}

void Effect::beginStackScrub(double y)
{
    m_stackScrubArmed = false;
    if (!m_stackScrubPane) return;
    m_stackScrub.begin(int(m_cardStage->paneStack(m_stackScrubPane).size()) + 1, 0, y);
    // Whatever the carry aimed at before the hold was told is withdrawn.
    m_carryDestination.reset(); m_carryPreview.reset();
    m_carryCardEntryOutput.clear(); m_carryCardExitOutput.clear();
    m_carryStackOnto.clear(); m_paneStackAim.clear(); m_paneStackDwell.stop();
    if (m_carryRuntime) m_carryRuntime->handoff.withdrawDrop();
    qInfo() << "Kadunce" << Revision << "scrubs the Stack of" << logNameOf(m_stackScrubPane)
            << "of" << m_stackScrub.size;
}

void Effect::endStackScrub()
{
    const bool shown = m_stackScrub.active();
    m_stackScrub.clear();
    m_stackScrubByKeys = false;
    m_stackScrubArmed = false;
    m_stackScrubHeldStill = false;
    m_stackScrubPane.clear();
    m_stackScrubShown.clear();
    if (shown) KWin::effects->addRepaintFull();
}

void Effect::refreshStackScrubShown()
{
    // The cards behind are hidden while the layout shows; the ones on the
    // ladder are drawn there, and only there.
    QSet<KWin::EffectWindow *> ladder;
    if (m_stackScrub.active() && m_stackScrubPane) {
        const auto order = stackScrubOrder(m_stackScrubPane.data(),
            m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
        const int end = std::min<int>(order.size(), m_stackScrub.ladderStart() + m_stackScrub.ladderCount());
        for (int i = m_stackScrub.ladderStart(); i < end; ++i)
            if (order.at(i) && order.at(i) != m_stackScrubPane) ladder.insert(order.at(i));
    }
    for (auto it = m_stackScrubShown.begin(); it != m_stackScrubShown.end();)
        it = ladder.contains(it.key()) ? std::next(it) : m_stackScrubShown.erase(it);
    for (auto *window : std::as_const(ladder))
        if (!m_stackScrubShown.contains(window) && !window->isDeleted())
            m_stackScrubShown.insert(window, KWin::EffectWindowVisibleRef(window,
                KWin::EffectWindow::PAINT_DISABLED | KWin::EffectWindow::PAINT_DISABLED_BY_MINIMIZE
                | KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
}

std::optional<std::pair<QRectF, QRectF>> Effect::stackScrubRung(const KWin::EffectWindow *window) const
{
    if (!window || !m_stackScrub.active() || !m_stackScrubPane || m_stackScrubPane->isDeleted())
        return std::nullopt;
    const auto order = stackScrubOrder(m_stackScrubPane.data(),
        m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
    const int index = order.indexOf(const_cast<KWin::EffectWindow *>(window));
    const int rung = index - m_stackScrub.ladderStart();
    const int count = std::min<int>(m_stackScrub.ladderCount(), order.size() - m_stackScrub.ladderStart());
    if (index < 0 || rung < 0 || rung >= count) return std::nullopt;
    const QRectF pane(m_stackScrubPane->frameGeometry());
    const auto box = stackLadderRung(pane.x(), pane.y(), pane.width(), pane.height(), rung, count);
    const QRectF rect(box.x, box.y, box.width, box.height);
    // The card in front shows whole; each behind it, its title bar.
    return std::pair{rect, rung == 0 ? rect : QRectF(rect.x(), rect.y(), rect.width(), StackLadderRung)};
}

bool Effect::reorderPaneStack(KWin::EffectWindow *pane, int place)
{
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (behind.isEmpty() || place <= 0) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> order = stackScrubOrder(pane, behind, place);
    KWin::EffectWindow *face = order.first();
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(face, order.mid(1));
    if (!m_desktopStage->replacePaneWithCard(pane, face,
            [this, face] {
                return m_cardStage->releaseCardToLiveBento(face);
            })) {
        m_cardStage->forgetPaneStack(face);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    KWin::effects->activateWindow(face);
    qInfo() << "Kadunce" << Revision << "put" << logNameOf(pane) << "at place" << place + 1
            << "of its Stack; " << logNameOf(face) << "is in front";
    KWin::effects->addRepaintFull();
    return true;
}

double Effect::stackLeanNow() const
{
    const int duration = motion(150);
    if (!m_stackLeanClock.isValid() || duration <= 1) return m_stackLeanTo ? 1.0 : 0.0;
    const double t = std::min(1.0, double(m_stackLeanClock.elapsed()) / duration);
    const double eased = 1.0 - (1.0 - t) * (1.0 - t);
    return m_stackLeanFrom + ((m_stackLeanTo ? 1.0 : 0.0) - m_stackLeanFrom) * eased;
}

double Effect::stackLean()
{
    // §11: a window rested where it would join a Stack shrinks a little and
    // tilts, as a card does joining a deck, and straightens off it again.
    const bool to = m_carriedWindow && m_carryStackOnto;
    if (to != m_stackLeanTo) {
        m_stackLeanFrom = stackLeanNow();
        m_stackLeanTo = to;
        m_stackLeanClock.start();
    }
    const double lean = stackLeanNow();
    if (lean != (m_stackLeanTo ? 1.0 : 0.0)) KWin::effects->addRepaintFull();
    return lean;
}

QRectF Effect::leanedIntoStack(const QRectF &rect, double lean) const
{
    // Shrunk to 80% about the pointer, so what is under the hand stays there.
    if (lean <= 0.0) return rect;
    const double scale = 1.0 - 0.2 * lean;
    const QPointF hand = m_lastCarryContact;
    return QRectF(hand + (rect.topLeft() - hand) * scale, rect.size() * scale);
}

void Effect::drawStackLadderOutline(const KWin::RenderTarget &renderTarget,
    const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
    KWin::LogicalOutput *screen)
{
    // §11, Connected: a pane holding a Stack shows nothing of it at rest. Held
    // and drawn up, the card on its ladder is outlined, so its place shows.
    if (!m_destinationShader || !screen || !m_stackScrubPane || m_stackScrubPane->screen() != screen) return;
    if (const auto rung = stackScrubRung(m_stackScrubPane))
        drawDestinationOutline(renderTarget, viewport, deviceRegion, screen, rung->first);
}

bool Effect::callPaneStackCard(KWin::EffectWindow *pane, KWin::EffectWindow *card)
{
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    const int at = behind.indexOf(card);
    if (at < 0) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    // As if stepped to it: the cards it passed go round to the back, in turn.
    const QList<KWin::EffectWindow *> ring = behind.mid(at + 1) + QList<KWin::EffectWindow *>{pane} + behind.mid(0, at);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(card, ring);
    appearInPlace(card);
    stepInUnseen(card);
    if (!m_desktopStage->replacePaneWithCard(pane, card,
            [this, card] {
                return m_cardStage->releaseCardToLiveBento(card);
            })) {
        m_cardStage->forgetPaneStack(card);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(card) << "to the front of its Stack's pane";
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::stepPane(KWin::EffectWindow *pane, int delta)
{
    if (!pane || !m_desktopStage->managesWindow(pane)) return false;
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (behind.isEmpty()) return false;
    KWin::EffectWindow *next = delta > 0 ? behind.first() : behind.last();
    // The ring after the step: going forward the old front goes to the back,
    // going back it returns to the front.
    QList<KWin::EffectWindow *> ring = behind;
    ring.removeAll(next);
    if (delta > 0) ring.append(pane);
    else ring.prepend(pane);
    m_cardStage->forgetPaneStack(pane);
    m_cardStage->notePaneStack(next, ring);
    appearInPlace(next);
    stepInUnseen(next);
    if (!m_desktopStage->replacePaneWithCard(pane, next,
            [this, next] {
                return m_cardStage->releaseCardToLiveBento(next);
            })) {
        m_cardStage->forgetPaneStack(next);
        m_cardStage->notePaneStack(pane, behind);
        return false;
    }
    m_cardStage->settlePaneStacks();
    observeCardOwnership();
    qInfo() << "Kadunce" << Revision << "stepped the Stack of" << logNameOf(pane)
            << (delta > 0 ? "forward;" : "back;") << logNameOf(next) << "is in front";
    activateShownPane(next);
    return true;
}

bool Effect::restoreLeftLayout(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind)
{
    // §11, Connected: a pane carried away from Spread's display by the keys
    // and carried back takes the side it left, beside what it left beside.
    // Stepped while it was away, its Stack comes back with another card in
    // front, which takes that place.
    // Only a display it can take back its side on uses the record up; on its
    // way through another display it keeps it.
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || front->screen() != tablet || !connectedForCardStage()
        || m_desktopStage->managesWindow(front) || m_cardStage->liveCardIndex(front) < 0) return false;
    std::optional<LeftLayout> found;
    for (auto *member : QList<KWin::EffectWindow *>{front} + behind) {
        const auto record = m_leftLayouts.constFind(member);
        if (record == m_leftLayouts.cend()) continue;
        if (!found) found = record.value();
        m_leftLayouts.erase(record);
    }
    if (!found) return false;
    const LeftLayout left = *found;
    const BentoSidePlacement side{left.right, true, true};
    bool placed = false;
    if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
        placed = m_desktopStage->admitCardToLiveBento(front,
            [this, front] { return m_cardStage->releaseCardToLiveBento(front); });
    } else {
        KWin::EffectWindow *partner = nullptr;
        for (const auto &other : left.partners)
            if (other && !other->isDeleted() && other.data() != front && !other->isMinimized()
                && other->screen() == tablet && m_cardStage->liveCardIndex(other) >= 0) {
                partner = other.data();
                break;
            }
        (void)m_cardStage->promoteToActive(front);
        placed = partner ? pairCardsIntoBento(tablet, front, partner, side)
                         : placeActiveCardAlone(tablet, side);
    }
    qInfo() << "Kadunce" << Revision << (placed ? "gave" : "could not give") << logNameOf(front)
            << "back the" << (left.right ? "right" : "left") << "pane it left";
    return placed;
}

void Effect::rejoinStack(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind)
{
    // §11, Every display: a Stack that followed its front card to this
    // display is still one Stack, whether that card arrives as a card or as
    // a pane. Members another display's stage still holds leave it for this one.
    if (!front) return;
    QList<QPointer<KWin::EffectWindow>> members;
    for (auto *member : behind) members.append(member);
    onThisStageLater(0, [this, front = QPointer(front), members] {
        if (!front || front->isDeleted() || m_releasing) return;
        QList<KWin::EffectWindow *> alive;
        // Those that cannot follow now stay one Stack where they are, behind
        // the first of them, rather than loose cards there.
        QHash<CardStageController *, QList<KWin::EffectWindow *>> left;
        for (const auto &member : members) {
            if (!member || member->isDeleted() || member == front) continue;
            CardStageController *from = stageHolding(member);
            if (from && from != m_cardStage) {
                const auto restore = from->managedRestore(member);
                const QPointer<KWin::EffectWindow> follower = member;
                if (!m_cardStage->admitCardFromOtherDisplay(member,
                        [from, follower] { return follower && from->releaseCardToStage(follower); },
                        restore ? &*restore : nullptr)) {
                    left[from].append(member.data());
                    continue;
                }
            }
            alive.append(member.data());
        }
        for (auto it = left.cbegin(); it != left.cend(); ++it)
            if (it.value().size() > 1) (void)it.key()->stackBehind(it.value().first(), it.value().mid(1));
        for (const auto &display : m_scopedSession->displays)
            if (display.second->cards.get() != m_cardStage) display.second->cards->forgetPaneStack(front);
        (void)restoreLeftLayout(front, alive);
        QList<QPointer<KWin::EffectWindow>> kept;
        for (auto *member : std::as_const(alive)) kept.append(member);
        if (!kept.isEmpty()) keepStackBehind(front, kept, 10);
    });
}

void Effect::keepStackBehind(KWin::EffectWindow *front, const QList<QPointer<KWin::EffectWindow>> &behind, int tries)
{
    // §11, Every display: the cards that came with `front` wait behind it
    // here, behind its pane if a layout took it, or in its Stack in the row.
    // A layout takes it in its own time, and until then it is neither, so it
    // is looked at again.
    if (!front || front->isDeleted() || m_releasing) return;
    QList<KWin::EffectWindow *> alive;
    for (const auto &member : behind)
        if (member && !member->isDeleted() && member != front && m_cardStage->liveCardIndex(member) >= 0)
            alive.append(member.data());
    if (alive.isEmpty()) return;
    if (m_desktopStage->managesWindow(front)) {
        m_cardStage->notePaneStack(front, alive);
        m_cardStage->settlePaneStacks();
    } else if (m_cardStage->liveCardIndex(front) < 0) {
        if (tries > 0) {
            onThisStageLater(50, [this, front = QPointer(front), behind, tries] {
                if (front) keepStackBehind(front, behind, tries - 1);
            });
            return;
        }
        qInfo() << "Kadunce" << Revision << "could not stack" << alive.size()
                << "cards behind" << logNameOf(front) << "again: it is neither a card nor a pane here";
        return;
    } else if (!m_cardStage->stackBehind(front, alive)) {
        qInfo() << "Kadunce" << Revision << "could not stack" << alive.size()
                << "cards behind" << logNameOf(front) << "again";
        return;
    }
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(front) << "back with"
            << alive.size() << "behind it";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::activateShownPane(KWin::EffectWindow *pane)
{
    // The card that now shows has the focus, so the next key reaches it. It
    // may only be shown the turn after, so the focus is asked for again then.
    KWin::effects->activateWindow(pane);
    const QPointer<KWin::EffectWindow> shown = pane;
    QTimer::singleShot(0, this, [shown] {
        if (shown && !shown->isDeleted() && KWin::effects->activeWindow() != shown)
            KWin::effects->activateWindow(shown);
    });
}

} // namespace Kadunce

/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The row: paging, the launcher guest, row drags, lifting and throwing,
// opening Spread, and the row's motion.
#include "CardStageInternal.h"

namespace Kadunce
{

namespace
{
// Let go short of halfway, Spread opening under three fingers goes back to the
// Active card over this long.
constexpr int OpenSpreadReturnDuration = 220;
// How long a flicked app has to close before its card comes back, and the
// most a card that asked a question waits to be answered in Spread.
constexpr int ThrownCloseWait = 2000;
// How long a flicked app that draws after it was asked to close has to go
// before what it drew counts as a question asked inside its own window.
constexpr int ThrownQuestionWait = 400;
}

void CardStageController::pageHorizontal(int delta)
{
    if (!m_active || m_presentation == CardPresentation::Bento) {
        return;
    }
    m_host->cancelInputForCardStage();
    dropRowMotion();
    if (m_arrivalWindow) clearCardTransition();
    finishCardGrab(false);
    const bool wasActive = m_presentation == CardPresentation::Active;
    const int requested = delta;
    // An open card's side swipe goes round its Stack, which it never leaves;
    // a card on its own walks the row. In Spread the row moves.
    bool activeStack = false;
    if (wasActive) {
        const ActiveStep step = activeStep(m_workspace.stackSizeForId(m_workspace.selectedId()),
            m_workspace.selectedIndex(), m_workspace.count(), delta);
        activeStack = step.along == ActiveStep::Along::Stack;
        delta = step.delta;
    } else if (!m_launcherGuestActive) {
        // The row has two ends; paging stops at them.
        const int selectedIndex = m_workspace.selectedIndex();
        delta = std::clamp(delta, -selectedIndex, m_workspace.count() - 1 - selectedIndex);
    }
    KWin::EffectWindow *leaving = wasActive ? selectedWindow() : nullptr;
    if (delta == 0) {
        // At a row end the Active card gives a little and settles back.
        if (wasActive && leaving && requested != 0 && !m_activeSlide.tracking)
            startActiveSlide(leaving, nullptr, requested > 0 ? 1 : -1, 0.0, ActiveSlideGive,
                             ActiveSlideMotion::Shape::Give, ActiveSlideDuration);
        return;
    }
    if (!wasActive && !m_launcherGuestActive) {
        captureCardTransition(false, true);
        m_rowPageTransition = true;
    }
    if (wasActive) {
        parkActiveSnapshot();
    }
    if (activeStack) {
        m_workspace.pageStack(delta);
    } else {
        m_workspace.page(delta);
        // Arriving at a Stack from beside it, an open card starts at its
        // near end, so the walk goes through every card.
        const auto members = m_workspace.model().stackMembersForId(m_workspace.selectedId());
        if (wasActive && members.size() > 1)
            (void)m_workspace.selectCard(delta > 0 ? members.front() : members.back());
    }
    if (wasActive && !enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    if (wasActive && m_presentation == CardPresentation::Active)
        continueActiveSlide(leaving, delta > 0 ? 1 : -1);
    if (!wasActive) anchorRowTransition();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce horizontal navigation selected"
            << m_workspace.selectedId() << "of" << m_workspace.count();
}

bool CardStageController::beginLauncherGuest()
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_workspace.windows().isEmpty() || m_cardGrabActive) {
        return false;
    }
    m_host->cancelInputForCardStage();
    dropRowMotion();
    captureCardTransition();
    m_arrivalTimer.stop();
    m_arrivalWindow.clear();
    m_arrivalExpanding = false;
    m_growingChosen = false;
    m_launcherGuestGroupCount = m_workspace.count();
    m_launcherGuestArrival = false;
    m_launcherGuestPrimarySide = m_workspace.count() == 2
        ? -rowNeighborSide() : 1;
    m_launcherGuestPrimaryWindow = selectedWindow();
    m_launcherGuestSecondaryWindow = m_workspace.count() > 1
        ? m_workspace.windows().value(m_workspace.idAtOffset(-1) - 1) : nullptr;
    m_launcherGuestOffset = 0.0;
    m_launcherGuestTransitionFrom = 0.0;
    m_launcherGuestTransitionTimer.invalidate();
    m_launcherGuestPendingPage = 0;
    m_launcherGuestActive = true;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::updateLauncherGuest(double horizontalDelta)
{
    if (!m_launcherGuestActive) {
        return;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) {
        return;
    }
    const KWin::RectF work = workArea(tablet);
    const SpreadLayout layout = makeSpreadLayout(
        work.x(), work.y(), work.width(), work.height());
    const double pitch = layout.cards[1].width + layout.gutter;
    m_launcherGuestOffset = std::clamp(
        horizontalDelta, -pitch, pitch);
    KWin::effects->addRepaintFull();
}

bool CardStageController::finishLauncherGuest(double horizontalDelta)
{
    if (!m_launcherGuestActive) {
        return false;
    }
    if (std::abs(horizontalDelta) < LauncherGuestCommitDistance) {
        m_launcherGuestOffset = 0.0;
        m_launcherGuestTransitionFrom = 0.0;
        m_launcherGuestTransitionTimer.invalidate();
        KWin::effects->addRepaintFull();
        return false;
    }

    const double previewFrom = std::clamp(
        std::abs(m_launcherGuestOffset) / LauncherGuestCommitDistance,
        0.0, 1.0) * LauncherGuestDragPreview;
    m_launcherGuestOffset = horizontalDelta > 0.0
        ? LauncherGuestCommitDistance : -LauncherGuestCommitDistance;
    m_launcherGuestTransitionFrom = previewFrom;
    m_launcherGuestTransitionTimer.restart();
    const int incomingSide = horizontalDelta > 0.0 ? -1 : 1;
    m_launcherGuestPendingPage = m_workspace.count() > 1
        && incomingSide != m_launcherGuestPrimarySide ? -1 : 0;
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::endLauncherGuest()
{
    if (!m_launcherGuestActive && qFuzzyIsNull(m_launcherGuestOffset)) {
        return;
    }
    m_host->cancelInputForCardStage();
    const bool easesPair = !m_launcherGuestArrival && m_workspace.count() == 2
        && m_launcherGuestTransitionTimer.isValid();
    // Search closing in Spread: the cards beside it glide back into the row
    // from where they stood around it, as they glided apart for it.
    if (!easesPair && m_launcherGuestActive && !m_launcherGuestArrival && m_active
        && m_presentation == CardPresentation::Spread) {
        captureCardTransition(true);
    }
    if (!m_launcherGuestArrival && m_launcherGuestPendingPage != 0) {
        m_workspace.page(m_launcherGuestPendingPage);
    }
    // Keep the guest's established landing intact, then ease that landing
    // into the larger pair. The shoulder stays on the side it actually used.
    if (easesPair) {
        m_workspace.setPairNeighborSide(m_launcherGuestOffset < 0.0 ? -1 : 1);
        if (auto *output = m_host->tabletOutputForCardStage()) {
            const auto work = workArea(output);
            if (work.width() > 0 && work.height() > 0) {
                m_previewOrigins.clear();
                for (const auto &window : std::as_const(m_workspace.windows())) {
                    if (!window || window->isDeleted()) continue;
                    const int slot = m_workspace.sameStack(liveCardIndex(window) + 1,
                        m_workspace.selectedId()) ? 0 : rowNeighborSide();
                    const auto rect = slot == 0 ? cardTargetForSlot(output, slot)
                        : launcherGuestTargetForSlot(output, slot);
                    m_previewOrigins.append({window, QRectF(
                        (rect.x() - work.x()) / work.width(),
                        (rect.y() - work.y()) / work.height(),
                        rect.width() / work.width(), rect.height() / work.height())});
                }
                m_previewTransition.start();
            }
        }
    }
    m_launcherGuestPendingPage = 0;
    m_launcherGuestActive = false;
    m_launcherGuestArrival = false;
    m_launcherGuestOffset = 0.0;
    m_launcherGuestPrimaryWindow.clear();
    m_launcherGuestSecondaryWindow.clear();
    m_launcherGuestTransitionFrom = 0.0;
    m_launcherGuestTransitionTimer.invalidate();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::endLauncherGuestOnCard(const QPointF &position)
{
    // Read the card while the row still stands around Search: the card
    // there is not the one under that point once the row closes up.
    auto *tablet = m_host->tabletOutputForCardStage();
    KWin::EffectWindow *window = m_launcherGuestActive && !m_launcherGuestArrival && tablet
        ? cardAt(tablet, position) : nullptr;
    const int cardId = window ? liveCardIndex(window) + 1 : 0;
    endLauncherGuest();
    const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
    if (entry < 0) return false;
    if (entry != m_workspace.selectedIndex()) m_workspace.selectIndex(entry);
    qInfo() << "Kadunce" << Revision << "tap beside Search opens entry" << entry + 1;
    return true;
}

void CardStageController::pageStack(int delta)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || delta == 0
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) {
        return;
    }
    m_host->cancelInputForCardStage();
    dropRowMotion();
    if (m_arrivalWindow) clearCardTransition(); // Explicit browsing wins over auto-expand.
    finishCardGrab(false);
    browseStack(delta);
}

bool CardStageController::stepActiveStack(int delta)
{
    if (!m_active || m_launcherGuestActive || m_presentation != CardPresentation::Active
        || delta == 0
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) return false;
    finishCardGrab(false);
    KWin::EffectWindow *leaving = selectedWindow();
    parkActiveSnapshot();
    m_workspace.pageStack(delta);
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
        return false;
    }
    continueActiveSlide(leaving, delta > 0 ? 1 : -1);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce stepped the Active card's Stack to position"
            << m_workspace.stackActivePositionForId(m_workspace.selectedId()) + 1
            << "of" << m_workspace.stackSizeForId(m_workspace.selectedId());
    return true;
}

void CardStageController::browseStack(int delta)
{
    // Capture complete fan poses, including an interrupted browse transition.
    // All input routes already share this action; do not add device-specific motion.
    captureCardTransition(false, true);
    m_stackBrowseOutgoing = selectedWindow();
    m_workspace.pageStack(delta);
    m_stackBrowseDirection = delta < 0 ? -1 : 1;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce stack selected member"
            << m_workspace.selectedId() << "position"
            << m_workspace.stackActivePositionForId(m_workspace.selectedId()) + 1
            << "of" << m_workspace.stackSizeForId(m_workspace.selectedId());
}

bool CardStageController::catchRow()
{
    if (!m_row.moving() || std::abs(m_row.velocity) <= RowCatchSpeed) return false;
    m_row.mode = RowMotion::Mode::Rest;
    m_row.velocity = 0.0;
    return true;
}

void CardStageController::settleRowFromStroke()
{
    if (m_row.mode != RowMotion::Mode::Rest || m_row.position == 0.0) return;
    settleRow(m_row, rowStops(m_host->tabletOutputForCardStage()));
    m_motionClock.start();
    KWin::effects->addRepaintFull();
}

bool CardStageController::beginRowDrag()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !output
        || m_cardGrabActive || m_launcherGuestActive || m_workspace.count() == 0
        || m_lift.phase != Lift::Phase::None) {
        return false;
    }
    // A canned page or an arrival still playing gives way to the hand.
    if (m_arrivalWindow || m_rowPageTransition) clearCardTransition();
    m_rowDragOrigin = rowUnstretch(m_row.position, rowStops(output));
    m_row.mode = RowMotion::Mode::Drag;
    m_row.velocity = 0.0;
    return true;
}

void CardStageController::updateRowDrag(double travel)
{
    if (m_row.mode != RowMotion::Mode::Drag) return;
    m_row.position = rowStretch(m_rowDragOrigin - travel,
                                rowStops(m_host->tabletOutputForCardStage()));
    KWin::effects->addRepaintFull();
}

void CardStageController::finishRowDrag(double velocity)
{
    if (m_row.mode != RowMotion::Mode::Drag) return;
    const RowStops row = rowStops(m_host->tabletOutputForCardStage());
    releaseRow(m_row, row, -velocity * 1000.0);
    // How hard a real hand throws is what the row's friction is judged by.
    const int aim = rowStopIndex(row, m_row.target);
    qInfo() << "Kadunce row let go at" << std::abs(velocity) << "px/ms from entry"
            << m_workspace.selectedIndex() + 1 << "of" << m_workspace.count() << "toward"
            << (m_row.mode == RowMotion::Mode::Coast ? QStringLiteral("an end")
                : aim < 0 ? QStringLiteral("nowhere") : QString::number(aim + 1));
    m_motionClock.start();
    KWin::effects->addRepaintFull();
}

bool CardStageController::beginScrub()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_cardGrabActive
        || m_launcherGuestActive || !m_row.still() || m_lift.phase != Lift::Phase::None
        || m_workspace.stackSizeForId(m_workspace.selectedId()) <= 1) {
        return false;
    }
    if (m_arrivalWindow) clearCardTransition();
    m_scrubbing = true;
    m_scrubApplied = 0;
    return true;
}

void CardStageController::updateScrub(double travel)
{
    if (!m_scrubbing) return;
    const int steps = scrubSteps(travel);
    if (steps == m_scrubApplied) return;
    browseStack(steps - m_scrubApplied);
    m_scrubApplied = steps;
}

void CardStageController::finishScrub()
{
    m_scrubbing = false;
    m_scrubApplied = 0;
}

KWin::EffectWindow *CardStageController::cardAt(KWin::LogicalOutput *output,
                                                const QPointF &position) const
{
    // Nearest the eye first: the centred face, then every other face, then
    // the cards fanned behind them.
    QList<KWin::EffectWindow *> order;
    if (auto *selected = selectedWindow()) order.append(selected);
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto &window : std::as_const(m_workspace.windows())) {
            if (!window || window->isDeleted() || order.contains(window.data())
                || thrownAway(window) || paintSlot(window) == 99) continue;
            const int cardId = liveCardIndex(window) + 1;
            const bool face = m_workspace.model().stackPositionForId(cardId)
                == m_workspace.model().stackActivePositionForId(cardId);
            if (face == (pass == 0)) order.append(window.data());
        }
    }
    for (auto *window : std::as_const(order)) {
        if (paintSlot(window) == 99) continue;
        auto rect = previewTargetForWindow(output, window);
        auto pose = stackPoseForWindow(window, rect.width());
        if (!pose.visible) continue;
        rect.translate(qRound(pose.x), qRound(pose.y));
        if (QRectF(rect).contains(position)) return window;
    }
    return nullptr;
}

bool CardStageController::beginLift(const QPointF &position)
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !output
        || m_cardGrabActive || m_launcherGuestActive || m_lift.phase != Lift::Phase::None) {
        return false;
    }
    auto *window = cardAt(output, position);
    if (!window) return false;
    // A pane's card is a card like any other: a flick up closes its
    // application, and a pull down takes it out of its layout as one takes a
    // card out of its Stack.
    const bool inGroup = inProjectedLayout(window);
    if (m_arrivalWindow) clearCardTransition();
    m_lift = {window, 0.0, 0.0,
              inGroup || m_workspace.stackSizeForId(liveCardIndex(window) + 1) > 1,
              inGroup, Lift::Phase::Drag};
    KWin::effects->setElevatedWindow(window, true);
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::updateLift(double travel)
{
    if (m_lift.phase != Lift::Phase::Drag) return;
    m_lift.y = liftShown(travel, m_lift.inStack);
    KWin::effects->addRepaintFull();
}

void CardStageController::finishLift(double velocity)
{
    if (m_lift.phase != Lift::Phase::Drag) return;
    if (!m_lift.window || m_lift.window->isDeleted()) {
        endLift();
        return;
    }
    m_motionClock.start();
    switch (liftOutcome(m_lift.y, velocity, m_lift.inStack)) {
    case LiftOutcome::Close:
        throwLifted(velocity);
        break;
    case LiftOutcome::PullOut:
        pullOutLifted();
        break;
    case LiftOutcome::Return:
        m_lift.phase = Lift::Phase::Return;
        m_lift.velocity = velocity * 1000.0;
        break;
    }
    KWin::effects->addRepaintFull();
}

void CardStageController::throwLifted(double velocity)
{
    const QPointer<KWin::EffectWindow> window = m_lift.window;
    m_lift.phase = Lift::Phase::Throw;
    m_lift.velocity = std::min(velocity * 1000.0, -CloseThrowSpeed);
    qInfo() << "Kadunce" << Revision << "flicked card" << liveCardIndex(window) + 1
            << "closed:" << logNameOf(window);
    forgetCloseAsk(window);
    CloseAsk ask{window, {}, {}, {}};
    ask.asked.start();
    ask.damage = QObject::connect(window, &KWin::EffectWindow::windowDamaged, &m_thrownTimer,
                                  [this](KWin::EffectWindow *drawn) {
        for (auto &ask : m_closeAsks)
            if (ask.window == drawn && !ask.drew.isValid()) ask.drew.start();
    });
    m_closeAsks.append(ask);
    // Ask as it leaves the hand, so a quick app is gone by the time the card is.
    window->closeWindow();
}

void CardStageController::pullOutLifted()
{
    const QPointer<KWin::EffectWindow> window = m_lift.window;
    const bool group = m_lift.inGroup;
    // The card travels from where the finger left it to its own place.
    captureCardTransition(false, true);
    endLift();
    (void)pullOut(window, group);
}

bool CardStageController::pullOutSelected()
{
    auto *window = selectedWindow();
    if (!m_active || m_presentation != CardPresentation::Spread || !window || window->isDeleted()
        || m_cardGrabActive || m_launcherGuestActive || m_lift.phase != Lift::Phase::None) {
        return false;
    }
    const bool group = inProjectedLayout(window);
    // A card in no Stack has nothing to leave.
    if (!group && m_workspace.stackSizeForId(liveCardIndex(window) + 1) <= 1) return false;
    if (m_arrivalWindow) clearCardTransition();
    // The card travels from the Stack to its own place.
    captureCardTransition();
    return pullOut(window, group);
}

bool CardStageController::letActiveCardOutOfStack()
{
    auto *window = activeCardIdentity();
    if (!window || m_presentation != CardPresentation::Active || m_cardGrabActive
        || m_launcherGuestActive || m_lift.phase != Lift::Phase::None) return false;
    const int cardId = liveCardIndex(window) + 1;
    if (m_workspace.stackSizeForId(cardId) <= 1 || !m_workspace.releaseMember(cardId)) return false;
    // Selection stayed with the Stack, which now shows its next card.
    (void)selectCardEntry(window);
    syncSelectedStackingOrder();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "let the Active card" << cardId
            << "out of its Stack; it now stands alone at entry"
            << m_workspace.model().entryIndexForId(cardId) + 1;
    return true;
}

bool CardStageController::pullOut(KWin::EffectWindow *window, bool group)
{
    const int cardId = liveCardIndex(window) + 1;
    // §11, Connected: a pane at the front of a Stack leaves the Stack as any
    // Stack's card does, and the Stack keeps the pane with its next card.
    KWin::EffectWindow *next = nullptr;
    if (group && cardId > 0 && m_workspace.stackSizeForId(cardId) > 1)
        for (auto *member : stackFrontToBack(window))
            if (member != window) { next = member; break; }
    KWin::EffectWindow *holder = group ? nullptr : paneHolding(window);
    const bool pulled = cardId > 0
        && (next ? handProjectedPaneTo(window, next) && m_workspace.releaseMember(cardId)
            : group ? takePaneOutOfLayout(window)
                    : m_workspace.releaseMember(cardId));
    if (cardId <= 0 || !pulled) {
        clearCardTransition();
        KWin::effects->addRepaintFull();
        return false;
    }
    m_slideLeavers = true;
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "pulled card" << cardId
            << (group ? "out of its layout" : "out of its Stack")
            << "; it now stands alone at entry"
            << m_workspace.model().entryIndexForId(cardId) + 1;
    return true;
}

bool CardStageController::takePaneOutOfLayout(KWin::EffectWindow *pane)
{
    return !m_cardGrabActive && detachProjectedPane(pane);
}

bool CardStageController::handProjectedPaneTo(KWin::EffectWindow *pane, KWin::EffectWindow *card)
{
    if (!pane || !card || pane == card || !m_bentoProjectionSession
        || !inProjectedLayout(pane) || inProjectedLayout(card) || liveCardIndex(card) < 0
        || !isEligiblePartner(card)) return false;
    const auto restore = managedRestore(card);
    if (!restore) return false;
    BentoProjectionSession projection = *m_bentoProjectionSession;
    const auto paneAt = std::find_if(projection.panes.begin(), projection.panes.end(),
        [pane](const auto &member) { return member.window == pane; });
    if (paneAt == projection.panes.end()) return false;
    *paneAt = {card, *restore, false};
    if (projection.lead == pane) projection.lead = card;
    if (projection.sideWindow == pane) projection.sideWindow = card;
    // §11, Connected: the Stack keeps the snapped card's place, and so its standing.
    if (projection.snappedWindow == pane) projection.snappedWindow = card;
    for (auto &stacked : projection.stackingOrder)
        if (stacked == pane) stacked = card;
    // The card stands in the place the pane was given.
    if (projection.placed)
        for (auto &column : projection.placed->columns)
            for (auto &slot : column)
                if (!slot.open && slot.window == pane) slot.window = card;
    if (!validBentoProjectionShape(projectionShape(projection))) return false;
    *m_bentoProjectionSession = projection;
    for (auto *list : {&m_bentoProjectionWindows, &m_bentoProjectionPaneWindows}) {
        const int at = list->indexOf(pane);
        if (at >= 0) (*list)[at] = card;
    }
    KWin::effects->setElevatedWindow(pane, false);
    m_host->retireBentoProjectionForCardStage({pane});
    // §3: the card stands in the Active card's place, as every card does.
    if (!pane->isMinimized()) settleCardsInActivePlace({pane});
    ++m_restoreGeneration;
    qInfo() << "Kadunce" << Revision << "handed the pane of" << logNameOf(pane)
            << "to" << logNameOf(card);
    return true;
}

bool CardStageController::detachProjectedPane(KWin::EffectWindow *pane)
{
    if (!pane || !inProjectedLayout(pane) || liveCardIndex(pane) < 0)
        return false;
    BentoProjectionSession projection = *m_bentoProjectionSession;
    const auto paneAt = std::find_if(projection.panes.begin(), projection.panes.end(),
        [pane](const auto &member) { return member.window == pane; });
    if (paneAt != projection.panes.end()) {
        projection.panes.erase(paneAt);
    } else {
        projection.sleeping.removeIf([pane](const auto &member) { return member.window == pane; });
    }
    projection.stackingOrder.removeAll(pane);
    if (projection.lead == pane)
        projection.lead = projection.panes.isEmpty() ? nullptr : projection.panes.first().window;
    if (projection.sideWindow == pane) {
        projection.sideWindow.clear();
        projection.side.reset();
    }
    if (projection.snappedWindow == pane) {
        projection.snappedWindow.clear();
        projection.snappedSide.reset();
    }
    // §11, Connected: the columns a side snap placed keep the panes left, and
    // the one taken out leaves its room open.
    std::vector<BentoRect> placedRects;
    if (projection.placed) {
        bentoColumnsRemove(*projection.placed, QPointer<KWin::EffectWindow>(pane));
        std::vector<QPointer<KWin::EffectWindow>> handles;
        std::vector<BentoRect> rects;
        bentoColumnsLayout(*projection.placed, handles, rects);
        // Columns a pair began stand no lone column (BentoColumns.h), so the
        // panes left by such a pair are laid out afresh.
        const bool lone = projection.placed->paired && projection.placed->columns.size() < 2;
        for (const auto &member : std::as_const(projection.panes)) {
            const auto at = std::find(handles.cbegin(), handles.cend(), member.window);
            if (lone || at == handles.cend()) { placedRects.clear(); break; }
            placedRects.push_back(rects[std::size_t(at - handles.cbegin())]);
        }
        if (placedRects.empty()) projection.placed.reset();
    }
    const auto members = m_bentoProjectionWindows;
    KWin::effects->setElevatedWindow(pane, false);
    if (projection.panes.isEmpty() || (projection.panes.size() < 2 && placedRects.empty())) {
        // §5: a layout left with one pane ends; every card was already a
        // card of its own, so nothing else moves.
        m_bentoProjectionSession.reset();
        m_bentoProjectionWindows.clear();
        m_bentoProjectionPaneWindows.clear();
        m_returnToGroup = false;
        m_host->retireBentoProjectionForCardStage(members);
    } else {
        // The panes left share the layout, laid out afresh for their count,
        // or keep the places they were given.
        const auto &area = projection.workspaceArea;
        const bool landscape = bentoLandscapeArea(area.width(), area.height());
        const int count = int(projection.panes.size());
        projection.rects = !placedRects.empty() ? placedRects
            : count <= BentoCuratedPaneCap
            ? makeBentoLayout(count, landscape)
            : makeGridBentoLayout(count, bentoGridColumns(count, landscape));
        if (!validBentoProjectionShape(projectionShape(projection))) return false;
        *m_bentoProjectionSession = projection;
        m_bentoProjectionWindows.removeAll(pane);
        m_bentoProjectionPaneWindows.removeAll(pane);
        m_host->retireBentoProjectionForCardStage({pane});
    }
    // §3: the card stands in the Active card's place, as every card does.
    if (!pane->isMinimized()) settleCardsInActivePlace({pane});
    ++m_restoreGeneration;
    qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of the layout";
    return true;
}

bool CardStageController::cardAtPoint(const QPointF &position) const
{
    auto *tablet = m_host->tabletOutputForCardStage();
    return m_active && m_presentation == CardPresentation::Spread && tablet
        && cardAt(tablet, position);
}

KWin::EffectWindow *CardStageController::cardUnderPoint(const QPointF &position) const
{
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !tablet) return nullptr;
    return cardAt(tablet, position);
}

bool CardStageController::selectReturnEntry()
{
    // Where the person was: the Bento layout they opened Spread from, or the
    // card they last had open, or failing both the entry already centred.
    if (m_returnToGroup) {
        for (const auto &window : m_bentoProjectionPaneWindows) {
            const int cardId = liveCardIndex(window) + 1;
            const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
            if (entry >= 0) {
                m_workspace.selectIndex(entry);
                return true;
            }
        }
    }
    if (auto *active = activeCardIdentity()) {
        const int cardId = liveCardIndex(active) + 1;
        if (cardId > 0 && m_workspace.selectCard(cardId)) return true;
    }
    return m_workspace.count() > 0;
}

bool CardStageController::backFromSpread()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_cardGrabActive
        || m_lift.phase != Lift::Phase::None) return false;
    if (m_returnToDesktop) {
        showDesktop();
        return false;
    }
    return selectReturnEntry();
}

bool CardStageController::beginOpenSpread()
{
    if (!m_active || m_presentation != CardPresentation::Active || m_cardGrabActive
        || m_launcherGuestActive || m_lift.phase != Lift::Phase::None || m_openProgress
        || !m_host->tabletOutputForCardStage()) return false;
    m_host->cancelInputForCardStage();
    dropRowMotion();
    clearCardTransition();
    parkActiveSnapshot();
    m_presentation = CardPresentation::Spread;
    m_openProgress = 0.0;
    m_openReturn.invalidate();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "opening Spread under three fingers";
    return true;
}

void CardStageController::followOpenSpread(double progress)
{
    if (!m_openProgress || m_openReturn.isValid()) return;
    m_openProgress = std::clamp(progress, 0.0, 1.0);
    KWin::effects->addRepaintFull();
}

void CardStageController::finishOpenSpread(bool open)
{
    if (!m_openProgress || m_openReturn.isValid()) return;
    if (open) {
        // The row finishes forming from wherever the fingers left it.
        captureCardTransition();
        m_openProgress.reset();
        qInfo() << "Kadunce" << Revision << "changed to Spread under three fingers";
    } else {
        m_openReturnFrom = *m_openProgress;
        m_openReturn.start();
        m_openReturnTimer.start(motion(OpenSpreadReturnDuration));
    }
    KWin::effects->addRepaintFull();
}

double CardStageController::spreadOpenProgress() const
{
    if (!m_openProgress) return 1.0;
    if (!m_openReturn.isValid()) return *m_openProgress;
    const double u = std::clamp(double(m_openReturn.elapsed()) / motion(OpenSpreadReturnDuration), 0.0, 1.0);
    return m_openReturnFrom * (1.0 - QEasingCurve(QEasingCurve::OutCubic).valueForProgress(u));
}

void CardStageController::stopOpeningSpread()
{
    m_openReturnTimer.stop();
    m_openReturn.invalidate();
    m_openProgress.reset();
}

void CardStageController::formRowFromActive()
{
    if (m_presentation != CardPresentation::Spread) return;
    stopOpeningSpread();
    clearCardTransition();
    // Each card's origin is where an opening at no progress draws it.
    m_openProgress = 0.0;
    captureCardTransition();
    m_openProgress.reset();
}

CardStageController::SpreadTap CardStageController::tapSpread(const QPointF &position)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || m_presentation != CardPresentation::Spread || !tablet
        || m_cardGrabActive || m_launcherGuestActive || !m_row.still()
        || m_lift.phase != Lift::Phase::None) return SpreadTap::None;
    KWin::EffectWindow *window = cardAt(tablet, position);
    if (!window) {
        qInfo() << "Kadunce" << Revision << "tap on empty Spread: back to where the person was, on"
                << tablet->name() << "with" << m_workspace.count() << "entries"
                << (m_returnToDesktop ? "to the desktop" : "");
        if (m_returnToDesktop) {
            showDesktop();
            return SpreadTap::None;
        }
        return selectReturnEntry() ? SpreadTap::Open : SpreadTap::None;
    }
    const int cardId = liveCardIndex(window) + 1;
    const int entry = cardId > 0 ? m_workspace.model().entryIndexForId(cardId) : -1;
    if (entry < 0) return SpreadTap::None;
    const auto &model = m_workspace.model();
    const bool face = model.stackPositionForId(cardId) == model.stackActivePositionForId(cardId);
    if (!face && entry == m_workspace.selectedIndex()) {
        // A card fanned behind the centred Stack's face comes forward, and
        // Spread stays open to see it.
        const int before = model.stackActivePositionForId(cardId);
        captureCardTransition(false, true);
        m_stackBrowseOutgoing = selectedWindow();
        (void)m_workspace.selectCard(cardId);
        m_stackBrowseDirection = model.stackActivePositionForId(cardId) < before ? -1 : 1;
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce" << Revision << "tap brought card" << cardId << "to its Stack's face";
        return SpreadTap::Forward;
    }
    // Any other card opens: a Stack's face, a card beside it, a pane's card.
    if (entry != m_workspace.selectedIndex()) m_workspace.selectIndex(entry);
    qInfo() << "Kadunce" << Revision << "tap opens entry" << entry + 1;
    return SpreadTap::Open;
}

void CardStageController::endLift()
{
    const auto window = m_lift.window;
    m_lift = {};
    if (window && !window->isDeleted()) KWin::effects->setElevatedWindow(window, false);
    syncSelectedElevation();
}

void CardStageController::cancelStroke()
{
    auto *output = m_host->tabletOutputForCardStage();
    if (m_row.mode == RowMotion::Mode::Drag
        || (m_row.mode == RowMotion::Mode::Rest && m_row.position != 0.0)) {
        settleRow(m_row, rowStops(output));
        m_motionClock.start();
    }
    finishScrub();
    if (m_lift.phase == Lift::Phase::Drag) {
        m_lift.phase = Lift::Phase::Return;
        m_lift.velocity = 0.0;
        m_motionClock.start();
    }
    KWin::effects->addRepaintFull();
}

KWin::EffectWindow *CardStageController::activeStepPeek(int delta) const
{
    if (!m_active || m_presentation != CardPresentation::Active || delta == 0) return nullptr;
    const ActiveStep step = activeStep(m_workspace.stackSizeForId(m_workspace.selectedId()),
        m_workspace.selectedIndex(), m_workspace.count(), delta);
    if (step.along == ActiveStep::Along::None) return nullptr;
    // The same step pageHorizontal takes, on a copy of the row.
    SpreadModel model = m_workspace.model();
    if (step.along == ActiveStep::Along::Stack) {
        model.pageStack(step.delta);
    } else {
        model.page(step.delta);
        const auto members = model.stackMembersForId(model.selectedId());
        if (members.size() > 1) {
            const int wanted = step.delta > 0 ? members.front() : members.back();
            for (int turn = 0; turn < int(members.size()) && model.selectedId() != wanted; ++turn)
                model.pageStack(1);
        }
    }
    KWin::EffectWindow *peek = m_workspace.windowForId(model.selectedId());
    return peek == selectedWindow() ? nullptr : peek;
}

double CardStageController::activeSlidePitch() const
{
    auto *tablet = m_host->tabletOutputForCardStage();
    const double width = tablet ? QRectF(activeTarget(tablet)).width() : 0.0;
    return width > 0.0 ? width + ActiveSlideGap : 0.0;
}

double CardStageController::activeSlidePosition() const
{
    const auto &slide = m_activeSlide;
    if (slide.tracking || !slide.clock.isValid()) return slide.position;
    return activeSlideAt(slide.motion, slide.clock.elapsed());
}

bool CardStageController::activeSlideRunning() const
{
    const auto &slide = m_activeSlide;
    if (!slide.leaving || slide.side == 0) return false;
    if (!m_active || m_presentation != CardPresentation::Active) return false;
    return slide.tracking || (slide.clock.isValid() && !activeSlideDone(slide.motion, slide.clock.elapsed()));
}

std::optional<double> CardStageController::activeSlideShift(const KWin::EffectWindow *window) const
{
    if (!window || !activeSlideRunning()) return std::nullopt;
    const auto &slide = m_activeSlide;
    const double position = activeSlidePosition();
    if (window == slide.leaving) return activeSlideLeavingShift(position, slide.side, slide.pitch);
    if (window == slide.arriving) return activeSlideArrivingShift(position, slide.side, slide.pitch);
    return std::nullopt;
}

void CardStageController::startActiveSlide(KWin::EffectWindow *leaving, KWin::EffectWindow *arriving,
    int side, double from, double to, ActiveSlideMotion::Shape shape, int baseMs)
{
    const double pitch = activeSlidePitch();
    if (!leaving || side == 0 || pitch <= 0.0 || motionIsInstant()) {
        endActiveSlide();
        return;
    }
    m_activeSlide.leaving = leaving;
    m_activeSlide.arriving = arriving;
    m_activeSlide.side = side;
    m_activeSlide.pitch = pitch;
    m_activeSlide.tracking = false;
    m_activeSlide.position = from;
    // Carried on from the hand, the strip keeps the hand's pace at every speed
    // but instant; a canned turn follows Plasma's animation speed.
    m_activeSlide.motion = {from, to, shape == ActiveSlideMotion::Shape::Out ? baseMs : motion(baseMs), shape};
    m_activeSlide.clock.start();
    m_motionClock.start();
    KWin::effects->addRepaintFull();
}

void CardStageController::endActiveSlide()
{
    m_activeSlide = {};
    KWin::effects->addRepaintFull();
}

void CardStageController::continueActiveSlide(KWin::EffectWindow *leaving, int side)
{
    KWin::EffectWindow *arriving = selectedWindow();
    if (!leaving || !arriving || leaving == arriving) {
        endActiveSlide();
        return;
    }
    // Input during a turn carries on from what is on screen: a turn back
    // reverses the strip where it stands; any other starts from its card.
    double from = 0.0;
    auto &slide = m_activeSlide;
    if (activeSlideRunning()) {
        const double now = activeSlidePosition();
        if (slide.tracking) from = now;
        else if (slide.leaving == arriving && slide.arriving == leaving) from = 1.0 - now;
    }
    const bool fromHand = slide.tracking;
    startActiveSlide(leaving, arriving, side, from, 1.0,
                     fromHand ? ActiveSlideMotion::Shape::Out : ActiveSlideMotion::Shape::InOut,
                     ActiveSlideDuration);
}

void CardStageController::dragActiveSlide(int side, double travel)
{
    if (!m_active || m_presentation != CardPresentation::Active || side == 0) return;
    const double pitch = activeSlidePitch();
    if (pitch <= 0.0) return;
    // A swipe in the left gap moves rightward and brings the card before.
    const int arrivingSide = side < 0 ? -1 : 1;
    const double toward = side < 0 ? travel : -travel;
    KWin::EffectWindow *arriving = activeStepPeek(arrivingSide);
    auto &slide = m_activeSlide;
    slide.leaving = selectedWindow();
    slide.arriving = arriving;
    slide.side = arrivingSide;
    slide.pitch = pitch;
    slide.tracking = true;
    slide.position = activeSlideTracked(toward, pitch, arriving != nullptr);
    slide.clock.invalidate();
    KWin::effects->addRepaintFull();
}

void CardStageController::releaseActiveSlide(int side, double travel, double speed)
{
    auto &slide = m_activeSlide;
    if (!slide.tracking) {
        // A touch that never moved is a tap: the card before or after.
        pageHorizontal(side < 0 ? -1 : 1);
        return;
    }
    const double pitch = std::max(1.0, slide.pitch);
    const double toward = side < 0 ? travel : -travel;
    const double flick = (side < 0 ? speed : -speed) * 1000.0 / pitch;
    if (slide.arriving && activeSlideCommits(std::max(0.0, toward) / pitch, flick)) {
        // pageHorizontal takes the step; the strip goes on from the hand.
        pageHorizontal(side < 0 ? -1 : 1);
        return;
    }
    // Short of it, the strip springs back to the card it started on.
    KWin::EffectWindow *leaving = slide.leaving;
    KWin::EffectWindow *arriving = slide.arriving;
    const double from = slide.position;
    startActiveSlide(leaving, arriving, slide.side, from, 0.0, ActiveSlideMotion::Shape::Out,
                     ActiveSlideDuration);
}

void CardStageController::advanceMotion()
{
    if (!m_motionClock.isValid()) return;
    const double seconds = std::min(0.05, m_motionClock.restart() / 1000.0);
    auto *output = m_host->tabletOutputForCardStage();
    // At Plasma's instant speed, what the hand set going lands on this frame.
    const bool instant = motionIsInstant();
    if (m_cardGrabActive) advanceCarry(seconds);
    if (m_row.moving()) {
        if (!rowMotionApplies() || !output) m_row = {};
        else if (instant) {
            finishRow(m_row, rowStops(output));
            commitRowStop();
        } else if (!stepRow(m_row, rowStops(output), seconds)) commitRowStop();
    }
    if (m_lift.phase == Lift::Phase::Return) {
        if (instant) m_lift.y = m_lift.velocity = 0.0;
        else springToward(m_lift.y, m_lift.velocity, 0.0, LiftSpring, seconds);
        if (std::abs(m_lift.y) < 1.0 && std::abs(m_lift.velocity) < 20.0) endLift();
    } else if (m_lift.phase == Lift::Phase::Throw) {
        m_lift.y += m_lift.velocity * seconds;
        const auto window = m_lift.window;
        const auto rect = output && window ? previewTargetForWindow(output, window) : KWin::Rect();
        const auto work = output ? workArea(output) : KWin::RectF();
        if (instant || !window || window->isDeleted() || rect.width() <= 0 || rect.bottom() < work.top()) {
            // Out of sight: the next card can be lifted while this one closes.
            if (window && !window->isDeleted() && liveCardIndex(window) >= 0) {
                Thrown thrown{window, {}};
                thrown.since.start();
                m_thrown.append(thrown);
                m_thrownTimer.start();
            }
            endLift();
        }
    }
    if (!m_activeSlide.tracking && m_activeSlide.side != 0 && !activeSlideRunning()) m_activeSlide = {};
    if (!animationsRunning()) m_motionClock.invalidate();
    KWin::effects->addRepaintFull();
}

void CardStageController::commitRowStop()
{
    auto *output = m_host->tabletOutputForCardStage();
    const int entry = rowStopIndex(rowStops(output), m_row.position);
    const int offset = entry < 0 ? 0 : entry - m_workspace.selectedIndex();
    if (offset == 0) {
        m_row = {};
        KWin::effects->addRepaintFull();
        return;
    }
    // The row stands exactly where the new selection draws its cards; only a
    // Stack's fan opening or closing is left to travel.
    captureCardTransition(false, true);
    m_row = {};
    m_workspace.page(offset);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce row settled on entry" << m_workspace.selectedIndex() + 1
            << "of" << m_workspace.count();
}

void CardStageController::dropRowMotion()
{
    finishScrub();
    if (m_row.still()) return;
    const auto row = rowStops(m_host->tabletOutputForCardStage());
    m_row.position = nearestRowStop(row, m_row.position);
    m_row.mode = RowMotion::Mode::Rest;
    m_row.velocity = 0.0;
    commitRowStop();
}

double CardStageController::liftOpacity(const KWin::EffectWindow *window) const
{
    if (m_lift.phase == Lift::Phase::None || window != m_lift.window || m_lift.y >= 0.0) return 1.0;
    return std::clamp(1.0 + m_lift.y / 900.0, 0.35, 1.0);
}

bool CardStageController::thrownAway(const KWin::EffectWindow *window) const
{
    if (!window) return false;
    for (const auto &thrown : m_thrown)
        if (thrown.window == window) return true;
    return false;
}

void CardStageController::checkThrownCards()
{
    for (int i = m_thrown.size() - 1; i >= 0; --i) {
        const auto window = m_thrown.at(i).window;
        if (!window) {
            m_thrown.removeAt(i);
            continue;
        }
        // A closed window's card is gone; its thrown mark stays until KWin
        // lets the window go, so no close animation draws it full size.
        if (window->isDeleted() || liveCardIndex(window) < 0) continue;
        if (window->findModal() || askedInside(window)) returnThrownCard(window, true);
        else if (m_thrown.at(i).since.elapsed() >= ThrownCloseWait) returnThrownCard(window, false);
    }
    if (m_thrown.isEmpty()) m_thrownTimer.stop();
}

bool CardStageController::askedInside(const KWin::EffectWindow *window) const
{
    for (const auto &ask : m_closeAsks)
        if (ask.window == window)
            return ask.drew.isValid() && ask.asked.elapsed() >= ThrownQuestionWait;
    return false;
}

void CardStageController::forgetCloseAsk(const KWin::EffectWindow *window)
{
    for (int i = m_closeAsks.size() - 1; i >= 0; --i) {
        const auto &asked = m_closeAsks.at(i).window;
        if (window && asked && asked != window) continue;
        QObject::disconnect(m_closeAsks.at(i).damage);
        m_closeAsks.removeAt(i);
    }
}

void CardStageController::returnThrownCard(KWin::EffectWindow *window, bool present)
{
    m_thrown.removeIf([window](const Thrown &thrown) { return thrown.window == window; });
    qint64 asked = -1;
    for (const auto &ask : m_closeAsks)
        if (ask.window == window) asked = ask.asked.elapsed();
    forgetCloseAsk(window);
    if (!m_active || m_presentation != CardPresentation::Spread) return;
    qInfo() << "Kadunce" << Revision << "flicked card came back:" << logNameOf(window)
            << (present ? "it asked a question" : "it did not close")
            << asked << "ms after it was asked to close";
    if (m_lift.phase == Lift::Phase::None && !m_cardGrabActive) {
        // Drops back into its place from above.
        auto *output = m_host->tabletOutputForCardStage();
        const auto rect = output ? previewTargetForWindow(output, window) : KWin::Rect();
        const auto work = output ? workArea(output) : KWin::RectF();
        m_lift = {window, rect.width() > 0 ? work.top() - rect.bottom() : 0.0, 0.0,
                  false, false, Lift::Phase::Return};
        m_motionClock.start();
    }
    if (present && !m_cardGrabActive && !m_launcherGuestActive) {
        dropRowMotion();
        if (selectCardEntry(window)) {
            syncSelectedElevation();
            m_host->presentSelectedForCardStage();
        }
    }
    KWin::effects->addRepaintFull();
}

} // namespace Kadunce

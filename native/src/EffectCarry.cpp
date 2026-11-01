/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Carrying a window: the native move, where it lands and how it settles, and
// the dialogs that travel with their card.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// Whether a window still stands where a motion takes it. At a fractional
// scale KWin may land a frame part of a pixel off the rect it was asked for,
// and that is still the motion's place.
bool landsAt(const QRectF &geometry, const QRectF &to)
{
    return std::abs(geometry.left() - to.left()) < 1.0 && std::abs(geometry.top() - to.top()) < 1.0
        && std::abs(geometry.right() - to.right()) < 1.0 && std::abs(geometry.bottom() - to.bottom()) < 1.0;
}
// How far the Active card must travel under a KWin move before the cards let go.
constexpr double ActiveMoveTravel = 12.0;
}

bool Effect::isDependentWindow(const KWin::EffectWindow *window)
{
    return window && !window->isDeleted()
        && (window->isNormalWindow() || window->isDialog() || window->isUtility())
        && dependentLead(window);
}

bool Effect::dependentShown(const KWin::EffectWindow *lead) const
{
    const DesktopSession *holder = sessionHolding(lead);
    const CardStageController *cards = holder ? holder->stageHolding(lead) : nullptr;
    if (!lead || !cards) return true;
    // A card on a desktop that is not shown is not in front.
    if (holder != m_currentSession) return false;
    return cards->presentation() == CardPresentation::Active && cards->selectedWindow() == lead;
}

void Effect::scheduleDependentSync()
{
    if (m_dependentSyncQueued) return;
    m_dependentSyncQueued = true;
    // KWin activates a new window inside the call that announces it, and an
    // activation unhides; the decision runs once that call has returned.
    QTimer::singleShot(0, this, [this] { syncDependentWindows(); });
}

void Effect::syncDependentWindows()
{
    m_dependentSyncQueued = false;
    m_dependents.removeIf([](const auto &w) { return !w || w->isDeleted(); });
    m_heldDependents.removeIf([](const auto &w) { return !w || w->isDeleted(); });
    KWin::Window *focus = nullptr;
    {
        QScopedValueRollback<bool> holding(m_holdingDependents, true);
        for (auto *window : KWin::effects->stackingOrder()) {
            if (!isDependentWindow(window)) continue;
            auto *lead = dependentLead(window);
            if (!m_dependents.contains(window)) m_dependents.append(window);
            const bool held = m_heldDependents.contains(window);
            if (!dependentShown(lead)) {
                if (!window->window()->isHidden()) {
                    // Hiding the focused window would have KWin focus the next
                    // one, which for a modal dialog is the dialog again. The
                    // keys go to the card in front, and nowhere when that card
                    // is the one the dialog is waiting with.
                    if (window->window()->isActive()) {
                        auto *front = m_cardStage->selectedWindow();
                        if (front && front != lead && front->window()) {
                            KWin::workspace()->activateWindow(front->window());
                        } else {
                            KWin::workspace()->setActiveWindow(nullptr);
                            KWin::workspace()->focusToNull();
                        }
                    }
                    window->window()->setHidden(true);
                }
                if (!held) m_heldDependents.append(window);
                if (!lead->window()->isActive() && !lead->window()->isDemandingAttention()) {
                    lead->window()->demandAttention(true);
                    if (!m_waitingLeads.contains(lead->window())) m_waitingLeads.append(lead->window());
                }
            } else if (held) {
                m_heldDependents.removeAll(window);
                window->window()->setHidden(false);
                // The person came to the application, so its dialog takes the
                // keys as it would have had it opened in front.
                if (lead->window()->isActive() || window->window()->isActive()) focus = window->window();
            }
        }
        for (const auto &lead : std::as_const(m_waitingLeads)) {
            if (!lead) continue;
            const bool stillWaiting = std::any_of(m_heldDependents.cbegin(), m_heldDependents.cend(),
                [&lead](const auto &w) { return w && dependentLead(w) && dependentLead(w)->window() == lead; });
            if (!stillWaiting) lead->demandAttention(false);
        }
        m_waitingLeads.removeIf([](const auto &lead) {
            return !lead || !lead->isDemandingAttention();
        });
    }
    if (focus) KWin::workspace()->activateWindow(focus);
}

void Effect::handleWindowOutputChanged()
{
    auto *client = qobject_cast<KWin::Window *>(sender());
    const QPointer<KWin::EffectWindow> window = client ? client->effectWindow() : nullptr;
    if (!window) return;
    // The window learns how large it may grow on the display it is on now, and
    // a pane it fell short of there is asked for again within those bounds.
    if (sendDisplayBounds(client, client->moveResizeOutput()) && m_desktopStage->managesWindow(window))
        QTimer::singleShot(0, this, [this, window] {
            if (window && !window->isDeleted()) (void)m_desktopStage->replacePaneShortOfPlace(window);
        });
    // A window KWin sends to another display some other way than a display
    // change, such as its window-to-screen shortcut, is answered as one that
    // opened there. KWin reports the change inside the move that makes it, so
    // the turn after sees where the window stands. A card, a pane or a window
    // being carried already has an owner deciding where it goes.
    QTimer::singleShot(0, this, [this, window] {
        // §11, Every display: a card sent to another display that holds its
        // own cards leaves its stage for that display's, which shows it.
        KWin::LogicalOutput *there = window && window->window() ? window->window()->moveResizeOutput() : nullptr;
        if (window && !m_releasing && window->window() && !window->isUserMove()
            && window != m_carriedWindow && window != m_nativeCarry && stageHolding(window)
            && !displaysJustChanged() && isCardDisplay(there) && stageKeyOf(window) != stageKeyFor(there)) {
            (void)moveCardToDisplay(window, there, true);
            return;
        }
        // A pane sent to another display leaves its layout there as one
        // carried there by its title bar does. Spread moves no pane.
        CardStageController *keeper = window ? stageKeepingStackOf(window) : nullptr;
        const QList<KWin::EffectWindow *> behind = keeper ? keeper->paneStack(window)
                                                          : QList<KWin::EffectWindow *>{};
        if (window && !m_releasing && window->window() && !window->isUserMove()
            && !window->isUserResize() && window != m_carriedWindow && window != m_nativeCarry
            && !stageHolding(window) && !anySpreadShown()
            && m_desktopStage->handoffPaneToOutput(window, there)) {
            // §11, Every display: on a display holding its own cards and no
            // layout it is the card shown there, and its Stack goes with it.
            there = window->screen();
            qInfo() << "Kadunce" << Revision << "handed" << logNameOf(window) << "to"
                    << (there ? there->name() : QString()) << "with" << behind.size()
                    << "behind it; a pane there" << m_desktopStage->managesWindow(window);
            if (isCardDisplay(there)) {
                SessionScope scope(this, m_scopedSession, stageKeyFor(there));
                if (!m_desktopStage->managesWindow(window)) claimWindowForItsDisplay(window);
                rejoinStack(window, behind);
            }
            KWin::effects->activateWindow(window);
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
            return;
        }
        if (!window || m_releasing || !isCardWindow(window) || isDependentWindow(window)
            || !window->window() || !window->window()->readyForPainting()
            || !window->isOnCurrentDesktop() || window->isUserMove() || window->isUserResize()
            || window == m_carriedWindow || window == m_nativeCarry
            || sessionHolding(window)) return;
        KWin::LogicalOutput *tablet = tabletOutput();
        if (tablet && window->window()->moveResizeOutput() == tablet) {
            // The card display hides any window that is not a card, so the
            // arrival becomes one the way a window a display change moved does.
            scheduleCardDisplaySettle();
            return;
        }
        // A display presenting a layout takes it as a pane; one without a
        // layout leaves it an ordinary window.
        if (m_desktopStage->handleWindowAdded(window)) {
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        }
    });
}

void Effect::handleTransientChanged()
{
    auto *client = qobject_cast<KWin::Window *>(sender());
    KWin::EffectWindow *window = client ? client->effectWindow() : nullptr;
    if (!isDependentWindow(window) || m_dependents.contains(window)) return;
    KWin::EffectWindow *lead = dependentLead(window);
    // The box was what the person was looking at, so its application is.
    const bool inFront = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->selectedWindow() == window;
    const bool card = m_cardStage->liveCardIndex(window) >= 0;
    const bool pane = m_desktopStage->managesWindow(window);
    if (card) m_cardStage->handleWindowClosed(window);
    if (pane) m_desktopStage->handleWindowClosed(window);
    if (card || pane) {
        // Placed as a card or a pane; it floats over its application instead.
        const KWin::RectF over = lead->frameGeometry();
        const KWin::RectF box = client->frameGeometry();
        client->move(onLeadDisplay(over, QSizeF(box.width(), box.height()),
            QPointF(over.center().x() - box.width() / 2.0, over.center().y() - box.height() / 2.0)));
        m_cardLabelTargets.remove(window);
        m_cardFaceRects.remove(window);
        observeCardOwnership();
        qInfo() << "Kadunce" << logNameOf(window) << "belongs to" << logNameOf(lead)
                << "and waits with it rather than as a card";
    }
    m_dependents.append(window);
    if (inFront) {
        m_cardStage->handleWindowActivated(lead);
        // It had the keys as a card and keeps them over its application.
        KWin::workspace()->activateWindow(client);
    }
    scheduleDependentSync();
    Q_EMIT workspaceContextChanged();
}

void Effect::returnDependentWindows()
{
    for (const auto &window : std::as_const(m_heldDependents))
        if (window && !window->isDeleted() && window->window()) window->window()->setHidden(false);
    for (const auto &lead : std::as_const(m_waitingLeads))
        if (lead) lead->demandAttention(false);
    m_heldDependents.clear();
    m_waitingLeads.clear();
    m_drawnDialogs.clear();
    releaseCarriedDialogs();
}

void Effect::updateDrawnDialogs()
{
    const bool spread = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    bool changed = false;
    for (auto it = m_drawnDialogs.begin(); it != m_drawnDialogs.end();) {
        if (spread && m_heldDependents.contains(it->first)) { ++it; continue; }
        it = m_drawnDialogs.erase(it);
        changed = true;
    }
    if (spread) {
        for (const auto &dialog : std::as_const(m_heldDependents)) {
            if (!dialog || dialog->isDeleted() || m_drawnDialogs.contains(dialog)) continue;
            m_drawnDialogs.emplace(dialog, std::make_unique<KWin::EffectWindowVisibleRef>(
                dialog, KWin::EffectWindow::PAINT_DISABLED));
            changed = true;
        }
    }
    if (changed) KWin::effects->addRepaintFull();
}

QList<KWin::EffectWindow *> Effect::drawnDialogsOf(const KWin::EffectWindow *lead) const
{
    QList<KWin::EffectWindow *> dialogs;
    if (m_drawnDialogs.empty()) return dialogs;
    for (auto *window : KWin::effects->stackingOrder()) {
        if (m_drawnDialogs.contains(window) && !window->isDeleted() && dependentLead(window) == lead)
            dialogs.append(window);
    }
    return dialogs;
}

bool Effect::paintDialogsOn(const KWin::RenderTarget &renderTarget,
                            const KWin::RenderViewport &viewport, KWin::EffectWindow *lead,
                            const QList<KWin::EffectWindow *> &dialogs, const KWin::Region &clip,
                            const KWin::WindowPaintData &leadData)
{
    for (auto *dialog : dialogs) {
        // Its application is scaled about its own corner and moved; the same
        // mapping about the dialog's corner moves it by its offset's change.
        const QPointF offset = dialog->pos() - lead->pos();
        KWin::WindowPaintData data;
        data.setOpacity(leadData.opacity());
        data.setXScale(leadData.xScale());
        data.setYScale(leadData.yScale());
        data.setXTranslation(leadData.xTranslation() + offset.x() * (leadData.xScale() - 1.0));
        data.setYTranslation(leadData.yTranslation() + offset.y() * (leadData.yScale() - 1.0));
        if (!qFuzzyIsNull(leadData.rotationAngle())) {
            const QVector3D origin = leadData.rotationOrigin();
            data.setRotationAngle(leadData.rotationAngle());
            data.setRotationOrigin(QVector3D(origin.x() - offset.x(), origin.y() - offset.y(), origin.z()));
        }
        if (!painted([&] {
                return KWin::effects->paintWindow(renderTarget, viewport, dialog,
                    PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT, clip, data);
            }))
            return false;
    }
    return true;
}

void Effect::takeCarriedDialogs(KWin::EffectWindow *lead)
{
    releaseCarriedDialogs();
    if (!lead || lead->isDeleted() || !lead->window()) return;
    const KWin::RectF frame = lead->frameGeometry();
    if (frame.width() <= 0 || frame.height() <= 0) return;
    for (const auto &dialog : std::as_const(m_dependents)) {
        if (!dialog || dialog->isDeleted() || !dialog->window() || dependentLead(dialog) != lead) continue;
        const KWin::RectF box = dialog->frameGeometry();
        // One wider or taller than its application has no place on it that
        // way but its middle; where the screen pushed it is not one.
        m_carriedDialogs.append({dialog, QPointF(
            box.width() >= frame.width() ? 0.5 : (box.center().x() - frame.x()) / frame.width(),
            box.height() >= frame.height() ? 0.5 : (box.center().y() - frame.y()) / frame.height())});
        // KWin sends a dialog to its application's new display on its own,
        // keeping its place on the display rather than on the application.
        m_carriedDialogWatch.append(connect(dialog->window(), &KWin::Window::frameGeometryChanged,
                                            this, &Effect::placeCarriedDialogs));
    }
    if (m_carriedDialogs.isEmpty()) return;
    m_dialogsLead = lead;
    m_carriedDialogWatch.append(connect(lead->window(), &KWin::Window::frameGeometryChanged,
                                        this, &Effect::placeCarriedDialogs));
}

void Effect::placeCarriedDialogs()
{
    if (m_placingCarriedDialogs || !m_dialogsLead || m_dialogsLead->isDeleted()) return;
    QScopedValueRollback<bool> placing(m_placingCarriedDialogs, true);
    const KWin::RectF frame = m_dialogsLead->frameGeometry();
    for (const auto &carried : std::as_const(m_carriedDialogs)) {
        auto *dialog = carried.dialog.data();
        if (!dialog || dialog->isDeleted() || !dialog->window()) continue;
        const KWin::RectF box = dialog->frameGeometry();
        const QPointF to = onLeadDisplay(frame, QSizeF(box.width(), box.height()),
            QPointF(std::round(frame.x() + carried.place.x() * frame.width() - box.width() / 2.0),
                    std::round(frame.y() + carried.place.y() * frame.height() - box.height() / 2.0)));
        if (QPointF(box.x(), box.y()) != to) dialog->window()->move(to);
    }
}

QPointF Effect::onLeadDisplay(const KWin::RectF &lead, const QSizeF &size, QPointF to)
{
    auto *output = KWin::effects->screenAt(lead.center().toPoint());
    if (!output) return to;
    const KWin::RectF area = KWin::effects->clientArea(KWin::MaximizeArea, output);
    to.setX(std::max(area.x(), std::min(to.x(), area.x() + area.width() - size.width())));
    to.setY(std::max(area.y(), std::min(to.y(), area.y() + area.height() - size.height())));
    return to;
}

void Effect::releaseCarriedDialogs()
{
    m_carriedDialogsRelease.stop();
    placeCarriedDialogs();
    for (const auto &connection : std::as_const(m_carriedDialogWatch)) disconnect(connection);
    m_carriedDialogWatch.clear();
    m_carriedDialogs.clear();
    m_dialogsLead.clear();
}

QList<KWin::EffectWindow *> Effect::carriedDialogs() const
{
    QList<KWin::EffectWindow *> dialogs;
    for (const auto &carried : std::as_const(m_carriedDialogs))
        if (carried.dialog && !carried.dialog->isDeleted()) dialogs.append(carried.dialog);
    return dialogs;
}

bool Effect::drawnWithCarriedCard(const KWin::EffectWindow *window) const
{
    if (!m_dialogsLead || m_dialogsLead->isDeleted()) return false;
    const bool carried = std::any_of(m_carriedDialogs.cbegin(), m_carriedDialogs.cend(),
        [window](const auto &entry) { return entry.dialog == window; });
    if (!carried) return false;
    KWin::EffectWindow *lead = m_dialogsLead;
    return (lead == m_carriedWindow && m_carryRuntime)
        || (lead == m_settlingWindow && dropSettleRect())
        || bentoMotionRect(lead);
}

void Effect::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
    // Answered by the stage holding the window, else its display's.
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    noteDisplayInUse(window ? window->screen() : nullptr);
    if (window && window == m_letGoOnDisplay) m_letGoOnDisplay.clear();
    if (window && window->window()) {
        qInfo() << "Kadunce sees KWin begin a" << (window->isUserResize() ? "resize" : "move")
                << "of" << window->window()->resourceClass()
                << (window->isUserMove() || window->isUserResize() ? "by hand" : "by itself")
                << "at" << window->window()->frameGeometry();
        if (m_touchWitness) m_touchWitness->moveStarted();
    }
    // A card does not become an ordinary window through a resize grip.
    // Defer cancellation until KWin has finished publishing native-start; never
    // tear down its transaction recursively inside that signal.
    if (window && window->window() && window->isUserResize()
        && m_cardStage->managedRestore(window)) {
        QPointer<KWin::EffectWindow> guarded = window;
        QTimer::singleShot(0, this, [this, guarded] {
            CardStageController *cards = guarded ? stageHolding(guarded) : nullptr;
            if (!guarded || !guarded->window() || !guarded->isUserResize()
                || !cards || !cards->managedRestore(guarded)) return;
            const auto *touch = KWin::input() ? KWin::input()->touch() : nullptr;
            StaleTouchRelease::cancel(guarded->window(), touch && touch->touchPointCount() > 0);
        });
        return;
    }
    traceNativeMove(window, "native-start");
    if (window == m_settlingWindow) clearDropSettle();
    if (m_carryRuntime && m_carryRuntime->route.busy()) m_carryRuntime->cancel();
    if (m_carryRuntime && window->window() && window->isUserMove()
        && !m_carryRuntime->route.busy()) {
        CardStageController *cards = m_cardStage;
        auto source = cards->prepareNativeCarrySource(window);
        const bool fromBento = !source;
        if (!source) source = m_desktopStage->prepareNativeCarrySource(window);
        if (source) {
            m_carryPickup = QRectF(window->frameGeometry());
            QPointer<KWin::EffectWindow> guarded = window;
            if (m_carryRuntime->handoff.stage(window->window(), *source,
                [this, saved = *source, fromBento] {
                    return fromBento ? m_desktopStage->nativeCarrySourceValid(saved)
                                     : cardCarrySourceValid(saved);
                }, [this, guarded] { if (guarded) beginLegacyNativeMove(guarded); })) {
                traceNativeMove(window, source->isDesktopWindow() ? "staged-ordinary" : "staged-card");
                qInfo() << "Kadunce" << Revision << "carries" << applicationIdentity(window)
                        << "by its title bar as"
                        << (!fromBento ? "the Active card"
                            : source->isDesktopWindow() ? "an ordinary window" : "a Bento pane");
                return;
            }
            traceNativeMove(window, "stage-refused");
            qInfo() << "Kadunce" << Revision << "leaves the drag of" << applicationIdentity(window)
                    << "to KWin: the carry could not be staged";
        } else {
            traceNativeMove(window, "no-carry-source");
            // A drag Kadunce does not carry is KWin's own move, and edges do
            // nothing Kadunce's. Say why, so a drag that did nothing is legible.
            qInfo() << "Kadunce" << Revision << "leaves the drag of" << applicationIdentity(window)
                    << "to KWin: as a card," << cards->nativeCarryRefusal(window)
                    << "; as a pane," << m_desktopStage->nativeCarryRefusal(window);
        }
    }
    beginLegacyNativeMove(window);
}

void Effect::beginLegacyNativeMove(KWin::EffectWindow *window)
{
    traceNativeMove(window, "native-fallback");
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    if (isApplicationWindow(window) && window->isUserMove()) {
        m_nativeCarry = window;
        m_nativeCarrySource = window->screen() ? window->screen()->name() : QString();
        m_nativeCarryFromBento = m_desktopStage->managesWindow(window);
    }
    // An application may start a move on a tap in its own title area, which
    // then ends where it began. The cards are let go only once the Active card
    // travels, so a tap leaves them as they were.
    if (window->isUserMove() && !window->isUserResize()
        && m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->selectedWindow() == window) {
        m_heldActiveMove = window;
        m_heldActiveMoveOrigin = QRectF(window->frameGeometry()).topLeft();
        qInfo() << "Kadunce" << Revision << "keeps the cards while" << applicationIdentity(window)
                << "has not moved yet";
    } else {
        m_heldActiveMove.clear();
        m_cardStage->handleManualWindowChange(window);
    }
    m_desktopStage->handleWindowMoveResizeStarted(window);
    if (m_nativeCarry == window) {
        KWin::effects->setElevatedWindow(window, true);
        KWin::effects->addRepaintFull();
    }
}

void Effect::endNativeCarryPresentation()
{
    if (m_carriedWindow) traceNativeMove(m_carriedWindow, "presentation-ended");
    m_lastCarryDestinationTrace.clear();
    m_lastEdgeEntryLog.clear();
    if (m_carriedWindow) {
        KWin::effects->setElevatedWindow(m_carriedWindow, false);
        unredirect(m_carriedWindow);
    }
    m_carriedWindow.clear(); m_carryDestination.reset(); m_carryPreview.reset();
    syncNotesPause();
    m_stackLeanTo = false; m_stackLeanFrom = 0.0; m_stackLeanClock.invalidate();
    m_carryWidthEdge = CarryWidthEdge::None; m_carryWidth = 0.0; m_carryWidthClock.invalidate();
    m_carryWidthHeld = false;
    m_carryCardEntryOutput.clear(); m_carryCardExitOutput.clear();
    m_carryStackOnto.clear(); m_paneStackAim.clear(); m_paneStackDwell.stop();
    endStackScrub();
    if (m_dialogsLead) m_carriedDialogsRelease.start();
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

void Effect::traceNativeMove(KWin::EffectWindow *window, const char *event)
{
    // No titles, query text, coordinates or persistent journal output. Keep only
    // the latest 48 lifecycle transitions; this read-only evidence dies on unload.
    const auto *client = window ? window->window() : nullptr;
    const QJsonObject entry{
        {QStringLiteral("event"), QString::fromLatin1(event)},
        {QStringLiteral("app"), client ? client->resourceClass() : QString()},
        {QStringLiteral("window"), client ? client->internalId().toString() : QString()},
        {QStringLiteral("type"), client ? QString::fromLatin1(client->metaObject()->className()) : QString()},
        {QStringLiteral("decorated"), client && bool(client->decoration())},
        {QStringLiteral("output"), window && window->screen() ? window->screen()->name() : QString()},
        {QStringLiteral("bentoMember"), window && m_desktopStage->managesWindow(window)},
        {QStringLiteral("carrying"), bool(m_carriedWindow)},
        {QStringLiteral("rendererActive"), isActive()}
    };
    m_nativeMoveTrace.append(QString::fromUtf8(QJsonDocument(entry).toJson(QJsonDocument::Compact)));
    while (m_nativeMoveTrace.size() > 48) m_nativeMoveTrace.removeFirst();
}

QString Effect::nativeCarryState() const
{
    QJsonArray yieldingPanes;
    for (const auto &yielding : m_yieldingPanes) {
        const auto opacity = yieldingPaneOpacity(yielding.window);
        if (opacity) yieldingPanes.append(QJsonObject{
            {QStringLiteral("window"), windowIdentity(yielding.window)},
            {QStringLiteral("rect"), geometryContext(KWin::RectF(yielding.rect).toRect())},
            {QStringLiteral("opacity"), *opacity}});
    }
    QJsonArray bentoMotion;
    for (const auto &m : m_bentoMotions) {
        const auto pose = bentoMotionRect(m.window);
        if (pose) bentoMotion.append(QJsonObject{
            {QStringLiteral("window"), windowIdentity(m.window)},
            {QStringLiteral("rect"), geometryContext(KWin::RectF(*pose).toRect())},
            {QStringLiteral("target"), geometryContext(KWin::RectF(m.to).toRect())},
            {QStringLiteral("arrival"), m.arrival},
            {QStringLiteral("started"), m.timer.isValid()}});
    }
    const auto settling = dropSettleRect();
    const auto &reservation = m_carriedWindow ? m_carryDestination : m_lineDestination;
    const auto &preview = m_carriedWindow ? m_carryPreview : m_linePreview;
    // A Card Stage entry has no Bento reservation to validate; its destination
    // is the Active card target, and it shows the same placement outline.
    const bool cardEntry = m_carriedWindow ? bool(m_carryCardEntryOutput)
                                           : bool(m_lineCardEntryOutput);
    const bool cardExit = m_carriedWindow && m_carryCardExitOutput;
    const bool previewValid = (m_carriedWindow || m_cardStage->cardGrabActive()) && preview
        && (cardEntry || cardExit || (reservation && m_desktopStage->cardDropValid(*reservation)));
    auto *tablet = tabletOutput();
    auto *selected = m_cardStage->selectedWindow();
    auto lineRect = tablet && selected
        ? m_cardStage->previewTargetForWindow(tablet, selected) : KWin::Rect();
    auto linePose = m_cardStage->stackPoseForWindow(selected, lineRect.width());
    if (m_cardStage->cardGrabActive()) {
        lineRect = m_cardStage->cardGrabTarget();
        const auto offset = m_cardStage->cardGrabOffset();
        lineRect.translate(qRound(offset.x()), qRound(offset.y()));
    } else {
        lineRect.translate(qRound(linePose.x), qRound(linePose.y));
        (void)m_cardStage->applyPoseTransition(selected, lineRect, linePose);
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("dropSettling"), bool(settling)},
        {QStringLiteral("guestNeighborOpacity"), guestNeighborOpacity()},
        {QStringLiteral("bentoMotion"), bentoMotion},
        {QStringLiteral("dropRect"), settling ? geometryContext(KWin::RectF(*settling).toRect()) : QJsonObject{}},
        {QStringLiteral("dropWindow"), settling ? windowIdentity(m_settlingWindow) : QString()},
        {QStringLiteral("dropTarget"), settling ? geometryContext(KWin::RectF(m_settleTo).toRect()) : QJsonObject{}},
        {QStringLiteral("dropStarted"), settling && m_dropSettleTimer.isValid()},
        {QStringLiteral("yieldingPanes"), yieldingPanes},
        {QStringLiteral("destinationPreview"), previewValid},
        {QStringLiteral("placementOutline"),
            previewValid && (cardEntry || cardExit || reservation->showsPlacementOutline())},
        {QStringLiteral("detachPreview"),
            previewValid && (cardExit || (!cardEntry && reservation->detachesToDesktop()))},
        {QStringLiteral("rendererActive"), isActive()},
        {QStringLiteral("destinationRect"), previewValid ? geometryContext(preview->toRect()) : QJsonObject{}},
        {QStringLiteral("lineAnimating"), m_cardStage->animationsRunning()},
        {QStringLiteral("lineRotation"), linePose.rotation},
        {QStringLiteral("lineRect"), QJsonObject{{QStringLiteral("x"), lineRect.x()},
            {QStringLiteral("y"), lineRect.y()}, {QStringLiteral("width"), lineRect.width()},
            {QStringLiteral("height"), lineRect.height()}}},
        {QStringLiteral("carrying"), bool(m_carriedWindow)},
        // §11, Connected: the pane a carried pane would join in a Stack.
        {QStringLiteral("stackOnto"), m_carryStackOnto ? windowIdentity(m_carryStackOnto) : QString()},
        // How far the carried window leans into that Stack, 0 to 1.
        {QStringLiteral("stackLean"), m_carryStackOnto ? stackLeanNow() : 0.0},
        {QStringLiteral("carryWidth"), m_carryWidth},
        // §11, Connected: the cards the room a side snap left open offers,
        // in order, and where each picture stands.
        {QStringLiteral("roomOffer"), [this] {
            QJsonObject offer;
            if (!m_roomOffer) return offer;
            QJsonArray cards, tiles;
            for (const auto &card : m_roomOffer->cards) cards.append(windowIdentity(card));
            for (const auto &tile : m_roomOffer->tiles) tiles.append(geometryContext(KWin::RectF(tile).toRect()));
            offer.insert(QStringLiteral("output"), m_roomOffer->output ? m_roomOffer->output->name() : QString());
            offer.insert(QStringLiteral("room"), geometryContext(KWin::RectF(m_roomOffer->room).toRect()));
            offer.insert(QStringLiteral("cards"), cards);
            offer.insert(QStringLiteral("tiles"), tiles);
            return offer;
        }()},
        // §11, Connected: the place a scrubbed face would go in at, and the
        // cards its ladder shows, front first.
        {QStringLiteral("scrubPlace"), m_stackScrub.active() ? m_stackScrub.place : -1},
        {QStringLiteral("scrubLadder"), [this] {
            QJsonArray ladder;
            if (!m_stackScrub.active() || !m_stackScrubPane) return ladder;
            const auto order = stackScrubOrder(m_stackScrubPane.data(),
                m_cardStage->paneStack(m_stackScrubPane), m_stackScrub.place);
            for (int i = m_stackScrub.ladderStart();
                 i < std::min<int>(order.size(), m_stackScrub.ladderStart() + m_stackScrub.ladderCount()); ++i)
                ladder.append(windowIdentity(order.at(i)));
            return ladder;
        }()},
        {QStringLiteral("titleBarHold"), KWin::input() && KWin::input()->touch()
            ? KWin::input()->touch()->decorationPressId() : -1},
        {QStringLiteral("inputBusy"), m_carryRuntime && m_carryRuntime->route.busy()},
        {QStringLiteral("destination"), bool(m_carryDestination) || bool(m_carryCardEntryOutput)
            || bool(m_carryCardExitOutput)},
        {QStringLiteral("lineCarrying"), m_cardStage->cardGrabActive()},
        {QStringLiteral("lineDestination"),
            bool(m_lineDestination) || bool(m_lineCardEntryOutput)},
        // What a held card would do if let go, where, and how far the row
        // under it has stepped back.
        {QStringLiteral("carryAim"), m_cardStage->carryAimName()},
        {QStringLiteral("carryIndex"), m_cardStage->carryAimIndex()},
        {QStringLiteral("carryScale"), m_cardStage->carryScale()}
    }).toJson(QJsonDocument::Compact));
}

void Effect::updateNativeCarryDestination(QPointF contact)
{
    // A Stack being scrubbed has no destination: it goes back in its pane.
    if (m_stackScrub.active()) return;
    const auto traceDestination = qScopeGuard([this] {
        const QString state = m_carryCardEntryOutput
            ? QStringLiteral("destination-card:%1").arg(m_carryCardEntryOutput->name())
            : m_carryCardExitOutput
            ? QStringLiteral("destination-exit:%1").arg(m_carryCardExitOutput->name())
            : !m_carryDestination ? QStringLiteral("destination-none")
            : QStringLiteral("destination-%1:%2")
                .arg(m_carryDestination->detachesToDesktop() ? "exit" : "placement",
                     m_carryDestination->destinationOutput()->name());
        if (state != m_lastCarryDestinationTrace) {
            m_lastCarryDestinationTrace = state;
            traceNativeMove(m_carriedWindow, qPrintable(state));
        }
    });
    const auto previousDestination = m_carryDestination;
    m_carryDestination.reset();
    m_carryCardEntryOutput.clear();
    m_carryCardExitOutput.clear();
    m_carryPreview.reset();
    m_carryRefusal.reset();
    m_carryStackOnto.clear();
    m_lastCarryContact = contact;
    // The aim is read again below only where a pane can be stacked on.
    bool aimingPane = false;
    const auto forgetPaneAim = qScopeGuard([this, &aimingPane] {
        if (aimingPane) return;
        m_paneStackAim.clear();
        m_paneStackDwell.stop();
    });
    if (!m_carriedWindow || !m_carryRuntime->handoff.source()) return;
    auto &handoff = m_carryRuntime->handoff;
    handoff.withdrawDrop(); // Leaving an exit/edge must retire its commit callback too.
    KWin::LogicalOutput *target = nullptr;
    for (auto *o : KWin::effects->screens())
        if (QRectF(o->geometry()).contains(contact)) { target = o; break; }
    KWin::effects->addRepaintFull();
    if (!target) return;
    // §11, Every display: what the carry would do there is asked of the
    // stage of the display under the contact.
    SessionScope scope(this, m_scopedSession, stageKeyFor(target));
    // The drop is checked and committed on the same display's stage.
    const auto previewDrop = [this, &handoff, desktop = m_scopedSession->desktopId, key = m_scopedKey](
        CarryDestination destination, std::function<bool()> valid,
        std::function<bool(const PreparedCarrySource &)> commit) {
        return handoff.previewDrop(std::move(destination),
            [this, desktop, key, valid = std::move(valid)] {
                const auto found = m_sessions.find(desktop);
                if (found == m_sessions.end()) return false;
                SessionScope there(this, found->second.get(), key);
                return valid();
            },
            [this, desktop, key, commit = std::move(commit)](const PreparedCarrySource &source) {
                const auto found = m_sessions.find(desktop);
                if (found == m_sessions.end()) return false;
                SessionScope there(this, found->second.get(), key);
                return commit(source);
            });
    };
    const bool bento = m_desktopStage->nativeCarrySourceValid(*handoff.source());
    const bool desktopWindow = handoff.source()->isDesktopWindow();
    const bool local = target->name() == handoff.source()->origin().output;
    const bool tablet = isTabletOutput(target);
    auto edge = carryEdgeAt(target, contact);
    // §11, Connected: a wheel turned while held away from the edges asks what
    // the top edge would, until the hand reaches a side or leaves the
    // monitors; on another display it is asked of the arrival below.
    if (m_carryWidthHeld && (!connectedOutputForDesktopStage(target) || (edge && *edge != CarryEdge::Top)))
        m_carryWidthHeld = false;
    // A pane held away from the edges keeps its place in the layout, the
    // wheel setting its column's width, until the wheel takes it to the whole
    // width, which the top edge gives.
    const bool paneWidth = m_carryWidthHeld && !edge && local && bento && !desktopWindow
        && m_carryWidth < WheelWidthWhole && m_desktopStage->holdsInPlacedColumns(target, m_carriedWindow);
    if (m_carryWidthHeld && !edge && local && !paneWidth) edge = CarryEdge::Top;
    // Every return below that leaves an edge without a drop names why, and a
    // drop that is previewed but then refused at release cannot go there.
    std::optional<GestureRefusal> refusal;
    const auto noteRefusal = qScopeGuard([&] {
        if (!edge) return;
        if (handoff.hasDrop()) refusal = GestureRefusal::Unplaced;
        if (refusal) m_carryRefusal = CarryRefusal{target, contact, *refusal};
    });
    std::optional<BentoSidePlacement> side;
    if (edge && (*edge == CarryEdge::Left || *edge == CarryEdge::Right))
        side = sideChoiceOn(target, *edge == CarryEdge::Right, contact.y(), previousDestination
                && previousDestination->destinationOutput() == target
                ? previousDestination->sidePlacement() : std::nullopt);
    // §11, Connected: the contact names a place, over, beside or under.
    if (side) side->connected = connectedOutputForDesktopStage(target);
    // A wheel turned at the edge names its width too, until the hand moves to
    // another edge. Only a monitor's top and side edges take it.
    const CarryWidthEdge widthEdge = !connectedOutputForDesktopStage(target) ? CarryWidthEdge::None
        : m_carryWidthHeld ? CarryWidthEdge::Top
        : !edge ? CarryWidthEdge::None
        : *edge == CarryEdge::Top ? CarryWidthEdge::Top
        : side ? CarryWidthEdge::Side : CarryWidthEdge::None;
    if (widthEdge != m_carryWidthEdge) {
        m_carryWidthEdge = widthEdge;
        m_carryWidth = 0.0;
        m_carryWidthClock.invalidate();
    }
    if (side && widthEdge == CarryWidthEdge::Side && m_carryWidth > 0.0) side->share = m_carryWidth;
    // Only an already-owned Bento carry may traverse the dock to the physical
    // bottom edge. This does not change panel hit testing for ordinary input.
    const QRectF area = nativeLandingAreaForOutput(target);
    // Landing clearance is separate from the physical-edge gesture target.
    const double exitBottom = area.bottom() - 10.0;
    const bool atBottomEdge = contact.y() >= double(target->geometry().bottom()) - 24.0;
    // Where a window let go at the bottom edge lands on the ordinary desktop:
    // its floating size, centred under the contact, above the dock.
    const auto exitBox = [&] {
        // KWin keeps a floating geometry only for a window it maximized, tiled
        // or made full screen; any other window's own size is the geometry it
        // had before Kadunce placed it.
        const auto &saved = handoff.source()->restoreSnapshot();
        const bool heldAside = saved.maximizeMode != KWin::MaximizeRestore
            || saved.quickTileMode != KWin::QuickTileMode{} || saved.fullScreen;
        // An empty size, which KWin reports for a window it never had to
        // restore, is no size: the next record answers instead.
        QSizeF size = QRectF(heldAside ? saved.floatingGeometry : saved.geometry).size();
        if (size.isEmpty()) size = QRectF(saved.floatingGeometry).size();
        if (size.isEmpty()) size = m_carryPickup.size();
        size.setWidth(std::clamp(size.width(), 200.0, std::max(200.0, area.width() * .8)));
        size.setHeight(std::clamp(size.height(), 150.0, std::max(150.0, area.height() * .8)));
        QRectF box(contact.x() - size.width() / 2, exitBottom - size.height(), size.width(), size.height());
        box.moveLeft(std::clamp(box.left(), area.left(), area.right() - box.width()));
        return box;
    };
    // §10: the bottom edge releases a card to the ordinary desktop, as it does
    // a pane, and the desktop is shown with it.
    if (local && tablet && !bento && !desktopWindow && atBottomEdge
        && m_cardStage->nativeCarrySourceValid(*handoff.source())) {
        const KWin::RectF box(exitBox());
        QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        QPointer<KWin::LogicalOutput> output = target;
        m_carryCardExitOutput = target;
        m_carryPreview = box;
        previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [carried, output] {
                return carried && !carried->isDeleted() && output
                    && KWin::effects->screens().contains(output.data()) && carried->screen() == output;
            },
            [this, output, box, carried](const PreparedCarrySource &source) {
                CardStageController *cards = carrySourceStage(source);
                const bool released = cards && cards->releaseNativeCarryToDesktop(source, output, box);
                if (released) m_letGoOnDisplay = carried;
                return released;
            });
        return;
    }
    if (local && bento && !desktopWindow && atBottomEdge) {
        const QRectF box = exitBox();
        const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
            KWin::RectF(box), DesktopStageController::CardDropIntent::NativeDesktop);
        if (!reserved) return;
        m_carryDestination = reserved;
        m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
        QPointer<KWin::LogicalOutput> output = target;
        QPointer<KWin::EffectWindow> released = m_carriedWindow;
        previewDrop({CarryDestinationKind::NativeDesktop, target->name(), target->name(), 0, 0},
            [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
            [this, reserved, output, released](const PreparedCarrySource &source) {
                const bool committed = m_desktopStage->transferNativeCarryToDesktop(source, *reserved);
                // Given back to the desktop, the pane is no longer the face of
                // the cards that waited behind it.
                if (committed && released && m_currentSession)
                    for (const auto &display : m_currentSession->displays)
                        display.second->cards->paneLeftWithoutCard(released);
                // §10 and §2 on the card display: the desktop is shown with
                // the pane, and what Kadunce still holds there goes aside into
                // Spread. It waits for the carry to finish, as a layout
                // retiring into Spread cancels input.
                if (committed && output && isTabletOutput(output))
                    QTimer::singleShot(0, this, [this, output, released] {
                        showDesktopWithReturned(output, released);
                    });
                return committed;
            });
        return;
    }
    // CARD-LIFECYCLE.md §3 and §10 decide what an edge action means before any
    // layout is reserved. The Spread grab asks the same question, so a side snap
    // cannot mean one thing carried from the desktop and another from Spread.
    const bool liveLayout = m_desktopStage->hasSessionOnOutput(target->name());
    // §5: a pane carried to the top edge leaves the layout it is in, which is
    // the one edge action a live layout answers by giving a window up. Every
    // other snap into one is §5's displacement and is reserved below.
    const bool extractingPane = liveLayout && edge && *edge == CarryEdge::Top
        && m_desktopStage->managesWindow(m_carriedWindow)
        && m_cardStage->canOwnCards(target);
    if (local && edge && (!liveLayout || extractingPane)) {
        const bool leftEdge = *edge == CarryEdge::Left;
        const bool sideEdge = leftEdge || *edge == CarryEdge::Right;
        // §3 names the partner from Spread order. Naming is read-only: the
        // prepared carry embeds the workspace revision, so it must not move
        // selection or the pair side.
        QString partnerReason;
        auto *partner = sideEdge
            ? m_cardStage->partnerForSideSnap(m_carriedWindow, leftEdge, &partnerReason) : nullptr;
        const auto outcome = planEdgeEntry({
            .edge = *edge,
            // §3: a display with a live layout is owned, whether or not this
            // stage also holds individual cards on it.
            .ownsDisplay = m_cardStage->ownsDisplay(target) || liveLayout,
            .canOwnCards = m_cardStage->canOwnCards(target),
            .hasBentoLayout = liveLayout,
            .carriedEligible = isCardWindow(m_carriedWindow),
            .partnerNamed = partner != nullptr,
            .carriedIsActive = m_cardStage->activeCardIdentity() == m_carriedWindow,
            .placesAlone = tablet && connectedForCardStage(),
        });
        QPointer<KWin::EffectWindow> carried = m_carriedWindow;
        QPointer<KWin::LogicalOutput> output = target;
        const auto destination = monitorDropIntent(target->name(), 0, std::nullopt, edge);
        refusal = edgeEntryRefusal(outcome, isCardWindow(m_carriedWindow));
        {
            // Once per edge and outcome in a carry: what this edge would do
            // with the window, and for a side, which partner and why.
            const char *meaning = outcome == EdgeEntryOutcome::Refuse ? "nothing (refused)"
                : outcome == EdgeEntryOutcome::Unchanged ? "nothing: no partner"
                : outcome == EdgeEntryOutcome::ComposeDisplayBento ? "a layout across the display"
                : outcome == EdgeEntryOutcome::PairIntoBento ? "a Bento pair"
                : outcome == EdgeEntryOutcome::PlaceAlone
                    ? (side && side->large ? "the card alone, beside"
                       : side && side->over ? "the card alone, over" : "the card alone, under")
                : outcome == EdgeEntryOutcome::AdoptDisplay ? "cards, starting with this window"
                : "the Active card";
            const QString line = QStringLiteral("%1 at the %2 edge would make %3%4")
                .arg(applicationIdentity(m_carriedWindow),
                     *edge == CarryEdge::Top ? QStringLiteral("top")
                         : *edge == CarryEdge::Bottom ? QStringLiteral("bottom")
                         : leftEdge ? QStringLiteral("left") : QStringLiteral("right"),
                     QString::fromLatin1(meaning),
                     sideEdge ? QStringLiteral("; partner: ") + partnerReason : QString());
            if (line != m_lastEdgeEntryLog) {
                m_lastEdgeEntryLog = line;
                qInfo() << "Kadunce" << Revision << qPrintable(line);
            }
        }
        switch (outcome) {
        case EdgeEntryOutcome::Refuse:
            return;
        case EdgeEntryOutcome::Unchanged: {
            // §11, Solo: no awake card to pair, but sleeping ones: the card
            // stays Active and the half beside its side offers them.
            if (!sideEdge || !side || !destination || !tablet || liveLayout
                || m_cardStage->activeCardIdentity() != m_carriedWindow
                || sleepingCardsOn(target).isEmpty()) return;
            refusal.reset();
            m_carryCardEntryOutput = target;
            const BentoSidePlacement place = *side;
            // The place it names: what the room leaves, across the gutter.
            const QRectF beside = pairOfferRoom(target, place);
            const QRectF whole(m_cardStage->activeTarget(target));
            const double gap = 10.0;
            const QRectF named = place.right
                ? QRectF(QPointF(beside.right() + gap, whole.top()), whole.bottomRight())
                : QRectF(whole.topLeft(), QPointF(beside.left() - gap, whole.bottom()));
            m_carryPreview = KWin::RectF(named);
            previewDrop(*destination,
                [this, carried, output] {
                    return carried && !carried->isDeleted() && output && carried->screen() == output
                        && m_cardStage->activeCardIdentity() == carried;
                },
                [this, carried, output, bento, place, named](const PreparedCarrySource &source) {
                    if (!(bento ? m_desktopStage->nativeCarrySourceValid(source) : cardCarrySourceValid(source)))
                        return false;
                    const QRectF drawn = carriedPose();
                    // It glides into the place its snap named first, and the
                    // offer opens in the rest.
                    m_cardStage->holdActivePlace(carried, KWin::RectF(named).toRect());
                    if (!m_cardStage->refitActive(carried)) {
                        m_cardStage->holdActivePlace(carried, std::nullopt);
                        return false;
                    }
                    glideIntoActive(carried, output, drawn);
                    askPairOffer(output, carried, place);
                    return true;
                });
            return;
        }
        case EdgeEntryOutcome::ComposeDisplayBento:
            break; // A display that cannot own cards reserves below as before.
        case EdgeEntryOutcome::PairIntoBento: {
            refusal = GestureRefusal::Unplaced;
            if (!side || !destination) return;
            refusal = GestureRefusal::NoRoom;
            const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
                KWin::RectF(QRectF(handoff.carry().position(), m_carryPickup.size())),
                DesktopStageController::CardDropIntent::ActivateBento, side, partner);
            if (!reserved) return;
            m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
            if (!m_carryPreview) return;
            m_carryDestination = reserved;
            previewDrop(*destination,
                [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                [this, reserved, carried, output, bento](const PreparedCarrySource &source) {
                    // Commit against the identity the reservation named, never
                    // a partner re-read after preparation.
                    QPointer<KWin::EffectWindow> named = reserved->namedPartner();
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    const bool committed = (bento ? m_desktopStage->nativeCarrySourceValid(source)
                                  : cardCarrySourceValid(source))
                        && m_desktopStage->activatePreparedTabletDrop(*reserved,
                            &source.restoreSnapshot(),
                            [this, carried, named] {
                                return m_cardStage->releasePairToBento(carried, named);
                            });
                    // It grows into its pane from where it was let go.
                    if (committed) startPaneArrival(carried, output, drawn);
                    return committed;
                });
            return;
        }
        case EdgeEntryOutcome::PlaceAlone: {
            // §11, Connected: the Active card takes the place its contact
            // names, alone. Nothing pairs, and the rest of the display stays
            // open for the next card.
            refusal = GestureRefusal::Unplaced;
            if (!side || !destination) return;
            refusal = GestureRefusal::NoRoom;
            const auto reserved = m_desktopStage->prepareCardDrop(m_carriedWindow, target,
                KWin::RectF(QRectF(handoff.carry().position(), m_carryPickup.size())),
                DesktopStageController::CardDropIntent::ActivateBento, side, nullptr, true);
            if (!reserved) return;
            m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
            if (!m_carryPreview) return;
            m_carryDestination = reserved;
            // §11, Connected: the layout the card left waits in Spread, and a
            // side snap is a purposeful act, so the layout comes back with the
            // card in the place its contact names and the others beside it.
            if (m_cardStage->hasBentoGroup() && m_cardStage->activeCardIdentity() == m_carriedWindow) {
                const BentoSidePlacement place = *side;
                previewDrop(*destination,
                    [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                    [this, carried, output, bento, place](const PreparedCarrySource &source) {
                        if (!carried || !output || !(bento ? m_desktopStage->nativeCarrySourceValid(source)
                                                           : cardCarrySourceValid(source)))
                            return false;
                        const QRectF drawn = carriedPose();
                        // After the carry has let the card go, as Meta+B would.
                        QTimer::singleShot(0, this, [this, carried, output, place, drawn] {
                            if (!carried || !output) return;
                            layOutHeldLayoutBeside(carried, output, place, drawn);
                            askRoomOffer(output);
                        });
                        return true;
                    });
                return;
            }
            previewDrop(*destination,
                [this, reserved] { return m_desktopStage->cardDropValid(*reserved); },
                [this, reserved, carried, output, bento](const PreparedCarrySource &source) {
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    const bool committed = (bento ? m_desktopStage->nativeCarrySourceValid(source)
                                  : cardCarrySourceValid(source))
                        && m_desktopStage->activatePreparedTabletDrop(*reserved,
                            &source.restoreSnapshot(),
                            [this, carried] {
                                return m_cardStage->releasePairToBento(carried, nullptr);
                            });
                    if (committed) {
                        startPaneArrival(carried, output, drawn);
                        // The room left open shows the desktop, not the cards.
                        m_cardStage->syncSelectedElevation();
                        // Asked here: leaving Active can end the carry's
                        // presentation before the release is reported.
                        askRoomOffer(output);
                    }
                    return committed;
                });
            return;
        }
        case EdgeEntryOutcome::AdoptDisplay:
        case EdgeEntryOutcome::MakeActive: {
            // §10: a card that is already owned and not Active cannot be carried
            // natively, so only adoption and a native arrival reach here.
            refusal = GestureRefusal::Unplaced;
            if (!destination) return;
            const bool adopt = outcome == EdgeEntryOutcome::AdoptDisplay;
            if (!adopt && m_cardStage->liveCardIndex(carried) >= 0) {
                // §11, Connected: the Active card itself, with the wheel
                // turned, stays the Active card at its new width.
                const double width = m_carryWidthEdge == CarryWidthEdge::Top ? m_carryWidth : 0.0;
                if (width <= 0.0 || m_cardStage->activeCardIdentity() != carried
                    || !m_carriedWindow->window()) return;
                refusal.reset();
                m_carryCardEntryOutput = target;
                const KWin::Rect whole = m_cardStage->activeTarget(target);
                m_carryPreview = KWin::RectF(width < 1.0
                    ? CardStageController::narrowedActive(whole, width, m_carriedWindow->window()->minSize().width())
                    : whole);
                previewDrop(*destination,
                    [this, carried, output] {
                        return carried && !carried->isDeleted() && output && carried->screen() == output
                            && m_cardStage->activeCardIdentity() == carried;
                    },
                    [this, carried, output, bento, width](const PreparedCarrySource &source) {
                        if (!(bento ? m_desktopStage->nativeCarrySourceValid(source) : cardCarrySourceValid(source)))
                            return false;
                        // Read before the card moves, which retires the carry's pose.
                        const QRectF drawn = carriedPose();
                        m_cardStage->setActiveWidth(carried, width);
                        if (!m_cardStage->refitActive(carried)) return false;
                        teachNaturalWidth(carried, output, width);
                        glideIntoActive(carried, output, drawn);
                        return true;
                    });
                return;
            }
            // The destination is Card Stage: the carried window lands as the
            // Active card, and adoption makes every other eligible window a
            // nonselected individual card.
            m_carryCardEntryOutput = target;
            // §11, Connected: narrowed by the wheel, kept centred.
            const double width = m_carryWidthEdge == CarryWidthEdge::Top ? m_carryWidth : 0.0;
            const KWin::Rect whole = m_cardStage->activeTarget(target);
            m_carryPreview = KWin::RectF(width > 0.0 && width < 1.0 && m_carriedWindow->window()
                ? CardStageController::narrowedActive(whole, width,
                      m_carriedWindow->window()->minSize().width())
                : whole);
            previewDrop(*destination,
                [this, carried, output, adopt] {
                    const bool valid = carried && !carried->isDeleted() && output
                        && KWin::effects->screens().contains(output.data())
                        && isCardWindow(carried) && carried->screen() == output
                        && (m_cardStage->ownsDisplay(output)
                            || m_desktopStage->hasSessionOnOutput(output->name())) != adopt;
                    if (!valid) {
                        qInfo() << "Kadunce card entry drop invalid:"
                                << "window" << bool(carried) << "output" << bool(output)
                                << "card" << (carried && isCardWindow(carried))
                                << "sameOutput" << (carried && output && carried->screen() == output)
                                << "ownsDisplay" << (output && m_cardStage->ownsDisplay(output))
                                << "bento" << (output && m_desktopStage->hasSessionOnOutput(output->name()))
                                << "adopt" << adopt;
                    }
                    return valid;
                },
                [this, carried, output, adopt, bento, extractingPane, width](const PreparedCarrySource &source) {
                    const auto sourceValid = [this, &source, bento] {
                        return bento ? m_desktopStage->nativeCarrySourceValid(source)
                                     : cardCarrySourceValid(source);
                    };
                    // Read before commit, which can retire the carry's pose.
                    const QRectF drawn = carriedPose();
                    // Let go with no wheel turned, it stands at the whole width.
                    m_cardStage->setActiveWidth(carried, width > 0.0 ? width : 1.0);
                    if (width > 0.0) teachNaturalWidth(carried, output, width);
                    if (adopt) {
                        if (!m_cardStage->adoptDisplayWithActive(carried, sourceValid)) return false;
                        glideIntoActive(carried, output, drawn);
                        return true;
                    }
                    // §5: a pane gives up Bento ownership in the same published
                    // step that makes it a card, so the layout it left is never
                    // observed still naming it. Everything else arrives without
                    // a layout to leave.
                    if (extractingPane
                        ? !m_desktopStage->extractPaneToCards(carried, sourceValid)
                        : !admitTransferredWindowToTablet(carried, output, sourceValid)) return false;
                    (void)m_cardStage->promoteToActive(carried);
                    glideIntoActive(carried, output, drawn);
                    // §10: taken out of a layout to stand alone, it is called
                    // back alone too, until Meta+B or an edge lays it out.
                    if (extractingPane && output && isTabletOutput(output)) markAloneOnPurpose(carried);
                    if (extractingPane && output && isTabletOutput(output) && connectedForCardStage()
                        && m_desktopStage->hasSessionOnOutput(output->name())) {
                        // After the glide, so the card is drawn where it lands.
                        QTimer::singleShot(motion(260), this, [this, carried, output] {
                            holdLayoutLeftBehind(carried, output);
                        });
                    }
                    return true;
                });
            return;
        }
        }
    }
    const bool stayNative = desktopWindow && !edge
        && (local || (!tablet && !m_desktopStage->hasSessionOnOutput(target->name())));
    if (local && (!bento || (!stayNative && (isPanelPoint(contact)
        || contact.y() >= target->geometry().bottom() - 48)))) return;
    // §11, Connected: a pane carried over another pane and rested there joins
    // that pane's Stack in front; let go sooner, the two swap below.
    KWin::EffectWindow *over = local && bento && !desktopWindow && !edge && !paneWidth
        && connectedOutputForDesktopStage(target)
        ? m_desktopStage->paneAt(target, contact, m_carriedWindow) : nullptr;
    // Carried in from another display and rested on Spread's Active card, or
    // on a pane there however that display is sized, as the card another
    // display shows alone is one, it lands as it would and then joins that
    // Stack.
    if (!over && !local && !desktopWindow && !edge) {
        if (tablet && m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Active) {
            KWin::EffectWindow *card = m_cardStage->selectedWindow();
            if (card && card != m_carriedWindow && !card->isDeleted()
                && QRectF(card->frameGeometry()).contains(contact)) over = card;
        }
        if (!over && m_desktopStage->hasSessionOnOutput(target->name()))
            over = m_desktopStage->paneAt(target, contact, m_carriedWindow);
    }
    QPointer<KWin::EffectWindow> stackAfter;
    if (over) {
        aimingPane = true;
        if (!m_paneStackClock.isValid()) m_paneStackClock.start();
        const long long now = m_paneStackClock.elapsed();
        if (!m_paneStackAim.aim(QPointer<KWin::EffectWindow>(over), now)) {
            m_paneStackDwell.start(int(std::max(1LL, m_paneStackAim.remaining(now))));
        } else if (!local) {
            m_paneStackDwell.stop();
            m_carryStackOnto = over;
            stackAfter = over;
        } else {
            m_paneStackDwell.stop();
            m_carryStackOnto = over;
            m_carryPreview = KWin::RectF(over->frameGeometry());
            QPointer<KWin::EffectWindow> carried = m_carriedWindow;
            QPointer<KWin::EffectWindow> onto = over;
            const auto destination = monitorDropIntent(target->name(), 0,
                std::optional<MonitorLayoutTarget>{{target->name(), 0, 0}}, std::nullopt);
            if (!destination) return;
            previewDrop(*destination,
                [this, carried, onto] {
                    return carried && onto && !carried->isDeleted() && !onto->isDeleted()
                        && m_desktopStage->managesWindow(carried) && m_desktopStage->managesWindow(onto);
                },
                [this, carried, onto](const PreparedCarrySource &) {
                    return carried && onto && stackPaneOnto(carried, onto);
                });
            return;
        }
    }
    // From here an edge asks the layout solve for room, which is the one
    // reason left for it to refuse.
    if (edge) refusal = GestureRefusal::NoRoom;
    QRectF landing(handoff.carry().position(), m_carryPickup.size());
    if (stayNative && inNativeDockReleaseZone(contact, QRectF(target->geometry()), area))
        landing = safeNativeLanding(landing, area);
    const KWin::RectF geometry(landing);
    const auto reserved = local && !desktopWindow && !side
        ? m_desktopStage->prepareLocalCardDrop(m_carriedWindow, target, geometry, contact,
              paneWidth && m_carryWidth > 0.0 ? std::optional<double>(m_carryWidth) : std::nullopt)
        : m_desktopStage->prepareCardDrop(m_carriedWindow, target, geometry,
        stayNative ? DesktopStageController::CardDropIntent::NativeDesktop
        : edge && !m_cardStage->canOwnCards(target)
             ? DesktopStageController::CardDropIntent::ActivateBento
             : DesktopStageController::CardDropIntent::OpenSpace, side);
    if (!reserved) return;
    m_carryDestination = reserved;
    m_carryPreview = m_desktopStage->cardDropPreview(*reserved);
    const bool existing = m_desktopStage->hasSessionOnOutput(target->name());
    if (side && !m_carryPreview) return;
    QPointer<KWin::LogicalOutput> output = target;
    const auto destination = stayNative
        ? monitorDropIntent(target->name(), 0, std::nullopt)
        : tablet && !existing
        ? std::optional<CarryDestination>{{CarryDestinationKind::LineGap, target->name(), target->name(), 0, 0}}
        : monitorDropIntent(target->name(), 0,
            existing ? std::optional<MonitorLayoutTarget>{{target->name(), 0, 0}} : std::nullopt, edge);
    if (!destination) return;
    if (stackAfter) m_carryPreview = KWin::RectF(stackAfter->frameGeometry());
    // §11, Connected: carried in from another display with the wheel turned,
    // it stands as this display's Active card at that width, kept centred.
    const double arrivalWidth = !local && !existing && !stackAfter && !stayNative && isCardDisplay(target)
        && m_carryWidthHeld && m_carryWidth > 0.0 && m_carriedWindow->window() ? m_carryWidth : 0.0;
    if (arrivalWidth > 0.0) {
        const KWin::Rect whole = m_cardStage->activeTarget(target);
        m_carryPreview = KWin::RectF(arrivalWidth < 1.0
            ? CardStageController::narrowedActive(whole, arrivalWidth, m_carriedWindow->window()->minSize().width())
            : whole);
    }
    // The width the wheel gave, which the application keeps from now on.
    const double taught = arrivalWidth > 0.0 ? arrivalWidth
        : paneWidth && m_carryWidth > 0.0 ? m_carryWidth
        : side && side->share > 0.0 ? side->share : 0.0;
    previewDrop(*destination,
        [this, reserved, stackAfter, aimed = bool(stackAfter)] {
            return m_desktopStage->cardDropValid(*reserved) && (!aimed || (stackAfter && !stackAfter->isDeleted()));
        },
        [this, reserved, bento, output, geometry, stackAfter, targetRect = stackAfter ? std::optional<KWin::RectF>{} : m_carryPreview,
         arrival = m_carriedWindow, arrivalWidth, taught](const PreparedCarrySource &source) {
            if (arrivalWidth > 0.0 && arrival) m_cardStage->setActiveWidth(arrival, arrivalWidth);
            const bool committed = bento
                ? m_desktopStage->transferNativeCarryToDesktop(source, *reserved)
                : carrySourceStage(source) && carrySourceStage(source)->transferNativeCarryToDesktop(source, output, geometry);
            if (committed && taught > 0.0) teachNaturalWidth(arrival, output, taught);
            if (committed && targetRect) startDropSettle(arrival, output, QRectF(geometry), QRectF(*targetRect));
            if (committed && arrivalWidth > 0.0 && output) fitArrivalWidth(arrival, output->name(), arrivalWidth, 10);
            if (committed && stackAfter) {
                QPointer<KWin::EffectWindow> carried = arrival;
                QPointer<KWin::EffectWindow> onto = stackAfter;
                const DesktopSession *landedIn = m_currentSession;
                const quint64 begun = m_carriesBegun;
                QTimer::singleShot(0, this, [this, carried, onto, landedIn, begun] {
                    stackWhenLanded(carried, onto, landedIn, begun);
                });
            }
            return committed;
        });
}

void Effect::stackWhenLanded(QPointer<KWin::EffectWindow> carried, QPointer<KWin::EffectWindow> onto,
                             const DesktopSession *landedIn, quint64 carriesBegun, int tries)
{
    if (!carried || !onto || carried->isDeleted() || onto->isDeleted() || m_releasing) return;
    // A carry since, or another desktop shown, leaves where it rested behind.
    if (m_carriesBegun != carriesBegun || m_currentSession != landedIn || !m_currentSession) {
        qInfo() << "Kadunce" << Revision << "leaves" << logNameOf(carried) << "unstacked; it moved on";
        return;
    }
    // Asked of the stage of the card it rested on.
    SessionScope scope(this, m_currentSession, stageKeyOf(onto));
    // CARD-LIFECYCLE.md §11: once it stands where it was carried, as a pane
    // of that layout or as the display's card, it joins the Stack it rested on.
    bool stacked = false;
    if (m_desktopStage->managesWindow(carried) && m_desktopStage->managesWindow(onto)) {
        stacked = stackPaneOnto(carried, onto);
    } else if (KWin::LogicalOutput *there = carried->screen();
               m_desktopStage->managesWindow(carried) && m_cardStage->liveCardIndex(onto) >= 0
               && isTabletOutput(there)) {
        // §11: the card the display showed gave its place to the arrival, a
        // pane now; it waits behind it, as a card given there with its Stack does.
        QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(onto);
        if (behind.isEmpty()) behind.append(onto);
        behind.append(m_cardStage->paneStack(carried));
        m_cardStage->notePaneStack(carried, behind);
        m_cardStage->settlePaneStacks();
        stacked = true;
        qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(carried) << "on" << logNameOf(onto)
                << "on" << there->name();
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    } else if (m_cardStage->liveCardIndex(carried) >= 0 && m_cardStage->liveCardIndex(onto) >= 0) {
        stacked = m_cardStage->stackCardOnto(carried, onto);
        if (stacked) {
            qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(carried) << "on" << logNameOf(onto);
            observeCardOwnership();
            Q_EMIT workspaceContextChanged();
        }
    }
    if (stacked) return;
    if (tries <= 0) {
        qInfo() << "Kadunce" << Revision << "could not stack" << logNameOf(carried) << "on"
                << logNameOf(onto) << "after it landed";
        return;
    }
    QTimer::singleShot(100, this, [this, carried, onto, landedIn, carriesBegun, tries] {
        stackWhenLanded(carried, onto, landedIn, carriesBegun, tries - 1);
    });
}

void Effect::showDesktopWithReturned(KWin::LogicalOutput *tablet, KWin::EffectWindow *returned)
{
    if (!tablet || !returned || returned->isDeleted()) return;
    if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
        m_desktopStage->transferTabletSessionToSpread(tablet,
            [this](const auto &projection, const auto &commit) {
                return m_cardStage->admitBentoStack(projection, commit);
            });
        // A layout that cannot become its Spread group stays, and so does
        // what it shows.
        if (m_desktopStage->hasSessionOnOutput(tablet->name())) {
            observeCardOwnership();
            return;
        }
    }
    m_cardStage->returnWindowToDesktop(returned);
    observeCardOwnership();
}

void Effect::clearDropSettle()
{
    const bool repaint = bool(m_settlingWindow);
    if (m_settlingWindow && !m_settlingWindow->isDeleted()) unredirect(m_settlingWindow);
    m_settlingWindow.clear(); m_settlingOutput.clear();
    m_dropSettleTimer.invalidate();
    m_settleAsked.invalidate();
    m_settleOnCards = false;
    m_settleDrawnMoving = false;
    if (repaint) KWin::effects->addRepaintFull();
}

QRectF Effect::carriedPose() const
{
    if (!m_carriedWindow || !m_carryRuntime) return {};
    // Where it is drawn, leaning into a Stack included, so a drop grows from there.
    return leanedIntoStack(QRectF(m_carryRuntime->handoff.carry().position(), m_carryPickup.size()),
                           m_carryStackOnto ? stackLeanNow() : 0.0);
}

bool Effect::cardDisplayPresents(KWin::EffectWindow *window) const
{
    // What the card display shows: its layout's panes while it presents the
    // layout, the Active card alone among the cards, and anything it does not
    // own as it stands.
    if (!window || !m_cardStage->isActive()) return true;
    if (m_desktopStage->managesWindow(window)) return m_cardStage->presentsBento();
    if (m_cardStage->liveCardIndex(window) >= 0)
        return m_cardStage->presentation() == CardPresentation::Active
            && m_cardStage->selectedWindow() == window;
    return true;
}

void Effect::glideIntoActive(KWin::EffectWindow *window, KWin::LogicalOutput *output, const QRectF &from)
{
    if (!window || window->isDeleted() || !window->window() || !output || !isTabletOutput(output)
        || !m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Active
        || m_cardStage->selectedWindow() != window) return;
    startDropSettle(window, output, from, QRectF(window->window()->moveResizeGeometry()), true);
}

void Effect::lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                               const QRectF &drawn)
{
    if (!scopedToCurrent()) return;
    glideIntoActive(window, output, drawn);
}

void Effect::paneYieldedForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                        const QRectF &drawn)
{
    // A pane leaving another display is a card on Spread's at once; its fade
    // would be drawn where it no longer stands.
    if (isOtherCardDisplay(output)) return;
    if (!scopedToCurrent() || !window || window->isDeleted() || !output || !drawn.isValid()
        || motion(PaneArrivalDuration) <= 1) return;
    m_yieldingPanes.removeIf([window](const auto &yielding) { return yielding.window == window; });
    YieldingPane yielding{window, output, drawn, QRectF(output->geometry()), {}};
    yielding.timer.start();
    m_yieldingPanes.append(yielding);
    KWin::effects->addRepaintFull();
}

std::optional<double> Effect::yieldingPaneOpacity(KWin::EffectWindow *window) const
{
    for (const auto &yielding : m_yieldingPanes) {
        if (yielding.window != window) continue;
        const int duration = motion(PaneArrivalDuration);
        // It fades only while it is a hidden card: shown again, as a pane or
        // as a card, it is drawn as that.
        if (!window || window->isDeleted() || window->isMinimized() || !yielding.output
            || QRectF(yielding.output->geometry()) != yielding.outputGeometry
            || yielding.timer.elapsed() >= duration
            || m_desktopStage->managesWindow(window)
            || (m_cardStage->isActive() && m_cardStage->presentation() != CardPresentation::Bento
                && isTabletOutput(yielding.output))
            || (m_cardStage->presentation() == CardPresentation::Active
                && m_cardStage->selectedWindow() == window)) return std::nullopt;
        return 1.0 - QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            double(yielding.timer.elapsed()) / duration);
    }
    return std::nullopt;
}

void Effect::clearYieldingPanes()
{
    if (m_yieldingPanes.isEmpty()) return;
    for (const auto &yielding : std::as_const(m_yieldingPanes))
        if (yielding.window && !yielding.window->isDeleted()) unredirect(yielding.window);
    m_yieldingPanes.clear();
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::bentoMotionRect(KWin::EffectWindow *window) const
{
    for (const auto &m : m_bentoMotions) {
        if (m.window != window) continue;
        // An arrival not yet drawn moving stands where the card was let go,
        // and is dropped if no frame draws it at all.
        const bool started = m.timer.isValid();
        const int duration = motion(m.arrival ? PaneArrivalDuration : 220);
        if (!window || window->isDeleted() || !window->window() || !m.output
            || (started ? m.timer.elapsed() >= duration
                        : !m.arrival || !m.asked.isValid() || m.asked.elapsed() >= PaneArrivalAbandon)
            || window->isUserMove() || window->isUserResize() || window->isMinimized()
            || QRectF(m.output->geometry()) != m.outputGeometry
            || window->window()->moveResizeOutput() != m.output
            || !landsAt(QRectF(window->window()->moveResizeGeometry()), m.to)
            // A pane is drawn moving only while its layout is what the display
            // presents; one that left it, or a layout gone into Spread, is
            // drawn as what it is now.
            || !m_desktopStage->managesWindow(window)
            || (isTabletOutput(m.output) && m_cardStage->isActive()
                && !m_cardStage->presentsBento())) return std::nullopt;
        const double t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
            paneArrivalProgress(started, started ? m.timer.elapsed() : 0, duration));
        return QRectF(m.from.topLeft()+(m.to.topLeft()-m.from.topLeft())*t,
            m.from.size()+(m.to.size()-m.from.size())*t);
    }
    return std::nullopt;
}

void Effect::appearInPlace(KWin::EffectWindow *window)
{
    // A window that has just opened, or a card a Stack steps to, appears in
    // its pane rather than flying in from where it waited.
    m_justOpened.removeIf([window](const auto &opened) {
        return !opened.first || opened.first->isDeleted() || opened.first == window
            || opened.second.elapsed() > OpenedStill;
    });
    if (!window) return;
    QElapsedTimer since;
    since.start();
    m_justOpened.append({QPointer<KWin::EffectWindow>(window), since});
}

void Effect::stepInUnseen(KWin::EffectWindow *window)
{
    // A card a Stack steps to is drawn only once it stands where it is asked
    // to, so it never shows for a frame where it waited.
    if (!window) return;
    m_steppingIn.removeIf([window](const auto &entry) { return !entry.first || entry.first == window; });
    QElapsedTimer since;
    since.start();
    m_steppingIn.append({QPointer<KWin::EffectWindow>(window), since});
    QTimer::singleShot(StepInUnseen + 16, this, [] { KWin::effects->addRepaintFull(); });
}

bool Effect::steppingInUnseen(const KWin::EffectWindow *window) const
{
    for (auto it = m_steppingIn.begin(); it != m_steppingIn.end(); ++it) {
        if (it->first != window) continue;
        if (!window->window() || it->second.elapsed() > StepInUnseen) {
            m_steppingIn.erase(it);
            return false;
        }
        const QRectF drawn(window->frameGeometry());
        const QRectF asked(window->window()->moveResizeGeometry());
        const bool there = std::abs(drawn.x() - asked.x()) < 2.0 && std::abs(drawn.y() - asked.y()) < 2.0
            && std::abs(drawn.width() - asked.width()) < 2.0 && std::abs(drawn.height() - asked.height()) < 2.0;
        if (there) {
            m_steppingIn.erase(it);
            return false;
        }
        return true;
    }
    return false;
}

bool Effect::justOpened(const KWin::EffectWindow *window) const
{
    return std::any_of(m_justOpened.cbegin(), m_justOpened.cend(), [window](const auto &opened) {
        return opened.first == window && opened.second.elapsed() <= OpenedStill;
    });
}

QRectF Effect::bentoPresentationRect(KWin::EffectWindow *window) const
{
    if (!window || window->isDeleted() || window->isMinimized() || window == m_carriedWindow)
        return {};
    if (const auto pose = bentoMotionRect(window)) return *pose;
    return QRectF(window->frameGeometry());
}

void Effect::clearBentoMotions()
{
    if (m_bentoMotions.isEmpty()) return;
    for (const auto &m : std::as_const(m_bentoMotions))
        if (m.window && !m.window->isDeleted()) unredirect(m.window);
    m_bentoMotions.clear();
    KWin::effects->addRepaintFull();
}

void Effect::animateBentoLayout(KWin::LogicalOutput *output,
    const QList<QPointer<KWin::EffectWindow>> &windows,
    const QList<QRectF> &from, const QList<QRectF> &to)
{
    // A layout on a desktop not shown settles without being watched.
    if (!scopedToCurrent()) return;
    // §11, Connected: a placed layout can open or fill room, which decides
    // whether the cards behind it are held aside. Asked after the turn, as the
    // layout is still being applied.
    if (output && isTabletOutput(output))
        QTimer::singleShot(0, this, [this] {
            if (!m_releasing && m_cardStage) m_cardStage->syncSelectedElevation();
        });
    // Geometry is already committed. This is bounded presentation only;
    // each output's changed panes share one clock and retain interrupted poses.
    for (auto it = m_bentoMotions.begin(); it != m_bentoMotions.end();) {
        if (it->output != output) { ++it; continue; }
        if (it->window && !it->window->isDeleted()) unredirect(it->window);
        it = m_bentoMotions.erase(it);
    }
    if (!output) return;
    QElapsedTimer clock; clock.start();
    for (int i = 0; i < windows.size() && i < from.size() && i < to.size(); ++i) {
        auto *w = windows[i].data();
        if (!w || w->isDeleted() || !w->window() || w->isMinimized()
            || w == m_carriedWindow || w == m_settlingWindow || !from[i].isValid()
            || from[i] == to[i] || w->window()->moveResizeOutput() != output
            || QRectF(w->window()->moveResizeGeometry()) != to[i] || justOpened(w)) continue;
        m_bentoMotions.append({w,output,from[i],to[i],QRectF(output->geometry()),clock});
    }
    if (!m_bentoMotions.isEmpty()) KWin::effects->addRepaintFull();
}

void Effect::startDropSettle(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                            const QRectF &from, const QRectF &to, bool onCards)
{
    clearDropSettle();
    // Logical commitment is not proof of native placement. Require KWin's
    // accepted target; the client's buffer may follow it late. Painting maps
    // whichever buffer is current. A move may wait a short, bounded time for
    // that buffer, as a card arriving in a pane does (PaneArrival.h), but it
    // never holds input or retries geometry.
    if (!window || window->isDeleted() || !output || (isTabletOutput(output) && !onCards)
        || !from.isValid() || !to.isValid() || from == to
        || !window->window() || window->window()->moveResizeOutput() != output
        || QRectF(window->window()->moveResizeGeometry()) != to
        || window->isUserMove() || window->isUserResize() || window->isMinimized()) return;
    m_settlingWindow = window; m_settlingOutput = output;
    m_settleFrom = from; m_settleTo = to;
    m_settleOutputGeometry = QRectF(output->geometry());
    m_settleOnCards = onCards;
    m_settleAsked.start();
    // On the card display it waits where it was let go for its client to
    // draw the shape it settles into, as a card arriving in a pane does.
    if (!onCards) m_dropSettleTimer.start();
    KWin::effects->addRepaintFull();
}

std::optional<QRectF> Effect::dropSettleRect() const
{
    const double Duration = motion(220);
    const bool started = m_dropSettleTimer.isValid();
    if (!m_settlingWindow || m_settlingWindow->isDeleted() || !m_settlingOutput
        || (started ? m_dropSettleTimer.elapsed() >= Duration
                    : !m_settleAsked.isValid() || m_settleAsked.elapsed() >= PaneArrivalAbandon)
        || (m_settleOnCards && !cardDisplayPresents(m_settlingWindow))
        || !m_settlingWindow->window()
        || m_settlingWindow->window()->moveResizeOutput() != m_settlingOutput
        || QRectF(m_settlingOutput->geometry()) != m_settleOutputGeometry
        || !landsAt(QRectF(m_settlingWindow->window()->moveResizeGeometry()), m_settleTo)
        || m_settlingWindow->isUserMove() || m_settlingWindow->isUserResize()
        || m_settlingWindow->isMinimized()) return std::nullopt;
    const auto t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        paneArrivalProgress(started, started ? m_dropSettleTimer.elapsed() : 0, int(Duration)));
    return QRectF(m_settleFrom.topLeft() + (m_settleTo.topLeft() - m_settleFrom.topLeft()) * t,
                  m_settleFrom.size() + (m_settleTo.size() - m_settleFrom.size()) * t);
}

void Effect::handleManagedStateChanged()
{
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (window->window() == sender()) {
            if (window == m_settlingWindow) clearDropSettle();
            DesktopSession *holder = sessionHolding(window);
            SessionScope scope(this, holder ? holder : m_currentSession);
            SessionScope own(this, m_scopedSession, stageKeyOf(window));
            m_cardStage->handleActiveGeometryChanged(window);
            return;
        }
    }
}

void Effect::handleWindowMoveResizeStepped(
    KWin::EffectWindow *window, const KWin::RectF &geometry)
{
    // §11, Every display: the Active card may be on its way to another
    // display holding its own cards, so where it lands decides.
    SessionScope scope(this, m_scopedSession, stageKeyOf(window));
    if (window && window == m_heldActiveMove && !carriesToAnotherCardDisplay()
        && QLineF(m_heldActiveMoveOrigin, QRectF(geometry).topLeft()).length() > ActiveMoveTravel) {
        m_heldActiveMove.clear();
        qInfo() << "Kadunce" << Revision << applicationIdentity(window) << "travelled, so the cards let go";
        m_cardStage->handleManualWindowChange(window);
    }
    m_desktopStage->handleWindowMoveResizeStepped(window, geometry);
}

void Effect::handleWindowMoveResizeFinished(KWin::EffectWindow *window)
{
    if (m_carryRuntime && m_carryRuntime->handoff.ownsNativeFinish(window->window())) return;
    if (m_carryRuntime && m_carryRuntime->deferredFor(window->window())) m_carryRuntime->clearDeferred();
    traceNativeMove(window, "native-finished");
    const bool carried = m_nativeCarry == window;
    const QString source = m_nativeCarrySource;
    const bool fromBento = m_nativeCarryFromBento;
    if (carried) {
        m_nativeCarry = nullptr;
        m_nativeCarrySource.clear();
        m_nativeCarryFromBento = false;
        KWin::effects->setElevatedWindow(window, false);
        if (m_dialogsLead) m_carriedDialogsRelease.start();
    }
    bool givenToDisplay = false;
    if (window && window == m_heldActiveMove) {
        m_heldActiveMove.clear();
        SessionScope scope(this, m_scopedSession, stageKeyOf(window));
        KWin::LogicalOutput *landed = window->screen();
        if (isOtherCardDisplay(landed) && m_cardStage->liveCardIndex(window) >= 0) {
            // §11, Every display: carried to another display holding its own
            // cards, the Active card is the card that display shows, its
            // Stack with it.
            givenToDisplay = true;
            const QPointer<KWin::EffectWindow> card = window;
            const QString there = landed->name();
            QTimer::singleShot(0, this, [this, card, there] {
                if (!card || card->isDeleted()) return;
                KWin::LogicalOutput *display = screenNamed(there);
                if (isCardDisplay(display) && moveCardToDisplay(card, display, true)) return;
                SessionScope own(this, m_scopedSession, stageKeyOf(card));
                m_cardStage->handleManualWindowChange(card);
            });
        } else if (QLineF(m_heldActiveMoveOrigin, QRectF(window->frameGeometry()).topLeft()).length() > ActiveMoveTravel) {
            qInfo() << "Kadunce" << Revision << applicationIdentity(window) << "travelled, so the cards let go";
            m_cardStage->handleManualWindowChange(window);
        } else {
            qInfo() << "Kadunce" << Revision << "keeps the cards: the move of" << applicationIdentity(window)
                    << "went nowhere";
            m_cardStage->followWorkArea(true);
        }
    }
    m_desktopStage->handleWindowMoveResizeFinished(window);
    // Use KWin's completed output assignment, not the cursor: Escape restores
    // the source output even if the pointer remains over the destination.
    if (carried && !fromBento && !givenToDisplay && window->screen()
        && window->screen()->name() != source) {
        if (isCardDisplay(window->screen())) {
            admitTransferredWindowToTablet(window, window->screen());
        } else {
            (void)m_desktopStage->admitCardWindow(window, window->screen(), window->frameGeometry());
        }
    }
    if (carried) {
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        // §11, Every display: a window carried loose onto another display
        // holding its own cards becomes a card there.
        const QPointer<KWin::EffectWindow> moved = window;
        if (!givenToDisplay)
            QTimer::singleShot(0, this, [this, moved, source] { claimWindowForItsDisplay(moved, source); });
    }
}

} // namespace Kadunce

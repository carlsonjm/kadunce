/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CardStageInternal.h"

namespace Kadunce
{

namespace
{
// How long room a panel gave up must stay free before the Active card takes it.
constexpr int WorkAreaGrowWait = 6000;
}

KWin::RectF CardStageHost::workAreaForCardStage(const KWin::LogicalOutput *output) const
{
    return KWin::effects->clientArea(KWin::MaximizeArea, output);
}

CardStageController::CardStageController(CardStageHost *host)
    : m_host(host)
{
    m_settings.onGutterChanged([this]() {
        const int moved = applyGutter();
        qInfo() << "Kadunce" << Revision << "Active card gutter is now"
                << m_settings.gutter() << "px;" << moved << "cards took it; Bento's panes stand"
                << bentoPaneGap() << "px apart";
        m_host->cardGutterChangedForCardStage();
    });
    m_arrivalTimer.setSingleShot(true);
    QObject::connect(&m_arrivalTimer, &QTimer::timeout, &m_arrivalTimer, [this]() {
        const auto window = m_arrivalWindow;
        if (!m_active || m_presentation != CardPresentation::Spread
            || !window || window->isDeleted() || selectedWindow() != window
            || !window->window() || m_launcherGuestActive || m_cardGrabActive
            || !m_host->tabletOutputForCardStage()
            || window->screen() != m_host->tabletOutputForCardStage()
            || window->window()->isInteractiveMove()
            || window->window()->isInteractiveResize()) {
            clearCardTransition();
            return;
        }
        if (!window->window()->readyForPainting() || !window->window()->isShown()) {
            if (m_arrivalWait.elapsed() < 10000) m_arrivalTimer.start(50);
            else clearCardTransition(); // Keep the overview; never fall back to the old app.
            return;
        }
        if (!m_arrivalExpanding) {
            captureCardTransition();
            m_arrivalExpanding = true;
            m_arrivalTimer.start(motion(ArrivalExpandDuration));
        } else {
            clearCardTransition();
            (void)enterActive();
        }
        KWin::effects->addRepaintFull();
    });
    // Keyboard and focus notifications arrive in bursts and from inside the
    // compositor's own handling, so the room is made once, after.
    m_openReturnTimer.setSingleShot(true);
    QObject::connect(&m_openReturnTimer, &QTimer::timeout, &m_openReturnTimer, [this]() {
        // Back where the fingers started: the Active card, as it was.
        m_openProgress.reset();
        m_openReturn.invalidate();
        if (m_active && m_presentation == CardPresentation::Spread && enterActive()) {
            syncSelectedElevation();
            qInfo() << "Kadunce" << Revision << "went back to Active from three fingers let go short";
        }
        KWin::effects->addRepaintFull();
    });
    m_keyboardRoomTimer.setSingleShot(true);
    m_keyboardRoomTimer.setInterval(0);
    QObject::connect(&m_keyboardRoomTimer, &QTimer::timeout,
                     &m_keyboardRoomTimer, [this]() { updateKeyboardRoom(); });
    // The bottom panels return a moment after the keyboard leaves. Until then
    // the work area still includes their room, so the card keeps the
    // placement it had before the keyboard came.
    m_keyboardRoomRelease.setSingleShot(true);
    m_keyboardRoomRelease.setInterval(1000);
    QObject::connect(&m_keyboardRoomRelease, &QTimer::timeout,
                     &m_keyboardRoomRelease, [this]() { m_keyboardRoom.reset(); });
    // The keys move by a new frame every refresh, so a moment without one is
    // the keys at rest, when a client is asked for the room they take. The
    // moment is longer than a finger's pause in a drag, so a hand that slows
    // is not taken for keys that have stopped.
    m_keyboardRestTimer.setSingleShot(true);
    m_keyboardRestTimer.setInterval(100);
    QObject::connect(&m_keyboardRestTimer, &QTimer::timeout,
                     &m_keyboardRestTimer, [this]() { updateKeyboardRoom(true); });
    m_keyboardHeadingTimer.setSingleShot(true);
    QObject::connect(&m_keyboardHeadingTimer, &QTimer::timeout,
                     &m_keyboardHeadingTimer, [this]() {
        auto *client = keyboardRoomClient();
        if (!client || !m_keyboardHeadingTop) return;
        const double heading = keyboardRoomFor(client, *m_keyboardHeadingTop);
        if (heading < m_keyboardRoom->height) askKeyboardRoom(client, heading);
    });
    // A flicked card's app either closes, asks a question with a dialog of its
    // own, or keeps running; the card is looked at until one of those is true.
    m_thrownTimer.setInterval(100);
    QObject::connect(&m_thrownTimer, &QTimer::timeout, &m_thrownTimer,
                     [this]() { checkThrownCards(); });
    m_workAreaGrowTimer.setSingleShot(true);
    m_workAreaGrowTimer.setInterval(WorkAreaGrowWait);
    QObject::connect(&m_workAreaGrowTimer, &QTimer::timeout, &m_workAreaGrowTimer,
                     [this]() { followWorkArea(true); });
    m_activeSettleTimer.setSingleShot(true);
    m_activeSettleTimer.setInterval(0);
    QObject::connect(&m_activeSettleTimer, &QTimer::timeout, &m_activeSettleTimer, [this]() {
        const auto window = m_activeRestore.window;
        auto *tablet = m_host->tabletOutputForCardStage();
        if (!m_active || m_presentation != CardPresentation::Active
            || !m_activeRestore.valid || !window || window->isDeleted()
            || !window->window() || !tablet || m_activeSettleRemaining <= 0) return;
        auto *client = window->window();
        if (client->isInteractiveMove() || client->isInteractiveResize()
            || client->isFullScreen() || client->maximizeMode() != KWin::MaximizeRestore
            || client->quickTileMode() != KWin::QuickTileMode{}) return;
        const auto target = activePlacement(tablet);
        if (window->frameGeometry().toRect() == target) return;
        --m_activeSettleRemaining;
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        client->moveResize(KWin::RectF(target));
        KWin::effects->addRepaintFull();
        qInfo() << "Kadunce Active source settle" << logNameOf(window)
                << "target" << target << "remaining" << m_activeSettleRemaining;
    });
}

bool CardStageController::isActive() const
{
    return m_active;
}

CardPresentation CardStageController::presentation() const
{
    return m_presentation;
}

KWin::EffectWindow *CardStageController::activeCardIdentity() const
{
    // Only an individual card this stage still owns can be the Active card.
    return m_active && m_presentedActive && !m_presentedActive->isDeleted()
        && liveCardIndex(m_presentedActive) >= 0 ? m_presentedActive.data() : nullptr;
}

KWin::EffectWindow *CardStageController::cardSpreadOpenedOver() const
{
    return m_presentation == CardPresentation::Spread && !m_returnToDesktop
        ? activeCardIdentity() : nullptr;
}

bool CardStageController::canOwnCards(const KWin::LogicalOutput *output) const
{
    // One workspace exists and it is bound to one output. CARD-LIFECYCLE.md §11
    // gives any other output ordinary windows or per-output Bento, never
    // cards, so this is the product's shape rather than a temporary limit.
    return output && m_host->isTabletOutputForCardStage(output);
}

bool CardStageController::ownsDisplay(const KWin::LogicalOutput *output) const
{
    // CARD-LIFECYCLE.md §3: Kadunce owns a display from its first Card or Bento
    // action, whether what it holds is individual cards, stacks or a group.
    return m_active && canOwnCards(output) && !m_workspace.windows().isEmpty();
}

bool CardStageController::selectCardEntry(KWin::EffectWindow *window)
{
    // Selection is an entry index; membership is a card index. They coincide
    // only while every entry is standalone, so a stack makes selecting by
    // card index land on a different entry.
    const int cardId = liveCardIndex(window) + 1;
    if (cardId <= 0) return false;
    const int offset = spreadEntryOffset(m_workspace.model(), cardId);
    if (offset < 0) return false;
    m_workspace.page(offset);
    for (int step = 0; step < m_workspace.stackSizeForId(cardId)
         && m_workspace.selectedId() != cardId; ++step) {
        m_workspace.pageStack(1);
    }
    return m_workspace.selectedId() == cardId;
}

bool CardStageController::isEligiblePartner(const KWin::EffectWindow *window) const
{
    if (!m_active || !window || window->isDeleted() || !window->window()) return false;
    auto *output = m_host->tabletOutputForCardStage();
    // On the current display and current virtual desktop, owned as an
    // individual card, and awake. isManagedWindowForCardStage carries the
    // desktop and activity test and excludes a minimized window, so §7 keeps a
    // sleeping card from silently returning to Bento.
    if (!output || window->screen() != output
        || !m_host->isManagedWindowForCardStage(window)
        || liveCardIndex(window) < 0) return false;
    // §11, Every display: a card waiting here for another display never joins
    // this one's layout.
    if (KWin::LogicalOutput *own = m_host->releaseDisplayForCardStage(window); own && own != output)
        return false;
    // A card of the layout is never a partner, and neither is a live pane.
    return !inProjectedLayout(window) && !isBentoProjectionPane(window);
}

KWin::EffectWindow *CardStageController::partnerForSideSnap(
    const KWin::EffectWindow *carried, bool leftEdge, QString *why, bool wholeStacks) const
{
    const auto say = [why](const QString &text) { if (why) *why = text; };
    const auto app = [](const KWin::EffectWindow *window) {
        return window && window->window() ? window->window()->resourceClass()
                                          : QStringLiteral("a closed window");
    };
    if (!m_active || !carried || carried->isDeleted()) {
        say(QStringLiteral("none: no cards are held here"));
        return nullptr;
    }
    auto *active = activeCardIdentity();
    // §3: when the carried window is not the Active card, the Active card is
    // the partner. It qualifies whether it stands alone or is a stack's
    // selected member, because the user named it Active.
    if (active != carried) {
        if (!active) say(QStringLiteral("none: the carried window is not a card and no card is Active"));
        else if (isEligiblePartner(active))
            say(QStringLiteral("the Active card %1, because the carried window is not a card").arg(app(active)));
        else say(QStringLiteral("none: the Active card %1 cannot pair (asleep, or not on this display or desktop)").arg(app(active)));
        return isEligiblePartner(active) ? active : nullptr;
    }
    const int carriedId = liveCardIndex(carried) + 1;
    if (carriedId <= 0) {
        say(QStringLiteral("none: the carried window is not a card"));
        return nullptr;
    }
    // §3: the Active card was carried, so the partner is the card used just
    // before it, whichever edge was contacted.
    // §4: a stacked card, or one waiting behind a pane, reaches Bento only
    // with its Stack, never pulled out of it as a partner.
    if (m_usedActive == carried && m_usedBefore && m_usedBefore != carried
        && isEligiblePartner(m_usedBefore) && !paneHolding(m_usedBefore)
        && stackFrontToBack(m_usedBefore).isEmpty()) {
        say(QStringLiteral("%1, the card used before the Active card").arg(app(m_usedBefore)));
        return m_usedBefore;
    }
    const QString before = m_usedActive == carried && m_usedBefore
        ? QStringLiteral("the card used before it, %1, cannot pair (asleep, gone, or not a card); ")
              .arg(app(m_usedBefore))
        : QStringLiteral("no card was used before it; ");
    // Failing that, the nearest eligible card on the contacted side of it.
    // The walk begins at the entry holding that card and is cyclic, stopping
    // where it started.
    const QString side = leftEdge ? QStringLiteral("left") : QStringLiteral("right");
    QStringList passed;
    const int direction = leftEdge ? -1 : 1;
    for (int step = 1; step < m_workspace.count(); ++step) {
        const int face = m_workspace.faceAtStepsFrom(carriedId, direction * step);
        if (face <= 0 || face == carriedId) break;
        // §4: a search passes over every ordinary stack, so no walk breaks a
        // composed group. A stacked card reaches Bento only as the Active card.
        auto *candidate = m_workspace.windowForId(face).data();
        if (m_workspace.stackSizeForId(face) > 1 && !wholeStacks) {
            passed.append(QStringLiteral("a Stack fronted by %1").arg(app(candidate)));
            continue;
        }
        if (candidate && candidate != carried && isEligiblePartner(candidate) && !paneHolding(candidate)) {
            say(before + QStringLiteral("%1, the nearest card to the %2 of the Active card in Spread order%3")
                .arg(app(candidate), side,
                     passed.isEmpty() ? QString()
                                      : QStringLiteral(", passing ") + passed.join(QStringLiteral(", "))));
            return candidate;
        }
        passed.append(QStringLiteral("%1 (asleep or not a card)").arg(app(candidate)));
    }
    say(before + QStringLiteral("none: no awake card stands to the %1 of the Active card in Spread order%2")
        .arg(side, passed.isEmpty() ? QString()
                                    : QStringLiteral(", which holds only ") + passed.join(QStringLiteral(", "))));
    return nullptr;
}

const SpreadModel &CardStageController::model() const
{
    return m_workspace.model();
}

CardWorkspaceSnapshot CardStageController::workspaceSnapshot() const
{
    QStringList identities;
    identities.reserve(m_workspace.windows().size());
    for (const auto &window : m_workspace.windows())
        identities.append(window ? window->internalId().toString(QUuid::WithoutBraces) : QString());
    return makeCardWorkspaceSnapshot(m_workspace.model(), identities, m_active);
}

const QList<QPointer<KWin::EffectWindow>> &CardStageController::liveCards() const
{
    return m_workspace.windows();
}

KWin::EffectWindow *CardStageController::selectedWindow() const
{
    if (m_workspace.windows().isEmpty()) {
        return nullptr;
    }
    const int index = m_workspace.selectedId() - 1;
    return index >= 0 && index < m_workspace.windows().size()
        ? m_workspace.windows().at(index).data() : nullptr;
}

QList<KWin::EffectWindow *> CardStageController::stackFrontToBack(const KWin::EffectWindow *window) const
{
    QList<KWin::EffectWindow *> result;
    const int index = liveCardIndex(window);
    if (index < 0) return result;
    for (const auto &member : m_workspace.stackFrontToBack(m_workspace.windows().at(index)))
        if (member) result.append(member.data());
    return result;
}

bool CardStageController::stackBehind(KWin::EffectWindow *face, const QList<KWin::EffectWindow *> &behind)
{
    if (!m_active || m_cardGrabActive || m_launcherGuestActive || !face) return false;
    QList<QPointer<KWin::EffectWindow>> members;
    for (auto *window : behind) members.append(window);
    if (!m_workspace.stackBehind(face, members)) return false;
    syncSelectedStackingOrder();
    standStackWhole(face);
    KWin::effects->addRepaintFull();
    return true;
}

int CardStageController::liveCardIndex(const KWin::EffectWindow *window) const
{
    for (int index = 0; index < m_workspace.windows().size(); ++index) {
        if (m_workspace.windows().at(index) == window) {
            return index;
        }
    }
    return -1;
}

bool CardStageController::inProjectedLayout(const KWin::EffectWindow *window) const
{
    return window && std::any_of(
        m_bentoProjectionWindows.cbegin(), m_bentoProjectionWindows.cend(),
        [window](const auto &projected) { return projected == window; });
}

bool CardStageController::isBentoProjectionPane(
    const KWin::EffectWindow *window) const
{
    return window && std::any_of(
        m_bentoProjectionPaneWindows.cbegin(),
        m_bentoProjectionPaneWindows.cend(),
        [window](const auto &pane) { return pane == window; });
}

// Whether choosing the selected entry opens the display's layout: it is one
// of the layout's cards, or a card of a Stack holding a pane, which fills that
// pane with its front card (CARD-LIFECYCLE.md §11).
bool CardStageController::selectedIsBentoGroup() const
{
    if (!m_bentoProjectionSession || !selectedWindow()) return false;
    if (inProjectedLayout(selectedWindow())) return true;
    const int selected = m_workspace.selectedId();
    for (const auto &pane : m_bentoProjectionPaneWindows) {
        if (!pane) continue;
        const int id = m_workspace.indexOf(pane) + 1;
        if (id > 0 && id != selected && m_workspace.sameStack(id, selected)) return true;
    }
    return false;
}

QList<QPointer<KWin::EffectWindow>> CardStageController::bentoProjectionPanes() const
{
    return m_bentoProjectionPaneWindows;
}

QList<KWin::EffectWindow *> CardStageController::layoutPartners(const KWin::EffectWindow *window) const
{
    QList<KWin::EffectWindow *> partners;
    if (!m_bentoProjectionSession || !isBentoProjectionPane(window)) return partners;
    // The panes beside it, in the order the layout shows them.
    QList<QPair<BentoRect, KWin::EffectWindow *>> others;
    for (const auto &pane : m_bentoProjectionPaneWindows) {
        if (!pane || pane == window || pane->isDeleted() || pane->isMinimized()) continue;
        others.append({bentoProjectionRect(pane).value_or(BentoRect{}), pane.data()});
    }
    std::stable_sort(others.begin(), others.end(), [](const auto &a, const auto &b) {
        return std::abs(a.first.x - b.first.x) > 1e-3 ? a.first.x < b.first.x : a.first.y < b.first.y;
    });
    for (const auto &other : std::as_const(others)) partners.append(other.second);
    return partners;
}

std::optional<BentoRect> CardStageController::bentoProjectionRect(
    const KWin::EffectWindow *window) const
{
    if (!window || !m_bentoProjectionSession) return std::nullopt;
    const auto member = std::find_if(m_bentoProjectionSession->panes.cbegin(),
        m_bentoProjectionSession->panes.cend(), [window](const auto &member) {
            return member.window == window;
        });
    if (member == m_bentoProjectionSession->panes.cend()) return std::nullopt;
    const auto index = std::distance(m_bentoProjectionSession->panes.cbegin(), member);
    if (index < 0 || std::size_t(index) >= m_bentoProjectionSession->rects.size())
        return std::nullopt;
    return m_bentoProjectionSession->rects[std::size_t(index)];
}

QList<QPointer<KWin::EffectWindow>> CardStageController::preparationNeighbors() const
{
    QList<QPointer<KWin::EffectWindow>> result;
    if (!m_active || m_workspace.count() == 0
        || m_presentation != CardPresentation::Spread || m_launcherGuestActive) return result;
    if (m_cardGrabActive) {
        // Under a held card, the nearest entry beyond each side of the screen.
        auto *output = m_host->tabletOutputForCardStage();
        const auto frame = carryFrame(output);
        if (!frame) return result;
        QPointer<KWin::EffectWindow> nearest[2];
        double distance[2] = {0.0, 0.0};
        for (const auto &window : std::as_const(m_workspace.windows())) {
            if (!window || window->isDeleted() || visibleSlot(window) != 99) continue;
            const auto target = carryTarget(output, window);
            if (target.width() <= 0) continue;
            const double offset = target.center().x() - frame->centreX;
            const int side = offset < 0.0 ? 0 : 1;
            if (!nearest[side] || std::abs(offset) < distance[side]) {
                nearest[side] = window;
                distance[side] = std::abs(offset);
            }
        }
        for (const auto &window : nearest)
            if (window) result.append(window);
        return result;
    }
    for (int side : {-1, 1}) {
        const int id = m_workspace.idAtOffset(side * 2);
        const auto window = m_workspace.windows().value(id - 1);
        if (window && !window->isDeleted() && visibleSlot(window) == 99 && !result.contains(window))
            result.append(window);
    }
    return result;
}

void CardStageController::rebuildLiveCards()
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    m_workspace.clear();
    m_presentedActive = nullptr;
    m_bentoProjectionWindows.clear();
    m_bentoProjectionPaneWindows.clear();
    m_bentoProjectionSession.reset();
    if (!tablet) {
        return;
    }
    KWin::EffectWindow *active = KWin::effects->activeWindow();
    int activeIndex = -1;
    int awakeIndex = -1;
    QList<QPointer<KWin::EffectWindow>> admitted;
    const QList<KWin::EffectWindow *> windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        // CARD-LIFECYCLE.md §2 and §7: a window minimized before Kadunce held
        // the display is held too, as a sleeping card, so picking it wakes it
        // as a card. Only an awake window can be the one presented.
        // Where KWin intends the window to be, as a display change moves it
        // before its client answers; one another stage holds stays its own.
        if (!m_host->mayHoldWindowForCardStage(window) || !window->window()
            || window->window()->moveResizeOutput() != tablet
            || m_host->heldElsewhereForCardStage(window)) {
            continue;
        }
        const bool awake = m_host->isManagedWindowForCardStage(window);
        if (awake && window == active) {
            activeIndex = admitted.size();
        }
        if (awake) {
            awakeIndex = admitted.size();
        }
        admitted.append(window);
    }
    // Nothing awake is nothing to present: the display waits for a window to
    // open, as it does holding nothing, and no minimized window is woken.
    if (awakeIndex < 0) {
        admitted.clear();
    }
    for (const auto &window : std::as_const(admitted)) {
        m_host->connectManagedWindowForCardStage(window);
    }
    // Entry publishes the whole admitted set before any of it is described.
    // A restore record is defined only for a published card, so capture here
    // cannot precede the reset that publishes them.
    publishOwnershipThenRecord(
        [&] {
            m_workspace.reset(admitted,
                activeIndex >= 0 ? activeIndex : awakeIndex);
            return true;
        },
        [&] {
            for (const auto &window : admitted) retainManagedOwnership(window);
            if (!admitted.isEmpty()) m_originalCardStackingOrder = admitted;
        });
    settleCardsInActivePlace(admitted);
}

void CardStageController::retainManagedOwnership(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted() || !window->window()
        || liveCardIndex(window) < 0 || m_activeRestore.window == window) return;
    for (const auto &saved : std::as_const(m_parkedRestores))
        if (saved.window == window) return;
    auto *client = window->window();
    // Membership owns restoration, independently of the selected presentation.
    // Use KWin's accepted placement, which can precede a Wayland buffer/frame.
    m_parkedRestores.append(ActiveRestoreSnapshot{
        placeOf(client, client->moveResizeGeometry(), client->output()), window, true});
    ++m_restoreGeneration;
}

bool CardStageController::enterActive()
{
    // A card chosen from the desktop or from Spread opened there: the cards
    // come back before one is shown.
    m_returnToDesktop = false;
    bringCardsBack();
    ++m_restoreGeneration;
    m_host->cancelInputForCardStage();
    clearCardTransition();
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 2;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    KWin::EffectWindow *effectWindow = selectedWindow();
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    // A card just taken in from another display was asked to move here, and
    // KWin may not have put it here yet.
    if (!m_host->isManagedWindowForCardStage(effectWindow) || !tablet || !effectWindow->window()
        || (effectWindow->screen() != tablet && effectWindow->window()->moveResizeOutput() != tablet)) {
        qWarning() << "Kadunce" << Revision
                   << "cannot admit the selected card to Active";
        return false;
    }

    KWin::Window *client = effectWindow->window();
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_workspace.windows())) {
        if (window && !window->isDeleted()) {
            m_host->unredirectForCardStage(window);
        }
    }
    parkActiveSnapshot();
    retainManagedOwnership(effectWindow);
    m_liftedInPlace.remove(effectWindow);
    const auto retained = std::find_if(m_parkedRestores.begin(), m_parkedRestores.end(),
        [effectWindow](const auto &saved) { return saved.window == effectWindow; });
    if (retained != m_parkedRestores.end()) {
        m_activeRestore = *retained;
        m_parkedRestores.erase(retained);
    } else m_activeRestore = ActiveRestoreSnapshot{
        placeOf(client, effectWindow->frameGeometry(), effectWindow->screen()), effectWindow, true};
    if (client->isFullScreen()) {
        client->setFullScreen(false);
    }
    if (client->maximizeMode() != KWin::MaximizeRestore) {
        client->maximize(KWin::MaximizeRestore);
    }
    if (client->quickTileMode() != KWin::QuickTileMode{}) {
        client->setQuickTileMode(KWin::QuickTileMode{},
                                 effectWindow->frameGeometry().center());
    }

    restoreOriginalStackingOrder();
    const KWin::Rect target = activeTargetFor(tablet, effectWindow);
    client->moveResize(KWin::RectF(target));
    // Publish Active before asking KWin to activate the client. The resulting
    // windowActivated signal is synchronous on some Plasma versions and must
    // not be mistaken for a second task-manager request.
    m_presentation = CardPresentation::Active;
    m_presentedActive = effectWindow;
    // A layout left for one card turned the card keys off; Active has them.
    m_host->setPagingShortcutsForCardStage(true);
    if (m_usedActive != effectWindow) {
        m_usedBefore = m_usedActive;
        m_usedActive = effectWindow;
    }
    m_returnToGroup = false;
    // All entry paths must retire Spread's temporary compositor elevation.
    // Active is a native window; panel popups must retain their normal layers.
    syncSelectedElevation();
    m_activeSettleTimer.start();
    // A keyboard may already be up over the card that just arrived.
    refreshKeyboardRoom();
    KWin::workspace()->raiseWindow(client);
    const QPointer<KWin::EffectWindow> keep = std::exchange(m_focusAfterActive, nullptr);
    if (keep && !keep->isDeleted() && keep->window() && !keep->isMinimized())
        KWin::workspace()->activateWindow(keep->window(), true);
    else
        KWin::workspace()->activateWindow(client, true);
    qInfo() << "Kadunce" << Revision
            << "entered interactive Active with" << logNameOf(effectWindow);
    return true;
}

void CardStageController::standStackWhole(const KWin::EffectWindow *member)
{
    const QList<KWin::EffectWindow *> stack = stackFrontToBack(member);
    if (stack.size() < 2) return;
    // Every card of it, on whichever display holds it, stands at that
    // display's whole Active place: one size, the Stack's.
    QHash<KWin::LogicalOutput *, QList<QPointer<KWin::EffectWindow>>> byDisplay;
    int narrowed = 0;
    for (auto *card : stack) {
        if (m_activeWidths.remove(card)) ++narrowed;
        if (card && card->screen() && canOwnCards(card->screen())) byDisplay[card->screen()].append(card);
    }
    qInfo() << "Kadunce" << Revision << "stands a Stack of" << stack.size() << "at the whole width;"
            << narrowed << "narrowed by the wheel";
    if (m_presentation == CardPresentation::Active && m_activeRestore.window
        && stack.contains(m_activeRestore.window.data())) (void)refitActive(m_activeRestore.window);
    for (auto it = byDisplay.cbegin(); it != byDisplay.cend(); ++it) settleCardsInActivePlace(it.value(), it.key());
}

void CardStageController::settleCardsInActivePlace(const QList<QPointer<KWin::EffectWindow>> &cards,
    KWin::LogicalOutput *output)
{
    // §3: a card behind the one presented stands where it would as Active. A
    // window taken maximized would otherwise still reach the bottom of the
    // work area, where a panel that watches for windows reaching it stays
    // opaque until each card has been brought forward once. Its own place is
    // already in its restore record, so release gives it back.
    KWin::LogicalOutput *tablet = output ? output : m_host->tabletOutputForCardStage();
    if (!tablet) return;
    const KWin::RectF target(activeTarget(tablet));
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    int settled = 0;
    for (const auto &window : cards) {
        if (!window || window->isDeleted() || !window->window()
            || !m_host->isManagedWindowForCardStage(window)
            || window->screen() != tablet) continue;
        KWin::Window *client = window->window();
        if (client->isFullScreen()) client->setFullScreen(false);
        if (client->maximizeMode() != KWin::MaximizeRestore) client->maximize(KWin::MaximizeRestore);
        if (client->quickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        if (client->moveResizeGeometry() == target) continue;
        // Seen in Spread, the card keeps its picture until the app has drawn
        // itself at the new size.
        if (m_active && m_presentation == CardPresentation::Spread) {
            const QPointer<KWin::EffectWindow> held(window);
            const auto resize = [this, held, target] {
                if (!held || held->isDeleted() || !held->window()) return;
                QScopedValueRollback<bool> applying(m_applyingWindowState, true);
                held->window()->moveResize(target);
            };
            if (m_host->holdPictureBeforeResizeForCardStage(window, resize)) {
                ++settled;
                continue;
            }
        }
        client->moveResize(target);
        ++settled;
    }
    if (settled > 0)
        qInfo() << "Kadunce" << Revision << "stood" << settled << "cards in the Active card's place";
}

void CardStageController::keepTooBigInPlace(KWin::EffectWindow *window, const KWin::RectF &target)
{
    // A window that cannot be made as small as the Active place keeps its top
    // and left where the place begins, so its title bar stays clear of the
    // Context Panel, and is as small as it can be, so only its far edge runs
    // over, by no more than it must. KWin pushes a window too big for the
    // work area up and across; it is put back a few times at most each time
    // it is shown, so neither is fought in a loop.
    KWin::Window *client = window && window->window() ? window->window() : nullptr;
    if (!client) return;
    const KWin::RectF placed = client->moveResizeGeometry();
    const QSizeF least = client->minSize();
    const KWin::RectF kept(target.left(), target.top(), std::max(target.width(), least.width()),
                           std::max(target.height(), least.height()));
    if (std::abs(placed.top() - kept.top()) <= 1 && std::abs(placed.left() - kept.left()) <= 1
        && std::abs(placed.width() - kept.width()) <= 1 && std::abs(placed.height() - kept.height()) <= 1) return;
    int &lifts = m_liftedInPlace[window];
    if (lifts >= 3) return;
    ++lifts;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(kept);
    qInfo() << "Kadunce" << Revision << logNameOf(window) << "is bigger than the Active place; its top and left"
            << "are kept inside it at" << target.topLeft();
}

void CardStageController::refitAfterMinimumChanged(KWin::EffectWindow *window)
{
    // A card its application could not make as small as the Active place,
    // as one carried from a display at another scale cannot be until it has
    // drawn itself there, is asked for the place again once its least size
    // changes, so docking or undocking leaves no card too tall.
    if (!m_active || m_presentation != CardPresentation::Active || m_applyingWindowState
        || m_cardGrabActive || !window || window->isDeleted() || m_activeRestore.window != window
        || !window->window()) return;
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    KWin::Window *client = window->window();
    if (!tablet || window->screen() != tablet || client->isInteractiveMove()
        || client->isInteractiveResize()) return;
    const KWin::RectF place(activePlacement(tablet));
    m_liftedInPlace.remove(window);
    const QSizeF least = client->minSize();
    if (least.width() > place.width() || least.height() > place.height()) {
        keepTooBigInPlace(window, place);
        return;
    }
    if (client->moveResizeGeometry().toRect() == place.toRect()) return;
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    client->moveResize(place);
    qInfo() << "Kadunce" << Revision << logNameOf(window) << "can be as small as the Active place again;"
            << "it takes" << place;
}

void CardStageController::holdCardInActivePlace(KWin::EffectWindow *window)
{
    // §3: an application that restores its saved state maximizes its window a
    // moment after showing it, by which time another card may stand in front.
    // Its wish is kept in its restore record, so release gives it back
    // maximized, and the card goes back to the Active place. A card is put
    // back at most every half second, and one whose minimum size cannot fit
    // the place is left alone, so no application is fought in a loop.
    if (m_applyingWindowState || m_cardGrabActive || !window || window->isDeleted()
        || !window->window() || liveCardIndex(window) < 0
        || m_bentoProjectionWindows.contains(window)
        || !m_host->isManagedWindowForCardStage(window)) return;
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || window->screen() != tablet) return;
    KWin::Window *client = window->window();
    if (client->isInteractiveMove() || client->isInteractiveResize()) return;
    const KWin::RectF target(activeTarget(tablet));
    const bool asked = client->isRequestedFullScreen()
        || client->requestedMaximizeMode() != KWin::MaximizeRestore
        || client->requestedQuickTileMode() != KWin::QuickTileMode{};
    const KWin::RectF placed = client->moveResizeGeometry();
    const bool reaches = placed.bottom() > target.bottom() + 1
        || placed.right() > target.right() + 1;
    const QSizeF minimum = client->minSize();
    if (minimum.width() > target.width() || minimum.height() > target.height()) {
        if (!asked) keepTooBigInPlace(window, target);
        return;
    }
    if (!asked && !reaches) return;
    auto &since = m_heldInPlace[window];
    if (since.isValid() && since.elapsed() < 500) return;
    since.start();
    if (asked) {
        for (auto &saved : m_parkedRestores) {
            if (saved.window != window) continue;
            saved.maximizeMode = client->requestedMaximizeMode();
            saved.fullScreen = client->isRequestedFullScreen();
            saved.quickTileMode = client->requestedQuickTileMode();
            break;
        }
    }
    const QPointer<KWin::EffectWindow> held(window);
    QTimer::singleShot(0, &m_activeSettleTimer, [this, held] {
        if (!held || held->isDeleted() || !held->window() || !m_active
            || liveCardIndex(held) < 0 || m_activeRestore.window == held) return;
        settleCardsInActivePlace({held});
    });
}

void CardStageController::restoreActiveSnapshot()
{
    ++m_restoreGeneration;
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    m_keyboardRoomTimer.stop();
    m_keyboardRoomRelease.stop();
    m_keyboardRestTimer.stop();
    m_keyboardHeadingTimer.stop();
    m_keyboardHeadingPending = false;
    m_keyboardHeadingTop.reset();
    m_keyboardRoom.reset();
    QScopedValueRollback<bool> applying(m_applyingWindowState, true);
    m_restoredMinimizations.clear();
    parkActiveSnapshot();
    const auto snapshots = std::exchange(m_parkedRestores, {});
    for (const auto &snapshot : snapshots) {
        if (!snapshot.valid || !snapshot.window || snapshot.window->isDeleted()
            || !snapshot.window->window()) continue;
        auto *client = snapshot.window->window();
        // Kadunce never minimizes a card, so a card minimized now was put
        // away by the person or the application while it was held, and stays
        // put away: its place is given back without bringing it up.
        const bool minimizedNow = client->isMinimized();
        // Each window given back says from where and to what, so a window
        // that did not go back can be read back.
        qInfo() << "Kadunce" << Revision << "gives" << client->resourceClass()
                << "back its place" << snapshot.geometry << "from" << client->moveResizeGeometry()
                << "maximized" << int(snapshot.maximizeMode) << "fullscreen" << snapshot.fullScreen
                << "minimized" << snapshot.minimized << "minimized now" << minimizedNow;
        const ActiveRestoreSnapshot own = onOwnDisplay(snapshot);
        const KWin::RectF place = placeClearOfPanels(own);
        if (place != own.geometry)
            qInfo() << "Kadunce" << Revision << client->resourceClass() << "is given back at" << place
                    << "instead: its place lies under a panel's room";
        restoreWindowState(client, own, place, true, !minimizedNow, false);
        if (!snapshot.fullScreen && snapshot.maximizeMode == KWin::MaximizeRestore
            && snapshot.quickTileMode == KWin::QuickTileMode{}
            && client->moveResizeGeometry() != place)
            qInfo() << "Kadunce" << Revision << client->resourceClass()
                    << "was given back but stands at" << client->moveResizeGeometry();
        if (snapshot.minimized && !minimizedNow && snapshot.window && !snapshot.window->isDeleted())
            m_restoredMinimizations.push_back(std::make_unique<RestoredMinimization>(client,
                RestoredMinimization::Target{client->moveResizeGeometry(), snapshot.maximizeMode,
                    snapshot.quickTileMode, snapshot.fullScreen}));
    }
}

CardStageController::ActiveRestoreSnapshot CardStageController::onOwnDisplay(
    const ActiveRestoreSnapshot &snapshot) const
{
    // §11, Every display: a card is given back on the display it belongs to,
    // never at a place its record kept from a display it has since left. In
    // every mode, a place whose display is gone, or that no display holds, is
    // carried onto a display there is (ReleasePlace.h).
    const KWin::LogicalOutput *own = m_host->releaseDisplayForCardStage(snapshot.window.data());
    const KWin::LogicalOutput *standing = snapshot.window ? snapshot.window->screen() : nullptr;
    if (!standing) standing = m_host->tabletOutputForCardStage();
    const auto carry = releaseCarry(releaseDisplays(), snapshot.releaseRecord(),
        own ? own->name() : QString(), standing ? standing->name() : QString());
    if (!carry || carry->display.work.isEmpty()) return snapshot;
    ActiveRestoreSnapshot moved = snapshot;
    static_cast<WindowPlace &>(moved) = snapshot.carriedOnto(carry->from, carry->display.work);
    qInfo() << "Kadunce" << Revision << "gives" << (snapshot.window ? logNameOf(snapshot.window) : QString())
            << "back on" << carry->display.name << "at" << moved.geometry << "not" << snapshot.geometry
            << "taken on" << (snapshot.outputName.isEmpty() ? QStringLiteral("an unknown display") : snapshot.outputName);
    return moved;
}

KWin::RectF CardStageController::placeClearOfPanels(const ActiveRestoreSnapshot &snapshot) const
{
    // A place taken before a panel reserved its room, as a window restored at
    // sign-in can hold, would be given back under the panel. Within one
    // display it is fitted into the room left now, as KWin itself keeps
    // ordinary windows clear when a panel takes room.
    if (snapshot.fullScreen || snapshot.maximizeMode != KWin::MaximizeRestore
        || snapshot.quickTileMode != KWin::QuickTileMode{}) return snapshot.geometry;
    const QRectF place(snapshot.geometry);
    // A place that runs off its display, as a pane's size kept by a card a
    // smaller display showed can, is fitted into the display holding most of it.
    KWin::LogicalOutput *holding = nullptr;
    double most = 0.0;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const QRectF screen(output->geometry());
        if (screen.contains(place)) { holding = output; break; }
        const QRectF shared = screen.intersected(place);
        const double held = shared.width() * shared.height();
        if (held > most) { most = held; holding = output; }
    }
    if (!holding) return snapshot.geometry;
    const QRectF area(KWin::effects->clientArea(KWin::MaximizeArea, holding));
    if (area.isEmpty() || area.contains(place)) return snapshot.geometry;
    QRectF fitted = place;
    fitted.setWidth(std::min(fitted.width(), area.width()));
    fitted.setHeight(std::min(fitted.height(), area.height()));
    fitted.moveLeft(std::clamp(fitted.left(), area.left(), area.right() - fitted.width()));
    fitted.moveTop(std::clamp(fitted.top(), area.top(), area.bottom() - fitted.height()));
    return KWin::RectF(fitted);
}

void CardStageController::parkActiveSnapshot()
{
    m_activeSettleTimer.stop();
    // A card leaving Active stays a card, so it leaves at its own size rather
    // than the one it gave up for the keyboard.
    if (m_keyboardRoom && m_keyboardRoom->window == m_activeRestore.window) {
        putBackKeyboardRoom();
    }
    if (m_activeRestore.valid && m_activeRestore.window) {
        const auto window = m_activeRestore.window;
        m_parkedRestores.removeIf([&](const auto &s) { return !s.window || s.window == window; });
        m_parkedRestores.append(m_activeRestore);
    }
    m_activeRestore = {};
}

void CardStageController::retireActiveIdentity(const KWin::EffectWindow *window)
{
    if (m_presentedActive == window) m_presentedActive = nullptr;
}

void CardStageController::forgetManagedRestore(KWin::EffectWindow *window)
{
    retireActiveIdentity(window);
    if (m_activeRestore.window == window) m_activeRestore = {};
    m_parkedRestores.removeIf([window](const auto &s) { return !s.window || s.window == window; });
    m_bentoProjectionWindows.removeIf(
        [window](const auto &projected) { return !projected || projected == window; });
    m_bentoProjectionPaneWindows.removeIf(
        [window](const auto &pane) { return !pane || pane == window; });
    if (m_bentoProjectionSession) {
        auto &projection = *m_bentoProjectionSession;
        for (int index = projection.panes.size() - 1; index >= 0; --index) {
            if (!projection.panes[index].window
                || projection.panes[index].window == window) {
                projection.panes.removeAt(index);
                if (index < int(projection.rects.size()))
                    projection.rects.erase(projection.rects.begin() + index);
            }
        }
        projection.sleeping.removeIf(
            [window](const auto &member) { return !member.window || member.window == window; });
        projection.stackingOrder.removeIf(
            [window](const auto &member) { return !member || member == window; });
        if (projection.lead == window) {
            projection.lead = projection.panes.isEmpty()
                ? nullptr : projection.panes.first().window;
        }
        if (projection.sideWindow == window) {
            projection.sideWindow.clear();
            projection.side.reset();
        }
        if (projection.snappedWindow == window) {
            projection.snappedWindow.clear();
            projection.snappedSide.reset();
        }
        if (projection.panes.isEmpty()) m_bentoProjectionSession.reset();
    }
    ++m_restoreGeneration;
}

std::optional<NativeMoveSnapshot> CardStageController::managedRestore(KWin::EffectWindow *window) const
{
    if (!m_active || !window || window->isDeleted() || !window->window()) return std::nullopt;
    const ActiveRestoreSnapshot *saved = m_activeRestore.window == window ? &m_activeRestore : nullptr;
    if (!saved) for (const auto &s : m_parkedRestores) {
        if (s.window == window) { saved = &s; break; }
    }
    if (!saved || !saved->valid) return std::nullopt;
    // The place belongs to the display it was taken on, while that display is
    // attached, not to the one the card stands on now.
    KWin::LogicalOutput *output = saved->outputName.isEmpty()
        ? nullptr : KWin::effects->findScreen(saved->outputName);
    if (!output) output = window->screen();
    return NativeMoveSnapshot{*saved, window->window(), output};
}

bool CardStageController::admitTransferredWindowToTablet(
    KWin::EffectWindow *window, const std::function<bool()> &commitSource,
    const QRectF &carriedOrigin, const NativeMoveSnapshot *restore)
{
    QPointer<KWin::LogicalOutput> tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !window || window->isDeleted() || !window->window()
        || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)) {
        return false;
    }
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    const ActiveRestoreSnapshot incoming{restore ? restore->place()
        : placeOf(client.data(), window->frameGeometry(), window->screen()), window, true};
    const KWin::RectF target(activeTarget(tablet));
    const auto ticket = m_transferGuard.issue();
    if (!target.isValid() || !KWin::effects->screens().contains(tablet.data())
        || window->isUserMove() || window->isUserResize()) return false;
    const bool newMember = liveCardIndex(window) < 0;
    const bool animateArrival = m_active && m_presentation == CardPresentation::Spread
        && !m_launcherGuestActive;
    const int previousSelection = m_workspace.selectedIndex();
    if (newMember) {
        // A second arrival must not replace a held card's rollback transaction.
        if (m_cardGrabActive) return false;
        const auto admission = m_workspace.prepareAdmission(arrival,
            animateArrival || m_launcherGuestActive);
        if (!admission) return false;
        // Capture the old arrangement before publishing the new member.
        if (animateArrival) captureCardTransition();
        if (!m_workspace.commitAdmission(*admission, commitSource)) return false;
        m_originalCardStackingOrder.append(arrival);
        // A fresh receiver must own the card before applying Active geometry.
        // Otherwise the first upward swipe can start an ordinary native resize.
        if (!m_active) {
            m_active = true;
            m_host->setPagingShortcutsForCardStage(true);
        }
    } else if (!commitSource()) {
        return false;
    }
    const auto valid = [&] {
        return m_transferGuard.accepts(ticket)
            && arrival && !arrival->isDeleted() && client && arrival->window() == client
            && tablet && m_host->tabletOutputForCardStage() == tablet
            && KWin::effects->screens().contains(tablet.data())
            && !arrival->isUserMove() && !arrival->isUserResize();
    };
    if (!valid()) return true;
    if (newMember) {
        // Both memberships are committed before any native/presentation cleanup.
        m_host->connectManagedWindowForCardStage(arrival);
        m_host->cancelInputForCardStage();
        if (!valid()) return true;
        m_presentation = CardPresentation::Spread;
        parkActiveSnapshot();
        if (!valid()) return true;
    }
    // A source record belongs to cancellation until commit. Committed, the
    // record the giving stage kept goes on as this card's own, whichever
    // display it was taken on: a maximized window carried here comes back
    // maximized, and on a display that is there (onOwnDisplay). A window
    // arriving from another display with no record establishes a new receiver
    // origin from KWin's accepted tablet placement before it takes the Active
    // card's size.
    const bool sameOutput = restore ? restore->output == tablet : window->screen() == tablet;
    if ((sameOutput || restore) && m_active && !managedRestore(arrival)) m_parkedRestores.append(incoming);
    if (!adoptPublishedCard(client.data(), tablet.data(),
            [&] { return liveCardIndex(arrival) >= 0; },
            [&] { retainManagedOwnership(arrival); }, target, valid)) {
        return true;
    }
    if (!m_active) {
        KWin::workspace()->raiseWindow(client);
        if (!valid()) return true;
        KWin::workspace()->activateWindow(client, true);
        return true;
    }
    // Reuse the same centered insertion and settling sequence as app launches.
    // Already-admitted windows use the existing selection/arrival path.
    if (newMember) {
        finishNewArrival(arrival, animateArrival, previousSelection);
    } else {
        stageWindowArrival(arrival);
    }
    // The receiver owns presentation. Seed its existing center/expand sequence
    // from the released face, not an invented off-screen Spread slot. Do
    // this after admission cleanup, which can cancel the source carry.
    if (valid() && animateArrival && m_presentation == CardPresentation::Spread
        && m_arrivalWindow == arrival && carriedOrigin.isValid()) {
        const auto work = workArea(tablet);
        if (work.width() > 0 && work.height() > 0) {
            m_previewOrigins.removeIf([&](const auto &origin) { return origin.window == arrival; });
            m_previewOrigins.append({arrival, QRectF(
                (carriedOrigin.x() - work.x()) / work.width(),
                (carriedOrigin.y() - work.y()) / work.height(),
                carriedOrigin.width() / work.width(),
                carriedOrigin.height() / work.height())});
            KWin::effects->addRepaintFull();
        }
    }
    return true;
}

bool CardStageController::growSelectedToActive()
{
    KWin::EffectWindow *window = selectedWindow();
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    // A second tap while it grows takes it straight in.
    if (m_arrivalExpanding && window && window == m_arrivalWindow) return false;
    // Instant motion, or a card with nothing to draw yet, goes straight in.
    if (motion(ArrivalExpandDuration) <= 1 || !tablet || m_launcherGuestActive
        || m_cardGrabActive || !window || window->isDeleted() || window->isMinimized()
        || window->screen() != tablet || !window->window()
        || !window->window()->readyForPainting() || !window->window()->isShown()
        || !m_host->isManagedWindowForCardStage(window)) return false;
    captureCardTransition();
    if (!m_previewTransition.isValid()) return false;
    // KWin picks the window a press goes to before Kadunce sees the press, by
    // its own stacking, and the cards behind stand in the Active place too.
    // The growing card is on top for KWin as it is on screen, so a press that
    // ends the growth reaches it and not the card that was Active before.
    KWin::workspace()->raiseWindow(window->window());
    m_arrivalWindow = window;
    m_arrivalExpanding = true;
    m_growingChosen = true;
    m_arrivalWait.start();
    m_arrivalTimer.start(motion(ArrivalExpandDuration));
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "growing the chosen card to Active" << logNameOf(window);
    return true;
}

bool CardStageController::finishGrowToActive()
{
    if (!m_growingChosen || !m_arrivalExpanding || !m_active
        || m_presentation != CardPresentation::Spread || selectedWindow() != m_arrivalWindow) return false;
    return enterActive();
}

void CardStageController::startArrivalTimer(KWin::EffectWindow *window)
{
    m_arrivalWindow = window;
    m_arrivalExpanding = false;
    m_growingChosen = false;
    m_arrivalWait.start();
    m_arrivalTimer.start(motion(PreviewTransitionDuration));
    qInfo() << "Kadunce new app settling at center" << logNameOf(window);
}

void CardStageController::stageWindowArrival(KWin::EffectWindow *window)
{
    if (!m_active || !window || window->isDeleted() || liveCardIndex(window) < 0) return;
    m_host->cancelInputForCardStage();
    if (m_presentation != CardPresentation::Spread) {
        handleWindowActivated(window);
        return;
    }
    const bool replacesGuest = m_launcherGuestActive;
    captureCardTransition(replacesGuest);
    if (replacesGuest) {
        // The new app replaces the guest aperture, never a shoulder. Keep
        // every existing app at its current painted origin during the reveal.
        auto *output = m_host->tabletOutputForCardStage();
        if (!output) return;
        const auto work = workArea(output);
        if (work.width() <= 0 || work.height() <= 0) return;
        const auto center = launcherGuestTarget(output);
        m_previewOrigins.removeIf([window](const auto &origin) { return origin.window == window; });
        m_previewOrigins.append({window, QRectF(
            (center.x() - work.x()) / work.width(), (center.y() - work.y()) / work.height(),
            center.width() / work.width(), center.height() / work.height())});
        m_launcherGuestArrival = true;
        m_launcherGuestPendingPage = 0;
        m_launcherGuestTransitionTimer.invalidate();
        m_launcherGuestOffset = 0.0;
    }
    const int id = liveCardIndex(window) + 1;
    for (int i = 0; i < m_workspace.count() && !m_workspace.sameStack(id, m_workspace.selectedId()); ++i)
        m_workspace.page(1);
    for (int i = 0; i < m_workspace.stackSizeForId(id) && m_workspace.selectedId() != id; ++i)
        m_workspace.pageStack(1);
    if (replacesGuest && m_workspace.count() == 2)
        m_workspace.setPairNeighborSide(m_launcherGuestPrimarySide);
    startArrivalTimer(window);
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::handleWindowAdded(KWin::EffectWindow *window)
{
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!m_active || !window || !tablet || !window->isNormalWindow()
        || !m_host->isManagedWindowForCardStage(window)
        || window->screen() != tablet || liveCardIndex(window) >= 0) {
        return false;
    }
    // §2: the display is presenting its panes, and this path publishes a
    // Spread presentation and an Active card. Refusing is what makes the
    // caller retire the layout first rather than remember to; a card drawn
    // over live panes is the state §2 does not name.
    if (m_presentation == CardPresentation::Bento) return false;
    // A new card changes the row under a moving hand: land it on a card first.
    dropRowMotion();

    const bool animateArrival = m_presentation == CardPresentation::Spread
        && !m_launcherGuestActive && !m_cardGrabActive;
    m_host->cancelInputForCardStage();
    if (animateArrival) captureCardTransition();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::Spread;
    int previousSelection = m_workspace.selectedIndex();
    m_workspace.append(window, animateArrival || m_launcherGuestActive);
    if (!m_openingClock.isValid()) m_openingClock.start();
    // Those past their opening are forgotten as each new one arrives.
    m_openedAt.removeIf([this](const auto &entry) {
        return m_openingClock.elapsed() - entry.value() > OpeningStateGrace;
    });
    m_openedAt.insert(window, m_openingClock.elapsed());
    retainManagedOwnership(window);
    m_originalCardStackingOrder.append(window);
    m_host->connectManagedWindowForCardStage(window);
    finishNewArrival(window, animateArrival, previousSelection);
    return true;
}

void CardStageController::finishNewArrival(KWin::EffectWindow *window,
    bool animateArrival, int previousSelection)
{
    if (m_launcherGuestActive) {
        if (m_workspace.selectedIndex() <= previousSelection) ++previousSelection;
        m_workspace.selectIndex(previousSelection);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return; // Admission must not bypass the guest's matching handoff.
    }
    if (animateArrival) {
        startArrivalTimer(window);
        syncSelectedElevation();
        KWin::effects->addRepaintFull();
        return;
    }
    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "admitted new card"
            << liveCardIndex(window) + 1 << logNameOf(window) << "as Active";
}

void CardStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    // A closed pane's Stack keeps its cards together in the row; its identity
    // may be reused by the next window, so it names no Stack from here on.
    m_paneStacks.remove(window);
    m_openedAt.remove(window);
    m_activeWidths.remove(window);
    m_heldInPlace.remove(window);
    m_liftedInPlace.remove(window);
    for (const auto &ask : m_closeAsks) {
        if (ask.window != window) continue;
        qInfo() << "Kadunce" << Revision << "flicked card's app closed" << ask.asked.elapsed()
                << "ms after it was asked;" << (ask.drew.isValid()
                    ? qPrintable(QStringLiteral("it drew %1 ms after").arg(ask.asked.elapsed() - ask.drew.elapsed()))
                    : "it drew nothing in between");
    }
    forgetCloseAsk(window);
    m_originalCardStackingOrder.removeAll(window);
    m_aside.removeAll(window);
    QObject::disconnect(m_asideListed.take(window));
    m_returnedToDesktop.removeAll(window);
    const int closedIndex = liveCardIndex(window);
    if (!m_active || closedIndex < 0) {
        return;
    }

    m_host->cancelInputForCardStage();
    dropRowMotion();
    // KWin hands the focus on as soon as this returns, in the same call, and
    // that choice is not the person's: Spread stays where it is.
    if (m_presentation == CardPresentation::Spread && !m_closeHandoff) {
        m_closeHandoff = true;
        QTimer::singleShot(0, &m_thrownTimer, [this]() { m_closeHandoff = false; });
    }
    const bool flicked = thrownAway(window)
        || (window == m_lift.window && m_lift.phase == Lift::Phase::Throw);
    if (window == m_lift.window) {
        // Closed before it left the screen: it stays out of sight all the same.
        if (flicked && !thrownAway(window)) {
            Thrown thrown{window, {}};
            thrown.since.start();
            m_thrown.append(thrown);
            m_thrownTimer.start();
        }
        endLift();
    }

    if (m_workspace.count() == 3 && m_workspace.stackSizeForId(closedIndex + 1) == 1)
        captureCardTransition();
    else if (m_presentation == CardPresentation::Spread && (flicked || !m_launcherGuestActive))
        captureCardTransition(false, true); // The row closes the gap it left.
    finishCardGrab(false);
    const bool closedActive = m_activeRestore.window == window;
    if (closedActive) {
        m_activeRestore = ActiveRestoreSnapshot{};
    }
    forgetManagedRestore(window);
    const bool removed = m_workspace.removeAt(closedIndex);
    if (m_workspace.windows().isEmpty()) {
        release();
        return;
    }
    if (!removed) {
        qWarning() << "Kadunce" << Revision
                   << "could not remove closed card in place; rebuilding";
        if (m_presentation == CardPresentation::Active) {
            parkActiveSnapshot();
        }
        m_presentation = CardPresentation::Spread;
        rebuildLiveCards();
    } else if (closedActive) {
        m_presentation = CardPresentation::Spread;
        formRowFromActive();
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::releaseCard(KWin::EffectWindow *window, bool restore)
{
    const int index = liveCardIndex(window);
    if (!m_active || index < 0 || !window || window->isDeleted() || !window->window())
        return false;
    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    clearCardTransition();
    const bool wasActive = m_activeRestore.window == window;
    // The Active card leaves at its own size, its keyboard room given back,
    // and its record joins the others to be read below.
    if (wasActive) parkActiveSnapshot();
    std::optional<ActiveRestoreSnapshot> record;
    for (const auto &saved : std::as_const(m_parkedRestores)) {
        if (saved.window == window && saved.valid) {
            record = saved;
            break;
        }
    }
    forgetManagedRestore(window);
    const bool removed = m_workspace.removeAt(index);
    m_originalCardStackingOrder.removeAll(window);
    KWin::effects->setElevatedWindow(window, false);
    m_host->unredirectForCardStage(window);
    if (record && restore) {
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        // Given back as a full release gives it: on a display that is still
        // there, and clear of the panels' room.
        const ActiveRestoreSnapshot own = onOwnDisplay(*record);
        restoreWindowState(window->window(), own, placeClearOfPanels(own), true);
    }
    qInfo() << "Kadunce" << Revision << "let" << logNameOf(window)
            << "go for another desktop;" << m_workspace.windows().size() << "individual cards";
    if (m_workspace.windows().isEmpty()) {
        release();
        return true;
    }
    if (!removed) {
        if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
        m_presentation = CardPresentation::Spread;
        rebuildLiveCards();
    } else if (wasActive) {
        m_presentation = CardPresentation::Spread;
    }
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::handleWindowActivated(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active || !window || m_cardGrabActive) {
        return;
    }
    if (window == m_arrivalWindow) return; // Let its center/expand sequence finish.
    if (m_closeHandoff && m_presentation == CardPresentation::Spread) {
        qInfo() << "Kadunce" << Revision << "kept Spread as a closed card handed the focus to"
                << logNameOf(window);
        return;
    }
    // §2: this stage keeps its cards hidden while the display presents its
    // Bento layout. An activation reaching here was not routed into the
    // layout, and drawing a card over live panes is the state §2 does not
    // name, so the presentation stands.
    if (m_presentation == CardPresentation::Bento) return;
    if (m_arrivalWindow) clearCardTransition(); // An explicit different activation wins.
    const int targetIndex = liveCardIndex(window);
    if (targetIndex < 0) {
        // §2: a window returned to the desktop is shown on it, so asking for
        // it shows the desktop; the cards would otherwise stand over it.
        if (m_presentation != CardPresentation::Desktop && m_returnedToDesktop.contains(window)) {
            m_host->cancelInputForCardStage();
            finishCardGrab(false);
            if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
            showDesktop();
        }
        return;
    }
    const int targetId = targetIndex + 1;
    if (m_presentation == CardPresentation::Active
        && m_workspace.selectedId() == targetId) {
        return;
    }

    m_host->cancelInputForCardStage();
    finishCardGrab(false);
    if (m_presentation == CardPresentation::Active) {
        parkActiveSnapshot();
    }
    m_presentation = CardPresentation::Spread;

    // Select through the stack model so a task-manager click can address both
    // standalone cards and a specific stack member while selection remains
    // centralized in the spread model.
    for (int step = 0;
         step < m_workspace.count()
         && !m_workspace.sameStack(targetId, m_workspace.selectedId());
         ++step) {
        m_workspace.page(1);
    }
    if (inProjectedLayout(window)) {
        syncSelectedElevation();
        (void)resumeSelectedBentoProjection();
        return;
    }
    const int stackSize = m_workspace.stackSizeForId(targetId);
    for (int step = 0;
         step < stackSize && m_workspace.selectedId() != targetId;
         ++step) {
        m_workspace.pageStack(1);
    }

    syncSelectedElevation();
    if (!enterActive()) {
        m_presentation = CardPresentation::Spread;
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision
            << "promoted externally activated card" << logNameOf(window);
}

void CardStageController::handleActiveGeometryChanged(
    KWin::EffectWindow *window)
{
    noteKeyboardRoomDrawn(window);
    if (m_active && window && m_activeRestore.window != window) {
        holdCardInActivePlace(window);
        return;
    }
    if (!m_active || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || m_activeRestore.window != window
        || !window->window()) {
        return;
    }
    if (m_applyingWindowState) return;
    // Late configure replies still need Active-size normalization, but never
    // resize recursively inside the notification or fight explicit user input.
    const auto *client = window->window();
    // Native start owns move/resize admission. Reported state may still describe
    // an older Wayland configure while our requested Active state is pending.
    if (client->isInteractiveMove() || client->isInteractiveResize()) return;
    // A window KWin has placed on another display, as a screenshot tool going
    // back to its last place does, takes only its own card there and stays.
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    KWin::LogicalOutput *placed = client->moveResizeOutput();
    if (tablet && placed && placed != tablet && !m_cardGrabActive
        && m_host->holdsOwnCardsForCardStage(placed)) {
        // §11, Every display: there it is the card that display shows, its
        // Stack with it, once this notification has returned.
        const QPointer<KWin::EffectWindow> moved = window;
        const QString there = placed->name();
        QTimer::singleShot(0, &m_arrivalTimer, [this, moved, there] {
            if (!moved || moved->isDeleted() || liveCardIndex(moved) < 0) return;
            KWin::LogicalOutput *display = KWin::effects->findScreen(there);
            if (display && m_host->holdsOwnCardsForCardStage(display)
                && (m_host->moveCardToDisplayForCardStage(moved, display)
                    || giveCardToDisplay(moved, display))) {
                KWin::effects->activateWindow(moved);
                return;
            }
            (void)releaseCard(moved, false);
        });
        return;
    }
    if (tablet && placed && placed != tablet && !m_cardGrabActive) {
        qInfo() << "Kadunce" << Revision << "lets only" << logNameOf(window)
                << "go: KWin placed it on" << placed->name();
        (void)releaseCard(window, false);
        return;
    }
    if (client->isRequestedFullScreen()
        || client->requestedMaximizeMode() != KWin::MaximizeRestore
        || client->requestedQuickTileMode() != KWin::QuickTileMode{}) {
        if (!keepOpeningStateInActivePlace(window)) letFillingCardGo(window);
        return;
    }
    if (tablet) {
        const KWin::RectF place(activePlacement(tablet));
        const QSizeF minimum = client->minSize();
        if (minimum.width() > place.width() || minimum.height() > place.height())
            keepTooBigInPlace(window, place);
    }
    if (m_activeSettleRemaining > 0 && !m_activeSettleTimer.isActive()) {
        m_activeSettleTimer.start();
    }
}

void CardStageController::handleManualWindowChange(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active
        || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || m_activeRestore.window != window) return;
    // The user's new state wins. Release presentation without replaying the
    // old snapshot over an in-progress move/resize or explicit state request.
    qInfo() << "Kadunce" << Revision << "lets every card go: KWin itself moved or resized the Active card"
            << (window && window->window() ? window->window()->resourceClass() : QString());
    m_activeRestore = ActiveRestoreSnapshot{};
    release();
}

bool CardStageController::keepOpeningStateInActivePlace(KWin::EffectWindow *window)
{
    // §3: an application restoring its saved state maximizes or tiles its
    // window a moment after showing it, by which time it is the Active card.
    // That is the window opening as it was left, not a person asking it to
    // fill the screen, so its wish is kept in its restore record, release
    // gives it back that way, and it stays the Active card. Full screen is
    // still let go: a game or a film asks for it as it opens.
    if (m_applyingWindowState || !window || !window->window() || !m_openedAt.contains(window)
        || m_openingClock.elapsed() - m_openedAt.value(window) > OpeningStateGrace) return false;
    KWin::Window *client = window->window();
    if (client->isRequestedFullScreen()) return false;
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet) return false;
    m_activeRestore.maximizeMode = client->requestedMaximizeMode();
    m_activeRestore.quickTileMode = client->requestedQuickTileMode();
    {
        QScopedValueRollback<bool> applying(m_applyingWindowState, true);
        if (client->requestedMaximizeMode() != KWin::MaximizeRestore)
            client->maximize(KWin::MaximizeRestore);
        if (client->requestedQuickTileMode() != KWin::QuickTileMode{})
            client->setQuickTileMode(KWin::QuickTileMode{}, window->frameGeometry().center());
        client->moveResize(KWin::RectF(activePlacement(tablet)));
    }
    m_activeSettleRemaining = std::max(m_activeSettleRemaining, 2);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "keeps" << logNameOf(window)
            << "the Active card: it opened in its saved state, given back on release";
    return true;
}

void CardStageController::letFillingCardGo(KWin::EffectWindow *window)
{
    if (m_applyingWindowState || !m_active
        || m_presentation != CardPresentation::Active
        || !m_activeRestore.valid || m_activeRestore.window != window) return;
    // The new state wins, for that window alone: it goes without its old
    // snapshot replayed over the request, and the other cards wait aside.
    qInfo() << "Kadunce" << Revision << "lets only" << logNameOf(window)
            << "go: it asked to fill the screen"
            << (window->window() ? window->window()->resourceClass() : QString());
    m_activeRestore = ActiveRestoreSnapshot{};
    (void)releaseCard(window, false);
    // §2, Desktop: it is a window returned to the desktop like one let go at
    // the bottom edge, so asking for it under a card opened later shows the
    // desktop again rather than leave it behind that card.
    if (m_active && m_presentation == CardPresentation::Active) returnWindowToDesktop(window);
}

} // namespace Kadunce

/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Carrying a card: the native carry to the desktop, setting cards aside, the
// card grab, and stacking order.
#include "CardStageInternal.h"

namespace Kadunce
{

bool CardStageController::selectedStackContains(const QPointF &position) const
{
    if (!m_active || m_presentation != CardPresentation::Spread) {
        return false;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    const int selectedId = m_workspace.selectedId();
    const int memberCount = m_workspace.stackSizeForId(selectedId);
    if (!tablet || memberCount <= 1) {
        return false;
    }
    const KWin::Rect center = m_launcherGuestActive ? cardTargetForSlot(tablet, 0)
        : posedTargetForWindow(tablet, selectedWindow());
    const CardStackEnvelope envelope = makeOpenStackEnvelope(
        memberCount, m_workspace.stackActivePositionForId(selectedId),
        center.width(), center.height());
    constexpr double VerticalSlop = 48.0;
    const KWin::RectF deck(
        center.x() + envelope.left,
        center.y() - VerticalSlop,
        center.width() + envelope.right - envelope.left,
        center.height() + VerticalSlop * 2.0);
    return deck.contains(position);
}

int CardStageController::activeSideForPoint(const QPointF &position) const
{
    if (!m_active || m_presentation != CardPresentation::Active) {
        return 0;
    }
    KWin::LogicalOutput *tablet = m_host->tabletOutputForCardStage();
    if (!tablet || !tablet->geometry().contains(position.toPoint())) {
        return 0;
    }
    const KWin::Rect active = activeTarget(tablet);
    if (position.x() < active.x()) {
        return -1;
    }
    if (position.x() >= active.right()) {
        return 1;
    }
    return 0;
}

std::optional<PreparedCarrySource> CardStageController::prepareNativeCarrySource(KWin::EffectWindow *window) const
{
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active || m_applyingWindowState || m_cardGrabActive || m_launcherGuestActive
        || m_presentation != CardPresentation::Active || !m_activeRestore.valid
        || !window || window->isDeleted() || !window->window() || !output
        || selectedWindow() != window || m_activeRestore.window != window
        || window->screen() != output || window->isUserResize() || window->isMinimized()
        || !KWin::effects->screens().contains(output)) return std::nullopt;
    PreparedCarrySource source;
    source.m_owner = m_carrySourceIdentity;
    source.m_window = window;
    source.m_generation = m_transferGuard.generation();
    source.m_outputGeometry = output->geometry();
    source.m_origin = {window->window()->internalId().toString(), output->name(),
        m_restoreGeneration, m_workspace.revision()};
    // Use the authoritative pre-Active record, NOT the current gutter geometry,
    // with the display its place was taken on: the carry stands on the tablet.
    source.m_restore = {m_activeRestore, window->window(), output};
    source.m_restore.minimized = false;
    return source;
}

QString CardStageController::nativeCarryRefusal(KWin::EffectWindow *window) const
{
    // The conditions of prepareNativeCarrySource, in its order, named.
    auto *output = m_host->tabletOutputForCardStage();
    if (!m_active) return QStringLiteral("no cards are held on this desktop");
    if (m_applyingWindowState) return QStringLiteral("a card is still being placed");
    if (m_cardGrabActive) return QStringLiteral("a card is held in Spread");
    if (m_launcherGuestActive) return QStringLiteral("Search is open");
    if (m_presentation != CardPresentation::Active)
        return m_presentation == CardPresentation::Bento ? QStringLiteral("Bento is shown")
            : m_presentation == CardPresentation::Desktop ? QStringLiteral("the desktop is shown")
            : QStringLiteral("Spread is shown");
    if (!window || window->isDeleted() || !window->window()) return QStringLiteral("the window is gone");
    if (selectedWindow() != window) return QStringLiteral("it is not the Active card");
    if (!m_activeRestore.valid || m_activeRestore.window != window)
        return QStringLiteral("the Active card has no restore record yet");
    if (!output || window->screen() != output) return QStringLiteral("it is not on the card display");
    if (window->isUserResize()) return QStringLiteral("it is being resized");
    if (window->isMinimized()) return QStringLiteral("it is minimized");
    return {};
}

bool CardStageController::nativeCarrySourceValid(const PreparedCarrySource &source) const
{
    if (source.m_owner.lock() != m_carrySourceIdentity) return false;
    const auto current = prepareNativeCarrySource(source.m_window);
    return current && current->m_origin == source.m_origin
        && current->m_generation == source.m_generation
        && current->m_outputGeometry == source.m_outputGeometry
        && current->m_restore.output == source.m_restore.output;
}

bool CardStageController::transferNativeCarryToDesktop(const PreparedCarrySource &source,
    KWin::LogicalOutput *destination, const KWin::RectF &geometry)
{
    if (!nativeCarrySourceValid(source) || !destination
        || m_host->isTabletOutputForCardStage(destination)
        || !KWin::effects->screens().contains(destination) || !geometry.isValid()
        || source.m_window->isUserMove() || source.m_window->isUserResize()) return false;
    const auto removal = m_workspace.prepareRemoval(source.m_window);
    if (!removal) return false;
    const QPointer<KWin::EffectWindow> arrival = source.m_window;
    const QPointer<KWin::LogicalOutput> target = destination;
    const auto targetGeometry = destination->geometry();
    bool committed = false;
    (void)m_host->admitCardToDesktopStage(arrival, destination, geometry,
        [&] {
            if (committed || !target || target->geometry() != targetGeometry
                || !KWin::effects->screens().contains(target.data())
                || !nativeCarrySourceValid(source) || arrival->isUserMove() || arrival->isUserResize()
                || !m_workspace.commitRemoval(*removal)) return false;
            committed = true;
            // Retire source restoration BEFORE the receiver performs native work.
            // This is logical state only: never replay the departed window's old
            // geometry during cleanup, release, or synchronous activation signals.
            ++m_restoreGeneration;
            forgetManagedRestore(arrival);
            m_presentation = CardPresentation::Spread;
            m_active = !m_workspace.windows().isEmpty();
            m_originalCardStackingOrder.removeAll(arrival);
            return true;
        },
        [&] {
            if (!committed) return;
            m_activeSettleTimer.stop();
            m_activeSettleRemaining = 0;
            if (arrival && !arrival->isDeleted()) {
                KWin::effects->setElevatedWindow(arrival, false);
                m_host->unredirectForCardStage(arrival);
            }
            m_host->setPagingShortcutsForCardStage(m_active);
            // §11, Every display: the display it left shows the card used
            // before it, as when its Active card is let go, not its Spread.
            KWin::LogicalOutput *own = m_host->tabletOutputForCardStage();
            const auto shows = [&](KWin::EffectWindow *card) {
                return card && card != arrival && !card->isDeleted() && card->window()
                    && !card->isMinimized() && card->screen() == own && liveCardIndex(card) >= 0;
            };
            KWin::EffectWindow *back = shows(m_usedBefore) ? m_usedBefore.data()
                : shows(selectedWindow()) ? selectedWindow() : nullptr;
            for (const auto &card : m_workspace.windows())
                if (!back && shows(card)) back = card;
            if (m_active && own && m_host->releaseDisplayForCardStage(nullptr) && back && selectCardEntry(back))
                (void)enterActive();
            syncSelectedElevation();
            KWin::effects->addRepaintFull();
        });
    // After publication, a receiver interruption is consumed, never a request
    // to replay source removal/restoration. The receiver follows this contract.
    return committed;
}

void CardStageController::holdCardsAside()
{
    for (const auto &window : std::as_const(m_workspace.windows())) {
        if (!window || window->isDeleted() || !window->window()
            || window->window()->isHidden() || m_aside.contains(window)) continue;
        window->setData(CardAsideRole, true);
        KWin::Window *client = window->window();
        client->setHidden(true);
        // KWin takes a hidden X11 window off the task list too, and again
        // whenever it rechecks the window. A card held aside is still open, so
        // it stays listed as a Wayland card does, unless its application
        // asked otherwise.
        const auto keepListed = [client] {
            if (client->isHidden() && client->skipTaskbar() != client->originalSkipTaskbar())
                client->setSkipTaskbar(client->originalSkipTaskbar());
        };
        keepListed();
        m_asideListed.insert(window, QObject::connect(client, &KWin::Window::skipTaskbarChanged, keepListed));
        m_aside.append(window);
    }
}

void CardStageController::bringCardsBack()
{
    for (const auto &listed : std::exchange(m_asideListed, {})) QObject::disconnect(listed);
    for (const auto &window : std::exchange(m_aside, {})) {
        if (!window || window->isDeleted()) continue;
        window->setData(CardAsideRole, QVariant());
        if (window->window()) window->window()->setHidden(false);
    }
}

void CardStageController::returnWindowToDesktop(KWin::EffectWindow *window)
{
    if (!m_active || !window || window->isDeleted() || liveCardIndex(window) >= 0) return;
    finishCardGrab(false);
    clearCardTransition();
    // §11, Connected: a pane going to the desktop leaves its Stack behind, and
    // the cards that waited there stay together in Spread, as when it closes.
    if (const auto behind = paneStack(window); !behind.isEmpty()) {
        m_paneStacks.remove(window);
        if (behind.size() > 1) (void)restack(behind.first(), behind.mid(1));
        qInfo() << "Kadunce" << Revision << "kept the Stack" << logNameOf(window)
                << "left for the desktop:" << behind.size() << "cards together";
    }
    if (m_presentation == CardPresentation::Active) parkActiveSnapshot();
    m_returnedToDesktop.removeIf([window](const auto &w) { return !w || w->isDeleted() || w == window; });
    m_returnedToDesktop.append(window);
    showDesktop();
}

void CardStageController::showDesktop()
{
    if (!m_active) return;
    m_presentation = CardPresentation::Desktop;
    m_returnToDesktop = false;
    syncSelectedElevation();
    m_host->setPagingShortcutsForCardStage(false);
    // The window last returned to the desktop takes the keys first, so KWin
    // has no hidden card to hand them on from; one that is a card again since
    // is no longer on the desktop.
    m_returnedToDesktop.removeIf([this](const auto &w) {
        return !w || w->isDeleted() || liveCardIndex(w) >= 0;
    });
    if (!m_returnedToDesktop.isEmpty() && m_returnedToDesktop.last()->window())
        KWin::workspace()->activateWindow(m_returnedToDesktop.last()->window(), true);
    holdCardsAside();
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "showed the desktop, with" << m_aside.size() << "cards aside";
}

bool CardStageController::releaseNativeCarryToDesktop(const PreparedCarrySource &source,
    KWin::LogicalOutput *output, const KWin::RectF &geometry)
{
    if (!m_active || m_launcherGuestActive || !nativeCarrySourceValid(source) || !output
        || !m_host->isTabletOutputForCardStage(output)
        || !KWin::effects->screens().contains(output) || !geometry.isValid()
        || source.m_window->isUserMove() || source.m_window->isUserResize()) return false;
    const QPointer<KWin::EffectWindow> leaving = source.m_window;
    const QPointer<KWin::Window> client = leaving->window();
    if (!client || liveCardIndex(leaving) < 0) return false;
    const auto removal = m_workspace.prepareRemoval(leaving);
    if (!removal || !m_workspace.commitRemoval(*removal)) return false;
    // The card leaves Spread before any native work, which can reenter this
    // stage, and its old geometry is never replayed: it lands where it was let go.
    ++m_restoreGeneration;
    forgetManagedRestore(leaving);
    if (m_activeRestore.window == leaving) m_activeRestore = ActiveRestoreSnapshot{};
    if (m_presentedActive == leaving) m_presentedActive = nullptr;
    m_originalCardStackingOrder.removeAll(leaving);
    m_activeSettleTimer.stop();
    m_activeSettleRemaining = 0;
    KWin::effects->setElevatedWindow(leaving, false);
    m_host->unredirectForCardStage(leaving);
    // §12: with no card left the display's session ends, and the next window
    // to open there starts cards again. Otherwise the desktop shows, so the
    // window is seen where it was let go.
    m_presentation = CardPresentation::Spread;
    m_active = !m_workspace.windows().isEmpty();
    const QPointer<KWin::LogicalOutput> target = output;
    const auto valid = [&] {
        return target && KWin::effects->screens().contains(target.data())
            && leaving && !leaving->isDeleted() && leaving->window() == client
            && !leaving->isUserMove() && !leaving->isUserResize();
    };
    // Where the person let it go, with the keys; the cards go aside after,
    // so KWin never has a hidden window to hand the focus on from.
    if (applyNativePlacement(client.data(), target.data(), geometry, valid)) {
        KWin::workspace()->raiseWindow(client);
        if (valid()) KWin::workspace()->activateWindow(client, true);
    }
    if (m_active && leaving) returnWindowToDesktop(leaving);
    m_host->setPagingShortcutsForCardStage(m_active && m_presentation != CardPresentation::Desktop);
    KWin::effects->addRepaintFull();
    return true;
}

void CardStageController::beginCardGrab(const QPointF &position)
{
    if (!m_active || m_presentation != CardPresentation::Spread
        || m_cardGrabActive || m_launcherGuestActive
        || !m_row.still() || m_lift.phase != Lift::Phase::None
        || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!tablet || !frame) return;
    // The card under the finger, wherever it stands. A pane is a card of its
    // own and is held as one (CARD-LIFECYCLE.md §11).
    KWin::EffectWindow *window = cardAt(tablet, position);
    const int cardId = liveCardIndex(window) + 1;
    if (!window || cardId <= 0) return;
    // Freeze the presentation at pickup: the finger keeps the point of the
    // card it took, and the row stands where it stood.
    auto pickup = previewTargetForWindow(tablet, window);
    auto pickupPose = stackPoseForWindow(window, pickup.width());
    pickup.translate(qRound(pickupPose.x), qRound(pickupPose.y));
    (void)applyPoseTransition(window, pickup, pickupPose);
    if (pickup.isEmpty()) return;
    const QPointer<KWin::EffectWindow> selectedBefore = selectedWindow();
    const int rowIndexBefore = m_workspace.selectedIndex();
    const QPointer<KWin::EffectWindow> faceBefore = m_workspace.windows().value(
        m_workspace.idAtOffset(m_workspace.model().entryIndexForId(cardId) - rowIndexBefore) - 1);
    // A card in a Stack is reordered within it; only a pull down takes it out.
    const std::vector<int> membersBefore = m_workspace.stackMembersForId(cardId);
    const bool inStack = membersBefore.size() > 1;
    captureCardTransition(false, true);
    if (!m_workspace.selectCard(cardId)) {
        m_carry.faceBefore = faceBefore;
        m_carry.selectedBefore = selectedBefore;
        restoreCarryOrigin();
        m_carry = {};
        clearCardTransition();
        return;
    }
    m_cardGrabActive = true;
    m_pickupTransition = m_poseTransition;
    m_stackStepTransition = m_poseTransition && inStack;
    m_stackOutlineFade.invalidate();
    m_cardGrabOffset = {};
    m_cardGrabStart = position;
    m_cardGrabTarget = pickup;
    m_cardGrabRotation = pickupPose.rotation;
    m_cardGrabScaleTimer.start();
    m_cardGrabDestinationOutput.clear();
    m_carry = {};
    m_carry.heldIndex = m_workspace.selectedIndex();
    m_carry.inStack = inStack;
    if (inStack) {
        // Held in its own Stack, the card stays over it and the row stands:
        // sideways travel takes it through the Stack's places, counted from
        // the card that was in front before the hold.
        const auto face = std::find(membersBefore.cbegin(), membersBefore.cend(),
                                    liveCardIndex(faceBefore) + 1);
        m_carry.stack = carryStackDeck(membersBefore,
            face == membersBefore.cend() ? 0 : int(std::distance(membersBefore.cbegin(), face)));
        const auto place = std::find(m_carry.stack.cbegin(), m_carry.stack.cend(), cardId);
        m_carry.home = int(std::distance(m_carry.stack.cbegin(), place));
        m_carry.paging = {m_carry.home, position.x()};
    } else {
        // The row without the held card, every entry standing as one.
        for (int entry = 0; entry < m_workspace.count(); ++entry)
            if (entry != m_carry.heldIndex) m_carry.items.push_back({entry, 0});
        m_carry.low = 0;
        m_carry.high = int(m_carry.items.size());
        m_carry.home = m_carry.heldIndex;
    }
    // Every item keeps its place, parted where the card was.
    m_carry.gap = m_carry.home;
    m_carry.shifts.assign(m_carry.items.size(), 0.0);
    for (int k = m_carry.home; k < int(m_carry.items.size()); ++k) m_carry.shifts[std::size_t(k)] = 1.0;
    m_carry.aim = {CarryAim::Kind::Gap, m_carry.home};
    m_carry.candidate = m_carry.aim;
    m_carry.candidateSince.start();
    // The row stands as it stood.
    m_carry.position = rowIndexBefore * frame->pitch;
    m_carry.heldWidth = pickup.width();
    m_carry.grip = QPointF((position.x() - pickup.x()) / pickup.width(),
                           (position.y() - pickup.y()) / pickup.height());
    m_carry.lean = carryLeanStart(position.x(), frame->screenLeft, frame->screenWidth);
    m_carry.selectedBefore = selectedBefore;
    m_carry.faceBefore = faceBefore;
    m_carry.animating = true;
    if (!m_motionClock.isValid()) m_motionClock.start();
    KWin::effects->setElevatedWindow(window, true);
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "lifted Spread card" << cardId
            << (inStack ? "to reorder its Stack" : "from entry") << m_carry.heldIndex + 1;
}

void CardStageController::updateCardGrab(const QPointF &position)
{
    if (!m_cardGrabActive || !qIsFinite(position.x()) || !qIsFinite(position.y())) {
        return;
    }
    m_cardGrabOffset = position - m_cardGrabStart;
    updateCardGrabDestination(position);
    if (!m_carry.moved
        && std::hypot(m_cardGrabOffset.x(), m_cardGrabOffset.y()) >= CarryMoveDistance) {
        m_carry.moved = true;
        qInfo() << "Kadunce" << Revision << "carrying Spread card" << m_workspace.selectedId();
    }
    if (m_carry.inStack) pageHeldStackCard(position.x());
    aimCarry();
    m_carry.animating = true;
    if (!m_motionClock.isValid()) m_motionClock.start();
    KWin::effects->addRepaintFull();
}

void CardStageController::updateCardGrabDestination(const QPointF &position)
{
    if (!m_cardGrabActive) {
        return;
    }
    KWin::LogicalOutput *output = KWin::effects->screenAt(position.toPoint());
    m_cardGrabDestinationOutput = output
        && !m_host->isTabletOutputForCardStage(output)
        ? output->name() : QString();
}

void CardStageController::pageHeldStackCard(double fingerX)
{
    const auto paging = carryPageStack(m_carry.paging, fingerX, int(m_carry.stack.size()));
    const int step = paging.place - m_carry.paging.place;
    if (step == 0) {
        m_carry.paging = paging;
        return;
    }
    // The Stack's cards move to their new places as the slow stroke moves
    // them, the card passed coming forward or going back.
    captureCardTransition(false, true);
    m_carry.paging = paging;
    m_stackBrowseDirection = step > 0 ? 1 : -1;
    m_stackStepTransition = true;
    qInfo() << "Kadunce" << Revision << "held card would take place" << paging.place + 1
            << "of" << m_carry.stack.size() << "in its Stack";
}

void CardStageController::aimCarry()
{
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!m_cardGrabActive || !m_carry.moved || m_carry.inStack || !frame) return;
    // The held card's centre, along the row.
    const QRectF held = QRectF(cardGrabTarget()).translated(m_cardGrabOffset);
    const double centre = carryRowAt(held.center().x(), *frame);
    const int items = int(m_carry.items.size());
    std::vector<double> positions(std::size_t(items), 0.0);
    // A card joins any card.
    std::vector<double> reach(std::size_t(items), 0.0);
    for (int k = 0; k < items; ++k) {
        positions[std::size_t(k)] = carryItemPosition(k, frame->pitch);
        const int entry = m_carry.items[std::size_t(k)].entry;
        const auto face = m_workspace.windows().value(
            m_workspace.idAtOffset(entry - m_workspace.selectedIndex()) - 1);
        if (face) reach[std::size_t(k)] = CarryJoinReach;
    }
    if (m_carry.sliding) {
        // A row sliding under the card stands closed up and joins nothing:
        // its gap follows the card, to part where the row stops.
        const int gap = carryGapAt(centre, positions, m_carry.low, m_carry.high);
        m_carry.aim = m_carry.candidate = {CarryAim::Kind::Gap, gap};
        m_carry.gap = gap;
        m_carry.candidateSince.restart();
        return;
    }
    CarryAim aim = carryAim(centre, positions, frame->width, reach, m_carry.low, m_carry.high);
    // A card that would join another keeps it until carried a little further
    // off, so a hand resting on it does not flicker between the two.
    if (m_carry.aim.kind == CarryAim::Kind::Card
        && (aim.kind != CarryAim::Kind::Card || aim.index != m_carry.aim.index)
        && m_carry.aim.index < items
        && std::abs(centre - positions[std::size_t(m_carry.aim.index)])
            < CarryJoinReach * CarryJoinKeep * frame->width) {
        aim = {CarryAim::Kind::Card, m_carry.aim.index};
    }
    if (!(aim == m_carry.candidate)) {
        m_carry.candidate = aim;
        m_carry.candidateSince.restart();
    }
    if (aim == m_carry.aim) return;
    const CarryAim next = carryAimStep(m_carry.aim, aim, m_carry.candidateSince.elapsed(), m_carry.gap);
    if (next == m_carry.aim) return;
    m_carry.aim = next;
    if (next.kind == CarryAim::Kind::Gap) {
        m_carry.gap = next.index;
    } else {
        m_carry.riseIndex = next.index;
        qInfo() << "Kadunce" << Revision << "held card would join item" << next.index + 1;
    }
    m_carry.animating = true;
}

void CardStageController::advanceCarry(double seconds)
{
    auto *tablet = m_host->tabletOutputForCardStage();
    const auto frame = carryFrame(tablet);
    if (!m_cardGrabActive || !frame) return;
    bool moving = false;
    // The row parting and closing under the card keeps the hand's pace, and
    // at instant stands where it is going.
    const bool instant = motionIsInstant();
    const auto ease = [&](double &value, double target, double seconds_, double tau, double close) {
        if (instant) {
            value = target;
            return;
        }
        value += (target - value) * (1.0 - std::exp(-seconds_ / tau));
        if (std::abs(target - value) < close) value = target;
        else moving = true;
    };
    if (m_carry.inStack) {
        // Held in its own Stack, the card rises and lifts out of it and the
        // row stands; its Stack's cards move as sideways travel pages it.
        ease(m_carry.heldWidth, m_cardGrabTarget.width() * carryHeldFraction(1.0),
             seconds, 0.08, 0.3);
        ease(m_carry.lift, 1.0, seconds, 0.08, 0.002);
        m_carry.animating = moving;
        return;
    }
    const int items = int(m_carry.items.size());
    const int span = m_carry.high - m_carry.low;
    const bool onTablet = m_cardGrabDestinationOutput.isEmpty();
    const double finger = m_cardGrabStart.x() + m_cardGrabOffset.x();
    if (m_carry.moved)
        m_carry.lean = carryLeanFollow(m_carry.lean, finger, frame->screenLeft, frame->screenWidth);
    m_carry.depth = m_carry.moved && onTablet
        ? carryLeanDepth(finger, m_carry.lean, frame->screenLeft, frame->screenWidth) : 0.0;
    // The row stays three across. Pulled down a short way, the card takes it
    // to one set zoom, and back up to three across, gliding either way.
    if (m_carry.moved) {
        const bool zoomed = carryZoomed(m_carry.zoomed, m_cardGrabOffset.y());
        if (zoomed != m_carry.zoomed) {
            m_carry.zoomed = zoomed;
            m_carry.zoomFrom = m_carry.scale;
            m_carry.zoomSince.start();
            qInfo() << "Kadunce" << Revision << (zoomed ? "zoomed out under" : "zoomed back in under")
                    << "Spread card" << m_workspace.selectedId();
        }
    }
    const double level = m_carry.zoomed
        ? carryZoomLevel(span, frame->pitch, frame->width, frame->screenWidth) : 1.0;
    const bool gliding = m_carry.zoomSince.isValid() && m_carry.zoomSince.elapsed() < motion(CarryZoomDuration);
    const double scale = gliding
        ? carryZoomAt(m_carry.zoomFrom, level, double(m_carry.zoomSince.elapsed()) / motion(CarryZoomDuration))
        : level;
    if (gliding) moving = true;
    // The zoom moves around the held card, so what is under it stays under it.
    const double heldX = QRectF(cardGrabTarget()).translated(m_cardGrabOffset).center().x();
    const double under = carryRowAt(heldX, *frame);
    m_carry.scale = scale;
    if (m_carry.moved)
        m_carry.position = under - (heldX - frame->centreX) / std::max(m_carry.scale, 0.05);
    const double heldTarget = m_carry.moved
        ? frame->width * carryHeldFraction(m_carry.scale)
        : m_cardGrabTarget.width() * carryHeldFraction(1.0);
    ease(m_carry.heldWidth, heldTarget, seconds, 0.08, 0.3);
    // Carried, the card comes to sit centred under the finger, so where it
    // was picked up changes nothing about where it goes.
    if (m_carry.moved) ease(m_carry.grip.rx(), 0.5, seconds, 0.08, 0.002);
    if (m_carry.moved) {
        if (m_carry.zoomed
            && carryRowFits(span, frame->pitch, frame->width, frame->screenWidth, m_carry.scale)) {
            // All of it shows: it stands centred, and a lean has nothing to reach.
            ease(m_carry.position, (items - 1 + m_carry.gapWidth) / 2.0 * frame->pitch,
                 seconds, 0.09, 0.3);
            m_carry.sliding = false;
        } else {
            const double speed = onTablet
                ? carrySlideSpeed(finger, m_carry.lean, frame->screenLeft, frame->screenWidth,
                                  m_carry.scale) : 0.0;
            const double before = m_carry.position;
            m_carry.position = carryClampPosition(m_carry.position + speed * seconds,
                                                  m_carry.low, m_carry.high, frame->pitch);
            // Sliding, the row stands closed up; creeping or stopped, even
            // against an end, it parts under the card.
            const double shown = std::abs(m_carry.position - before) * m_carry.scale
                / std::max(seconds, 0.001);
            m_carry.sliding = shown > CarrySlideShown;
            if (m_carry.position != before) moving = true;
        }
        // Come to rest over a card, the row settles it under the held one,
        // where the held card stood then; it does not follow the finger after.
        if (m_carry.depth == 0.0 && !m_carry.sliding
            && m_carry.candidate.kind == CarryAim::Kind::Card) {
            if (m_carry.settle != m_carry.candidate.index) {
                m_carry.settle = m_carry.candidate.index;
                m_carry.settleX = heldX;
            }
            ease(m_carry.position, carryItemPosition(m_carry.settle, frame->pitch)
                    - (m_carry.settleX - frame->centreX) / std::max(m_carry.scale, 0.05),
                 seconds, CarrySettleTime, 0.3);
        } else {
            m_carry.settle = -1;
        }
    }
    // The row moved under a still finger, so what it is over may have too.
    aimCarry();
    // The held card's gap: whole where it was picked up, closed while the row
    // slides, parted where it stops. Over a card it holds as it is, so parting
    // never pushes that card out from under the held one.
    const bool overCard = m_carry.moved && !m_carry.sliding
        && (m_carry.candidate.kind == CarryAim::Kind::Card || m_carry.aim.kind == CarryAim::Kind::Card);
    // The card nearest the held one stands still as the gap closes or parts,
    // so a card carried right onto its neighbour finds it where it showed.
    if (!overCard) {
        const double before = m_carry.gapWidth;
        ease(m_carry.gapWidth, !m_carry.moved ? 1.0 : m_carry.sliding ? 0.0 : CarryGapWidth,
             seconds, 0.09, 0.002);
        if (m_carry.moved && m_carry.gapWidth != before)
            m_carry.position = carryGapAnchored(m_carry.position, carryRowAt(heldX, *frame),
                m_carry.shifts, frame->pitch, before, m_carry.gapWidth);
    }
    for (int k = 0; k < items; ++k) {
        const double target = k >= m_carry.gap ? 1.0 : 0.0;
        ease(m_carry.shifts[std::size_t(k)], target, seconds, 0.055, 0.002);
    }
    // The card under a resting one rises as it waits, and the rest of the
    // way once it would take it.
    double riseTarget = 0.0;
    if (m_carry.aim.kind == CarryAim::Kind::Card) {
        riseTarget = 1.0;
    } else if (m_carry.moved && m_carry.candidate.kind == CarryAim::Kind::Card) {
        m_carry.riseIndex = m_carry.candidate.index;
        riseTarget = 0.6 * std::min(1.0, double(m_carry.candidateSince.elapsed()) / double(carryJoinDwell()));
    }
    ease(m_carry.rise, riseTarget, seconds, 0.05, 0.002);
    if (riseTarget == 0.0 && m_carry.rise == 0.0) {
        m_carry.riseIndex = -1;
    }
    // A card resting on another, or over a new gap, is still waiting to count.
    if (!(m_carry.candidate == m_carry.aim) && m_carry.moved) moving = true;
    m_carry.animating = moving;
}

void CardStageController::restoreCarryOrigin()
{
    if (m_workspace.hasDetachedMember()) m_workspace.restoreDetachedMember();
    // The Stack shows the face it had, and the row the entry it had.
    for (const auto &window : {m_carry.faceBefore, m_carry.selectedBefore}) {
        const int cardId = liveCardIndex(window) + 1;
        if (cardId > 0) (void)m_workspace.selectCard(cardId);
    }
}

void CardStageController::finishCardGrab(bool commit)
{
    if (!m_cardGrabActive) {
        return;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    // Release animation is presentation only. Capture before changing order;
    // the held surface remains directly attached to its contact until release.
    // A cancel glides too: the held card goes back from where it is drawn to
    // its place and the row closes up around it. Only Spread draws that
    // motion; a caller that leaves Spread starts its own or clears it, and
    // release(), which hands every window back, always clears it.
    if (m_launcherGuestActive) {
        if (!commit) clearCardTransition();
    } else {
        captureCardTransition(false, true);
        if (m_carry.inStack) {
            // Its Stack closes at the pace it parted.
            m_stackStepTransition = m_poseTransition;
        } else if (commit && m_poseTransition) {
            // The card lands in one move: it drops into the line and the row
            // grows back around it, cards leaving the screen as it does.
            m_landTransition = true;
            m_landScale = m_carry.scale;
            m_landWindow = grabbed;
            m_slideLeavers = true;
        }
    }
    if (m_carry.inStack) {
        // The outline of its place fades rather than vanishing.
        const auto outline = stackOutline();
        m_stackOutlineRect = outline.rect;
        m_stackOutlineFrom = outline.opacity;
        m_stackOutlineFade.start();
    }
    const CarryAim aim = m_carry.aim;
    const bool still = commit && !m_carry.moved;
    const bool home = m_carry.inStack ? m_carry.paging.place == m_carry.home
                                      : aim.kind == CarryAim::Kind::Gap && aim.index == m_carry.home;
    bool stacked = false;
    const char *outcome = "stayed";
    if (!commit || still || home) {
        // Cancelled, let go still, or where it was: back as it was.
        restoreCarryOrigin();
        outcome = !commit ? "cancelled" : still ? "held still" : "stayed";
    } else if (m_carry.inStack) {
        // Into its own Stack at its new place, and whichever card is then in
        // front is the one the Stack shows.
        const int held = m_workspace.selectedId();
        auto others = m_workspace.stackMembersForId(held);
        others.erase(std::remove(others.begin(), others.end(), held), others.end());
        const auto landing = carryStackLanding(m_carry.stack, held, m_carry.paging.place, others);
        if (!others.empty() && m_workspace.detachSelectedMember())
            stacked = m_workspace.stackSelectedWith(others.front(), landing.insertion);
        if (stacked) {
            m_workspace.commitDetachedMember();
            stacked = m_workspace.selectCard(landing.face);
        } else {
            restoreCarryOrigin();
        }
        outcome = stacked ? "reordered" : "stayed";
    } else if (aim.kind == CarryAim::Kind::Card) {
        const int entry = m_carry.items[std::size_t(aim.index)].entry;
        const int destinationId = m_workspace.idAtOffset(entry - m_workspace.selectedIndex());
        // A pane joining a Stack leaves its own slot first; the Stack it
        // joins fills one pane, never two.
        const bool left = !(grabbed && inProjectedLayout(grabbed))
            || detachProjectedPane(grabbed);
        // It joins at the front, and is the face the Stack shows.
        stacked = left && m_workspace.stackSelectedWith(destinationId,
            m_workspace.model().stackActivePositionForId(destinationId));
        if (!stacked && !left) restoreCarryOrigin();
        outcome = stacked ? "joined" : "stayed";
    } else {
        const int movement = std::clamp(aim.index, 0, m_workspace.count() - 1)
            - m_workspace.selectedIndex();
        if (movement != 0) m_workspace.moveSelected(movement);
        outcome = movement != 0 ? "moved" : "stayed";
    }
    resetCardGrabState(grabbed, stacked);
    if (stacked) standStackWhole(selectedWindow());
    qInfo() << "Kadunce" << Revision << "released Spread card"
            << m_workspace.selectedId() << outcome << "at entry" << m_workspace.selectedIndex() + 1;
}

void CardStageController::resetCardGrabState(
    KWin::EffectWindow *grabbed, bool stacked)
{
    // A release changes the row. Do not replay a row page captured against
    // its old membership over the committed stack/drop.
    if (m_rowPageTransition) clearCardTransition();
    if (grabbed && !grabbed->isDeleted() && !stacked) {
        KWin::effects->setElevatedWindow(grabbed, false);
    }
    m_cardGrabActive = false;
    m_cardGrabOffset = {};
    m_cardGrabStart = {};
    m_cardGrabTarget = {};
    m_cardGrabRotation = 0.0;
    m_cardGrabScaleTimer.invalidate();
    m_cardGrabDestinationOutput.clear();
    m_carry = {};
    syncSelectedElevation();
    KWin::effects->addRepaintFull();
}

bool CardStageController::finishCardGrabOnOutput(const QPointF &position)
{
    if (!m_cardGrabActive
        || m_presentation != CardPresentation::Spread) {
        return false;
    }
    KWin::LogicalOutput *destination = KWin::effects->screenAt(
        position.toPoint());
    if (!destination || !QRectF(destination->geometry()).contains(position)) {
        finishCardGrab(false);
        return true;
    }
    if (m_host->isTabletOutputForCardStage(destination)) {
        return false;
    }
    KWin::EffectWindow *grabbed = selectedWindow();
    const int cardIndex = liveCardIndex(grabbed);
    if (!grabbed || cardIndex < 0 || !grabbed->window()) {
        return false;
    }
    // A pane held on Connected stays in its layout when let go elsewhere.
    if (inProjectedLayout(grabbed)) { finishCardGrab(false); return true; }

    const auto removal = m_workspace.prepareRemoval(QPointer<KWin::EffectWindow>(grabbed));
    if (!removal) { finishCardGrab(false); return true; }
    QPointer<KWin::EffectWindow> arrival = grabbed;
    const bool accepted = m_host->admitCardToDesktopStage(
        grabbed, destination, KWin::RectF(activeTarget(destination)),
        [&] {
            if (!m_workspace.commitRemoval(*removal)) return false;
            forgetManagedRestore(arrival);
            return true;
        },
        [&] {
            // Both logical owners are published before native/visual cleanup.
            m_originalCardStackingOrder.removeAll(arrival);
            const bool empty = m_workspace.windows().isEmpty();
            if (empty) {
                m_active = false;
                m_presentation = CardPresentation::Spread;
                m_originalCardStackingOrder.clear();
            }
            resetCardGrabState(arrival, false);
            if (empty) m_host->setPagingShortcutsForCardStage(false);
            if (arrival && !arrival->isDeleted()) m_host->unredirectForCardStage(arrival);
        });
    if (!accepted) finishCardGrab(false);
    return true;
}

void CardStageController::syncSelectedElevation()
{
    // Whatever left the desktop, by Spread, a choice or an arrival, brings
    // the cards held aside back first. A placed layout can leave room open,
    // where the desktop shows, so the cards behind it are held aside as well
    // and nothing drawn nowhere takes a touch in that room.
    if (m_presentation == CardPresentation::Bento && m_host->layoutLeavesRoomForCardStage())
        holdCardsAside();
    else if (m_presentation != CardPresentation::Desktop) bringCardsBack();
    if (!m_active) {
        liftSurfacesAboveCards(false);
        return;
    }
    const int selectedId = m_workspace.selectedId();
    bool cardsElevated = false;
    // Tette owns the center for its entire guest lease, including drawer
    // collapse. Restore stack elevation only after the guest actually leaves.
    for (int index = 0; index < m_workspace.windows().size(); ++index) {
        KWin::EffectWindow *window = m_workspace.windows().at(index).data();
        if (window && !window->isDeleted()) {
            const bool elevated = m_presentation == CardPresentation::Spread
                && !m_launcherGuestActive
                && index + 1 == selectedId
                && m_workspace.stackSizeForId(selectedId) > 1;
            KWin::effects->setElevatedWindow(window, elevated);
            cardsElevated = cardsElevated || elevated;
        }
    }
    liftSurfacesAboveCards(cardsElevated);
    syncSelectedStackingOrder();
}

void CardStageController::liftSurfacesAboveCards(bool cardsElevated)
{
    // KWin draws an elevated window over everything it stacks, so the dock,
    // a panel applet's popup, a notification and the keys would fall under
    // the selected Stack. Each is elevated after the cards, in KWin's own
    // order, and so stays drawn where KWin stacks it: above them.
    for (const auto &surface : std::as_const(m_surfacesAboveCards))
        if (surface && !surface->isDeleted()) KWin::effects->setElevatedWindow(surface, false);
    m_surfacesAboveCards.clear();
    m_cardsElevated = cardsElevated;
    if (!cardsElevated) return;
    for (auto *window : KWin::effects->stackingOrder()) liftSurfaceAboveCards(window);
}

void CardStageController::liftSurfaceAboveCards(KWin::EffectWindow *window)
{
    if (!m_cardsElevated || !window || window->isDeleted() || !window->window()
        || window->window()->layer() <= KWin::NormalLayer || liveCardIndex(window) >= 0
        || m_surfacesAboveCards.contains(window)) return;
    KWin::effects->setElevatedWindow(window, true);
    m_surfacesAboveCards.append(window);
}

void CardStageController::syncSelectedStackingOrder()
{
    if (!m_active || m_presentation != CardPresentation::Spread || m_launcherGuestActive) {
        return;
    }
    if (m_cardGrabActive) return;
    const int faceId = m_workspace.selectedId();
    const std::vector<int> paintOrder =
        m_workspace.stackPaintOrderForId(faceId);
    if (paintOrder.size() <= 1) {
        return;
    }
    for (const int cardId : paintOrder) {
        const int index = cardId - 1;
        if (index < 0 || index >= m_workspace.windows().size()) {
            continue;
        }
        KWin::EffectWindow *window = m_workspace.windows().at(index).data();
        if (window && !window->isDeleted() && window->window()) {
            KWin::workspace()->raiseWindow(window->window());
        }
    }
}

void CardStageController::restoreOriginalStackingOrder()
{
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(m_originalCardStackingOrder)) {
        if (window && !window->isDeleted() && window->window()) {
            KWin::workspace()->raiseWindow(window->window());
        }
    }
}

} // namespace Kadunce

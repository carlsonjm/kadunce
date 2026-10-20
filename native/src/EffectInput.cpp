/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The input router's side of the Effect: touches, the card gap, card grabs and
// Spread gestures (docs/INPUT.md).
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// A swipe up from the bottom bezel has opened Spread all the way once it has
// risen this share of the tablet's height past where it committed, about the
// four centimetres KWin gives three fingers on the tablet.
constexpr double BezelSpreadTravel = 0.22;
// A finger still rising this fast as it lifts, in logical pixels per
// millisecond, opens Spread however short the swipe.
constexpr double BezelFlickSpeed = 0.5;
}

WorkspacePresentation Effect::presentationForInput() const
{
    // Bento is presented by the desktop stage's real panes. Card Stage still
    // owns its hidden individual cards, but none of its gestures apply, so the
    // router sees the same thing it saw before those cards could coexist.
    if (!m_cardStage->isActive()
        || m_cardStage->presentation() == CardPresentation::Bento
        || m_cardStage->presentation() == CardPresentation::Desktop) {
        return WorkspacePresentation::Inactive;
    }
    return m_cardStage->presentation() == CardPresentation::Spread
        ? WorkspacePresentation::Spread
        : WorkspacePresentation::Active;
}

void Effect::settleChosenCardForInput()
{
    if (m_cardStage->finishGrowToActive()) observeCardOwnership();
}

WorkspaceInputGeometry Effect::geometryForInput() const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) {
        return {};
    }
    const KWin::Rect tabletRect = tablet->geometry();
    const auto *selected = m_cardStage->selectedWindow();
    const KWin::Rect centerCard = selected && !m_cardStage->cardGrabActive()
        && !m_cardStage->launcherGuestActive()
        ? m_cardStage->posedTargetForWindow(tablet, selected)
        : cardTargetForSlot(tablet, 0);
    // A touchscreen driving another display has that display's own edges to
    // start from, whichever mode the display is in.
    KWin::LogicalOutput *touched = KWin::effects->findScreen(m_touchOutputName);
    WorkspaceInputGeometry geometry{
        QRectF(tabletRect),
        QRectF(centerCard),
        double(tabletRect.bottom()),
        double(centerCard.right()),
    };
    if (touched) geometry.touchscreen = {QRectF(touched->geometry()), double(touched->geometry().bottom())};
    return geometry;
}

bool Effect::cardGrabActiveForInput() const
{
    return m_cardStage->cardGrabActive();
}

bool Effect::nativeWindowInteractionForInput() const
{
    return KWin::workspace()->moveResizeWindow() != nullptr;
}

bool Effect::centerCardContainsForInput(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return false;
    const auto *selected = m_cardStage->selectedWindow();
    const auto target = selected && !m_cardStage->launcherGuestActive()
        ? m_cardStage->posedTargetForWindow(tablet, selected)
        : cardTargetForSlot(tablet, 0);
    return target.contains(position.toPoint());
}

bool Effect::launcherGuestActiveForInput() const
{
    return m_cardStage->launcherGuestActive();
}

bool Effect::launcherGuestContainsForInput(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    return m_cardStage->launcherGuestActive() && tablet
        && (m_launcherGuestExpanded ? launcherGuestExpandedTarget(tablet) : m_cardStage->launcherGuestTarget(tablet))
               .contains(position.toPoint());
}

void Effect::toggleFromInput()
{
    toggle();
}

bool Effect::scrollFromFingersForInput() const
{
    // KWin counts every finger on the glass, those its recogniser took for a
    // gesture too, until each has lifted.
    const KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
    return m_cardStage->spreadOpening() || (touch && touch->touchPointCount() > 0);
}

void Effect::beginBezelSpreadFromInput()
{
    // The swipe opens the Spread of the display it rises on.
    m_spreadGesture = {};
    m_spreadGesture.key = m_scopedKey;
}

void Effect::followBezelSpreadFromInput(double rise)
{
    const KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return;
    followSpreadGesture(rise / (tablet->geometry().height() * BezelSpreadTravel));
}

void Effect::finishBezelSpreadFromInput(double rise, double speed, bool cancelled)
{
    using Mode = SpreadGesture::Mode;
    auto &gesture = m_spreadGesture;
    if (cancelled) {
        gesture.progress = 0.0;
    } else if (speed >= BezelFlickSpeed) {
        // A flick opens as surely as a pull past halfway, even one that lifts
        // before the row has begun to form.
        if (gesture.mode == Mode::Idle) gesture.mode = beginSpreadGesture();
        if (gesture.mode == Mode::Follow) {
            gesture.progress = 1.0;
        } else if (gesture.mode == Mode::Commit) {
            gesture.mode = Mode::Opened;
            toggle();
        }
    }
    qInfo() << "Kadunce bezel swipe let go, risen" << rise << "at" << speed << "px/ms"
            << (cancelled ? "cancelled" : "");
    finishSpreadGesture();
}

void Effect::dismissLauncherGuestFromInput()
{
    callLauncherGuestOwner(QStringLiteral("dismissGuest"));
    closeLauncherGuest();
}

void Effect::callLauncherGuestOwner(const QString &method, const QVariantList &arguments)
{
    if (m_launcherGuestOwner.isEmpty()) return;
    QDBusMessage request = QDBusMessage::createMethodCall(m_launcherGuestOwner,
        m_launcherGuestPath, m_launcherGuestInterface, method);
    request.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(request);
}

void Effect::tapBesideLauncherGuestFromInput(const QPointF &position)
{
    // Search closes. A tap on a card beside it opens that card, as a tap on
    // any card in Spread does; Search grown to the whole display has none.
    const bool card = !m_launcherGuestExpanded
        && m_cardStage->endLauncherGuestOnCard(position);
    dismissLauncherGuestFromInput();
    if (card) activateSelectedFromInput();
}

void Effect::pageLeftFromInput()
{
    pageLeft();
}

void Effect::pageRightFromInput()
{
    pageRight();
}

void Effect::pageStackFromInput(int delta)
{
    pageStack(delta);
}

bool Effect::hasActiveDesktopStage() const
{
    return m_desktopStage && m_desktopStage->hasActiveSession();
}

KWin::LogicalOutput *Effect::externalDesktopOutput() const
{
    // The largest output that is not the tablet. CARD-LIFECYCLE.md §3 gives a
    // Bento action to it while one is attached.
    KWin::LogicalOutput *largest = nullptr;
    double largestArea = 0.0;
    for (auto *output : KWin::effects->screens()) {
        if (!output || isTabletOutput(output)) continue;
        const auto geometry = output->geometry();
        const double area = double(geometry.width()) * double(geometry.height());
        if (area > largestArea) { largestArea = area; largest = output; }
    }
    return largest;
}

bool Effect::cancelForwardedTouchForInput()
{
    auto *server = KWin::waylandServer();
    if (!server || !server->seat() || !server->seat()->isTouchSequence()) return false;
    // Cancel only the client delivery, not the physical input stream that
    // our router now owns until release. This prevents a swipe becoming a tap.
    server->seat()->notifyTouchCancel();
    return true;
}

QRectF Effect::nativeLandingAreaForOutput(KWin::LogicalOutput *output) const
{
    QList<QRectF> docks;
    for (auto *window : KWin::effects->stackingOrder()) {
        if (window && !window->isDeleted() && window->isDock()
            && window->isOnCurrentDesktop() && window->isOnCurrentActivity()
            && window->window() && window->window()->isShown())
            docks.append(QRectF(window->frameGeometry()));
    }
    return nativeLandingArea(QRectF(output->geometry()),
        QRectF(KWin::effects->clientArea(KWin::MaximizeArea, output)), docks);
}

bool Effect::isPanelPoint(const QPointF &position) const
{
    // Use Plasma's actual input surface, not a fixed bottom strip: floating,
    // vertical and hidden panels must keep their own geometry and visibility.
    // The tray opens a separate AppletPopup surface. Its controls (especially
    // Disable) must remain reachable while the other device owns a card hold.
    // Do not exempt arbitrary app dialogs or the desktop behind the cards.
    for (auto *window : KWin::effects->stackingOrder()) {
        if (window && !window->isDeleted()
            && (window->isDock() || window->isAppletPopup())
            && window->isOnCurrentDesktop() && window->isOnCurrentActivity()
            && window->window() && window->window()->isShown()
            && window->window()->hitTest(position)) return true;
    }
    return false;
}

// The gutters are Kadunce's. Every card and Bento pane stands a gutter in from
// the display's edges and from its neighbours, and an application drawing its
// own title bar keeps an invisible resize border wider than that, so in a
// gutter the pointer was over a resize border and took a resize shape. While
// the pointer is in the gutter around one of Kadunce's own cards or panes,
// Kadunce holds it, and its shape says what a press there does: a page tab in
// the gap beside the Active card, a divider's resize across a divider's whole
// reach, panes included, and an arrow elsewhere, where nothing takes a press.
// KWin picks the pointer's window before any input filter runs, so the hold
// is the mouse interception KWin gives effects such as Overview; the input
// router comes before KWin's effect filter, so its presses still arrive.
void Effect::quietCardGap(const QPointF &position)
{
    // An open Table holds the pointer too, as Overview does: it is drawn over
    // the windows rather than being one, so without the hold KWin hands the
    // pointer to the client beneath, which sets its own shape and never hears
    // the pointer move again. While a finger types a name
    // on the keys it lets go: while an effect holds the pointer KWin finds no
    // window under any touch, so the keys would never feel their own taps.
    HeldPointerPlace place;
    HeldPointer held = HeldPointer::None;
    if (m_table.isOpen()) {
        place.tableOpen = true;
        place.tableCarrying = m_table.carrying();
        if (!(m_tableRenaming >= 0 && m_tableRenameByTouch)) held = heldPointer(place);
    } else {
        // A press, a drag and a move or resize under way keep what they have.
        if (KWin::input()->pointer()->areButtonsPressed() || KWin::waylandServer()->seat()->isDragPointer()
            || KWin::workspace()->moveResizeWindow()) return;
        // Since the last touch the pointer is hidden and still: a hold would
        // keep every touch from the window under it.
        if (m_pointerResting) held = HeldPointer::None;
        else {
            // A panel, or a popup of one lying over a divider, keeps its
            // own pointer, as its clicks are its own.
            const bool panel = isPanelPoint(position);
            if (!panel)
                for (const auto &rail : m_desktopStage->grabRails())
                    if (rail.hitArea.contains(position)) place.divider = rail.vertical ? 1 : 2;
            // The dock, the keys and Search beside the Active card keep their
            // own pointer.
            if (!panel && !inputPanelContainsForInput(position)
                && !(launcherGuestActiveForInput() && launcherGuestContainsForInput(position)))
                place.activeSide = isTabletPoint(position) ? activeSideForPoint(position)
                                                           : stackSideForPoint(position);
            place.cardGap = !panel && place.divider == 0 && inCardGap(position);
            held = heldPointer(place);
        }
    }
    if (held == m_heldPointer) return;
    const bool wasHeld = m_heldPointer != HeldPointer::None;
    m_heldPointer = held;
    m_gapHeld = held != HeldPointer::None;
    if (!m_gapHeld) {
        KWin::effects->stopMouseInterception(this);
        return;
    }
    showHeldPointer(held, wasHeld);
}

// KWin lets an effect holding the pointer choose among Qt's shapes, which the
// arrow, the resize shapes and grabbing are. A page tab is named in the
// cursor theme, so it is set on the source KWin shows for the hold; a theme
// without it shows the arrow.
void Effect::showHeldPointer(HeldPointer held, bool wasHeld)
{
    const Qt::CursorShape shape = held == HeldPointer::ResizeColumns ? Qt::SplitHCursor
        : held == HeldPointer::ResizeRows ? Qt::SplitVCursor
        : held == HeldPointer::Carrying ? Qt::ClosedHandCursor
        : Qt::ArrowCursor;
    if (wasHeld) KWin::effects->defineCursor(shape);
    else KWin::effects->startMouseInterception(this, shape);
    if (held != HeldPointer::PageLeft && held != HeldPointer::PageRight) return;
    // Another effect's hold, a drag or the lock screen shows a source of its
    // own, which keeps its shape.
    auto *source = qobject_cast<KWin::ShapeCursorSource *>(KWin::Cursors::self()->mouse()->source());
    if (!source || source->shape() != KWin::CursorShape(shape).name()) return;
    source->setShape(heldPointerName(held));
    if (source->image().isNull()) source->setShape(shape);
}

// A touch puts the pointer to rest, hidden where it was, until it moves
// again; an open Table keeps its own hold.
void Effect::touchBeganForInput()
{
    m_keysUpAtTouch = m_keysForPerson;
    m_pointerResting = true;
    if (m_table.isOpen() || m_heldPointer == HeldPointer::None) return;
    m_heldPointer = HeldPointer::None;
    m_gapHeld = false;
    KWin::effects->stopMouseInterception(this);
}

// Outside every window's frame, but inside the margin a card or pane keeps
// around its frame: the topmost window whose frame or input reaches the
// position decides.
bool Effect::inCardGap(const QPointF &position) const
{
    // A divider between panes lies in their gutter, and its press is the
    // router's: held here, KWin would hand that press to this effect and the
    // divider would never hear it.
    for (const auto &rail : m_desktopStage->grabRails())
        if (rail.hitArea.contains(position)) return false;
    const auto stack = KWin::workspace()->stackingOrder();
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::Window *window = *it;
        if (!window || window->isDeleted() || !window->isShown() || !window->isOnCurrentDesktop()
            || window->isHiddenByShowDesktop()) continue;
        // The cards a presented layout hides lie behind its panes, and the
        // gutter between the panes is the layout's, not theirs.
        if (m_cardStage->presentsBento() && window->effectWindow()
            && m_cardStage->liveCardIndex(window->effectWindow()) >= 0) continue;
        if (window->frameGeometry().contains(position)) return false;
        if (!window->hitTest(position)) continue;
        const KWin::EffectWindow *effectWindow = window->effectWindow();
        return effectWindow
            && m_ownership.ownerOf(reinterpret_cast<quintptr>(effectWindow)) != CardOwner::Native;
    }
    return false;
}

int Effect::activeSideForPoint(const QPointF &position) const
{
    // A window floating at its own size takes what lands on it.
    for (auto *window : KWin::effects->stackingOrder())
        if (floatsAtItsOwnSize(window) && !window->isMinimized() && window->isOnCurrentDesktop()
            && QRectF(window->frameGeometry()).contains(position)) return 0;
    return m_cardStage->activeSideForPoint(position);
}

bool Effect::selectedStackContains(const QPointF &position) const
{
    return m_cardStage->selectedStackContains(position);
}

void Effect::beginCardGrab(const QPointF &position)
{
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    m_cardStage->beginCardGrab(position);
    if (m_cardStage->cardGrabActive()) takeCarriedDialogs(m_cardStage->selectedWindow());
}

void Effect::updateCardGrab(const QPointF &position)
{
    const auto previousDestination = m_lineDestination;
    m_linePreview.reset();
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    m_cardStage->updateCardGrab(position);
    // A card held in its own Stack goes nowhere but its Stack.
    if (!m_cardStage->cardGrabActive() || m_cardStage->heldInStack()) return;
    for (auto *output : KWin::effects->screens()) {
        if (!QRectF(output->geometry()).contains(position)) continue;
        const auto edge = carryEdgeAt(output, position);
        std::optional<BentoSidePlacement> side;
        if (edge && (*edge == CarryEdge::Left || *edge == CarryEdge::Right))
            side = bentoSideChoice(*edge == CarryEdge::Right, position.y(),
                QRectF(output->geometry()).center().y(), previousDestination
                    && previousDestination->destinationOutput() == output
                    ? previousDestination->sidePlacement() : std::nullopt);
        // Spread's side edges only move the row under the card: Bento comes
        // from the Active card (CARD-LIFECYCLE.md §3). The top edge still
        // makes the carried card Active (§10).
        if (m_cardStage->canOwnCards(output) && edge != CarryEdge::Top) continue;
        const KWin::RectF geometry(QRectF(m_cardStage->cardGrabTarget())
            .translated(m_cardStage->cardGrabOffset()));
        if (m_cardStage->canOwnCards(output)) {
            // CARD-LIFECYCLE.md §3 and §10, asked exactly as the native carry
            // seam asks it. Spread selection chooses what is carried; §3 names
            // who it pairs with.
            auto *grabbed = selectedWindow();
            const bool leftEdge = *edge == CarryEdge::Left;
            const bool sideEdge = leftEdge || *edge == CarryEdge::Right;
            auto *partner = sideEdge
                ? m_cardStage->partnerForSideSnap(grabbed, leftEdge) : nullptr;
            const bool liveLayout = m_desktopStage->hasSessionOnOutput(output->name());
            const auto outcome = planEdgeEntry({
                .edge = *edge,
                // §3: a display with a live layout is owned, whether or not
                // this stage also holds individual cards on it.
                .ownsDisplay = m_cardStage->ownsDisplay(output) || liveLayout,
                .canOwnCards = true,
                .hasBentoLayout = liveLayout,
                .carriedEligible = isCardWindow(grabbed),
                .partnerNamed = partner != nullptr,
                .carriedIsActive = m_cardStage->activeCardIdentity() == grabbed,
            });
            if (outcome == EdgeEntryOutcome::MakeActive) {
                m_lineCardEntryOutput = output;
                m_linePreview = KWin::RectF(m_cardStage->activeTarget(output));
                m_lineDestinationWindow = grabbed;
                m_lineDestinationContact = position;
                break;
            }
            if (outcome != EdgeEntryOutcome::PairIntoBento || !side) break;
            m_lineDestination = m_desktopStage->prepareCardDrop(grabbed, output, geometry,
                DesktopStageController::CardDropIntent::ActivateBento, side, partner);
            if (m_lineDestination) {
                m_linePreview = m_desktopStage->cardDropPreview(*m_lineDestination);
                if (!m_linePreview) { m_lineDestination.reset(); break; }
                m_lineDestinationWindow = grabbed;
                m_lineDestinationContact = position;
            }
            break;
        }
        m_lineDestination = m_desktopStage->prepareCardDrop(selectedWindow(), output, geometry,
            edge ? DesktopStageController::CardDropIntent::ActivateBento
                 : DesktopStageController::CardDropIntent::OpenSpace, side);
        if (m_lineDestination) {
            m_linePreview = m_desktopStage->cardDropPreview(*m_lineDestination);
            if (side && !m_linePreview) { m_lineDestination.reset(); continue; }
            m_lineDestinationWindow = selectedWindow();
            m_lineDestinationContact = position;
        }
        break;
    }
}

void Effect::finishCardGrab(bool commit)
{
    m_cardStage->finishCardGrab(commit);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    m_lineCardEntryOutput.clear();
    if (m_dialogsLead) m_carriedDialogsRelease.start();
}

bool Effect::finishCardGrabOnOutput(const QPointF &position)
{
    if (m_dialogsLead) m_carriedDialogsRelease.start();
    // A release cannot invent a destination that was never evaluated in motion.
    if (position != m_lineDestinationContact) {
        m_lineDestination.reset();
        m_lineCardEntryOutput.clear();
    }
    // A card held in its own Stack is let go into it, wherever the finger is.
    if (m_cardStage->heldInStack()) return false;
    if (m_lineCardEntryOutput) {
        // §10: nothing distinct to pair with, so the carried card becomes the
        // Active card. Its grab is cancelled first, which returns it to the
        // membership it was lifted from.
        QPointer<KWin::EffectWindow> grabbed = m_lineDestinationWindow;
        m_lineCardEntryOutput.clear(); m_lineDestinationWindow.clear();
        m_cardStage->finishCardGrab(false);
        (void)m_cardStage->promoteToActive(grabbed);
        return true;
    }
    if (m_lineDestination
        && m_cardStage->canOwnCards(m_lineDestination->destinationOutput())) {
        const auto reserved = *m_lineDestination;
        QPointer<KWin::EffectWindow> carried = m_lineDestinationWindow;
        // The partner the reservation was prepared and validated against, never
        // one re-read at release: a selection between the two can otherwise
        // surrender one window's ownership while the layout publishes another.
        QPointer<KWin::EffectWindow> partner = reserved.namedPartner();
        if (!m_desktopStage->activatePreparedTabletDrop(reserved, nullptr,
                [this, carried, partner] {
                    return m_cardStage->releasePairToBento(carried, partner);
                })) {
            m_cardStage->finishCardGrab(false);
        } else {
            // §11, Connected: a card carried from Active to a side snaps
            // there too, so the room it leaves offers the other cards.
            if (reserved.placesAlone()) askRoomOffer(reserved.destinationOutput());
        }
        m_lineDestination.reset(); m_lineDestinationWindow.clear();
        return true;
    }
    // §10: the top edge's action commits only when released inside its zone,
    // and a release inside it is that action whether or not it was admitted.
    // Neither branch above claimed this one, so handing it to the ordinary
    // Spread drop would place the card where the person did not aim, which
    // §14 forbids a refused gesture from doing. Cancelling the grab restores
    // the membership the card was lifted from. Spread's side edges have no
    // action of their own: a release there is an ordinary drop.
    if (auto *edgeOutput = KWin::effects->screenAt(position.toPoint());
        edgeOutput && QRectF(edgeOutput->geometry()).contains(position)
        && m_cardStage->canOwnCards(edgeOutput)
        && carryEdgeAt(edgeOutput, position) == CarryEdge::Top) {
        m_cardStage->finishCardGrab(false);
        m_lineDestinationWindow.clear();
        return true;
    }
    // A card let go on a display that cannot hold cards joins its layout, and
    // one the layout has no room for springs back into the row.
    QPointer<KWin::EffectWindow> grabbed = selectedWindow();
    QPointer<KWin::LogicalOutput> destination = KWin::effects->screenAt(position.toPoint());
    const bool toLayout = grabbed && destination
        && QRectF(destination->geometry()).contains(position)
        && !m_cardStage->canOwnCards(destination);
    const bool result = m_cardStage->finishCardGrabOnOutput(position);
    m_lineDestination.reset(); m_lineDestinationWindow.clear();
    if (result && toLayout && grabbed && destination && m_cardStage->liveCardIndex(grabbed) >= 0)
        showGestureNote(destination, position, GestureRefusal::NoRoom);
    return result;
}

bool Effect::cardAtForInput(const QPointF &position) const
{
    return m_cardStage->cardAtPoint(position);
}

void Effect::tapSpreadFromInput(const QPointF &position)
{
    if (m_cardStage->tapSpread(position) == CardStageController::SpreadTap::Open)
        activateSelectedFromInput();
}

void Effect::syncSelectedElevation()
{
    m_cardStage->syncSelectedElevation();
}

void Effect::holdOverviewOff()
{
    // KDE's Overview claims three fingers up and down, four on a touchpad,
    // Meta+W, Meta+G and the top-left corner; unloading it gives them all up,
    // and the destructor loads it again.
    if (!KWin::effects->isEffectLoaded(OverviewEffect)) return;
    KWin::effects->unloadEffect(OverviewEffect);
    m_overviewHeld = true;
    qInfo() << "Kadunce" << Revision << "set KDE's Overview aside while it runs";
}

Effect::SpreadGesture::Mode Effect::beginSpreadGesture()
{
    using Mode = SpreadGesture::Mode;
    if (m_cardStage->launcherGuestActive() || m_cardStage->cardGrabActive() || m_table.isOpen())
        return Mode::Refused;
    if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
        return Mode::Refused;
    // From the Active card the row forms under the fingers; from a layout or
    // the desktop the row has to be made first, so it opens past halfway.
    return m_cardStage->beginOpenSpread() ? Mode::Follow : Mode::Commit;
}

void Effect::followSpreadGesture(qreal progress)
{
    // Three fingers on the touchscreen open its display's Spread.
    followSpreadGestureOn(progress, screenNamed(m_touchOutputName));
}

void Effect::followSpreadGestureOn(qreal progress, KWin::LogicalOutput *output)
{
    using Mode = SpreadGesture::Mode;
    auto &gesture = m_spreadGesture;
    if (!gesture.key) gesture.key = output ? stageKeyFor(output) : m_scopedKey;
    SessionScope scope(this, m_currentSession, gesture.key);
    noteDisplayInUse(output);
    if (gesture.mode == Mode::Idle) gesture.mode = beginSpreadGesture();
    gesture.progress = std::clamp(double(progress), 0.0, 1.0);
    if (gesture.mode == Mode::Follow) {
        m_cardStage->followOpenSpread(gesture.progress);
    } else if (gesture.mode == Mode::Commit && gesture.progress > 0.5) {
        gesture.mode = Mode::Opened;
        toggle();
    }
}

void Effect::finishSpreadGesture()
{
    SessionScope scope(this, m_currentSession, m_spreadGesture.key);
    // Past halfway Spread opens; short of it the card springs back.
    if (m_spreadGesture.mode == SpreadGesture::Mode::Follow)
        m_cardStage->finishOpenSpread(m_spreadGesture.progress > 0.5);
    m_spreadGesture = {};
}

} // namespace Kadunce

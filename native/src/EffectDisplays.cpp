/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Every display's stages: the desktop and display a card stage belongs to,
// the card book, cards moving between displays, and displays coming and going.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// The separately installed Rounded Corners effect trims windows itself.
const QString RoundedCornersEffect = QStringLiteral("kwin4_effect_shapecorners");
}

Effect::DesktopSession *Effect::sessionFor(KWin::VirtualDesktop *desktop)
{
    const QString id = desktop ? desktop->id() : QString();
    auto &slot = m_sessions[id];
    if (!slot) {
        slot = std::make_unique<DesktopSession>();
        slot->desktopId = id;
        slot->desktop = desktop;
        slot->host = std::make_unique<SessionHost>(this, slot.get());
        slot->layouts = std::make_unique<DesktopStageController>(
            static_cast<DesktopStageHost *>(slot->host.get()));
    }
    return slot.get();
}

CardStageController &Effect::stageOn(DesktopSession *session, const QString &key)
{
    auto &slot = session->displays[key];
    if (!slot) {
        slot = std::make_unique<DisplayStage>();
        slot->host = std::make_unique<StageHost>(this, session, key);
        slot->cards = std::make_unique<CardStageController>(static_cast<CardStageHost *>(slot->host.get()));
    }
    return *slot->cards;
}

Effect::DesktopSession *Effect::sessionHolding(const KWin::EffectWindow *window) const
{
    if (!window) return nullptr;
    auto *held = const_cast<KWin::EffectWindow *>(window);
    for (const auto &entry : m_sessions)
        if (entry.second->stageHolding(window) || entry.second->layouts->managesWindow(held))
            return entry.second.get();
    return nullptr;
}

CardStageController *Effect::stageHolding(const KWin::EffectWindow *window) const
{
    return m_scopedSession ? m_scopedSession->stageHolding(window) : nullptr;
}

CardStageController *Effect::stageKeepingStackOf(const KWin::EffectWindow *window) const
{
    // The stage holding the window as a card, else the one keeping the Stack
    // behind it as a pane.
    if (!m_scopedSession || !window) return nullptr;
    if (CardStageController *held = m_scopedSession->stageHolding(window)) return held;
    for (const auto &display : m_scopedSession->displays)
        if (!display.second->cards->paneStack(window).isEmpty()) return display.second->cards.get();
    return nullptr;
}

bool Effect::anySpreadShown() const
{
    if (!m_scopedSession) return false;
    for (const auto &display : m_scopedSession->displays)
        if (display.second->cards->isActive()
            && display.second->cards->presentation() == CardPresentation::Spread) return true;
    return false;
}

bool Effect::anyCardStageActive() const
{
    if (m_cardStage && m_cardStage->isActive()) return true;
    if (!m_currentSession) return false;
    for (const auto &display : m_currentSession->displays)
        if (display.second->cards->isActive()) return true;
    return false;
}

bool Effect::roundsCorners() const
{
    return m_fanApertureShader && m_fanPaintSizeLocation >= 0 && m_fanApertureOriginLocation >= 0
        && m_fanApertureSizeLocation >= 0 && m_fanApertureRadiusLocation >= 0
        && !KWin::effects->isEffectLoaded(RoundedCornersEffect);
}

QString Effect::stageKeyFor(const KWin::LogicalOutput *output) const
{
    // One display holding cards keeps one stage, which moves with it. A
    // display holding none has no stage of its own.
    if (!m_cardsOnEveryDisplay) return QString();
    return output && isCardDisplay(output) ? output->name() : m_scopedKey;
}

QString Effect::stageKeyOf(const KWin::EffectWindow *window) const
{
    // The stage holding it, else the one of the display it stands on.
    if (m_scopedSession)
        if (const auto held = m_scopedSession->keyHolding(window)) return *held;
    return stageKeyFor(window ? window->screen() : nullptr);
}

void Effect::scopeTo(DesktopSession *session, const QString &key)
{
    // Every display holding its own cards keeps its stage under its name;
    // none is kept twice, once by name and once as the primary display.
    const QString named = !m_cardsOnEveryDisplay ? QString() : key.isEmpty() ? m_cardOutputName : key;
    m_scopedSession = session;
    m_scopedKey = named;
    m_cardStage = session ? &stageOn(session, named) : nullptr;
    m_desktopStage = session ? session->layouts.get() : nullptr;
}

void Effect::noteDisplayInUse(const KWin::LogicalOutput *output)
{
    // What names no display of its own, a key or a timer, acts on the display
    // in use: the one last touched, or holding the window last focused. It is
    // taken up on the next turn, so a call under way keeps the display it began on.
    if (!output || !m_cardsOnEveryDisplay || !isCardDisplay(output)) return;
    const QString key = stageKeyFor(output);
    if (key == m_inUseKey) return;
    m_inUseKey = key;
    QTimer::singleShot(0, this, [this] { restInUse(); });
}

void Effect::enterInputScope(const QPointF &position, InputReach reach, qint64 contact)
{
    // A contact KWin no longer holds lifted where the router did not see it.
    if (reach == InputReach::Press && contact >= 0)
        if (const auto down = touchesDownForInput())
            m_strokeContacts.removeIf([&](qint64 held) {
                return held >= 0 && held != contact && !down->contains(qint32(held));
            });
    const bool stroking = !m_strokeContacts.isEmpty();
    QString key = stroking ? m_strokeKey : m_scopedKey;
    if (!stroking && (reach == InputReach::Press || reach == InputReach::Within))
        if (KWin::LogicalOutput *at = KWin::effects->screenAt(position.toPoint()); isCardDisplay(at))
            key = stageKeyFor(at);
    if (reach == InputReach::Press) {
        if (!stroking) m_strokeKey = key;
        m_strokeContacts.insert(contact);
        noteDisplayInUse(screenNamed(key));
    }
    m_inputScopes.push_back(std::make_unique<SessionScope>(this, m_currentSession, key));
}

void Effect::noteEdgeContact(const QPointF &position) const
{
    // A contact that lands at a display's top or bottom says how the edges
    // read it, before anything takes it, so a swipe there that did nothing
    // can be read back.
    const KWin::LogicalOutput *at = KWin::effects->screenAt(position.toPoint());
    if (!at || !m_inputRouter) return;
    const QRectF area(at->geometry());
    const bool nearBottom = position.y() >= area.bottom() - 24.0;
    if (!nearBottom && position.y() >= area.top() + 24.0) return;
    const WorkspaceInputGeometry edges = geometryForInput();
    qInfo() << "Kadunce edge contact" << position << "on" << at->name() << area
            << (nearBottom ? "at the bottom;" : "at the top;")
            << "edges owned" << m_inputRouter->ownsSystemEdges()
            << "backend" << (m_usesDirectSystemEdges ? "z13-direct" : "plasma-native")
            << "stage display" << (edges.isValid() ? edges.tablet : QRectF())
            << "touchscreen" << m_touchOutputName << edges.touchscreen.area
            << "keys here" << inputPanelContainsForInput(position)
            << "bottom band" << m_inputRouter->inBottomBezel(position)
            << "table open" << tableOpenForInput()
            << "strokes held" << m_strokeContacts.size() << "on" << m_strokeKey
            << "router holds" << m_inputRouter->heldForLog();
}

void Effect::leaveInputScope(InputReach reach, qint64 contact)
{
    if (!m_inputScopes.empty()) m_inputScopes.pop_back();
    if (reach == InputReach::Release) m_strokeContacts.remove(contact);
    if (reach == InputReach::Cancel) m_strokeContacts.removeIf([](qint64 held) { return held >= 0; });
}

QString Effect::inUseKey() const
{
    if (!m_cardsOnEveryDisplay) return QString();
    return isCardDisplay(screenNamed(m_inUseKey)) ? m_inUseKey : m_cardOutputName;
}

bool Effect::isInUseDisplay(const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && stageKeyFor(output) == inUseKey();
}

void Effect::restInUse()
{
    if (m_scopeDepth > 0) return;
    const QString key = inUseKey();
    if (key != m_scopedKey || m_scopedSession != m_currentSession) scopeTo(m_currentSession, key);
}

bool Effect::isTabletOutput(const KWin::LogicalOutput *output) const
{
    // The display of the stage named now: the one display holding cards, or
    // while every display holds its own, the one the stage is kept for.
    if (!output || m_cardOutputName.isEmpty()) return false;
    return output->name() == (m_scopedKey.isEmpty() ? m_cardOutputName : m_scopedKey);
}

bool Effect::isCardDisplay(const KWin::LogicalOutput *output) const
{
    return output && !m_cardOutputName.isEmpty()
        && (m_cardsOnEveryDisplay || output->name() == m_cardOutputName)
        && KWin::effects->screens().contains(const_cast<KWin::LogicalOutput *>(output));
}

void Effect::refreshCardOutput()
{
    QList<TouchDisplayOutput> outputs;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (output)
            outputs.append({output->name(), output->isInternal(), QSizeF(output->physicalSize()),
                QSizeF(output->geometry().width(), output->geometry().height()), output->uuid()});
    }
    // §11: the chosen display is kept by its lasting identity as well as its
    // name. A choice that has none yet takes its display's; one whose display
    // came back under another name keeps that name from now on.
    {
        const QString chosen = chosenOutputName(outputs, m_spreadChoice);
        QString identity = m_spreadChoice.identity;
        if (identity.isEmpty() && !chosen.isEmpty()) {
            for (const auto &output : outputs)
                if (output.name == chosen) identity = output.identity;
        }
        const QString display = chosen.isEmpty() ? m_spreadChoice.display : chosen;
        if (display != m_spreadChoice.display || identity != m_spreadChoice.identity) {
            m_spreadChoice.display = display;
            m_spreadChoice.identity = identity;
            writeSpreadChoice();
        }
    }
    QList<TouchDisplayDevice> devices;
    if (KWin::input()) {
        for (KWin::InputDevice *device : KWin::input()->devices()) {
            // Only a kernel touchscreen drives a display. Fake and virtual
            // input deliver contacts in global coordinates and belong to none.
            if (!device || !device->isTouch() || !device->isEnabled()
                || device->sysPath().isEmpty()) continue;
            devices.append({device->outputName(), device->property("size").toSizeF()});
        }
    }
    m_touchOutputName = touchOutputName(outputs, devices);
    // A touchscreen that arrives after KWin, or is plugged in later, gives
    // the edges to Kadunce's own router.
    adoptDirectSystemEdges();
    const QString name = cardOutputName(outputs, devices, m_spreadChoice);
    if (name == m_cardOutputName) return;
    const QString previous = m_cardOutputName;
    m_cardOutputName = name;
    qInfo() << "Kadunce" << Revision << (m_cardsOnEveryDisplay ? "the primary display is" : "cards belong to")
            << (name.isEmpty() ? QStringLiteral("no display") : name)
            << (previous.isEmpty() ? QString() : QStringLiteral("after ") + previous);
    if (m_cardsOnEveryDisplay) {
        // §11: every display keeps its own Spread and cards. The primary
        // display moves nothing; it is where a window or Search goes when
        // nothing else says where. A display that holds cards for the first
        // time holds its windows as cards, as switching Kadunce on does.
        if (m_scopeDepth == 0) restInUse();
        m_letGo = false;
        QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
        scheduleCardDisplaySettle();
        Q_EMIT workspaceContextChanged();
        return;
    }
    if (m_cardStage->isActive()) {
        cancelInputForCardStage();
        // With no display left to hold cards, they return to the desktop.
        // Otherwise Spread moves whole: every card goes to the new display in
        // the settle that follows, and that display's windows join it there.
        if (name.isEmpty()) m_cardStage->release();
        else m_cardDisplayMoved = true;
    } else if (!name.isEmpty()) {
        // §3: a display that takes cards over holds its windows as cards, as
        // switching Kadunce on does. The turn lets KWin finish moving windows.
        m_letGo = false;
        QTimer::singleShot(0, this, [this] { (void)startTabletInCards(nullptr); });
    }
    scheduleCardDisplaySettle();
    Q_EMIT workspaceContextChanged();
}

void Effect::setSpreadDisplay(const QString &outputName)
{
    KWin::LogicalOutput *output = outputName.isEmpty() ? nullptr : KWin::effects->findScreen(outputName);
    // A display away keeps the identity it was chosen with. Choosing the same
    // name again for another monitor that has it now is a new choice.
    const QString identity = output ? output->uuid()
        : outputName == m_spreadChoice.display ? m_spreadChoice.identity : QString();
    if (outputName == m_spreadChoice.display && identity == m_spreadChoice.identity) return;
    m_spreadChoice.display = outputName;
    m_spreadChoice.identity = identity;
    writeSpreadChoice();
    refreshCardOutput();
}

void Effect::writeSpreadChoice()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Spread"));
    if (m_spreadChoice.display.isEmpty()) group.deleteEntry(QStringLiteral("Display"));
    else group.writeEntry(QStringLiteral("Display"), m_spreadChoice.display);
    if (m_spreadChoice.identity.isEmpty()) group.deleteEntry(QStringLiteral("DisplayIdentity"));
    else group.writeEntry(QStringLiteral("DisplayIdentity"), m_spreadChoice.identity);
    group.sync();
}

QString Effect::chosenSizingFor(const KWin::LogicalOutput *output) const
{
    if (!output) return QString();
    return chosenSizing(m_sizingChoices, m_sizingIdentities, output->name(), output->uuid());
}

DisplaySizing Effect::sizingFor(const KWin::LogicalOutput *output) const
{
    if (!output) return DisplaySizing::Solo;
    const auto geometry = output->geometry();
    return Kadunce::displaySizing(chosenSizingFor(output),
        geometry.width(), geometry.height());
}

QString Effect::displaySizing(const QString &outputName) const
{
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (output && output->name() == outputName) return sizingName(sizingFor(output));
    }
    return QString();
}

void Effect::setDisplaySizing(const QString &outputName, const QString &sizing)
{
    // An empty or unknown choice returns the display to sizing by room.
    const std::optional<DisplaySizing> parsed = parseSizing(sizing);
    const QString chosen = parsed ? sizingName(*parsed) : QString();
    if (outputName.isEmpty()) return;
    KWin::LogicalOutput *output = KWin::effects->findScreen(outputName);
    if (output && output->name() != outputName) output = nullptr;
    const QString identity = output ? output->uuid() : QString();
    const QString current = output ? chosenSizingFor(output) : m_sizingChoices.value(outputName);
    if (current == chosen && (chosen.isEmpty() || m_sizingChoices.contains(outputName))) return;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Sizing"));
    KConfigGroup displays = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("SizingDisplays"));
    // A choice is kept once, under the display's name now: one this display
    // made under an earlier name goes, as does whatever another display left
    // under this name.
    const QStringList names = m_sizingIdentities.keys();
    for (const QString &name : names) {
        if (name == outputName || identity.isEmpty() || m_sizingIdentities.value(name) != identity) continue;
        m_sizingChoices.remove(name);
        m_sizingIdentities.remove(name);
        group.deleteEntry(name);
        displays.deleteEntry(name);
    }
    if (chosen.isEmpty()) {
        m_sizingChoices.remove(outputName);
        m_sizingIdentities.remove(outputName);
        group.deleteEntry(outputName);
        displays.deleteEntry(outputName);
    } else {
        m_sizingChoices.insert(outputName, chosen);
        group.writeEntry(outputName, chosen);
        if (identity.isEmpty()) {
            m_sizingIdentities.remove(outputName);
            displays.deleteEntry(outputName);
        } else {
            m_sizingIdentities.insert(outputName, identity);
            displays.writeEntry(outputName, identity);
        }
    }
    group.sync();
    QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
    Q_EMIT workspaceContextChanged();
    KWin::effects->addRepaintFull();
}

bool Effect::connectedForCardStage() const
{
    KWin::LogicalOutput *output = tabletOutput();
    return output && sizingFor(output) == DisplaySizing::Connected;
}

bool Effect::layoutLeavesRoomForCardStage() const
{
    KWin::LogicalOutput *output = tabletOutput();
    return output && m_desktopStage->presentsPlacedLayoutOn(output->name());
}

KWin::LogicalOutput *Effect::tabletOutput() const
{
    if (m_cardOutputName.isEmpty()) return nullptr;
    return screenNamed(m_scopedKey.isEmpty() ? m_cardOutputName : m_scopedKey);
}

KWin::LogicalOutput *Effect::primaryOutput() const
{
    return screenNamed(m_cardOutputName);
}

bool Effect::isTabletOutputForDesktopStage(
    const KWin::LogicalOutput *output) const
{
    // Asked for its own display (SessionHost), every display holding cards is
    // its layouts' card display: its own stage presents them, and takes the
    // windows they shed.
    return isTabletOutput(output);
}

bool Effect::allowsDesktopStageOnOutput(
    const KWin::LogicalOutput *output) const
{
    return output && KWin::effects->screens().contains(const_cast<KWin::LogicalOutput *>(output));
}

bool Effect::isManagedWindowForDesktopStage(
    const KWin::EffectWindow *window) const
{
    return isCardWindow(window);
}

KWin::LogicalOutput *Effect::tabletOutputForDesktopStage() const
{
    return tabletOutput();
}

bool Effect::connectedOutputForDesktopStage(
    const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && sizingFor(output) == DisplaySizing::Connected;
}

bool Effect::turnCarryWidth(int notches)
{
    // A part of a notch still belongs to the carry.
    if (notches == 0 || !m_carriedWindow || !m_carriedWindow->window()) return true;
    KWin::LogicalOutput *target = nullptr;
    for (auto *o : KWin::effects->screens())
        if (QRectF(o->geometry()).contains(m_lastCarryContact)) { target = o; break; }
    if (!target) return true;
    const bool top = m_carryWidthEdge == CarryWidthEdge::Top;
    const double span = workAreaForCardStage(target).width();
    if (span <= 0.0) return true;
    const std::span<const double> stops = top ? std::span<const double>(WheelWidthTopStops)
                                              : std::span<const double>(WheelWidthSideStops);
    // The first notch starts from the width the box shows, taken to the
    // stop it stands at.
    if (m_carryWidth <= 0.0) {
        // Held away from the edges, or with no box shown, it starts from the
        // width the window was picked up at.
        const bool fromPickup = !m_carryPreview || (top && m_carryWidthHeld);
        double width = std::clamp((fromPickup ? m_carryPickup.width() : m_carryPreview->width()) / span, 0.0, 1.0);
        if (!top && !m_carryPreview) width = 0.5;
        for (const double stop : stops)
            if (std::abs(stop - width) < .03) width = stop;
        m_carryKnob = WheelWidthKnob{width};
    }
    const double least = std::max(WheelWidthLeast,
        m_carriedWindow->window()->minSize().width() / span);
    const bool crank = m_carryWidthClock.isValid() && m_carryWidthClock.elapsed() < WheelWidthCrankMs;
    m_carryWidthClock.restart();
    m_carryKnob.turn(notches, crank, least, top ? 1.0 : WheelWidthSideMost, stops);
    m_carryWidth = m_carryKnob.width;
    updateNativeCarryDestination(m_lastCarryContact);
    KWin::effects->addRepaintFull();
    return true;
}

bool Effect::holdCarryWidth()
{
    // Pointer carries only: a touchscreen has no wheel.
    if (!m_carriedWindow) return false;
    const QPointF contact = KWin::effects->cursorPos();
    for (auto *o : KWin::effects->screens()) {
        if (!QRectF(o->geometry()).contains(contact)) continue;
        if (!connectedOutputForDesktopStage(o) || carryEdgeAt(o, contact) || isPanelPoint(contact)) return false;
        m_carryWidthHeld = true;
        m_carryWidthEdge = CarryWidthEdge::Top;
        m_carryWidth = 0.0;
        m_carryWidthClock.invalidate();
        m_lastCarryContact = contact;
        return true;
    }
    return false;
}

void Effect::fitArrivalWidth(QPointer<KWin::EffectWindow> window, QString output, double width, int tries)
{
    if (!window || window->isDeleted() || m_releasing) return;
    KWin::LogicalOutput *there = screenNamed(output);
    if (!there) return;
    {
        SessionScope scope(this, m_scopedSession, stageKeyFor(there));
        if (m_cardStage->activeCardIdentity() == window) {
            m_cardStage->setActiveWidth(window, width);
            if (m_cardStage->refitActive(window)) return;
        }
    }
    if (tries > 0)
        QTimer::singleShot(100, this, [this, window, output, width, tries] {
            fitArrivalWidth(window, output, width, tries - 1);
        });
}

BentoSidePlacement Effect::sideChoiceOn(const KWin::LogicalOutput *output, bool right,
    double y, std::optional<BentoSidePlacement> previous) const
{
    const QRectF geometry(output->geometry());
    if (connectedOutputForDesktopStage(output))
        return bentoConnectedSideChoice(right, y, geometry.top(), geometry.height(), previous);
    return bentoSideChoice(right, y, geometry.center().y(), previous);
}

bool Effect::holdsOwnCardsForDesktopStage(const KWin::LogicalOutput *output) const
{
    // Asked for its own display, no display is another's: each holds its
    // cards as the one display holding cards did.
    return isOtherCardDisplay(output);
}

KWin::LogicalOutput *Effect::releaseDisplayForCardStage(const KWin::EffectWindow *) const
{
    // §11, Every display: a card is given back on the display whose stage
    // holds it.
    if (!m_cardsOnEveryDisplay || KWin::effects->screens().size() < 2) return nullptr;
    return tabletOutput();
}

bool Effect::releasesOnOwnDisplayForDesktopStage(const KWin::LogicalOutput *output) const
{
    return m_cardsOnEveryDisplay && isCardDisplay(output);
}

bool Effect::showsOneCardForDesktopStage(const KWin::LogicalOutput *) const
{
    // A display sized Solo shows its Active card on its own stage, never as
    // a layout of one.
    return false;
}

bool Effect::isOtherCardDisplay(const KWin::LogicalOutput *output) const
{
    return isCardDisplay(output) && !isTabletOutput(output);
}

bool Effect::holdDisplaysInCards(KWin::EffectWindow *arrival)
{
    // CARD-LIFECYCLE.md §3, on every display holding cards. A window opening
    // starts only its own display's cards; after letting go only one does.
    bool held = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output) || (arrival && arrival->screen() != output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (startTabletInCards(arrival)) held = true;
    }
    return held;
}

bool Effect::moveCardToDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *to, bool show)
{
    // §11: a card leaving for another display holding its own cards is taken
    // by that display's stage from the one holding it, in one step, and its
    // Stack goes with it. Sent there, it is shown there: in the layout that
    // display shows, or as its Active card. Otherwise it waits in that
    // display's Spread, at the end of its row.
    DesktopSession *session = card ? sessionHolding(card) : nullptr;
    if (!session || card->isDeleted() || !card->window() || !isCardDisplay(to)) return false;
    // Read from the card's own desktop: a desktop not shown holds its cards
    // under keys the one shown cannot find.
    const auto heldAt = session->keyHolding(card);
    if (!heldAt) return false;
    SessionScope here(this, session, *heldAt);
    CardStageController *from = m_cardStage;
    const QString toKey = stageKeyFor(to);
    if (from->liveCardIndex(card) < 0 || toKey == m_scopedKey) return false;
    const QList<KWin::EffectWindow *> behind = cardsBehind(*from, card);
    const auto record = [from](KWin::EffectWindow *window) { return from->managedRestore(window); };
    const auto restore = record(card);
    CardStageController &into = stageOn(session, toKey);
    const QPointer<KWin::EffectWindow> moving = card;
    {
        SessionScope there(this, session, toKey);
        if (!into.admitCardFromOtherDisplay(card,
                [from, moving] { return moving && from->releaseCardToStage(moving); },
                restore ? &*restore : nullptr)) {
            qInfo() << "Kadunce" << Revision << "could not move" << logNameOf(card) << "to" << to->name();
            return false;
        }
    }
    from->forgetPaneStack(card);
    QList<KWin::EffectWindow *> carried;
    for (auto *member : std::as_const(behind)) {
        const auto memberRestore = record(member);
        const QPointer<KWin::EffectWindow> follower = member;
        SessionScope there(this, session, toKey);
        if (into.admitCardFromOtherDisplay(member,
                [from, follower] { return follower && from->releaseCardToStage(follower); },
                memberRestore ? &*memberRestore : nullptr)) carried.append(member);
    }
    {
        SessionScope there(this, session, toKey);
        if (!carried.isEmpty() && !into.stackBehind(card, carried))
            qInfo() << "Kadunce" << Revision << "could not stack" << carried.size() << "cards behind"
                    << logNameOf(card) << "on" << to->name();
        // Shown there once its window stands there: KWin moves it to that
        // display after this turn, and a layout takes only what stands on it.
        if (show && moving && !moving->isMinimized()) {
            QList<QPointer<KWin::EffectWindow>> behindIt;
            for (auto *member : std::as_const(carried)) behindIt.append(member);
            onThisStageLater(0, [this, moving, behindIt] { showMovedCard(moving, behindIt); });
        }
    }
    qInfo() << "Kadunce" << Revision << "moved" << logNameOf(card) << "to" << to->name()
            << "with" << carried.size() << "behind it" << (show ? "and shows it there" : "waiting in its Spread");
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    return true;
}

void Effect::showMovedCard(KWin::EffectWindow *card, const QList<QPointer<KWin::EffectWindow>> &behind, int tries)
{
    // §11: a card sent to this display is shown here: a pane the keys carried
    // away takes back the side it left, else it joins the layout shown here,
    // else it is the Active card.
    if (!card || card->isDeleted() || card->isMinimized() || m_releasing) return;
    // A layout here that took it in already holds its Stack with it.
    if (m_cardStage->liveCardIndex(card) < 0) {
        keepStackBehind(card, behind, 10);
        return;
    }
    KWin::LogicalOutput *here = tabletOutput();
    // Its application answers the move in its own time, and until it has the
    // window stands on the display it left.
    if (here && card->screen() != here && tries > 0) {
        onThisStageLater(50, [this, card = QPointer(card), behind, tries] {
            if (card) showMovedCard(card, behind, tries - 1);
        });
        return;
    }
    QList<KWin::EffectWindow *> stack;
    for (const auto &member : behind)
        if (member && !member->isDeleted()) stack.append(member.data());
    bool shown = restoreLeftLayout(card, stack);
    if (!shown && here && m_cardStage->presentsBento() && m_desktopStage->hasSessionOnOutput(here->name()))
        shown = m_desktopStage->admitCardToLiveBento(card,
            [this, card = QPointer(card)] { return card && m_cardStage->releaseCardToLiveBento(card); });
    if (shown) keepStackBehind(card, behind, 10);
    else (void)m_cardStage->presentCardActive(card, true);
    KWin::effects->activateWindow(card);
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::onThisStageLater(int ms, const std::function<void()> &f)
{
    const QString desktop = m_scopedSession ? m_scopedSession->desktopId : QString();
    const QString key = m_scopedKey;
    QTimer::singleShot(ms, this, [this, desktop, key, f] {
        const auto found = m_sessions.find(desktop);
        if (found == m_sessions.end()) return;
        SessionScope scope(this, found->second.get(), key);
        f();
    });
}

long long Effect::lastUsedForDesktopStage(const KWin::EffectWindow *window) const
{
    return window ? static_cast<long long>(m_activationOrder.value(windowIdentity(window))) : 0;
}

bool Effect::outputCanOwnCards(const KWin::LogicalOutput *output) const
{
    return m_cardStage->canOwnCards(output);
}

KWin::Rect Effect::activeTargetForDesktopStage(
    KWin::LogicalOutput *output) const
{
    return activeTarget(output);
}

void Effect::retireOutputFromDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output)) m_cardStage->leaveBentoPresentation();
}

void Effect::layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *output)
{
    // §11, Every display: a display whose layout's last pane was carried to
    // another shows its desktop, its cards waiting in its Spread, so the pane
    // carried back is shown, or takes its side again, as on any display.
    if (!m_cardsOnEveryDisplay || !output || !isTabletOutput(output)
        || m_cardStage->presentation() != CardPresentation::Bento) return;
    m_cardStage->showDesktop();
}

void Effect::displacedCardMayReturnForDesktopStage(KWin::EffectWindow *card, KWin::LogicalOutput *output)
{
    // §5: the card a pane became when an arrival took its slot comes back to
    // the layout once that arrival leaves, as long as it still waits behind
    // the layout as it was left there: not called forward, not asleep, not
    // sent to another display and not behind another card. The window in use
    // stays in use, as the arrival may have gone elsewhere with the person.
    if (!card || card->isDeleted() || card->isMinimized() || !output || m_releasing
        || card->screen() != output || !m_cardStage->presentsBento()
        || m_cardStage->liveCardIndex(card) < 0 || m_cardStage->paneHolding(card)
        || !m_desktopStage->hasSessionOnOutput(output->name())) return;
    const QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(card);
    if (!stack.isEmpty() && stack.first() != card) return;
    const QPointer<KWin::EffectWindow> back = card;
    if (!m_desktopStage->admitCardToLiveBento(card,
            [this, back] { return back && m_cardStage->releaseCardToLiveBento(back); }, true, false)) {
        qInfo() << "Kadunce" << Revision << "kept" << logNameOf(card) << "waiting behind the layout on"
                << output->name() << "with no room for it there";
        return;
    }
    // It was lowered under the panes as a hidden card. A pane again, it stands
    // above the cards still hidden there, or a press on it would reach one.
    if (back && back->window()) KWin::workspace()->raiseWindow(back->window());
    if (stack.size() > 1) m_cardStage->notePaneStack(card, stack.mid(1));
    m_cardStage->settlePaneStacks();
    qInfo() << "Kadunce" << Revision << "brought" << logNameOf(card) << "back to the layout on"
            << output->name() << "with" << qMax<qsizetype>(stack.size() - 1, 0)
            << "cards behind it, as the pane that took its place leaves";
    Q_EMIT workspaceContextChanged();
}

void Effect::prepareOutputForDesktopStage(KWin::LogicalOutput *output)
{
    if (output && isTabletOutput(output) && m_cardStage->isActive()) {
        qInfo() << "Kadunce" << Revision << "lets the cards go for a layout across" << output->name();
        m_cardStage->release(); // Do not release Bento sessions on other displays.
    }
}

std::optional<NativeMoveSnapshot> Effect::activeRestoreForDesktopStage(KWin::EffectWindow *window) const
{
    return m_cardStage->managedRestore(window);
}

bool Effect::admitDisplacedPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // §5: while the display presents its layout, the pane that yielded becomes
    // a nonselected card behind it. Anywhere else there is no layout in front
    // of it, and an ordinary transfer is the right arrival. A layout that holds
    // every window there owns no card yet, so the pane that yields starts card
    // ownership behind it, as any arrival behind a layout does.
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
    KWin::LogicalOutput *tablet = tabletOutput();
    const bool behindLayout = !m_cardStage->isActive() && tablet
        && m_desktopStage->hasSessionOnOutput(tablet->name()) && m_cardStage->startBehindLayout();
    if (m_cardStage->admitDisplacedPaneAsHiddenCard(window, commitSource, restore))
        return true;
    if (behindLayout) m_cardStage->stopBehindLayoutIfEmpty();
    return admitTransferredWindowToTablet(window, output, commitSource, restore);
}

void Effect::settleStacksOfArrival()
{
    // A pane that is a card again stands in front of its Stack in Spread's
    // row. The transfer that made it one is still under way, so the Stack is
    // settled the turn after, once the card is in the row.
    onThisStageLater(0, [this] {
        if (m_releasing || !m_cardStage) return;
        m_cardStage->settlePaneStacks();
        observeCardOwnership();
    });
}

CardStageController *Effect::carrySourceStage(const PreparedCarrySource &source) const
{
    if (m_cardStage && m_cardStage->nativeCarrySourceValid(source)) return m_cardStage;
    if (!m_scopedSession) return nullptr;
    for (const auto &display : m_scopedSession->displays)
        if (display.second->cards->nativeCarrySourceValid(source)) return display.second->cards.get();
    return nullptr;
}

bool Effect::cardCarrySourceValid(const PreparedCarrySource &source) const
{
    return carrySourceStage(source) != nullptr;
}

bool Effect::admitSleepingPaneToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // Unlike the awake door, this one cannot find the record for itself: it
    // would prepare a carry, and a carry refuses a minimized window. The caller
    // still holds the session that has it, so it supplies it, and without it
    // the state §13 restores the window to would be the one the user just asked
    // for -- release would re-minimize a window it had woken.
    if (!restore) return false;
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
    // §7: a minimized pane is a sleeping individual card, not an arrival. Both
    // other doors present what they take, so neither can hold one.
    return m_cardStage->admitSleepingPaneAsCard(window, commitSource, restore);
}

std::optional<CarryEdge> Effect::carryEdgeAt(const KWin::LogicalOutput *output, QPointF contact) const
{
    if (!output) return std::nullopt;
    const auto edge = monitorCarryEdge(QRectF(output->geometry()), contact);
    if (edge && *edge != CarryEdge::Top && isPanelPoint(contact)) return std::nullopt;
    return edge;
}

bool Effect::carriesToAnotherCardDisplay() const
{
    if (!m_cardsOnEveryDisplay || !tabletOutput()) return false;
    const auto screens = KWin::effects->screens();
    return std::any_of(screens.cbegin(), screens.cend(),
        [this](KWin::LogicalOutput *output) { return isOtherCardDisplay(output); });
}

void Effect::claimWindowForItsDisplay(KWin::EffectWindow *window, const QString &from)
{
    // §11, Every display: a window that left another display loose on one
    // holding its own cards is a card there, as one opening there is. A
    // window let go on its own display stays as it was let go.
    if (!window || window->isDeleted() || m_releasing || m_letGo || !window->window()
        || !isCardWindow(window) || isDependentWindow(window) || window->isMinimized()
        || window->isUserMove() || window->isUserResize() || window == m_carriedWindow
        || window == m_nativeCarry || !window->window()->readyForPainting()) return;
    KWin::LogicalOutput *output = window->screen();
    if (!isCardDisplay(output) || output->name() == from || sessionHolding(window)
        || m_desktopStage->managesWindow(window)) return;
    if (window == m_letGoOnDisplay) {
        qInfo() << "Kadunce" << Revision << "leaves" << logNameOf(window) << "on" << output->name()
                << "where it was let go at the bottom edge";
        return;
    }
    qInfo() << "Kadunce" << Revision << "takes" << logNameOf(window) << "left loose on"
            << output->name() << "as a card there";
    SessionScope scope(this, m_scopedSession, stageKeyFor(output));
    const bool taken = m_desktopStage->hasSessionOnOutput(output->name())
        ? m_desktopStage->handleWindowAdded(window)
        : m_cardStage->isActive() ? admitTransferredWindowToTablet(window, output)
        : startTabletInCards(window);
    if (taken) {
        KWin::effects->activateWindow(window);
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    }
}

bool Effect::admitTransferredWindowToTablet(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore)
{
    // The display named takes it, else the display holding cards it stands on.
    if (!output && window && isCardDisplay(window->screen())) output = window->screen();
    std::optional<SessionScope> scope;
    if (output) scope.emplace(this, m_scopedSession, stageKeyFor(output));
    // Copy before guest/source cleanup can retire NativeCarryRuntime's pose.
    const QRectF carriedOrigin = window == m_carriedWindow && m_carryRuntime
        ? QRectF(m_carryRuntime->handoff.carry().position(), m_carryPickup.size())
        : QRectF();
    if (m_cardStage->launcherGuestActive()) closeLauncherGuest();
    // A caller that still held the window's session record supplies it. Asking
    // the desktop stage afterwards would read the pane rectangle instead, since
    // by then the session has published a plan that no longer names the window.
    const auto source = restore ? std::nullopt : m_desktopStage->prepareNativeCarrySource(window);
    const auto derived = source ? std::optional<NativeMoveSnapshot>(source->restoreSnapshot())
        : std::nullopt;
    if (!restore && derived) restore = &*derived;
    // §11: a card another display's stage holds leaves it in the same step,
    // and its Stack follows it here.
    CardStageController *keeper = stageKeepingStackOf(window);
    if (keeper == m_cardStage) keeper = nullptr;
    const QList<KWin::EffectWindow *> behind = keeper ? cardsBehind(*keeper, window)
                                                      : QList<KWin::EffectWindow *>{};
    const QPointer<KWin::EffectWindow> moving = window;
    const std::function<bool()> commit = keeper
        ? std::function<bool()>([commitSource, keeper, moving] {
              return commitSource() && moving
                  && (keeper->liveCardIndex(moving) < 0 || keeper->releaseCardToStage(moving));
          })
        : commitSource;
    const bool admitted = m_cardStage->admitTransferredWindowToTablet(window, commit,
        carriedOrigin, restore);
    if (admitted && keeper) {
        keeper->forgetPaneStack(window);
        rejoinStack(window, behind);
    }
    return admitted;
}

void Effect::handleScreenRemoved(KWin::LogicalOutput *output)
{
    m_displaysChanged.start();
    if (output) m_displayLeft[output->name()].start();
    cancelInputForCardStage();
    if (m_table.isOpen() && tableOutput() == output) closeTable();
    forEachSession([&] { m_desktopStage->handleScreenRemoved(output); });
    // KWin still lists the display while announcing its removal.
    QTimer::singleShot(0, this, &Effect::refreshCardOutput);
    scheduleCardDisplaySettle();
}

void Effect::scheduleCardDisplaySettle()
{
    m_cardDisplaySettleRounds = 0;
    if (m_cardDisplaySettleQueued) return;
    m_cardDisplaySettleQueued = true;
    // KWin announces a display inside the call that rearranges windows for it,
    // and the placement it remembers for a layout is put back at the end of
    // that call. The turn after is the first one that sees where KWin left
    // every window, and it follows the card display's own refresh.
    QTimer::singleShot(0, this, &Effect::settleCardsOnDisplays);
}

void Effect::settleCardsOnDisplays()
{
    m_cardDisplaySettleQueued = false;
    // §11: a stage whose display went away, or that the way displays hold
    // cards no longer keeps, gives each card to the display it stands on now.
    handOverGoneStages();
    moveCardsToJoinedDisplay();
    // Each display holding cards settles its own, and is held again once
    // every card is placed.
    QTimer::singleShot(0, this, [this] { (void)holdDisplaysInCards(nullptr); });
    bool again = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (settleCardsOnThisDisplay()) again = true;
    }
    // A client can still acknowledge the placement KWin asked of it before the
    // return, and KWin applies an acknowledged placement where it was asked:
    // a card that kept its size goes straight back, then that answer takes it
    // to the other display again. Look once more after the round trip, until
    // nothing needs to move, a bounded number of times.
    constexpr int SettleRounds = 8;
    if (again && ++m_cardDisplaySettleRounds < SettleRounds) {
        QTimer::singleShot(250, this, [this] {
            if (!m_cardDisplaySettleQueued) settleCardsOnDisplays();
        });
    }
}

void Effect::handOverGoneStages()
{
    // Each card goes, its Stack with it, to the stage of the display KWin
    // moved its window to, else the primary display's, and waits at the end
    // of that Spread's row. The receiving display's Active card and layout stay.
    if (m_scopeDepth == 0) restInUse();
    KWin::LogicalOutput *primary = primaryOutput();
    if (!primary || !isCardDisplay(primary)) return;
    for (const auto &entry : m_sessions) {
        DesktopSession *session = entry.second.get();
        QStringList gone;
        for (const auto &display : session->displays) {
            const QString &key = display.first;
            if (key.isEmpty() ? m_cardsOnEveryDisplay : !isCardDisplay(screenNamed(key)) || !m_cardsOnEveryDisplay)
                gone.append(key);
        }
        for (const QString &key : std::as_const(gone)) {
            const auto found = session->displays.find(key);
            if (found == session->displays.end()) continue;
            CardStageController *stage = found->second->cards.get();
            QList<QPointer<KWin::EffectWindow>> cards;
            for (const auto &card : stage->liveCards())
                if (card && !card->isDeleted()) cards.append(card);
            if (cards.isEmpty()) continue;
            qInfo() << "Kadunce" << Revision << "hands" << cards.size() << "cards held for"
                    << (key.isEmpty() ? QStringLiteral("the one card display") : key) << "to the displays they stand on";
            // The card the gone display showed is shown where it went, when
            // nothing is shown there.
            const QPointer<KWin::EffectWindow> wasShown = stage->presentation() == CardPresentation::Active
                ? stage->activeCardIdentity() : nullptr;
            for (const auto &card : std::as_const(cards)) {
                if (!card || card->isDeleted() || stage->liveCardIndex(card) < 0) continue;
                // A card behind another goes with its face.
                const auto deck = stage->stackFrontToBack(card);
                if (!deck.isEmpty() && deck.first() != card) continue;
                KWin::LogicalOutput *there = card->window() ? card->window()->moveResizeOutput() : nullptr;
                if (!isCardDisplay(there)) there = primary;
                if (!moveCardToDisplay(card, there, false))
                    qInfo() << "Kadunce" << Revision << "could not hand" << logNameOf(card) << "to" << there->name();
            }
            const auto heldAt = wasShown && !wasShown->isDeleted() ? session->keyHolding(wasShown) : std::nullopt;
            if (heldAt && *heldAt != key) {
                SessionScope there(this, session, *heldAt);
                if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Desktop)
                    (void)m_cardStage->presentCardActive(wasShown, false);
            }
        }
    }
    forgetGoneStages();
}

KWin::LogicalOutput *Effect::dockDisplayBesides(const KWin::LogicalOutput *joined) const
{
    // The display a dock stands along the bottom of: the Shuffle Dock, or
    // Plasma's own bottom panel. Without one, the primary display.
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!window || window->isDeleted() || !window->isDock()) continue;
        KWin::LogicalOutput *output = window->screen();
        if (!output || output == joined || !isCardDisplay(output)) continue;
        const QRectF frame(window->frameGeometry());
        const QRectF area(output->geometry());
        // A floating panel stands a little above the edge.
        if (frame.y() > area.center().y() && frame.y() + frame.height() >= area.y() + area.height() - 32)
            return output;
    }
    KWin::LogicalOutput *primary = primaryOutput();
    return primary != joined ? primary : nullptr;
}

void Effect::noteDisplayJoined(KWin::LogicalOutput *output)
{
    // Read as KWin announces the display, before the dock has moved for it.
    if (!output || !m_cardsOnEveryDisplay || m_releasing) return;
    KWin::LogicalOutput *dock = dockDisplayBesides(output);
    if (!dock) return;
    // A display back within moments, as one is through sleep or a loose
    // cable, was not plugged in by the person: its cards come back as KWin
    // puts their windows back, and the rest stay where they are.
    constexpr qint64 ShortAbsence = 10000;
    const auto left = m_displayLeft.constFind(output->name());
    const bool returned = left != m_displayLeft.cend() && left->isValid() && left->elapsed() < ShortAbsence;
    m_displayJoined = DisplayJoin{dock->name(), output->name(), returned};
    qInfo() << "Kadunce" << Revision << output->name() << (returned ? "came back" : "was plugged in")
            << "beside the dock on" << dock->name();
}

void Effect::moveCardsToJoinedDisplay()
{
    // §11: plugged in, a display takes every card and pane of the display
    // with the dock, each Stack with its card. The one in use there is shown
    // on the new display and the rest wait in its Spread. A display that came
    // back from a short absence takes only the cards KWin put back on it.
    const auto join = std::exchange(m_displayJoined, std::nullopt);
    if (!join || m_releasing || !m_cardsOnEveryDisplay) return;
    KWin::LogicalOutput *from = screenNamed(join->from);
    KWin::LogicalOutput *to = screenNamed(join->to);
    if (!from || !to || from == to || !isCardDisplay(from) || !isCardDisplay(to)) return;
    cancelInputForCardStage();
    std::vector<DesktopSession *> sessions;
    for (const auto &entry : m_sessions) sessions.push_back(entry.second.get());
    int moved = 0;
    for (DesktopSession *session : sessions) {
        const auto found = session->displays.find(join->from);
        if (found == session->displays.end()) continue;
        CardStageController *stage = found->second->cards.get();
        const auto standsThere = [to](KWin::EffectWindow *window) {
            return window && window->window() && window->window()->moveResizeOutput() == to;
        };
        // The window in use there: the Active card, else the one used last.
        QPointer<KWin::EffectWindow> inUse = stage->isActive() && stage->presentation() == CardPresentation::Active
            ? stage->activeCardIdentity() : nullptr;
        if (!inUse) {
            quint64 latest = 0;
            QList<QPointer<KWin::EffectWindow>> candidates = stage->liveCards();
            candidates.append(session->layouts->panesOn(join->from));
            for (const auto &window : std::as_const(candidates)) {
                const quint64 used = window ? m_activationOrder.value(windowIdentity(window)) : 0;
                if (window && !window->isDeleted() && !window->isMinimized() && (!inUse || used > latest)) {
                    inUse = window;
                    latest = used;
                }
            }
        }
        QSet<KWin::EffectWindow *> tried;
        for (int round = 0; round < 64; ++round) {
            SessionScope here(this, session, join->from);
            if (m_cardStage->launcherGuestActive()) closeLauncherGuest();
            // Each pick is read again: a layout left with one pane ends into a
            // card, and a card's Stack goes with it.
            KWin::EffectWindow *pane = nullptr;
            if (!join->returned) {
                for (const auto &window : m_desktopStage->panesOn(join->from))
                    if (window && !window->isDeleted() && !tried.contains(window.data())
                        && (!pane || pane == inUse)) pane = window;
            }
            if (pane) {
                tried.insert(pane);
                if (m_desktopStage->handoffPaneToOutput(pane, to)) ++moved;
                continue;
            }
            KWin::EffectWindow *card = nullptr;
            for (const auto &window : m_cardStage->liveCards()) {
                if (!window || window->isDeleted() || tried.contains(window.data())) continue;
                if (join->returned && !standsThere(window)) continue;
                const auto deck = m_cardStage->stackFrontToBack(window);
                if (!deck.isEmpty() && deck.first() != window) continue;
                if (!card || card == inUse) card = window;
            }
            if (!card) break;
            tried.insert(card);
            const bool show = !join->returned && card == inUse && session == m_currentSession;
            if (moveCardToDisplay(card, to, show)) ++moved;
        }
        // A desktop not shown shows it there when it is shown again.
        if (join->returned || session == m_currentSession || !inUse || inUse->isDeleted()) continue;
        SessionScope there(this, session, join->to);
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Desktop
            && m_cardStage->liveCardIndex(inUse) >= 0)
            (void)m_cardStage->presentCardActive(inUse, false);
    }
    qInfo() << "Kadunce" << Revision << "moved" << moved << "cards and panes from" << join->from
            << "to" << join->to << (join->returned ? "as KWin put them back" : "as it was plugged in");
    if (moved > 0) {
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
    }
}

void Effect::forgetGoneStages()
{
    // A stage no display holds and no card is left in is let go, unless it is
    // the one named now.
    for (const auto &entry : m_sessions) {
        DesktopSession *session = entry.second.get();
        for (auto it = session->displays.begin(); it != session->displays.end();) {
            const QString &key = it->first;
            const bool kept = key.isEmpty() ? !m_cardsOnEveryDisplay
                                            : m_cardsOnEveryDisplay && isCardDisplay(screenNamed(key));
            if (kept || (session == m_scopedSession && key == m_scopedKey)
                || !it->second->cards->liveCards().isEmpty()) {
                ++it;
                continue;
            }
            it->second->cards->release();
            it = session->displays.erase(it);
        }
    }
}

bool Effect::settleCardsOnThisDisplay()
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet || !KWin::effects->screens().contains(tablet)) return false;
    // Spread itself moved: every card takes its place on the new display,
    // including those KWin already put there when the old one went away.
    const bool moved = std::exchange(m_cardDisplayMoved, false);
    if (moved) {
        // A layout Card Stage presented stayed on the display Spread left.
        if (m_cardStage->presentsBento() && !m_desktopStage->hasSessionOnOutput(tablet->name()))
            m_cardStage->leaveBentoPresentation();
        // The one card the new display showed becomes a card of Spread's.
        m_desktopStage->endLonePanes();
    }
    // The paint route hides any window on the card display that is not a card,
    // so a window KWin moved here must become one or it cannot be seen. So
    // must one that opened or arrived on this desktop while another was shown.
    // A window another desktop's session still holds is that session's.
    const auto arrived = [this, tablet](KWin::EffectWindow *window) {
        return window && isCardWindow(window) && window->isNormalWindow()
            && !isDependentWindow(window) && window->window()
            && window->window()->moveResizeOutput() == tablet
            && window->window()->readyForPainting()
            && window->isOnCurrentDesktop()
            && !window->isUserMove() && !window->isUserResize()
            && window != m_carriedWindow && window != m_nativeCarry
            && !sessionHolding(window);
    };
    QList<QPointer<KWin::EffectWindow>> arrivals;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (arrived(window)) arrivals.append(window);
    bool strayed = false;
    for (const auto &card : m_cardStage->liveCards()) {
        if (card && !card->isDeleted() && card->window()
            && card->window()->moveResizeOutput() != tablet) strayed = true;
    }
    // A desktop presenting its layout and owning no card takes what arrives
    // as cards behind the panes, as one that owns cards does. Left alone they
    // would lie over the panes as windows no one owns, taking every touch: a
    // stack Table carried back to a desktop holding only a layout did.
    const bool behindLayout = !m_cardStage->isActive() && !arrivals.isEmpty()
        && m_desktopStage->hasSessionOnOutput(tablet->name()) && m_cardStage->startBehindLayout();
    // With no card owned and no layout nothing is hidden, so an arrival is an
    // ordinary window there until ownership starts the ordinary way.
    if (!m_cardStage->isActive() || (arrivals.isEmpty() && !strayed && !moved)) return false;
    cancelInputForCardStage();
    // A client can still acknowledge the placement KWin asked of it before the
    // return, and KWin applies an acknowledged placement where it was asked:
    // a card that kept its size goes straight back, then that answer takes it
    // to the other display again. Look once more after the round trip, until
    // nothing needs to move, a bounded number of times.
    const bool returned = m_cardStage->returnCardsToDisplay(moved) > 0;
    // The arrival the person used last is the one they pick up again, so it
    // becomes the Active card and the others wait behind it. The card that was
    // in front stays in the line. Ties go to the higher window in the stack.
    KWin::EffectWindow *resume = nullptr;
    quint64 latest = 0;
    for (const auto &window : std::as_const(arrivals)) {
        if (!window || !m_cardStage->admitArrivalAsCard(window)) continue;
        const quint64 used = m_activationOrder.value(windowIdentity(window));
        if (!resume || used >= latest) {
            resume = window;
            latest = used;
        }
    }
    if (behindLayout) m_cardStage->stopBehindLayoutIfEmpty();
    restackCarriedDecks();
    // A card stacked behind another is reached through its face.
    if (resume) {
        const QList<KWin::EffectWindow *> deck = m_cardStage->stackFrontToBack(resume);
        (void)m_cardStage->promoteToActive(deck.isEmpty() ? resume : deck.first());
    }
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    return returned;
}

void Effect::restackCarriedDecks()
{
    KWin::VirtualDesktop *desktop = KWin::effects->currentDesktop();
    const auto found = desktop ? m_pendingDecks.find(desktop->id()) : m_pendingDecks.end();
    if (found == m_pendingDecks.end()) return;
    auto &decks = found.value();
    for (auto it = decks.begin(); it != decks.end();) {
        QList<KWin::EffectWindow *> alive;
        for (const auto &window : std::as_const(*it))
            if (window && !window->isDeleted() && window->isOnDesktop(desktop)) alive.append(window);
        if (alive.size() < 2) {
            it = decks.erase(it);
            continue;
        }
        // Until every one of its windows is a card here, it waits.
        if (!std::all_of(alive.cbegin(), alive.cend(), [this](KWin::EffectWindow *window) {
                return m_cardStage->liveCardIndex(window) >= 0; })) {
            ++it;
            continue;
        }
        if (!m_cardStage->stackBehind(alive.first(), alive.mid(1)))
            qInfo() << "Kadunce could not stack again the" << alive.size() << "cards Table carried here";
        it = decks.erase(it);
    }
    if (decks.isEmpty()) m_pendingDecks.erase(found);
}

void Effect::observeCardOwnership()
{
    observeCardBook();
    // One ledger over every desktop: a window has one owner across all of
    // them. A display has at most one layout per desktop, so another desktop's
    // layout is named with its desktop.
    std::vector<quintptr> cards;
    std::vector<BentoOwnershipView> sessions;
    // A card two desktops both hold is still one kind of owner to the ledger,
    // so it is caught here.
    std::vector<OwnershipViolation> reported;
    QSet<quintptr> held;
    for (const auto &entry : m_sessions) {
        const DesktopSession *session = entry.second.get();
        // Each display's cards are individual cards alike.
        std::vector<const CardStageController *> stages;
        for (const auto &display : session->displays) stages.push_back(display.second->cards.get());
        for (const CardStageController *stage : stages) {
            for (const auto &window : stage->liveCards()) {
                if (!window) continue;
                const auto identity = reinterpret_cast<quintptr>(window.data());
                if (held.contains(identity))
                    reported.push_back({identity, OwnershipViolation::Rule::TwoOwners,
                                        QStringLiteral("desktop ") + entry.first});
                held.insert(identity);
                cards.push_back(identity);
            }
        }
        for (auto view : session->layouts->ownershipView()) {
            if (session != m_currentSession) view.output += QStringLiteral(" on desktop ") + entry.first;
            sessions.push_back(std::move(view));
        }
    }

    // Every window whose owner changed is put to the ledger as a transition.
    // The ledger accepts only the six the contract defines, so a change it
    // refuses is a change §14 does not permit, reported from this one place.
    QSet<quintptr> present;
    const auto put = [&](quintptr window, CardOwner observed, const QString &output) {
        present.insert(window);
        if (m_ownership.ownerOf(window) == observed) return;
        if (!m_ownership.transfer(window, observed, output)) {
            reported.push_back({window, OwnershipViolation::Rule::TwoOwners, output});
        }
    };
    for (const auto &session : sessions) {
        for (const auto pane : session.panes) {
            if (pane) put(pane, CardOwner::BentoPane, session.output);
        }
    }
    for (const auto card : cards) {
        if (card) put(card, CardOwner::IndividualCard, QString());
    }
    // A window that left both containers returned to Plasma.
    for (const auto known : m_ownership.windowsOwnedAs(CardOwner::IndividualCard)) {
        if (!present.contains(known)) put(known, CardOwner::Native, QString());
    }
    for (const auto known : m_ownership.windowsOwnedAs(CardOwner::BentoPane)) {
        if (!present.contains(known)) put(known, CardOwner::Native, QString());
    }

    auto violations = m_ownership.reconcile(cards, sessions);
    violations.insert(violations.end(), reported.cbegin(), reported.cend());

    // Report a shape once. The observer runs on every published change, and a
    // standing defect must not bury the next new one in repetition.
    if (violations == m_observedOwnershipViolations) return;
    for (const auto &violation : violations) {
        if (std::find(m_observedOwnershipViolations.cbegin(),
                      m_observedOwnershipViolations.cend(), violation)
            != m_observedOwnershipViolations.cend()) continue;
        qWarning() << "Kadunce card ownership:"
                   << describeOwnershipViolation(violation);
        if (m_ownershipViolationLog.size() < 64)
            m_ownershipViolationLog.append(describeOwnershipViolation(violation));
    }
    m_observedOwnershipViolations = std::move(violations);
}

void Effect::observeCardBook()
{
    m_cardBookSettle.start();
}

void Effect::readCardBook()
{
    if (!m_cardStage || !m_desktopStage || m_releasing || !m_scopedSession) return;
    QList<CardBookDisplay> displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const bool holds = isCardDisplay(output);
        displays.append({output->name(), holds, holds, sizingFor(output) == DisplaySizing::Solo});
    }

    // This desktop's windows, read from every record that names one.
    QHash<quintptr, KWin::EffectWindow *> windows;
    const auto add = [&](KWin::EffectWindow *window) {
        if (window && !window->isDeleted()) windows.insert(reinterpret_cast<quintptr>(window), window);
    };
    QHash<quintptr, QString> paneOf;
    for (const auto &view : m_desktopStage->ownershipView())
        for (const auto pane : view.panes) paneOf.insert(pane, view.output);
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (paneOf.contains(reinterpret_cast<quintptr>(window))) add(window);
    // §11: each display's own stage, by the display it holds cards for.
    QHash<quintptr, QStringList> stagesOf;
    QHash<quintptr, std::vector<quintptr>> behind;
    QHash<quintptr, quintptr> faceOf;
    for (const auto &display : m_scopedSession->displays) {
        const CardStageController &cards = *display.second->cards;
        const QString name = display.first.isEmpty() ? m_cardOutputName : display.first;
        for (const auto &card : cards.liveCards()) {
            if (!card) continue;
            add(card.data());
            stagesOf[reinterpret_cast<quintptr>(card.data())].append(name);
            const auto stack = cards.stackFrontToBack(card);
            if (stack.size() > 1)
                faceOf.insert(reinterpret_cast<quintptr>(card.data()), reinterpret_cast<quintptr>(stack.first()));
        }
        const auto &stacks = cards.paneStackRecords();
        for (auto it = stacks.cbegin(); it != stacks.cend(); ++it) {
            const auto holder = reinterpret_cast<quintptr>(it.key());
            for (const auto &member : it.value()) {
                if (!member || member->isDeleted()) continue;
                add(member.data());
                behind[reinterpret_cast<quintptr>(member.data())].push_back(holder);
            }
        }
    }

    std::vector<CardBookEntry> entries;
    entries.reserve(windows.size());
    for (auto it = windows.cbegin(); it != windows.cend(); ++it) {
        KWin::EffectWindow *window = it.value();
        CardBookEntry entry;
        entry.window = it.key();
        entry.screen = window->screen() ? window->screen()->name() : QString();
        const QStringList held = stagesOf.value(it.key());
        entry.card = !held.isEmpty();
        entry.cardStage = held.isEmpty() ? QString() : held.first();
        entry.cardStages = int(held.size());
        entry.paneOf = paneOf.value(it.key());
        entry.behindPanes = behind.value(it.key());
        entry.stackFace = faceOf.value(it.key());
        entry.minimized = window->isMinimized();
        entries.push_back(std::move(entry));
    }

    auto mismatches = checkCardBook(displays, entries);
    // Reported once, as ownership is: the book is read on every change.
    if (mismatches == m_observedCardBook) return;
    for (const auto &mismatch : mismatches) {
        if (std::find(m_observedCardBook.cbegin(), m_observedCardBook.cend(), mismatch)
            != m_observedCardBook.cend()) continue;
        const auto named = windows.value(mismatch.window);
        QString line = (named ? logNameOf(named)
                          : mismatch.window ? QStringLiteral("a window no record holds") : QStringLiteral("a display")) + QStringLiteral(": ")
            + cardBookRuleName(mismatch.rule);
        if (!mismatch.detail.isEmpty()) line += QStringLiteral(" (") + mismatch.detail + QLatin1Char(')');
        qWarning().noquote() << "Kadunce card book:" << line;
        if (m_cardBookLog.size() < 64) m_cardBookLog.append(line);
    }
    m_observedCardBook = std::move(mismatches);
}

} // namespace Kadunce

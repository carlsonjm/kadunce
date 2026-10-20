/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The on-screen keyboard's room and Kadunce's own keys (docs/INPUT.md).
#include "EffectInternal.h"

namespace Kadunce
{

KWin::LogicalOutput *Effect::tabletOutputForCardStage() const
{
    return tabletOutput();
}

bool Effect::isTabletOutputForCardStage(
    const KWin::LogicalOutput *output) const
{
    return isTabletOutput(output);
}

bool Effect::isManagedWindowForCardStage(
    const KWin::EffectWindow *window) const
{
    return isCardWindow(window);
}

std::optional<double> Effect::inputPanelTopForCardStage(
    KWin::LogicalOutput *output) const
{
    // KWin hands every effect the input panel, so the keyboard is readable
    // without a downstream surface and without the Keyboard telling anyone.
    // What a placement must never do is infer the keyboard from the panel that
    // yielded to it: that reads an occupied region as a free one.
    // Keys raised for nobody are never drawn, so they take no room either;
    // keys on their way out after the compositor hid them still do.
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    if (!output || !m_keysForPerson || !panel || panel->isDeleted()
        || !(panel->isVisible() || panel == m_leavingPanel)) {
        return std::nullopt;
    }
    const KWin::RectF covered =
        KWin::RectF(panel->frameGeometry()).intersected(
            KWin::RectF(output->geometry()));
    // Keys held below the screen's edge leave a sliver of panel a pixel or two
    // tall. The dock steps aside only for keys on screen, and the card makes
    // room only for them too.
    constexpr double KeysOnScreen = 8.0;
    if (covered.isEmpty() || covered.height() < KeysOnScreen) {
        return std::nullopt;
    }
    return covered.top();
}

KWin::RectF Effect::workAreaForCardStage(const KWin::LogicalOutput *output) const
{
    const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    // A panel that gives its room up to the keys lends it to them, not to the
    // cards: a card chosen while the keys are up, or in the moment before the
    // panel is back, stops where the panel stood. One that appears meanwhile
    // still takes its room.
    const auto held = m_keysWorkAreas.constFind(output);
    return held == m_keysWorkAreas.cend() ? work : work.intersected(*held);
}

void Effect::followKeysWorkArea()
{
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if ((method && method->isVisible()) || m_leavingPanel) {
        m_keysWorkAreaRelease.stop();
        // Keys back before a panel returned find the area already held.
        if (m_keysWorkAreas.isEmpty()) {
            for (const KWin::LogicalOutput *output : KWin::effects->screens())
                m_keysWorkAreas.insert(output, KWin::effects->clientArea(KWin::MaximizeArea, output));
        }
        return;
    }
    if (m_keysWorkAreas.isEmpty()) return;
    // The keys have gone. Nothing lent them room, or it is back: let go now.
    // Otherwise wait for the panel, but not for one that never returns.
    for (const KWin::LogicalOutput *output : KWin::effects->screens()) {
        const auto held = m_keysWorkAreas.constFind(output);
        if (held == m_keysWorkAreas.cend()) continue;
        const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
        if (work.intersected(*held) != work) {
            if (!m_keysWorkAreaRelease.isActive()) m_keysWorkAreaRelease.start();
            return;
        }
    }
    releaseKeysWorkArea();
}

void Effect::releaseKeysWorkArea()
{
    m_keysWorkAreaRelease.stop();
    if (m_keysWorkAreas.isEmpty()) return;
    m_keysWorkAreas.clear();
    m_cardStage->followWorkArea(true);
    forEachDisplayStage([](CardStageController &stage) { stage.followWorkArea(true); });
}

bool Effect::keyboardTypesIntoForCardStage(
    const KWin::EffectWindow *window) const
{
    // KWin sends text to one window. A card behind a layout or a launcher is
    // not it, and a dialog floating over its own card is the card's.
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    const KWin::Window *target = method ? method->activeWindow() : nullptr;
    const KWin::Window *client = window ? window->window() : nullptr;
    return target && client && (target == client || client->hasTransient(target, true));
}

void Effect::keyboardHeading(double height, int durationMs)
{
    if (!callerMayDrive()) return;
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    if (!m_keysForPerson || !panel || panel->isDeleted()
        || !(panel->isVisible() || panel == m_leavingPanel)
        || !std::isfinite(height) || height < 0 || durationMs < 0 || durationMs > 2000) {
        return;
    }
    // The keys stand on the bottom edge of their panel, which is where the
    // height is measured from.
    const KWin::RectF keys = panel->frameGeometry();
    m_cardStage->keyboardHeading(keys.y() + keys.height() - height, durationMs);
    forEachDisplayStage([&](CardStageController &stage) {
        stage.keyboardHeading(keys.y() + keys.height() - height, durationMs);
    });
}

void Effect::raiseKeyboard()
{
    if (!callerMayDrive()) return;
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method) return;
    m_keyboardAskedSince.start();
    // Plasma's touch-only setting shows the keys only while a touch or a pen
    // was the last input, so a click on the tray's entry asked for nothing.
    // A request here is the person's own however it was made, so it counts
    // as a touch; KWin's next input of any kind sets it back.
    if (KWin::input()) KWin::input()->setLastInputHandler(KWin::input()->touch());
    method->forceActivate();
}

void Effect::tapEndedForInput(const KWin::Window *window)
{
    if (!window || !m_inputRouter) return;
    // An application that asks for the keys itself does so as the finger
    // lifts; this waits long enough for it to have done so.
    constexpr int OwnAskMs = 250;
    QTimer::singleShot(OwnAskMs, this,
                       [this, tapped = QPointer<KWin::Window>(const_cast<KWin::Window *>(window)),
                        events = m_inputRouter->touchEvents()] { raiseKeysForTerminalTap(tapped, events); });
}

void Effect::raiseKeysForTerminalTap(QPointer<KWin::Window> window, quint64 touchEvents)
{
    // Another touch since belongs to whatever that touch does.
    if (!window || !m_inputRouter || m_inputRouter->touchEvents() != touchEvents
        || m_inputRouter->latestTouchKept()) return;
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isEnabled() || method->isVisible()) return;
    KWin::TextInputV3Interface *input = KWin::waylandServer()->seat()->textInputV3();
    if (!input || !input->isEnabled()
        || input->contentPurpose() != KWin::TextInputContentPurpose::Terminal) return;
    const KWin::Window *target = method->activeWindow();
    if (!target || (target != window.data() && !window->hasTransient(target, true))) return;
    raiseKeyboard();
}

const char *Effect::keyboardRefusal(const KWin::InputMethod &method) const
{
    // A request through raiseKeyboard is the person's own, from a swipe up
    // from the bottom bezel or the precision surface. It is honoured for the
    // moment the raise takes.
    constexpr qint64 AskedMs = 1500;
    if (m_keyboardAskedSince.isValid() && m_keyboardAskedSince.elapsed() < AskedMs)
        return nullptr;
    // Otherwise the compositor raised the keys because a field was enabled
    // soon after a touch, which is also true when the application focused
    // the field itself after a touch somewhere else in it. A tap on a field
    // puts the text cursor on the line the finger landed on; an application's
    // own focus leaves it wherever the field is. Across the line the cursor
    // lands at the end of the text, far from the finger, so only the line is
    // read.
    KWin::TouchInputRedirection *touch = KWin::input() ? KWin::input()->touch() : nullptr;
    const KWin::Window *target = method.activeWindow();
    if (!target) return "no window holds the text";
    if (!touch || KWin::input()->lastInputHandler() != touch)
        return "the last input was not a touch";
    // A touch Kadunce kept, such as the tap that chose a card in Spread,
    // reached no application, so wherever it landed it was not on the text.
    if (m_inputRouter && m_inputRouter->latestTouchKept())
        return "the last touch was Kadunce's own";
    // Keys a tap brought have answered it once they are put away; with no
    // touch since, the application took the focus back on its own.
    if (m_inputRouter && m_inputRouter->touchEvents() == m_answeredTouchEvents)
        return "keys put away since answered the last touch";
    const QPointF finger = touch->position();
    // Typing on the keys is asking for them.
    if (const KWin::EffectWindow *panel = KWin::effects->inputPanel();
        panel && panel->frameGeometry().contains(finger)) return nullptr;
    if (!target->frameGeometry().contains(finger)) return "the last touch was outside its window";
    // A field that asks within a moment of a tap in its own window, made while
    // the keys were up, is the one tapped. The cursor a browser reports then
    // cannot be read: moving into a box in a frame from another site, it gives
    // the last box's, or this one's from before the window last changed size.
    // With the keys down the tap may have been on a link or a list, and a
    // page that opens focuses its own box; only the cursor can tell.
    constexpr qint64 TapJustNowMs = 1000;
    if (m_keysUpAtTouch && m_inputRouter && m_inputRouter->msSinceLatestTouch() < TapJustNowMs
        && m_inputRouter->latestTouchWindow() == target)
        return nullptr;
    const KWin::RectF cursor = method.cursorRectangle();
    // A client that never says where its cursor is cannot be read; the touch
    // landing in its window is all there is to go on.
    if (cursor.height() <= 0.0) return nullptr;
    const double reach = cursor.height() / 2.0;
    if (finger.y() >= cursor.top() - reach && finger.y() <= cursor.bottom() + reach) return nullptr;
    return "the last touch was off the line of its text";
}

void Effect::keepUnaskedKeyboardDown()
{
    // Decided once for each time the keys come up: a client that enables its
    // field again with every keystroke must not have each one read afresh.
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isVisible()) {
        // Hidden with its picture still there, as an application asking for
        // the keys to go does: drawn on while the keyboard takes them out.
        KWin::EffectWindow *panel = KWin::effects->inputPanel();
        KWin::Window *surfaceWindow = method ? method->panel() : nullptr;
        KWin::SurfaceInterface *surface = surfaceWindow ? surfaceWindow->surface() : nullptr;
        if (m_keysForPerson && panel && !panel->isDeleted() && surface && surface->isMapped()) {
            if (!m_leavingKeys) {
                answerTouches();
                m_leavingKeys = std::make_unique<KWin::EffectWindowVisibleRef>(
                    panel, KWin::EffectWindow::PAINT_DISABLED);
                m_leavingPanel = panel;
                m_leavingKeysUnmap = connect(surface, &KWin::SurfaceInterface::unmapped,
                                             this, &Effect::releaseLeavingKeys);
                m_leavingKeysLimit.start();
            }
            return;
        }
        if (m_keysForPerson && !m_leavingKeys) answerTouches();
        releaseLeavingKeys();
        m_keysForPerson = false;
        return;
    }
    // Back before they were out: the same keys, still the person's.
    if (m_leavingKeys) {
        disconnect(m_leavingKeysUnmap);
        m_leavingKeysLimit.stop();
        m_leavingKeys.reset();
        m_leavingPanel.clear();
    }
    if (m_keysForPerson) return;
    const char *refusal = keyboardRefusal(*method);
    if (!refusal) {
        m_keysForPerson = true;
        if (KWin::EffectWindow *panel = KWin::effects->inputPanel())
            KWin::effects->addRepaint(panel->expandedGeometry().toAlignedRect());
        return;
    }
    method->hide();
    const KWin::Window *target = method->activeWindow();
    qInfo().nospace() << "Kadunce kept the keyboard down for "
                      << (target ? logNameOf(target) : QStringLiteral("no window"))
                      << ": " << refusal;
}

void Effect::answerTouches()
{
    // A touch that ended just before the keys began to go may be what sent
    // them: a tap on another field, which a browser focuses only once the
    // finger has lifted and the field before has let go. That field may still
    // ask, so such a touch is left to ask.
    constexpr qint64 SentAwayMs = 1000;
    if (m_inputRouter && m_inputRouter->msSinceLatestTouch() >= SentAwayMs)
        m_answeredTouchEvents = m_inputRouter->touchEvents();
}

void Effect::releaseLeavingKeys()
{
    disconnect(m_leavingKeysUnmap);
    m_leavingKeysLimit.stop();
    if (!m_leavingKeys) return;
    if (m_leavingPanel && !m_leavingPanel->isDeleted())
        KWin::effects->addRepaint(m_leavingPanel->expandedGeometry().toAlignedRect());
    m_leavingKeys.reset();
    m_leavingPanel.clear();
    KWin::InputMethod *method = KWin::kwinApp()->inputMethod();
    if (!method || !method->isVisible()) m_keysForPerson = false;
    m_cardStage->refreshKeyboardRoom();
    forEachDisplayStage([](CardStageController &stage) { stage.refreshKeyboardRoom(); });
    followKeysWorkArea();
}

void Effect::setPagingShortcutsActive(bool active)
{
    // Each display's cards answer the keys while that display shows them.
    m_cardsShownForKeys.insert(m_scopedKey, active);
}

QString Effect::keyStageKey() const
{
    // §11: the keys act on one display. While a Spread is open, the one on
    // the display in use, else any open; otherwise the focused window's
    // display, then the active one, then the primary display.
    if (!m_cardsOnEveryDisplay) return QString();
    QString open;
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
            && (open.isEmpty() || m_scopedKey == m_inUseKey)) open = m_scopedKey;
    });
    if (!open.isEmpty()) return open;
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    if (focused && isCardDisplay(focused->screen())) return stageKeyOf(focused);
    KWin::LogicalOutput *active = KWin::effects->activeScreen();
    return isCardDisplay(active) ? active->name() : m_cardOutputName;
}

bool Effect::answerKey(int key, Qt::KeyboardModifiers modifiers, bool repeat)
{
    const QString stage = keyStageKey();
    SessionScope scope(this, m_scopedSession, stage);
    KeyContext context;
    context.table = true;
    context.cardsShown = m_cardsShownForKeys.value(m_scopedKey);
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    // The card a display sized Solo shows, other than the primary display's,
    // answers the keys as a pane: Meta and the arrows reach the display beside.
    context.pane = focused && (m_desktopStage->managesWindow(focused)
        || (context.cardsShown && m_cardsOnEveryDisplay && !connectedForCardStage()
            && tabletOutput() != primaryOutput()
            && m_cardStage->presentation() == CardPresentation::Active
            && m_cardStage->activeCardIdentity() == focused));
    // So does the Active card of any display sized Solo with cards on every
    // display: Meta and Shift's arrows send it, with its Stack, to the one
    // beside (carryToward), the primary display's included.
    context.lone = context.cardsShown && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->activeCardIdentity()
        && (connectedForCardStage()
            || (m_cardsOnEveryDisplay && m_cardStage->activeCardIdentity() == focused));
    context.stacked = context.cardsShown && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->stackFrontToBack(m_cardStage->activeCardIdentity()).size() > 1;
    // Plain keys go to an effect holding the keyboard, as Table does while open.
    context.spreadShown = context.cardsShown && !KWin::effects->hasKeyboardGrab()
        && m_cardStage->presentation() == CardPresentation::Spread
        && !m_cardStage->launcherGuestActive() && !m_cardStage->cardGrabActive();
    const KeyAction action = keyActionFor(key, modifiers, context);
    if (action == KeyAction::None || (repeat && !keyActionRepeats(action))) return false;
    // Open, Table keeps every key but its own.
    if (m_table.isOpen() && action != KeyAction::Table) return false;
    // A ladder shown by the keys answers only Shift's Up and Down; any other
    // key of Kadunce's places the card first.
    if (m_stackScrubByKeys && action != KeyAction::Deeper && action != KeyAction::LetGo)
        QTimer::singleShot(0, this, &Effect::commitKeyLadder);
    // The action runs after the key's own handling, never inside an input
    // filter's callback.
    noteDisplayInUse(screenNamed(m_scopedKey));
    onThisStageLater(0, [this, action]() { runKeyAction(action); });
    return true;
}

void Effect::runKeyAction(KeyAction action)
{
    switch (action) {
    case KeyAction::Spread: toggle(); break;
    case KeyAction::Previous: pageLeft(); break;
    case KeyAction::Next: pageRight(); break;
    case KeyAction::StackPrevious: pageStackUp(); break;
    case KeyAction::StackNext: pageStackDown(); break;
    case KeyAction::Bento: toggleBento(); break;
    case KeyAction::ZoneMode: switchZoneMode(); break;
    case KeyAction::Release: release(); break;
    case KeyAction::Open: activateSelectedFromInput(); break;
    case KeyAction::Back:
        if (m_cardStage->backFromSpread()) activateSelectedFromInput();
        break;
    case KeyAction::Table: toggleTable(); break;
    case KeyAction::FocusLeft: focusToward(KeyDirection::Left); break;
    case KeyAction::FocusRight: focusToward(KeyDirection::Right); break;
    case KeyAction::FocusUp: focusToward(KeyDirection::Up); break;
    case KeyAction::FocusDown: focusToward(KeyDirection::Down); break;
    case KeyAction::CarryLeft: carryToward(KeyDirection::Left); break;
    case KeyAction::CarryRight: carryToward(KeyDirection::Right); break;
    case KeyAction::Deeper: stepKeyLadder(1); break;
    case KeyAction::LetGo:
        if (m_stackScrubByKeys) stepKeyLadder(-1);
        else if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
            (void)m_cardStage->pullOutSelected();
        else if (KWin::EffectWindow *focused = KWin::effects->activeWindow();
                 focused && m_desktopStage->managesWindow(focused))
            letPaneGo();
        else letActiveCardGo();
        break;
    case KeyAction::DividerLeft: moveDividerByKey(-1); break;
    case KeyAction::DividerRight: moveDividerByKey(1); break;
    case KeyAction::Hold:
    case KeyAction::None: break;
    }
}

QList<KWin::EffectWindow *> Effect::keyTargets() const
{
    QList<KWin::EffectWindow *> targets;
    for (auto *output : KWin::effects->screens()) {
        if (!output) continue;
        for (const auto &pane : m_desktopStage->panesOn(output->name()))
            if (pane && !pane->isDeleted() && !pane->isMinimized() && !targets.contains(pane.data()))
                targets.append(pane.data());
    }
    // The Active card each display shows.
    eachCardDisplay([&](KWin::LogicalOutput *) {
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Active)
            if (KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
                card && !card->isMinimized() && !targets.contains(card))
                targets.append(card);
    });
    return targets;
}

KWin::LogicalOutput *Effect::outputToward(const KWin::LogicalOutput *output, KeyDirection direction) const
{
    if (!output) return nullptr;
    QList<KWin::LogicalOutput *> others;
    QList<QRectF> rects;
    for (auto *other : KWin::effects->screens()) {
        if (!other || other == output) continue;
        others.append(other);
        rects.append(QRectF(other->geometry()));
    }
    const int to = keyNeighbour(QRectF(output->geometry()), rects, direction);
    return to < 0 ? nullptr : others.at(to);
}

KWin::EffectWindow *Effect::paneUsedBefore(const KWin::EffectWindow *pane) const
{
    if (!pane || !pane->screen()) return nullptr;
    KWin::EffectWindow *before = nullptr;
    quint64 latest = 0;
    for (const auto &other : m_desktopStage->panesOn(pane->screen()->name())) {
        if (!other || other.data() == pane || other->isDeleted()) continue;
        const quint64 at = m_activationOrder.value(windowIdentity(other.data()), 0);
        if (!before || at > latest) {
            before = other.data();
            latest = at;
        }
    }
    return before;
}

void Effect::focusToward(KeyDirection direction)
{
    KWin::EffectWindow *from = KWin::effects->activeWindow();
    if (!from) return;
    QList<KWin::EffectWindow *> targets = keyTargets();
    targets.removeAll(from);
    QList<QRectF> rects;
    for (auto *target : std::as_const(targets)) rects.append(QRectF(target->frameGeometry()));
    const int to = keyNeighbour(QRectF(from->frameGeometry()), rects, direction);
    if (to < 0) return;
    qInfo() << "Kadunce" << Revision << "moved the focus from" << logNameOf(from)
            << "to" << logNameOf(targets.at(to));
    KWin::effects->activateWindow(targets.at(to));
}

void Effect::carryToward(KeyDirection direction)
{
    const bool right = direction == KeyDirection::Right;
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    SessionScope scope(this, m_scopedSession, stageKeyOf(focused));
    // §11, Every display: the Active card of a display sized Solo has no room
    // beside it, so it goes on to the display beside with its Stack, and is
    // shown there: in the layout there, or as its Active card.
    if (focused && m_cardsOnEveryDisplay && m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active
        && m_cardStage->activeCardIdentity() == focused && !connectedForCardStage()) {
        KWin::LogicalOutput *next = focused->screen() ? outputToward(focused->screen(), direction) : nullptr;
        if (!next || !isCardDisplay(next)) return;
        qInfo() << "Kadunce" << Revision << "sends" << logNameOf(focused) << "to" << next->name();
        if (!moveCardToDisplay(focused, next, true))
            qInfo() << "Kadunce" << Revision << "could not send" << logNameOf(focused) << "to" << next->name();
        return;
    }
    if (focused && m_desktopStage->managesWindow(focused) && focused->window() && focused->screen()) {
        KWin::LogicalOutput *output = focused->screen();
        // A pane beside it on its display trades places with it.
        QList<KWin::EffectWindow *> beside;
        QList<QRectF> rects;
        for (const auto &other : m_desktopStage->panesOn(output->name())) {
            if (!other || other.data() == focused || other->isDeleted()) continue;
            beside.append(other.data());
            rects.append(QRectF(other->frameGeometry()));
        }
        const int to = keyNeighbour(QRectF(focused->frameGeometry()), rects, direction);
        if (to >= 0) {
            if (m_desktopStage->swapPanes(focused, beside.at(to))) {
                qInfo() << "Kadunce" << Revision << "swapped" << logNameOf(focused)
                        << "with" << logNameOf(beside.at(to)) << "by the keys";
                observeCardOwnership();
                KWin::effects->activateWindow(focused);
            } else {
                qInfo() << "Kadunce" << Revision << "found" << logNameOf(focused)
                        << "could not trade places with" << logNameOf(beside.at(to));
            }
            return;
        }
        // With open room toward that edge, it moves there, as a side snap
        // would place it.
        if (connectedOutputForDesktopStage(output)
            && keyRoomToward(QRectF(focused->frameGeometry()), QRectF(output->geometry()), direction)
            && m_desktopStage->placePaneBySide(focused, output, BentoSidePlacement{right, true, true})) {
            qInfo() << "Kadunce" << Revision << "placed" << logNameOf(focused)
                    << (right ? "right" : "left") << "by the keys";
            observeCardOwnership();
            return;
        }
        // At the display's edge it goes on to the display beside, as KWin's
        // own window-to-screen key sends it (handleWindowOutputChanged).
        if (KWin::LogicalOutput *next = outputToward(output, direction)) {
            qInfo() << "Kadunce" << Revision << "sends" << logNameOf(focused) << "to" << next->name();
            // The pane it leaves on Spread's display is its again when it
            // comes back.
            if (isTabletOutput(output)) {
                LeftLayout left;
                left.right = QRectF(focused->frameGeometry()).center().x()
                    > QRectF(output->geometry()).center().x();
                for (const auto &other : m_desktopStage->panesOn(output->name()))
                    if (other && other.data() != focused) left.partners.append(other);
                m_leftLayouts.insert(focused, left);
            }
            focused->window()->sendToOutput(next);
        }
        return;
    }
    // The Active card alone on a Connected display: a full card takes the
    // half toward that side, a half moves into the open half, and at the edge
    // it goes on to the display beside.
    KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
    if (!card || !card->window() || !card->screen() || !connectedForCardStage()) {
        qInfo() << "Kadunce" << Revision << "found nothing to carry; the focus is on"
                << (focused ? logNameOf(focused) : QStringLiteral("nothing"));
        return;
    }
    KWin::LogicalOutput *output = card->screen();
    const QRectF frame(card->frameGeometry());
    const QRectF display(output->geometry());
    const bool wide = frame.width() > display.width() * 0.6;
    if ((wide || keyRoomToward(frame, display, direction))
        && placeActiveCardAlone(output, BentoSidePlacement{right, true, true})) {
        qInfo() << "Kadunce" << Revision << "placed" << logNameOf(card)
                << (right ? "right" : "left") << "by the keys";
        observeCardOwnership();
        return;
    }
    if (KWin::LogicalOutput *next = outputToward(output, direction)) {
        qInfo() << "Kadunce" << Revision << "sends" << logNameOf(card) << "to" << next->name();
        card->window()->sendToOutput(next);
    }
}

void Effect::stepKeyLadder(int delta)
{
    // A ladder a hand is holding is the hand's.
    if (m_stackScrub.active() && !m_stackScrubByKeys) return;
    if (!m_stackScrub.active()) {
        KWin::EffectWindow *pane = KWin::effects->activeWindow();
        if (!pane || !m_desktopStage->managesWindow(pane) || delta <= 0) return;
        if (m_cardStage->paneStack(pane).isEmpty()) {
            // With no Stack it joins the one of the pane used before it, as a
            // pane rested on another does.
            if (KWin::EffectWindow *onto = paneUsedBefore(pane); onto && stackPaneOnto(pane, onto))
                qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(pane) << "by the keys";
            return;
        }
        m_stackScrubPane = pane;
        m_stackScrubByKeys = true;
        m_stackScrub.begin(int(m_cardStage->paneStack(pane).size()) + 1, 0, 0.0);
        qInfo() << "Kadunce" << Revision << "shows the ladder of" << logNameOf(pane) << "by the keys";
    }
    m_stackScrub.stepKey(delta);
    refreshStackScrubShown();
    KWin::effects->addRepaintFull();
}

void Effect::commitKeyLadder()
{
    if (!m_stackScrubByKeys) return;
    const QPointer<KWin::EffectWindow> pane = m_stackScrub.active() ? m_stackScrubPane : nullptr;
    const int place = m_stackScrub.place;
    // A Stack that changed under the ladder has nothing to order.
    const bool whole = pane && int(m_cardStage->paneStack(pane).size()) + 1 == m_stackScrub.size;
    endStackScrub();
    if (whole && place > 0) (void)reorderPaneStack(pane, place);
}

void Effect::keyReleased(int key)
{
    // Queued behind the actions of the keys pressed before it, so a ladder
    // those keys begin is placed even when Meta is let go at once.
    if (key == Qt::Key_Meta || key == Qt::Key_Super_L || key == Qt::Key_Super_R)
        QTimer::singleShot(0, this, &Effect::commitKeyLadder);
}

void Effect::letPaneGo()
{
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane)) return;
    // Its Stack is kept by the stage of the display it stands on.
    SessionScope scope(this, m_scopedSession, stageKeyOf(pane));
    const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(pane);
    if (!behind.isEmpty()) {
        // Out of its Stack: the next card takes the pane, and this one waits
        // in Spread as a card of its own, as one pulled down there does.
        KWin::EffectWindow *next = behind.first();
        m_cardStage->forgetPaneStack(pane);
        m_cardStage->notePaneStack(next, behind.mid(1));
        if (!m_desktopStage->replacePaneWithCard(pane, next,
                [this, next] {
                    return m_cardStage->releaseCardToLiveBento(next);
                })) {
            m_cardStage->forgetPaneStack(next);
            m_cardStage->notePaneStack(pane, behind);
            return;
        }
        m_cardStage->settlePaneStacks();
        observeCardOwnership();
        qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of its Stack by the keys";
        // The card let go joins the layout as a pane of its own beside the
        // Stack it left, as a card called while the layout shows does; with
        // no room it waits in its display's Spread, as one pulled down does.
        const QPointer<KWin::EffectWindow> released = pane;
        const QPointer<KWin::EffectWindow> stayed = next;
        const QString key = stageKeyOf(stayed);
        QTimer::singleShot(0, this, [this, released, stayed, key] {
            SessionScope scope(this, m_scopedSession, key);
            const bool joined = released && !released->isDeleted()
                && m_cardStage->liveCardIndex(released) >= 0
                && m_desktopStage->admitCardToLiveBento(released,
                    [this, released] { return m_cardStage->releaseCardToLiveBento(released); });
            if (joined) {
                qInfo() << "Kadunce" << Revision << "gave" << logNameOf(released)
                        << "a pane of its own by the keys";
                observeCardOwnership();
                KWin::effects->activateWindow(released);
            } else if (stayed && !stayed->isDeleted()) {
                KWin::effects->activateWindow(stayed);
            }
            KWin::effects->addRepaintFull();
        });
        return;
    }
    // Out of the layout: it waits in Spread as a card, and the pane used
    // last has the keys.
    const QString outputName = pane->screen() ? pane->screen()->name() : QString();
    const QPointer<KWin::EffectWindow> released = pane;
    const bool left = m_desktopStage->extractPaneToCards(pane, [] { return true; }, true);
    if (!left) return;
    qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of its layout by the keys";
    observeCardOwnership();
    KWin::EffectWindow *latest = nullptr;
    for (const auto &standing : m_desktopStage->panesOn(outputName)) {
        if (!standing || standing == released || standing->isDeleted() || standing->isMinimized())
            continue;
        if (!latest || lastUsedForDesktopStage(standing) > lastUsedForDesktopStage(latest))
            latest = standing.data();
    }
    if (latest && KWin::effects->activeWindow() == released) KWin::effects->activateWindow(latest);
}

void Effect::letActiveCardGo()
{
    KWin::EffectWindow *card = m_cardStage->activeCardIdentity();
    QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(card);
    if (!card || stack.size() < 2 || !m_cardStage->letActiveCardOutOfStack()) return;
    stack.removeAll(card);
    const QPointer<KWin::EffectWindow> released = card;
    const QPointer<KWin::EffectWindow> next = stack.first();
    qInfo() << "Kadunce" << Revision << "took the Active card" << logNameOf(card)
            << "out of its Stack by the keys";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    // Solo, it stays the card shown, and the Stack it left waits in Spread.
    KWin::LogicalOutput *output = card->screen();
    if (!output || !connectedOutputForDesktopStage(output)) return;
    // §11, Connected: the two are laid out side by side, the rest of the
    // Stack behind the card it left, and the layout fills as Meta+B fills it.
    if (m_desktopStage->hasSessionOnOutput(output->name())) {
        toggleBento();
        return;
    }
    if (!next || !pairCardsIntoBento(output, released, next, BentoSidePlacement{false, true})) {
        qInfo() << "Kadunce" << Revision << "could not lay" << logNameOf(released)
                << "out beside the Stack it left on" << output->name();
        return;
    }
    qInfo() << "Kadunce" << Revision << "laid" << logNameOf(released) << "out beside"
            << logNameOf(next) << "on" << output->name();
    observeCardOwnership();
    onThisStageLater(0, [this, output = QPointer(output)] { fillLayoutWithCards(output); });
}

void Effect::moveDividerByKey(int delta)
{
    KWin::EffectWindow *pane = KWin::effects->activeWindow();
    if (!pane || !m_desktopStage->managesWindow(pane) || !pane->screen()) return;
    const QRectF frame(pane->frameGeometry());
    const QString output = pane->screen()->name();
    // The divider at the pane's right edge, else at its left.
    std::optional<DesktopStageController::GrabRail> chosen;
    double best = 0.0;
    for (const auto &rail : m_desktopStage->grabRails()) {
        if (rail.output != output || !rail.vertical) continue;
        if (rail.hitArea.bottom() < frame.top() || rail.hitArea.top() > frame.bottom()) continue;
        const double x = rail.pill.center().x();
        const double distance = std::min(std::abs(x - frame.right()), std::abs(x - frame.left()) + 1.0);
        if (distance > 40.0 || (chosen && distance >= best)) continue;
        chosen = rail;
        best = distance;
    }
    if (!chosen) return;
    const QPointF start = chosen->hitArea.center();
    if (!m_desktopStage->beginRail(start)) return;
    m_desktopStage->updateRail(start + QPointF(pane->screen()->geometry().width() * 0.05 * delta, 0.0));
    m_desktopStage->finishRail(true);
    qInfo() << "Kadunce" << Revision << "moved a divider of" << logNameOf(pane) << "by the keys";
}

void Effect::activateSelectedFromInput()
{
    // Defer the state change so the input filter is never destroyed from
    // inside one of its own callbacks.
    const auto ticket = m_inputActivationGuard.issue();
    const QPointer<KWin::EffectWindow> requested = selectedWindow();
    onThisStageLater(0, [this, ticket, requested]() {
        if (m_inputActivationGuard.accepts(ticket)
            && requested && !requested->isDeleted() && selectedWindow() == requested
            && !nativeWindowInteractionForInput() && !m_cardStage->cardGrabActive()
            && !m_cardStage->launcherGuestActive() && m_cardStage->isActive()
            && m_cardStage->presentation() == CardPresentation::Spread) {
            // CARD-LIFECYCLE.md section 6: choosing a pane's card resumes its
            // layout, on that pane. A refused resume is not an invitation to
            // try the individual-card path on it, which cannot admit a pane
            // and leaves the selection doing nothing at all.
            if (m_cardStage->selectedIsBentoGroup()) {
                if (!m_cardStage->resumeSelectedBentoProjection()) {
                    qWarning() << "Kadunce" << Revision
                               << "declined to resume the selected Bento layout";
                } else if (m_desktopStage->managesWindow(requested)
                           && KWin::effects->activeWindow() != requested) {
                    KWin::effects->activateWindow(requested);
                }
                observeCardOwnership();
            // §10: a card the top edge left alone is picked alone, as it left.
            } else if (!requested->isMinimized() && !m_aloneOnPurpose.contains(requested)
                       && playPickedCardIntoLayout(requested)) {
                observeCardOwnership();
            } else if (requested->isMinimized()) {
                // §2: selecting a sleeping card wakes it and presents it as
                // Active, as picking it from the dock does.
                KWin::effects->activateWindow(requested);
            } else {
                toggleOwnedPresentation(true);
            }
        }
    });
}

} // namespace Kadunce

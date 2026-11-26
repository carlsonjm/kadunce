/* SPDX-License-Identifier: GPL-2.0-or-later */

// INPUT.md: Meta+Shift+S stacks the card or pane in use onto the one used
// before it, and pressed again puts it one deeper; Meta+Shift+PgDown and
// PgUp move a Stack's front card one place back or forward again; and
// Meta+Shift+Backspace gives the card or pane in use back to the Plasma desktop,
// as the bottom edge does.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// The order a Stack would have with `card` moved `delta` places toward the back.
QList<KWin::EffectWindow *> movedInStack(QList<KWin::EffectWindow *> order, KWin::EffectWindow *card, int delta)
{
    const int at = int(order.indexOf(card));
    const int to = std::clamp(at + delta, 0, int(order.size()) - 1);
    if (at < 0 || to == at) return order;
    order.move(at, to);
    return order;
}
}

void Effect::stackByKeys()
{
    // A pane: the ladder, as Meta+Shift+Up was (stepKeyLadder).
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    if (m_stackScrubByKeys || (focused && m_desktopStage->managesWindow(focused))) {
        stepKeyLadder(1);
        return;
    }
    // A card, the centred one in Spread or the Active one.
    KWin::EffectWindow *card = m_cardStage->presentation() == CardPresentation::Spread
        ? m_cardStage->selectedWindow() : m_cardStage->activeCardIdentity();
    if (!card || m_cardStage->liveCardIndex(card) < 0) return;
    const QList<KWin::EffectWindow *> stack = m_cardStage->stackFrontToBack(card);
    if (stack.size() > 1) {
        // In a Stack already: one deeper, as the ladder goes.
        m_movingInStack = card;
        moveInStackByKeys(1);
        return;
    }
    // In none: in front of the Stack of the card used just before it.
    KWin::EffectWindow *before = nullptr;
    quint64 latest = 0;
    for (const auto &other : m_cardStage->liveCards()) {
        if (!other || other == card || other->isDeleted() || other->screen() != card->screen()) continue;
        const QList<KWin::EffectWindow *> its = m_cardStage->stackFrontToBack(other);
        if (!its.isEmpty() && its.first() != other) continue;
        const quint64 at = m_activationOrder.value(windowIdentity(other.data()), 0);
        if (!before || at > latest) {
            before = other.data();
            latest = at;
        }
    }
    if (!before) return;
    QList<KWin::EffectWindow *> under = m_cardStage->stackFrontToBack(before);
    if (under.isEmpty()) under.append(before);
    if (m_cardStage->regroup(card, under)) {
        m_movingInStack = card;
        qInfo() << "Kadunce" << Revision << "stacked" << logNameOf(card) << "on" << logNameOf(before) << "by the keys";
        observeCardOwnership();
        Q_EMIT workspaceContextChanged();
        KWin::effects->addRepaintFull();
    }
}

void Effect::moveInStackByKeys(int delta)
{
    // The card moved stays the one moved while it is in that Stack; else the
    // Stack's front card is the one.
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    const bool pane = focused && m_desktopStage->managesWindow(focused);
    if (pane) {
        SessionScope scope(this, m_scopedSession, stageKeyOf(focused));
        QList<KWin::EffectWindow *> order{focused};
        order.append(m_cardStage->paneStack(focused));
        if (order.size() < 2) return;
        KWin::EffectWindow *card = order.contains(m_movingInStack.data()) ? m_movingInStack.data() : focused;
        const QList<KWin::EffectWindow *> moved = movedInStack(order, card, delta);
        if (moved == order) return;
        m_movingInStack = card;
        KWin::EffectWindow *face = moved.first();
        m_cardStage->forgetPaneStack(focused);
        m_cardStage->notePaneStack(face, moved.mid(1));
        if (face != focused
            && !m_desktopStage->replacePaneWithCard(focused, face,
                   [this, face] { return m_cardStage->releaseCardToLiveBento(face); })) {
            m_cardStage->forgetPaneStack(face);
            m_cardStage->notePaneStack(focused, order.mid(1));
            return;
        }
        m_cardStage->settlePaneStacks();
        if (face != focused) KWin::effects->activateWindow(face);
        qInfo() << "Kadunce" << Revision << "moved" << logNameOf(card) << "to place" << moved.indexOf(card) + 1
                << "of its pane's Stack by the keys";
    } else {
        KWin::EffectWindow *front = m_cardStage->presentation() == CardPresentation::Spread
            ? m_cardStage->selectedWindow() : m_cardStage->activeCardIdentity();
        const QList<KWin::EffectWindow *> order = front ? m_cardStage->stackFrontToBack(front) : QList<KWin::EffectWindow *>();
        if (order.size() < 2) return;
        KWin::EffectWindow *card = order.contains(m_movingInStack.data()) ? m_movingInStack.data() : order.first();
        const QList<KWin::EffectWindow *> moved = movedInStack(order, card, delta);
        if (moved == order || !m_cardStage->regroup(moved.first(), moved.mid(1))) return;
        m_movingInStack = card;
        qInfo() << "Kadunce" << Revision << "moved" << logNameOf(card) << "to place" << moved.indexOf(card) + 1
                << "of its Stack by the keys";
    }
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    KWin::effects->addRepaintFull();
}

void Effect::sendFocusedToDesktop()
{
    // As the bottom edge does: a pane leaves its layout for the place it had
    // before it, a card goes back to its own size, and the desktop is shown
    // with it on the display that showed cards.
    KWin::EffectWindow *window = KWin::effects->activeWindow();
    KWin::LogicalOutput *output = window ? window->screen() : nullptr;
    if (!window || !output) return;
    const QPointer<KWin::EffectWindow> released = window;
    if (m_desktopStage->managesWindow(window)) {
        SessionScope scope(this, m_scopedSession, stageKeyOf(window));
        m_desktopStage->releaseWindow(window);
        if (released && m_currentSession)
            for (const auto &display : m_currentSession->displays) display.second->cards->paneLeftWithoutCard(released);
        if (isTabletOutput(output))
            QTimer::singleShot(0, this, [this, output = QPointer(output), released] {
                if (output && released) showDesktopWithReturned(output, released);
            });
    } else if (CardStageController *stage = stageHolding(window); stage && stage->liveCardIndex(window) >= 0) {
        const bool active = stage->presentation() == CardPresentation::Active && stage->activeCardIdentity() == window;
        if (!stage->releaseCard(window, true)) return;
        if (active && released) stage->returnWindowToDesktop(released);
    } else {
        return;
    }
    qInfo() << "Kadunce" << Revision << "gave" << logNameOf(window) << "back to the desktop by the keys";
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

} // namespace Kadunce

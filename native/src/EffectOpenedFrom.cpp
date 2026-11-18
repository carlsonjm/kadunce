/* SPDX-License-Identifier: GPL-2.0-or-later */

// A window opened from a card joins that card's Stack, in front, where the
// person has switched that on (OpenedFrom.h, CARD-LIFECYCLE.md §9).
#include "EffectInternal.h"
#include "OpenedFromWatch.h"

namespace Kadunce
{

bool Effect::stacksOpenedWindows() const
{
    return m_openedFrom && m_openedFrom->joins;
}

void Effect::setStacksOpenedWindows(bool stacks)
{
    if (!m_openedFrom || stacks == m_openedFrom->joins) return;
    m_openedFrom->joins = stacks;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Stacks"));
    group.writeEntry(QStringLiteral("JoinOpener"), stacks);
    group.sync();
    qInfo() << "Kadunce" << Revision << "window origin: a window opened from a card"
            << (stacks ? "joins that card's Stack from now on" : "opens on its own from now on");
}

bool Effect::joinStackOpenedFrom(KWin::EffectWindow *opened, bool late)
{
    KWin::EffectWindow *opener = m_openedFrom ? m_openedFrom->openerOf(opened) : nullptr;
    if (!opener || !opened || opened->isDeleted()) return false;
    // Each window is asked once it has a place; whatever the answer, it is
    // not asked again.
    const auto leaves = [this, opened, opener](const char *why) {
        qInfo() << "Kadunce" << Revision << "window origin: leaves" << logNameOf(opened)
                << "out of the Stack of" << logNameOf(opener) << "because" << why;
        m_openedFrom->forget(opened);
        return false;
    };
    if (!m_openedFrom->joins) return leaves("joining is switched off");
    if (opener == opened || opener->isMinimized() || !isCardWindow(opened))
        return leaves("one of the two is no longer a card shown");
    // A second window of an application is that application's own business.
    if (applicationIdentity(opened) == applicationIdentity(opener)
        || (opened->window() && opener->window() && opened->window()->pid() == opener->window()->pid()))
        return leaves("both belong to one application");
    if (opened->screen() != opener->screen()) return leaves("it opened on another display");
    if (m_carriedWindow || m_nativeCarry || m_releasing) return leaves("a window is being carried");

    SessionScope scope(this, m_scopedSession, stageKeyOf(opener));
    const auto placeOf = [this](KWin::EffectWindow *window) {
        return m_desktopStage->managesWindow(window) ? OpenedFrom::Place::Pane
            : m_cardStage->liveCardIndex(window) >= 0 ? OpenedFrom::Place::Card : OpenedFrom::Place::Window;
    };
    const OpenedFrom::Join join = OpenedFrom::joinFor(placeOf(opened), placeOf(opener));
    // §11: a pane keeps a Stack behind it on a display that holds cards and
    // is sized Connected. On any other, the card behind would go to the dock
    // or wait where no step reaches it.
    const bool holdsCards = isTabletOutput(opener->screen()) && connectedForCardStage();
    bool joined = false;
    switch (join) {
    case OpenedFrom::Join::None:
        return leaves("the card it opened from is neither a pane nor a card now");
    case OpenedFrom::Join::Waits:
        // Asked again once it is a card; a late ticket finds it one already.
        return late ? leaves("it has no place in Cards") : false;
    case OpenedFrom::Join::TakesPane: {
        if (late) return false;
        if (!holdsCards) return leaves("its display keeps no Stack behind a pane");
        // As a Stack steps (stepPane): the new window is the pane, and the
        // card it opened from is the first of those behind it.
        const QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(opener);
        m_cardStage->forgetPaneStack(opener);
        m_cardStage->notePaneStack(opened, QList<KWin::EffectWindow *>{opener} + behind);
        if (!m_desktopStage->replacePaneWithCard(opener, opened, [] { return true; })) {
            m_cardStage->forgetPaneStack(opened);
            m_cardStage->notePaneStack(opener, behind);
            return leaves("it does not fit that card's pane");
        }
        m_cardStage->settlePaneStacks();
        activateShownPane(opened);
        joined = true;
        break;
    }
    case OpenedFrom::Join::PaneOnPane:
        // Its ticket followed it, so it had a pane of its own for a moment.
        joined = holdsCards && stackPaneOnto(opened, opener);
        break;
    case OpenedFrom::Join::CardBehindPane: {
        if (!holdsCards) return leaves("its display keeps no Stack behind a pane");
        // The card left the layout as the window opened; it waits behind the
        // window's pane with its own Stack (stackWhenLanded).
        QList<KWin::EffectWindow *> behind = m_cardStage->stackFrontToBack(opener);
        if (behind.isEmpty()) behind.append(opener);
        for (KWin::EffectWindow *card : m_cardStage->paneStack(opened))
            if (!behind.contains(card)) behind.append(card);
        m_cardStage->notePaneStack(opened, behind);
        m_cardStage->settlePaneStacks();
        joined = m_cardStage->paneStack(opened).contains(opener);
        break;
    }
    case OpenedFrom::Join::CardOnCard:
        // A card of a layout that left the screen stays in that layout.
        if (m_cardStage->inProjectedLayout(opener) || m_cardStage->inProjectedLayout(opened))
            return leaves("one of the two belongs to a layout waiting in Spread");
        joined = m_cardStage->stackCardOnto(opened, opener);
        break;
    }
    if (!joined) return leaves("Cards could not stack the two as they stand");
    m_openedFrom->forget(opened);
    qInfo() << "Kadunce" << Revision << "window origin: stacked" << logNameOf(opened)
            << "in front of" << logNameOf(opener) << "which it opened from"
            << (late ? "once its ticket came" : "as it opened");
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    KWin::effects->addRepaintFull();
    return true;
}

} // namespace Kadunce

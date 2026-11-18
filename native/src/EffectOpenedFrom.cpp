/* SPDX-License-Identifier: GPL-2.0-or-later */

// A window opened from a card joins that card's Stack, in front, where the
// person has switched that on (OpenedFrom.h, CARD-LIFECYCLE.md §9).
#include "EffectInternal.h"
#include "OpenedFromWatch.h"

namespace Kadunce
{

bool Effect::stacksOpenedWindows() const
{
    return m_openedFrom && m_openedFrom->follows != OpenedFrom::Follows::None;
}

void Effect::setStacksOpenedWindows(bool stacks)
{
    if (!m_openedFrom || stacks == stacksOpenedWindows()) return;
    setOpenedWindows(stacks ? QStringLiteral("every") : QStringLiteral("off"), m_openedFrom->pairs());
}

QVariantMap Effect::openedWindows() const
{
    if (!m_openedFrom) return {};
    QVariantMap answer;
    answer.insert(QStringLiteral("follows"), QString::fromLatin1(OpenedFrom::words(m_openedFrom->follows)));
    answer.insert(QStringLiteral("pairs"), m_openedFrom->pairs());
    return answer;
}

void Effect::setOpenedWindows(const QString &follows, const QStringList &pairs)
{
    if (!m_openedFrom) return;
    m_openedFrom->follows = OpenedFrom::followsFrom(follows.trimmed().toStdString());
    m_openedFrom->setPairs(pairs);
    const QString kept = QString::fromLatin1(OpenedFrom::words(m_openedFrom->follows));
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Stacks"));
    group.writeEntry(QStringLiteral("Opened"), kept);
    group.writeEntry(QStringLiteral("Pairs"), m_openedFrom->pairs());
    // Read by a Kadunce from before there were pairs.
    group.writeEntry(QStringLiteral("JoinOpener"), m_openedFrom->follows == OpenedFrom::Follows::Every);
    group.sync();
    qInfo().noquote() << "Kadunce" << Revision << "window origin: windows opened from a card follow" << kept
                      << "from now on, with" << m_openedFrom->pairs().size() << "pairs:"
                      << m_openedFrom->pairs().join(QStringLiteral(", "));
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
    if (m_openedFrom->follows == OpenedFrom::Follows::None) return leaves("joining is switched off");
    if (opener == opened || opener->isMinimized() || !isCardWindow(opened))
        return leaves("one of the two is no longer a card shown");
    // A second window of an application is that application's own business.
    if (applicationIdentity(opened) == applicationIdentity(opener)
        || (opened->window() && opener->window() && opened->window()->pid() == opener->window()->pid()))
        return leaves("both belong to one application");
    // Where the pair of the two applications says it lands.
    const OpenedFrom::Lands lands = m_openedFrom->landsFor(applicationIdentity(opener), applicationIdentity(opened));
    if (lands == OpenedFrom::Lands::AsBefore) return leaves("no pair names these two applications");
    if (lands == OpenedFrom::Lands::Alone) {
        // Nothing stands it beside the card either (handleWindowAdded).
        m_openedFrom->keepApart(opened);
        return leaves("their pair opens it on its own");
    }
    const bool beside = lands == OpenedFrom::Lands::Beside;
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
        // Beside a pane is where its layout stands any window (§8).
        if (beside) return leaves("their pair stands it beside, as the layout does with any window");
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
        if (beside) return leaves("it stands beside that card already");
        // Its ticket followed it, so it had a pane of its own for a moment.
        joined = holdsCards && stackPaneOnto(opened, opener);
        break;
    case OpenedFrom::Join::CardBehindPane: {
        if (beside) return leaves("the card it opened from left the layout for it");
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
        if (beside) {
            // §11, Connected: a window opening beside the one card shown is
            // paired with it as it opens (handleWindowAdded). Anywhere else,
            // and for a ticket that came late, the two are paired here, the
            // new window at the right and in use, as a pick from Spread is.
            if (holdsCards && !late) return leaves("their pair stands it beside, as a Connected display does");
            joined = pairCardsIntoBento(opener->screen(), opened, opener, BentoSidePlacement{true, true, holdsCards, false});
            if (joined) KWin::effects->activateWindow(opened);
            break;
        }
        joined = m_cardStage->stackCardOnto(opened, opener);
        break;
    }
    if (!joined) return leaves(beside ? "there is no room beside that card" : "Cards could not stack the two as they stand");
    m_openedFrom->forget(opened);
    qInfo() << "Kadunce" << Revision << "window origin:" << (beside ? "stood" : "stacked") << logNameOf(opened)
            << (beside ? "beside" : "in front of") << logNameOf(opener) << "which it opened from"
            << (late ? "once its ticket came" : "as it opened");
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
    KWin::effects->addRepaintFull();
    return true;
}

} // namespace Kadunce

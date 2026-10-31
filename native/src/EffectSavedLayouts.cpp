/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Saved layouts: reading the desk on the workspace and display in use as
// groups of applications, and arranging a desk from groups as the requester
// launches what is not running (SavedLayout.h, docs/REQUESTS.md § Saved
// layouts).
#include "EffectInternal.h"

#include <QDBusArgument>

namespace Kadunce
{

QString Effect::layoutAppIdentity(KWin::EffectWindow *window) const
{
    if (!isApplicationWindow(window) || !window->isNormalWindow() || dependentLead(window)) return {};
    // Only an application that can be launched again has a desktop id.
    QString app = applicationIdentity(window);
    if (app.endsWith(QStringLiteral(".desktop"))) app.chop(8);
    return !app.isEmpty() && KService::serviceByDesktopName(app) ? app : QString();
}

KWin::LogicalOutput *Effect::layoutDisplay() const
{
    // The display in use, as the dock or a touch last left it: asking from a
    // popup on another display must not read that one.
    if (KWin::LogicalOutput *inUse = screenNamed(inUseKey()); inUse && m_cardsOnEveryDisplay) return inUse;
    return KWin::effects->activeScreen();
}

QVariantMap Effect::currentLayout()
{
    if (!callerMayDrive()) return {};
    KWin::VirtualDesktop *desktop = KWin::effects->currentDesktop();
    KWin::LogicalOutput *output = layoutDisplay();
    QList<QStringList> entries;
    QList<QStringList> paneEntries;
    QSet<const KWin::EffectWindow *> counted;
    const auto stack = KWin::effects->stackingOrder();
    const auto onDesk = [output, desktop](const KWin::EffectWindow *window) {
        return window && !window->isDeleted() && window->screen() == output && desktop
            && window->isOnDesktop(desktop) && !window->isOnAllDesktops();
    };
    const auto entryOf = [this, &counted](const QList<KWin::EffectWindow *> &members) {
        QStringList apps;
        for (KWin::EffectWindow *member : members) {
            if (!member || counted.contains(member)) continue;
            counted.insert(member);
            apps.append(layoutAppIdentity(member));
        }
        return apps;
    };
    if (output && m_currentSession) {
        // A pane's Stack is the pane and the cards waiting behind it, as a
        // display showing its layout keeps one, read from the stage holding
        // it; front-most pane first.
        for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
            KWin::EffectWindow *pane = *it;
            if (!onDesk(pane) || counted.contains(pane) || !m_desktopStage->managesWindow(pane)) continue;
            SessionScope scope(this, m_currentSession, stageKeyOf(pane));
            paneEntries.append(entryOf(QList<KWin::EffectWindow *>{pane} + m_cardStage->paneStack(pane)));
        }
        // The cards on the display in use, left to right, each Stack front
        // first; those waiting behind a pane went with it.
        {
            SessionScope scope(this, m_currentSession, stageKeyFor(output));
            for (const auto &members : m_cardStage->rowEntries()) {
                QList<KWin::EffectWindow *> here;
                for (KWin::EffectWindow *member : members)
                    if (onDesk(member)) here.append(member);
                if (const QStringList apps = entryOf(here); !apps.isEmpty()) entries.append(apps);
            }
        }
        // Then any card that row did not hold, with its Stack, from the
        // stage holding it.
        for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
            KWin::EffectWindow *card = *it;
            if (!onDesk(card) || counted.contains(card)) continue;
            SessionScope scope(this, m_currentSession, stageKeyOf(card));
            if (m_cardStage->liveCardIndex(card) < 0) continue;
            const auto members = m_cardStage->stackFrontToBack(card);
            entries.append(entryOf(members.isEmpty() ? QList<KWin::EffectWindow *>{card} : members));
        }
    }
    entries.append(paneEntries);
    // Then windows that are neither, front-most first.
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::EffectWindow *window = *it;
        if (!onDesk(window) || counted.contains(window)) continue;
        entries.append({layoutAppIdentity(window)});
    }
    qInfo() << "Kadunce" << Revision << "read the desk on" << (output ? output->name() : QString()) << "as" << entries;
    QDBusArgument groups;
    groups.beginArray(qMetaTypeId<QVariantMap>());
    for (const auto &group : SavedLayouts::deskGroups(entries)) groups << SavedLayouts::toVariant(group);
    groups.endArray();
    return {
        {QStringLiteral("workspace"), desktop ? desktop->id() : QString()},
        {QStringLiteral("groups"), QVariant::fromValue(groups)},
    };
}

QVariantMap Effect::arrangeLayout(const QString &requestToken, const QList<QVariantMap> &requested)
{
    if (!callerMayDrive()) return {};
    const auto refused = [](const QString &reason) {
        return QVariantMap{{QStringLiteral("accepted"), false}, {QStringLiteral("reason"), reason}};
    };
    // One arrangement at a time, and never during a bring-back.
    if (m_layoutRun || m_bringBack) return refused(QStringLiteral("busy"));
    const auto groups = SavedLayouts::groupsFrom(requested);
    if (requestToken.isEmpty() || groups.isEmpty()) return refused(QStringLiteral("empty"));
    QList<SavedLayouts::OpenWindow> open;
    const auto stack = KWin::effects->stackingOrder();
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::EffectWindow *window = *it;
        if (!window || window->isDeleted() || window->isMinimized()) continue;
        const QString app = layoutAppIdentity(window);
        if (!app.isEmpty()) open.append({app, windowIdentity(window)});
    }
    // A minimized window still stands for its app, after every one shown.
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::EffectWindow *window = *it;
        if (!window || window->isDeleted() || !window->isMinimized()) continue;
        const QString app = layoutAppIdentity(window);
        if (!app.isEmpty()) open.append({app, windowIdentity(window)});
    }
    const auto plan = SavedLayouts::planArrangement(groups, open);
    KWin::VirtualDesktop *desktop = KWin::effects->currentDesktop();
    KWin::LogicalOutput *output = layoutDisplay();
    m_layoutDesktop = desktop ? desktop->id() : QString();
    m_layoutOutput = output ? output->name() : QString();
    m_layoutReadySince.clear();
    m_layoutSent.clear();
    m_layoutRun.emplace(requestToken, plan);
    const auto generation = ++m_layoutGeneration;
    QTimer::singleShot(LayoutWaitMs, this, [this, generation] {
        if (generation == m_layoutGeneration && m_layoutRun) m_layoutRun->expire();
    });
    // Windows already open go to their places from the next turn, after
    // the reply, and the run is looked at until it is over.
    QTimer::singleShot(0, this, [this, generation] {
        if (generation == m_layoutGeneration) settleLayout();
    });
    qInfo() << "Kadunce" << Revision << "arranges a layout of" << groups.size() << "groups;"
            << plan.launch.size() << "apps to launch," << plan.alreadyOpen.size() << "already open";
    return {
        {QStringLiteral("accepted"), true},
        {QStringLiteral("launch"), plan.launch},
        {QStringLiteral("alreadyOpen"), plan.alreadyOpen},
    };
}

void Effect::cancelLayout(const QString &requestToken)
{
    if (!callerMayDrive()) return;
    if (!m_layoutRun || m_layoutRun->token() != requestToken) return;
    m_layoutRun->expire();
}

bool Effect::claimLayoutWindow(KWin::EffectWindow *window)
{
    if (!m_layoutRun || !window) return false;
    const QString app = layoutAppIdentity(window);
    const QString id = windowIdentity(window);
    if (app.isEmpty() || !m_layoutRun->claim(app, id)) return false;
    // The window goes to the layout's workspace and display before it is
    // admitted anywhere, so it arrives there as a card.
    KWin::VirtualDesktop *desktop = m_layoutDesktop.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(m_layoutDesktop);
    if (desktop && (!window->isOnDesktop(desktop) || window->isOnAllDesktops()))
        KWin::effects->windowToDesktops(window, {desktop});
    KWin::LogicalOutput *output = screenNamed(m_layoutOutput);
    if (output && window->window() && window->screen() != output) window->window()->sendToOutput(output);
    return true;
}

void Effect::completeLayoutForWindow(KWin::EffectWindow *window)
{
    // The next look at the run brings it to its place.
    if (m_layoutRun && window) (void)m_layoutRun->admitted(windowIdentity(window));
}

bool Effect::layoutWindowSettled(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                 KWin::VirtualDesktop *desktop)
{
    if (window->screen() != output || !window->isOnDesktop(desktop) || window->isOnAllDesktops()) return false;
    if (desktop != KWin::effects->currentDesktop() || !m_currentSession) return true;
    SessionScope scope(this, m_currentSession, stageKeyFor(output));
    // A display that holds cards takes it as one; a pane keeps its layout.
    if (!m_cardStage->canOwnCards(output)) return true;
    return m_cardStage->liveCardIndex(window) >= 0 || m_desktopStage->managesWindow(window);
}

void Effect::settleLayout()
{
    if (!m_layoutRun) return;
    KWin::VirtualDesktop *desktop = m_layoutDesktop.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(m_layoutDesktop);
    if (!desktop) desktop = KWin::effects->currentDesktop();
    KWin::LogicalOutput *output = screenNamed(m_layoutOutput);
    if (!output) output = KWin::effects->activeScreen();
    const qint64 now = steadyMs();
    bool placedAny = false;
    // Nothing moves while a window is carried; the run waits for the hand.
    const bool held = m_carriedWindow || m_nativeCarry || m_releasing;
    for (const auto &slot : m_layoutRun->ready()) {
        if (held) break;
        KWin::EffectWindow *window = nullptr;
        for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
            if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == slot.windowId) window = candidate;
        const qint64 since = m_layoutReadySince.value(slot.windowId, now);
        m_layoutReadySince.insert(slot.windowId, since);
        bool placed = false;
        bool settled = !window || !output || !desktop;
        if (!settled && layoutWindowSettled(window, output, desktop)) {
            placed = settled = true;
        } else if (!settled && !m_layoutSent.contains(slot.windowId)) {
            // Sent once: to the layout's workspace, out of the minimized
            // windows, and then as a placement request's window to the
            // middle of the display, which makes it a card there.
            m_layoutSent.insert(slot.windowId);
            if (!window->isOnDesktop(desktop) || window->isOnAllDesktops())
                KWin::effects->windowToDesktops(window, {desktop});
            if (window->isMinimized() && window->window()) window->window()->setMinimized(false);
            if (desktop == KWin::effects->currentDesktop() && m_currentSession) {
                bool ownsCards = false;
                {
                    SessionScope scope(this, m_currentSession, stageKeyFor(output));
                    ownsCards = m_cardStage->canOwnCards(output);
                }
                const QRectF area(output->geometry());
                const QPointF point = area.center();
                const PlacementAim aim = placementAim({{output->name(), area, ownsCards}}, point);
                if (aim.kind != PlacementAimKind::None) (void)placeWindow(window, aim, point);
            } else if (window->window() && window->screen() != output) {
                window->window()->sendToOutput(output);
            }
        } else if (!settled && now - since > LayoutSettleMs) {
            settled = true;
        }
        if (!settled || !m_layoutRun->settle(slot.windowId, placed)) continue;
        placedAny = placedAny || placed;
        Q_EMIT layoutAppPlaced(m_layoutRun->token(), slot.appId, placed);
    }
    // Stacks form as their apps come; the row is put in order at the end.
    if (placedAny) arrangeLayoutCards(false);
    if (!m_layoutRun->finished()) {
        const auto generation = m_layoutGeneration;
        QTimer::singleShot(LayoutTickMs, this, [this, generation] {
            if (generation == m_layoutGeneration) settleLayout();
        });
        return;
    }
    arrangeLayoutCards(true);
    const QString token = m_layoutRun->token();
    const int placed = m_layoutRun->placed();
    const int missed = m_layoutRun->missed();
    m_layoutRun.reset();
    m_layoutReadySince.clear();
    m_layoutSent.clear();
    ++m_layoutGeneration;
    qInfo() << "Kadunce" << Revision << "arranged a layout:" << placed << "apps placed," << missed << "missed";
    Q_EMIT layoutFinished(token, placed, missed);
}

void Effect::arrangeLayoutCards(bool order)
{
    if (!m_layoutRun || !m_currentSession) return;
    KWin::LogicalOutput *output = screenNamed(m_layoutOutput);
    if (!output) output = KWin::effects->activeScreen();
    KWin::VirtualDesktop *current = KWin::effects->currentDesktop();
    // A layout's workspace not shown forms its cards when it is entered.
    if (!output || !current || current->id() != m_layoutDesktop) return;
    QList<QList<KWin::EffectWindow *>> entries;
    for (const auto &ids : m_layoutRun->entries()) {
        QList<KWin::EffectWindow *> windows;
        for (const auto &id : ids)
            for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
                if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == id) windows.append(candidate);
        if (!windows.isEmpty()) entries.append(windows);
    }
    if (entries.isEmpty()) return;
    // A display showing its layout keeps a Stack as a pane with the rest
    // behind it: the group's first pane stands in front, every other pane of
    // it joins in its order, and its cards wait behind them.
    for (auto &windows : entries) {
        if (windows.size() < 2) continue;
        QList<KWin::EffectWindow *> panes;
        QList<KWin::EffectWindow *> cards;
        {
            SessionScope scope(this, m_currentSession, stageKeyFor(output));
            if (!m_desktopStage->managesWindow(windows.first())) continue;
            for (KWin::EffectWindow *window : windows) {
                if (m_desktopStage->managesWindow(window)) panes.append(window);
                else if (m_cardStage->liveCardIndex(window) >= 0) cards.append(window);
            }
        }
        for (qsizetype at = panes.size() - 1; at > 0; --at) {
            bool already = false;
            {
                SessionScope scope(this, m_currentSession, stageKeyOf(panes.at(at)));
                already = m_cardStage->paneStack(panes.at(at - 1)).contains(panes.at(at));
            }
            if (!already) (void)stackPaneOnto(panes.at(at - 1), panes.at(at));
        }
        KWin::EffectWindow *front = panes.first();
        if (!cards.isEmpty()) {
            SessionScope scope(this, m_currentSession, stageKeyOf(front));
            QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(front);
            for (KWin::EffectWindow *card : std::as_const(cards))
                if (!behind.contains(card)) behind.append(card);
            m_cardStage->notePaneStack(front, behind);
            m_cardStage->settlePaneStacks();
        }
        qInfo() << "Kadunce" << Revision << "stood a layout's Stack of" << windows.size() << "with"
                << logNameOf(front) << "in front";
        // Every one of them now stands in the pane's Stack.
        windows = {front};
    }
    KWin::EffectWindow *first = nullptr;
    {
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        // Only cards here join; a pane keeps its layout.
        for (auto &windows : entries)
            windows.removeIf([this](KWin::EffectWindow *window) { return m_cardStage->liveCardIndex(window) < 0; });
        entries.removeIf([](const auto &windows) { return windows.isEmpty(); });
        if (entries.isEmpty() || !m_cardStage->arrangeEntries(entries, order)) return;
        first = entries.first().first();
    }
    observeCardOwnership();
    // The first group is the card in use.
    if (order && first) (void)activateApplicationWindow(windowIdentity(first));
}

} // namespace Kadunce

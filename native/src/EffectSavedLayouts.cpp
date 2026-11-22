/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Saved layouts: reading the desk on the workspace shown as groups of
// applications, each with the display it stands on and a pane's shape, and
// arranging a desk from groups as the requester launches what is not running
// (SavedLayout.h, docs/REQUESTS.md § Saved layouts).
#include "EffectInternal.h"

#include <QDBusArgument>
#include <QTimer>

#include <functional>
#include <memory>

namespace Kadunce
{

// The display a group of the run stands on: the one kept with it while it
// is connected, else the display the layout was asked on.
static QString displayOfGroup(const std::optional<SavedLayouts::LayoutRun> &run, int group, const QString &asked)
{
    const QString kept = run && group >= 0 ? run->groups().value(group).display : QString();
    return !kept.isEmpty() && screenNamed(kept) ? kept : asked;
}

QString Effect::layoutAppIdentity(KWin::EffectWindow *window, bool anyDesktop) const
{
    if (!isApplicationWindow(window, anyDesktop) || !window->isNormalWindow() || dependentLead(window)) return {};
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
    KWin::LogicalOutput *inUse = layoutDisplay();
    QSet<const KWin::EffectWindow *> counted;
    const auto stack = KWin::effects->stackingOrder();
    const auto onDesk = [desktop](const KWin::EffectWindow *window, const KWin::LogicalOutput *output) {
        return window && !window->isDeleted() && window->screen() == output && desktop
            && window->isOnDesktop(desktop) && !window->isOnAllDesktops();
    };
    // An entry of windows not yet counted: their applications, the display,
    // and where the first of them is drawn on it.
    const auto entryOf = [this, &counted](const QList<KWin::EffectWindow *> &members, KWin::LogicalOutput *output) {
        SavedLayouts::Entry entry;
        entry.display = output->name();
        const QRectF area(output->geometry());
        for (KWin::EffectWindow *member : members) {
            if (!member || counted.contains(member)) continue;
            counted.insert(member);
            const QRectF frame(member->frameGeometry());
            if (entry.apps.isEmpty() && area.width() > 0 && area.height() > 0)
                entry.picture = {(frame.x() - area.x()) / area.width(), (frame.y() - area.y()) / area.height(),
                                 frame.width() / area.width(), frame.height() / area.height()};
            entry.apps.append(layoutAppIdentity(member));
        }
        return entry;
    };
    // One display's desk: its cards left to right, then its panes, then any
    // other window, front-most first.
    const auto read = [&](KWin::LogicalOutput *output) {
        QList<SavedLayouts::Entry> entries;
        QList<SavedLayouts::Entry> paneEntries;
        if (output && m_currentSession) {
            // A pane's Stack is the pane and the cards waiting behind it, as a
            // display showing its layout keeps one, read from the stage holding
            // it; front-most pane first. Its shape is the layout's own.
            const auto panes = m_desktopStage->panesOn(output->name());
            const auto shapes = m_desktopStage->paneShapesOn(output->name());
            for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
                KWin::EffectWindow *pane = *it;
                if (!onDesk(pane, output) || counted.contains(pane) || !m_desktopStage->managesWindow(pane)) continue;
                SessionScope scope(this, m_currentSession, stageKeyOf(pane));
                SavedLayouts::Entry entry = entryOf(QList<KWin::EffectWindow *>{pane} + m_cardStage->paneStack(pane), output);
                for (int index = 0; index < panes.size() && std::size_t(index) < shapes.size(); ++index) {
                    if (panes.at(index).data() != pane) continue;
                    const BentoRect &shape = shapes[std::size_t(index)];
                    entry.pane = true;
                    entry.shape = {shape.x, shape.y, shape.width, shape.height};
                    entry.shown = true;
                }
                paneEntries.append(entry);
            }
            // The cards on the display, left to right, each Stack front
            // first; those waiting behind a pane went with it.
            {
                SessionScope scope(this, m_currentSession, stageKeyFor(output));
                // The card in use is the one seen, where no pane covers it.
                KWin::EffectWindow *inUseCard = paneEntries.isEmpty() ? m_cardStage->activeCardIdentity() : nullptr;
                for (const auto &members : m_cardStage->rowEntries()) {
                    QList<KWin::EffectWindow *> here;
                    for (KWin::EffectWindow *member : members)
                        if (onDesk(member, output)) here.append(member);
                    auto entry = entryOf(here, output);
                    if (entry.apps.isEmpty()) continue;
                    entry.shown = inUseCard && here.contains(inUseCard);
                    entries.append(entry);
                }
            }
            // Then any card that row did not hold, with its Stack, from the
            // stage holding it.
            for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
                KWin::EffectWindow *card = *it;
                if (!onDesk(card, output) || counted.contains(card)) continue;
                SessionScope scope(this, m_currentSession, stageKeyOf(card));
                if (m_cardStage->liveCardIndex(card) < 0) continue;
                const auto members = m_cardStage->stackFrontToBack(card);
                entries.append(entryOf(members.isEmpty() ? QList<KWin::EffectWindow *>{card} : members, output));
            }
        }
        entries.append(paneEntries);
        // Then windows that are neither, front-most first.
        for (auto it = stack.crbegin(); output && it != stack.crend(); ++it) {
            KWin::EffectWindow *window = *it;
            if (!onDesk(window, output) || counted.contains(window)) continue;
            auto entry = entryOf({window}, output);
            entry.shown = !window->isMinimized();
            entries.append(entry);
        }
        return entries;
    };
    // The display in use first, then every other, left to right.
    QList<KWin::LogicalOutput *> displays = KWin::effects->screens();
    std::stable_sort(displays.begin(), displays.end(), [inUse](KWin::LogicalOutput *a, KWin::LogicalOutput *b) {
        if ((a == inUse) != (b == inUse)) return a == inUse;
        return a->geometry().x() < b->geometry().x();
    });
    QList<SavedLayouts::Entry> here;
    QList<SavedLayouts::Entry> everywhere;
    for (KWin::LogicalOutput *display : std::as_const(displays)) {
        const auto entries = read(display);
        if (display == inUse) here = entries;
        everywhere.append(entries);
    }
    qInfo() << "Kadunce" << Revision << "read the desk:" << here.size() << "entries on"
            << (inUse ? inUse->name() : QString()) << "and" << everywhere.size() << "on every display";
    const auto marshalled = [](const QList<SavedLayouts::Group> &groups) {
        QDBusArgument argument;
        argument.beginArray(qMetaTypeId<QVariantMap>());
        for (const auto &group : groups) argument << SavedLayouts::toVariant(group);
        argument.endArray();
        return QVariant::fromValue(argument);
    };
    QDBusArgument shown;
    shown.beginArray(qMetaTypeId<QVariantMap>());
    for (KWin::LogicalOutput *display : std::as_const(displays)) {
        const QRectF area(display->geometry());
        shown << QVariantMap{{QStringLiteral("name"), display->name()}, {QStringLiteral("x"), area.x()},
                             {QStringLiteral("y"), area.y()}, {QStringLiteral("width"), area.width()},
                             {QStringLiteral("height"), area.height()}, {QStringLiteral("inUse"), display == inUse}};
    }
    shown.endArray();
    return {
        {QStringLiteral("workspace"), desktop ? desktop->id() : QString()},
        {QStringLiteral("groups"), marshalled(SavedLayouts::deskGroups(here))},
        {QStringLiteral("everywhere"), marshalled(SavedLayouts::deskGroups(everywhere))},
        {QStringLiteral("displays"), QVariant::fromValue(shown)},
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
        // An app open on another workspace is brought over, not launched:
        // one that keeps a single window would only raise it where it is.
        const QString app = layoutAppIdentity(window, true);
        if (!app.isEmpty()) open.append({app, windowIdentity(window)});
    }
    // A minimized window still stands for its app, after every one shown.
    for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
        KWin::EffectWindow *window = *it;
        if (!window || window->isDeleted() || !window->isMinimized()) continue;
        const QString app = layoutAppIdentity(window, true);
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
    const auto claimed = m_layoutRun->slotFor(id);
    qInfo() << "Kadunce" << Revision << "layout claims" << logNameOf(window) << "on" << (window->screen() ? window->screen()->name() : QString())
            << "for display" << displayOfGroup(m_layoutRun, claimed ? claimed->group : -1, m_layoutOutput);
    // The window goes to the layout's workspace and display before it is
    // admitted anywhere, so it arrives there as a card.
    KWin::VirtualDesktop *desktop = m_layoutDesktop.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(m_layoutDesktop);
    if (desktop && (!window->isOnDesktop(desktop) || window->isOnAllDesktops()))
        KWin::effects->windowToDesktops(window, {desktop});
    const auto slot = m_layoutRun->slotFor(id);
    KWin::LogicalOutput *output = screenNamed(displayOfGroup(m_layoutRun, slot ? slot->group : -1, m_layoutOutput));
    if (output && window->window() && window->screen() != output) window->window()->sendToOutput(output);
    qInfo() << "Kadunce" << Revision << "layout sent" << logNameOf(window) << "to" << (window->screen() ? window->screen()->name() : QString());
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
    const qint64 now = steadyMs();
    bool placedAny = false;
    // Nothing moves while a window is carried; the run waits for the hand.
    const bool held = m_carriedWindow || m_nativeCarry || m_releasing;
    for (const auto &slot : m_layoutRun->ready()) {
        if (held) break;
        KWin::EffectWindow *window = nullptr;
        for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
            if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == slot.windowId) window = candidate;
        // Each window goes to the display its group stood on.
        KWin::LogicalOutput *output = screenNamed(displayOfGroup(m_layoutRun, slot.group, m_layoutOutput));
        if (!output) output = KWin::effects->activeScreen();
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
            // Held as a pane or a card on another display, as a window that
            // opens where its app last was is taken there before it can move,
            // it is let go there first; placed while held, it stays.
            if (window->screen() != output && m_currentSession && desktop == KWin::effects->currentDesktop()) {
                SessionScope there(this, m_currentSession, stageKeyFor(window->screen()));
                if (CardStageController *own = stageHolding(window)) {
                    (void)own->releaseCard(window);
                } else if (m_desktopStage->managesWindow(window)) {
                    m_desktopStage->releaseWindow(window);
                    for (const auto &display : m_currentSession->displays)
                        display.second->cards->paneLeftWithoutCard(window);
                }
                qInfo() << "Kadunce" << Revision << "layout lets" << logNameOf(window) << "go from"
                        << (window->screen() ? window->screen()->name() : QString()) << "for" << output->name();
            }
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
            // Which of layoutWindowSettled's conditions held it back.
            int card = -2;
            bool managed = false;
            if (desktop == KWin::effects->currentDesktop() && m_currentSession) {
                SessionScope scope(this, m_currentSession, stageKeyFor(output));
                card = m_cardStage->canOwnCards(output) ? m_cardStage->liveCardIndex(window) : -3;
                managed = m_desktopStage->managesWindow(window);
            }
            qInfo() << "Kadunce" << Revision << "layout missed" << logNameOf(window) << "on"
                    << (window->screen() ? window->screen()->name() : QString()) << "for" << output->name()
                    << "on its workspace" << window->isOnDesktop(desktop) << "everywhere" << window->isOnAllDesktops()
                    << "shown" << (desktop == KWin::effects->currentDesktop()) << "card" << card << "managed" << managed
                    << "minimized" << window->isMinimized() << "full screen" << window->isFullScreen()
                    << "frame" << window->frameGeometry();
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
    if (!m_layoutRun) return;
    // Put in order at the end, a Stack Cards could not form yet, as while
    // Search is still leaving, is tried again until it stands.
    const auto generation = ++m_layoutStandGeneration;
    // Display by display, the one the layout was asked on last, so that its
    // first group is the one in use at the end.
    QHash<QString, QSet<int>> groupsOn;
    QStringList names;
    for (int group = 0; group < m_layoutRun->groups().size(); ++group) {
        const QString name = displayOfGroup(m_layoutRun, group, m_layoutOutput);
        if (!groupsOn.contains(name)) names.append(name);
        groupsOn[name].insert(group);
    }
    if (names.removeAll(m_layoutOutput) > 0) names.append(m_layoutOutput);
    for (const QString &name : std::as_const(names)) {
        // The card that was seen on the display is the one in use there.
        QString seen;
        for (int group = 0; seen.isEmpty() && group < m_layoutRun->groups().size(); ++group)
            if (const auto &kept = m_layoutRun->groups().at(group); groupsOn[name].contains(group) && kept.shown && !kept.pane)
                seen = m_layoutRun->frontOf(group);
        standLayout(m_layoutRun->entries(&groupsOn[name]), name, m_layoutDesktop, order,
                    order ? LayoutStandTries : 0, generation, seen);
    }
    if (!order) return;
    // Then each display's panes take the shapes kept for them. A display is
    // stood whole: every pane of the layout there, and no other.
    QHash<int, QString> fronts;
    for (int group = 0; group < m_layoutRun->groups().size(); ++group)
        if (const QString front = m_layoutRun->frontOf(group); !front.isEmpty()) fronts.insert(group, front);
    for (const QString &name : std::as_const(names)) {
        const auto shapes = SavedLayouts::paneShapes(m_layoutRun->groups(), groupsOn[name], fronts);
        // One pane has no shape to take: it fills what its display gives it.
        if (shapes.size() < 2) continue;
        const auto attempt = std::make_shared<std::function<void(int)>>();
        *attempt = [this, name, shapes, generation, again = std::weak_ptr(attempt)](int tries) {
            if (generation != m_layoutStandGeneration) return;
            KWin::LogicalOutput *output = screenNamed(name);
            if (!output) return;
            const auto later = [this, &again, tries](const QString &why) {
                const auto self = again.lock();
                if (tries <= 0 || !self) {
                    qInfo() << "Kadunce" << Revision << "left a layout's panes as their display laid them out:" << why;
                    return;
                }
                QTimer::singleShot(LayoutStandRetryMs, this, [self, tries] { (*self)(tries - 1); });
            };
            if (m_carriedWindow || m_nativeCarry || m_releasing) return later(QStringLiteral("a window is carried"));
            QList<KWin::EffectWindow *> windows;
            std::vector<BentoRect> rects;
            for (const auto &shape : shapes) {
                KWin::EffectWindow *window = nullptr;
                for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
                    if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == shape.windowId) window = candidate;
                if (!window) return later(QStringLiteral("a window of it is gone"));
                if (window->screen() != output || layoutRoleOf(window) != QLatin1String("pane"))
                    return later(logNameOf(window) + QStringLiteral(" is not a pane on ") + name);
                windows.append(window);
                rects.push_back({shape.shape.x, shape.shape.y, shape.shape.width, shape.shape.height});
            }
            // A pane the layout does not keep leaves for the display's cards,
            // where it has them; nothing is closed.
            for (KWin::EffectWindow *pane : m_desktopStage->panesOn(output)) {
                if (windows.contains(pane)) continue;
                // As Shift+Down takes a pane out of its layout: it waits in
                // Spread as a card.
                SessionScope scope(this, m_currentSession, stageKeyOf(pane));
                if (!m_desktopStage->extractPaneToCards(pane, [] { return true; }, true))
                    return later(logNameOf(pane) + QStringLiteral(" stays a pane beside it"));
                qInfo() << "Kadunce" << Revision << "took" << logNameOf(pane) << "out of the layout a saved one stands in";
                observeCardOwnership();
            }
            if (!m_desktopStage->standPanesAt(output, windows, rects)) return later(QStringLiteral("its shapes were refused"));
        };
        (*attempt)(LayoutStandTries);
    }
}

QString Effect::layoutRoleOf(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted()) return QStringLiteral("gone");
    SessionScope scope(this, m_currentSession, stageKeyOf(window));
    if (m_desktopStage->managesWindow(window)) return QStringLiteral("pane");
    if (m_cardStage->liveCardIndex(window) >= 0) return QStringLiteral("card");
    return QStringLiteral("window");
}

void Effect::standLayout(const QList<QStringList> &ids, const QString &outputName, const QString &desktopId,
                         bool order, int tries, quint64 generation, const QString &seen)
{
    if (generation != m_layoutStandGeneration || !m_currentSession) return;
    // A hand moving a window since leaves the desk to it.
    if (tries < LayoutStandTries && (m_carriedWindow || m_nativeCarry || m_releasing)) return;
    KWin::LogicalOutput *output = screenNamed(outputName);
    if (!output) output = KWin::effects->activeScreen();
    KWin::VirtualDesktop *current = KWin::effects->currentDesktop();
    // A layout's workspace not shown forms its cards when it is entered.
    if (!output || !current || current->id() != desktopId) return;
    QList<QList<KWin::EffectWindow *>> entries;
    for (const auto &group : ids) {
        QList<KWin::EffectWindow *> windows;
        for (const auto &id : group)
            for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
                if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == id) windows.append(candidate);
        if (!windows.isEmpty()) entries.append(windows);
    }
    if (entries.isEmpty()) return;
    QStringList unformed;
    QString busy;
    {
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        if (!m_cardStage->isActive()) busy = QStringLiteral("Cards is not running here");
        else if (m_cardStage->launcherGuestActive()) busy = QStringLiteral("Search is still open");
        else if (m_cardStage->cardGrabActive()) busy = QStringLiteral("a card is held");
    }
    // A display showing its layout keeps a Stack as a pane with the rest
    // behind it: the group's first pane stands in front, every other pane of
    // it joins in its order, and its cards wait behind them.
    QList<QList<KWin::EffectWindow *>> cardStacks;
    for (auto &windows : entries) {
        if (windows.size() < 2) continue;
        KWin::EffectWindow *front = windows.first();
        if (layoutRoleOf(front) != QLatin1String("pane")) {
            cardStacks.append(windows);
            continue;
        }
        QList<KWin::EffectWindow *> panes;
        QList<KWin::EffectWindow *> cards;
        for (KWin::EffectWindow *window : std::as_const(windows)) {
            const QString role = layoutRoleOf(window);
            if (role == QLatin1String("pane")) panes.append(window);
            else if (role == QLatin1String("card")) cards.append(window);
        }
        for (qsizetype at = panes.size() - 1; at > 0; --at) {
            bool already = false;
            {
                SessionScope scope(this, m_currentSession, stageKeyOf(panes.at(at)));
                already = m_cardStage->paneStack(panes.at(at - 1)).contains(panes.at(at));
            }
            if (already) continue;
            SessionScope scope(this, m_currentSession, stageKeyOf(panes.at(at)));
            (void)stackPaneOnto(panes.at(at - 1), panes.at(at));
        }
        if (!cards.isEmpty()) {
            SessionScope scope(this, m_currentSession, stageKeyOf(front));
            QList<KWin::EffectWindow *> behind = m_cardStage->paneStack(front);
            for (KWin::EffectWindow *card : std::as_const(cards))
                if (!behind.contains(card)) behind.append(card);
            m_cardStage->notePaneStack(front, behind);
            m_cardStage->settlePaneStacks();
        }
        // It stands when every other one waits behind the front pane, and
        // Cards was free to set them there.
        bool formed = busy.isEmpty();
        {
            SessionScope scope(this, m_currentSession, stageKeyOf(front));
            const auto behind = m_cardStage->paneStack(front);
            for (KWin::EffectWindow *window : std::as_const(windows))
                if (window != front && !behind.contains(window)) formed = false;
        }
        if (formed) {
            qInfo() << "Kadunce" << Revision << "stood a layout's Stack of" << windows.size() << "with"
                    << logNameOf(front) << "in front";
        } else {
            QStringList roles;
            for (KWin::EffectWindow *window : std::as_const(windows))
                roles.append(logNameOf(window) + QLatin1Char(' ') + layoutRoleOf(window));
            unformed.append(roles.join(QStringLiteral(", ")));
        }
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
        if (!entries.isEmpty() && m_cardStage->arrangeEntries(entries, order)) first = entries.first().first();
        // The card seen when the layout was kept, when it is a card here.
        for (const auto &windows : std::as_const(entries))
            if (first && !seen.isEmpty() && windowIdentity(windows.first()) == seen) first = windows.first();
        // A Stack of cards stands when its front holds every one of them.
        for (const auto &windows : std::as_const(cardStacks)) {
            const auto stood = m_cardStage->stackFrontToBack(windows.first());
            bool formed = stood.size() == windows.size();
            for (KWin::EffectWindow *window : windows) formed = formed && stood.contains(window);
            if (formed) {
                qInfo() << "Kadunce" << Revision << "stood a layout's Stack of" << windows.size() << "cards with"
                        << logNameOf(windows.first()) << "in front";
                continue;
            }
            QStringList roles;
            for (KWin::EffectWindow *window : windows) {
                const QString role = m_cardStage->liveCardIndex(window) >= 0 ? QStringLiteral("card here")
                                                                                : layoutRoleOf(window);
                roles.append(logNameOf(window) + QLatin1Char(' ') + role);
            }
            unformed.append(roles.join(QStringLiteral(", ")));
        }
    }
    if (first) observeCardOwnership();
    // The first group is the card in use: so at once, and again once a
    // Stack that waited stands.
    if (first && order && (tries == LayoutStandTries || unformed.isEmpty()))
        (void)activateApplicationWindow(windowIdentity(first));
    if (unformed.isEmpty() || tries <= 0) {
        if (!unformed.isEmpty() && order)
            qInfo() << "Kadunce" << Revision << "could not stand a layout's Stack on" << output->name() << ":"
                    << unformed << (busy.isEmpty() ? QString() : QStringLiteral("while ") + busy);
        return;
    }
    if (tries == LayoutStandTries)
        qInfo() << "Kadunce" << Revision << "waits to stand a layout's Stack on" << output->name() << ":" << unformed
                << (busy.isEmpty() ? QString() : QStringLiteral("while ") + busy);
    QTimer::singleShot(LayoutStandRetryMs, this, [this, ids, outputName, desktopId, order, tries, generation, seen] {
        standLayout(ids, outputName, desktopId, order, tries - 1, generation, seen);
    });
}

} // namespace Kadunce

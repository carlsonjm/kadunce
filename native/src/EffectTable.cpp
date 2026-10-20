/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Table and the workspaces it shows, the desktop preview and named desktops.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// Off unless asked for (TESTING.md § Confirming the running build): what the
// pointer is over near a display's top edge, read on every motion there.
Q_LOGGING_CATEGORY(KaduncePointer, "kadunce.pointer", QtWarningMsg)
// The shape a window's border asks the pointer for. KWin 6.8 no longer
// exposes it, and there it reads as none.
template<typename Window>
QByteArray borderCursorName(Window *window)
{
    if constexpr (requires { window->cursor().name(); }) return window->cursor().name();
    else return QByteArray();
}

const char *tableActionName(TableAction::Kind kind)
{
    switch (kind) {
    case TableAction::Kind::None: return "nothing";
    case TableAction::Kind::StayOpen: return "the tabs stay open";
    case TableAction::Kind::Close: return "cancelled";
    case TableAction::Kind::Enter: return "entered a workspace";
    case TableAction::Kind::Activate: return "opened a card";
    case TableAction::Kind::Move: return "moved a card";
    case TableAction::Kind::Create: return "made a workspace";
    case TableAction::Kind::New: return "made an empty workspace";
    case TableAction::Kind::Rename: return "renaming a workspace";
    }
    return "nothing";
}

// KDE names a desktop it makes "Desktop N". Table treats that as no name yet:
// the tab is its number until an application names it.
bool unnamedDesktop(const QString &name)
{
    static const QRegularExpression kdeDefault(QStringLiteral("^Desktop \\d+$"));
    return name.trimmed().isEmpty() || kdeDefault.match(name).hasMatch();
}
}

void Effect::handleDesktopChanged(KWin::VirtualDesktop *previous, KWin::VirtualDesktop *current)
{
    if (previous == current) return;
    if (current) m_unenteredDesktops.remove(current->id());
    // Table's own switches close it first; any other ends it.
    if (m_table.isOpen()) closeTable();
    // A switch made some other way ends a preview of a different desktop. A
    // commit keeps it, and the screen, until the switch has been announced.
    if (m_previewDesktop && current != m_previewDesktop) endDesktopPreview();
    // Nothing started on the desktop being left may still be in hand. Its
    // session keeps its cards and layouts exactly as they are.
    cancelInputForCardStage();
    m_currentSession = sessionFor(current);
    scopeTo(m_currentSession, m_scopedKey);
    bool holding = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output)) continue;
        SessionScope scope(this, m_currentSession, stageKeyFor(output));
        setPagingShortcutsActive(m_cardStage->isActive());
        // Coming back finds the cards presented, not a Spread left open. It
        // cannot close on the way out: its cards are not on the desktop being shown.
        if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread)
            toggleOwnedPresentation();
        if (m_cardStage->isActive()) holding = true;
    }
    // Windows that opened or arrived here while it was not shown become this
    // desktop's cards, as windows arriving on a display do.
    if (holding) scheduleCardDisplaySettle();
    // §3 for a display with no cards yet on this desktop: its windows become
    // cards, the one in use Active, as when Kadunce is switched on. The turn
    // lets KWin finish activating a window on the desktop it switched to. A
    // display presenting its layout starts no cards; what arrived there while
    // it was not shown waits behind the layout instead.
    QTimer::singleShot(0, this, [this, holding] {
        if (!holdDisplaysInCards(nullptr) && !holding) scheduleCardDisplaySettle();
    });
    scheduleDependentSync();
    observeCardOwnership();
    // The desktop just left may now be an empty, unnamed workspace.
    scheduleDissolve();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleDesktopRemoved(KWin::VirtualDesktop *desktop)
{
    if (m_table.isOpen()) closeTable();
    if (desktop && m_namedDesktops.removeAll(desktop->id()) > 0) saveNamedDesktops();
    if (desktop) m_unenteredDesktops.remove(desktop->id());
    // A stack carried to it and not yet dealt goes with it.
    if (desktop) m_pendingDecks.remove(desktop->id());
    // KWin has already moved the desktop's windows to a neighbour. Its session
    // lets them go as a release would, and the desktop they stand on now takes
    // them as its own (DesktopRemoval.h). Removing the desktop shown can come
    // before the switch away from it, or with none, so the desktop KWin shows
    // now is made current first.
    const QString id = desktop ? desktop->id() : QString();
    auto removed = m_sessions.find(id);
    KWin::VirtualDesktop *shown = KWin::effects->currentDesktop();
    const DesktopRemovalPlan plan = planDesktopRemoval(removed != m_sessions.end(), id,
        m_currentSession ? m_currentSession->desktopId : QString(), shown ? shown->id() : QString());
    if (!plan.release) return;
    if (plan.switchFirst) handleDesktopChanged(desktop, shown);
    {
        SessionScope scope(this, removed->second.get());
        if (hasActiveDesktopStage()) {
            m_desktopStage->stopPendingSettle();
            m_desktopStage->restoreAllSessions();
        }
        forEachDisplayStage([](CardStageController &stage) { stage.release(); });
    }
    if (plan.forget) m_sessions.erase(removed);
    scheduleCardDisplaySettle();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

void Effect::handleWindowDesktopsChanged(KWin::EffectWindow *window)
{
    if (!window || window->isDeleted()) return;
    DesktopSession *holder = sessionHolding(window);
    if (holder && holder->desktop
        && (!window->isOnDesktop(holder->desktop) || window->isOnAllDesktops())) {
        SessionScope scope(this, holder);
        if (CardStageController *own = stageHolding(window)) (void)own->releaseCard(window);
        else {
            m_desktopStage->releaseWindow(window);
            // A pane sent to another desktop leaves the cards behind it here.
            for (const auto &display : holder->displays) display.second->cards->paneLeftWithoutCard(window);
        }
    }
    if (window->isOnCurrentDesktop()) scheduleCardDisplaySettle();
    syncDesktopPreview();
    scheduleDissolve();
    scheduleDependentSync();
    observeCardOwnership();
    Q_EMIT workspaceContextChanged();
}

bool Effect::hiddenOnOtherDesktop(const KWin::EffectWindow *window) const
{
    if (!window || window->isOnCurrentDesktop()) return false;
    const DesktopSession *holder = sessionHolding(window);
    const CardStageController *cards = holder ? holder->stageHolding(window) : nullptr;
    // A Spread left open there comes back as its Active card, so that card is
    // the one shown; a desktop presenting its layout shows only its panes.
    return holder && holder != m_currentSession && cards
        && !(cards->presentation() != CardPresentation::Bento && cards->selectedWindow() == window);
}

bool Effect::hiddenByDesktopPreview(const KWin::EffectWindow *window) const
{
    if (!m_previewDesktop || !window || window->isDeleted()) return false;
    if (window->isDock() || window->isDesktop() || window->isOnAllDesktops()
        || window == KWin::effects->inputPanel()) return false;
    if (!window->isOnDesktop(m_previewDesktop)) return true;
    // A desktop's cards show as it presents them: the card Table's finger is
    // on, or else its front card. Its panes and plain windows show as they are.
    const DesktopSession *holder = sessionHolding(window);
    const CardStageController *cards = holder ? holder->stageHolding(window) : nullptr;
    if (!cards) return false;
    if (m_previewCard && cards->liveCardIndex(m_previewCard) >= 0) return window != m_previewCard;
    return !(cards->presentation() != CardPresentation::Bento && cards->selectedWindow() == window);
}

void Effect::syncDesktopPreview()
{
    if (!m_previewDesktop) return;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        const bool held = !window->isDeleted() && window->isOnDesktop(m_previewDesktop)
            && !window->isOnCurrentDesktop();
        if (held && !m_previewRefs.contains(window)) {
            m_previewRefs.insert(window,
                KWin::EffectWindowVisibleRef(window, KWin::EffectWindow::PAINT_DISABLED_BY_DESKTOP));
        } else if (!held) {
            m_previewRefs.remove(window);
        }
    }
}

void Effect::endDesktopPreview()
{
    if (!m_previewDesktop) return;
    m_previewDesktop.clear();
    m_previewCard.clear();
    m_previewRefs.clear();
    if (KWin::effects->activeFullScreenEffect() == this)
        KWin::effects->setActiveFullScreenEffect(nullptr);
    KWin::effects->addRepaintFull();
}

bool Effect::previewDesktop(const QString &desktopId)
{
    if (!callerMayDrive()) return false;
    KWin::VirtualDesktop *desktop = desktopId.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(desktopId);
    if (!desktop || desktop == KWin::effects->currentDesktop()) {
        endDesktopPreview();
        return desktopId.isEmpty() || desktop;
    }
    // Another effect holding the screen, such as Overview, keeps it.
    if (KWin::effects->hasActiveFullScreenEffect()
        && KWin::effects->activeFullScreenEffect() != this) return false;
    showDesktopPreview(desktop, nullptr);
    return true;
}

void Effect::showDesktopPreview(KWin::VirtualDesktop *desktop, KWin::EffectWindow *card)
{
    if (!desktop) {
        endDesktopPreview();
        return;
    }
    if (m_previewDesktop == desktop && m_previewCard == card) return;
    if (KWin::effects->hasActiveFullScreenEffect()
        && KWin::effects->activeFullScreenEffect() != this) return;
    // Holding the screen is what makes KWin's switching effects stand aside
    // when the preview is committed (slide.cpp's desktopChanged).
    KWin::effects->setActiveFullScreenEffect(this);
    m_previewDesktop = desktop;
    m_previewCard = card;
    syncDesktopPreview();
    KWin::effects->addRepaintFull();
}

bool Effect::commitDesktopPreview()
{
    if (!callerMayDrive()) return false;
    if (!m_previewDesktop) return false;
    KWin::effects->setCurrentDesktop(m_previewDesktop);
    endDesktopPreview();
    return true;
}

KWin::LogicalOutput *Effect::tableOutput() const
{
    return m_tableOutput ? m_tableOutput.data() : tabletOutput();
}

QRectF Effect::tableDisplay() const
{
    KWin::LogicalOutput *output = tableOutput();
    return output ? QRectF(output->geometry()) : QRectF();
}

qint64 Effect::tableNow() const
{
    return m_tableTime.isValid() ? m_tableTime.elapsed() : 0;
}

QList<Effect::TableWorkspace> Effect::tableWorkspaces()
{
    const auto stack = KWin::effects->stackingOrder();
    // What hangs in a workspace's row: an application's own window, not a
    // dialog, not one on every desktop, and not one kept out of the taskbar,
    // the pager or the task switcher, even when it is a card.
    const auto application = [](KWin::EffectWindow *window) {
        if (!window || window->isDeleted() || !window->window()) return false;
        const KWin::Window *client = window->window();
        return tableListsWindow({
            .normal = window->isNormalWindow(),
            .onAllDesktops = window->isOnAllDesktops(),
            .dependent = dependentLead(window) != nullptr,
            .skipSwitcher = client->skipSwitcher(),
            .skipTaskbar = client->skipTaskbar(),
            .skipPager = client->skipPager(),
        });
    };
    const auto cardOf = [this](KWin::EffectWindow *window) {
        TableCard card;
        card.windows.append(window);
        card.application = applicationDisplayName(window);
        card.title = window->caption();
        card.icon = window->icon();
        return card;
    };
    QList<TableWorkspace> workspaces;
    for (KWin::VirtualDesktop *desktop : KWin::effects->desktops()) {
        TableWorkspace workspace;
        workspace.desktop = desktop;
        workspace.name = desktop->name();
        workspace.named = m_namedDesktops.contains(desktop->id());
        workspace.current = desktop == KWin::effects->currentDesktop();
        QSet<KWin::EffectWindow *> taken;
        const auto found = m_sessions.find(desktop->id());
        if (found != m_sessions.end()) {
            const DesktopSession *session = found->second.get();
            // Each display's cards, the primary display's first: its front
            // card first, then the rest in Spread's order.
            std::vector<const CardStageController *> stages;
            for (const auto &display : session->displays)
                if (display.first == m_cardOutputName || display.first.isEmpty())
                    stages.insert(stages.begin(), display.second->cards.get());
                else stages.push_back(display.second->cards.get());
            for (const CardStageController *cards : stages) {
                KWin::EffectWindow *front = cards->isActive() ? cards->selectedWindow() : nullptr;
                QList<KWin::EffectWindow *> ordered;
                if (front) ordered.append(front);
                for (const auto &window : cards->liveCards())
                    if (window && window != front) ordered.append(window);
                for (KWin::EffectWindow *window : std::as_const(ordered)) {
                    if (!window || window->isDeleted() || taken.contains(window)) continue;
                    // A stack is one card, its face in front, of the members
                    // Table lists.
                    QList<KWin::EffectWindow *> deck = cards->stackFrontToBack(window);
                    if (deck.isEmpty()) deck.append(window);
                    for (KWin::EffectWindow *member : std::as_const(deck)) taken.insert(member);
                    deck.removeIf([&application](KWin::EffectWindow *member) { return !application(member); });
                    if (deck.isEmpty()) continue;
                    TableCard card = cardOf(deck.first());
                    for (KWin::EffectWindow *member : std::as_const(deck))
                        if (member != card.windows.first()) card.windows.append(member);
                    card.stacked = int(card.windows.size()) - 1;
                    workspace.cards.append(card);
                }
            }
            // A layout's panes are cards of their own.
            for (const auto &view : session->layouts->ownershipView()) {
                for (const quintptr pane : view.panes) {
                    auto *window = reinterpret_cast<KWin::EffectWindow *>(pane);
                    if (!window || window->isDeleted() || taken.contains(window) || !application(window)) continue;
                    workspace.cards.append(cardOf(window));
                    taken.insert(window);
                }
            }
        }
        // A stack carried here whose windows are not cards yet is one card.
        const auto pending = m_pendingDecks.value(desktop->id());
        for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
            KWin::EffectWindow *window = *it;
            if (!application(window) || taken.contains(window) || !window->isOnDesktop(desktop)) continue;
            QList<KWin::EffectWindow *> deck;
            for (const auto &carried : pending) {
                if (!carried.contains(window)) continue;
                for (const auto &member : carried)
                    if (member && application(member) && !taken.contains(member) && member->isOnDesktop(desktop))
                        deck.append(member);
            }
            TableCard card = cardOf(deck.isEmpty() ? window : deck.first());
            for (KWin::EffectWindow *member : std::as_const(deck))
                if (member != card.windows.first()) card.windows.append(member);
            card.stacked = int(card.windows.size()) - 1;
            for (const auto &member : std::as_const(card.windows)) taken.insert(member);
            workspace.cards.append(card);
        }
        // A workspace nothing has named shows the name of the application it
        // holds first, as one made by carrying a card to + does.
        if (unnamedDesktop(workspace.name) && !workspace.cards.isEmpty())
            workspace.name = workspace.cards.first().application;
        if (unnamedDesktop(workspace.name)) workspace.name.clear();
        workspaces.append(workspace);
    }
    return workspaces;
}

void Effect::readTableWorkspaces()
{
    m_tableWorkspaces = tableWorkspaces();
    // The name each unnamed workspace shows becomes its own, saved once.
    bool renamed = false;
    for (const TableWorkspace &workspace : std::as_const(m_tableWorkspaces)) {
        if (!workspace.desktop || workspace.name.isEmpty()
            || !unnamedDesktop(workspace.desktop->name())) continue;
        workspace.desktop->setName(workspace.name);
        renamed = true;
    }
    if (renamed) KWin::VirtualDesktopManager::self()->save();
}

void Effect::relayoutTable()
{
    QList<TableTabContent> tabs;
    QList<QList<TableCardContent>> cards;
    for (const auto &workspace : std::as_const(m_tableWorkspaces)) {
        const bool editing = m_tableRenaming == tabs.size();
        tabs.append({editing ? m_tableRenameText : workspace.name, workspace.current,
            int(std::min<qsizetype>(workspace.cards.size(), 4)), int(tabs.size()) + 1, editing});
        QList<TableCardContent> own;
        for (const auto &card : workspace.cards) own.append({card.application, card.title, card.stacked});
        cards.append(own);
    }
    // KDE holds at most 25 desktops; at that, + goes.
    auto *manager = KWin::VirtualDesktopManager::self();
    const bool plus = manager->count() < manager->maximum();
    // A name cleared while it is typed offers to remove the workspace; KDE
    // keeps at least one.
    const bool removable = m_tableRenaming >= 0 && m_tableRenameText.trimmed().isEmpty() && manager->count() > 1;
    m_tableLayout = layoutTable(tableDisplay().size(), tabs, cards, m_table.carrying() ? tr("New") : QString(),
        tr("Empty. It dissolves when you leave."),
        TableMetrics{&TablePresenter::measure}, plus, removable ? tr("Remove workspace") : QString());
    QList<double> tabCentres;
    for (const QRectF &tab : std::as_const(m_tableLayout.tabs)) tabCentres.append(tab.center().x());
    QList<QList<double>> cardCentres;
    for (const auto &row : std::as_const(m_tableLayout.cards)) {
        QList<double> centres;
        for (const QRectF &card : row) centres.append(card.center().x());
        cardCentres.append(centres);
    }
    m_table.setLayout(tabCentres,
        plus ? m_tableLayout.plus.center().x() : std::numeric_limits<double>::quiet_NaN(), cardCentres);
    // Past the line just under the tabs the cards hang, past the one just
    // under the cards a card lifts, and above the tabs a lift cancels.
    const double height = tableDisplay().height();
    if (height > 0)
        m_table.setThresholds(m_tableLayout.depth / height, m_tableLayout.tabsTop / height, m_tableLayout.lift / height);
}

void Effect::refreshTable()
{
    if (!m_table.isOpen()) return;
    KWin::LogicalOutput *output = tableOutput();
    if (!output) {
        closeTable();
        return;
    }
    const QRectF display = tableDisplay();
    const int shown = m_table.shownWorkspace();
    const bool shownValid = shown >= 0 && shown < m_tableWorkspaces.size();
    KWin::EffectWindow *shownCard = nullptr;
    if (shownValid && m_table.shownCard() >= 0 && m_table.shownCard() < m_tableWorkspaces[shown].cards.size()) {
        const TableCard &card = m_tableWorkspaces[shown].cards[m_table.shownCard()];
        if (!card.windows.isEmpty()) shownCard = card.windows.first();
    }
    // Over Spread, the tabs leave Spread showing: a pull previews once it
    // reaches a workspace's cards, and a menu bar once a tap chooses
    // another workspace's tab.
    const bool keepSpread = m_tableOverSpread && m_table.level() == TableLevel::Tabs
        && (m_table.scrubbing() || shown == m_table.current());
    showDesktopPreview(shownValid && !keepSpread ? m_tableWorkspaces[shown].desktop.data() : nullptr,
        shownCard);

    QVariantMap model;
    model[QStringLiteral("row")] = m_tableLayout.row;
    model[QStringLiteral("cancelling")] = m_table.cancelling();
    model[QStringLiteral("carrying")] = m_table.carrying();
    // Float draws no line: the cards come up to meet a
    // finger nearing them, and the chosen one leans toward the lift line as
    // it is pulled there.
    const QPointF finger = m_table.finger();
    const double tabsBottom = m_tableLayout.tabsTop + m_tableLayout.row;
    double approach = 1.0;
    double lean = 0.0;
    if (m_table.scrubbing() && !m_table.carrying()) {
        if (m_table.level() == TableLevel::Tabs) {
            approach = std::clamp((finger.y() - tabsBottom) / std::max(1.0, m_tableLayout.depth - tabsBottom), 0.0, 1.0);
        } else if (m_table.card() >= 0) {
            const double middle = m_tableLayout.trayTop + m_tableLayout.cardHeight / 2.0;
            lean = std::clamp((finger.y() - middle) / std::max(1.0, m_tableLayout.lift - middle), 0.0, 1.0);
        }
    }
    model[QStringLiteral("approach")] = approach;
    model[QStringLiteral("lean")] = lean;
    // The finger quiets the screen as it pulls: during a
    // stroke the band's darkness follows how far it has come, full by the
    // tabs; a flick, the corner and the key bring it in on its own.
    model[QStringLiteral("scrubbing")] = m_table.scrubbing();
    model[QStringLiteral("pull")] = m_table.scrubbing()
        ? std::clamp(finger.y() / std::max(1.0, tabsBottom), 0.0, 1.0) : 1.0;
    // Letting go at the edge changes nothing, and says so beneath the tabs.
    // While a name is typed, the rule it serves is said beneath the tabs.
    model[QStringLiteral("hint")] = m_table.cancelling() ? tr("Let go to cancel")
        : m_tableRenaming >= 0 ? tr("A named workspace stays, even empty") : QString();
    model[QStringLiteral("hintY")] = tabsBottom + 12.0;
    switch (m_tableLayout.parts) {
    case TableTabParts::All: model[QStringLiteral("parts")] = QStringLiteral("all"); break;
    case TableTabParts::NoColours: model[QStringLiteral("parts")] = QStringLiteral("noColours"); break;
    case TableTabParts::NamesOnly: model[QStringLiteral("parts")] = QStringLiteral("namesOnly"); break;
    case TableTabParts::Numbers: model[QStringLiteral("parts")] = QStringLiteral("numbers"); break;
    }
    QVariantList tabs;
    for (int i = 0; i < m_tableWorkspaces.size() && i < m_tableLayout.tabs.size(); ++i) {
        const TableWorkspace &workspace = m_tableWorkspaces[i];
        const QRectF rect = m_tableLayout.tabs[i];
        QString state = QStringLiteral("idle");
        if (m_table.carrying() && m_table.over() == TableGesture::Over::Workspace && m_table.overWorkspace() == i)
            state = QStringLiteral("target");
        else if (m_table.level() == TableLevel::Cards && m_table.locked() == i) state = QStringLiteral("locked");
        else if (m_table.hovered() == i) state = QStringLiteral("hover");
        QVariantList icons;
        for (int c = 0; c < workspace.cards.size() && c < 4; ++c) icons.append(QVariant::fromValue(workspace.cards[c].icon));
        tabs.append(QVariantMap{
            {QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()},
            {QStringLiteral("name"), workspace.name}, {QStringLiteral("nameWidth"), m_tableLayout.nameWidths.value(i)},
            {QStringLiteral("number"), i + 1},
            {QStringLiteral("current"), workspace.current},
            {QStringLiteral("state"), state}, {QStringLiteral("icons"), icons},
            {QStringLiteral("editing"), m_tableRenaming == i},
        });
    }
    model[QStringLiteral("tabs")] = tabs;
    const QRectF remove = m_tableLayout.remove;
    model[QStringLiteral("remove")] = QVariantMap{
        {QStringLiteral("visible"), !remove.isEmpty()},
        {QStringLiteral("x"), remove.x()}, {QStringLiteral("y"), remove.y()},
        {QStringLiteral("width"), remove.width()}, {QStringLiteral("height"), remove.height()},
        {QStringLiteral("label"), tr("Remove workspace")},
    };
    const QRectF plus = m_tableLayout.plus;
    model[QStringLiteral("plus")] = QVariantMap{
        {QStringLiteral("visible"), !plus.isEmpty()},
        {QStringLiteral("x"), plus.x()}, {QStringLiteral("y"), plus.y()},
        {QStringLiteral("width"), plus.width()}, {QStringLiteral("height"), plus.height()},
        {QStringLiteral("label"), m_table.carrying() ? tr("New") : QString()},
        {QStringLiteral("hover"), m_table.onPlus()},
        {QStringLiteral("target"), m_table.carrying() && m_table.over() == TableGesture::Over::Plus},
    };
    const int tray = m_table.trayWorkspace();
    QVariantList cards;
    if (tray >= 0 && tray < m_tableWorkspaces.size() && tray < m_tableLayout.cards.size()) {
        const auto &rects = m_tableLayout.cards[tray];
        for (int i = 0; i < rects.size() && i < m_tableWorkspaces[tray].cards.size(); ++i) {
            const TableCard &card = m_tableWorkspaces[tray].cards[i];
            cards.append(QVariantMap{
                {QStringLiteral("x"), rects[i].x()}, {QStringLiteral("y"), rects[i].y()},
                {QStringLiteral("width"), rects[i].width()}, {QStringLiteral("height"), rects[i].height()},
                {QStringLiteral("application"), card.application}, {QStringLiteral("title"), card.title},
                {QStringLiteral("textWidth"), m_tableLayout.textWidths[tray].value(i)},
                {QStringLiteral("icon"), QVariant::fromValue(card.icon)},
                {QStringLiteral("stacked"), std::min(card.stacked, TableSizes::SliversShown)},
                {QStringLiteral("chosen"), m_table.level() == TableLevel::Cards && m_table.card() == i},
                {QStringLiteral("carried"), m_table.carrying() && m_table.carryFrom() == tray && m_table.carryCard() == i},
            });
        }
        if (m_tableWorkspaces[tray].cards.isEmpty()) {
            const QRectF empty = m_tableLayout.empty.value(tray);
            model[QStringLiteral("empty")] = QVariantMap{
                {QStringLiteral("visible"), true}, {QStringLiteral("x"), empty.x()}, {QStringLiteral("y"), empty.y()},
                {QStringLiteral("width"), empty.width()}, {QStringLiteral("height"), empty.height()},
                {QStringLiteral("text"), m_tableWorkspaces[tray].named ? tr("Empty. Named, so it stays.")
                                                                         : tr("Empty. It dissolves when you leave.")},
            };
        }
    }
    model[QStringLiteral("cards")] = cards;
    // The card in hand is the card itself, hanging from the finger by its top
    // edge and never higher than just under the tabs, so the tab it is carried
    // to stays in view.
    const int from = m_table.carryFrom();
    const int held = m_table.carryCard();
    if (m_table.carrying() && from >= 0 && from < m_tableWorkspaces.size() && from < m_tableLayout.cards.size()
        && held >= 0 && held < m_tableWorkspaces[from].cards.size() && held < m_tableLayout.cards[from].size()) {
        const TableCard &card = m_tableWorkspaces[from].cards[held];
        const QRectF rect = m_tableLayout.cards[from][held];
        m_tableCarried = QVariantMap{
            {QStringLiteral("x"), finger.x() - rect.width() / 2.0},
            {QStringLiteral("y"), std::max(finger.y() - 12.0, tabsBottom + 8.0)},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()},
            {QStringLiteral("textWidth"), m_tableLayout.textWidths[from].value(held)},
            {QStringLiteral("application"), card.application}, {QStringLiteral("title"), card.title},
            {QStringLiteral("icon"), QVariant::fromValue(card.icon)},
            {QStringLiteral("stacked"), std::min(card.stacked, TableSizes::SliversShown)},
        };
        model[QStringLiteral("carried")] = m_tableCarried;
    }
    model[QStringLiteral("drop")] = m_tableDrop;
    if (!m_tablePresenter) m_tablePresenter = std::make_unique<TablePresenter>();
    m_tablePresenterOutput = output;
    m_tablePresenter->show(display.toRect(), output->scale(), model);
    m_tableLine = tableLine();
    syncSpokenLine();
}

void Effect::openTable(bool sticky, KWin::LogicalOutput *output)
{
    if (m_table.isOpen()) return;
    m_tableOutput = output ? output : tabletOutput();
    if (!m_tableOutput) return;
    // Nothing begun elsewhere may still be in hand.
    if (m_carryRuntime) m_carryRuntime->cancel();
    readTableWorkspaces();
    int current = -1;
    for (int i = 0; i < m_tableWorkspaces.size(); ++i)
        if (m_tableWorkspaces[i].current) current = i;
    if (current < 0) return;
    m_tableTime.start();
    m_tableOverSpread = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Spread;
    m_table.open(current, tableNow(), sticky);
    m_tableKeyboard = KWin::effects->grabKeyboard(this);
    quietCardGap(KWin::effects->cursorPos());
    relayoutTable();
    refreshTable();
    // A Table button elsewhere, the dock's among them, shows Table open.
    Q_EMIT workspaceContextChanged();
}

void Effect::closeTable()
{
    m_table.close();
    finishTable();
}

void Effect::finishTable()
{
    m_tableHold.stop();
    endTableRename(false);
    if (m_tableKeyboard) KWin::effects->ungrabKeyboard();
    m_tableKeyboard = false;
    m_tableOutput.clear();
    m_tableOverSpread = false;
    endDesktopPreview();
    m_tableCarried.clear();
    m_tableDrop.clear();
    if (m_tablePresenter) m_tablePresenter->close();
    quietCardGap(KWin::effects->cursorPos());
    scheduleDissolve();
    Q_EMIT workspaceContextChanged();
}

int Effect::tableCardAt(const QPointF &local) const
{
    const int tray = m_table.trayWorkspace();
    if (tray < 0 || tray >= m_tableLayout.cards.size()) return -1;
    const auto &cards = m_tableLayout.cards[tray];
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i].contains(local)) return i;
    return -1;
}

TableHit Effect::tableHitAt(const QPointF &local) const
{
    TableHit hit;
    for (int i = 0; i < m_tableLayout.tabs.size(); ++i)
        if (m_tableLayout.tabs[i].contains(local)) hit.tab = i;
    hit.plus = !m_tableLayout.plus.isEmpty() && m_tableLayout.plus.contains(local);
    hit.card = tableCardAt(local);
    const int tray = m_table.trayWorkspace();
    const bool onEmpty = tray >= 0 && m_tableLayout.empty.value(tray).contains(local);
    hit.insideRows = hit.tab >= 0 || hit.plus || hit.card >= 0 || onEmpty
        || (local.y() >= m_tableLayout.tabsTop && local.y() <= m_tableLayout.tabsTop + m_tableLayout.row);
    return hit;
}

bool Effect::aboveActiveCardForInput(const QPointF &position) const
{
    if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Active) return false;
    const KWin::EffectWindow *front = m_cardStage->selectedWindow();
    return front && !front->isDeleted() && isTabletPoint(position)
        && position.y() < front->frameGeometry().top();
}

void Effect::beginTableFromInput(const QPointF &position)
{
    // Table comes down where it is pulled, so the finger stays on its tabs:
    // from a touchscreen's own top edge, on that touchscreen's display.
    openTable(false, KWin::effects->screenAt(position.toPoint()));
    m_tableStrokeFrom = position;
}

void Effect::pressTableFromInput(const QPointF &position, bool touch)
{
    // While a name is typed, a press only says where the next lift is.
    if (m_tableRenaming >= 0) {
        m_tableRenamePress = true;
        return;
    }
    const QPointF local = position - tableDisplay().topLeft();
    const TableHit hit = tableHitAt(local);
    m_tablePressTouch = touch;
    m_table.press(local, tableCardAt(local), hit.tab, tableNow());
    if (m_table.sticky() && hit.tab >= 0) m_tableHold.start(m_table.config().holdMs);
    else m_tableHold.stop();
    refreshTable();
}

void Effect::contextTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen()) return;
    if (m_tableRenaming >= 0) {
        endTableRename(true);
        return;
    }
    const TableAction action = m_table.contextOn(tableHitAt(position - tableDisplay().topLeft()).tab);
    if (action.kind == TableAction::Kind::Rename) beginTableRename(action.workspace, false);
}

void Effect::moveTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen() || m_tableRenaming >= 0) return;
    const QRectF display = tableDisplay();
    const QPointF local = position - display.topLeft();
    const bool carrying = m_table.carrying();
    m_table.move(local, display.height() > 0 ? local.y() / display.height() : 0.0);
    if (!carrying && m_table.carrying()) {
        const int from = m_table.carryFrom();
        const int card = m_table.carryCard();
        if (from < 0 || from >= m_tableWorkspaces.size() || card < 0 || card >= m_tableWorkspaces[from].cards.size()) {
            m_table.refuseCarry();
        }
    }
    if (carrying != m_table.carrying()) relayoutTable();
    refreshTable();
}

void Effect::releaseTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen()) return;
    m_tableHold.stop();
    const QPointF local = position - tableDisplay().topLeft();
    if (m_tableRenaming >= 0) {
        // The finger that held the tab lets go and the name waits to be
        // typed. A later tap on Remove removes the workspace, on the tab
        // leaves the typing alone, and anywhere else keeps what was typed.
        if (!m_tableRenamePress) {
            (void)m_table.release(local, tableNow());
            return;
        }
        m_tableRenamePress = false;
        if (!m_tableLayout.remove.isEmpty() && m_tableLayout.remove.contains(local)) removeTableWorkspace(m_tableRenaming);
        else if (!m_tableLayout.tabs.value(m_tableRenaming).contains(local)) endTableRename(true);
        return;
    }
    const bool carrying = m_table.carrying();
    const bool stroke = m_table.scrubbing();
    const bool onCards = m_table.level() == TableLevel::Cards;
    const qint64 held = tableNow();
    const TableAction action = m_table.release(local, held, tableHitAt(local));
    // Each stroke says how it went, so one that surprised the hand can be
    // read back against the lines it crossed.
    if (stroke) {
        qInfo() << "Kadunce Table stroke" << held << "ms," << (position.y() - m_tableStrokeFrom.y())
                << "px down, let go" << (carrying ? "carrying a card" : onCards ? "on the cards" : "on the tabs")
                << "with the cards line at" << m_tableLayout.depth << "and the lift line at" << m_tableLayout.lift
                << ":" << tableActionName(action.kind);
    }
    if (carrying && m_table.isOpen()) relayoutTable();
    applyTableAction(action);
}

void Effect::cancelTableFromInput()
{
    closeTable();
}

void Effect::hoverTableFromInput(const QPointF &position)
{
    if (!m_table.isOpen() || !m_table.sticky()) return;
    const TableHit hit = tableHitAt(position - tableDisplay().topLeft());
    if (hit.tab < 0 && hit.card < 0 && !hit.plus) return;
    const int shown = m_table.shownWorkspace();
    const int card = m_table.shownCard();
    const TableLevel level = m_table.level();
    const bool onPlus = m_table.onPlus();
    m_table.hover(hit.tab, hit.tab >= 0 ? -1 : hit.card, hit.plus);
    if (shown != m_table.shownWorkspace() || card != m_table.shownCard() || level != m_table.level()
        || onPlus != m_table.onPlus())
        refreshTable();
}

void Effect::wheelTableFromInput(int steps)
{
    if (!m_table.isOpen() || m_table.down()) return;
    m_table.step(steps);
    refreshTable();
}

void Effect::pointerMovedForInput(const QPointF &position)
{
    m_pointerResting = false;
    quietCardGap(position);
    tracePointerAtTop(position);
    // Each arrival at a display's top-left corner says whether Table opened,
    // so a push that did not open it can be read back.
    KWin::LogicalOutput *corner = nullptr;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const KWin::Rect geometry = output->geometry();
        if (std::abs(position.x() - geometry.x()) < 3.0 && std::abs(position.y() - geometry.y()) < 3.0) corner = output;
    }
    if (corner == m_pointerCorner) return;
    m_pointerCorner = corner;
    if (!corner) return;
    qInfo() << "Kadunce pointer reached the top-left corner of" << corner->name() << "at" << position
            << (m_table.isOpen() ? "with Table open" : "");
    QTimer::singleShot(600, this, [this] {
        qInfo() << "Kadunce Table" << (m_table.isOpen() ? "is open" : "did not open")
                << "0.6 s after the pointer reached a corner; it is at" << KWin::effects->cursorPos();
    });
}

void Effect::tracePointerAtTop(const QPointF &position)
{
    // Nothing is read for a log no one asked for.
    if (!KaduncePointer().isInfoEnabled()) return;
    // Near a display's top edge, each change in what the pointer is over, or
    // in the image it shows, is logged, so a pointer changing shape there can
    // be read back. A client sets its own pointer after
    // the motion that asked for it, so a change shows on the next motion.
    bool nearTop = false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const KWin::Rect geometry = output->geometry();
        if (position.x() >= geometry.x() && position.x() < geometry.x() + geometry.width()
            && position.y() >= geometry.y() && position.y() < geometry.y() + 40.0) nearTop = true;
    }
    if (!nearTop) {
        m_topPointer.clear();
        return;
    }
    KWin::Window *under = KWin::input()->findToplevel(position);
    const KWin::PlatformCursorImage cursor = KWin::effects->cursorImage();
    const QImage image = cursor.image();
    const size_t picture = image.isNull() ? 0 : qHashBits(image.constBits(), size_t(image.sizeInBytes()));
    const QByteArray border = under ? borderCursorName(under) : QByteArray();
    const QString seen = QStringLiteral("%1 %2 %3").arg(quintptr(under)).arg(QString::fromLatin1(border)).arg(picture);
    // An animated pointer changes its image by itself, so the log is held
    // to one line in 150 ms.
    if (seen == m_topPointer || (m_topPointerLogged.isValid() && m_topPointerLogged.elapsed() < 150)) return;
    m_topPointer = seen;
    m_topPointerLogged.start();
    const bool inside = under && under->frameGeometry().contains(position);
    qCInfo(KaduncePointer).nospace() << "Kadunce pointer near the top at " << position << " is over "
                      << (under ? logNameOf(under) : QStringLiteral("nothing"))
                      << (under ? (inside ? " inside its frame " : " outside its frame, in its input margin ") : " ")
                      << (under ? under->frameGeometry() : KWin::RectF())
                      << "; its border asks for " << (border.isEmpty() ? QByteArrayLiteral("no shape") : border)
                      << "; the pointer shows image " << picture << " with hot spot " << cursor.hotSpot()
                      << " at " << image.size();
}

void Effect::followTableGesture(qreal progress, KWin::LogicalOutput *output)
{
    if (m_tableGestureDone || progress <= 0.5) return;
    m_tableGestureDone = true;
    if (m_table.isOpen()) {
        if (!m_table.down()) closeTable();
        return;
    }
    openTable(true, output);
}

void Effect::toggleTable()
{
    if (!callerMayDrive()) return;
    if (m_table.isOpen()) {
        if (!m_table.down()) closeTable();
        return;
    }
    openTable(true, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
}

bool Effect::borderActivated(KWin::ElectricBorder border)
{
    if (border != KWin::ElectricTopLeft) return false;
    qInfo() << "Kadunce Table's corner fired at" << KWin::effects->cursorPos();
    if (!m_table.isOpen())
        openTable(true, KWin::effects->screenAt(KWin::effects->cursorPos().toPoint()));
    return true;
}

void Effect::grabbedKeyboardEvent(QKeyEvent *event)
{
    if (!m_table.isOpen()) return;
    if (m_tableRenaming >= 0) {
        // Enter keeps the name typed, Escape leaves the old one, and every
        // other key is the name's.
        const bool press = event->type() == QEvent::KeyPress;
        if (press && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) endTableRename(true);
        else if (press && event->key() == Qt::Key_Escape) endTableRename(false);
        else if (m_tablePresenter) m_tablePresenter->forwardKey(event);
        return;
    }
    if (event->type() != QEvent::KeyPress) return;
    using Key = TableGesture::Key;
    std::optional<Key> key;
    switch (event->key()) {
    case Qt::Key_Left: key = Key::Left; break;
    case Qt::Key_Right: key = Key::Right; break;
    case Qt::Key_Up: key = Key::Up; break;
    case Qt::Key_Down: key = Key::Down; break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: key = Key::Enter; break;
    case Qt::Key_Escape: key = Key::Escape; break;
    case Qt::Key_F2: key = Key::Rename; break;
    default: break;
    }
    // Keys wait while a finger or a button is on Table.
    if (!key || m_table.down()) return;
    applyTableAction(m_table.key(*key));
}

void Effect::moveTableCard(int workspace, int card, KWin::VirtualDesktop *destination)
{
    if (!destination || workspace < 0 || workspace >= m_tableWorkspaces.size()
        || card < 0 || card >= m_tableWorkspaces[workspace].cards.size()) return;
    const TableCard moved = m_tableWorkspaces[workspace].cards[card];
    // A stack goes whole and is stacked again there, as it left. Its windows
    // leave this desktop's cards as any window does;
    // they are that desktop's cards once it is shown.
    for (auto &decks : m_pendingDecks)
        decks.removeIf([&moved](const auto &deck) {
            return std::any_of(deck.cbegin(), deck.cend(), [&moved](const auto &window) { return moved.windows.contains(window); });
        });
    if (moved.stacked > 0) m_pendingDecks[destination->id()].append(moved.windows);
    for (const auto &window : moved.windows)
        if (window && !window->isDeleted()) KWin::effects->windowToDesktops(window, {destination});
    readTableWorkspaces();
    int destinationIndex = -1;
    int cardIndex = -1;
    for (int i = 0; i < m_tableWorkspaces.size(); ++i) {
        if (m_tableWorkspaces[i].desktop != destination) continue;
        destinationIndex = i;
        for (int c = 0; c < m_tableWorkspaces[i].cards.size(); ++c)
            if (!moved.windows.isEmpty() && m_tableWorkspaces[i].cards[c].windows.contains(moved.windows.first())) cardIndex = c;
    }
    relayoutTable();
    if (destinationIndex >= 0) m_table.showAfterMove(destinationIndex, cardIndex);
    // The card lands from where it was let go into its place in the row it
    // joined, which Table now shows.
    if (!m_tableCarried.isEmpty() && destinationIndex >= 0 && destinationIndex < m_tableLayout.cards.size()
        && cardIndex >= 0 && cardIndex < m_tableLayout.cards[destinationIndex].size()) {
        const QRectF slot = m_tableLayout.cards[destinationIndex][cardIndex];
        QVariantMap drop = m_tableCarried;
        drop[QStringLiteral("seq")] = ++m_tableDrops;
        drop[QStringLiteral("toX")] = slot.x();
        drop[QStringLiteral("toY")] = slot.y();
        drop[QStringLiteral("card")] = cardIndex;
        m_tableDrop = drop;
    }
    m_tableCarried.clear();
    refreshTable();
}

void Effect::applyTableAction(const TableAction &action)
{
    const auto desktopAt = [this](int index) -> KWin::VirtualDesktop * {
        return index >= 0 && index < m_tableWorkspaces.size() ? m_tableWorkspaces[index].desktop.data() : nullptr;
    };
    switch (action.kind) {
    case TableAction::Kind::None:
    case TableAction::Kind::StayOpen:
        refreshTable();
        return;
    case TableAction::Kind::Close:
        finishTable();
        return;
    case TableAction::Kind::Enter: {
        KWin::VirtualDesktop *desktop = desktopAt(action.workspace);
        if (desktop && desktop != KWin::effects->currentDesktop()) {
            showDesktopPreview(desktop, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        return;
    }
    case TableAction::Kind::Activate: {
        KWin::VirtualDesktop *desktop = desktopAt(action.workspace);
        QPointer<KWin::EffectWindow> window;
        if (desktop && action.card >= 0 && action.card < m_tableWorkspaces[action.workspace].cards.size()
            && !m_tableWorkspaces[action.workspace].cards[action.card].windows.isEmpty())
            window = m_tableWorkspaces[action.workspace].cards[action.card].windows.first();
        if (desktop && desktop != KWin::effects->currentDesktop()) {
            showDesktopPreview(desktop, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        // Active in its own workspace (§6: selecting a card makes it Active).
        if (window && !window->isDeleted()) KWin::effects->activateWindow(window);
        return;
    }
    case TableAction::Kind::Move:
        moveTableCard(action.workspace, action.card, desktopAt(action.destination));
        return;
    case TableAction::Kind::Create: {
        if (action.workspace < 0 || action.workspace >= m_tableWorkspaces.size()
            || action.card < 0 || action.card >= m_tableWorkspaces[action.workspace].cards.size()) {
            refreshTable();
            return;
        }
        // Named after the application of the card that made it.
        auto *manager = KWin::VirtualDesktopManager::self();
        KWin::VirtualDesktop *created = manager->createVirtualDesktop(
            manager->count(), m_tableWorkspaces[action.workspace].cards[action.card].application);
        if (!created) {
            m_table.showAfterMove(action.workspace, action.card);
            refreshTable();
            return;
        }
        // Table's own workspace dissolves as any left one does.
        m_unenteredDesktops.remove(created->id());
        moveTableCard(action.workspace, action.card, created);
        return;
    }
    case TableAction::Kind::New: {
        // KDE names it; its tab is its number until an application names it.
        auto *manager = KWin::VirtualDesktopManager::self();
        if (KWin::VirtualDesktop *created = manager->createVirtualDesktop(manager->count())) {
            showDesktopPreview(created, nullptr);
            commitDesktopPreview();
        }
        finishTable();
        return;
    }
    case TableAction::Kind::Rename:
        beginTableRename(action.workspace, false);
        return;
    }
}

void Effect::beginTableRename(int workspace, bool touch)
{
    if (workspace < 0 || workspace >= m_tableWorkspaces.size() || !m_tableWorkspaces[workspace].desktop) return;
    m_tableRenaming = workspace;
    m_tableRenamePress = false;
    m_tableRenameByTouch = touch;
    m_tableRenameText = m_tableWorkspaces[workspace].name;
    relayoutTable();
    refreshTable();
    if (!m_tablePresenter) return;
    // KWin gives the keys' text to the focused application's field first;
    // while a name is typed that application has none.
    auto *seat = KWin::waylandServer()->seat();
    m_tableTextFocus = seat->focusedTextInputSurface();
    seat->setFocusedTextInputSurface(nullptr);
    if (m_spokenLine) m_spokenLine->hide();
    m_tablePresenter->beginEditing(m_tableRenameText, [this](const QString &typed) {
        if (m_tableRenaming < 0) return;
        m_tableRenameText = typed;
        relayoutTable();
        refreshTable();
    });
    // A finger held the tab, so the keys come up, as the person asked for
    // them; a mouse or the keyboard has keys already.
    if (touch) raiseKeyboard();
    quietCardGap(KWin::effects->cursorPos());
}

void Effect::endTableRename(bool commit)
{
    if (m_tableRenaming < 0) return;
    const int workspace = m_tableRenaming;
    const QString typed = (m_tablePresenter ? m_tablePresenter->editedText() : m_tableRenameText).trimmed();
    m_tableRenaming = -1;
    m_tableRenamePress = false;
    m_tableRenameByTouch = false;
    m_tableRenameText.clear();
    if (m_tablePresenter) m_tablePresenter->endEditing();
    quietCardGap(KWin::effects->cursorPos());
    auto *seat = KWin::waylandServer()->seat();
    if (!seat->focusedTextInputSurface() && m_tableTextFocus && m_tableTextFocus == seat->focusedKeyboardSurface())
        seat->setFocusedTextInputSurface(m_tableTextFocus);
    m_tableTextFocus.clear();
    KWin::VirtualDesktop *desktop = workspace < m_tableWorkspaces.size() ? m_tableWorkspaces[workspace].desktop.data() : nullptr;
    if (commit && desktop) {
        // A name the person gives keeps the workspace, even one kept as it
        // was; cleared, it goes back to its application's name and keeps
        // it no longer.
        if (!typed.isEmpty()) {
            desktop->setName(typed);
            if (!m_namedDesktops.contains(desktop->id())) m_namedDesktops.append(desktop->id());
        } else {
            desktop->setName(QString());
            m_namedDesktops.removeAll(desktop->id());
        }
        KWin::VirtualDesktopManager::self()->save();
        saveNamedDesktops();
    }
    if (!m_table.isOpen()) return;
    readTableWorkspaces();
    relayoutTable();
    refreshTable();
}

void Effect::removeTableWorkspace(int workspace)
{
    auto *manager = KWin::VirtualDesktopManager::self();
    KWin::VirtualDesktop *desktop = workspace >= 0 && workspace < m_tableWorkspaces.size()
        ? m_tableWorkspaces[workspace].desktop.data() : nullptr;
    if (!desktop || manager->count() <= 1) {
        endTableRename(false);
        return;
    }
    endTableRename(false);
    m_namedDesktops.removeAll(desktop->id());
    saveNamedDesktops();
    // KDE moves its windows to the workspace that takes its place, or the one
    // before it when it was the last, and closes Table as a desktop goes;
    // Table comes back as the menu bar it was, on the same display.
    const QPointer<KWin::LogicalOutput> output = m_tableOutput;
    endDesktopPreview();
    manager->removeVirtualDesktop(desktop);
    if (!m_table.isOpen() && output) openTable(true, output);
}

QString Effect::tableState() const
{
    QJsonArray workspaces;
    for (const auto &workspace : m_tableWorkspaces) {
        QJsonArray cards;
        QJsonArray titles;
        QJsonArray stacked;
        for (const auto &card : workspace.cards) {
            cards.append(card.application);
            titles.append(card.title);
            stacked.append(card.stacked);
        }
        workspaces.append(QJsonObject{
            {QStringLiteral("id"), workspace.desktop ? workspace.desktop->id() : QString()},
            {QStringLiteral("name"), workspace.name},
            {QStringLiteral("named"), workspace.named},
            {QStringLiteral("current"), workspace.current},
            {QStringLiteral("cards"), cards},
            {QStringLiteral("titles"), titles},
            {QStringLiteral("stacked"), stacked},
        });
    }
    const auto centre = [](const QRectF &rect) {
        return QJsonArray{rect.center().x(), rect.center().y()};
    };
    QJsonArray tabs;
    for (const QRectF &rect : m_tableLayout.tabs) tabs.append(centre(rect));
    QJsonArray tray;
    const int trayWorkspace = m_table.trayWorkspace();
    if (trayWorkspace >= 0 && trayWorkspace < m_tableLayout.cards.size())
        for (const QRectF &rect : m_tableLayout.cards[trayWorkspace]) tray.append(centre(rect));
    QJsonArray allCards;
    for (const auto &row : m_tableLayout.cards) {
        QJsonArray centres;
        for (const QRectF &rect : row) centres.append(centre(rect));
        allCards.append(centres);
    }
    const QJsonObject layout{
        {QStringLiteral("tabs"), tabs},
        {QStringLiteral("plus"), centre(m_tableLayout.plus)},
        {QStringLiteral("remove"), m_tableLayout.remove.isEmpty() ? QJsonValue() : QJsonValue(centre(m_tableLayout.remove))},
        {QStringLiteral("tray"), tray},
        {QStringLiteral("cards"), allCards},
        {QStringLiteral("depth"), m_tableLayout.depth},
        {QStringLiteral("trayTop"), m_tableLayout.trayTop},
        {QStringLiteral("lift"), m_tableLayout.lift},
    };
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("layout"), layout},
        {QStringLiteral("open"), m_table.isOpen()},
        // Where the layout's display starts, for a pointer on another display.
        {QStringLiteral("origin"), QJsonArray{tableDisplay().x(), tableDisplay().y()}},
        {QStringLiteral("keys"), m_tableKeyboard},
        {QStringLiteral("sticky"), m_table.sticky()},
        {QStringLiteral("level"), m_table.level() == TableLevel::Cards ? QStringLiteral("cards") : QStringLiteral("tabs")},
        {QStringLiteral("hovered"), m_table.hovered()},
        {QStringLiteral("locked"), m_table.locked()},
        {QStringLiteral("card"), m_table.card()},
        {QStringLiteral("carrying"), m_table.carrying()},
        {QStringLiteral("cancelling"), m_table.cancelling()},
        {QStringLiteral("preview"), m_previewDesktop ? m_previewDesktop->id() : QString()},
        {QStringLiteral("previewCard"), m_previewCard ? m_previewCard->caption() : QString()},
        {QStringLiteral("named"), QJsonArray::fromStringList(m_namedDesktops)},
        {QStringLiteral("renaming"), m_tableRenaming},
        {QStringLiteral("typing"), m_tablePresenter && m_tablePresenter->hasTextFocus()},
        {QStringLiteral("textFocusedApplication"), KWin::waylandServer()->seat()->focusedTextInputSurface() != nullptr},
        {QStringLiteral("renameText"), m_tableRenaming >= 0 && m_tablePresenter ? m_tablePresenter->editedText() : QString()},
        {QStringLiteral("workspaces"), workspaces},
    }).toJson(QJsonDocument::Compact));
}

void Effect::loadNamedDesktops()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Table"));
    m_namedDesktops = group.readEntry(QStringLiteral("NamedDesktops"), QStringList{});
    // A workspace pinned before names kept them stays kept, as named.
    if (group.hasKey("PinnedDesktops")) {
        for (const QString &id : group.readEntry(QStringLiteral("PinnedDesktops"), QStringList{}))
            if (!m_namedDesktops.contains(id)) m_namedDesktops.append(id);
        group.deleteEntry(QStringLiteral("PinnedDesktops"));
        saveNamedDesktops();
    }
}

void Effect::setUsesDrawnZones(bool uses)
{
    if (uses == m_usesDrawnZones) return;
    m_usesDrawnZones = uses;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Bento"));
    group.writeEntry(QStringLiteral("UseDrawnZones"), uses);
    group.sync();
    forEachSession([this] { m_desktopStage->zoneModeChanged(); });
}

void Effect::setShowsKeyboardOutline(bool shows)
{
    if (shows == m_showsKeyboardOutline) return;
    m_showsKeyboardOutline = shows;
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Windows"));
    group.writeEntry(QStringLiteral("KeyboardOutline"), shows);
    group.sync();
    KWin::effects->addRepaintFull();
}

// Meta+Shift+B says which arrangement the monitors now take, as Plasma's own
// switches do.
void Effect::switchZoneMode()
{
    setUsesDrawnZones(!m_usesDrawnZones);
    const QString text = m_usesDrawnZones ? QStringLiteral("Monitors: my zones") : QStringLiteral("Monitors: auto fill");
    // A surface showing the context says it on its line instead of a popup
    // over the work.
    if (!m_contextLineClaims.isEmpty()) {
        m_zoneNotice = ZoneNotice{text, {}};
        m_zoneNotice->shown.start();
        m_contextNoticeEnd.start();
        Q_EMIT workspaceContextChanged();
        return;
    }
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"), QStringLiteral("org.kde.osdService"), QStringLiteral("showText"));
    message << QStringLiteral("view-grid") << text;
    QDBusConnection::sessionBus().asyncCall(message);
}

void Effect::saveNamedDesktops()
{
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kaduncerc"))->group(QStringLiteral("Table"));
    group.writeEntry(QStringLiteral("NamedDesktops"), m_namedDesktops);
    group.sync();
}

void Effect::scheduleDissolve()
{
    if (m_dissolveQueued) return;
    m_dissolveQueued = true;
    // A closing or departing window is still counted inside the call that
    // announces it; the turn after sees where everything stands.
    QTimer::singleShot(0, this, &Effect::dissolveEmptyWorkspaces);
}

void Effect::dissolveEmptyWorkspaces()
{
    m_dissolveQueued = false;
    if (m_table.isOpen()) return;
    auto *manager = KWin::VirtualDesktopManager::self();
    const auto desktops = KWin::effects->desktops();
    const auto stack = KWin::effects->stackingOrder();
    for (KWin::VirtualDesktop *desktop : desktops) {
        if (manager->count() <= 1) return;
        // The workspace the person is in stays until they leave it, and one
        // made elsewhere stays until it has been entered and left.
        if (desktop == KWin::effects->currentDesktop() || m_namedDesktops.contains(desktop->id())
            || m_unenteredDesktops.contains(desktop->id())) continue;
        const bool kept = std::any_of(stack.cbegin(), stack.cend(), [desktop](KWin::EffectWindow *window) {
            return !window->isDeleted() && window->isNormalWindow() && !window->isOnAllDesktops()
                && !dependentLead(window) && window->isOnDesktop(desktop);
        });
        if (!kept) manager->removeVirtualDesktop(desktop);
    }
}

} // namespace Kadunce

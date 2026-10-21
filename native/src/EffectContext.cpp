/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// What the Effect tells other programs: the top edge and context line, the
// workspace context, placement requests (docs/REQUESTS.md), gesture notes and
// the spoken line.
#include "EffectInternal.h"

namespace Kadunce
{

namespace
{
// A rectangle as the journal and workspace context write it.
QString rectText(const QRectF &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}
}

QStringList Effect::namesBehindInSpread(int cardId) const
{
    QStringList names;
    const auto &cards = m_cardStage->liveCards();
    const auto order = m_cardStage->model().stackPaintOrderForId(cardId);
    // Painted farthest first, the face last.
    for (auto id = order.crbegin(); id != order.crend(); ++id) {
        if (id == order.crbegin()) continue;
        KWin::EffectWindow *card = *id >= 1 && *id <= cards.size() ? cards.at(*id - 1).data() : nullptr;
        if (card && !card->isDeleted()) names.append(applicationDisplayName(card));
    }
    return names;
}

QString Effect::spreadCardName(KWin::EffectWindow *window, int cardId) const
{
    const QStringList behind = namesBehindInSpread(cardId);
    if (!behind.isEmpty()) return stackedApplicationName(applicationDisplayName(window), behind);
    QStringList partners;
    for (auto *pane : m_cardStage->layoutPartners(window)) partners.append(applicationDisplayName(pane));
    return pairedApplicationName(applicationDisplayName(window), partners);
}

QString Effect::applicationDisplayName(KWin::EffectWindow *window) const
{
    if (!window) return {};
    const auto cached = m_applicationDisplayNames.constFind(window);
    if (cached != m_applicationDisplayNames.cend()) return *cached;
    QString serviceName;
    QString resourceClass;
    if (window->window()) {
        const QString desktopFileName =
            window->window()->desktopFileName().trimmed();
        if (!desktopFileName.isEmpty()) {
            QString desktopName = desktopFileName;
            if (desktopName.endsWith(QStringLiteral(".desktop")))
                desktopName.chop(8);
            auto service = KService::serviceByDesktopName(desktopName);
            if (!service) service = KService::serviceByStorageId(desktopFileName);
            if (service) serviceName = service->name();
        }
        resourceClass = window->window()->resourceClass();
    }
    const QString name = humanApplicationName(serviceName, resourceClass,
        window->windowClass().section(QLatin1Char(' '), -1),
        window->caption());
    m_applicationDisplayNames.insert(window, name);
    return name;
}

bool Effect::claimTopEdge(const QString &key, const QString &outputName,
    double x, double y, double width, double height)
{
    if (!calledFromDBus() || key.isEmpty() || outputName.isEmpty()
        || !(width > 0.0) || !(height > 0.0)) {
        return false;
    }
    const QString owner = message().service();
    watchClaimOwner(owner);
    m_topEdgeClaims.insert(owner + QLatin1Char('\n') + key,
        TopEdgeClaim{owner, outputName, QRectF(x, y, width, height)});
    return true;
}

void Effect::watchClaimOwner(const QString &owner)
{
    if (!m_topEdgeClaimWatcher) {
        m_topEdgeClaimWatcher = new QDBusServiceWatcher(
            QString(), QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForUnregistration, this);
        connect(m_topEdgeClaimWatcher, &QDBusServiceWatcher::serviceUnregistered,
                this, [this](const QString &service) {
            m_topEdgeClaims.removeIf([&service](const auto &claim) {
                return claim.value().owner == service;
            });
            const bool hadLine = m_contextLineClaims.removeIf([&service](const auto &claim) {
                return claim.value().owner == service;
            }) > 0;
            const bool hadFrost = m_frostBehind.remove(service) > 0;
            m_topEdgeClaimWatcher->removeWatchedService(service);
            // Kadunce's own words come back on the displays it left.
            if (hadLine || hadFrost) KWin::effects->addRepaintFull();
        });
    }
    m_topEdgeClaimWatcher->addWatchedService(owner);
}

bool Effect::claimContextLine(const QString &key, const QString &outputName)
{
    if (!calledFromDBus() || key.isEmpty() || outputName.isEmpty()) return false;
    const QString owner = message().service();
    watchClaimOwner(owner);
    m_contextLineClaims.insert(owner + QLatin1Char('\n') + key, ContextLineClaim{owner, outputName});
    KWin::effects->addRepaintFull();
    return true;
}

void Effect::clearContextLineClaim(const QString &key)
{
    if (!calledFromDBus()) return;
    if (m_contextLineClaims.remove(message().service() + QLatin1Char('\n') + key) > 0)
        KWin::effects->addRepaintFull();
}

void Effect::frostBehind(bool on)
{
    if (!calledFromDBus()) return;
    const QString caller = message().service();
    if (!on) {
        if (m_frostBehind.remove(caller) > 0) KWin::effects->addRepaintFull();
        return;
    }
    const auto pid = connection().interface()->servicePid(caller);
    if (!pid.isValid() || pid.value() == 0) return;
    watchClaimOwner(caller);
    m_frostBehind.insert(caller, qint64(pid.value()));
    KWin::effects->addRepaintFull();
}

bool Effect::contextLineClaimed(const KWin::LogicalOutput *output) const
{
    if (!output || m_contextLineClaims.isEmpty()) return false;
    for (const ContextLineClaim &claim : m_contextLineClaims)
        if (claim.output == output->name()) return true;
    return false;
}

QJsonValue Effect::contextNotice() const
{
    const auto notice = [](const QString &kind, const QString &text, const KWin::LogicalOutput *output) {
        return QJsonObject{
            {QStringLiteral("kind"), kind},
            {QStringLiteral("text"), text},
            {QStringLiteral("output"), output ? output->name() : QString()},
        };
    };
    if (m_detachShownOn) return notice(QStringLiteral("returnToDesktop"), tr("Return to desktop"), m_detachShownOn);
    if (m_gestureNote && m_gestureNote->output && m_gestureNote->shown.elapsed() < GestureNoteTiming::TotalMs)
        return notice(QStringLiteral("refused"), m_gestureNote->text, m_gestureNote->output);
    if (m_lineNotice && m_lineNotice->shown.elapsed() < GestureNoteTiming::TotalMs)
        return notice(m_lineNotice->kind, m_lineNotice->text, nullptr);
    // The open room's offer says how to choose on its display's own line.
    if (m_roomOffer && m_roomOffer->output && contextLineClaimed(m_roomOffer->output))
        return notice(QStringLiteral("offer"), roomOfferHint(), m_roomOffer->output);
    return QJsonValue(QJsonValue::Null);
}

void Effect::eachCardDisplay(const std::function<void(KWin::LogicalOutput *)> &f) const
{
    // Each display holding cards, its own stage named while it is read.
    auto *self = const_cast<Effect *>(this);
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!isCardDisplay(output) || !m_currentSession) continue;
        SessionScope scope(self, m_currentSession, stageKeyFor(output));
        f(output);
    }
}

QJsonObject Effect::cardStageContext() const
{
    // §2 has three presentations and this reported two, so the one where the
    // display shows its panes and Card Stage holds its cards hidden read as
    // Active -- the state physical review could not tell from a card actually
    // drawn over the layout. `cardLine` is a frozen interface identity; adding
    // a value beside it is not a rename.
    const QString presentation = !m_cardStage->isActive()
        ? QStringLiteral("inactive")
        : m_cardStage->presentation() == CardPresentation::Spread
            ? QStringLiteral("cardLine")
            : m_cardStage->presentation() == CardPresentation::Bento
                ? QStringLiteral("bento")
            : m_cardStage->presentation() == CardPresentation::Desktop
                ? QStringLiteral("desktop") : QStringLiteral("active");
    const auto workspace = m_cardStage->workspaceSnapshot();
    QJsonArray selectedStack;
    for (const auto &member : workspace.selectedStack) selectedStack.append(member);
    KWin::EffectWindow *active = m_cardStage->isActive()
        && m_cardStage->presentation() == CardPresentation::Active ? m_cardStage->activeCardIdentity() : nullptr;
    return QJsonObject{
        {QStringLiteral("active"), m_cardStage->isActive()},
        {QStringLiteral("presentation"), presentation},
        {QStringLiteral("launcherGuestActive"), m_cardStage->launcherGuestActive()},
        {QStringLiteral("selectedCardId"), workspace.selectedCardId.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(workspace.selectedCardId)},
        {QStringLiteral("selectedStack"), selectedStack},
        {QStringLiteral("spreadEntry"), spreadEntryContext()},
        {QStringLiteral("activeCardId"), active ? QJsonValue(windowIdentity(active)) : QJsonValue(QJsonValue::Null)},
    };
}

QJsonValue Effect::spreadEntryContext() const
{
    if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread)
        return QJsonValue(QJsonValue::Null);
    const SpreadModel &model = m_cardStage->model();
    const auto &cards = m_cardStage->liveCards();
    const int id = model.count() > 0 ? model.idAtOffset(0) : 0;
    KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
    if (!window || window->isDeleted()) return QJsonValue(QJsonValue::Null);
    const int size = std::max(1, model.stackSizeForId(id));
    return QJsonObject{
        {QStringLiteral("cardId"), window->internalId().toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), spreadCardName(window, id)},
        {QStringLiteral("bento"), !m_cardStage->layoutPartners(window).isEmpty()},
        {QStringLiteral("stackPosition"), size > 1 ? m_cardStage->shownStackPosition(id) + 1 : 1},
        {QStringLiteral("stackSize"), size},
        {QStringLiteral("sleeping"), window->isMinimized()},
    };
}

QString Effect::contextLineSignature() const
{
    // Every display's line, so a change on any one is announced once.
    QString signature;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        signature += output->name() + QLatin1Char(' ');
        if (!m_cardStage->isActive() || m_cardStage->presentation() != CardPresentation::Spread) {
            signature += QLatin1Char('\n');
            return;
        }
        const SpreadModel &model = m_cardStage->model();
        const auto &cards = m_cardStage->liveCards();
        const int id = model.count() > 0 ? model.idAtOffset(0) : 0;
        KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
        if (window && !window->isDeleted()) {
            signature += QStringLiteral("%1 %2 %3 %4 %5").arg(id)
                .arg(m_cardStage->shownStackPosition(id)).arg(model.stackSizeForId(id))
                .arg(window->isMinimized()).arg(spreadCardName(window, id));
        }
        signature += QLatin1Char('\n');
    });
    if (m_detachShownOn) signature += QStringLiteral("d ") + m_detachShownOn->name();
    else if (m_gestureNote && m_gestureNote->output && m_gestureNote->shown.elapsed() < GestureNoteTiming::TotalMs)
        signature += QStringLiteral("r ") + m_gestureNote->text;
    else if (m_lineNotice && m_lineNotice->shown.elapsed() < GestureNoteTiming::TotalMs)
        signature += QStringLiteral("z ") + m_lineNotice->text;
    else if (m_roomOffer && m_roomOffer->output && contextLineClaimed(m_roomOffer->output))
        signature += QStringLiteral("o ") + m_roomOffer->output->name();
    return signature;
}

void Effect::clearTopEdgeClaim(const QString &key)
{
    if (!calledFromDBus()) return;
    m_topEdgeClaims.remove(message().service() + QLatin1Char('\n') + key);
}

bool Effect::topEdgeClaimedForInput(const QPointF &position) const
{
    if (m_topEdgeClaims.isEmpty()) return false;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!output) continue;
        const QRectF display(output->geometry());
        if (!display.contains(position)) continue;
        for (const TopEdgeClaim &claim : m_topEdgeClaims) {
            if (claim.output == output->name()
                && claim.area.translated(display.topLeft()).contains(position)) {
                return true;
            }
        }
    }
    return false;
}

bool Effect::surfaceOwnsTouchAt(const QPointF &position) const
{
    // Asks for the window KWin itself would deliver the touch to, so a layer
    // surface's input region is honoured: a touch beside a masked strip lands
    // on whatever is beneath and stays a bottom swipe candidate. Panels are
    // excluded because a swipe from the dock is the bottom swipe. So is the
    // desktop background, also a layer surface, which is all the bezel lies
    // over when no dock is on this display.
    const KWin::Window *window = KWin::input()->findToplevel(position);
    return window && window->inherits("KWin::LayerShellV1Window")
        && !window->isDock() && !window->isDesktop() && !window->isAppletPopup();
}

bool Effect::clientReceivesTouchAt(const QPointF &position) const
{
    // The window KWin would deliver the touch to. On its frame or the resize
    // border beyond it, KWin's decoration handling takes the contact and no
    // client ever holds it.
    const KWin::Window *window = KWin::input()->findToplevel(position);
    return window && (!window->decoration() || QRectF(window->clientGeometry()).contains(position));
}

QString Effect::describeTitleTouch(const QPointF &position) const
{
    // Only while Cards is up on the tablet, and only on a title: on the
    // decoration of the window KWin would deliver the touch to, or along the
    // top of the chosen card, whatever lies there.
    if (!m_cardStage || !m_cardStage->isActive() || !isTabletPoint(position)) return {};
    const KWin::EffectWindow *chosen = m_cardStage->selectedWindow();
    const KWin::Window *card = chosen ? chosen->window() : nullptr;
    const KWin::Window *under = KWin::input()->findToplevel(position);
    const bool onDecoration = under && under->decoration()
        && !QRectF(under->clientGeometry()).contains(position);
    bool onCardTop = false;
    if (card) {
        const QRectF frame(card->frameGeometry());
        onCardTop = position.x() >= frame.left() && position.x() < frame.right()
            && position.y() >= frame.top() - 12.0 && position.y() < frame.top() + 60.0;
    }
    if (!onDecoration && !onCardTop) return {};
    const auto name = [](const KWin::Window *window) { return logNameOf(window); };
    QString presentation = QStringLiteral("Spread");
    if (m_cardStage->presentation() == CardPresentation::Active) presentation = QStringLiteral("Active");
    else if (m_cardStage->presentation() == CardPresentation::Bento) presentation = QStringLiteral("Bento");
    const KWin::Window *moving = KWin::workspace()->moveResizeWindow();
    const auto box = [](const KWin::Window *window) {
        const QRectF frame(window->frameGeometry());
        return QStringLiteral(" frame %1,%2 %3x%4").arg(frame.x()).arg(frame.y()).arg(frame.width()).arg(frame.height());
    };
    QString line;
    QDebug(&line).nospace().noquote() << "in " << presentation << "; KWin delivers it to " << name(under)
        << (under ? (onDecoration ? " on its title or border" : " inside its client area") : "")
        << (under ? box(under) : QString())
        << "; the chosen card is " << name(card)
        << (card ? box(card) + (card->isActive() ? QStringLiteral(", focused") : QStringLiteral(", not focused"))
                       + (card->isMinimized() ? QStringLiteral(", minimized") : QString()) : QString())
        << (under && under == card ? " (the same window)" : " (a different window)")
        << "; KWin is moving " << name(moving)
        << "; Kadunce is carrying " << (m_carriedWindow ? "a window" : "nothing");
    if (m_touchWitness && m_touchWitness->describeTouchState)
        line += QStringLiteral("; before this press KWin ") + m_touchWitness->describeTouchState();
    return line;
}

bool Effect::inputPanelContainsForInput(const QPointF &position) const
{
    // The panel's own input region, not its frame: a keyboard drawn over a
    // wider transparent surface takes only the touches on its keys.
    KWin::EffectWindow *panel = KWin::effects->inputPanel();
    return panel && !panel->isDeleted() && panel->isVisible() && panel->window()
        && panel->window()->hitTest(position);
}

bool Effect::isTabletPoint(const QPointF &position) const
{
    KWin::LogicalOutput *tablet = tabletOutput();
    if (!tablet) return false;
    // Half-open and unrounded: the display's last physical rows map to within
    // a pixel of its logical bottom, and rounding them carried them off it.
    const QRectF area(tablet->geometry());
    return position.x() >= area.left() && position.x() < area.right()
        && position.y() >= area.top() && position.y() < area.bottom();
}

QStringList Effect::outputStageState() const
{
    return m_desktopStage->outputStageState();
}

QString Effect::loadedPluginProvenance() const
{
    // Replacing the plugin file gives it a new inode. A KWin that kept the
    // previous image mapped across an unload re-instantiates the earlier build
    // under the same effect id, and the only place that is visible is this
    // process's own map of itself. An unlinked mapping is reported rather than
    // hidden: it is the clearest form of the same answer.
    // Read with a plain stream: procfs reports a size of zero, so QFile's
    // end-of-file and bytes-available answers are derived from a length that
    // does not exist and a QFile loop reads nothing at all.
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) {
        return {};
    }
    std::string raw;
    while (std::getline(maps, raw)) {
        const QString line = QString::fromStdString(raw).trimmed();
        const qsizetype pathStart = line.indexOf(QLatin1Char('/'));
        if (pathStart < 0) {
            continue;
        }
        QString path = line.mid(pathStart);
        const bool deleted = path.endsWith(QLatin1String(" (deleted)"));
        if (deleted) {
            path.chop(QLatin1String(" (deleted)").size());
        }
        if (!path.endsWith(QLatin1String("/kwin4_effect_kadunce.so"))) {
            continue;
        }
        const QStringList fields = line.left(pathStart).split(QLatin1Char(' '),
                                                              Qt::SkipEmptyParts);
        if (fields.size() < 5) {
            continue;
        }
        return QStringLiteral("%1 %2").arg(fields.at(4),
            deleted ? QStringLiteral("deleted") : QStringLiteral("present"));
    }
    return {};
}

QString Effect::workspaceContext() const
{
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    // §11: every display's own stage, under that display's name.
    struct StageView {
        QString display;
        const CardStageController *cards;
        CardWorkspaceSnapshot workspace;
    };
    std::vector<StageView> stages;
    eachCardDisplay([&](KWin::LogicalOutput *output) {
        stages.push_back({output->name(), m_cardStage, m_cardStage->workspaceSnapshot()});
    });
    const auto viewHolding = [&](const KWin::EffectWindow *window) -> const StageView * {
        for (const auto &view : stages)
            if (view.cards->liveCardIndex(window) >= 0) return &view;
        return nullptr;
    };

    QJsonArray applications;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(window)) {
            continue;
        }
        const StageView *view = viewHolding(window);
        const auto *card = view ? view->workspace.find(windowIdentity(window)) : nullptr;
        const CardStageController *keeper = stageKeepingStackOf(window);
        QJsonObject application{
            {QStringLiteral("windowId"), windowIdentity(window)},
            {QStringLiteral("appId"), applicationIdentity(window)},
            {QStringLiteral("title"), window->caption()},
            {QStringLiteral("output"),
             window->screen() ? window->screen()->name() : QString()},
            {QStringLiteral("focused"), window == focused},
            {QStringLiteral("lastActivated"), static_cast<qint64>(m_activationOrder.value(windowIdentity(window)))},
            {QStringLiteral("minimized"), window->isMinimized()},
            {QStringLiteral("hasCard"), card != nullptr},
            {QStringLiteral("pane"), m_desktopStage->managesWindow(window)},
            // The display whose card stage holds it as a card; empty when none does.
            {QStringLiteral("stage"), card ? view->display : QString()},
            // Whether Kadunce rounds its corners, and what would keep it from it.
            {QStringLiteral("corners"), QJsonObject{
                {QStringLiteral("rounded"), trimsCorners(window)},
                {QStringLiteral("available"), roundsCorners()},
                {QStringLiteral("shadow"), drawsOutsideFrame(window)},
                // How its last frame was drawn, and the sizes that decide it.
                {QStringLiteral("drawnAs"), m_drawnAs.value(window)},
                {QStringLiteral("frame"), rectText(window->frameGeometry())},
                {QStringLiteral("expanded"), rectText(window->expandedGeometry())},
                {QStringLiteral("buffer"), rectText(window->bufferGeometry())},
                {QStringLiteral("maximized"), window->window()
                    && window->window()->maximizeMode() == KWin::MaximizeFull}}},
            // §11, Connected: the cards waiting behind a pane, front to back.
            {QStringLiteral("paneStack"), [&] {
                QJsonArray behind;
                if (keeper)
                    for (auto *member : keeper->paneStack(window)) behind.append(windowIdentity(member));
                return behind;
            }()},
        };
        if (card) {
            const CardStageController *cards = view->cards;
            application.insert(QStringLiteral("cardId"),
                               windowIdentity(window));
            application.insert(QStringLiteral("cardIndex"), card->cardIndex);
            application.insert(QStringLiteral("stackId"),
                               card->stackId);
            application.insert(QStringLiteral("stackPosition"),
                               card->stackPosition);
            application.insert(QStringLiteral("stackSize"),
                               card->stackSize);
            application.insert(QStringLiteral("selected"),
                               card->selected);
            // Where the row stands, and where Spread draws the card.
            const int id = cards->liveCards().indexOf(window) + 1;
            application.insert(QStringLiteral("entry"), cards->model().entryIndexForId(id));
            auto *display = screenNamed(view->display);
            if (display && cards->isActive()
                && cards->presentation() == CardPresentation::Spread
                && cards->paintSlot(window) != 99) {
                auto rect = cards->previewTargetForWindow(display, window);
                const auto pose = cards->stackPoseForWindow(window, rect.width());
                rect.translate(qRound(pose.x), qRound(pose.y));
                application.insert(QStringLiteral("spreadRect"), geometryContext(rect));
            }
        }
        applications.append(application);
    }

    // A note being typed over a window has the keys, and the window it is
    // stuck to stays the one in use: focus names that window and says so.
    KWin::EffectWindow *notesHost = notesSurfaceHost(focused);
    KWin::EffectWindow *inUse = notesHost ? notesHost : focused;
    QJsonObject focus;
    if (inUse && isApplicationWindow(inUse)) {
        const bool focusedHasCard = viewHolding(inUse) != nullptr;
        focus = {
            {QStringLiteral("windowId"), windowIdentity(inUse)},
            {QStringLiteral("appId"), applicationIdentity(inUse)},
            {QStringLiteral("title"), inUse->caption()},
            {QStringLiteral("hasCard"), focusedHasCard},
        };
        if (focusedHasCard) {
            focus.insert(QStringLiteral("cardId"),
                         windowIdentity(inUse));
        }
        if (notesHost) focus.insert(QStringLiteral("notes"), true);
    }

    // The display in use: its stage is the top-level cardStage, and it is the
    // display whose role is "tablet".
    const QJsonObject inUseStage = cardStageContext();

    QJsonArray displays;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        if (!output) {
            continue;
        }
        const bool tablet = isTabletOutput(output);
        const bool holdsCards = isCardDisplay(output);
        QJsonObject own;
        if (holdsCards) {
            SessionScope scope(const_cast<Effect *>(this), m_currentSession, stageKeyFor(output));
            own = cardStageContext();
            own.insert(QStringLiteral("spreadOpen"), m_cardStage->isActive()
                && m_cardStage->presentation() == CardPresentation::Spread && !m_cardStage->spreadOpening());
        }
        // What the display presents and the windows it shows that way: its
        // own Active card, or a layout's panes.
        QJsonArray shows;
        QString presents = QStringLiteral("none");
        if (m_desktopStage->hasSessionOnOutput(output->name())) {
            presents = QStringLiteral("layout");
            for (const auto &pane : m_desktopStage->panesOn(output->name()))
                if (pane && !pane->isMinimized()) shows.append(windowIdentity(pane));
        } else if (holdsCards && own.value(QStringLiteral("active")).toBool()) {
            presents = own.value(QStringLiteral("presentation")).toString();
            if (own.value(QStringLiteral("activeCardId")).isString())
                shows.append(own.value(QStringLiteral("activeCardId")));
        }
        QJsonObject display{
            {QStringLiteral("name"), output->name()},
            {QStringLiteral("role"), tablet
                ? QStringLiteral("tablet") : QStringLiteral("external")},
            {QStringLiteral("geometry"), geometryContext(output->geometry())},
            {QStringLiteral("bentoActive"),
             m_desktopStage->hasSessionOnOutput(output->name())},
            {QStringLiteral("sizing"), sizingName(sizingFor(output))},
            {QStringLiteral("sizingChosen"), !chosenSizingFor(output).isEmpty()},
            {QStringLiteral("holdsCards"), holdsCards},
            {QStringLiteral("primary"), output->name() == m_cardOutputName},
            {QStringLiteral("touchscreen"), output->name() == m_touchOutputName},
            {QStringLiteral("presentation"), presents},
            {QStringLiteral("shows"), shows},
            // Why its layout draws no dividers, when it has one and draws none.
            {QStringLiteral("dividersWithheld"), m_desktopStage->hasSessionOnOutput(output->name())
                ? m_desktopStage->dividersWithheld(output) : QString()},
        };
        if (holdsCards) display.insert(QStringLiteral("cardStage"), own);
        displays.append(display);
    }

    QJsonObject root{
        {QStringLiteral("schema"),
         QStringLiteral("co.goodinput.kadunce.workspace-context")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("focus"), focus.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(focus)},
        {QStringLiteral("applications"), applications},
        {QStringLiteral("cardStage"), inUseStage},
        {QStringLiteral("notice"), contextNotice()},
        {QStringLiteral("desktopStage"), QJsonObject{
            {QStringLiteral("active"), hasActiveDesktopStage()},
        }},
        {QStringLiteral("table"), QJsonObject{
            {QStringLiteral("open"), m_table.isOpen()},
        }},
        {QStringLiteral("displayContext"), QJsonObject{
            {QStringLiteral("edgeBackend"), m_usesDirectSystemEdges
                ? QStringLiteral("z13-direct")
                : QStringLiteral("plasma-native")},
            {QStringLiteral("displays"), displays},
            {QStringLiteral("spreadDisplay"), m_spreadChoice.display},
            {QStringLiteral("primaryDisplay"), m_cardOutputName},
        }},
    };
    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool Effect::activateApplicationWindow(const QString &windowId)
{
    if (!callerMayDrive()) return false;
    const QString requested = windowId.trimmed();
    if (requested.isEmpty()) {
        return false;
    }

    const QList<KWin::EffectWindow *> windows =
        KWin::effects->stackingOrder();
    for (auto iterator = windows.crbegin(); iterator != windows.crend();
         ++iterator) {
        KWin::EffectWindow *window = *iterator;
        if (!isApplicationWindow(window)
            || windowIdentity(window) != requested
            || !window->window()) {
            continue;
        }

        closeLauncherGuest();
        if (window->isMinimized()) {
            window->unminimize();
        }
        KWin::workspace()->raiseWindow(window->window());
        KWin::workspace()->activateWindow(window->window(), true);
        return true;
    }
    return false;
}

// Placement requests (docs/REQUESTS.md). The dock, or any other requester,
// carries an application to a point; Kadunce shows where it would go and puts
// it there when it is let go.
int Effect::placementProtocolVersion() const
{
    return 1;
}

QList<PlacementOutput> Effect::placementOutputs() const
{
    QList<PlacementOutput> outputs;
    for (auto *output : KWin::effects->screens()) {
        if (output) outputs.append({output->name(), QRectF(output->geometry()), m_cardStage->canOwnCards(output)});
    }
    return outputs;
}

PlacementAim Effect::placementAimAt(QPointF point) const
{
    auto aim = placementAim(placementOutputs(), point);
    // CARD-LIFECYCLE.md §3: a side snap on a display Kadunce does not yet own
    // gives an Active card, as the middle does. On Connected (§11) the edge
    // still names a place, the card alone there.
    if ((aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        && !m_cardStage->isActive()) {
        for (auto *output : KWin::effects->screens()) {
            if (output && output->name() == aim.output && m_cardStage->canOwnCards(output)
                && !connectedOutputForDesktopStage(output))
                aim.kind = PlacementAimKind::Card;
        }
    }
    return aim;
}

std::optional<KWin::RectF> Effect::placementPreview(const PlacementAim &aim, QPointF point) const
{
    KWin::LogicalOutput *output = nullptr;
    for (auto *candidate : KWin::effects->screens()) {
        if (candidate && candidate->name() == aim.output) output = candidate;
    }
    if (!output) return std::nullopt;
    QRectF area = QRectF(KWin::effects->clientArea(KWin::MaximizeArea, output))
        .intersected(QRectF(output->geometry()));
    area.setBottom(std::min(area.bottom(), double(output->geometry().bottom()) - PlacementDockBand));
    switch (aim.kind) {
    case PlacementAimKind::Card:
    case PlacementAimKind::Top:
        return KWin::RectF(m_cardStage->activeTarget(output));
    case PlacementAimKind::Left:
    case PlacementAimKind::Right: {
        const bool right = aim.kind == PlacementAimKind::Right;
        const auto side = sideChoiceOn(output, right, point.y());
        // §11, Connected: the box is the place, beside a column at that edge,
        // over the top of that side and under its bottom.
        if (connectedOutputForDesktopStage(output)) {
            const double width = area.width() / 2.0;
            const double top = side.large || side.over ? area.top() : area.center().y();
            const double bottom = side.over ? area.center().y() : area.bottom();
            return KWin::RectF(right ? area.right() - width : area.left(), top, width,
                               bottom - top);
        }
        const double width = area.width() * (side.large ? 2.0 / 3.0 : 1.0 / 3.0);
        return KWin::RectF(right ? area.right() - width : area.left(), area.top(), width, area.height());
    }
    case PlacementAimKind::Display:
        return KWin::RectF(area);
    case PlacementAimKind::None:
        break;
    }
    return std::nullopt;
}

QString Effect::aimPlacement(double x, double y)
{
    if (!callerMayDrive()) return {};
    const QPointF point(x, y);
    const auto aim = placementAimAt(point);
    const auto preview = aim.kind == PlacementAimKind::None ? std::nullopt : placementPreview(aim, point);
    m_placementAim = aim;
    m_placementPreview = preview;
    m_placementPreviewOutput.clear();
    for (auto *output : KWin::effects->screens()) {
        if (output && preview && output->name() == aim.output) m_placementPreviewOutput = output;
    }
    KWin::effects->addRepaintFull();
    return placementAimName(aim.kind);
}

void Effect::clearPlacementAim()
{
    if (!callerMayDrive()) return;
    if (!m_placementAim && !m_placementPreview) return;
    m_placementAim.reset();
    m_placementPreview.reset();
    m_placementPreviewOutput.clear();
    KWin::effects->addRepaintFull();
}

bool Effect::placeApplication(const QStringList &applicationIds, const QString &windowId,
                              double x, double y, const QString &requestToken)
{
    if (!callerMayDrive()) return false;
    const QPointF point(x, y);
    const auto aim = placementAimAt(point);
    const auto aimed = m_placementAim;
    clearPlacementAim();
    // A request is placed only where it was last aimed, never where a release
    // happens to land.
    if (requestToken.isEmpty() || aim.kind == PlacementAimKind::None || !aimed || *aimed != aim)
        return false;
    QString requested = windowId.trimmed();
    requested.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    if (requested.isEmpty()) {
        auto launch = PendingLaunch::make(applicationIds, requestToken);
        if (!launch) return false;
        if (m_placementLaunch)
            Q_EMIT placementSettled(m_placementLaunch->token, QString(), false);
        m_placementLaunch = std::move(*launch);
        m_placementLaunchAim = aim;
        m_placementLaunchPoint = point;
        const auto generation = ++m_placementGeneration;
        QTimer::singleShot(PlacementLaunchTimeoutMs, this, [this, generation] {
            if (generation == m_placementGeneration && m_placementLaunch) cancelPlacement(m_placementLaunch->token);
        });
        return true;
    }
    KWin::EffectWindow *window = nullptr;
    for (auto *candidate : KWin::effects->stackingOrder()) {
        if (isApplicationWindow(candidate) && candidate->window()
            && windowIdentity(candidate).compare(requested, Qt::CaseInsensitive) == 0) window = candidate;
    }
    if (!window) return false;
    const bool placed = placeWindow(window, aim, point);
    Q_EMIT placementSettled(requestToken, windowIdentity(window), placed);
    return placed;
}

void Effect::cancelPlacement(const QString &requestToken)
{
    if (!callerMayDrive()) return;
    if (!m_placementLaunch || m_placementLaunch->token != requestToken) return;
    ++m_placementGeneration;
    const auto token = m_placementLaunch->token;
    m_placementLaunch.reset();
    Q_EMIT placementSettled(token, QString(), false);
}

void Effect::completePlacementForWindow(KWin::EffectWindow *window)
{
    if (!m_placementLaunch || !isApplicationWindow(window)
        || !m_placementLaunch->matches(applicationIdentity(window))) return;
    const auto token = m_placementLaunch->token;
    const auto aim = m_placementLaunchAim;
    const auto point = m_placementLaunchPoint;
    m_placementLaunch.reset();
    ++m_placementGeneration;
    // The arrival is admitted first, as every arrival is, and placed from there.
    QPointer<KWin::EffectWindow> arrival = window;
    QTimer::singleShot(0, this, [this, arrival, token, aim, point] {
        const bool placed = arrival && !arrival->isDeleted() && placeWindow(arrival, aim, point);
        Q_EMIT placementSettled(token, windowIdentity(arrival), placed);
    });
}

bool Effect::placeWindow(KWin::EffectWindow *window, const PlacementAim &aim, QPointF point)
{
    KWin::LogicalOutput *output = nullptr;
    for (auto *candidate : KWin::effects->screens()) {
        if (candidate && candidate->name() == aim.output) output = candidate;
    }
    if (!output || !window || window->isDeleted() || !window->window()) return false;
    SessionScope scope(this, m_scopedSession, stageKeyFor(output));
    const bool card = m_cardStage->isActive() && m_cardStage->liveCardIndex(window) >= 0;
    if (m_cardStage->canOwnCards(output)) {
        // An ordinary window shown elsewhere comes to the card display as one
        // carried there would, and is then handled as a card already here. A
        // pane keeps its layout.
        if (!card && window->screen() != output) {
            if (m_carriedWindow || m_nativeCarry || m_desktopStage->managesWindow(window)
                || !admitTransferredWindowToTablet(window, output, [] { return true; })
                || m_cardStage->liveCardIndex(window) < 0) return false;
            (void)m_cardStage->promoteToActive(window);
            observeCardOwnership();
        }
        if (!activateApplicationWindow(windowIdentity(window))) return false;
        if (aim.kind == PlacementAimKind::Card) return true;
        // CARD-LIFECYCLE.md §3, as the Active card carried to that edge.
        const bool right = aim.kind == PlacementAimKind::Right;
        auto side = sideChoiceOn(output, right, point.y());
        side.connected = connectedOutputForDesktopStage(output);
        QPointer<KWin::LogicalOutput> destination = output;
        QPointer<KWin::EffectWindow> placed = window;
        QTimer::singleShot(0, this, [this, destination, placed, side, right] {
            if (!destination || !placed) return;
            // §11, Connected: the edge names a place, as it does for the
            // Active card carried there: alone on an open display, or among
            // the columns of the layout the card was played into.
            if (side.connected) {
                m_aloneOnPurpose.removeAll(placed);
                // A layout already standing takes the card as a pane first,
                // as a pick from Spread would; only an open display places
                // it alone.
                if (!m_desktopStage->managesWindow(placed))
                    (void)admitActivatedCardToLiveBento(placed);
                if (!m_desktopStage->managesWindow(placed) && m_cardStage->hasBentoGroup())
                    (void)playPickedCardIntoLayout(placed);
                if (m_desktopStage->managesWindow(placed))
                    (void)m_desktopStage->placePaneBySide(placed, destination, side);
                else if (m_cardStage->activeCardIdentity() == placed
                         && !m_cardStage->hasBentoGroup())
                    (void)placeActiveCardAlone(destination, side);
                observeCardOwnership();
                return;
            }
            if (m_cardStage->activeCardIdentity() != placed) return;
            (void)pairActiveCardIntoBento(destination, side, !right);
            observeCardOwnership();
        });
        return true;
    }
    // A card goes to another display as the Active card carried there would.
    // A pane keeps its layout.
    if (card) return placeCardOnDisplay(window, output, aim, point);
    if (m_desktopStage->managesWindow(window)) {
        return aim.kind == PlacementAimKind::Display && window->screen() == output
            && activateApplicationWindow(windowIdentity(window));
    }
    if (aim.kind == PlacementAimKind::Display && window->screen() == output)
        return activateApplicationWindow(windowIdentity(window));
    std::optional<BentoSidePlacement> side;
    if (aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        side = bentoSideChoice(aim.kind == PlacementAimKind::Right, point.y(),
                               QRectF(output->geometry()).center().y());
    const auto drop = m_desktopStage->prepareCardDrop(window, output, KWin::RectF(window->frameGeometry()),
        aim.kind == PlacementAimKind::Display ? DesktopStageController::CardDropIntent::OpenSpace
                                              : DesktopStageController::CardDropIntent::ActivateBento, side);
    if (!drop) return false;
    const bool placed = m_desktopStage->transferPreparedCard(*drop, [] { return true; }, [] {});
    observeCardOwnership();
    return placed;
}

bool Effect::placeCardOnDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *output,
    const PlacementAim &aim, QPointF point)
{
    if (!card || !output || m_cardStage->canOwnCards(output) || card->screen() == output
        || m_carriedWindow || m_nativeCarry || m_placementDrop) return false;
    std::optional<BentoSidePlacement> side;
    if (aim.kind == PlacementAimKind::Left || aim.kind == PlacementAimKind::Right)
        side = bentoSideChoice(aim.kind == PlacementAimKind::Right, point.y(),
                               QRectF(output->geometry()).center().y());
    // A layout begins or is joined at an edge; anywhere else the card opens
    // there, at the size it had before it was a card, around the point.
    const auto intent = aim.kind == PlacementAimKind::Display
        ? DesktopStageController::CardDropIntent::OpenSpace
        : DesktopStageController::CardDropIntent::ActivateBento;
    const QRectF area = QRectF(output->geometry());
    const auto landingFor = [&](QSizeF size) {
        size = size.boundedTo(area.size());
        QRectF landing(QPointF(), size);
        landing.moveCenter(point);
        landing.moveLeft(std::clamp(landing.left(), area.left(), area.right() - size.width()));
        landing.moveTop(std::clamp(landing.top(), area.top(), area.bottom() - size.height()));
        return KWin::RectF(landing);
    };
    // The destination accepts before anything about the card changes, so a
    // refusal leaves the card as it was.
    if (!m_desktopStage->prepareCardDrop(card, output, landingFor(QSizeF(card->frameGeometry().width(), card->frameGeometry().height())),
            intent, side)) return false;
    // The card leaves as the Active card does, with the record of where it
    // was before it was a card.
    if (m_cardStage->presentation() != CardPresentation::Active
        || m_cardStage->selectedWindow() != card) (void)m_cardStage->promoteToActive(card);
    const auto source = m_cardStage->prepareNativeCarrySource(card);
    if (!source) return false;
    const KWin::RectF landing = landingFor(QSizeF(source->restoreSnapshot().geometry.width(),
        source->restoreSnapshot().geometry.height()));
    m_placementDrop = m_desktopStage->prepareCardDrop(card, output, landing, intent, side);
    if (!m_placementDrop) return false;
    const bool placed = m_cardStage->transferNativeCarryToDesktop(*source, output, landing);
    m_placementDrop.reset();
    observeCardOwnership();
    return placed;
}

void Effect::showGestureNote(KWin::LogicalOutput *output, QPointF contact, GestureRefusal reason)
{
    if (!output) return;
    const QString text = gestureRefusalText(reason);
    if (m_gestureNote) KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
    ShownGestureNote note{output, gestureNoteBox(QRectF(output->geometry()), contact,
                                                 m_gestureNoteLabel.size(text)), text, {}};
    note.shown.start();
    qInfo() << "Kadunce" << Revision << "refused a gesture on" << output->name() << ":" << text;
    m_gestureNote = std::move(note);
    KWin::effects->addRepaint(KWin::RectF(m_gestureNote->box).toAlignedRect());
    // Said rather than drawn on a claimed display, so no frame ends it.
    if (contextLineClaimed(output)) m_contextNoticeEnd.start();
}


bool Effect::notesShownInSpread() const
{
    return m_stuckNotesWatcher && !m_stuckNotes.isEmpty() && tabletOutput()
        && m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
        && !m_cardStage->spreadOpening() && !m_cardStage->launcherGuestActive()
        && !m_cardStage->cardGrabActive() && !m_table.isOpen() && !m_previewDesktop;
}

// What Spread says for its chosen entry: each entry by its front card, a
// pane's card with the panes beside it, and the entry's place in the row.
QString Effect::spreadLine()
{
    const SpreadModel &model = m_cardStage->model();
    const auto &cards = m_cardStage->liveCards();
    const int count = model.count();
    QList<SpreadReadingEntry> entries;
    entries.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int id = model.idAtOffset(i - model.selectedIndex());
        KWin::EffectWindow *window = id >= 1 && id <= cards.size() ? cards.at(id - 1).data() : nullptr;
        SpreadReadingEntry entry;
        if (window && !window->isDeleted()) {
            entry.application = applicationDisplayName(window);
            entry.title = window->caption();
            entry.stacked = std::max(1, model.stackSizeForId(id));
            entry.sleeping = window->isMinimized();
            for (auto *pane : m_cardStage->layoutPartners(window))
                entry.pairedWith.append(applicationDisplayName(pane));
            entry.behind = namesBehindInSpread(id);
        }
        entries.append(entry);
    }
    return Kadunce::spreadLine(entries, model.selectedIndex());
}

// What Table says: what is carried and where it would go, the card chosen,
// or the tab under the finger, else the workspace shown.
QString Effect::tableLine() const
{
    const auto workspaceName = [this](int index) {
        const TableWorkspace &workspace = m_tableWorkspaces[index];
        QString name = workspace.name.isEmpty() ? tr("Workspace %1").arg(index + 1)
                                                : tr("Workspace %1, %2").arg(index + 1).arg(workspace.name);
        if (workspace.current) name = tr("%1, current").arg(name);
        return name;
    };
    const auto cardName = [](const TableCard &card) {
        SpreadReadingEntry entry;
        entry.application = card.application;
        entry.title = card.title;
        entry.stacked = card.stacked + 1;
        return spreadEntryName(entry);
    };
    const auto validWorkspace = [this](int index) { return index >= 0 && index < m_tableWorkspaces.size(); };
    if (m_table.cancelling()) return tr("Let go to cancel");
    if (m_table.carrying()) {
        if (m_table.over() == TableGesture::Over::Plus) return tr("New workspace");
        if (m_table.over() == TableGesture::Over::Workspace && validWorkspace(m_table.overWorkspace()))
            return tr("Move to %1").arg(workspaceName(m_table.overWorkspace()));
        const int from = m_table.carryFrom();
        const int card = m_table.carryCard();
        if (validWorkspace(from) && card >= 0 && card < m_tableWorkspaces[from].cards.size())
            return tr("Moving %1").arg(cardName(m_tableWorkspaces[from].cards[card]));
    }
    const int tray = m_table.trayWorkspace();
    if (m_table.level() == TableLevel::Cards && validWorkspace(tray) && m_table.card() >= 0
        && m_table.card() < m_tableWorkspaces[tray].cards.size())
        return cardName(m_tableWorkspaces[tray].cards[m_table.card()]);
    if (m_table.onPlus()) return tr("New workspace");
    if (validWorkspace(m_table.hovered())) return workspaceName(m_table.hovered());
    if (validWorkspace(m_table.shownWorkspace())) return workspaceName(m_table.shownWorkspace());
    return tr("Table");
}

// Spread and Table are said to a screen reader while either shows, the
// line following what is chosen. A workspace's name being typed has the
// focus to itself, and Search over Spread's centre speaks for itself.
void Effect::syncSpokenLine()
{
    if (!SpokenLine::listening() || m_tableRenaming >= 0) {
        m_spokenSurface = SpokenSurface::None;
        m_spokenText.clear();
        if (m_spokenLine && m_spokenLine->visible()) m_spokenLine->hide();
        return;
    }
    KWin::LogicalOutput *output = nullptr;
    SpokenSurface surface = SpokenSurface::None;
    QString line;
    if (m_table.isOpen()) {
        output = tableOutput();
        surface = SpokenSurface::Table;
        line = m_tableLine;
    } else if (m_cardStage->isActive() && m_cardStage->presentation() == CardPresentation::Spread
               && !m_cardStage->launcherGuestActive() && !m_previewDesktop
               && !m_cardStage->liveCards().isEmpty()) {
        output = tabletOutput();
        surface = SpokenSurface::Spread;
        line = spreadLine();
    }
    if (!output || line.isEmpty()) {
        m_spokenSurface = SpokenSurface::None;
        m_spokenText.clear();
        if (m_spokenLine && m_spokenLine->visible()) m_spokenLine->hide();
        return;
    }
    // Opening says what it opened; after that only a change is said.
    const bool opened = surface != m_spokenSurface || !m_spokenLine || !m_spokenLine->visible();
    if (!opened && line == m_spokenText) return;
    m_spokenSurface = surface;
    m_spokenText = line;
    if (opened) line = surface == SpokenSurface::Table ? tr("Table, %1").arg(line) : tr("Spread, %1").arg(line);
    if (!m_spokenLine) m_spokenLine = std::make_unique<SpokenLine>();
    m_spokenLine->say(QRectF(output->geometry()).toRect(), line);
}

} // namespace Kadunce

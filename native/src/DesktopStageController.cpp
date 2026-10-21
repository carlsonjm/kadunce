/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DesktopStageInternal.h"

namespace Kadunce
{

DesktopStageController::DesktopStageController(DesktopStageHost *host)
    : m_host(host)
{
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(450);
    QObject::connect(&m_settleTimer, &QTimer::timeout, [this]() {
        settleSessions();
    });
}

void DesktopStageController::stopPendingSettle()
{
    m_applicationGuard.invalidate();
    m_settleTimer.stop();
}

std::optional<PreparedCarrySource> DesktopStageController::prepareNativeCarrySource(KWin::EffectWindow *window) const
{
    if (m_restoring || !window || window->isDeleted() || !window->window()
        || window->isUserResize() || window->isMinimized() || m_interactionWindow || !window->screen()
        || !KWin::effects->screens().contains(window->screen())
        || m_retiredOutputs.contains(window->screen())) return std::nullopt;
    const auto *session = sessionForOutput(window->screen());
    // Ownership here is the restore record, not pane membership: a window on
    // its way out of a layout is still owned and must hand over the record it
    // had before Bento placed it, not the pane rectangle it is sitting in.
    if (!ownsWindow(window) && m_host->isManagedWindowForDesktopStage(window)
        && m_host->allowsDesktopStageOnOutput(window->screen())) {
        const auto saved = makeSnapshot(window);
        PreparedCarrySource source;
        source.m_owner = m_carrySourceIdentity;
        source.m_window = window;
        source.m_generation = m_applicationGuard.generation();
        source.m_outputGeometry = window->screen()->geometry();
        source.m_desktopWindow = true;
        source.m_origin = {window->window()->internalId().toString(), saved.outputName,
            source.m_generation + 1, source.m_generation};
        source.m_restore = {saved, window->window(), window->screen()};
        return source;
    }
    if (!session || session->applying || !session->applicationToken) return std::nullopt;
    for (const auto &saved : session->snapshots) {
        if (saved.window != window || !saved.valid) continue;
        PreparedCarrySource source;
        source.m_owner = m_carrySourceIdentity;
        source.m_window = window;
        source.m_generation = m_applicationGuard.generation();
        source.m_outputGeometry = window->screen()->geometry();
        source.m_origin = {window->window()->internalId().toString(), session->outputName,
            session->applicationToken, source.m_generation};
        // Preserve pre-Bento restore geometry and flags; live tile geometry is
        // only a rendering origin and must not overwrite this record.
        source.m_restore = {saved, window->window(), outputForKey(saved.outputName)};
        return source;
    }
    return std::nullopt;
}

QString DesktopStageController::nativeCarryRefusal(KWin::EffectWindow *window) const
{
    // The conditions of prepareNativeCarrySource that a pane can meet, named.
    if (m_restoring) return QStringLiteral("Bento is being restored");
    if (m_interactionWindow) return QStringLiteral("another pane is being moved");
    if (!window || window->isDeleted() || !window->window() || !window->screen())
        return QStringLiteral("the window is gone");
    if (window->isUserResize()) return QStringLiteral("it is being resized");
    if (window->isMinimized()) return QStringLiteral("it is minimized");
    const auto *session = sessionForOutput(window->screen());
    if (!session) return QStringLiteral("no layout is on its display");
    if (session->applying) return QStringLiteral("its layout is being placed");
    if (!session->applicationToken) return QStringLiteral("its layout was never placed");
    return QStringLiteral("it is not a pane of its layout");
}

bool DesktopStageController::nativeCarrySourceValid(const PreparedCarrySource &source) const
{
    if (source.m_owner.lock() != m_carrySourceIdentity) return false;
    const auto current = prepareNativeCarrySource(source.m_window);
    if (current && source.m_desktopWindow && current->m_desktopWindow
        && source.m_window->isUserMove()) {
        // Native geometry/output may advance before deliberate entry. Identity,
        // generation and the original output topology must remain unchanged.
        auto *home = source.m_restore.output.data();
        return current->m_generation == source.m_generation && home
            && KWin::effects->screens().contains(home)
            && home->geometry() == source.m_outputGeometry;
    }
    return current && current->m_origin == source.m_origin
        && current->m_desktopWindow == source.m_desktopWindow
        && current->m_generation == source.m_generation
        && current->m_outputGeometry == source.m_outputGeometry
        && current->m_restore.output == source.m_restore.output;
}

std::vector<BentoOwnershipView> DesktopStageController::ownershipView() const
{
    std::vector<BentoOwnershipView> views;
    views.reserve(static_cast<std::size_t>(m_sessions.size()));
    const auto identity = [](const QPointer<KWin::EffectWindow> &window) {
        return reinterpret_cast<quintptr>(window.data());
    };
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        BentoOwnershipView view;
        view.output = it.key();
        for (const auto &window : it.value().windows) {
            if (window) view.panes.push_back(identity(window));
        }
        views.push_back(std::move(view));
    }
    return views;
}

QStringList DesktopStageController::outputStageState() const
{
    QStringList state;
    // "tablet" names the display in use, as the workspace context does. Each
    // display holding its own cards is the tablet to its own layout, so
    // asking each in turn would name every one of them.
    const KWin::LogicalOutput *inUse = m_host->tabletOutputForDesktopStage();
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const Session *session = sessionForOutput(output);
        const KWin::Rect geometry = output->geometry();
        state.append(QStringLiteral("%1|%2|%3,%4 %5x%6|%7")
            .arg(output->name(),
                 inUse && output == inUse
                     ? QStringLiteral("tablet")
                     : QStringLiteral("external"))
            .arg(geometry.x()).arg(geometry.y())
            .arg(geometry.width()).arg(geometry.height())
            .arg(session ? session->windows.size() : 0));
    }
    return state;
}

bool DesktopStageController::toggleOnOutput(const QString &outputName)
{
    KWin::LogicalOutput *output = outputForKey(outputName);
    if (!output) {
        return false;
    }
    if (m_sessions.contains(outputName)) {
        restoreSession(outputName);
        return true;
    }
    if (!m_host->allowsDesktopStageOnOutput(output)) {
        return false;
    }
    return activate(output, KWin::effects->activeWindow());
}

bool DesktopStageController::handoffLeadToOutput(
    const QString &sourceName, const QString &destinationName)
{
    const auto source = m_sessions.constFind(sourceName);
    KWin::LogicalOutput *destination = outputForKey(destinationName);
    if (source == m_sessions.cend() || !destination
        || sourceName == destinationName) {
        return false;
    }
    const auto lead = std::find_if(
        source->windows.cbegin(), source->windows.cend(),
        [](const QPointer<KWin::EffectWindow> &window) {
            return window && !window->isDeleted() && window->window();
        });
    if (lead == source->windows.cend()) {
        return false;
    }
    return handoffWindowToOutput(
        *lead, destination,
        KWin::RectF(m_host->activeTargetForDesktopStage(destination)));
}

bool DesktopStageController::hasActiveSession() const
{
    // Empty visible layout still owns restore records and must remain releasable.
    return !m_sessions.isEmpty();
}

bool DesktopStageController::ownsWindow(KWin::EffectWindow *window) const
{
    if (!window) return false;
    for (const auto &session : m_sessions)
        for (const auto &saved : session.snapshots)
            if (saved.valid && saved.window == window) return true;
    return false;
}

bool DesktopStageController::managesWindow(KWin::EffectWindow *window) const
{
    for (const Session &session : m_sessions) {
        if (session.windows.contains(window)) return true;
    }
    return false;
}

QString DesktopStageController::outputHoldingPane(const KWin::EffectWindow *window) const
{
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        for (const auto &pane : it.value().windows)
            if (pane && pane.data() == window) return it.key();
    }
    return {};
}

bool DesktopStageController::handoffPaneToOutput(KWin::EffectWindow *window,
    KWin::LogicalOutput *destination)
{
    if (m_restoring || !window || window->isDeleted() || !window->window() || !destination
        || window == m_interactionWindow) return false;
    QString sourceKey;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it)
        if (it->windows.contains(window)) sourceKey = it.key();
    if (sourceKey.isEmpty() || sourceKey == outputKey(destination)) return false;
    if (!handoffWindowToOutput(window, destination, window->frameGeometry())) {
        // A refused transfer keeps the layout it had, the pane back in it.
        qInfo() << "Kadunce" << Revision << "kept" << logNameOf(window) << "in the layout on"
                << sourceKey << "rather than hand it to" << outputKey(destination);
        auto source = m_sessions.find(sourceKey);
        if (source != m_sessions.end() && applySession(source.value(), false)) scheduleSettle();
    }
    return true;
}

bool DesktopStageController::hasSessionOnOutput(
    const QString &outputName) const
{
    return m_sessions.contains(outputName);
}

bool DesktopStageController::laysOutInZones(const QString &outputName) const
{
    const auto session = m_sessions.constFind(outputName);
    return session != m_sessions.cend() && !session->zones.isEmpty();
}

bool DesktopStageController::hasSettledSessionOnOutput(const QString &outputName) const
{
    const auto session = m_sessions.constFind(outputName);
    return session != m_sessions.cend() && !session->applying && !m_interactionWindow;
}

bool DesktopStageController::inPaneGutter(QPointF position) const
{
    for (const auto &session : m_sessions) {
        if (session.windows.size() < 2) continue;
        QRectF panes;
        bool onPane = false;
        for (const auto &window : session.windows) {
            if (!window || window->isDeleted()) continue;
            const QRectF frame(window->frameGeometry());
            panes = panes.united(frame);
            onPane = onPane || frame.contains(position);
        }
        if (!onPane && panes.contains(position)) return true;
    }
    return false;
}

bool DesktopStageController::presentsPlacedLayoutOn(const QString &outputName) const
{
    const auto session = m_sessions.constFind(outputName);
    return session != m_sessions.cend() && session->placed && !session->placed->paired;
}

std::optional<QRectF> DesktopStageController::openRoomOn(KWin::LogicalOutput *output) const
{
    const Session *session = output ? sessionForOutput(output) : nullptr;
    if (!session || !session->placed) return std::nullopt;
    const auto rooms = bentoColumnsOpenRooms(*session->placed);
    if (rooms.empty()) return std::nullopt;
    const KWin::Rect area = stageArea(output);
    const auto pixels = makePixelBentoLayout(rooms, area.x(), area.y(), area.width(), area.height());
    if (pixels.empty()) return std::nullopt;
    return QRectF(pixels.front().x, pixels.front().y, pixels.front().width, pixels.front().height);
}

QString DesktopStageController::outputKey(const KWin::LogicalOutput *output) const
{
    return output ? output->name() : QString();
}

KWin::LogicalOutput *DesktopStageController::outputForKey(const QString &key) const
{
    return key.isEmpty() ? nullptr : KWin::effects->findScreen(key);
}

KWin::Rect DesktopStageController::stageArea(KWin::LogicalOutput *output) const
{
    const KWin::Rect work = workspaceArea(output);
    if (work.isEmpty()) return {};
    const auto stage = makeBentoStageArea({double(work.x()), double(work.y()),
        double(work.width()), double(work.height())});
    return KWin::Rect(qRound(stage.x), qRound(stage.y),
        std::max(1, qRound(stage.width)), std::max(1, qRound(stage.height)));
}

KWin::Rect DesktopStageController::workspaceArea(KWin::LogicalOutput *output) const
{
    if (!output) return {};
    const KWin::RectF work = KWin::effects->clientArea(KWin::MaximizeArea, output);
    return KWin::Rect(qRound(work.x()), qRound(work.y()),
        std::max(1, qRound(work.width())), std::max(1, qRound(work.height())));
}

DesktopStageController::Session *DesktopStageController::sessionForOutput(
    KWin::LogicalOutput *output)
{
    const auto it = m_sessions.find(outputKey(output));
    return it == m_sessions.end() ? nullptr : &it.value();
}

const DesktopStageController::Session *DesktopStageController::sessionForOutput(
    const KWin::LogicalOutput *output) const
{
    const auto it = m_sessions.constFind(outputKey(output));
    return it == m_sessions.cend() ? nullptr : &it.value();
}

QList<QPointer<KWin::EffectWindow>> DesktopStageController::collectWindows(
    KWin::LogicalOutput *output, KWin::EffectWindow *preferred) const
{
    QList<QPointer<KWin::EffectWindow>> windows;
    if (!output) {
        return windows;
    }
    const QList<KWin::EffectWindow *> stacking = KWin::effects->stackingOrder();
    for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
        KWin::EffectWindow *window = *it;
        if (!m_host->isManagedWindowForDesktopStage(window) || window->screen() != output) {
            continue;
        }
        windows.append(window);
    }
    const auto footprint = [](const QPointer<KWin::EffectWindow> &window) {
        const KWin::RectF geometry = window->frameGeometry();
        return std::max(1.0, geometry.width())
            * std::max(1.0, geometry.height());
    };
    std::stable_sort(windows.begin(), windows.end(),
                     [&footprint](const auto &first, const auto &second) {
        return footprint(first) > footprint(second);
    });
    KWin::EffectWindow *lead = preferred;
    if (!lead || lead->screen() != output || !m_host->isManagedWindowForDesktopStage(lead)) {
        KWin::EffectWindow *active = KWin::effects->activeWindow();
        lead = active && active->screen() == output && m_host->isManagedWindowForDesktopStage(active)
            ? active : nullptr;
    }
    if (lead) {
        windows.removeAll(lead);
        windows.prepend(lead);
    }
    return windows;
}

DesktopStageController::RestoreSnapshot DesktopStageController::makeSnapshot(
    KWin::EffectWindow *window) const
{
    if (!window || !window->window()) {
        return {};
    }
    KWin::Window *client = window->window();
    if (const auto saved = m_host->activeRestoreForDesktopStage(window))
        return {saved->place(), window, true};
    return {placeOf(client, window->frameGeometry(), window->screen()), window, true};
}

bool DesktopStageController::handoffWindowToOutput(
    KWin::EffectWindow *window, KWin::LogicalOutput *destination,
    const KWin::RectF &requestedGeometry, CardDropIntent intent,
    std::optional<BentoSidePlacement> side)
{
    if (m_restoring) return false;
    if (!window || !window->window() || !destination) {
        return false;
    }
    QString sourceKey;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        const auto snapshot = std::find_if(
            it->snapshots.cbegin(), it->snapshots.cend(),
            [window](const RestoreSnapshot &item) {
                return item.window == window;
            });
        if (snapshot != it->snapshots.cend()) {
            sourceKey = it.key();
            break;
        }
    }
    const bool detach = intent == CardDropIntent::NativeDesktop;
    if (sourceKey.isEmpty() || (outputKey(destination) == sourceKey && !detach)
        || (detach && outputKey(destination) != sourceKey)) {
        return false;
    }
    // A display that keeps no layout takes no pane; its host places the window.
    if (!detach && !m_host->allowsDesktopStageOnOutput(destination)) return false;

    KWin::RectF destinationGeometry = requestedGeometry;
    if (!destination->geometry().intersects(destinationGeometry.toRect())) {
        destinationGeometry = KWin::RectF(m_host->activeTargetForDesktopStage(destination));
    }
    if (!detach && (sessionForOutput(destination)
        || intent == CardDropIntent::ActivateBento)) {
        const QString destinationKey = outputKey(destination);
        const RestoreSnapshot snapshot{WindowPlace{.geometry = destinationGeometry,
            .floatingGeometry = destinationGeometry, .outputName = destinationKey,
            .outputGeometry = QRectF(destination->geometry())}, window, true};
        const auto planner = [this, side, window, destinationKey](Session &session, const QPointer<KWin::EffectWindow> &preferred, bool required) {
                if (side && session.outputName == destinationKey) {
                    session.side = side; session.sideWindow = window;
                }
                return reflowSession(session, preferred, required, false);
            };
        std::optional<BentoSessionTransfer<Session>> transfer;
        QList<QPointer<KWin::EffectWindow>> unadopted;
        QList<PendingEviction> pending;
        if (m_sessions.contains(destinationKey)) {
            // CARD-LIFECYCLE.md §5: the destination yields a pane to the
            // arrival rather than parking it. The yield is chosen on a value
            // copy, so the two checks below can still refuse this transfer and
            // leave the destination exactly as it was.
            const Session live = m_sessions.value(destinationKey);
            Session shortened = live;
            if (!shortenToShowable(shortened, snapshot, side, window)) return false;
            transfer = prepareBentoSessionTransfer(m_sessions.value(sourceKey),
                shortened, QPointer<KWin::EffectWindow>(window), snapshot, planner);
            if (transfer) pending = captureEvictions(live, transfer->destination, window);
        } else {
            Session empty;
            empty.outputName = destinationKey;
            QList<RestoreSnapshot> residents;
            for (const auto &resident : collectWindows(destination, window))
                residents.append(makeSnapshot(resident));
            // §5 again, for a first layout: a resident this layout cannot show
            // is not adopted here at all. It is still Native, so it has nothing
            // to give up, and card ownership takes it once both sessions are
            // published.
            Session probe = empty;
            probe.snapshots = residents;
            probe.snapshots.append(snapshot);
            if (side) { probe.side = side; probe.sideWindow = window; }
            if (planSession(probe, window, true, &unadopted)) {
                residents.removeIf([&unadopted](const auto &resident) {
                    return unadopted.contains(resident.window);
                });
            }
            const auto admission = prepareBentoActivationWithArrival(empty, residents,
                QPointer<KWin::EffectWindow>(window), snapshot, planner);
            const auto departure = prepareBentoDeparture(m_sessions.value(sourceKey),
                QPointer<KWin::EffectWindow>(window), planner);
            if (admission && departure) transfer = BentoSessionTransfer<Session>{*departure, *admission};
        }
        if (!transfer) return false;
        // No native calls or callbacks between these publications. Destination
        // feasibility was established while both original sessions were intact.
        // Both sessions are published together; the yielded pane is owned by
        // neither until card ownership takes it, and the two applySession
        // checks below can return before the end of this function.
        bool publishedPlan = false;
        const auto handOver = qScopeGuard([&] { if (publishedPlan) publishEvictions(pending); });
        m_sessions[destinationKey] = transfer->destination;
        const bool emptied = transfer->source.snapshots.isEmpty();
        if (emptied) m_sessions.remove(sourceKey);
        else m_sessions[sourceKey] = transfer->source;
        publishedPlan = true;
        // Re-fetch after every native application; it can emit lifecycle signals.
        auto source = m_sessions.find(sourceKey);
        if (source != m_sessions.end() && !applySession(source.value(), false)) return true;
        auto target = m_sessions.find(destinationKey);
        if (target == m_sessions.end() || !applySession(target.value(), true)) return true;
        scheduleSettle();
        // The yielded pane leaves through the scope guard above, which runs on
        // every exit that published. These were never adopted at all.
        for (const auto &resident : std::as_const(unadopted)) {
            if (!resident || resident->isDeleted() || !resident->window()) continue;
            if (parksOverflow(destinationKey)) {
                parkWindow(destinationKey, resident);
                continue;
            }
            m_host->admitTransferredWindowToTablet(resident, outputForKey(destinationKey),
                [] { return true; });
        }
        // §5: the display the pane left may now hold one, which ends its Bento.
        if (emptied) retireEmptiedLayout(sourceKey);
        endLayoutIntoCardOwnership(sourceKey);
        return true;
    }
    if (detach || !m_host->isTabletOutputForDesktopStage(destination)) {
        QPointer<KWin::LogicalOutput> target = destination;
        QPointer<KWin::EffectWindow> arrival = window;
        QPointer<KWin::Window> client = window->window();
        if (window->isDeleted() || window->isUserMove() || window->isUserResize()
            || m_retiredOutputs.contains(target) || !KWin::effects->screens().contains(target)
            || !destinationGeometry.isValid()) return false;
        const auto departure = prepareBentoDeparture(m_sessions.value(sourceKey), arrival,
            [this](Session &session, const auto &preferred, bool required) {
                return reflowSession(session, preferred, required, false);
            });
        if (!departure) return false;
        // Ordinary desktop has no layout admission. Validate its output first,
        // prepare source departure by value, then publish before native calls.
        const bool emptied = departure->snapshots.isEmpty();
        if (emptied) m_sessions.remove(sourceKey);
        else m_sessions[sourceKey] = *departure;
        const auto token = m_applicationGuard.issue();
        const auto valid = [&] {
            return m_applicationGuard.accepts(token) && !m_restoring
                && target && !m_retiredOutputs.contains(target)
                && KWin::effects->screens().contains(target.data())
                && arrival && !arrival->isDeleted() && client && arrival->window() == client
                && !arrival->isUserMove() && !arrival->isUserResize();
        };
        if (!applyNativePlacement(client.data(), target.data(), destinationGeometry, valid)) return true;
        KWin::workspace()->raiseWindow(client);
        if (!valid()) return true;
        KWin::workspace()->activateWindow(client, true);
        if (!valid()) return true;
        auto source = m_sessions.find(sourceKey);
        if (source != m_sessions.end() && !applySession(source.value(), false)) return true;
        scheduleSettle();
        // §13 released only the carried window. §5 then ends a layout the
        // departure left with one pane.
        if (emptied) retireEmptiedLayout(sourceKey);
        endLayoutIntoCardOwnership(sourceKey);
        return true;
    }
    const auto departure = prepareBentoDeparture(m_sessions.value(sourceKey),
        QPointer<KWin::EffectWindow>(window),
        [this](Session &session, const auto &preferred, bool required) {
            return reflowSession(session, preferred, required);
        });
    if (!departure) return false;
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::LogicalOutput> target = destination;
    const auto token = m_applicationGuard.issue();
    bool committed = false;
    const bool accepted = m_host->admitTransferredWindowToTablet(window, destination, [&] {
        if (committed || !m_applicationGuard.accepts(token) || m_restoring
            || !arrival || arrival->isDeleted() || !arrival->window()
            || arrival->isUserMove() || arrival->isUserResize()
            || !target || m_retiredOutputs.contains(target)
            || !KWin::effects->screens().contains(target.data()) || !m_sessions.contains(sourceKey)) return false;
        if (departure->snapshots.isEmpty()) m_sessions.remove(sourceKey);
        else m_sessions[sourceKey] = *departure;
        committed = true;
        return true;
    });
    if (!committed) return false;
    // Acceptance is permission to commit, not asynchronous placement proof.
    // Never return rejection after source publication and trigger a stale replay.
    if (!accepted || !m_applicationGuard.accepts(token)) return true;
    auto source = m_sessions.find(sourceKey);
    if (source != m_sessions.end() && !applySession(source.value(), false)) return true;
    scheduleSettle();
    // §5: a layout the pane left with one pane ends, as above; the display
    // taking it shows it as a card.
    if (departure->snapshots.isEmpty()) retireEmptiedLayout(sourceKey);
    endLayoutIntoCardOwnership(sourceKey);
    return true;
}

void DesktopStageController::retireEmptiedLayout(const QString &key)
{
    // §11: a layout its last pane left for another display presents nothing
    // there, so that display's cards can be shown again, the pane among them
    // when it comes back.
    if (m_sessions.contains(key)) return;
    if (KWin::LogicalOutput *output = outputForKey(key)) m_host->layoutLeftForOtherDisplayForDesktopStage(output);
}

void DesktopStageController::removeWindow(KWin::EffectWindow *window,
                                   bool restoreSnapshot)
{
    if (!window) {
        m_applicationGuard.invalidate();
        return;
    }
    QString sourceKey;
    RestoreSnapshot removedSnapshot;
    bool wasShown = false;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        Session &session = it.value();
        auto snapshot = std::find_if(
            session.snapshots.begin(), session.snapshots.end(),
            [window](const RestoreSnapshot &item) {
                return item.window == window;
            });
        if (snapshot == session.snapshots.end()) {
            continue;
        }
        sourceKey = it.key();
        removedSnapshot = *snapshot;
        wasShown = session.windows.contains(window);
        session.snapshots.erase(snapshot);
        // A pane's zone and rectangle stand at its index: they leave with it,
        // or every later pane reads its neighbour's zone.
        const int index = session.windows.indexOf(window);
        if (index >= 0 && index < static_cast<int>(session.rects.size()))
            session.rects.erase(session.rects.begin() + index);
        if (index >= 0 && index < session.zones.size()) session.zones.removeAt(index);
        session.windows.removeAll(window);
        break;
    }
    // Only a window a layout holds changes a layout. A popup or a notification
    // closing elsewhere must not retire pending commands or a prepared carry.
    if (sourceKey.isEmpty()) {
        return;
    }
    m_applicationGuard.invalidate();
    if (restoreSnapshot && removedSnapshot.valid
        && removedSnapshot.window && removedSnapshot.window->window()) {
        // The window leaves as restoreSession gives one back: on the display
        // it came from while that is attached, maximized, tiled, full screen
        // or minimized as it was before the layout.
        QPointer<KWin::EffectWindow> leaving = removedSnapshot.window;
        QPointer<KWin::Window> client = leaving->window();
        KWin::LogicalOutput *original = outputForKey(removedSnapshot.outputName);
        KWin::LogicalOutput *target = original ? original : leaving->screen();
        if (!target) target = m_host->tabletOutputForDesktopStage();
        const bool preserveGeometry = target && target == original;
        const KWin::RectF geometry = givenBackPlace(removedSnapshot, target, preserveGeometry);
        if (target) client->sendToOutput(target);
        const auto valid = [&] {
            return leaving && !leaving->isDeleted() && client && leaving->window() == client;
        };
        if (valid() && restoreWindowStateChecked(client.data(), removedSnapshot, geometry,
                           preserveGeometry, true, false, valid))
            (void)reminimizeRestored(client.data(), removedSnapshot);
    }
    auto source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    if (source->snapshots.isEmpty()) {
        // §11, Connected: a placed card was the layout's last pane, so the
        // display no longer presents one.
        const bool placed = source->placed.has_value();
        m_sessions.erase(source);
        KWin::LogicalOutput *output = outputForKey(sourceKey);
        if (placed && output && !m_restoring) m_host->retireOutputFromDesktopStage(output);
        return;
    }
    reflowSession(source.value());
    // DECISIONS.md § A display without cards organizes everything it shows: a
    // window waiting in the dock comes back when a pane leaves room for it.
    const bool parked = std::any_of(source->snapshots.cbegin(), source->snapshots.cend(),
        [](const auto &saved) { return saved.parked; });
    if (wasShown && parked && !m_restoring && parksOverflow(sourceKey)) wakeParked(sourceKey);
    // CARD-LIFECYCLE.md §12: a closure that leaves one pane ends Bento, and
    // that pane becomes an individual card rather than being laid out again.
    endLayoutIntoCardOwnership(sourceKey);
    auto surviving = m_sessions.find(sourceKey);
    if (surviving != m_sessions.end() && applySession(surviving.value(), false))
        scheduleSettle();
}

void DesktopStageController::handleScreenRemoved(KWin::LogicalOutput *output)
{
    std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
        return pending->output() == output || pending->result() != RestoredMinimization::Result::Pending;
    });
    if (!output) {
        return;
    }
    m_retiredOutputs.removeIf([](const auto &entry) { return !entry; });
    if (!m_retiredOutputs.contains(output)) m_retiredOutputs.append(output);
    const QString key = outputKey(output);
    if (m_sessions.contains(key)) {
        restoreSession(key, true);
    }
}

std::vector<BentoPixelRect> DesktopStageController::sessionPixels(const Session &session,
                                                                  const KWin::Rect &area) const
{
    auto pixels = makePixelBentoLayout(session.rects, area.x(), area.y(), area.width(), area.height());
    for (int index = 0; index < session.zones.size() && index < int(pixels.size()); ++index) {
        const QRectF rect = zoneWindowRect(session.zones.at(index));
        if (rect.width() >= 1 && rect.height() >= 1) pixels[std::size_t(index)] = pixelRect(rect);
    }
    return pixels;
}

void DesktopStageController::syncZonesFromKde(Session &session) const
{
    for (int index = 0; index < session.windows.size() && index < session.zones.size(); ++index) {
        const auto &window = session.windows.at(index);
        if (!window || window->isDeleted() || !window->window()) continue;
        QObject *tile = zoneOf(window->window());
        if (!tile || tile == session.zones.at(index)) continue;
        const int other = session.zones.indexOf(tile);
        if (other >= 0) {
            session.zones.swapItemsAt(index, other);
            std::swap(session.rects[std::size_t(index)], session.rects[std::size_t(other)]);
        } else {
            session.zones[index] = tile;
        }
    }
}

void DesktopStageController::zoneModeChanged()
{
    const auto keys = m_sessions.keys();
    for (const auto &key : keys) {
        if (!canTakeDrawnZones(key)) continue;
        auto found = m_sessions.find(key);
        if (found == m_sessions.end() || found->windows.isEmpty()) continue;
        // The new arrangement may have room for a window the dock holds; on a
        // Connected display, a pane it has no room for goes to Spread.
        if (!wakeParked(key)) continue;
        found = m_sessions.find(key);
        if (found != m_sessions.end() && applySession(found.value(), false)) scheduleSettle();
    }
    releaseStrayZones();
}

bool DesktopStageController::wakeParked(const QString &key)
{
    auto found = m_sessions.find(key);
    if (found == m_sessions.end()) return false;
    // A window the dock holds is asked again. What it cannot show waits in the
    // dock, as it does for any arrival, before the rest is laid out again.
    QList<QPointer<KWin::EffectWindow>> waking;
    for (auto &saved : found->snapshots)
        if (saved.parked && saved.window) { saved.parked = false; waking.append(saved.window); }
    // A layout the dock emptied is led by the first window it holds there.
    const QPointer<KWin::EffectWindow> lead = !found->windows.isEmpty()
        ? found->windows.first() : waking.isEmpty() ? nullptr : waking.first();
    if (!lead || !shedUnshowable(key, found.value(), lead, false)) return false;
    found = m_sessions.find(key);
    if (found == m_sessions.end() || !reflowSession(found.value(), lead, false, true)) return false;
    // Kadunce's own unminimize is not the person's, as its minimize is not.
    const bool wasParking = std::exchange(m_parking, true);
    for (const auto &window : std::as_const(waking))
        if (window && window->window() && found->windows.contains(window)) window->window()->setMinimized(false);
    m_parking = wasParking;
    return true;
}

void DesktopStageController::releaseStrayZones()
{
    m_zoned.removeIf([this](const QPointer<KWin::Window> &client) {
        if (!client) return true;
        for (const auto &session : std::as_const(m_sessions)) {
            if (session.zones.isEmpty()) continue;
            for (const auto &window : session.windows)
                if (window && window->window() == client) return false;
        }
        leaveZone(client);
        return true;
    });
}

} // namespace Kadunce

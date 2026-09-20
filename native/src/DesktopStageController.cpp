/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DesktopStageController.h"
#include "BentoCompositeGeometry.h"
#include "BentoSessionTransfer.h"
#include "OwnershipHandoff.h"
#include "NativePlacement.h"
#include "WindowStateRestore.h"
#include "RestoreOutputPlan.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QSet>
#include <QScopeGuard>
#include <QScopedValueRollback>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Kadunce
{

namespace
{
constexpr auto Revision = "0.1.0-kadunce-baseline";
}

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
    if ((!session || !managesWindow(window)) && m_host->isManagedWindowForDesktopStage(window)
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
        source.m_restore = {window->window(), window->screen(), saved.geometry,
            saved.floatingGeometry, saved.fullscreenRestoreGeometry, saved.maximizeMode,
            saved.quickTileMode, saved.fullScreen, saved.minimized};
        return source;
    }
    if (!session || session->applying || !session->applicationToken
        || !session->windows.contains(window)) return std::nullopt;
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
        source.m_restore = {window->window(), outputForKey(saved.outputName), saved.geometry,
            saved.floatingGeometry, saved.fullscreenRestoreGeometry, saved.maximizeMode,
            saved.quickTileMode, saved.fullScreen, saved.minimized};
        return source;
    }
    return std::nullopt;
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
        for (const auto &window : it.value().overflow) {
            if (window) view.overflow.push_back(identity(window));
        }
        views.push_back(std::move(view));
    }
    return views;
}

QStringList DesktopStageController::outputStageState() const
{
    QStringList state;
    for (KWin::LogicalOutput *output : KWin::effects->screens()) {
        const Session *session = sessionForOutput(output);
        const KWin::Rect geometry = output->geometry();
        state.append(QStringLiteral("%1|%2|%3,%4 %5x%6|%7|%8")
            .arg(output->name(),
                 m_host->isTabletOutputForDesktopStage(output)
                     ? QStringLiteral("tablet")
                     : QStringLiteral("external"))
            .arg(geometry.x()).arg(geometry.y())
            .arg(geometry.width()).arg(geometry.height())
            .arg(session ? session->windows.size() : 0)
            .arg(session ? session->overflow.size() : 0));
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

bool DesktopStageController::handleWindowAdded(KWin::EffectWindow *window)
{
    if (m_restoring) return false;
    if (ownsWindow(window)) return true;
    if (!window || !m_host->isManagedWindowForDesktopStage(window)) {
        return false;
    }
    Session *session = sessionForOutput(window->screen());
    if (!session) {
        return false;
    }
    for (const auto &snapshot : std::as_const(session->snapshots))
        if (snapshot.window == window) return true;
    // Keep ordinary monitor admission unchanged. Tablet launches must remain
    // owned even when no curated pane can satisfy the native minimum size.
    Session candidate = *session;
    candidate.snapshots.append(makeSnapshot(window));
    bool visible = reflowSession(candidate, window, true, false);
    if (!visible && candidate.side) {
        candidate.side.reset();
        candidate.sideWindow.clear();
        visible = reflowSession(candidate, window, true, false);
    }
    if (!visible) {
        if (!m_host->isTabletOutputForDesktopStage(window->screen())) return true;
        candidate = *session;
        candidate.snapshots.append(makeSnapshot(window));
        candidate.overflow.append(window);
    }
    m_applicationGuard.invalidate();
    *session = std::move(candidate);
    if (applySession(*session, visible, visible ? nullptr : window)) scheduleSettle();
    return true;
}

void DesktopStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    removeWindow(window, false);
}

void DesktopStageController::handleWindowMinimizedChanged(KWin::EffectWindow *window)
{
    if (m_restoring || !window || !window->window()) return;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto &session = it.value();
        // Placement/overflow minimization is not a user participation change.
        if (session.applying) continue;
        // Deferred preparation minimizes only an already-owned overflow card.
        if (window->isMinimized() && session.overflow.contains(window)) continue;
        auto saved = std::find_if(session.snapshots.begin(), session.snapshots.end(),
            [window](const auto &s) { return s.window == window; });
        if (saved == session.snapshots.end()) continue;
        saved->userMinimized = window->isMinimized();
        session.participationDirty = true;
        m_applicationGuard.invalidate();
        if (m_interactionWindow) return;
        if (applySession(session, false)) scheduleSettle();
        return;
    }
}

bool DesktopStageController::admitCardWindow(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry)
{
    if (m_restoring) return false;
    Session *session = sessionForOutput(output);
    if (!session || !window || window->isDeleted() || !window->window()) {
        return false;
    }
    for (const auto &existing : std::as_const(session->snapshots)) {
        if (existing.window == window) return session->windows.contains(window);
    }
    const RestoreSnapshot snapshot{
        .window = window,
        .geometry = geometry,
        .outputName = outputKey(output),
        .quickTileMode = {},
        .maximizeMode = KWin::MaximizeRestore,
        .fullScreen = false,
        .minimized = false,
        .valid = true,
    };
    Session candidate = *session;
    candidate.snapshots.append(snapshot);
    // Solve on a value copy: rejection must not park the arrival or disturb
    // the destination's existing windows, restore records or geometry.
    if (!reflowSession(candidate, window, true)) return false;
    *session = std::move(candidate);
    if (applySession(*session, true)) scheduleSettle();
    return true;
}

std::optional<DesktopStageController::PreparedDrop> DesktopStageController::prepareCardDrop(
    KWin::EffectWindow *window, KWin::LogicalOutput *output, const KWin::RectF &geometry,
    CardDropIntent intent, std::optional<BentoSidePlacement> side,
    KWin::EffectWindow *pairPartner) const
{
    if (m_restoring || !window || window->isDeleted() || !window->window()
        || !output || m_retiredOutputs.contains(output)
        || !KWin::effects->screens().contains(output) || !geometry.isValid()
        || !std::isfinite(geometry.x()) || !std::isfinite(geometry.y())
        || !std::isfinite(geometry.width()) || !std::isfinite(geometry.height())
        || !m_host->allowsDesktopStageOnOutput(output)
        || !m_host->isManagedWindowForDesktopStage(window)
        || (intent != CardDropIntent::OpenSpace && intent != CardDropIntent::ActivateBento
            && intent != CardDropIntent::NativeDesktop))
        return std::nullopt;
    // An ordinary source may finish its held carry on a free external output.
    // Keep managed departures and occupied/tablet destinations on their
    // workspace admission path; NativeDesktop must not bypass that ownership.
    if (intent == CardDropIntent::NativeDesktop && window->screen() != output
        && (managesWindow(window) || sessionForOutput(output)
            || m_host->isTabletOutputForDesktopStage(output)))
        return std::nullopt;
    PreparedDrop drop;
    drop.window = window;
    drop.output = output;
    drop.geometry = geometry;
    drop.outputGeometry = output->geometry();
    drop.area = stageArea(output);
    drop.generation = m_applicationGuard.generation();
    drop.owner = m_carrySourceIdentity;
    drop.intent = intent;
    drop.side = side;
    drop.hadSession = sessionForOutput(output) != nullptr;
    drop.leavingBento = managesWindow(window);
    // §3: a pairing names its partner, so nothing else on the display is an
    // input to the solve, the preview or the revalidation.
    if (pairPartner && !drop.hadSession) {
        if (pairPartner == window || pairPartner->isDeleted() || !pairPartner->window()
            || pairPartner->screen() != output || pairPartner->isMinimized()
            || !m_host->isManagedWindowForDesktopStage(pairPartner)) return std::nullopt;
        drop.pairPartner = pairPartner;
        drop.residents = {QPointer<KWin::EffectWindow>(pairPartner)};
    } else {
        drop.residents = collectWindows(output, window);
    }
    auto inputs = drop.residents;
    if (!inputs.contains(window)) inputs.append(window);
    for (const auto &input : inputs) {
        if (!input || !input->window()) return std::nullopt;
        drop.minimumSizes.append(input->window()->minSize());
        drop.sourceGeometries.append(input->frameGeometry());
    }
    return drop;
}

std::optional<DesktopStageController::PreparedDrop> DesktopStageController::prepareLocalCardDrop(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, QPointF contact) const
{
    auto drop = prepareCardDrop(window, output, geometry);
    const auto *session = sessionForOutput(output);
    if (!drop || !session || !session->windows.contains(window)) return std::nullopt;
    const auto pixels = makePixelBentoLayout(session->rects, drop->area.x(), drop->area.y(),
        drop->area.width(), drop->area.height());
    for (int i = 0; i < session->windows.size() && i < static_cast<int>(pixels.size()); ++i) {
        const auto &p = pixels.at(i);
        if (!QRectF(p.x, p.y, p.width, p.height).contains(contact)) continue;
        drop->localTarget = session->windows.at(i);
        return prepareLocalPlacement(*drop) ? drop : std::nullopt;
    }
    return std::nullopt;
}

std::optional<DesktopStageController::Session> DesktopStageController::prepareLocalPlacement(
    const PreparedDrop &drop) const
{
    const auto *session = sessionForOutput(drop.output);
    if (!session || session->applying) return std::nullopt;
    if (drop.side && session->windows.contains(drop.window)) {
        Session plan = *session;
        plan.side = drop.side;
        plan.sideWindow = drop.window;
        return planSession(plan, drop.window, true)
            ? std::optional<Session>(plan) : std::nullopt;
    }
    if (!drop.localTarget) return std::nullopt;
    const int from = session->windows.indexOf(drop.window);
    const int to = session->windows.indexOf(drop.localTarget);
    const auto pixels = makePixelBentoLayout(session->rects, drop.area.x(), drop.area.y(),
        drop.area.width(), drop.area.height());
    if (from < 0 || to < 0 || from >= static_cast<int>(pixels.size())
        || to >= static_cast<int>(pixels.size())) return std::nullopt;
    const auto fits = [&](const auto &window, int index) {
        if (!window || window->isDeleted() || !window->window()
            || window->isUserMove() || window->isUserResize()) return false;
        const auto size = window->window()->minSize();
        return size.width() <= pixels.at(index).width && size.height() <= pixels.at(index).height;
    };
    if (!fits(drop.window, to) || !fits(drop.localTarget, from)) return std::nullopt;
    Session result = *session;
    result.side.reset();
    result.sideWindow.clear();
    result.windows.swapItemsAt(from, to);
    // Carry the original restore records with their identities; never replace
    // them with pane geometry. Keep collection order aligned for later reflow.
    auto a = std::find_if(result.snapshots.begin(), result.snapshots.end(),
        [&](const auto &s) { return s.window == drop.window; });
    auto b = std::find_if(result.snapshots.begin(), result.snapshots.end(),
        [&](const auto &s) { return s.window == drop.localTarget; });
    if (a == result.snapshots.end() || b == result.snapshots.end()) return std::nullopt;
    std::iter_swap(a, b);
    return result;
}

bool DesktopStageController::cardDropValid(const PreparedDrop &drop) const
{
    if (!drop.consumed || *drop.consumed || drop.owner.lock() != m_carrySourceIdentity)
        return false;
    const auto current = prepareCardDrop(drop.window, drop.output, drop.geometry,
        drop.intent, drop.side, drop.pairPartner);
    if (!current || current->generation != drop.generation
        || current->pairPartner != drop.pairPartner
        || current->outputGeometry != drop.outputGeometry || current->area != drop.area
        || current->hadSession != drop.hadSession
        || current->leavingBento != drop.leavingBento
        || current->residents != drop.residents) return false;
    const bool sameInputs = current->minimumSizes == drop.minimumSizes
        && current->sourceGeometries == drop.sourceGeometries;
    // First-layout edge entry is re-solved by transferCardWindow immediately
    // before publication. A configure acknowledgement or still-feasible minimum
    // hint update must not invalidate otherwise unchanged receiver ownership.
    const bool refreshableFirstEdge = drop.intent == CardDropIntent::ActivateBento
        && !drop.hadSession && !drop.leavingBento && !drop.localTarget;
    return (sameInputs || refreshableFirstEdge)
        && (!drop.localTarget || prepareLocalPlacement(drop).has_value());
}

bool DesktopStageController::transferNativeCarryToDesktop(
    const PreparedCarrySource &source, const PreparedDrop &drop)
{
    if (!nativeCarrySourceValid(source) || !cardDropValid(drop)
        || source.m_window != drop.window
        || drop.window->isUserMove() || drop.window->isUserResize()
        || !drop.outputGeometry.intersects(drop.geometry.toRect())) return false;
    if (drop.intent == CardDropIntent::NativeDesktop && !source.isDesktopWindow()) {
        if (source.origin().output != outputKey(drop.output)) return false;
        *drop.consumed = true;
        return handoffWindowToOutput(drop.window, drop.output, drop.geometry, drop.intent);
    }
    if (source.isDesktopWindow()) {
        if (drop.localTarget) return false;
        // No source layout exists to remove from. Preserve the pickup snapshot
        // if this admission creates Bento; carried geometry is presentation only.
        PreparedDrop validation = drop;
        validation.consumed = std::make_shared<bool>(false);
        *drop.consumed = true;
        if (m_host->isTabletOutputForDesktopStage(drop.output) && !drop.hadSession
            && drop.intent == CardDropIntent::OpenSpace
            && source.origin().output != outputKey(drop.output)) {
            bool committed = false;
            m_host->admitTransferredWindowToTablet(drop.window, [&] {
                if (committed || !nativeCarrySourceValid(source) || !cardDropValid(validation)) return false;
                committed = true;
                return true;
            });
            return committed;
        }
        return transferCardWindow(drop.window, drop.output, drop.geometry,
            [&] { return nativeCarrySourceValid(source) && cardDropValid(validation); },
            [] {}, drop.intent, &source.restoreSnapshot(), drop.side);
    }
    if (source.origin().output == outputKey(drop.output)) {
        const auto placement = prepareLocalPlacement(drop);
        if (!placement) return false;
        *drop.consumed = true;
        if (!drop.side && drop.localTarget == drop.window) return true; // Already physically home.
        auto *session = sessionForOutput(drop.output);
        *session = *placement; // Publish before native callbacks can intervene.
        if (applySession(*session, false)) scheduleSettle();
        return true; // Publication cannot become a stale source rollback.
    }
    if (drop.localTarget) return false;
    // The existing Bento transaction solves source and receiver on value copies
    // and publishes both without callbacks/native writes between them. Reserve
    // that exact transaction, not a second departure/placement implementation.
    *drop.consumed = true;
    return handoffWindowToOutput(drop.window, drop.output, drop.geometry, drop.intent, drop.side);
}

std::optional<KWin::RectF> DesktopStageController::cardDropPreview(const PreparedDrop &drop)
{
    if (drop.intent == CardDropIntent::NativeDesktop)
        return cardDropValid(drop) ? std::optional<KWin::RectF>(drop.geometry) : std::nullopt;
    if (!cardDropValid(drop) || (m_host->isTabletOutputForDesktopStage(drop.output)
        && drop.intent == CardDropIntent::OpenSpace && !drop.hadSession))
        return std::nullopt; // Tablet arrival has its own presentation owner.
    if (!drop.hadSession && drop.intent == CardDropIntent::OpenSpace) return drop.geometry;
    const bool local = drop.localTarget || (drop.side && managesWindow(drop.window)
        && drop.window->screen() == drop.output);
    const auto plan = local ? prepareLocalPlacement(drop)
        : prepareCardAdmission(drop.window, drop.output, drop.geometry, nullptr,
                               drop.side, drop.pairPartner);
    if (!plan) return std::nullopt;
    const auto index = plan->windows.indexOf(drop.window);
    const auto pixels = makePixelBentoLayout(plan->rects, drop.area.x(), drop.area.y(),
        drop.area.width(), drop.area.height());
    if (index < 0 || index >= static_cast<int>(pixels.size())) return std::nullopt;
    const auto &pixel = pixels.at(index);
    return KWin::RectF(pixel.x, pixel.y, pixel.width, pixel.height);
}

bool DesktopStageController::activatePreparedTabletDrop(const PreparedDrop &drop,
    const NativeMoveSnapshot *restore, const std::function<bool()> &commitSource)
{
    if (m_restoring || !commitSource || !cardDropValid(drop)
        || !m_host->isTabletOutputForDesktopStage(drop.output)
        || drop.intent != CardDropIntent::ActivateBento || drop.hadSession
        || !drop.side || !drop.pairPartner || drop.localTarget) return false;
    // CARD-LIFECYCLE.md §3: Bento begins with the carried window on the side it
    // was released into and the Active card opposite it. No other window joins,
    // so the plan is solved from exactly those two and is refused if it is not
    // two panes. Nothing else on the display is read, moved or minimized.
    // A window arriving from the desktop brings its own record; a card already
    // owned here has one the card stage holds, which `makeSnapshot` reads.
    const auto plan = prepareCardAdmission(drop.window, drop.output, drop.geometry,
        restore, drop.side, drop.pairPartner);
    if (!plan || plan->windows.size() != 2 || !plan->overflow.isEmpty()
        || !plan->windows.contains(drop.window)
        || !plan->windows.contains(drop.pairPartner)) return false;
    std::erase_if(m_restoredMinimizations, [&drop](const auto &pending) {
        return pending->output() == drop.output
            || pending->result() != RestoredMinimization::Result::Pending;
    });
    *drop.consumed = true;
    // Destination acceptance is complete before the source gives the pair up,
    // and publication follows with no callback in between.
    if (!commitSource()) return false;
    m_sessions.insert(plan->outputName, *plan);
    Session &stored = m_sessions[plan->outputName];
    if (applySession(stored, true)) scheduleSettle();
    qInfo() << "Kadunce" << Revision << "paired two cards into output-local Bento";
    return true;
}

std::optional<DesktopStageController::Session> DesktopStageController::prepareCardAdmission(
    KWin::EffectWindow *window, KWin::LogicalOutput *output, const KWin::RectF &geometry,
    const NativeMoveSnapshot *restore, std::optional<BentoSidePlacement> side,
    KWin::EffectWindow *pairPartner)
{
    const QString key = outputKey(output);
    const auto managed = m_host->activeRestoreForDesktopStage(window);
    if (!restore && managed) restore = &*managed;
    RestoreSnapshot incoming{
        .window = window, .geometry = geometry, .floatingGeometry = geometry,
        .fullscreenRestoreGeometry = {}, .outputName = key, .quickTileMode = {},
        .maximizeMode = KWin::MaximizeRestore, .fullScreen = false,
        .minimized = false, .valid = true};
    if (restore) {
        incoming = {.window = window, .geometry = restore->geometry,
            .floatingGeometry = restore->floatingGeometry,
            .fullscreenRestoreGeometry = restore->fullscreenRestoreGeometry,
            .outputName = outputKey(restore->output), .quickTileMode = restore->quickTileMode,
            .maximizeMode = restore->maximizeMode, .fullScreen = restore->fullScreen,
            .minimized = restore->minimized, .valid = true};
    }
    const auto planner = [this, side, window](Session &plan, const auto &preferred, bool required) {
        if (side) { plan.side = side; plan.sideWindow = window; }
        return reflowSession(plan, preferred, required, false);
    };
    if (const auto *session = sessionForOutput(output))
        return prepareBentoAdmission(*session, QPointer<KWin::EffectWindow>(window), incoming, planner);
    Session empty;
    empty.outputName = key;
    QList<RestoreSnapshot> owned;
    // §3: first entry pairs the arrival with one named card and fills no other
    // pane. Without a partner this is an ordinary display-wide activation.
    if (pairPartner) owned.append(makeSnapshot(pairPartner));
    else for (const auto &resident : collectWindows(output, window)) owned.append(makeSnapshot(resident));
    return prepareBentoActivationWithArrival(empty, owned,
        QPointer<KWin::EffectWindow>(window), incoming, planner);
}

bool DesktopStageController::transferPreparedCard(const PreparedDrop &drop,
    const std::function<bool()> &commitSource, const std::function<void()> &releaseSource)
{
    if (!commitSource || !releaseSource || !cardDropValid(drop)) return false;
    // Consume before any caller callback, but revalidate the original receiver
    // immediately before source commitment. Solving still occurs on value copies.
    PreparedDrop validation = drop;
    validation.consumed = std::make_shared<bool>(false);
    *drop.consumed = true;
    return transferCardWindow(drop.window, drop.output, drop.geometry, [&] {
        return cardDropValid(validation) && commitSource();
    }, releaseSource, drop.intent, nullptr, drop.side);
}

bool DesktopStageController::transferCardWindow(KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry, const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource, CardDropIntent intent,
    const NativeMoveSnapshot *restore, std::optional<BentoSidePlacement> side)
{
    if (m_restoring || !commitSource || !releaseSource || !window || window->isDeleted() || !window->window()
        || window->isUserMove() || window->isUserResize() || !output || !geometry.isValid()
        || m_retiredOutputs.contains(output) || !KWin::effects->screens().contains(output)) return false;
    if (intent != CardDropIntent::OpenSpace && intent != CardDropIntent::ActivateBento
        && intent != CardDropIntent::NativeDesktop) return false;
    if (intent == CardDropIntent::NativeDesktop && managesWindow(window)) return false;
    if (intent == CardDropIntent::ActivateBento
        && (!m_host->allowsDesktopStageOnOutput(output)
            || !m_host->isManagedWindowForDesktopStage(window))) return false;
    const QString key = outputKey(output);
    std::optional<Session> candidate;
    if (intent != CardDropIntent::NativeDesktop
        && (sessionForOutput(output) || intent == CardDropIntent::ActivateBento)) {
        candidate = prepareCardAdmission(window, output, geometry, restore, side);
        if (!candidate) return false;
    }
    // Existing Bento takes priority. Its rejection must never become a native
    // desktop fallback. Neither source nor destination is mutated during solve.
    if (!commitSource()) return false;
    if (candidate) m_sessions[key] = std::move(*candidate);
    const auto token = m_applicationGuard.issue();
    QPointer<KWin::EffectWindow> arrival = window;
    QPointer<KWin::Window> client = window->window();
    QPointer<KWin::LogicalOutput> target = output;
    releaseSource();
    const auto valid = [&] {
        return m_applicationGuard.accepts(token) && !m_restoring && arrival && !arrival->isDeleted()
            && client && arrival->window() == client && target && !m_retiredOutputs.contains(target)
            && KWin::effects->screens().contains(target.data())
            && !arrival->isUserMove() && !arrival->isUserResize();
    };
    if (!valid()) return true; // Logical commit happened; never replay source removal.
    if (candidate) {
        if (restore && restore->output != target && m_host->isTabletOutputForDesktopStage(target)) {
            // Committed ordinary monitor arrival into existing tablet Bento has
            // the same receiver-origin contract as Spread adoption (A1).
            // Ask KWin for tablet placement before overwriting it with a pane.
            client->sendToOutput(target);
            if (!valid()) return true;
            auto session = m_sessions.find(key);
            if (session == m_sessions.end()) return true;
            auto saved = std::find_if(session->snapshots.begin(), session->snapshots.end(),
                [&](const auto &s) { return s.window == arrival; });
            if (saved == session->snapshots.end()) return true;
            *saved = {.window = arrival, .geometry = client->moveResizeGeometry(),
                .floatingGeometry = client->geometryRestore(),
                .fullscreenRestoreGeometry = client->fullscreenGeometryRestore(),
                .outputName = key, .quickTileMode = client->quickTileMode(),
                .maximizeMode = client->maximizeMode(), .fullScreen = client->isFullScreen(),
                .minimized = client->isMinimized(), .valid = true};
        }
        auto session = m_sessions.find(key);
        if (session != m_sessions.end() && applySession(session.value(), true)) scheduleSettle();
    } else {
        if (!applyNativePlacement(client.data(), target.data(), geometry, valid)) return true;
        KWin::workspace()->raiseWindow(client);
        if (valid()) KWin::workspace()->activateWindow(client, true);
    }
    return true;
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

bool DesktopStageController::hasSessionOnOutput(
    const QString &outputName) const
{
    return m_sessions.contains(outputName);
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
    if (const auto saved = m_host->activeRestoreForDesktopStage(window)) {
        return {.window = window, .geometry = saved->geometry,
            .floatingGeometry = saved->floatingGeometry,
            .fullscreenRestoreGeometry = saved->fullscreenRestoreGeometry,
            .outputName = outputKey(saved->output), .quickTileMode = saved->quickTileMode,
            .maximizeMode = saved->maximizeMode, .fullScreen = saved->fullScreen,
            .minimized = saved->minimized, .valid = true};
    }
    return {
        .window = window,
        .geometry = window->frameGeometry(),
        .floatingGeometry = client->geometryRestore(),
        .fullscreenRestoreGeometry = client->fullscreenGeometryRestore(),
        .outputName = outputKey(window->screen()),
        .quickTileMode = client->quickTileMode(),
        .maximizeMode = client->maximizeMode(),
        .fullScreen = client->isFullScreen(),
        .minimized = window->isMinimized(),
        .valid = true,
    };
}

bool DesktopStageController::activate(KWin::LogicalOutput *output,
                           KWin::EffectWindow *preferred, std::optional<BentoSidePlacement> side)
{
    if (m_restoring) return false;
    if (!output || !m_host->allowsDesktopStageOnOutput(output)
        || sessionForOutput(output)) {
        return false;
    }
    std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
        return pending->output() == output || pending->result() != RestoredMinimization::Result::Pending;
    });
    if (m_host->isTabletOutputForDesktopStage(output)) {
        qInfo() << "Kadunce" << Revision
                << "using the tablet Desktop Stage fallback with no external display";
    }
    // Capture every retained card restore before releasing Card Stage. Native
    // configure acknowledgement may lag behind that release.
    QHash<KWin::EffectWindow *, NativeMoveSnapshot> cardRestores;
    for (const auto &window : collectWindows(output, preferred)) {
        if (const auto saved = m_host->activeRestoreForDesktopStage(window))
            cardRestores.insert(window, *saved);
    }
    m_host->prepareOutputForDesktopStage(output);
    const QList<QPointer<KWin::EffectWindow>> owned =
        collectWindows(output, preferred);
    if (owned.isEmpty()) {
        qInfo() << "Kadunce" << Revision
                << "found no monitor windows for Bento on" << output->name();
        return false;
    }

    Session session;
    session.outputName = outputKey(output);
    QList<RestoreSnapshot> snapshots;
    for (const QPointer<KWin::EffectWindow> &window : owned) {
        auto saved = makeSnapshot(window);
        const auto retained = cardRestores.constFind(window);
        const auto activeRestore = retained == cardRestores.cend()
            ? std::optional<NativeMoveSnapshot>() : std::optional<NativeMoveSnapshot>(*retained);
        if (activeRestore && window && window->window() == activeRestore->window
            && activeRestore->output == output) {
            // Restoration was requested, but the client may still report Active
            // bounds. Carry the source owner's record across this boundary.
            saved.geometry = activeRestore->geometry;
            saved.floatingGeometry = activeRestore->floatingGeometry;
            saved.fullscreenRestoreGeometry = activeRestore->fullscreenRestoreGeometry;
            saved.maximizeMode = activeRestore->maximizeMode;
            saved.quickTileMode = activeRestore->quickTileMode;
            saved.fullScreen = activeRestore->fullScreen;
            saved.minimized = activeRestore->minimized;
        }
        snapshots.append(saved);
    }
    auto prepared = prepareBentoActivation(session, snapshots,
        QPointer<KWin::EffectWindow>(preferred), false,
        [this, side, preferred](Session &candidate, const auto &lead, bool required) {
            if (side) { candidate.side = side; candidate.sideWindow = preferred; }
            return reflowSession(candidate, lead, required);
        });
    if (!prepared) return false;
    // Publish only a complete plan. Callers must prepare their arrival/source
    // transaction before invoking this mutating entry point.
    m_sessions.insert(session.outputName, std::move(*prepared));
    Session &stored = m_sessions[session.outputName];
    if (applySession(stored, true)) scheduleSettle();
    qInfo() << "Kadunce" << Revision << "activated output-local Bento";
    return true;
}

bool DesktopStageController::reflowSession(Session &session,
                                KWin::EffectWindow *preferred, bool requirePreferred,
                                bool invalidateApplication,
                                QList<QPointer<KWin::EffectWindow>> *evicted)
{
    if (invalidateApplication) m_applicationGuard.invalidate();
    return planSession(session, preferred, requirePreferred, evicted);
}

bool DesktopStageController::planSession(Session &session,
    KWin::EffectWindow *preferred, bool requirePreferred,
    QList<QPointer<KWin::EffectWindow>> *evicted) const
{
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (!output) {
        return false;
    }
    // Report every owned window the solve could not place. The caller decides
    // whether to act on it: a preview must not move an owner.
    const auto report = [evicted](const QList<QPointer<KWin::EffectWindow>> &windows) {
        if (!evicted) return;
        for (const auto &window : windows)
            if (window && !evicted->contains(window)) evicted->append(window);
    };
    QList<QPointer<KWin::EffectWindow>> owned;
    for (const RestoreSnapshot &snapshot : std::as_const(session.snapshots)) {
        if (snapshot.valid && !snapshot.userMinimized && snapshot.window && !snapshot.window->isDeleted()
            && snapshot.window->window() && !owned.contains(snapshot.window)) {
            owned.append(snapshot.window);
        }
    }
    if (preferred && owned.removeAll(preferred) > 0) {
        owned.prepend(preferred);
    }
    if (session.side && owned.contains(session.sideWindow)) {
        // Existing edge occupancy outranks a new whole-display side preset.
        // This operates on the same value plan for preview and commit.
        auto splitWindows = session.windows;
        const int existingIndex = splitWindows.indexOf(session.sideWindow);
        if (existingIndex < 0) splitWindows.append(session.sideWindow);
        std::vector<BentoCandidate> splitCandidates;
        bool validSplitInputs = true;
        for (const auto &w : splitWindows) {
            if (!owned.contains(w)) { validSplitInputs = false; break; }
            const auto minimum = w->window()->minSize();
            splitCandidates.push_back({minimum.width(),minimum.height(),1,1,false});
        }
        const auto splitArea = stageArea(output);
        const auto split = validSplitInputs ? splitBentoColumn(session.rects, splitCandidates,
            existingIndex < 0 ? session.windows.size() : existingIndex, *session.side,
            splitArea.width(), splitArea.height())
            : std::nullopt;
        if (split && (!requirePreferred || splitWindows.contains(preferred))) {
            session.windows = splitWindows;
            session.rects = *split;
            session.overflow = owned;
            for (const auto &w : splitWindows) session.overflow.removeAll(w);
            report(session.overflow);
            session.side.reset(); session.sideWindow.clear();
            return true;
        }
        owned.removeAll(session.sideWindow);
        owned.prepend(session.sideWindow);
        const auto candidate = [](KWin::EffectWindow *w) {
            const auto minimum = w->window()->minSize();
            return BentoCandidate{minimum.width(), minimum.height(), 1, 1, false};
        };
        const auto area = stageArea(output);
        std::vector<BentoCandidate> candidates;
        for (int i = 0; i < std::min(10, int(owned.size())); ++i)
            candidates.push_back(candidate(owned[i]));
        const auto admission = chooseBentoSideAdmission(*session.side, area.width(), area.height(),
            candidates, requirePreferred ? owned.indexOf(preferred) : 0);
        if (!admission) return false;
        session.windows.clear();
        session.overflow = owned;
        for (const int index : admission->candidateIndices) {
            session.windows.append(owned[index]);
            session.overflow.removeAll(owned[index]);
        }
        session.rects = admission->rects;
        report(session.overflow);
        return true;
    }
    // The curated library has eight panes. Two alternate candidates are
    // enough to resolve minimum-size conflicts without making the bounded
    // subset search grow with a desktop's entire window history. How many of
    // them this display shows is bentoPaneCap, which the edge path reads too.
    const QList<QPointer<KWin::EffectWindow>> considered = owned.mid(0, 10);
    const KWin::Rect area = stageArea(output);
    std::vector<BentoCandidate> candidates;
    candidates.reserve(considered.size());
    for (int index = 0; index < considered.size(); ++index) {
        KWin::EffectWindow *window = considered.at(index);
        const QSizeF minimum = window->window()->minSize();
        const KWin::RectF geometry = window->frameGeometry();
        candidates.push_back({minimum.width(), minimum.height(),
                              geometry.width(), geometry.height(),
                              index == 0});
    }
    const auto required = requirePreferred
        ? chooseBentoTransferAdmission(candidates, considered.indexOf(preferred),
            area.width(), area.height())
        : std::optional<BentoAdmission>{};
    if (requirePreferred && !required) return false;
    const BentoAdmission admission = required ? *required : chooseBentoAdmission(
        candidates, area.width(), area.height());
    session.windows.clear();
    session.overflow.clear();
    session.rects = admission.rects;
    QSet<KWin::EffectWindow *> admitted;
    for (const int index : admission.candidateIndices) {
        if (index >= 0 && index < considered.size()) {
            session.windows.append(considered.at(index));
            admitted.insert(considered.at(index));
        }
    }
    for (const QPointer<KWin::EffectWindow> &window : owned) {
        if (!admitted.contains(window)) {
            session.overflow.append(window);
        }
    }
    report(session.overflow);
    return true;
}

bool DesktopStageController::applySession(Session &session, bool activateLead,
    KWin::EffectWindow *prepareOverflow)
{
    if (session.participationDirty) {
        if (!reflowSession(session)) return false;
        session.participationDirty = false;
    }
    const Session plan = session;
    const QString key = plan.outputName;
    QPointer<KWin::LogicalOutput> output = outputForKey(key);
    if (!output) return false;
    std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
        return pending->output() == output || pending->result() != RestoredMinimization::Result::Pending;
    });
    const quint64 token = m_applicationGuard.issue();
    session.applying = true;
    session.applicationToken = token;
    const auto cleanup = qScopeGuard([&] {
        auto current = m_sessions.find(key);
        if (current != m_sessions.end() && current->applicationToken == token)
            current->applying = false;
    });
    const auto current = [&] {
        return m_applicationGuard.accepts(token) && output
            && outputForKey(key) == output && m_sessions.contains(key);
    };
    const KWin::Rect area = stageArea(output);
    const std::vector<BentoPixelRect> pixels = makePixelBentoLayout(
        plan.rects, area.x(), area.y(), area.width(), area.height());
    QList<QRectF> motionFrom, motionTo;
    for (int i = 0; i < plan.windows.size() && i < int(pixels.size()); ++i) {
        motionFrom.append(m_host->bentoPresentationRect(plan.windows[i]));
        const auto &p = pixels[i];
        motionTo.append(QRectF(p.x,p.y,p.width,p.height));
    }
    for (int index = 0;
         index < plan.windows.size()
            && index < static_cast<int>(pixels.size());
         ++index) {
        QPointer<KWin::EffectWindow> window = plan.windows.at(index);
        if (!current()) return false;
        if (!window || window->isDeleted() || !window->window()) {
            continue;
        }
        QPointer<KWin::Window> client = window->window();
        const auto valid = [&] {
            return current() && window && !window->isDeleted() && client
                && window->window() == client && !window->isUserMove() && !window->isUserResize();
        };
        if (!valid()) return false;
        const BentoPixelRect &pixel = pixels.at(index);
        const KWin::RectF target(pixel.x, pixel.y, pixel.width, pixel.height);
        if (!applyNativePlacement(client.data(), output.data(), target, valid)) return false;
    }
    for (const QPointer<KWin::EffectWindow> &window :
         plan.overflow) {
        if (!current()) return false;
        if (window && !window->isDeleted() && window->window()) {
            if (window == prepareOverflow) {
                const QPointer<KWin::Window> client = window->window();
                const auto valid = [&] {
                    return current() && client && window && !window->isDeleted()
                        && window->window() == client && !window->isUserMove() && !window->isUserResize();
                };
                if (!applyNativePlacement(client.data(), output.data(),
                        KWin::RectF(m_host->activeTargetForDesktopStage(output)), valid)) return false;
                // Observe KWin's accepted Active target, then minimize once;
                // the arrival's minimum already ruled out the curated panes.
                m_restoredMinimizations.push_back(std::make_unique<RestoredMinimization>(client,
                    RestoredMinimization::Target{client->moveResizeGeometry(), KWin::MaximizeRestore,
                        KWin::QuickTileMode{}, false}));
            } else window->window()->setMinimized(true);
            if (!current()) return false;
        }
    }
    const QPointer<KWin::EffectWindow> lead = plan.windows.value(0);
    if (activateLead && current() && lead && !lead->isDeleted() && lead->window()) {
        KWin::workspace()->raiseWindow(lead->window());
        if (!current() || !lead || lead->isDeleted() || !lead->window()) return false;
        KWin::workspace()->activateWindow(lead->window(), true);
    }
    if (!current()) return false;
    m_host->animateBentoLayout(output, plan.windows, motionFrom, motionTo);
    KWin::effects->addRepaintFull();
    return true;
}

void DesktopStageController::scheduleSettle()
{
    if (m_sessions.isEmpty() || m_interactionWindow) return;
    m_settleTimer.start();
}

void DesktopStageController::settleSessions()
{
    if (m_interactionWindow) {
        return;
    }
    const QStringList keys = m_sessions.keys();
    // Placement is requested once. Observe asynchronous results without issuing
    // more resizes; reconcile unresolved sessions once after the bounded grace.
    for (const QString &key : keys) {
        auto pending = m_sessions.find(key);
        if (pending != m_sessions.end() && pending->participationDirty) {
            if (applySession(pending.value(), false)) scheduleSettle();
            continue; // Give the new configure its own acknowledgement grace.
        }
        const auto session = m_sessions.constFind(key);
        if (session == m_sessions.cend() || sessionGeometryMatches(session.value())) continue;
        qWarning() << "Kadunce Bento geometry did not settle; restoring output" << key;
        restoreSession(key, !outputForKey(key));
    }
}

bool DesktopStageController::sessionGeometryMatches(const Session &session) const
{
    auto *output = outputForKey(session.outputName);
    if (!output || session.windows.size() != static_cast<int>(session.rects.size())) return false;
    const auto area = stageArea(output);
    const auto pixels = makePixelBentoLayout(session.rects, area.x(), area.y(), area.width(), area.height());
    for (int index = 0; index < session.windows.size(); ++index) {
        const auto &window = session.windows.at(index);
        if (!window || window->isDeleted() || !window->window()) return false;
        const auto &pixel = pixels.at(index);
        if (window->screen() != output || window->isMinimized()
            || window->frameGeometry().toRect() != KWin::Rect(pixel.x, pixel.y, pixel.width, pixel.height)) return false;
    }
    return true;
}

void DesktopStageController::restoreSession(const QString &key, bool outputRemoving)
{
    std::erase_if(m_restoredMinimizations, [&](const auto &pending) {
        return pending->result() != RestoredMinimization::Result::Pending
            || pending->output() == outputForKey(key);
    });
    QScopedValueRollback<bool> restoring(m_restoring, true);
    m_applicationGuard.invalidate();
    const auto it = m_sessions.find(key);
    if (it == m_sessions.end()) {
        return;
    }
    const Session session = it.value();
    m_sessions.erase(it);
    QPointer<KWin::LogicalOutput> fallbackOutput = outputForKey(key);
    QPointer<KWin::LogicalOutput> replacementOutput;
    if (outputRemoving) {
        KWin::LogicalOutput *tablet = m_host->tabletOutputForDesktopStage();
        if (tablet && outputKey(tablet) != key) {
            replacementOutput = tablet;
        } else {
            const QList<KWin::LogicalOutput *> outputs = KWin::effects->screens();
            const auto replacement = std::find_if(
                outputs.cbegin(), outputs.cend(),
                [this, &key](const KWin::LogicalOutput *output) {
                    return output && outputKey(output) != key;
                });
            if (replacement != outputs.cend()) {
                replacementOutput = *replacement;
            }
        }
    }
    for (const RestoreSnapshot &snapshot : session.snapshots) {
        if (!snapshot.valid || !snapshot.window
            || snapshot.window->isDeleted() || !snapshot.window->window()) {
            continue;
        }
        QPointer<KWin::Window> client = snapshot.window->window();
        QPointer<KWin::LogicalOutput> originalOutput = outputForKey(snapshot.outputName);
        const auto clientValid = [&] {
            return snapshot.window && !snapshot.window->isDeleted() && client
                && snapshot.window->window() == client
                && !snapshot.window->isUserMove() && !snapshot.window->isUserResize();
        };
        QList<QPointer<KWin::LogicalOutput>> candidates;
        const auto append = [&](KWin::LogicalOutput *output) {
            if (output && !candidates.contains(output)) candidates.append(output);
        };
        append(outputRemoving ? replacementOutput.data()
                             : (originalOutput ? originalOutput.data() : fallbackOutput.data()));
        append(m_host->tabletOutputForDesktopStage());
        for (auto *output : KWin::effects->screens()) append(output);
        const auto available = [&](const QPointer<KWin::LogicalOutput> &output) {
            return output && !m_retiredOutputs.contains(output)
                && (!outputRemoving || outputKey(output) != key)
                && KWin::effects->screens().contains(output.data());
        };
        const auto result = restoreOnSurvivingOutput(candidates, available,
            [&](const QPointer<KWin::LogicalOutput> &targetOutput) {
        if (!clientValid()) return RestoreResult::Aborted;
        const bool preserveGeometry = !outputRemoving && targetOutput == originalOutput;
        const auto valid = [&] { return clientValid() && available(targetOutput); };
        KWin::RectF restoreGeometry = snapshot.geometry;
        if (!preserveGeometry) {
            const KWin::RectF work = KWin::effects->clientArea(
                KWin::MaximizeArea, targetOutput);
            const double width = std::min(restoreGeometry.width(), work.width());
            const double height = std::min(restoreGeometry.height(), work.height());
            const double right = std::max(work.x(), work.right() - width);
            const double bottom = std::max(work.y(), work.bottom() - height);
            restoreGeometry = KWin::RectF(
                std::clamp(restoreGeometry.x(), work.x(), right),
                std::clamp(restoreGeometry.y(), work.y(), bottom),
                width, height);
        }
        client->sendToOutput(targetOutput);
        if (valid() && restoreWindowStateChecked(client.data(), snapshot, restoreGeometry,
                           preserveGeometry, true, false, valid)) {
            if (snapshot.minimized || snapshot.userMinimized) {
                if (client->frameGeometry() == client->moveResizeGeometry()
                    && client->maximizeMode() == snapshot.maximizeMode
                    && client->isFullScreen() == snapshot.fullScreen
                    && client->quickTileMode() == snapshot.quickTileMode) {
                    client->setMinimized(true);
                    return valid() ? RestoreResult::Completed
                        : (clientValid() ? RestoreResult::OutputLost : RestoreResult::Aborted);
                }
                // Observe the final native target, including maximize/tiling.
                // Re-minimizing in the same configure can leave stale client bounds.
                m_restoredMinimizations.push_back(std::make_unique<RestoredMinimization>(
                    client, RestoredMinimization::Target{client->moveResizeGeometry(),
                        snapshot.maximizeMode, snapshot.quickTileMode, snapshot.fullScreen}));
            }
            return RestoreResult::Completed;
        }
        return clientValid() ? RestoreResult::OutputLost : RestoreResult::Aborted;
        });
        if (result == RestoreResult::OutputLost)
            qWarning() << "Kadunce restore has no surviving candidate output; leaving placement to KWin";
    }
    m_host->retireOutputFromDesktopStage(fallbackOutput.data());
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "restored output-local Bento on"
            << key << "during removal" << outputRemoving;
}

bool DesktopStageController::transferTabletSessionToSpread(KWin::LogicalOutput *output,
    const std::function<bool(const BentoProjectionSession &,
                             const std::function<bool()> &)> &accept)
{
    if (m_restoring || !output || !m_host->isTabletOutputForDesktopStage(output)
        || m_interactionWindow || m_railDrag) return false;
    const auto *session = sessionForOutput(output);
    if (!session || session->applying || session->windows.isEmpty()) return false;
    // Preserve pane order alongside the authoritative rect vector. The largest
    // real pane is selected as the group lead without reordering that vector.
    auto ordered = session->windows;
    int lead = 0;
    for (int i = 1; i < int(session->rects.size()); ++i)
        if (session->rects[i].width * session->rects[i].height
            > session->rects[lead].width * session->rects[lead].height) lead = i;
    for (const auto &saved : session->snapshots)
        if (!ordered.contains(saved.window)) ordered.append(saved.window);
    BentoProjectionSession projection;
    projection.output = output;
    projection.outputName = outputKey(output);
    projection.workspaceArea = workspaceArea(output);
    projection.rects = session->rects;
    projection.side = session->side;
    projection.sideWindow = session->sideWindow;
    for (const auto &window : ordered) {
        const auto saved = std::find_if(session->snapshots.cbegin(), session->snapshots.cend(),
            [&](const auto &s) { return s.window == window; });
        if (!window || window->isDeleted() || !window->window() || window->screen() != output
            || saved == session->snapshots.cend() || !saved->valid) return false;
        BentoProjectionMember member{window,
            {window->window(), output, saved->geometry, saved->floatingGeometry,
            saved->fullscreenRestoreGeometry, saved->maximizeMode, saved->quickTileMode,
            saved->fullScreen, saved->minimized || saved->userMinimized},
            window->isMinimized()};
        if (session->windows.contains(window)) projection.panes.append(member);
        else projection.overflow.append(member);
    }
    projection.lead = session->windows.value(lead);
    for (auto *stacked : KWin::effects->stackingOrder()) {
        if (ordered.contains(stacked)) projection.stackingOrder.append(stacked);
    }
    for (const auto &member : ordered) {
        if (!projection.stackingOrder.contains(member))
            projection.stackingOrder.append(member);
    }
    if (!validBentoProjectionShape(projectionShape(projection))) return false;
    const auto generation = m_applicationGuard.generation();
    const QString key = outputKey(output);
    bool committed = false;
    const bool accepted = accept(projection, [&] {
        if (committed || generation != m_applicationGuard.generation()
            || !m_sessions.contains(key)) return false;
        m_sessions.remove(key);
        m_applicationGuard.invalidate();
        std::erase_if(m_restoredMinimizations, [output](const auto &pending) {
            return pending->output() == output;
        });
        committed = true;
        return true;
    });
    return accepted && committed;
}

bool DesktopStageController::resumeProjectedSession(
    const BentoProjectionSession &projection,
    const std::function<bool()> &commitSource,
    const std::function<void()> &releaseSource)
{
    if (m_restoring || m_interactionWindow || m_railDrag || !projection.output
        || outputForKey(projection.outputName) != projection.output
        || sessionForOutput(projection.output)
        || !m_host->isTabletOutputForDesktopStage(projection.output)
        || !validBentoProjectionShape(projectionShape(projection))) {
        return false;
    }
    Session candidate;
    candidate.outputName = projection.outputName;
    candidate.rects = projection.rects;
    candidate.side = projection.side;
    candidate.sideWindow = projection.sideWindow;
    const auto append = [&](const BentoProjectionMember &member, bool pane) {
        if (!member.window || member.window->isDeleted() || !member.window->window()
            || member.restore.window != member.window->window()
            || member.window->screen() != projection.output
            || member.window->isMinimized() != member.minimized) {
            return false;
        }
        if (pane) candidate.windows.append(member.window);
        else candidate.overflow.append(member.window);
        candidate.snapshots.append({
            .window = member.window,
            .geometry = member.restore.geometry,
            .floatingGeometry = member.restore.floatingGeometry,
            .fullscreenRestoreGeometry = member.restore.fullscreenRestoreGeometry,
            .outputName = outputKey(member.restore.output),
            .quickTileMode = member.restore.quickTileMode,
            .maximizeMode = member.restore.maximizeMode,
            .fullScreen = member.restore.fullScreen,
            .minimized = member.restore.minimized,
            .valid = true,
        });
        return true;
    };
    for (const auto &member : projection.panes) {
        if (member.minimized || !append(member, true)) return false;
    }
    for (const auto &member : projection.overflow) {
        if (!member.minimized || !append(member, false)) return false;
    }
    if (workspaceArea(projection.output) != projection.workspaceArea) return false;
    const KWin::Rect area = stageArea(projection.output);
    const auto pixels = makePixelBentoLayout(candidate.rects,
        area.x(), area.y(), area.width(), area.height());
    if (pixels.size() != std::size_t(candidate.windows.size())) return false;
    for (int index = 0; index < candidate.windows.size(); ++index) {
        const auto &pixel = pixels[std::size_t(index)];
        if (candidate.windows[index]->frameGeometry().toRect()
            != KWin::Rect(pixel.x, pixel.y, pixel.width, pixel.height)) {
            return false;
        }
    }
    if (!commitResumeHandback(
            [&] { return commitSource && commitSource(); },
            [&] {
                m_applicationGuard.invalidate();
                m_sessions.insert(candidate.outputName, std::move(candidate));
            },
            [&] { if (releaseSource) releaseSource(); })) {
        return false;
    }
    if (projection.lead && !projection.lead->isDeleted()
        && projection.lead->window()) {
        KWin::workspace()->raiseWindow(projection.lead->window());
        KWin::workspace()->activateWindow(projection.lead->window(), true);
    }
    KWin::effects->addRepaintFull();
    return true;
}

void DesktopStageController::restoreAllSessions()
{
    QScopedValueRollback<bool> restoring(m_restoring, true);
    stopPendingSettle();
    const QStringList keys = m_sessions.keys();
    for (const QString &key : keys) {
        restoreSession(key);
    }
}

void DesktopStageController::cancelRestoredMinimizations()
{
    m_restoredMinimizations.clear();
}

void DesktopStageController::addWindow(KWin::EffectWindow *window,
                              KWin::LogicalOutput *output,
                              const RestoreSnapshot &snapshot)
{
    Session *session = sessionForOutput(output);
    if (!session || !window || !window->window()) {
        return;
    }
    for (const RestoreSnapshot &existing :
         std::as_const(session->snapshots)) {
        if (existing.window == window) {
            return;
        }
    }
    session->snapshots.append(snapshot.valid
        ? snapshot : makeSnapshot(window));
    reflowSession(*session, window);
    if (applySession(*session, true)) scheduleSettle();
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

    KWin::RectF destinationGeometry = requestedGeometry;
    if (!destination->geometry().intersects(destinationGeometry.toRect())) {
        destinationGeometry = KWin::RectF(m_host->activeTargetForDesktopStage(destination));
    }
    if (!detach && (sessionForOutput(destination)
        || intent == CardDropIntent::ActivateBento)) {
        const QString destinationKey = outputKey(destination);
        const RestoreSnapshot snapshot{
            .window = window,
            .geometry = destinationGeometry,
            .floatingGeometry = destinationGeometry,
            .fullscreenRestoreGeometry = {},
            .outputName = destinationKey,
            .quickTileMode = {},
            .maximizeMode = KWin::MaximizeRestore,
            .fullScreen = false,
            .minimized = false,
            .valid = true,
        };
        const auto planner = [this, side, window, destinationKey](Session &session, const QPointer<KWin::EffectWindow> &preferred, bool required) {
                if (side && session.outputName == destinationKey) {
                    session.side = side; session.sideWindow = window;
                }
                return reflowSession(session, preferred, required, false);
            };
        std::optional<BentoSessionTransfer<Session>> transfer;
        if (m_sessions.contains(destinationKey)) {
            transfer = prepareBentoSessionTransfer(m_sessions.value(sourceKey),
                m_sessions.value(destinationKey), QPointer<KWin::EffectWindow>(window), snapshot, planner);
        } else {
            Session empty;
            empty.outputName = destinationKey;
            QList<RestoreSnapshot> residents;
            for (const auto &resident : collectWindows(destination, window))
                residents.append(makeSnapshot(resident));
            const auto admission = prepareBentoActivationWithArrival(empty, residents,
                QPointer<KWin::EffectWindow>(window), snapshot, planner);
            const auto departure = prepareBentoDeparture(m_sessions.value(sourceKey),
                QPointer<KWin::EffectWindow>(window), planner);
            if (admission && departure) transfer = BentoSessionTransfer<Session>{*departure, *admission};
        }
        if (!transfer) return false;
        // No native calls or callbacks between these publications. Destination
        // feasibility was established while both original sessions were intact.
        m_sessions[destinationKey] = transfer->destination;
        if (transfer->source.snapshots.isEmpty()) m_sessions.remove(sourceKey);
        else m_sessions[sourceKey] = transfer->source;
        // Re-fetch after every native application; it can emit lifecycle signals.
        auto source = m_sessions.find(sourceKey);
        if (source != m_sessions.end() && !applySession(source.value(), false)) return true;
        auto target = m_sessions.find(destinationKey);
        if (target == m_sessions.end() || !applySession(target.value(), true)) return true;
        scheduleSettle();
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
        if (departure->snapshots.isEmpty()) m_sessions.remove(sourceKey);
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
    const bool accepted = m_host->admitTransferredWindowToTablet(window, [&] {
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
    return true;
}

void DesktopStageController::removeWindow(KWin::EffectWindow *window,
                                   bool restoreSnapshot)
{
    m_applicationGuard.invalidate();
    if (!window) {
        return;
    }
    QString sourceKey;
    RestoreSnapshot removedSnapshot;
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
        session.snapshots.erase(snapshot);
        session.windows.removeAll(window);
        session.overflow.removeAll(window);
        break;
    }
    if (sourceKey.isEmpty()) {
        return;
    }
    if (restoreSnapshot && removedSnapshot.valid
        && removedSnapshot.window && removedSnapshot.window->window()) {
        KWin::Window *client = removedSnapshot.window->window();
        KWin::LogicalOutput *output = outputForKey(removedSnapshot.outputName);
        if (output) {
            client->sendToOutput(output);
        }
        client->setMinimized(false);
        client->moveResize(removedSnapshot.geometry);
    }
    auto source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    if (source->snapshots.isEmpty()) {
        m_sessions.erase(source);
        return;
    }
    reflowSession(source.value());
    if (applySession(source.value(), false)) scheduleSettle();
}

void DesktopStageController::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
    m_applicationGuard.invalidate();
    if (!window || m_interactionWindow) {
        return;
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        Session &session = it.value();
        if (!session.applying && session.windows.contains(window)) {
            m_interactionWindow = window;
            m_interactionStart = window->frameGeometry();
            m_interactionOutput = it.key();
            m_pendingDropOutput.clear();
            m_interactionResize = window->isUserResize();
            m_settleTimer.stop();
            qInfo() << "Kadunce" << Revision
                    << "began Bento interaction" << window->caption()
                    << (m_interactionResize ? "rail" : "carry");
            return;
        }
    }
}

void DesktopStageController::handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                           const KWin::RectF &)
{
    if (!window || !window->window()
        || m_interactionWindow != window
        || m_interactionResize) {
        return;
    }
    KWin::LogicalOutput *destination = KWin::effects->screenAt(
        KWin::effects->cursorPos().toPoint());
    if (!destination) {
        return;
    }
    const QString destinationKey = outputKey(destination);
    if (destinationKey == m_interactionOutput) {
        m_pendingDropOutput.clear();
        return;
    }
    if (m_pendingDropOutput != destinationKey) {
        m_pendingDropOutput = destinationKey;
        qInfo() << "Kadunce" << Revision
                << "armed Bento drop across output seam"
                << m_interactionOutput << "to" << destinationKey;
    }
}

void DesktopStageController::handleWindowMoveResizeFinished(KWin::EffectWindow *window)
{
    if (!window || m_interactionWindow != window) {
        return;
    }
    const QString sourceKey = m_interactionOutput;
    const KWin::RectF start = m_interactionStart;
    const bool resized = m_interactionResize;
    m_interactionWindow = nullptr;
    m_interactionStart = {};
    m_interactionOutput.clear();
    m_pendingDropOutput.clear();
    m_interactionResize = false;

    auto source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    // KWin has finalized (or canceled) the move before this signal. A cursor
    // left across the seam must not turn Escape into an accepted transfer.
    KWin::LogicalOutput *destination = window->screen();
    const bool changedOutput = destination
        && outputKey(destination) != sourceKey;
    if (changedOutput && !resized) {
        if (!handoffWindowToOutput(window, destination, window->frameGeometry())) {
            // A native titlebar drag already moved the client. Rejected managed
            // transfer retains the source session; replay its unchanged layout.
            source = m_sessions.find(sourceKey);
            if (source != m_sessions.end() && applySession(source.value(), false)) scheduleSettle();
        }
        return;
    }

    source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    if (resized) {
        adjustRail(source.value(), window, start,
                        window->frameGeometry());
    }
    if (applySession(source.value(), false)) scheduleSettle();
}

QList<DesktopStageController::GrabRail> DesktopStageController::grabRails() const
{
    QList<GrabRail> rails;
    if (m_restoring || m_interactionWindow) return rails;
    for (const auto &session : m_sessions) {
        auto *output = outputForKey(session.outputName);
        if (!output || session.applying || !sessionGeometryMatches(session)) continue;
        const QRectF area(stageArea(output));
        for (int i = 0; i < int(session.rects.size()); ++i) {
            const auto &a = session.rects[i];
            for (int j = 0; j < int(session.rects.size()); ++j) {
                if (i == j) continue;
                const auto &b = session.rects[j];
                for (bool vertical : {true, false}) {
                    const double boundary = vertical ? a.x+a.width : a.y+a.height;
                    const double opposite = vertical ? b.x : b.y;
                    const double first = std::max(vertical ? a.y : a.x, vertical ? b.y : b.x);
                    const double last = std::min(vertical ? a.y+a.height : a.x+a.width,
                        vertical ? b.y+b.height : b.x+b.width);
                    if (std::abs(boundary-opposite) > .002 || last <= first) continue;
                    const QPointF center(area.x() + (vertical ? boundary : (first+last)/2)*area.width(),
                        area.y() + (vertical ? (first+last)/2 : boundary)*area.height());
                    bool covered = false;
                    const auto stacking = KWin::effects->stackingOrder();
                    for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
                        auto *w = *it;
                        if (w == session.windows[i] || w == session.windows[j]) break;
                        if (w->isVisible() && !w->isDesktop() && !session.windows.contains(w)
                            && QRectF(w->frameGeometry()).intersects(QRectF(center.x()-16,center.y()-16,32,32))) {
                            covered = true; break;
                        }
                    }
                    if (covered) continue; // Do not paint/grab through Plasma popups or free windows.
                    rails.append({session.outputName, i, vertical,
                        QRectF(center.x()-(vertical ? 2 : 21), center.y()-(vertical ? 21 : 2),
                            vertical ? 4 : 42, vertical ? 42 : 4),
                        (vertical
                            ? QRectF(center.x()-16,area.y()+first*area.height(),32,(last-first)*area.height())
                            : QRectF(area.x()+first*area.width(),center.y()-16,(last-first)*area.width(),32))
                            .intersected(area)});
                }
            }
        }
    }
    if (railValid()) for (auto &rail : rails) {
        const auto &drag = *m_railDrag;
        if (rail.output != drag.rail.output || rail.index != drag.rail.index
            || rail.vertical != drag.rail.vertical) continue;
        const auto &rect = drag.preview.rects[rail.index];
        QPointF center = rail.pill.center();
        if (rail.vertical) center.setX(drag.area.x()+(rect.x+rect.width)*drag.area.width());
        else center.setY(drag.area.y()+(rect.y+rect.height)*drag.area.height());
        rail.pill.setSize(rail.vertical ? QSizeF(4,48) : QSizeF(48,4));
        rail.pill.moveCenter(center);
    }
    return rails;
}

bool DesktopStageController::railValid() const
{
    if (!m_railDrag || m_restoring || m_interactionWindow
        || m_applicationGuard.generation() != m_railDrag->generation) return false;
    const auto it = m_sessions.constFind(m_railDrag->rail.output);
    auto *output = outputForKey(m_railDrag->rail.output);
    return it != m_sessions.cend() && output && stageArea(output) == m_railDrag->area
        && it->windows == m_railDrag->original.windows && sessionGeometryMatches(*it);
}

bool DesktopStageController::beginRail(QPointF position)
{
    if (m_railDrag) return false;
    for (const auto &rail : grabRails()) {
        // Claim the entire shared divider at down, before Plasma sees a hold.
        // The centered pill is feedback, not the extent of the input target.
        if (!rail.hitArea.contains(position)) continue;
        const auto session = m_sessions.value(rail.output);
        bool covered = false;
        const auto stacking = KWin::effects->stackingOrder();
        for (auto it = stacking.crbegin(); it != stacking.crend(); ++it) {
            auto *w = *it;
            if (w == session.windows[rail.index]) break;
            if (w->isVisible() && !w->isDesktop() && !session.windows.contains(w)
                && QRectF(w->frameGeometry()).contains(position)) { covered = true; break; }
        }
        if (covered) continue;
        m_railDrag = RailDrag{rail, session, session, m_applicationGuard.generation(),
            stageArea(outputForKey(rail.output)), position};
        m_railRevealed = false;
        return true;
    }
    return false;
}

void DesktopStageController::updateRail(QPointF position)
{
    if (!railValid()) { finishRail(false); return; }
    m_railRevealed = true;
    auto &drag = *m_railDrag;
    drag.preview = drag.original;
    const auto window = drag.original.windows[drag.rail.index];
    const KWin::RectF start = window->frameGeometry();
    QRectF finish(start);
    if (drag.rail.vertical) finish.setRight(finish.right()+position.x()-drag.contact.x());
    else finish.setBottom(finish.bottom()+position.y()-drag.contact.y());
    adjustRail(drag.preview, window, start, KWin::RectF(finish));
    KWin::effects->addRepaintFull();
}

QList<QRectF> DesktopStageController::railPreview(const QString &output) const
{
    QList<QRectF> boxes;
    if (!m_railRevealed || !railValid() || m_railDrag->rail.output != output) return boxes;
    const auto &a = m_railDrag->area;
    for (const auto &r : makePixelBentoLayout(m_railDrag->preview.rects, a.x(), a.y(), a.width(), a.height()))
        boxes.append(QRectF(r.x,r.y,r.width,r.height));
    return boxes;
}

void DesktopStageController::finishRail(bool commit)
{
    if (!m_railDrag) return;
    const bool valid = commit && railValid();
    auto drag = std::move(*m_railDrag);
    m_railDrag.reset();
    m_railRevealed = false;
    if (valid) {
        m_applicationGuard.invalidate();
        m_sessions[drag.rail.output].rects = std::move(drag.preview.rects);
        if (applySession(m_sessions[drag.rail.output], false)) scheduleSettle();
    }
    KWin::effects->addRepaintFull();
}

void DesktopStageController::adjustRail(Session &session,
                             KWin::EffectWindow *window,
                             const KWin::RectF &start,
                             const KWin::RectF &finish)
{
    const int resizedIndex = session.windows.indexOf(window);
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (resizedIndex < 0 || resizedIndex >= static_cast<int>(session.rects.size())
        || !output) {
        return;
    }
    const std::array<double, 4> deltas{
        std::abs(finish.x() - start.x()),
        std::abs(finish.right() - start.right()),
        std::abs(finish.y() - start.y()),
        std::abs(finish.bottom() - start.bottom()),
    };
    const int edge = static_cast<int>(std::distance(
        deltas.cbegin(), std::max_element(deltas.cbegin(), deltas.cend())));
    if (deltas.at(edge) < 1.0) {
        return;
    }
    const bool vertical = edge < 2;
    const BentoRect base = session.rects.at(resizedIndex);
    const double oldBoundary = edge == 0 ? base.x
        : edge == 1 ? base.x + base.width
        : edge == 2 ? base.y : base.y + base.height;
    if (oldBoundary < 0.001 || oldBoundary > 0.999) {
        return;
    }
    const double segmentStart = vertical ? base.y : base.x;
    const double segmentEnd = vertical
        ? base.y + base.height : base.x + base.width;
    const KWin::Rect area = stageArea(output);
    double boundary = vertical
        ? ((edge == 0 ? finish.x() : finish.right()) - area.x())
            / area.width()
        : ((edge == 2 ? finish.y() : finish.bottom()) - area.y())
            / area.height();

    constexpr double Epsilon = 0.002;
    std::vector<BentoRect> next = session.rects;
    double connectedStart = segmentStart;
    double connectedEnd = segmentEnd;
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const BentoRect &rect : next) {
            const double crossStart = vertical ? rect.y : rect.x;
            const double crossEnd = vertical
                ? rect.y + rect.height : rect.x + rect.width;
            const double startEdge = vertical ? rect.x : rect.y;
            const double endEdge = vertical
                ? rect.x + rect.width : rect.y + rect.height;
            const bool attached = std::abs(endEdge - oldBoundary) < Epsilon
                || std::abs(startEdge - oldBoundary) < Epsilon;
            const bool connected = crossEnd >= connectedStart - Epsilon
                && crossStart <= connectedEnd + Epsilon;
            if (!attached || !connected) {
                continue;
            }
            const double newStart = std::min(connectedStart, crossStart);
            const double newEnd = std::max(connectedEnd, crossEnd);
            if (newStart < connectedStart - Epsilon
                || newEnd > connectedEnd + Epsilon) {
                connectedStart = newStart;
                connectedEnd = newEnd;
                expanded = true;
            }
        }
    }
    std::vector<int> before;
    std::vector<int> after;
    for (int index = 0; index < static_cast<int>(next.size()); ++index) {
        const BentoRect &rect = next.at(index);
        const double crossStart = vertical ? rect.y : rect.x;
        const double crossEnd = vertical
            ? rect.y + rect.height : rect.x + rect.width;
        if (crossEnd < connectedStart - Epsilon
            || crossStart > connectedEnd + Epsilon) {
            continue;
        }
        const double startEdge = vertical ? rect.x : rect.y;
        const double endEdge = vertical
            ? rect.x + rect.width : rect.y + rect.height;
        if (std::abs(endEdge - oldBoundary) < Epsilon) {
            before.push_back(index);
        }
        if (std::abs(startEdge - oldBoundary) < Epsilon) {
            after.push_back(index);
        }
    }
    if (before.empty() || after.empty()) {
        return;
    }
    const double fallbackMinimum = vertical
        ? std::max(0.09, 180.0 / area.width())
        : std::max(0.11, 130.0 / area.height());
    double lower = 0.0;
    double upper = 1.0;
    for (const int index : before) {
        const BentoRect &rect = next.at(index);
        const QSizeF minimum = session.windows.at(index)->window()->minSize();
        const double advertised = vertical
            ? (minimum.width() + 14.0) / area.width()
            : (minimum.height() + 14.0) / area.height();
        lower = std::max(lower, (vertical ? rect.x : rect.y)
                         + std::max(fallbackMinimum, advertised));
    }
    for (const int index : after) {
        const BentoRect &rect = next.at(index);
        const QSizeF minimum = session.windows.at(index)->window()->minSize();
        const double advertised = vertical
            ? (minimum.width() + 14.0) / area.width()
            : (minimum.height() + 14.0) / area.height();
        upper = std::min(upper,
                         (vertical ? rect.x + rect.width
                                   : rect.y + rect.height)
                         - std::max(fallbackMinimum, advertised));
    }
    if (lower > upper) {
        return;
    }
    boundary = std::clamp(boundary, lower, upper);
    for (const int index : before) {
        BentoRect &rect = next.at(index);
        if (vertical) {
            rect.width = boundary - rect.x;
        } else {
            rect.height = boundary - rect.y;
        }
    }
    for (const int index : after) {
        BentoRect &rect = next.at(index);
        if (vertical) {
            const double end = rect.x + rect.width;
            rect.x = boundary;
            rect.width = end - boundary;
        } else {
            const double end = rect.y + rect.height;
            rect.y = boundary;
            rect.height = end - boundary;
        }
    }
    session.rects = std::move(next);
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

} // namespace Kadunce

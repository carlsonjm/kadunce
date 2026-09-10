/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DesktopStageController.h"
#include "WindowStateRestore.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>

#include <QDebug>
#include <QSet>

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
    m_settleTimer.setInterval(90);
    QObject::connect(&m_settleTimer, &QTimer::timeout, [this]() {
        settleSessions();
    });
}

void DesktopStageController::stopPendingSettle()
{
    m_settleTimer.stop();
    m_settleAttempts = 0;
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
    if (!window || !m_host->isManagedWindowForDesktopStage(window)) {
        return false;
    }
    Session *session = sessionForOutput(window->screen());
    if (!session) {
        return false;
    }
    session->snapshots.append(makeSnapshot(window));
    session->overflow.append(window);
    if (window->window()) {
        window->window()->setMinimized(true);
    }
    qInfo() << "Kadunce" << Revision
            << "parked a newly opened monitor app until Bento exits"
            << window->caption();
    return true;
}

void DesktopStageController::handleWindowClosed(KWin::EffectWindow *window)
{
    removeWindow(window, false);
}

bool DesktopStageController::admitCardWindow(
    KWin::EffectWindow *window, KWin::LogicalOutput *output,
    const KWin::RectF &geometry)
{
    if (!sessionForOutput(output)) {
        return false;
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
    addWindow(window, output, snapshot);
    return true;
}

bool DesktopStageController::hasActiveSession() const
{
    return !m_sessions.isEmpty();
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
    if (!output) {
        return {};
    }
    const KWin::RectF work = KWin::effects->clientArea(
        KWin::MaximizeArea, output);
    return KWin::Rect(qRound(work.x()) + 10, qRound(work.y()) + 10,
                      std::max(1, qRound(work.width()) - 20),
                      std::max(1, qRound(work.height()) - 30));
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
                           KWin::EffectWindow *preferred)
{
    if (!output || !m_host->allowsDesktopStageOnOutput(output)
        || sessionForOutput(output)) {
        return false;
    }
    if (m_host->isTabletOutputForDesktopStage(output)) {
        qInfo() << "Kadunce" << Revision
                << "using the tablet Desktop Stage fallback with no external display";
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
    for (const QPointer<KWin::EffectWindow> &window : owned) {
        session.snapshots.append(makeSnapshot(window));
    }
    m_sessions.insert(session.outputName, session);
    Session &stored = m_sessions[session.outputName];
    reflowSession(stored, preferred);
    applySession(stored, true);
    scheduleSettle();
    qInfo() << "Kadunce" << Revision << "activated output-local Bento on"
            << stored.outputName << "visible" << stored.windows.size()
            << "parked" << stored.overflow.size();
    return true;
}

void DesktopStageController::toggleUnderPointer()
{
    KWin::LogicalOutput *output = KWin::effects->screenAt(
        KWin::effects->cursorPos().toPoint());
    if (!output) {
        return;
    }
    const QString key = outputKey(output);
    if (m_sessions.contains(key)) {
        restoreSession(key);
        return;
    }
    if (!m_host->allowsDesktopStageOnOutput(output)) {
        qInfo() << "Kadunce" << Revision
                << "kept the tablet attention stage while an external composition stage exists";
        return;
    }
    activate(output, KWin::effects->activeWindow());
}

void DesktopStageController::reflowSession(Session &session,
                                KWin::EffectWindow *preferred)
{
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (!output) {
        return;
    }
    QList<QPointer<KWin::EffectWindow>> owned;
    for (const RestoreSnapshot &snapshot : std::as_const(session.snapshots)) {
        if (snapshot.valid && snapshot.window && !snapshot.window->isDeleted()
            && snapshot.window->window() && !owned.contains(snapshot.window)) {
            owned.append(snapshot.window);
        }
    }
    if (preferred && owned.removeAll(preferred) > 0) {
        owned.prepend(preferred);
    }
    // The curated library has eight panes. Two alternate candidates are
    // enough to resolve minimum-size conflicts without making the bounded
    // subset search grow with a desktop's entire window history.
    const QList<QPointer<KWin::EffectWindow>> considered = owned.mid(0, 10);
    const KWin::Rect area = stageArea(output);
    const bool compact = area.width() < 1800 || area.height() < 1000;
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
    const BentoAdmission admission = chooseBentoAdmission(
        candidates, area.width(), area.height(), compact ? 2 : 8);
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
}

void DesktopStageController::applySession(Session &session, bool activateLead)
{
    KWin::LogicalOutput *output = outputForKey(session.outputName);
    if (!output) {
        return;
    }
    const KWin::Rect area = stageArea(output);
    const std::vector<BentoPixelRect> pixels = makePixelBentoLayout(
        session.rects, area.x(), area.y(), area.width(), area.height());
    session.applying = true;
    for (int index = 0;
         index < session.windows.size()
            && index < static_cast<int>(pixels.size());
         ++index) {
        KWin::EffectWindow *window = session.windows.at(index);
        if (!window || window->isDeleted() || !window->window()) {
            continue;
        }
        KWin::Window *client = window->window();
        client->sendToOutput(output);
        client->setMinimized(false);
        if (client->isFullScreen()) {
            client->setFullScreen(false);
        }
        if (client->maximizeMode() != KWin::MaximizeRestore) {
            client->maximize(KWin::MaximizeRestore);
        }
        if (client->quickTileMode() != KWin::QuickTileMode{}) {
            client->setQuickTileMode(KWin::QuickTileMode{},
                                     window->frameGeometry().center());
        }
        const BentoPixelRect &pixel = pixels.at(index);
        client->moveResize(KWin::RectF(
            pixel.x, pixel.y, pixel.width, pixel.height));
    }
    for (const QPointer<KWin::EffectWindow> &window :
         std::as_const(session.overflow)) {
        if (window && !window->isDeleted() && window->window()) {
            window->window()->setMinimized(true);
        }
    }
    session.applying = false;
    if (activateLead && !session.windows.isEmpty()
        && session.windows.first() && session.windows.first()->window()) {
        KWin::workspace()->raiseWindow(session.windows.first()->window());
        KWin::workspace()->activateWindow(session.windows.first()->window(), true);
    }
    KWin::effects->addRepaintFull();
}

void DesktopStageController::scheduleSettle()
{
    m_settleAttempts = 4;
    m_settleTimer.start();
}

void DesktopStageController::settleSessions()
{
    if (m_interactionWindow) {
        m_settleAttempts = 0;
        return;
    }
    bool corrected = false;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        Session &session = it.value();
        KWin::LogicalOutput *output = outputForKey(session.outputName);
        if (!output) {
            continue;
        }
        const KWin::Rect area = stageArea(output);
        const std::vector<BentoPixelRect> pixels = makePixelBentoLayout(
            session.rects, area.x(), area.y(), area.width(), area.height());
        for (int index = 0;
             index < session.windows.size()
                && index < static_cast<int>(pixels.size());
             ++index) {
            KWin::EffectWindow *window = session.windows.at(index);
            if (!window || window->isDeleted() || !window->window()
                || window->isUserMove() || window->isUserResize()) {
                continue;
            }
            const BentoPixelRect &pixel = pixels.at(index);
            const KWin::Rect target(pixel.x, pixel.y, pixel.width, pixel.height);
            if (window->frameGeometry().toRect() != target) {
                session.applying = true;
                window->window()->moveResize(KWin::RectF(target));
                session.applying = false;
                corrected = true;
            }
        }
    }
    --m_settleAttempts;
    if (m_settleAttempts > 0 && (corrected || m_settleAttempts > 1)) {
        m_settleTimer.start();
    }
}

void DesktopStageController::restoreSession(const QString &key, bool outputRemoving)
{
    const auto it = m_sessions.find(key);
    if (it == m_sessions.end()) {
        return;
    }
    const Session session = it.value();
    m_sessions.erase(it);
    KWin::LogicalOutput *fallbackOutput = outputForKey(key);
    KWin::LogicalOutput *replacementOutput = nullptr;
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
        KWin::Window *client = snapshot.window->window();
        KWin::LogicalOutput *originalOutput = outputForKey(snapshot.outputName);
        KWin::RectF restoreGeometry = snapshot.geometry;
        if (outputRemoving && replacementOutput) {
            client->sendToOutput(replacementOutput);
            const KWin::RectF work = KWin::effects->clientArea(
                KWin::MaximizeArea, replacementOutput);
            const double width = std::min(restoreGeometry.width(), work.width());
            const double height = std::min(restoreGeometry.height(), work.height());
            const double right = std::max(work.x(), work.right() - width);
            const double bottom = std::max(work.y(), work.bottom() - height);
            restoreGeometry = KWin::RectF(
                std::clamp(restoreGeometry.x(), work.x(), right),
                std::clamp(restoreGeometry.y(), work.y(), bottom),
                width, height);
        } else if (!outputRemoving && originalOutput) {
            client->sendToOutput(originalOutput);
        } else if (!outputRemoving && fallbackOutput) {
            client->sendToOutput(fallbackOutput);
        }
        restoreWindowState(client, snapshot, restoreGeometry,
                           !outputRemoving, true, snapshot.minimized);
    }
    KWin::effects->addRepaintFull();
    qInfo() << "Kadunce" << Revision << "restored output-local Bento on"
            << key << "during removal" << outputRemoving;
}

void DesktopStageController::restoreAllSessions()
{
    const QStringList keys = m_sessions.keys();
    for (const QString &key : keys) {
        restoreSession(key);
    }
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
    applySession(*session, true);
    scheduleSettle();
}

bool DesktopStageController::handoffWindowToOutput(
    KWin::EffectWindow *window, KWin::LogicalOutput *destination,
    const KWin::RectF &requestedGeometry)
{
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
    if (sourceKey.isEmpty() || outputKey(destination) == sourceKey) {
        return false;
    }

    KWin::RectF destinationGeometry = requestedGeometry;
    if (!destination->geometry().intersects(destinationGeometry.toRect())) {
        destinationGeometry = KWin::RectF(m_host->activeTargetForDesktopStage(destination));
    }
    removeWindow(window, false);
    if (m_host->isTabletOutputForDesktopStage(destination)) {
        m_host->admitTransferredWindowToTablet(window);
    } else if (sessionForOutput(destination)) {
        const RestoreSnapshot snapshot{
            .window = window,
            .geometry = destinationGeometry,
            .outputName = outputKey(destination),
            .quickTileMode = {},
            .maximizeMode = KWin::MaximizeRestore,
            .fullScreen = false,
            .minimized = false,
            .valid = true,
        };
        addWindow(window, destination, snapshot);
    } else {
        KWin::Window *client = window->window();
        client->sendToOutput(destination);
        client->moveResize(destinationGeometry);
        KWin::workspace()->raiseWindow(client);
        KWin::workspace()->activateWindow(client, true);
    }
    qInfo() << "Kadunce" << Revision << "handed Bento window"
            << window->caption() << "from" << sourceKey << "to"
            << destination->name();
    return true;
}

void DesktopStageController::removeWindow(KWin::EffectWindow *window,
                                   bool restoreSnapshot)
{
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
    applySession(source.value(), false);
    scheduleSettle();
}

void DesktopStageController::handleWindowMoveResizeStarted(KWin::EffectWindow *window)
{
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
            m_settleAttempts = 0;
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
    const QString pendingDropOutput = m_pendingDropOutput;
    m_interactionWindow = nullptr;
    m_interactionStart = {};
    m_interactionOutput.clear();
    m_pendingDropOutput.clear();
    m_interactionResize = false;

    auto source = m_sessions.find(sourceKey);
    if (source == m_sessions.end()) {
        return;
    }
    KWin::LogicalOutput *destination = pendingDropOutput.isEmpty()
        ? KWin::effects->screenAt(KWin::effects->cursorPos().toPoint())
        : outputForKey(pendingDropOutput);
    const bool changedOutput = destination
        && outputKey(destination) != sourceKey;
    if (changedOutput && !resized) {
        handoffWindowToOutput(window, destination,
                                   window->frameGeometry());
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
    applySession(source.value(), false);
    scheduleSettle();
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
            ? minimum.width() / area.width()
            : minimum.height() / area.height();
        lower = std::max(lower, (vertical ? rect.x : rect.y)
                         + std::max(fallbackMinimum, advertised));
    }
    for (const int index : after) {
        const BentoRect &rect = next.at(index);
        const QSizeF minimum = session.windows.at(index)->window()->minSize();
        const double advertised = vertical
            ? minimum.width() / area.width()
            : minimum.height() / area.height();
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
    qInfo() << "Kadunce" << Revision
            << "moved one connected Bento rail on" << session.outputName;
}

void DesktopStageController::handleScreenRemoved(KWin::LogicalOutput *output)
{
    if (!output) {
        return;
    }
    const QString key = outputKey(output);
    if (m_sessions.contains(key)) {
        restoreSession(key, true);
    }
}

} // namespace Kadunce

/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Workspace memory: where every window stood, written to the person's data
// folder every minute and as the session ends; bringing a remembered
// session's apps back to their places as the requester launches them; and
// the sets of apps opened together after signing in (WorkspaceMemory.h,
// docs/REQUESTS.md § Bringing apps back).
#include "EffectInternal.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace Kadunce
{

namespace
{
QString memoryFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kadunce");
}

QString memoryPath()
{
    return memoryFolder() + QStringLiteral("/workspace-memory.json");
}

QString openTogetherPath()
{
    return memoryFolder() + QStringLiteral("/open-together.json");
}

QByteArray readSmallFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    // Both files stay a few kilobytes; anything far larger is not ours.
    return file.size() > 1024 * 1024 ? QByteArray() : file.readAll();
}

bool writeWhole(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(memoryFolder())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(bytes);
    return file.commit();
}

// The sign-in this compositor serves: logind's session id, else this KWin
// since boot, which a restart of KWin within one sign-in reads as a new one.
QString signInId()
{
    const QString session = qEnvironmentVariable("XDG_SESSION_ID");
    if (!session.isEmpty()) return session;
    QFile boot(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    const QString bootId = boot.open(QIODevice::ReadOnly) ? QString::fromLatin1(boot.readAll().trimmed()) : QString();
    return QStringLiteral("kwin-%1-%2").arg(bootId).arg(QCoreApplication::applicationPid());
}
} // namespace

void Effect::startWorkspaceMemory()
{
    m_memory = Memory::MemoryFile::decode(readSmallFile(memoryPath()));
    m_memory.begin(signInId());
    m_openTogether = Memory::OpenTogetherLog::decode(readSmallFile(openTogetherPath()));
    const auto stack = KWin::effects->stackingOrder();
    const bool windowsOpen = std::any_of(stack.cbegin(), stack.cend(),
        [this](KWin::EffectWindow *window) { return isApplicationWindow(window); });
    m_signInGathering.begin(windowsOpen);
    writeMemoryFile();
    m_memorySave.setInterval(MemorySaveMs);
    connect(&m_memorySave, &QTimer::timeout, this, [this] {
        gatherSignInSet(false);
        if (!m_sessionHold.ending()) saveWorkspaceMemory(false);
    });
    m_memorySave.start();
}

void Effect::rememberWorkspaceAtSessionEnd()
{
    m_memorySave.stop();
    gatherSignInSet(true);
    saveWorkspaceMemory(true);
    qInfo() << "Kadunce" << Revision << "remembers" << m_memory.current.windows.size()
            << "windows as the session ends";
}

void Effect::resumeWorkspaceMemory()
{
    m_memoryEndWritten = false;
    if (!m_memorySave.isActive()) m_memorySave.start();
}

Memory::RememberedSession Effect::captureWorkspace()
{
    Memory::RememberedSession session;
    session.sessionId = m_memory.current.sessionId;
    session.savedAt = QDateTime::currentSecsSinceEpoch();
    KWin::EffectWindow *focused = KWin::effects->activeWindow();
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder()) {
        if (!isApplicationWindow(window) || !window->isNormalWindow() || dependentLead(window)) continue;
        // Only an application that can be launched again is remembered.
        QString app = applicationIdentity(window);
        if (app.endsWith(QStringLiteral(".desktop"))) app.chop(8);
        if (app.isEmpty() || !KService::serviceByDesktopName(app)) continue;
        Memory::RememberedWindow entry;
        entry.appId = app;
        KWin::LogicalOutput *output = window->screen();
        entry.output = output ? output->name() : QString();
        const auto desktops = window->desktops();
        entry.desktopId = desktops.size() == 1 && desktops.front() ? desktops.front()->id() : QString();
        const QRectF area = output ? QRectF(output->geometry()) : QRectF();
        const auto shape = window->frameGeometry();
        const QRectF frame(shape.x(), shape.y(), shape.width(), shape.height());
        entry.size = Memory::sizeClass(frame.width(), area.width());
        entry.minimized = window->isMinimized();
        entry.active = window == focused;
        if (DesktopSession *holder = sessionHolding(window)) {
            SessionScope scope(this, holder, holder->keyHolding(window));
            if (m_desktopStage->managesWindow(window)) {
                entry.role = Memory::Role::Pane;
                entry.side = Memory::paneSide(frame, area);
            } else if (m_cardStage->liveCardIndex(window) >= 0) {
                entry.role = Memory::Role::Card;
                const auto snapshot = m_cardStage->workspaceSnapshot();
                if (const auto *card = snapshot.find(windowIdentity(window))) {
                    entry.order = card->cardIndex;
                    // Another workspace's card in use is the one its row selects.
                    if (holder != m_currentSession) entry.active = card->selected;
                }
            }
        }
        session.windows.append(entry);
        if (session.windows.size() >= 64) break;
    }
    return session;
}

void Effect::saveWorkspaceMemory(bool clean)
{
    Memory::RememberedSession captured = captureWorkspace();
    // A periodic save that finds every window where it was writes nothing, so
    // the record's time is when the desk last changed.
    if (!clean && m_memory.current.savedAt > 0 && captured.windows == m_memory.current.windows) return;
    captured.clean = clean;
    m_memory.current = captured;
    writeMemoryFile();
}

void Effect::writeMemoryFile()
{
    if (!writeWhole(memoryPath(), m_memory.encode()))
        qWarning() << "Kadunce" << Revision << "could not write" << memoryPath();
}

void Effect::writeOpenTogether()
{
    if (!writeWhole(openTogetherPath(), m_openTogether.encode()))
        qWarning() << "Kadunce" << Revision << "could not write" << openTogetherPath();
}

void Effect::gatherSignInSet(bool force)
{
    if (!m_signInGathering.gathering() || (!force && !m_signInGathering.due(steadyMs()))) return;
    const QStringList set = m_signInGathering.close();
    if (set.isEmpty()) return;
    m_openTogether.add(set, QDateTime::currentSecsSinceEpoch());
    writeOpenTogether();
}

QVariantMap Effect::savedSession()
{
    if (!callerMayDrive()) return {};
    QVariantMap reply{{QStringLiteral("available"), m_memory.hasOffer()}};
    if (!m_memory.hasOffer()) return reply;
    const auto &offered = *m_memory.offered;
    const QStringList apps = offered.apps();
    reply.insert(QStringLiteral("count"), int(apps.size()));
    reply.insert(QStringLiteral("apps"), apps);
    reply.insert(QStringLiteral("when"), qlonglong(offered.savedAt));
    reply.insert(QStringLiteral("clean"), offered.clean);
    reply.insert(QStringLiteral("windows"), int(offered.windows.size()));
    return reply;
}

QVariantMap Effect::bringBack(const QString &requestToken)
{
    if (!callerMayDrive()) return {};
    const auto refused = [](const QString &reason) {
        return QVariantMap{{QStringLiteral("accepted"), false}, {QStringLiteral("reason"), reason}};
    };
    // Never while a saved layout is being arranged.
    if (m_bringBack || m_layoutRun) return refused(QStringLiteral("busy"));
    if (requestToken.isEmpty() || !m_memory.hasOffer()) return refused(QStringLiteral("nothing"));
    QStringList outputs;
    for (KWin::LogicalOutput *output : KWin::effects->screens())
        if (output) outputs.append(output->name());
    QStringList desktops;
    for (KWin::VirtualDesktop *desktop : KWin::effects->desktops())
        if (desktop) desktops.append(desktop->id());
    QStringList running;
    for (KWin::EffectWindow *window : KWin::effects->stackingOrder())
        if (isApplicationWindow(window)) running.append(applicationIdentity(window));
    KWin::LogicalOutput *inUse = KWin::effects->activeScreen();
    KWin::VirtualDesktop *current = KWin::effects->currentDesktop();
    const auto plan = Memory::planBringBack(*m_memory.offered, outputs, desktops,
        inUse ? inUse->name() : QString(), current ? current->id() : QString(), running);
    // Bringing back answers the offer.
    m_memory.offered.reset();
    writeMemoryFile();
    m_bringBackClaims.clear();
    m_bringBackActive.clear();
    m_bringBackPlacing = 0;
    m_bringBack.emplace(requestToken, plan.steps);
    const auto generation = ++m_bringBackGeneration;
    QTimer::singleShot(BringBackWaitMs, this, [this, generation] {
        if (generation != m_bringBackGeneration || !m_bringBack) return;
        placeBroughtBack(m_bringBack->expire());
    });
    qInfo() << "Kadunce" << Revision << "brings back" << plan.launch.size() << "apps;"
            << plan.alreadyOpen.size() << "already open";
    // A plan with nothing to wait for ends on the next turn, after the reply.
    if (plan.steps.isEmpty()) QTimer::singleShot(0, this, [this] { finishBringBackIfDone(); });
    return {
        {QStringLiteral("accepted"), true},
        {QStringLiteral("launch"), plan.launch},
        {QStringLiteral("alreadyOpen"), plan.alreadyOpen},
        {QStringLiteral("count"), int(plan.launch.size())},
    };
}

bool Effect::reopenedAtSignInForCardStage(const KWin::EffectWindow *window) const
{
    if (m_bringBack || m_layoutRun || !m_memory.hasOffer() || !isApplicationWindow(window)) return false;
    const QString app = LaunchIdentity::normalized(applicationIdentity(window));
    return !app.isEmpty() && std::any_of(m_memory.offered->windows.cbegin(), m_memory.offered->windows.cend(),
        [&app](const auto &remembered) { return LaunchIdentity::normalized(remembered.appId) == app; });
}

bool Effect::usedAtSignOutForCardStage(const KWin::EffectWindow *window) const
{
    if (!m_memory.hasOffer() || !isApplicationWindow(window)) return false;
    const QString app = LaunchIdentity::normalized(applicationIdentity(window));
    return !app.isEmpty() && std::any_of(m_memory.offered->windows.cbegin(), m_memory.offered->windows.cend(),
        [&app](const auto &remembered) { return remembered.active && LaunchIdentity::normalized(remembered.appId) == app; });
}

void Effect::cancelBringBack(const QString &requestToken)
{
    if (!callerMayDrive()) return;
    if (!m_bringBack || m_bringBack->token() != requestToken) return;
    ++m_bringBackGeneration;
    placeBroughtBack(m_bringBack->expire());
}

void Effect::dismissSavedSession()
{
    if (!callerMayDrive()) return;
    if (!m_memory.offered) return;
    m_memory.offered.reset();
    writeMemoryFile();
    scheduleDissolve();
}

QVariantList Effect::openTogether()
{
    if (!callerMayDrive()) return {};
    auto sets = m_openTogether.sets();
    std::stable_sort(sets.begin(), sets.end(), [](const auto &a, const auto &b) { return a.count > b.count; });
    QVariantList reply;
    for (const auto &set : sets)
        reply.append(QVariantMap{{QStringLiteral("apps"), set.apps},
                                 {QStringLiteral("count"), set.count},
                                 {QStringLiteral("lastSeen"), qlonglong(set.lastSeen)}});
    return reply;
}

void Effect::forgetWorkspaceMemory()
{
    if (!callerMayDrive()) return;
    m_memory.offered.reset();
    m_memory.current.windows.clear();
    m_memory.current.savedAt = 0;
    m_openTogether.clear();
    (void)m_signInGathering.close();
    writeMemoryFile();
    writeOpenTogether();
    scheduleDissolve();
}

QSet<QString> Effect::workspacesAwaitedByBringBack() const
{
    QSet<QString> awaited = m_bringBack ? m_bringBack->desktops() : QSet<QString>{};
    // The workspaces of a session still offered are kept until the person
    // has answered, so a bring-back finds them.
    if (m_memory.hasOffer())
        for (const auto &window : m_memory.offered->windows)
            if (!window.desktopId.isEmpty()) awaited.insert(window.desktopId);
    return awaited;
}

void Effect::claimBroughtBackWindow(KWin::EffectWindow *window)
{
    if (!isApplicationWindow(window) || !window->isNormalWindow() || dependentLead(window)) return;
    const QString app = applicationIdentity(window);
    const auto step = m_bringBack ? m_bringBack->claim(app) : std::nullopt;
    // Apps brought back were not opened by hand.
    if (!step) {
        m_signInGathering.noteOpened(app, steadyMs());
        return;
    }
    const QString id = windowIdentity(window);
    KWin::VirtualDesktop *current = KWin::effects->currentDesktop();
    KWin::VirtualDesktop *desktop = step->desktopId.isEmpty()
        ? nullptr : KWin::VirtualDesktopManager::self()->desktopForId(step->desktopId);
    if (!desktop || desktop == current) {
        // Placed once admitted, as a placement request's window is.
        m_bringBackClaims.insert(id, *step);
        return;
    }
    // Another workspace: the window goes there and onto its display before
    // it is admitted anywhere, and that workspace makes it a card when shown.
    KWin::effects->windowToDesktops(window, {desktop});
    KWin::LogicalOutput *output = screenNamed(step->output);
    if (output && window->window() && window->screen() != output) window->window()->sendToOutput(output);
    const auto ready = m_bringBack->arrived(*step, id);
    for (const auto &placement : ready) {
        m_bringBack->notePlaced(true);
        Q_EMIT appBroughtBack(m_bringBack->token(), placement.step.appId, placement.windowId, true);
    }
    QTimer::singleShot(0, this, [this] { finishBringBackIfDone(); });
}

void Effect::completeBringBackForWindow(KWin::EffectWindow *window)
{
    if (!m_bringBack || !window) return;
    const auto claim = m_bringBackClaims.find(windowIdentity(window));
    if (claim == m_bringBackClaims.end()) return;
    const Memory::BringBackStep step = claim.value();
    m_bringBackClaims.erase(claim);
    const QString id = windowIdentity(window);
    // The arrival is admitted first, as every arrival is, and placed from there.
    QTimer::singleShot(0, this, [this, step, id] {
        if (!m_bringBack) return;
        placeBroughtBack(m_bringBack->arrived(step, id));
    });
}

void Effect::placeBroughtBack(QList<Memory::ReadyPlacement> ready)
{
    if (ready.isEmpty() || !m_bringBack) {
        finishBringBackIfDone();
        return;
    }
    const Memory::ReadyPlacement next = ready.takeFirst();
    KWin::EffectWindow *window = nullptr;
    for (KWin::EffectWindow *candidate : KWin::effects->stackingOrder())
        if (candidate && !candidate->isDeleted() && windowIdentity(candidate) == next.windowId) window = candidate;
    bool placed = false;
    if (window) {
        KWin::LogicalOutput *output = screenNamed(next.step.output);
        if (!output) output = window->screen();
        if (output) {
            bool ownsCards = false;
            {
                SessionScope scope(this, m_scopedSession, stageKeyFor(output));
                ownsCards = m_cardStage->canOwnCards(output);
            }
            const QRectF area(output->geometry());
            const QPointF point = Memory::bringBackPoint(next, area, ownsCards);
            const PlacementAim aim = placementAim({{output->name(), area, ownsCards}}, point);
            placed = aim.kind != PlacementAimKind::None && placeWindow(window, aim, point);
        }
    }
    m_bringBack->notePlaced(placed);
    if (placed && next.step.active) m_bringBackActive = window;
    Q_EMIT appBroughtBack(m_bringBack->token(), next.step.appId, next.windowId, placed);
    ++m_bringBackPlacing;
    // A pair's second pane goes once the first stands and has paired, which
    // placing takes a turn or two to finish.
    QTimer::singleShot(ready.isEmpty() ? 0 : 150, this, [this, ready] {
        --m_bringBackPlacing;
        placeBroughtBack(ready);
    });
}

void Effect::finishBringBackIfDone()
{
    if (!m_bringBack || !m_bringBack->finished() || m_bringBackPlacing > 0) return;
    const QString token = m_bringBack->token();
    const int placed = m_bringBack->placed();
    const int missed = m_bringBack->missed();
    m_bringBack.reset();
    m_bringBackClaims.clear();
    ++m_bringBackGeneration;
    // The card that was in use is in use again.
    if (m_bringBackActive && !m_bringBackActive->isDeleted())
        (void)activateApplicationWindow(windowIdentity(m_bringBackActive));
    m_bringBackActive.clear();
    qInfo() << "Kadunce" << Revision << "brought back" << placed << "apps," << missed << "missed";
    Q_EMIT bringBackFinished(token, placed, missed);
    scheduleDissolve();
}

} // namespace Kadunce

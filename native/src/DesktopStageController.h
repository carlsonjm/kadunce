/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "BentoLayout.h"

#include <effect/effectwindow.h>

#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <vector>

namespace KWin
{
class LogicalOutput;
}

namespace Kadunce
{

// Cross-stage operations stay explicit. Desktop Stage owns physical monitor
// composition; its host owns tablet admission and product-wide window rules.
class DesktopStageHost
{
public:
    virtual ~DesktopStageHost() = default;

    [[nodiscard]] virtual bool isTabletOutputForDesktopStage(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const = 0;
    [[nodiscard]] virtual KWin::LogicalOutput *tabletOutputForDesktopStage()
        const = 0;
    [[nodiscard]] virtual KWin::Rect activeTargetForDesktopStage(
        KWin::LogicalOutput *output) const = 0;
    virtual void prepareOutputForDesktopStage(
        KWin::LogicalOutput *output) = 0;
    virtual void admitTransferredWindowToTablet(
        KWin::EffectWindow *window) = 0;
};

class DesktopStageController final
{
public:
    explicit DesktopStageController(DesktopStageHost *host);

    [[nodiscard]] bool hasActiveSession() const;
    [[nodiscard]] bool hasSessionOnOutput(const QString &outputName) const;
    void toggleUnderPointer();
    void restoreAllSessions();
    void stopPendingSettle();

    [[nodiscard]] QStringList outputStageState() const;
    bool toggleOnOutput(const QString &outputName);
    bool handoffLeadToOutput(const QString &sourceName,
                             const QString &destinationName);

    // Returns true when a new window was parked by an existing session.
    bool handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleScreenRemoved(KWin::LogicalOutput *output);
    void handleWindowMoveResizeStarted(KWin::EffectWindow *window);
    void handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                       const KWin::RectF &geometry);
    void handleWindowMoveResizeFinished(KWin::EffectWindow *window);

    // Card Stage has already committed removal before asking Desktop Stage to
    // accept this physical client. False means leave it as an ordinary window.
    [[nodiscard]] bool admitCardWindow(KWin::EffectWindow *window,
                                       KWin::LogicalOutput *output,
                                       const KWin::RectF &geometry);

private:
    struct RestoreSnapshot {
        QPointer<KWin::EffectWindow> window;
        KWin::RectF geometry;
        KWin::RectF floatingGeometry;
        KWin::RectF fullscreenRestoreGeometry;
        QString outputName;
        KWin::QuickTileMode quickTileMode;
        KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
        bool fullScreen = false;
        bool minimized = false;
        bool valid = false;
    };

    struct Session {
        QString outputName;
        QList<QPointer<KWin::EffectWindow>> windows;
        QList<QPointer<KWin::EffectWindow>> overflow;
        QList<RestoreSnapshot> snapshots;
        std::vector<BentoRect> rects;
        bool applying = false;
    };

    [[nodiscard]] QString outputKey(const KWin::LogicalOutput *output) const;
    [[nodiscard]] KWin::LogicalOutput *outputForKey(const QString &key) const;
    [[nodiscard]] KWin::Rect stageArea(KWin::LogicalOutput *output) const;
    [[nodiscard]] Session *sessionForOutput(KWin::LogicalOutput *output);
    [[nodiscard]] const Session *sessionForOutput(
        const KWin::LogicalOutput *output) const;
    [[nodiscard]] QList<QPointer<KWin::EffectWindow>> collectWindows(
        KWin::LogicalOutput *output, KWin::EffectWindow *preferred) const;
    [[nodiscard]] RestoreSnapshot makeSnapshot(
        KWin::EffectWindow *window) const;
    bool activate(KWin::LogicalOutput *output,
                  KWin::EffectWindow *preferred = nullptr);
    void restoreSession(const QString &key, bool outputRemoving = false);
    void applySession(Session &session, bool activateLead);
    void scheduleSettle();
    void settleSessions();
    void removeWindow(KWin::EffectWindow *window, bool restoreSnapshot);
    void addWindow(KWin::EffectWindow *window,
                   KWin::LogicalOutput *output,
                   const RestoreSnapshot &snapshot);
    bool handoffWindowToOutput(KWin::EffectWindow *window,
                               KWin::LogicalOutput *destination,
                               const KWin::RectF &destinationGeometry);
    void reflowSession(Session &session,
                       KWin::EffectWindow *preferred = nullptr);
    void adjustRail(Session &session, KWin::EffectWindow *window,
                    const KWin::RectF &start, const KWin::RectF &finish);

    DesktopStageHost *m_host;
    QTimer m_settleTimer;
    QHash<QString, Session> m_sessions;
    QPointer<KWin::EffectWindow> m_interactionWindow;
    KWin::RectF m_interactionStart;
    QString m_interactionOutput;
    QString m_pendingDropOutput;
    bool m_interactionResize = false;
    int m_settleAttempts = 0;
};

} // namespace Kadunce

/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardStageController.h"
#include "DesktopStageController.h"
#include "WorkspaceInputRouter.h"
#include "DeferredCommandGuard.h"
#include "NativeCarryRuntime.h"
#include "NativeEdgePolicy.h"
#include <options.h>

#include <effect/offscreeneffect.h>
#include "DesktopExitLabel.h"
#include "CardLabelRenderer.h"

#include <QList>
#include <QHash>
#include <QDBusContext>
#include <QPointer>
#include <QStringList>

#include <memory>
#include <array>
#include <optional>

class QAction;
class QDBusServiceWatcher;
class QFileSystemWatcher;

namespace Kadunce
{

class Effect final : public KWin::OffscreenEffect,
                     protected QDBusContext,
                     private WorkspaceInputTarget,
                     private DesktopStageHost,
                     private CardStageHost
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.Kadunce")

public:
    Effect();
    ~Effect() override;

    static bool supported();

    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    void postPaintScreen() override;
    void prePaintWindow(KWin::RenderView *view,
                        KWin::EffectWindow *window,
                        KWin::WindowPrePaintData &data) override;
    void paintScreen(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::LogicalOutput *screen) override;
    void paintWindow(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     KWin::EffectWindow *window,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::WindowPaintData &data) override;
    void drawWindow(const KWin::RenderTarget &renderTarget,
                    const KWin::RenderViewport &viewport,
                    KWin::EffectWindow *window,
                    int mask,
                    const KWin::Region &deviceRegion,
                    KWin::WindowPaintData &data) override;

    [[nodiscard]] int requestedEffectChainPosition() const override
    {
        return 50;
    }

    [[nodiscard]] bool blocksDirectScanout() const override
    {
        return (m_cardStage && m_cardStage->isActive())
            || hasActiveDesktopStage() || bool(m_settlingWindow) || bool(m_carriedWindow) || !m_bentoMotions.isEmpty();
    }

    [[nodiscard]] bool isActive() const override
    {
        return (m_cardStage && m_cardStage->isActive())
            || hasActiveDesktopStage() || bool(m_settlingWindow) || bool(m_carriedWindow) || !m_bentoMotions.isEmpty();
    }

private Q_SLOTS:
    void toggle();
    void release();
    void pageLeft();
    void pageRight();
    void pageStackUp();
    void pageStackDown();
    void toggleBento();
    [[nodiscard]] KWin::LogicalOutput *externalDesktopOutput() const;
    bool pairActiveCardIntoBento(KWin::LogicalOutput *output);

public Q_SLOTS:
    Q_SCRIPTABLE void showCardLine();
    Q_SCRIPTABLE void showActive();
    Q_SCRIPTABLE QStringList outputStageState() const;
    // Which plugin image this compositor actually has open. An installer runs
    // outside KWin and, under a restricted-ptrace kernel, cannot read its maps;
    // KWin can always read its own. Reported as "<inode> present|deleted".
    Q_SCRIPTABLE QString loadedPluginProvenance() const;
    Q_SCRIPTABLE QString workspaceContext() const;
    Q_SCRIPTABLE QString nativeCarryState() const;
    Q_SCRIPTABLE QStringList nativeMoveTrace() const { return m_nativeMoveTrace; }
    Q_SCRIPTABLE bool activateApplicationWindow(const QString &windowId);
    Q_SCRIPTABLE int launcherGuestProtocolVersion() const;
    Q_SCRIPTABLE QString beginLauncherGuest(const QString &ownerService);
    Q_SCRIPTABLE bool setLauncherGuestExpanded(bool expanded);
    Q_SCRIPTABLE void updateLauncherGuest(double horizontalDelta);
    Q_SCRIPTABLE bool finishLauncherGuest(double horizontalDelta);
    Q_SCRIPTABLE bool prepareLauncherGuestLaunch(const QStringList &applicationIds, const QString &requestToken);
    Q_SCRIPTABLE void cancelLauncherGuestLaunch();
    Q_SCRIPTABLE void endLauncherGuest();
    Q_SCRIPTABLE bool toggleBentoOnOutput(const QString &outputName);
    Q_SCRIPTABLE bool handoffBentoLeadToOutput(
        const QString &sourceName, const QString &destinationName);

Q_SIGNALS:
    Q_SCRIPTABLE void workspaceContextChanged();
    Q_SCRIPTABLE void bridgeUnavailable();

private:
    // The tablet kit decides which backend owns the top and bottom edges, and it
    // can appear after the effect loads. These move the session onto the direct
    // router at that point instead of leaving the constructor's answer final.
    void watchForTabletKit();
    void adoptDirectSystemEdges();
    // Source-local bounds only: ordinary window movement does not recapture.
    QHash<KWin::EffectWindow *, std::array<QRectF, 3>> m_previewSourceBounds;
    QHash<KWin::EffectWindow *, QRectF> m_cardLabelTargets;
    QHash<KWin::EffectWindow *, QString> m_applicationDisplayNames;
    CardLabelRenderer m_cardLabelRenderer;
    [[nodiscard]] QString applicationDisplayName(KWin::EffectWindow *window);
    void redirectPreviewSource(KWin::EffectWindow *window);
    struct BentoMotion {
        QPointer<KWin::EffectWindow> window;
        QPointer<KWin::LogicalOutput> output;
        QRectF from, to, outputGeometry;
        QElapsedTimer timer;
    };
    QList<BentoMotion> m_bentoMotions;
    QRectF bentoPresentationRect(KWin::EffectWindow *window) const override;
    void animateBentoLayout(KWin::LogicalOutput *output,
        const QList<QPointer<KWin::EffectWindow>> &windows,
        const QList<QRectF> &from, const QList<QRectF> &to) override;
    std::optional<QRectF> bentoMotionRect(KWin::EffectWindow *window) const;
    void clearBentoMotions();
    bool beginRailFromInput(QPointF p) override {
        return !m_carriedWindow && !m_cardStage->cardGrabActive()
            && !(isTabletPoint(p) && m_cardStage->isActive()) && m_desktopStage->beginRail(p);
    }
    void updateRailFromInput(QPointF p) override { m_desktopStage->updateRail(p); }
    void finishRailFromInput(bool commit) override { m_desktopStage->finishRail(commit); }
    void traceNativeMove(KWin::EffectWindow *window, const char *event);
    QStringList m_nativeMoveTrace;
    QString m_lastCarryDestinationTrace;
    bool completeLauncherGuestForWindow(KWin::EffectWindow *window);
    void handleLaunchWindowChanged();
    static bool isTabletOutput(const KWin::LogicalOutput *output);
    static bool isCardWindow(const KWin::EffectWindow *window);
    static bool isApplicationWindow(const KWin::EffectWindow *window);
    KWin::LogicalOutput *tabletOutput() const;
    [[nodiscard]] bool isTabletOutputForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] KWin::LogicalOutput *tabletOutputForDesktopStage()
        const override;
    [[nodiscard]] bool outputCanOwnCards(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] KWin::Rect activeTargetForDesktopStage(
        KWin::LogicalOutput *output) const override;
    void retireOutputFromDesktopStage(KWin::LogicalOutput *output) override;
    void prepareOutputForDesktopStage(
        KWin::LogicalOutput *output) override;
    [[nodiscard]] std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(
        KWin::EffectWindow *window) const override;
    bool admitDisplacedPaneToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) override;
    bool admitSleepingPaneToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) override;
    [[nodiscard]] KWin::LogicalOutput *tabletOutputForCardStage()
        const override;
    [[nodiscard]] bool isTabletOutputForCardStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool isManagedWindowForCardStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] bool mayHoldWindowForCardStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] std::optional<double> inputPanelTopForCardStage(
        KWin::LogicalOutput *output) const override;
    void setPagingShortcutsForCardStage(bool active) override;
    void cancelInputForCardStage() override;
    void connectManagedWindowForCardStage(
        KWin::EffectWindow *window) override;
    void unredirectForCardStage(KWin::EffectWindow *window) override;
    void retireBentoProjectionForCardStage(
        const QList<QPointer<KWin::EffectWindow>> &windows) override;
    [[nodiscard]] bool admitCardToDesktopStage(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override;
    [[nodiscard]] bool resumeBentoProjectionForCardStage(
        const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) override;
    void setPagingShortcutsActive(bool active);
    [[nodiscard]] WorkspacePresentation presentationForInput() const override;
    [[nodiscard]] WorkspaceInputGeometry geometryForInput() const override;
    [[nodiscard]] bool cardGrabActiveForInput() const override;
    [[nodiscard]] bool nativeWindowInteractionForInput() const override;
    [[nodiscard]] bool stackPreviewArmedForInput() const override;
    [[nodiscard]] int stackPreviewTargetForInput() const override;
    [[nodiscard]] bool centerCardContainsForInput(
        const QPointF &position) const override;
    [[nodiscard]] bool launcherGuestActiveForInput() const override;
    [[nodiscard]] bool launcherGuestContainsForInput(
        const QPointF &position) const override;
    [[nodiscard]] bool isPanelPoint(const QPointF &position) const override;
    [[nodiscard]] bool surfaceOwnsTouchAt(const QPointF &position) const override;
    [[nodiscard]] QRectF nativeLandingAreaForOutput(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool cancelForwardedTouchForInput() override;
    [[nodiscard]] bool isTabletPoint(
        const QPointF &position) const override;
    [[nodiscard]] int activeSideForPoint(
        const QPointF &position) const override;
    [[nodiscard]] bool selectedStackContains(
        const QPointF &position) const override;
    void toggleFromInput() override;
    void dismissLauncherGuestFromInput() override;
    void navigateLauncherGuestFromInput(
        const QPointF &position) override;
    void pageLeftFromInput() override;
    void pageRightFromInput() override;
    void pageStackFromInput(int delta) override;
    void pageHorizontal(int delta);
    void pageStack(int delta);
    void beginCardGrab(const QPointF &position) override;
    void updateCardGrab(const QPointF &position) override;
    void pageCardGrab(int direction) override;
    void finishCardGrab(bool commit) override;
    [[nodiscard]] bool finishCardGrabOnOutput(
        const QPointF &position) override;
    [[nodiscard]] int cardStackCandidate() const override;
    void setCardStackPreview(int destinationId) override;
    void clearCardStackPreview() override;
    [[nodiscard]] bool pageCardStackInsertion(int direction) override;
    [[nodiscard]] int cardStackBrowseTarget() const;
    [[nodiscard]] double cardStackInsertionBlend() const;
    [[nodiscard]] double cardStackPreviewBlend() const;
    void syncSelectedElevation();
    void activateSelectedFromInput() override;
    [[nodiscard]] KWin::EffectWindow *selectedWindow() const;
    void handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleWindowActivated(KWin::EffectWindow *window);
    // CARD-LIFECYCLE.md §8: while a display presents its layout, a card the
    // user calls forward joins that layout. True means the activation was
    // answered here; false leaves it to Card Stage, which by then is no longer
    // presenting Bento.
    [[nodiscard]] bool admitActivatedCardToLiveBento(KWin::EffectWindow *window);
    // §8: retire a layout that cannot take an arrival into a Spread group, so
    // the card the arrival becomes is not drawn over live panes. Both arrival
    // paths ask this before handing the window to Card Stage.
    [[nodiscard]] bool retireLayoutIntoSpreadGroup(KWin::LogicalOutput *output);
    void handleActiveGeometryChanged(KWin::EffectWindow *window,
                                     const KWin::RectF &oldGeometry);
    void handleWindowMoveResizeStarted(KWin::EffectWindow *window);
    void beginLegacyNativeMove(KWin::EffectWindow *window);
    void updateNativeCarryDestination(QPointF contact);
    void endNativeCarryPresentation();
    void handleManagedStateChanged();
    void handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                       const KWin::RectF &geometry);
    void handleWindowMoveResizeFinished(KWin::EffectWindow *window);
    void handleScreenRemoved(KWin::LogicalOutput *output);
    void handleSessionStateChanged();
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    [[nodiscard]] int visibleSlot(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect activeTarget(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool hasActiveDesktopStage() const;
    void connectManagedWindow(KWin::EffectWindow *window);
    bool admitTransferredWindowToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource = [] { return true; },
        const NativeMoveSnapshot *restore = nullptr) override;

    QAction *m_toggleAction = nullptr;
    QAction *m_releaseAction = nullptr;
    QAction *m_previousAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_stackPreviousAction = nullptr;
    QAction *m_stackNextAction = nullptr;
    QAction *m_bentoAction = nullptr;
    void observeCardOwnership();
    // The authority for who owns a window. Both stages' containers are checked
    // against it; it is never repaired from them.
    CardOwnershipLedger m_ownership;
    std::vector<OwnershipViolation> m_observedOwnershipViolations;

    QAction *m_showSpreadAction = nullptr;
    QAction *m_showActiveAction = nullptr;
    bool m_usesDirectSystemEdges = true;
    std::unique_ptr<QFileSystemWatcher> m_tabletKitWatcher;
    std::unique_ptr<WorkspaceInputRouter> m_inputRouter;
    std::unique_ptr<NativeEdgePolicy<KWin::Options>> m_nativeEdgePolicy;
    std::unique_ptr<NativeCarryRuntime> m_carryRuntime;
    QPointer<KWin::EffectWindow> m_carriedWindow;
    QRectF m_carryPickup;
    std::optional<DesktopStageController::PreparedDrop> m_carryDestination;
    std::optional<DesktopStageController::PreparedDrop> m_lineDestination;
    // A tablet edge action that admits to Card Stage has no Bento reservation
    // to hold: its destination is the Active card target on this output.
    // CARD-LIFECYCLE.md §3 and §10 decide which of the two a gesture is.
    QPointer<KWin::LogicalOutput> m_carryCardEntryOutput;
    QPointer<KWin::LogicalOutput> m_lineCardEntryOutput;
    QPointer<KWin::EffectWindow> m_lineDestinationWindow;
    QPointF m_lineDestinationContact;
    DeferredCommandGuard m_inputActivationGuard;
    std::unique_ptr<DesktopStageController> m_desktopStage;
    std::unique_ptr<CardStageController> m_cardStage;
    KWin::LogicalOutput *m_paintingOutput = nullptr;
    bool m_continueRepaint = false;
    QList<QPointer<KWin::EffectWindow>> m_preparationNeighbors;
    bool m_neighborPreparedThisFrame = false;
    unsigned int m_neighborPreparationCursor = 0;
    int m_neighborPreparationFrames = 0;
    QPointer<KWin::EffectWindow> m_nativeCarry;
    QString m_nativeCarrySource;
    bool m_nativeCarryFromBento = false;
    std::unique_ptr<KWin::GLShader> m_fanApertureShader;
    std::unique_ptr<KWin::GLShader> m_destinationShader;
    std::optional<KWin::RectF> m_carryPreview;
    DesktopExitLabel m_detachLabel;
    DesktopExitLabel m_stackSlotLabel;
    std::optional<KWin::RectF> m_linePreview;
    void startDropSettle(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                         const QRectF &from, const QRectF &to);
    void clearDropSettle();
    [[nodiscard]] std::optional<QRectF> dropSettleRect() const;
    QPointer<KWin::EffectWindow> m_settlingWindow;
    QPointer<KWin::LogicalOutput> m_settlingOutput;
    QRectF m_settleFrom, m_settleTo, m_settleOutputGeometry;
    QElapsedTimer m_dropSettleTimer;
    KWin::EffectWindow *m_fanApertureWindow = nullptr;
    QSizeF m_fanPaintSize;
    QPointF m_fanApertureOrigin;
    QSizeF m_fanApertureSize;
    float m_fanApertureRadius = 0.0F;
    int m_fanPaintSizeLocation = -1;
    int m_fanApertureOriginLocation = -1;
    int m_fanApertureSizeLocation = -1;
    int m_fanApertureRadiusLocation = -1;
    QPointer<QDBusServiceWatcher> m_launcherGuestWatcher;
    QString m_launcherGuestOwner;
    bool m_launcherGuestExpanded = false;
    QElapsedTimer m_guestNeighborMotion;
    double m_guestNeighborFrom = 1.0;
    double guestNeighborOpacity() const;
    KWin::Rect launcherGuestExpandedTarget(KWin::LogicalOutput *output) const;
    QPointer<KWin::EffectWindow> m_guestSwipeFocusReturn;
    bool m_launcherGuestLaunchPending = false;
    QStringList m_launcherGuestLaunchApps;
    QString m_launcherGuestLaunchToken;
    quint64 m_guestGeneration = 0;
    QHash<QString, quint64> m_activationOrder;
    quint64 m_activationSequence = 0;
};

} // namespace Kadunce

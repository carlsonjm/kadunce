/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardLineModel.h"
#include "CardLineLayout.h"
#include "ActiveSettings.h"
#include "CardWorkspaceSnapshot.h"
#include "CardWorkspaceState.h"
#include "DeferredCommandGuard.h"
#include "PreparedCarrySource.h"

#include <effect/effectwindow.h>

#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <functional>

namespace KWin
{
class LogicalOutput;
}

namespace Kadunce
{

enum class CardPresentation {
    CardLine,
    Active,
};

// Product-wide operations stay explicit. Card Stage owns the logical card
// transaction; its host owns output discovery, effect redirection, shortcuts,
// desktop-stage acceptance.
class CardStageHost
{
public:
    virtual ~CardStageHost() = default;

    [[nodiscard]] virtual KWin::LogicalOutput *tabletOutputForCardStage()
        const = 0;
    [[nodiscard]] virtual bool isTabletOutputForCardStage(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool isManagedWindowForCardStage(
        const KWin::EffectWindow *window) const = 0;
    virtual void setPagingShortcutsForCardStage(bool active) = 0;
    virtual void cancelInputForCardStage() = 0;
    virtual void connectManagedWindowForCardStage(
        KWin::EffectWindow *window) = 0;
    virtual void unredirectForCardStage(KWin::EffectWindow *window) = 0;
    [[nodiscard]] virtual bool admitCardToDesktopStage(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) = 0;
};

class CardStageController final
{
public:
    explicit CardStageController(CardStageHost *host);
    [[nodiscard]] std::optional<PreparedCarrySource> prepareNativeCarrySource(KWin::EffectWindow *window) const;
    [[nodiscard]] bool nativeCarrySourceValid(const PreparedCarrySource &source) const;
    // Synchronous receiver acceptance. Rejection preserves Active and its restore
    // record; success retires only the departed card, never restores it on source.
    bool transferNativeCarryToDesktop(const PreparedCarrySource &source,
        KWin::LogicalOutput *destination, const KWin::RectF &geometry);

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] CardPresentation presentation() const;
    [[nodiscard]] const CardLineModel &model() const;
    [[nodiscard]] CardWorkspaceSnapshot workspaceSnapshot() const;
    [[nodiscard]] const QList<QPointer<KWin::EffectWindow>> &liveCards() const;
    [[nodiscard]] KWin::EffectWindow *selectedWindow() const;
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    [[nodiscard]] int visibleSlot(const KWin::EffectWindow *window) const;
    [[nodiscard]] QList<QPointer<KWin::EffectWindow>> preparationNeighbors() const;

    [[nodiscard]] bool cardGrabActive() const;
    [[nodiscard]] QPointF cardGrabOffset() const;
    [[nodiscard]] KWin::Rect cardGrabTarget() const;
    [[nodiscard]] int cardGrabPageOffset() const;
    [[nodiscard]] int stackPreviewTarget() const;
    [[nodiscard]] bool stackPreviewArmed() const;
    [[nodiscard]] bool stackInsertionPreviewValid() const;
    [[nodiscard]] KWin::Rect stackPlaceholderTarget() const;
    [[nodiscard]] int stackInsertionIndex() const;
    [[nodiscard]] int previousStackInsertionIndex() const;
    [[nodiscard]] int stackBrowseTarget() const;
    [[nodiscard]] double stackInsertionBlend() const;
    [[nodiscard]] double stackPreviewBlend() const;
    [[nodiscard]] bool animationsRunning() const;
    [[nodiscard]] bool launcherGuestActive() const;
    [[nodiscard]] double launcherGuestOffset() const;
    [[nodiscard]] double launcherGuestTransitionProgress() const;

    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect previewTargetForWindow(
        KWin::LogicalOutput *output, const KWin::EffectWindow *window) const;
    [[nodiscard]] int paintSlot(const KWin::EffectWindow *window) const;
    void anchorRowTransition();
    [[nodiscard]] CardStackPose stackPoseForWindow(const KWin::EffectWindow *window, double width) const;
    [[nodiscard]] KWin::Rect posedTargetForWindow(KWin::LogicalOutput *output, const KWin::EffectWindow *window) const;
    [[nodiscard]] double applyPoseTransition(const KWin::EffectWindow *window, KWin::Rect &rect, CardStackPose &pose) const;
    [[nodiscard]] KWin::Rect launcherGuestTarget(
        KWin::LogicalOutput *output) const;
    [[nodiscard]] KWin::Rect launcherGuestTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect activeTarget(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool selectedStackContains(const QPointF &position) const;
    [[nodiscard]] int activeSideForPoint(const QPointF &position) const;
    [[nodiscard]] QStringList hudState() const;

    void toggle();
    void release();
    void pageHorizontal(int delta);
    void pageStack(int delta);
    [[nodiscard]] bool beginLauncherGuest();
    void updateLauncherGuest(double horizontalDelta);
    [[nodiscard]] bool finishLauncherGuest(double horizontalDelta);
    void endLauncherGuest();

    void beginCardGrab(const QPointF &position);
    void updateCardGrab(const QPointF &position);
    void updateCardGrabDestination(const QPointF &position);
    void pageCardGrab(int direction);
    void finishCardGrab(bool commit);
    [[nodiscard]] bool finishCardGrabOnOutput(const QPointF &position);
    [[nodiscard]] int cardStackCandidate() const;
    void setCardStackPreview(int destinationId);
    void clearCardStackPreview();
    [[nodiscard]] bool pageCardStackInsertion(int direction);

    void syncSelectedElevation();
    void handleWindowActivated(KWin::EffectWindow *window);
    bool admitTransferredWindowToTablet(KWin::EffectWindow *window,
        const std::function<bool()> &commitSource,
        const QRectF &carriedOrigin = {});
    [[nodiscard]] bool handleWindowAdded(KWin::EffectWindow *window);
    void stageWindowArrival(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleActiveGeometryChanged(KWin::EffectWindow *window);
    void handleManualWindowChange(KWin::EffectWindow *window);

private:
    struct ActiveRestoreSnapshot {
        QPointer<KWin::EffectWindow> window;
        KWin::RectF geometry;
        KWin::RectF floatingGeometry;
        KWin::RectF fullscreenRestoreGeometry;
        KWin::QuickTileMode quickTileMode;
        KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
        bool fullScreen = false;
        bool valid = false;
    };

    void rebuildLiveCards();
    void captureCardTransition(bool includeGuest = false, bool includeGrab = false);
    void clearCardTransition();
    void startArrivalTimer(KWin::EffectWindow *window);
    void finishNewArrival(KWin::EffectWindow *window, bool animateArrival, int previousSelection);
    bool enterActive();
    void restoreActiveSnapshot();
    void resetCardGrabState(KWin::EffectWindow *grabbed, bool stacked);
    void syncSelectedStackingOrder();
    void restoreOriginalStackingOrder();

    CardStageHost *m_host;
    std::shared_ptr<const int> m_carrySourceIdentity = std::make_shared<const int>(0);
    quint64 m_restoreGeneration = 0;
    DeferredCommandGuard m_transferGuard;
    ActiveSettings m_settings;
    CardWorkspaceState<QPointer<KWin::EffectWindow>> m_workspace;
    struct PreviewOrigin {
        QPointer<KWin::EffectWindow> window;
        QRectF normalized;
        double rotation = 0.0;
        bool visible = true;
        double opacity = 1.0;
        int slot = 99;
    };
    QList<PreviewOrigin> m_previewOrigins;
    QElapsedTimer m_previewTransition;
    bool m_poseTransition = false;
    bool m_rowPageTransition = false;
    bool m_pickupTransition = false;
    int m_stackBrowseDirection = 0;
    QPointer<KWin::EffectWindow> m_stackBrowseOutgoing;
    double m_rowDisplacement = 0; // normalized shared horizontal travel
    QTimer m_arrivalTimer;
    QElapsedTimer m_arrivalWait;
    QPointer<KWin::EffectWindow> m_arrivalWindow;
    bool m_arrivalExpanding = false;
    QList<QPointer<KWin::EffectWindow>> m_originalCardStackingOrder;
    ActiveRestoreSnapshot m_activeRestore;
    bool m_applyingWindowState = false;
    QTimer m_activeSettleTimer;
    int m_activeSettleRemaining = 0;
    CardPresentation m_presentation = CardPresentation::CardLine;
    QPointF m_cardGrabOffset;
    QPointF m_cardGrabStart;
    KWin::Rect m_cardGrabTarget;
    double m_cardGrabRotation = 0.0;
    QSizeF m_cardGrabDestinationSize;
    QElapsedTimer m_cardGrabScaleTimer;
    int m_cardGrabPageOffset = 0;
    int m_cardStackPreviewTarget = 0;
    int m_cardStackInsertionIndex = -1;
    int m_cardStackPreviousInsertionIndex = -1;
    double m_cardStackPreviewFrom = 0.0;
    double m_cardStackPreviewTo = 0.0;
    QElapsedTimer m_cardStackPreviewTimer;
    QElapsedTimer m_cardStackInsertionTimer;
    QPointF m_cardGrabPointer;
    QString m_cardGrabDestinationOutput;
    bool m_cardGrabMoved = false;
    bool m_cardStackPreviewArmed = false;
    bool m_cardGrabActive = false;
    quint64 m_cardStackPreviewRevision = 0;
    std::optional<CardWorkspaceState<QPointer<KWin::EffectWindow>>::PreparedStackInsertion> m_stackInsertion;
    double m_launcherGuestOffset = 0.0;
    double m_launcherGuestTransitionFrom = 0.0;
    QElapsedTimer m_launcherGuestTransitionTimer;
    int m_launcherGuestPendingPage = 0;
    bool m_launcherGuestActive = false;
    bool m_launcherGuestArrival = false;
    int m_launcherGuestGroupCount = 0;
    int m_launcherGuestPrimarySide = 1;
    QPointer<KWin::EffectWindow> m_launcherGuestPrimaryWindow;
    QPointer<KWin::EffectWindow> m_launcherGuestSecondaryWindow;
    bool m_active = false;
};

} // namespace Kadunce

/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "SpreadModel.h"
#include "SpreadLayout.h"
#include "ActiveSettings.h"
#include "CardWorkspaceSnapshot.h"
#include "CardWorkspaceState.h"
#include "BentoProjectionSession.h"
#include "DeferredCommandGuard.h"
#include "PreparedCarrySource.h"
#include "RestoredMinimization.h"

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
    Spread,
    Active,
    // The display's Bento pane combination is shown. Card Stage still owns its
    // individual cards and keeps them hidden, per CARD-LIFECYCLE.md §2: Bento
    // owns its panes, not the rest of the display.
    Bento,
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
    virtual void retireBentoProjectionForCardStage(
        const QList<QPointer<KWin::EffectWindow>> &windows) = 0;
    [[nodiscard]] virtual bool admitCardToDesktopStage(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource) = 0;
    [[nodiscard]] virtual bool resumeBentoProjectionForCardStage(
        const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
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
    // Presentation and navigation context, never a fourth owner: which
    // individual card is Active. It survives Spread so a side snap can find the
    // card to pair with, and retires as soon as that card stops being an
    // individual card.
    [[nodiscard]] KWin::EffectWindow *activeCardIdentity() const;
    // CARD-LIFECYCLE.md §4 "Eligible as a Bento partner", asked in one place.
    // Adoption eligibility is a different question and never stands in for it.
    [[nodiscard]] bool isEligiblePartner(const KWin::EffectWindow *window) const;
    // §3 names the partner: the Active card when something else is carried,
    // otherwise the nearest eligible card on the contacted side of the carried
    // card in Spread order. Read-only — a prepared carry embeds the workspace
    // revision, so naming a partner must not move selection or the pair side.
    [[nodiscard]] KWin::EffectWindow *partnerForSideSnap(
        const KWin::EffectWindow *carried, bool leftEdge) const;
    // Whether this display can hold cards at all. State and capability decide
    // the grammar; the display's hardware identity never does.
    [[nodiscard]] bool canOwnCards(const KWin::LogicalOutput *output) const;
    [[nodiscard]] bool ownsDisplay(const KWin::LogicalOutput *output) const;
    [[nodiscard]] const SpreadModel &model() const;
    [[nodiscard]] CardWorkspaceSnapshot workspaceSnapshot() const;
    [[nodiscard]] const QList<QPointer<KWin::EffectWindow>> &liveCards() const;
    [[nodiscard]] KWin::EffectWindow *selectedWindow() const;
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    // Presentation provenance only. These windows entered Spread while their
    // live surfaces still had Bento pane dimensions; membership and restoration
    // remain entirely in the ordinary workspace/restore owners.
    [[nodiscard]] bool usesBentoProjectionAperture(
        const KWin::EffectWindow *window) const;
    [[nodiscard]] bool isBentoProjectionPane(
        const KWin::EffectWindow *window) const;
    [[nodiscard]] bool selectedIsBentoProjection() const;
    [[nodiscard]] bool selectedIsBentoGroup() const;
    [[nodiscard]] QList<QPointer<KWin::EffectWindow>> bentoProjectionPanes() const;
    [[nodiscard]] KWin::Rect bentoProjectionWorkspace() const;
    [[nodiscard]] std::optional<BentoRect> bentoProjectionRect(
        const KWin::EffectWindow *window) const;
    [[nodiscard]] bool resumeSelectedBentoProjection();
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
        const QRectF &carriedOrigin = {}, const NativeMoveSnapshot *restore = nullptr);
    // CARD-LIFECYCLE.md §3: the first deliberate edge action on the display
    // adopts every eligible window as an individual card and presents the
    // carried one as Active. No layout is solved and no pane is filled.
    bool adoptDisplayWithActive(KWin::EffectWindow *carried,
        const std::function<bool()> &commitSource);
    // §3: the Active card gives up individual ownership so the destination can
    // publish it and the carried window as the display's only two panes. The
    // carried window may still be Native, in which case this stage has nothing
    // of its own to give up. Membership only; the destination is already proven.
    bool releasePairToBento(KWin::EffectWindow *carried, KWin::EffectWindow *partner);
    // §8: a card the user calls forward while the display presents its layout
    // joins that layout. This stage gives up exactly that card's individual
    // ownership and keeps presenting Bento; the destination has already proved
    // it can show the card and is publishing around this.
    bool releaseCardToLiveBento(KWin::EffectWindow *card);
    // §5: the other direction. A pane that yielded its slot becomes a
    // nonselected individual card while the display is still presenting the
    // layout it left, so it takes membership and its pre-Bento record and
    // nothing else: no Active geometry, no selection, no change of
    // presentation. `admitTransferredWindowToTablet` cannot serve this — it
    // exists to make an arriving window the Active card.
    bool admitDisplacedPaneAsHiddenCard(KWin::EffectWindow *window,
        const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore);
    // Whether this stage is presenting the display's Bento layout rather than
    // its own cards, so a caller can route an activation to the layout.
    [[nodiscard]] bool presentsBento() const {
        return m_active && m_presentation == CardPresentation::Bento;
    }
    // §10: a carried window with nothing to pair with becomes the Active card.
    bool promoteToActive(KWin::EffectWindow *window);
    // The display's Bento layout ended, so this stage cannot still be
    // presenting one. §12: what this stage owns returns to Spread.
    void leaveBentoPresentation();
    [[nodiscard]] bool handleWindowAdded(KWin::EffectWindow *window);
    void stageWindowArrival(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    void handleActiveGeometryChanged(KWin::EffectWindow *window);
    void handleManualWindowChange(KWin::EffectWindow *window);
    [[nodiscard]] std::optional<NativeMoveSnapshot> managedRestore(KWin::EffectWindow *window) const;

    bool admitBentoStack(const BentoProjectionSession &projection,
                         const std::function<bool()> &commitSource);

private:
    struct ActiveRestoreSnapshot {
        QPointer<KWin::EffectWindow> window;
        KWin::RectF geometry;
        KWin::RectF floatingGeometry;
        KWin::RectF fullscreenRestoreGeometry;
        KWin::QuickTileMode quickTileMode;
        KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
        bool fullScreen = false;
        bool minimized = false;
        bool valid = false;
    };

    void rebuildLiveCards();
    void retainManagedOwnership(KWin::EffectWindow *window);
    void captureCardTransition(bool includeGuest = false, bool includeGrab = false);
    void clearCardTransition();
    void startArrivalTimer(KWin::EffectWindow *window);
    void finishNewArrival(KWin::EffectWindow *window, bool animateArrival, int previousSelection);
    bool enterActive();
    void restoreActiveSnapshot();
    void parkActiveSnapshot();
    void forgetManagedRestore(KWin::EffectWindow *window);
    void retireActiveIdentity(const KWin::EffectWindow *window);
    bool selectCardEntry(KWin::EffectWindow *window);
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
    QList<QPointer<KWin::EffectWindow>> m_bentoProjectionWindows;
    QList<QPointer<KWin::EffectWindow>> m_bentoProjectionPaneWindows;
    std::optional<BentoProjectionSession> m_bentoProjectionSession;
    ActiveRestoreSnapshot m_activeRestore;
    QPointer<KWin::EffectWindow> m_presentedActive;
    QList<ActiveRestoreSnapshot> m_parkedRestores;
    std::vector<std::unique_ptr<RestoredMinimization>> m_restoredMinimizations;
    bool m_applyingWindowState = false;
    QTimer m_activeSettleTimer;
    int m_activeSettleRemaining = 0;
    CardPresentation m_presentation = CardPresentation::Spread;
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

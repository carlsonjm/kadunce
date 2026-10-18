/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "CardBook.h"
#include "CardStageController.h"
#include "DesktopStageController.h"
#include "DisplaySizing.h"
#include "WorkspaceInputRouter.h"
#include "DeferredCommandGuard.h"
#include "NativeCarryRuntime.h"
#include "PaneStackAim.h"
#include "StackScrub.h"
#include "WheelWidth.h"
#include "TouchDisplay.h"
#include "TouchWitness.h"
#include "NativeEdgePolicy.h"
#include "KeyboardOverlayPolicy.h"
#include "PaintResult.h"
#include "PendingLaunch.h"
#include "PlacementAim.h"
#include "PerOutputDesktopsPolicy.h"
#include "TableGesture.h"
#include "TableLayout.h"
#include "TablePresenter.h"
#include "KeyFocus.h"
#include "SpokenLine.h"
#include "SpreadReading.h"
#include "KeyRoute.h"
#include "HeldPointer.h"
#include <options.h>

#include <effect/effectwindow.h>
#include <effect/offscreeneffect.h>
#include "DesktopExitLabel.h"
#include "GestureNoteLabel.h"
#include "CardLabelRenderer.h"
#include "NoteStackRenderer.h"
#include "KeptPicture.h"
#include "StuckNotes.h"

#include <QJsonValue>
#include <QList>
#include <QHash>
#include <QIcon>
#include <QTimer>
#include <QDBusContext>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include <map>
#include <memory>
#include <array>
#include <map>
#include <optional>

class QAction;
class QDBusServiceWatcher;

namespace KWin
{
class SurfaceInterface;
}

namespace Kadunce
{

class StuckNotesWatcher;

class Effect final : public KWin::OffscreenEffect,
                     protected QDBusContext,
                     private WorkspaceInputTarget,
                     private DesktopStageHost,
                     private CardStageHost
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "co.goodinput.Kadunce")

public:
    Effect();
    ~Effect() override;

    static bool supported();

    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    void postPaintScreen() override;
    void prePaintWindow(KWin::RenderView *view,
                        KWin::EffectWindow *window,
                        KWin::WindowPrePaintData &data) override;
    PaintResult paintScreen(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::LogicalOutput *screen) override;
    PaintResult paintWindow(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     KWin::EffectWindow *window,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::WindowPaintData &data) override;
    PaintResult drawWindow(const KWin::RenderTarget &renderTarget,
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
        return anyCardStageActive()
            || hasActiveDesktopStage() || bool(m_settlingWindow) || bool(m_carriedWindow) || !m_bentoMotions.isEmpty()
            || !m_yieldingPanes.isEmpty() || bool(m_previewDesktop) || bool(m_gestureNote);
    }

    // Painting while any display's cards are on, and while windows are given
    // rounded corners, which every window keeps, card or not.
    [[nodiscard]] bool isActive() const override
    {
        return anyCardStageActive() || roundsCorners()
            || hasActiveDesktopStage() || bool(m_settlingWindow) || bool(m_carriedWindow) || !m_bentoMotions.isEmpty()
            || !m_yieldingPanes.isEmpty() || keysAwaitingPerson() || bool(m_previewDesktop) || bool(m_gestureNote);
    }

    // Keys on screen that are not yet known to be the person's are painted
    // through this effect, which withholds them, whether or not it holds any
    // card. Once they are the person's, or gone, it asks for nothing.
    // Table by pointer and keyboard: a push into a display's top-left corner
    // opens it there, and while it is open the keys move through it.
    bool borderActivated(KWin::ElectricBorder border) override;
    void grabbedKeyboardEvent(QKeyEvent *event) override;

    [[nodiscard]] bool keysAwaitingPerson() const
    {
        const KWin::EffectWindow *panel = KWin::effects->inputPanel();
        return !m_keysForPerson && panel && panel->isVisible();
    }

private Q_SLOTS:
    // Table's corner is every display's top-left, which KDE gives Overview.
    // Overview's reservation is held off while this runs and given back on
    // unload; a corner the person gave to something else stays theirs.
    void holdTableCorner();
    void toggle();
    void release();
    void pageLeft();
    void pageRight();
    void pageStackUp();
    void pageStackDown();
    void toggleBento();
    // Search floating, as Tettegouche says it is shown or hidden.
    void floatingLauncherShown(bool shown);
    [[nodiscard]] KWin::LogicalOutput *externalDesktopOutput() const;
    bool pairActiveCardIntoBento(KWin::LogicalOutput *output);
    bool pairActiveCardIntoBento(KWin::LogicalOutput *output, BentoSidePlacement side, bool walkLeft);

public Q_SLOTS:
    Q_SCRIPTABLE void showCardLine();
    Q_SCRIPTABLE void showActive();
    // CARD-LIFECYCLE.md §11: the same, for the Spread of the display named.
    Q_SCRIPTABLE void showCardLineOn(const QString &outputName);
    Q_SCRIPTABLE void showActiveOn(const QString &outputName);
    Q_SCRIPTABLE QStringList outputStageState() const;
    // Which plugin image this compositor actually has open. An installer runs
    // outside KWin and, under a restricted-ptrace kernel, cannot read its maps;
    // KWin can always read its own. Reported as "<inode> present|deleted".
    Q_SCRIPTABLE QString loadedPluginProvenance() const;
    Q_SCRIPTABLE QString workspaceContext() const;
    Q_SCRIPTABLE QString nativeCarryState() const;
    Q_SCRIPTABLE QStringList nativeMoveTrace() const { return m_nativeMoveTrace; }
    Q_SCRIPTABLE bool activateApplicationWindow(const QString &windowId);
    Q_SCRIPTABLE void raiseKeyboard();
    // The keys' own word on where they are going: they come to rest `height`
    // logical pixels tall above their screen's bottom edge, 0 when leaving,
    // in `durationMs`. Optional: keys that never say are read as they move.
    Q_SCRIPTABLE void keyboardHeading(double height, int durationMs);
    Q_SCRIPTABLE int launcherGuestProtocolVersion() const;
    Q_SCRIPTABLE QString beginLauncherGuest(const QString &ownerService);
    // Any application may hold Spread's centre while Spread is shown, answered
    // on its own object (TETTEGOUCHE-CONTEXT.md § Companion guests).
    Q_SCRIPTABLE int companionGuestProtocolVersion() const;
    Q_SCRIPTABLE QString beginCompanionGuest(const QString &ownerService,
        const QString &objectPath, const QString &interfaceName);
    Q_SCRIPTABLE bool setLauncherGuestExpanded(bool expanded);
    Q_SCRIPTABLE void updateLauncherGuest(double horizontalDelta);
    Q_SCRIPTABLE bool finishLauncherGuest(double horizontalDelta);
    Q_SCRIPTABLE bool prepareLauncherGuestLaunch(const QStringList &applicationIds, const QString &requestToken);
    Q_SCRIPTABLE void cancelLauncherGuestLaunch();
    Q_SCRIPTABLE void endLauncherGuest();
    // Placement requests: docs/REQUESTS.md.
    Q_SCRIPTABLE int placementProtocolVersion() const;
    Q_SCRIPTABLE QString aimPlacement(double x, double y);
    Q_SCRIPTABLE void clearPlacementAim();
    Q_SCRIPTABLE bool placeApplication(const QStringList &applicationIds, const QString &windowId,
                                       double x, double y, const QString &requestToken);
    Q_SCRIPTABLE void cancelPlacement(const QString &requestToken);
    Q_SCRIPTABLE bool toggleBentoOnOutput(const QString &outputName);
    // A strip along a display's top edge kept from Table's pull, in that
    // display's own coordinates, under the caller's key; held until cleared or
    // the caller leaves the bus (TETTEGOUCHE-CONTEXT.md § Top-edge claims).
    Q_SCRIPTABLE bool claimTopEdge(const QString &key, const QString &outputName,
        double x, double y, double width, double height);
    Q_SCRIPTABLE void clearTopEdgeClaim(const QString &key);
    // A display whose context a surface shows in words, under the caller's
    // key; while held, Kadunce draws none of its own words there and says
    // them in the workspace context instead, until cleared or the caller
    // leaves the bus (TETTEGOUCHE-CONTEXT.md § Context-line claims).
    Q_SCRIPTABLE bool claimContextLine(const QString &key, const QString &outputName);
    Q_SCRIPTABLE void clearContextLineClaim(const QString &key);
    // Monitors fill themselves unless asked to take the zones drawn with Meta+T.
    Q_SCRIPTABLE bool usesDrawnZones() const { return m_usesDrawnZones; }
    Q_SCRIPTABLE void setUsesDrawnZones(bool uses);
    // The display Spread lives on, by name; empty while Kadunce chooses
    // (TouchDisplay.h). Kept in kaduncerc, and followed at once.
    Q_SCRIPTABLE QString spreadDisplay() const { return m_spreadChoice.display; }
    Q_SCRIPTABLE void setSpreadDisplay(const QString &outputName);
    // How a display plays cards: "solo" or "connected" (DisplaySizing.h).
    // "active", Solo's earlier name, is accepted and kept as "solo". The choice is kept per display in
    // kaduncerc's [Sizing] group and followed at once; an empty choice lets
    // Kadunce choose by room.
    Q_SCRIPTABLE QString displaySizing(const QString &outputName) const;
    Q_SCRIPTABLE void setDisplaySizing(const QString &outputName, const QString &sizing);
    Q_SCRIPTABLE bool handoffBentoLeadToOutput(
        const QString &sourceName, const QString &destinationName);
    // Table's preview: another desktop drawn on every display in place of the
    // current one, as it stands, without switching to it. An empty id, or the
    // current desktop's, ends the preview and changes nothing.
    Q_SCRIPTABLE bool previewDesktop(const QString &desktopId);
    // Makes the previewed desktop current, with KWin's switching effects
    // standing aside, and ends the preview.
    Q_SCRIPTABLE bool commitDesktopPreview();
    // Table as it stands, for a test: whether it is open, which level, which
    // workspace and card the finger is on, and every workspace's cards.
    Q_SCRIPTABLE QString tableState() const;
    // Opens Table on the display under the pointer, as its shortcut does, or
    // closes it.
    Q_SCRIPTABLE void toggleTable();
    // Every ownership violation observed since load, in words, so a test can
    // require none; a warning in the journal is not something a test can read.
    Q_SCRIPTABLE QStringList ownershipViolations() const { return m_ownershipViolationLog; }
    // Every card book mismatch observed since load, in words: each place the
    // records of a card's status disagree.
    Q_SCRIPTABLE QStringList cardBookMismatches() const { return m_cardBookLog; }

Q_SIGNALS:
    Q_SCRIPTABLE void workspaceContextChanged();
    Q_SCRIPTABLE void bridgeUnavailable();
    Q_SCRIPTABLE void placementSettled(const QString &requestToken, const QString &windowId, bool placed);

private:
    // A touchscreen driving a display decides which backend owns the top and
    // bottom edges, and it can appear after the effect loads. This moves the
    // session onto the direct router then instead of leaving the constructor's
    // answer final.
    void adoptDirectSystemEdges();
    // Source-local bounds only: ordinary window movement does not recapture.
    QHash<KWin::EffectWindow *, std::array<QRectF, 3>> m_previewSourceBounds;
    QHash<KWin::EffectWindow *, QRectF> m_cardLabelTargets;
    // Where each card's window is drawn this frame, inside its card: what
    // KWin is told to draw it at, kept as it is told. Notes placed on the
    // window stand on this, so they move with the picture frame by frame.
    QHash<KWin::EffectWindow *, QRectF> m_cardFaceRects;
    mutable QHash<KWin::EffectWindow *, QString> m_applicationDisplayNames;
    CardLabelRenderer m_cardLabelRenderer;
    [[nodiscard]] QString applicationDisplayName(KWin::EffectWindow *window) const;
    // The cards Spread shows behind a Stack's face, nearest first.
    [[nodiscard]] QStringList namesBehindInSpread(int cardId) const;
    // A window carried to another display and rested on a pane or card there
    // joins that Stack once it has landed; it retries while the landing settles.
    void stackWhenLanded(QPointer<KWin::EffectWindow> carried, QPointer<KWin::EffectWindow> onto,
                         int tries = 20);
    // A card's name in Spread: a Stack names the cards behind its face, a
    // pane alone the panes beside it.
    [[nodiscard]] QString spreadCardName(KWin::EffectWindow *window, int cardId) const;
    void redirectPreviewSource(KWin::EffectWindow *window);
    // A card Kadunce resizes while Spread shows it keeps its last good picture,
    // fitted to its new frame, until the app has drawn itself at that size:
    // an app's first frames at a new size can be empty. Only that window is
    // kept, and for at most HeldPictureLimitMs.
    struct HeldPicture {
        KeptPicture picture;
        QSizeF bufferAtCapture;
        std::function<void()> resize;
        int drawnSince = 0;
        QMetaObject::Connection damaged;
    };
    std::map<KWin::EffectWindow *, HeldPicture> m_heldPictures;
    KWin::EffectWindow *m_capturingHeld = nullptr;
    void captureHeldPictures();
    void releaseHeldPicture(KWin::EffectWindow *window);
    // Redirects a window into its own picture, taken again when its frame and
    // buffer stop lining up; true when the picture is new.
    bool redirectSource(KWin::EffectWindow *window, bool reportChange);
    // Whether Kadunce cuts this window's corners to the paper tier itself
    // (ITASCA-VISUAL-LANGUAGE.md § Rounded boxes are information).
    [[nodiscard]] bool trimsCorners(KWin::EffectWindow *window) const;
    PaintResult drawTrimmedWindow(const KWin::RenderTarget &renderTarget,
                                  const KWin::RenderViewport &viewport,
                                  KWin::EffectWindow *window,
                                  int mask,
                                  const KWin::Region &deviceRegion,
                                  KWin::WindowPaintData &data);
    struct BentoMotion {
        QPointer<KWin::EffectWindow> window;
        QPointer<KWin::LogicalOutput> output;
        QRectF from, to, outputGeometry;
        QElapsedTimer timer;
        // A card let go on a pane waits at `from` from when its move was
        // `asked` for, and its `timer` starts once a frame has drawn it
        // moving (PaneArrival.h).
        bool arrival = false;
        bool drawnMoving = false;
        QElapsedTimer asked;
    };
    QList<BentoMotion> m_bentoMotions;
    // Windows opened in the last moment: placed in their panes without motion.
    static constexpr qint64 OpenedStill = 2000;
    QList<QPair<QPointer<KWin::EffectWindow>, QElapsedTimer>> m_justOpened;
    static constexpr qint64 StepInUnseen = 400;
    mutable QList<QPair<QPointer<KWin::EffectWindow>, QElapsedTimer>> m_steppingIn;
    struct LeftLayout {
        bool right = false;
        QList<QPointer<KWin::EffectWindow>> partners;
    };
    QHash<const KWin::EffectWindow *, LeftLayout> m_leftLayouts;
    void startPaneArrival(KWin::EffectWindow *window, KWin::LogicalOutput *output, const QRectF &from);
    QRectF bentoPresentationRect(KWin::EffectWindow *window) const override;
    void animateBentoLayout(KWin::LogicalOutput *output,
        const QList<QPointer<KWin::EffectWindow>> &windows,
        const QList<QRectF> &from, const QList<QRectF> &to) override;
    std::optional<QRectF> bentoMotionRect(KWin::EffectWindow *window) const;
    void clearBentoMotions();
    // CARD-LIFECYCLE.md §8: a pane that yields to an arrival fades where it
    // stood while the arrival takes its place, rather than vanishing.
    struct YieldingPane {
        QPointer<KWin::EffectWindow> window;
        QPointer<KWin::LogicalOutput> output;
        QRectF rect, outputGeometry;
        QElapsedTimer timer;
    };
    QList<YieldingPane> m_yieldingPanes;
    void paneYieldedForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                    const QRectF &drawn) override;
    // How opaque a yielding pane is drawn now, or nothing once it is gone.
    [[nodiscard]] std::optional<double> yieldingPaneOpacity(KWin::EffectWindow *window) const;
    void clearYieldingPanes();
    // CARD-LIFECYCLE.md §5: the last pane of a layout grows from where it was
    // drawn into its place as the Active card.
    void lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                                           const QRectF &drawn) override;
    // §11: a display its layout's last pane left for another shows its desktop.
    void layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *output) override;
    // On the card display a divider is Bento's only while Bento is what the
    // display presents; the cards it hides behind the layout do not take it.
    bool beginRailFromInput(QPointF p) override {
        return !m_carriedWindow && !m_cardStage->cardGrabActive()
            && !(isTabletPoint(p) && m_cardStage->isActive() && !m_cardStage->presentsBento())
            && m_desktopStage->beginRail(p);
    }
    [[nodiscard]] bool inPaneGutterForInput(const QPointF &p) const override {
        return isTabletPoint(p) && m_cardStage->presentsBento() && m_desktopStage->inPaneGutter(p);
    }
    void updateRailFromInput(QPointF p) override { m_desktopStage->updateRail(p); }
    void finishRailFromInput(bool commit) override { m_desktopStage->finishRail(commit); }
    // Spread's row and the card under one finger.
    [[nodiscard]] bool rowMovingForInput() const override { return m_cardStage->rowMoving(); }
    [[nodiscard]] bool rowStillForInput() const override { return m_cardStage->rowStill(); }
    [[nodiscard]] bool scrollFromFingersForInput() const override;
    bool catchRowFromInput() override { return m_cardStage->catchRow(); }
    void settleRowFromInput() override { m_cardStage->settleRowFromStroke(); }
    bool beginRowFromInput() override { return m_cardStage->beginRowDrag(); }
    void updateRowFromInput(double travel) override { m_cardStage->updateRowDrag(travel); }
    void finishRowFromInput(double velocity) override { m_cardStage->finishRowDrag(velocity); }
    bool beginScrubFromInput() override { return m_cardStage->beginScrub(); }
    void updateScrubFromInput(double travel) override { m_cardStage->updateScrub(travel); }
    void finishScrubFromInput() override { m_cardStage->finishScrub(); }
    bool beginLiftFromInput(const QPointF &position) override { return m_cardStage->beginLift(position); }
    void updateLiftFromInput(double travel) override { m_cardStage->updateLift(travel); }
    void finishLiftFromInput(double velocity) override { m_cardStage->finishLift(velocity); }
    void cancelStrokeFromInput() override { m_cardStage->cancelStroke(); }
    void presentSelectedForCardStage() override { activateSelectedFromInput(); }
    void cardGutterChangedForCardStage() override;
    [[nodiscard]] bool connectedForCardStage() const override;
    [[nodiscard]] bool layoutLeavesRoomForCardStage() const override;
    [[nodiscard]] bool holdsOwnCardsForCardStage(const KWin::LogicalOutput *output) const override
    {
        return isOtherCardDisplay(output);
    }
    bool moveCardToDisplayForCardStage(KWin::EffectWindow *card, KWin::LogicalOutput *to) override
    {
        return moveCardToDisplay(card, to, true);
    }
    [[nodiscard]] KWin::LogicalOutput *releaseDisplayForCardStage(
        const KWin::EffectWindow *window) const override;
    void traceNativeMove(KWin::EffectWindow *window, const char *event);
    QStringList m_nativeMoveTrace;
    QString m_lastCarryDestinationTrace;
    // The last edge decision written to the journal in this carry.
    QString m_lastEdgeEntryLog;
    bool completeLauncherGuestForWindow(KWin::EffectWindow *window);
    QString acceptLauncherGuest(const QString &ownerService, const QString &objectPath,
        const QString &interfaceName, int protocol);
    void callLauncherGuestOwner(const QString &method, const QVariantList &arguments = {});
    // Search declined while Spread is not shown floats over the work. Spread
    // opening asks it back, so it joins as though opened in Spread.
    void floatLauncher(const QString &owner);
    void forgetFloatingLauncher();
    void inviteFloatingLauncher();
    QString m_floatingLauncherOwner;
    bool m_spreadOpenForLauncher = false;
    // Whether the call being answered may steer Spread's guest: one Kadunce
    // makes itself, or one over D-Bus from the guest's own connection
    // (GuestCaller.h).
    [[nodiscard]] bool callerIsGuest() const;
    // Whether the call being answered may bring anything forward: one Kadunce
    // makes itself, or one over D-Bus from a program of the suite's
    // (CallerTrust.h). A stranger is answered with an access error.
    [[nodiscard]] bool callerMayDrive() const;
    // Each connection's answer, kept for as long as the bus runs: a
    // connection's unique name is never given to another.
    mutable QHash<QString, bool> m_callerVerdicts;
    // What endLauncherGuest and cancelLauncherGuestLaunch do once the caller
    // may ask it, and what Kadunce itself calls.
    void closeLauncherGuest();
    void dropLauncherGuestLaunch();
    void handleLaunchWindowChanged();
    // The display of the card stage named now (SessionScope).
    [[nodiscard]] bool isTabletOutput(const KWin::LogicalOutput *output) const;
    // Any attached display holding cards: every display, unless kaduncerc
    // [Spread] OtherDisplays keeps cards to the primary display alone.
    [[nodiscard]] bool isCardDisplay(const KWin::LogicalOutput *output) const;
    // The primary display (TouchDisplay.h cardOutputName): the one holding
    // cards when only one does, and where nothing else says where.
    [[nodiscard]] KWin::LogicalOutput *primaryOutput() const;
    void refreshCardOutput();
    // CARD-LIFECYCLE.md §3 on every display holding cards: one holding none
    // holds its windows as cards, the one in use (or `arrival`) Active.
    bool holdDisplaysInCards(KWin::EffectWindow *arrival);
    // Both answer for the desktop of the session being worked on.
    [[nodiscard]] bool isCardWindow(const KWin::EffectWindow *window) const;
    [[nodiscard]] bool isApplicationWindow(const KWin::EffectWindow *window) const;
    // CARD-LIFECYCLE.md §4: a dialog, or any other window that names a window
    // it belongs to, follows the application window at the root of that chain
    // and is never a card or a pane. Null for a window that belongs to none.
    static KWin::EffectWindow *dependentLead(const KWin::EffectWindow *window);
    // CARD-LIFECYCLE.md gives every virtual desktop its own ownership
    // session: its own cards and its own layouts, which carry on unseen while
    // another desktop is shown.
    struct DesktopSession;
    class SessionHost;
    class SessionScope;
    // CARD-LIFECYCLE.md §11: every display holding cards has a card stage of
    // its own on each desktop, with its own Spread, Active card and Stacks.
    // Its host names it, and its display, for each call it makes.
    struct DisplayStage;
    class StageHost;
    DesktopSession *sessionFor(KWin::VirtualDesktop *desktop);
    // The session's stage on the display `key` names, made the first time.
    CardStageController &stageOn(DesktopSession *session, const QString &key);
    // The session whose cards or layouts hold the window, if any does.
    [[nodiscard]] DesktopSession *sessionHolding(const KWin::EffectWindow *window) const;
    // The scoped desktop's stage holding the window as a card, if any does.
    [[nodiscard]] CardStageController *stageHolding(const KWin::EffectWindow *window) const;
    // The stage holding it as a card, else the one keeping its pane's Stack.
    [[nodiscard]] CardStageController *stageKeepingStackOf(const KWin::EffectWindow *window) const;
    // Whether any display of the scoped desktop shows its Spread.
    [[nodiscard]] bool anySpreadShown() const;
    // Whether any display's cards are on, on the desktop shown.
    [[nodiscard]] bool anyCardStageActive() const;
    // Whether Kadunce gives windows their rounded corners, as it does unless
    // another effect does or its shader could not be made.
    [[nodiscard]] bool roundsCorners() const;
    // The key a display's stage is kept under: its name while every display
    // holds its own cards, and one key for all while one display does.
    [[nodiscard]] QString stageKeyFor(const KWin::LogicalOutput *output) const;
    // The key of the stage holding the window, else of its display's.
    [[nodiscard]] QString stageKeyOf(const KWin::EffectWindow *window) const;
    void scopeTo(DesktopSession *session) { scopeTo(session, m_scopedKey); }
    void scopeTo(DesktopSession *session, const QString &key);
    // Runs f once with each session's stages named by m_cardStage and
    // m_desktopStage, for what reaches every desktop: a window closing,
    // a display going away, release. m_cardStage keeps the display named.
    template<typename F> void forEachSession(F &&f);
    // Runs f once with every desktop's stage on every display named.
    template<typename F> void forEachStage(F &&f);
    // Runs f once with the scoped desktop's stage on every display named.
    template<typename F> void forEachDisplayStage(F &&f);
    [[nodiscard]] bool scopedToCurrent() const { return m_scopedSession == m_currentSession; }
    // The display in use names the stage that keys, timers and anything else
    // naming no display act on, from the next turn.
    void noteDisplayInUse(const KWin::LogicalOutput *output);
    void restInUse();
    void handleDesktopChanged(KWin::VirtualDesktop *previous, KWin::VirtualDesktop *current);
    void handleDesktopRemoved(KWin::VirtualDesktop *desktop);
    // A window KWin moved to another desktop leaves the session it was owned
    // by, and the desktop it now stands on takes it as that desktop's own. One
    // put on every desktop leaves and stays a plain window.
    void handleWindowDesktopsChanged(KWin::EffectWindow *window);
    void toggleOwnedPresentation(bool growToActive = false);
    // KWin draws another desktop's windows only while showing it as well, as
    // its slide does between desktops. That desktop's cards stay hidden but for
    // the one it presents Active; its layouts' panes are real windows and show.
    [[nodiscard]] bool hiddenOnOtherDesktop(const KWin::EffectWindow *window) const;
    // While a desktop is previewed, the current desktop's own windows are not
    // drawn; what every desktop shares, the dock and the desktop itself, is.
    [[nodiscard]] bool hiddenByDesktopPreview(const KWin::EffectWindow *window) const;
    // KWin draws a desktop that is not current only for a window held visible,
    // so each window of the previewed desktop is held while it is shown.
    void syncDesktopPreview();
    void endDesktopPreview();
    QPointer<KWin::VirtualDesktop> m_previewDesktop;
    // The card a preview shows in front of its desktop, when Table's finger is
    // on one; otherwise the desktop's own front card.
    QPointer<KWin::EffectWindow> m_previewCard;
    QHash<KWin::EffectWindow *, KWin::EffectWindowVisibleRef> m_previewRefs;
    void showDesktopPreview(KWin::VirtualDesktop *desktop, KWin::EffectWindow *card);

    // Table (TABLE.md): every workspace as tabs under the top edge of the
    // display that holds cards, one stroke choosing a workspace or a card, and
    // a card carried to a tab or to +.
    struct TableCard {
        QList<QPointer<KWin::EffectWindow>> windows;
        QString application;
        QString title;
        QIcon icon;
        // A stack is one card, its face first in windows and the cards
        // behind it after; this counts those behind.
        int stacked = 0;
    };
    struct TableWorkspace {
        QPointer<KWin::VirtualDesktop> desktop;
        QString name;
        // The person named it, which keeps it.
        bool named = false;
        bool current = false;
        QList<TableCard> cards;
    };
    // Each workspace as Table shows it, renaming none.
    [[nodiscard]] QList<TableWorkspace> tableWorkspaces();
    // Reads Table's workspaces, and gives each unnamed one the name it shows.
    void readTableWorkspaces();
    // The display Table opened on: the tablet for a finger, the display under
    // the pointer for the pointer and the keyboard.
    [[nodiscard]] KWin::LogicalOutput *tableOutput() const;
    [[nodiscard]] QRectF tableDisplay() const;
    [[nodiscard]] qint64 tableNow() const;
    void openTable(bool sticky, KWin::LogicalOutput *output = nullptr);
    void closeTable();
    void finishTable();
    void relayoutTable();
    void refreshTable();
    void applyTableAction(const TableAction &action);
    void moveTableCard(int workspace, int card, KWin::VirtualDesktop *destination);
    [[nodiscard]] TableHit tableHitAt(const QPointF &local) const;
    [[nodiscard]] int tableCardAt(const QPointF &local) const;
    [[nodiscard]] bool tableOpenForInput() const override { return m_table.isOpen(); }
    [[nodiscard]] bool aboveActiveCardForInput(const QPointF &position) const override;
    [[nodiscard]] bool inCardGapForInput(const QPointF &position) const override { return inCardGap(position); }
    void beginTableFromInput(const QPointF &position) override;
    void pressTableFromInput(const QPointF &position, bool touch) override;
    void contextTableFromInput(const QPointF &position) override;
    void moveTableFromInput(const QPointF &position) override;
    void releaseTableFromInput(const QPointF &position) override;
    void cancelTableFromInput() override;
    void hoverTableFromInput(const QPointF &position) override;
    void wheelTableFromInput(int steps) override;
    QPointer<KWin::LogicalOutput> m_pointerCorner;
    void tracePointerAtTop(const QPointF &position);
    QString m_topPointer;
    QElapsedTimer m_topPointerLogged;
    TableGesture m_table;
    QList<TableWorkspace> m_tableWorkspaces;
    TableLayout m_tableLayout;
    std::unique_ptr<TablePresenter> m_tablePresenter;
    // Spread and Table said to a screen reader, made only while one listens.
    std::unique_ptr<SpokenLine> m_spokenLine;
    // What Table says now, kept as Table is drawn.
    QString m_tableLine;
    // The surface last said and its line, without the name it opened with.
    enum class SpokenSurface { None, Spread, Table };
    SpokenSurface m_spokenSurface = SpokenSurface::None;
    QString m_spokenText;
    [[nodiscard]] QString spreadLine();
    [[nodiscard]] QString tableLine() const;
    void syncSpokenLine();
    QElapsedTimer m_tableTime;
    // The card in hand as last drawn, and the landing of the last card moved.
    QVariantMap m_tableCarried;
    QVariantMap m_tableDrop;
    int m_tableDrops = 0;
    QPointer<KWin::LogicalOutput> m_tablePresenterOutput;
    // Where the finger that pulled Table down first touched.
    QPointF m_tableStrokeFrom;
    QPointer<KWin::LogicalOutput> m_tableOutput;
    // Table came down over Spread, which its tabs leave showing.
    bool m_tableOverSpread = false;
    bool m_tableKeyboard = false;
    // Three fingers up, or four on a touchpad, and whether this swipe has
    // already opened or closed Table.
    QAction *m_tableGestureAction = nullptr;
    QAction *m_tablePadGestureAction = nullptr;
    bool m_tableGestureDone = false;
    // A workspace with no cards dissolves unless the person named it
    // KDE keeps the name; which names are the
    // person's is Kadunce's, kept by desktop id in kaduncerc.
    QStringList m_namedDesktops;
    // Desktops something else made while Kadunce ran that have not yet been
    // shown. Leaving is what dissolves a workspace, so one never entered
    // waits: a script or another tool that makes a desktop and then sends a
    // window to it finds it still there.
    QSet<QString> m_unenteredDesktops;
    void loadNamedDesktops();
    void saveNamedDesktops();
    // The tab being renamed, what has been typed into it, and whether a
    // press landed while it was.
    int m_tableRenaming = -1;
    QString m_tableRenameText;
    bool m_tableRenamePress = false;
    // A finger began it, so the keys are up for it.
    bool m_tableRenameByTouch = false;
    // The application field that had the keys' text before the name took it.
    QPointer<KWin::SurfaceInterface> m_tableTextFocus;
    // A menu bar's press held still becomes a rename.
    QTimer m_tableHold;
    bool m_tablePressTouch = false;
    // Stacks carried by Table to a desktop, front to back, by desktop id:
    // their windows become cards when the desktop is next shown, one by one,
    // and then are stacked again as they left.
    QHash<QString, QList<QList<QPointer<KWin::EffectWindow>>>> m_pendingDecks;
    void restackCarriedDecks();
    void beginTableRename(int workspace, bool touch);
    void endTableRename(bool commit);
    void removeTableWorkspace(int workspace);
    void scheduleDissolve();
    void dissolveEmptyWorkspaces();
    bool m_dissolveQueued = false;
    static bool isDependentWindow(const KWin::EffectWindow *window);
    // Whether a dependent of lead may be seen: always, unless lead is a card
    // that is not the Active card presented in front.
    [[nodiscard]] bool dependentShown(const KWin::EffectWindow *lead) const;
    // A dependent whose application is not in front waits with it, hidden and
    // unfocused, and its application is marked as wanting attention.
    void syncDependentWindows();
    void scheduleDependentSync();
    void returnDependentWindows();
    // A window named the window it belongs to after it was shown, as
    // Electron's native boxes do on Wayland. Taken for a card or a pane
    // before then, it leaves them and waits with its application from now on.
    void handleTransientChanged();
    void handleWindowOutputChanged();
    KWin::LogicalOutput *tabletOutput() const;
    [[nodiscard]] bool isTabletOutputForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool usesDrawnZonesForDesktopStage() const override { return m_usesDrawnZones; }
    [[nodiscard]] bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] KWin::LogicalOutput *tabletOutputForDesktopStage()
        const override;
    [[nodiscard]] bool connectedOutputForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool holdsOwnCardsForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool showsOneCardForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] bool releasesOnOwnDisplayForDesktopStage(
        const KWin::LogicalOutput *output) const override;
    // CARD-LIFECYCLE.md §11: a display holding cards other than the one the
    // stage named now is kept for.
    [[nodiscard]] bool isOtherCardDisplay(const KWin::LogicalOutput *output) const;
    void settleStacksOfArrival();
    // The card stage a title-bar carry was prepared by; none once that carry
    // no longer holds.
    [[nodiscard]] CardStageController *carrySourceStage(const PreparedCarrySource &source) const;
    [[nodiscard]] bool cardCarrySourceValid(const PreparedCarrySource &source) const;
    // §11: the card goes, with its Stack, to the stage of another display
    // holding cards, shown there when `show`, else waiting in its Spread.
    bool moveCardToDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *to, bool show);
    // Runs f after `ms` with the stage named now named again, while its
    // desktop's session is still kept.
    void onThisStageLater(int ms, const std::function<void()> &f);
    // Shows a card sent to the stage named now there, once it stands there.
    void showMovedCard(KWin::EffectWindow *card, const QList<QPointer<KWin::EffectWindow>> &behind, int tries = 20);
    // A pane's Stack steps in place (CARD-LIFECYCLE.md §11).
    bool stepPane(KWin::EffectWindow *pane, int delta);
    void activateShownPane(KWin::EffectWindow *pane);
    // Cards the top edge took out of a layout, called back alone (§10).
    QList<QPointer<KWin::EffectWindow>> m_aloneOnPurpose;
    void fillLayoutWithCards(KWin::LogicalOutput *output);
    void appearInPlace(KWin::EffectWindow *window);
    void stepInUnseen(KWin::EffectWindow *window);
    [[nodiscard]] bool steppingInUnseen(const KWin::EffectWindow *window) const;
    bool restoreLeftLayout(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind);
    void rejoinStack(KWin::EffectWindow *front, const QList<KWin::EffectWindow *> &behind);
    // Keeps the cards behind `front` its Stack on the stage named now, as a
    // pane's or in the row, once it is either.
    void keepStackBehind(KWin::EffectWindow *front, const QList<QPointer<KWin::EffectWindow>> &behind, int tries);
    [[nodiscard]] bool justOpened(const KWin::EffectWindow *window) const;
    bool callPaneStackCard(KWin::EffectWindow *pane, KWin::EffectWindow *card);
    // §11, Connected: a carried pane let go on `onto` joins its Stack in
    // front, and the next card of its own Stack takes the slot it left.
    bool stackPaneOnto(KWin::EffectWindow *pane, KWin::EffectWindow *onto);
    // §11, Every display: the other panes of that display wait in Spread for
    // it, and this one is the card it shows alone.
    // §11, Connected: a pane's face held by its title bar and drawn up passes
    // the cards behind it, shown as a ladder of title bars, and goes in where
    // it is let go.
    bool followStackScrub(QPointF contact);
    bool stackScrubHeldStillGoesOn();
    void beginStackScrub(double y);
    double stackScrubHand(double pointerY) const;
    void endStackScrub();
    void refreshStackScrubShown();
    // Where the ladder draws `window`: the card's box, and the part of it
    // that shows.
    [[nodiscard]] std::optional<std::pair<QRectF, QRectF>> stackScrubRung(const KWin::EffectWindow *window) const;
    bool reorderPaneStack(KWin::EffectWindow *pane, int place);
    // INPUT.md § Connected, by the keys: Meta's arrows move the focus to the
    // window beside, on this display or the next; with Shift the pane goes
    // there, swapping with a pane beside it, into open room toward that edge,
    // or on to the display beside. Shift+Up puts it deeper in its Stack, shown
    // on the ladder until Meta is let go, or with no Stack joins the pane used
    // before it; Shift+Down takes it out of its Stack, or out of the layout.
    // The brackets move the divider beside it.
    void focusToward(KeyDirection direction);
    void carryToward(KeyDirection direction);
    void stepKeyLadder(int delta);
    void commitKeyLadder();
    void letPaneGo();
    void letActiveCardGo();
    void moveDividerByKey(int delta);
    void keyReleased(int key);
    // The windows the arrows reach: every display's panes, and the Active card.
    [[nodiscard]] QList<KWin::EffectWindow *> keyTargets() const;
    // The display beside `output` on that side, if any.
    [[nodiscard]] KWin::LogicalOutput *outputToward(const KWin::LogicalOutput *output,
                                                    KeyDirection direction) const;
    // The pane of the same display used most recently before `pane`.
    [[nodiscard]] KWin::EffectWindow *paneUsedBefore(const KWin::EffectWindow *pane) const;
    // The held card's place on the ladder of a Stack being scrubbed.
    [[nodiscard]] double stackLean();
    [[nodiscard]] double stackLeanNow() const;
    [[nodiscard]] QRectF leanedIntoStack(const QRectF &rect, double lean) const;
    void drawStackLadderOutline(const KWin::RenderTarget &renderTarget,
                            const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
                            KWin::LogicalOutput *screen);
    // A window that left another display loose on one holding cards becomes
    // a card there, as one opened there does. From names the display it
    // left, when known: a window still standing on it stays as it is.
    void claimWindowForItsDisplay(KWin::EffectWindow *window, const QString &from = {});
    // Whether another display holds its own cards, so where a carried Active
    // card lands decides what it becomes.
    [[nodiscard]] bool carriesToAnotherCardDisplay() const;
    // The edge of `output` a carried window's contact reaches. A panel along
    // the top edge, a status bar say, never takes that contact, so the top
    // edge is reached over it; a panel anywhere else keeps its place.
    [[nodiscard]] std::optional<CarryEdge> carryEdgeAt(const KWin::LogicalOutput *output,
                                                       QPointF contact) const;
    bool m_cardsOnEveryDisplay = true;
    [[nodiscard]] long long lastUsedForDesktopStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] bool outputCanOwnCards(
        const KWin::LogicalOutput *output) const override;
    [[nodiscard]] KWin::Rect activeTargetForDesktopStage(
        KWin::LogicalOutput *output) const override;
    void retireOutputFromDesktopStage(KWin::LogicalOutput *output) override;
    void prepareOutputForDesktopStage(
        KWin::LogicalOutput *output) override;
    [[nodiscard]] std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(
        KWin::EffectWindow *window) const override;
    // The doors a layout's windows take into cards; `output` names the
    // display whose stage takes them, and none the stage named now.
    bool admitDisplacedPaneToTablet(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override;
    bool admitSleepingPaneToTablet(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) override;
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
    [[nodiscard]] bool keyboardTypesIntoForCardStage(
        const KWin::EffectWindow *window) const override;
    [[nodiscard]] KWin::RectF workAreaForCardStage(
        const KWin::LogicalOutput *output) const override;
    void setPagingShortcutsForCardStage(bool active) override;
    void cancelInputForCardStage() override;
    void connectManagedWindowForCardStage(
        KWin::EffectWindow *window) override;
    void unredirectForCardStage(KWin::EffectWindow *window) override;
    [[nodiscard]] bool holdPictureBeforeResizeForCardStage(
        KWin::EffectWindow *window, const std::function<void()> &resize) override;
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
    // Three fingers down open Spread: under the fingers from the Active card,
    // or once past halfway from a layout or the desktop.
    struct SpreadGesture {
        enum class Mode { Idle, Refused, Follow, Commit, Opened };
        Mode mode = Mode::Idle;
        double progress = 0.0;
        // The stage of the display the fingers opened it on, once they have.
        std::optional<QString> key;
    };
    void setPagingShortcutsActive(bool active);
    // The stage key the keys act on (CARD-LIFECYCLE.md §11).
    [[nodiscard]] QString keyStageKey() const;
    bool answerKey(int key, Qt::KeyboardModifiers modifiers, bool repeat);
    void runKeyAction(KeyAction action);
    // KWin's gesture recogniser calls these; they are invokable, never on the
    // bus, so a private compositor's probe can drive them where its virtual
    // displays report no physical size for KWin to measure fingers by.
    Q_INVOKABLE void followSpreadGesture(qreal progress);
    // The same, on the display the fingers are on: the touchscreen's for
    // three fingers, the pointer's for four on a touchpad.
    void followSpreadGestureOn(qreal progress, KWin::LogicalOutput *output);
    [[nodiscard]] SpreadGesture::Mode beginSpreadGesture();
    Q_INVOKABLE void finishSpreadGesture();
    void holdOverviewOff();
    void followTableGesture(qreal progress, KWin::LogicalOutput *output);
    [[nodiscard]] WorkspacePresentation presentationForInput() const override;
    void settleChosenCardForInput() override;
    [[nodiscard]] WorkspaceInputGeometry geometryForInput() const override;
    [[nodiscard]] bool cardGrabActiveForInput() const override;
    [[nodiscard]] bool nativeWindowInteractionForInput() const override;
    [[nodiscard]] bool centerCardContainsForInput(
        const QPointF &position) const override;
    [[nodiscard]] bool launcherGuestActiveForInput() const override;
    [[nodiscard]] bool launcherGuestContainsForInput(
        const QPointF &position) const override;
    [[nodiscard]] bool isPanelPoint(const QPointF &position) const override;
    [[nodiscard]] bool topEdgeClaimedForInput(const QPointF &position) const override;
    [[nodiscard]] bool surfaceOwnsTouchAt(const QPointF &position) const override;
    [[nodiscard]] bool clientReceivesTouchAt(const QPointF &position) const override;
    [[nodiscard]] std::optional<QSet<qint32>> touchesDownForInput() const override {
        if (!m_touchWitness) return std::nullopt;
        return m_touchWitness->down();
    }
    void touchReachedRouterForInput(qint32 id) override {
        if (m_touchWitness) m_touchWitness->reachedRouter(id);
    }
    void touchRoutedForInput(qint32 id, bool kept) override {
        if (m_touchWitness) m_touchWitness->routed(id, kept);
    }
    void enterInputScope(const QPointF &position, InputReach reach, qint64 contact) override;
    void leaveInputScope(InputReach reach, qint64 contact) override;
    [[nodiscard]] QString describeTitleTouch(const QPointF &position) const;
    [[nodiscard]] bool inputPanelContainsForInput(const QPointF &position) const override;
    [[nodiscard]] QRectF nativeLandingAreaForOutput(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool cancelForwardedTouchForInput() override;
    [[nodiscard]] bool isTabletPoint(
        const QPointF &position) const override;
    void pointerMovedForInput(const QPointF &position) override;
    // The gutter around Kadunce's own cards and panes holds the pointer.
    void quietCardGap(const QPointF &position);
    void showHeldPointer(HeldPointer held, bool wasHeld);
    [[nodiscard]] bool inCardGap(const QPointF &position) const;
    void touchBeganForInput() override;
    // A terminal's field is enabled for as long as it has the focus and never
    // asks for the keys again, so a tap in it asks on its behalf.
    void tapEndedForInput(const KWin::Window *window) override;
    void raiseKeysForTerminalTap(QPointer<KWin::Window> window, quint64 touchEvents);
    bool m_gapHeld = false;
    HeldPointer m_heldPointer = HeldPointer::None;
    // A touch hides the pointer where it stands; it holds nothing until it moves.
    bool m_pointerResting = false;
    // kaduncerc's choice, so a monitor keeps it across sessions.
    bool m_usesDrawnZones = false;
    void switchZoneMode();
    [[nodiscard]] int activeSideForPoint(
        const QPointF &position) const override;
    [[nodiscard]] bool selectedStackContains(
        const QPointF &position) const override;
    void toggleFromInput() override;
    void beginBezelSpreadFromInput() override;
    void followBezelSpreadFromInput(double rise) override;
    void finishBezelSpreadFromInput(double rise, double speed, bool cancelled) override;
    void dismissLauncherGuestFromInput() override;
    void tapBesideLauncherGuestFromInput(
        const QPointF &position) override;
    void pageLeftFromInput() override;
    void pageRightFromInput() override;
    void pageStackFromInput(int delta) override;
    void pageHorizontal(int delta);
    void pageStack(int delta);
    void beginCardGrab(const QPointF &position) override;
    void updateCardGrab(const QPointF &position) override;
    void finishCardGrab(bool commit) override;
    [[nodiscard]] bool finishCardGrabOnOutput(
        const QPointF &position) override;
    [[nodiscard]] bool cardAtForInput(const QPointF &position) const override;
    void tapSpreadFromInput(const QPointF &position) override;
    // Gooseberry's notes on Spread's cards (TETTEGOUCHE-CONTEXT.md § Stuck
    // notes from Gooseberry): each placed note where it stands on its window,
    // the rest in a stack at the card's corner. A tap on either opens the
    // card with the notes out on the window, a tapped note ready to type; a
    // hold carries a note to another card.
    [[nodiscard]] bool pressNotesFromInput(const QPointF &position) override;
    void moveNotesFromInput(const QPointF &position) override;
    void holdNotesFromInput() override;
    void releaseNotesFromInput(const QPointF &position, bool still) override;
    void cancelNotesFromInput() override;
    // Notes are shown and answered only in Spread at rest on the card
    // display, with no guest in its centre and no card in hand.
    [[nodiscard]] bool notesShownInSpread() const;
    // Gooseberry's surface of notes over a window, live or closing.
    [[nodiscard]] bool isNotesSurface(const KWin::EffectWindow *window) const;
    // Tettegouche's Search surface, which holds the keys while it is open.
    [[nodiscard]] static bool isSearchSurface(const KWin::EffectWindow *window);
    [[nodiscard]] bool notesSurfaceStepsAside(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::EffectWindow *notesSurfaceHost(const KWin::EffectWindow *window) const;
    // Spread's cards as notes see them, nearest the eye first, where they
    // were last drawn.
    [[nodiscard]] QList<NotesCard> notesCards() const;
    [[nodiscard]] KWin::EffectWindow *notesCardWindow(const QUuid &id) const;
    void paintStuckNotes(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                         const KWin::Region &deviceRegion, KWin::LogicalOutput *screen,
                         const QRectF &heldRect, QList<QRectF> &drawn);
    StuckNotesWatcher *m_stuckNotesWatcher = nullptr;
    StuckNotesSpread m_stuckNotes;
    NoteStackRenderer m_noteStackRenderer;
    // Spread was shown on the last frame, and whether Gooseberry was last
    // asked to pause its notes over the windows for it.
    bool m_notesSpreadShown = false;
    bool m_notesPaused = false;
    void syncNotesPause();
    // As Spread closes onto an Active card whose notes are up, Spread keeps
    // drawing them over the card, where Gooseberry draws them, until
    // Gooseberry's own surface stands over the window, and keeps that surface
    // undrawn till then; at most NotesHandoverLimitMs.
    struct NotesHandover {
        QPointer<KWin::EffectWindow> card;
        QElapsedTimer since;
        // Its notes were asked up from Spread, so they come up even before
        // Gooseberry says they are.
        bool asked = false;
    };
    std::optional<NotesHandover> m_notesHandover;
    // The card whose notes a tap in this Spread asked up.
    QUuid m_notesAskedFor;
    static constexpr int NotesHandoverLimitMs = 800;
    void syncNotesHandover(bool spreadClosed);
    void paintNotesHandover(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
                            QList<QRectF> &drawn);
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
    void markAloneOnPurpose(KWin::EffectWindow *card);
    [[nodiscard]] bool callAloneAsItLeft(KWin::EffectWindow *window);
    [[nodiscard]] DisplaySizing sizingFor(const KWin::LogicalOutput *output) const;
    // The display's chosen sizing as kaduncerc keeps it; empty when none.
    [[nodiscard]] QString chosenSizingFor(const KWin::LogicalOutput *output) const;
    void writeSpreadChoice();
    // CARD-LIFECYCLE.md §11, Connected: a card picked from Spread is played
    // into the display's layout. False leaves the pick to the Active path.
    [[nodiscard]] bool playPickedCardIntoLayout(KWin::EffectWindow *picked);
    // §11, Connected: the Active card takes the place `side` names, alone.
    bool placeActiveCardAlone(KWin::LogicalOutput *output, BentoSidePlacement side);
    // A side snap of the Active card brings back the layout Spread holds,
    // with the card in the place the snap named.
    // A card let go out of its Stack on a Solo display joins the layout on
    // Spread's display beside it.
    void layOutHeldLayoutBeside(KWin::EffectWindow *card, KWin::LogicalOutput *output,
                                BentoSidePlacement side, const QRectF &drawn);
    // The two named cards become a Bento pair, `active` placed by `side`.
    bool pairCardsIntoBento(KWin::LogicalOutput *output, KWin::EffectWindow *active,
        KWin::EffectWindow *partner, BentoSidePlacement side);
    // §11, Connected: Meta+Tab on a pane steps the Stack that fills it.
    bool stepPaneStack(int delta);
    // §8: retire a layout that cannot take an arrival into a Spread group, so
    // the card the arrival becomes is not drawn over live panes. Both arrival
    // paths ask this before handing the window to Card Stage.
    [[nodiscard]] bool retireLayoutIntoSpreadGroup(KWin::LogicalOutput *output);
    // §11, Connected: a pane taken to the top edge out of three or more is
    // the card shown alone. The panes it left are held in Spread for it, as
    // the cards of a Solo display wait there, rather than standing live
    // behind it where any of them can rise over it. Spread can take them
    // once they have settled, so this asks again a few times.
    void holdLayoutLeftBehind(KWin::EffectWindow *card, KWin::LogicalOutput *output, int tries = 20);
    void handleMinimumSizeChanged();
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
    // A display plugged in, unplugged or moved. KWin moves windows between
    // displays as it happens; once it has, cards go back to the display that
    // holds them and windows it moved onto that display become cards.
    void scheduleCardDisplaySettle();
    void settleCardsOnDisplays();
    // The scoped display's own settle; true when a card was sent back to it.
    bool settleCardsOnThisDisplay();
    // §11: the cards of a stage no display holds any more go to the displays
    // their windows stand on, and empty stages of that kind are let go.
    void handOverGoneStages();
    void forgetGoneStages();
    // KWin rearranged windows for a display that came, went or moved, so a
    // window it moved between displays just now was not sent by the person.
    [[nodiscard]] bool displaysJustChanged() const
    {
        return m_displaysChanged.isValid() && m_displaysChanged.elapsed() < 1500;
    }
    void handleSessionStateChanged();
    [[nodiscard]] int liveCardIndex(const KWin::EffectWindow *window) const;
    [[nodiscard]] int visibleSlot(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    [[nodiscard]] KWin::Rect activeTarget(KWin::LogicalOutput *output) const;
    [[nodiscard]] bool hasActiveDesktopStage() const;
    void connectManagedWindow(KWin::EffectWindow *window);
    void handleManagedWindowMinimizedChanged();
    bool admitTransferredWindowToTablet(
        KWin::EffectWindow *window, KWin::LogicalOutput *output = nullptr,
        const std::function<bool()> &commitSource = [] { return true; },
        const NativeMoveSnapshot *restore = nullptr) override;

    std::unique_ptr<KeyRoute> m_keyRoute;
    // Cards are on the tablet, so Meta's arrows move through them.
    QHash<QString, bool> m_cardsShownForKeys;
    SpreadGesture m_spreadGesture;
    QAction *m_spreadGestureAction = nullptr;
    // KDE's Overview was set aside here and is loaded again when Kadunce stops.
    bool m_overviewHeld = false;
    void observeCardOwnership();
    // Reads the card book and reports each new mismatch once; never repairs.
    void observeCardBook();
    bool startTabletInCards(KWin::EffectWindow *arrival);
    // The authority for who owns a window. Both stages' containers are checked
    // against it; it is never repaired from them.
    CardOwnershipLedger m_ownership;
    std::vector<OwnershipViolation> m_observedOwnershipViolations;
    QStringList m_ownershipViolationLog;
    std::vector<CardBookMismatch> m_observedCardBook;
    // The book is read once a change has settled: a move publishes on its
    // way, before KWin has put each window where it goes.
    QTimer m_cardBookSettle;
    void readCardBook();
    QStringList m_cardBookLog;

    QAction *m_showSpreadAction = nullptr;
    QAction *m_showActiveAction = nullptr;
    bool m_usesDirectSystemEdges = true;
    std::unique_ptr<WorkspaceInputRouter> m_inputRouter;
    std::unique_ptr<NativeEdgePolicy<KWin::Options>> m_nativeEdgePolicy;
    std::unique_ptr<KeyboardOverlayPolicy<KWin::Options>> m_keyboardOverlayPolicy;
    std::unique_ptr<PerOutputDesktopsPolicy<KWin::Options>> m_perOutputDesktopsPolicy;
    // KDE's desktop-name pop-up is a KWin script, held unloaded while this
    // effect runs; KWin loads it again on every reconfigure. Unload lets KWin
    // load it again as the person's own settings say.
    void holdDesktopPopupOff();
    bool m_desktopPopupHeld = false;
    void giveTableCornerBack();
    bool m_tableCornerReserved = false;
    bool m_overviewCornerHeld = false;
    QMetaObject::Connection m_inputPanelGeometry;
    // The keys come up only when the person asks for them: a tap on the line
    // the text cursor sits on or, just after a tap made with the keys up,
    // anywhere in the window that asks, or a request through raiseKeyboard.
    // Anything else the compositor raises goes back down before it is drawn.
    QElapsedTimer m_keyboardAskedSince;
    // Whether the person's keys were on screen as the latest touch came down.
    // Only then is a field asking just after it a move from one field to the
    // next; with the keys down it may be the application's own focus.
    bool m_keysUpAtTouch = false;
    // Set once keys on screen are found asked for, and held until they go.
    // Until then they are not drawn, so keys raised for nobody are never seen
    // for the frame the compositor can paint before the decision reaches it.
    bool m_keysForPerson = false;
    // How many touch events there had been when keys the person had up last
    // began to go, unless a touch had only just ended. Those keys answered
    // every one of them.
    quint64 m_answeredTouchEvents = 0;
    void answerTouches();
    // Why keys the compositor raised are not the person's, or null when they
    // asked for them.
    [[nodiscard]] const char *keyboardRefusal(const KWin::InputMethod &method) const;
    void keepUnaskedKeyboardDown();
    // Keys the compositor hid while they still had a picture, drawn on until
    // the keyboard lets go of it, so a keyboard that slides out when typing
    // ends is seen doing so. Held no longer than a slide takes.
    std::unique_ptr<KWin::EffectWindowVisibleRef> m_leavingKeys;
    QPointer<KWin::EffectWindow> m_leavingPanel;
    QMetaObject::Connection m_leavingKeysUnmap;
    QTimer m_leavingKeysLimit;
    void releaseLeavingKeys();
    // Each display's work area as it stood when the keys came up, held while
    // they are on screen and until a panel that stepped aside for them takes
    // its room back, or a moment after they have gone if none does.
    QHash<const KWin::LogicalOutput *, KWin::RectF> m_keysWorkAreas;
    QTimer m_keysWorkAreaRelease;
    bool m_workAreaCheckQueued = false;
    void followKeysWorkArea();
    void releaseKeysWorkArea();
    std::unique_ptr<NativeCarryRuntime> m_carryRuntime;
    std::unique_ptr<TouchWitness> m_touchWitness;
    // Kadunce is giving windows back: one returning from minimized then was
    // not picked by the person.
    bool m_releasing = false;
    // §13: windows given back stay plain windows until a window opens or the
    // person asks for cards again; nothing takes them back on its own.
    bool m_letGo = false;
    // Sleeping cards Spread keeps drawn, dimmed.
    QHash<KWin::EffectWindow *, KWin::EffectWindowVisibleRef> m_sleepingShown;
    static constexpr qreal SleepingCardOpacity = 0.45;
    void showSleepingCardsInSpread();
    // Unredirects the neighbours the scoped Spread no longer prepares.
    void prepareNeighbors(KWin::LogicalOutput *screen);
    // The stage key of the display in use, and whether a display is it.
    [[nodiscard]] QString inUseKey() const;
    [[nodiscard]] bool isInUseDisplay(const KWin::LogicalOutput *output) const;
    QPointer<KWin::EffectWindow> m_carriedWindow;
    QRectF m_carryPickup;
    // Where the carried window is drawn under the hand, and whether its
    // release landed; one that did not glides back from there (§10 Cancel).
    QRectF m_carryDrawn;
    bool m_carryLanded = false;
    // Set while a cancel that finds no carry passes through the carry input:
    // motion already under way then carries on.
    bool m_motionOutlivesInterrupt = false;
    [[nodiscard]] QRectF carriedPose() const;
    // A carry let go as the Active card glides from the hand into its place.
    void glideIntoActive(KWin::EffectWindow *window, KWin::LogicalOutput *output, const QRectF &from);
    std::optional<DesktopStageController::PreparedDrop> m_carryDestination;
    std::optional<DesktopStageController::PreparedDrop> m_lineDestination;
    // A tablet edge action that admits to Card Stage has no Bento reservation
    // to hold: its destination is the Active card target on this output.
    // CARD-LIFECYCLE.md §3 and §10 decide which of the two a gesture is.
    QPointer<KWin::LogicalOutput> m_carryCardEntryOutput;
    // §10: a card carried to the bottom edge of its own display leaves for the
    // ordinary desktop, which Card Stage commits without a Bento reservation.
    QPointer<KWin::LogicalOutput> m_carryCardExitOutput;
    QPointer<KWin::LogicalOutput> m_lineCardEntryOutput;
    QPointer<KWin::EffectWindow> m_lineDestinationWindow;
    QPointF m_lineDestinationContact;
    DeferredCommandGuard m_inputActivationGuard;
    // Every desktop's session, by desktop id. The two stage pointers name the
    // session being worked on: the current desktop's, except inside a
    // SessionScope that reaches another desktop's.
    std::map<QString, std::unique_ptr<DesktopSession>> m_sessions;
    DesktopSession *m_currentSession = nullptr;
    DesktopSession *m_scopedSession = nullptr;
    // The display whose stage m_cardStage is, by stage key, and how many
    // SessionScopes are open.
    QString m_scopedKey;
    // Each routed event's display (WorkspaceInputTarget::enterInputScope),
    // and the contacts and buttons of the stroke under way with the display
    // its first one named.
    std::vector<std::unique_ptr<SessionScope>> m_inputScopes;
    QSet<qint64> m_strokeContacts;
    QString m_strokeKey;
    int m_scopeDepth = 0;
    QString m_inUseKey;
    DesktopStageController *m_desktopStage = nullptr;
    CardStageController *m_cardStage = nullptr;
    KWin::LogicalOutput *m_paintingOutput = nullptr;
    bool m_continueRepaint = false;
    // A quiet record of frames the card display waited for while Kadunce was
    // moving something, with what the frame before cost, so a stall seen on
    // hardware can be told apart from Kadunce's own drawing.
    QElapsedTimer m_frameClock;
    qint64 m_frameStartedAt = -1;
    qint64 m_frameWork = 0;
    bool m_frameWanted = false;
    bool m_framingTablet = false;
    // The presentation the workspace context last reported, -1 with cards
    // off, so a frame that changes it announces the context once.
    QString m_reportedPresentation;
    int m_framePreviewsMade = 0;
    void noteTabletFrameStart();
    void noteTabletFrameEnd(bool wanted);
    QList<QPointer<KWin::EffectWindow>> m_preparationNeighbors;
    bool m_neighborPreparedThisFrame = false;
    unsigned int m_neighborPreparationCursor = 0;
    int m_neighborPreparationFrames = 0;
    QPointer<KWin::EffectWindow> m_nativeCarry;
    QString m_nativeCarrySource;
    // The display a carry Kadunce draws picked its window up from.
    QString m_carryOrigin;
    // The Active card under a KWin move that has not yet gone anywhere, and
    // where it began. A tap an application reads as a move does not let the
    // cards go; travel does.
    QPointer<KWin::EffectWindow> m_heldActiveMove;
    QPointF m_heldActiveMoveOrigin;
    // A display's own card let go at its bottom edge is an ordinary window
    // there until it is carried again, never taken back as a card (§10).
    QPointer<KWin::EffectWindow> m_letGoOnDisplay;
    bool m_nativeCarryFromBento = false;
    std::unique_ptr<KWin::GLShader> m_fanApertureShader;
    std::unique_ptr<KWin::GLShader> m_destinationShader;
    std::optional<KWin::RectF> m_carryPreview;
    // §11, Connected: the pane a carried pane rests over, which it joins in
    // a Stack once it has rested there; the pane it would join, once it
    // would; and a reading of the aim when the dwell runs out, should the
    // carry be resting still.
    PaneStackAim<QPointer<KWin::EffectWindow>> m_paneStackAim;
    QPointer<KWin::EffectWindow> m_carryStackOnto;
    // How far the carried window leans into the Stack it would join, 0 to 1,
    // easing from where it stood when it last turned toward or away.
    QElapsedTimer m_stackLeanClock;
    double m_stackLeanFrom = 0.0;
    bool m_stackLeanTo = false;
    // The carried pane holds a Stack and its hold is not yet told from a drag.
    bool m_stackScrubArmed = false;
    // Held long enough to show the ladder, but not yet drawn up or aside.
    bool m_stackScrubHeldStill = false;
    bool m_stackScrubLetGo = false;
    // The ladder shows for Meta+Shift+Up, and is placed when Meta is let go.
    bool m_stackScrubByKeys = false;
    StackScrub m_stackScrub;
    QPointer<KWin::EffectWindow> m_stackScrubPane;
    QHash<KWin::EffectWindow *, KWin::EffectWindowVisibleRef> m_stackScrubShown;
    QElapsedTimer m_paneStackClock;
    QTimer m_paneStackDwell;
    QPointF m_lastCarryContact;
    // §11, Connected: the width a wheel turned while holding the carried
    // window at the top or a side edge sets, a share of the display; none
    // until it turns, and forgotten when the edge changes.
    enum class CarryWidthEdge { None, Top, Side };
    CarryWidthEdge m_carryWidthEdge = CarryWidthEdge::None;
    double m_carryWidth = 0.0;
    WheelWidthKnob m_carryKnob;
    QElapsedTimer m_carryWidthClock;
    bool turnCarryWidth(int notches);
    DesktopExitLabel m_detachLabel;
    // A refused gesture springs back and says why (GestureNote.h). The refusal
    // is decided while the carry moves, never invented at release, and shown
    // only when letting go commits nothing.
    struct CarryRefusal {
        QPointer<KWin::LogicalOutput> output;
        QPointF contact;
        GestureRefusal reason = GestureRefusal::Unplaced;
    };
    std::optional<CarryRefusal> m_carryRefusal;
    struct ShownGestureNote {
        QPointer<KWin::LogicalOutput> output;
        QRectF box;
        QString text;
        QElapsedTimer shown;
    };
    std::optional<ShownGestureNote> m_gestureNote;
    GestureNoteLabel m_gestureNoteLabel;
    void showGestureNote(KWin::LogicalOutput *output, QPointF contact, GestureRefusal reason);
    std::optional<KWin::RectF> m_linePreview;
    // `onCards` lets a settle run on the card display, where it waits for the
    // client to draw the shape it settles into (PaneArrival.h) and lasts only
    // while the card display presents the window there.
    void startDropSettle(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                         const QRectF &from, const QRectF &to, bool onCards = false);
    [[nodiscard]] bool cardDisplayPresents(KWin::EffectWindow *window) const;
    void clearDropSettle();
    // §10 and §2: a pane returned to the card display's desktop is shown on
    // it, and the rest of what Kadunce holds there goes aside into Spread.
    void showDesktopWithReturned(KWin::LogicalOutput *tablet, KWin::EffectWindow *returned);
    [[nodiscard]] std::optional<QRectF> dropSettleRect() const;
    QPointer<KWin::EffectWindow> m_settlingWindow;
    QPointer<KWin::LogicalOutput> m_settlingOutput;
    QRectF m_settleFrom, m_settleTo, m_settleOutputGeometry;
    QElapsedTimer m_dropSettleTimer;
    bool m_settleOnCards = false;
    bool m_settleDrawnMoving = false;
    QElapsedTimer m_settleAsked;
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
    struct TopEdgeClaim {
        QString owner;
        QString output;
        QRectF area;
    };
    // Keyed by the caller's bus name and its own key.
    QHash<QString, TopEdgeClaim> m_topEdgeClaims;
    struct ContextLineClaim {
        QString owner;
        QString output;
    };
    QHash<QString, ContextLineClaim> m_contextLineClaims;
    // One watcher for both kinds of claim: a caller leaving drops all of them.
    QPointer<QDBusServiceWatcher> m_topEdgeClaimWatcher;
    void watchClaimOwner(const QString &owner);
    [[nodiscard]] bool contextLineClaimed(const KWin::LogicalOutput *output) const;
    // What Kadunce would say on a claimed display, for the workspace context:
    // a refused gesture, the zones taken, or a card about to leave for the
    // desktop. Spread's chosen entry is read from Card Stage as it is.
    [[nodiscard]] QJsonValue contextNotice() const;
    [[nodiscard]] QJsonValue spreadEntryContext() const;
    // The scoped display's own stage as the workspace context reports it.
    [[nodiscard]] QJsonObject cardStageContext() const;
    // Calls f for each display holding cards, its stage named meanwhile.
    void eachCardDisplay(const std::function<void(KWin::LogicalOutput *)> &f) const;
    // What those say, cheaply, so a frame that changes them announces it once.
    [[nodiscard]] QString contextLineSignature() const;
    QString m_reportedContextLine;
    struct ZoneNotice {
        QString text;
        QElapsedTimer shown;
    };
    std::optional<ZoneNotice> m_zoneNotice;
    // A notice no frame will clear, since nothing is drawn for it, ends here.
    QTimer m_contextNoticeEnd;
    // The display a card about to leave for the desktop is over, this frame.
    QPointer<KWin::LogicalOutput> m_detachShownOn;
    QString m_launcherGuestOwner;
    QString m_launcherGuestPath;
    QString m_launcherGuestInterface;
    bool m_launcherGuestExpanded = false;
    QElapsedTimer m_guestNeighborMotion;
    double m_guestNeighborFrom = 1.0;
    double guestNeighborOpacity() const;
    KWin::Rect launcherGuestExpandedTarget(KWin::LogicalOutput *output) const;
    // The card KWin will hand focus back to as a guest gives up the keyboard,
    // and the application card that last held focus.
    QPointer<KWin::EffectWindow> m_guestFocusReturn;
    quint64 m_guestFocusReturnGeneration = 0;
    QPointer<KWin::EffectWindow> m_lastActiveApplication;
    // The keys coming back from Gooseberry's notes as Spread covers them.
    NotesKeysReturn m_notesKeysReturn;
    // The keys coming back from Search as it closes over Spread, told the
    // same way, whether or not Spread took Search in as its guest.
    NotesKeysReturn m_searchKeysReturn;
    QString m_cardOutputName;
    // The display a touchscreen drives, which alone takes Kadunce's own edges.
    QString m_touchOutputName;
    SpreadDisplayChoice m_spreadChoice;
    // kaduncerc's [Sizing] group: each display's chosen sizing, by name, and
    // [SizingDisplays]: the lasting identity each of those names had.
    QHash<QString, QString> m_sizingChoices;
    QHash<QString, QString> m_sizingIdentities;
    QList<QPointer<KWin::EffectWindow>> m_dependents;
    QList<QPointer<KWin::EffectWindow>> m_heldDependents;
    QList<QPointer<KWin::EffectWindow>> m_freshDependents;
    QList<QPointer<KWin::Window>> m_waitingLeads;
    bool m_dependentSyncQueued = false;
    bool m_cardDisplaySettleQueued = false;
    bool m_cardDisplayMoved = false;
    QElapsedTimer m_displaysChanged;
    int m_cardDisplaySettleRounds = 0;
    bool m_holdingDependents = false;
    // Waiting dialogs drawn on their cards in Spread. KWin keeps them hidden,
    // so each is held paintable for as long as it is drawn.
    std::map<KWin::EffectWindow *, std::unique_ptr<KWin::EffectWindowVisibleRef>> m_drawnDialogs;
    void updateDrawnDialogs();
    [[nodiscard]] QList<KWin::EffectWindow *> drawnDialogsOf(const KWin::EffectWindow *lead) const;
    // Draws dialogs as their application was just drawn: the same scale and
    // turn about the same point, so each stands where it stands over it.
    bool paintDialogsOn(const KWin::RenderTarget &renderTarget,
                        const KWin::RenderViewport &viewport, KWin::EffectWindow *lead,
                        const QList<KWin::EffectWindow *> &dialogs, const KWin::Region &clip,
                        const KWin::WindowPaintData &leadData);
    // The dialogs of a card in hand. Each keeps its place on the application
    // as a share of its frame, so it lands where it stood on it, and is drawn
    // with the card while the card is drawn anywhere but its own frame.
    struct CarriedDialog {
        QPointer<KWin::EffectWindow> dialog;
        QPointF place;
    };
    QPointer<KWin::EffectWindow> m_dialogsLead;
    QList<CarriedDialog> m_carriedDialogs;
    QList<QMetaObject::Connection> m_carriedDialogWatch;
    QTimer m_carriedDialogsRelease;
    bool m_placingCarriedDialogs = false;
    void takeCarriedDialogs(KWin::EffectWindow *lead);
    // Where a dialog put at `to` over its application stands wholly on that
    // application's display: over a pane narrower than itself it overhangs
    // the pane, never the screen.
    [[nodiscard]] static QPointF onLeadDisplay(const KWin::RectF &lead, const QSizeF &size, QPointF to);
    void placeCarriedDialogs();
    void releaseCarriedDialogs();
    [[nodiscard]] QList<KWin::EffectWindow *> carriedDialogs() const;
    [[nodiscard]] bool drawnWithCarriedCard(const KWin::EffectWindow *window) const;
    [[nodiscard]] QList<PlacementOutput> placementOutputs() const;
    [[nodiscard]] PlacementAim placementAimAt(QPointF point) const;
    // The place a side-edge contact names on `output`: on Connected a
    // quarter, a half and a quarter of the edge (BentoSidePlacement.h),
    // elsewhere its upper or lower half.
    [[nodiscard]] BentoSidePlacement sideChoiceOn(const KWin::LogicalOutput *output, bool right,
        double y, std::optional<BentoSidePlacement> previous = std::nullopt) const;
    [[nodiscard]] std::optional<KWin::RectF> placementPreview(const PlacementAim &aim, QPointF point) const;
    bool placeWindow(KWin::EffectWindow *window, const PlacementAim &aim, QPointF point);
    bool placeCardOnDisplay(KWin::EffectWindow *card, KWin::LogicalOutput *output,
        const PlacementAim &aim, QPointF point);
    // The destination a card placed by request was reserved, while it is handed
    // over; admitCardToDesktopStage reads it as it reads a carry's.
    std::optional<DesktopStageController::PreparedDrop> m_placementDrop;
    void completePlacementForWindow(KWin::EffectWindow *window);
    void drawDestinationOutline(const KWin::RenderTarget &renderTarget,
                                const KWin::RenderViewport &viewport, const KWin::Region &deviceRegion,
                                KWin::LogicalOutput *screen, const QRectF &box,
                                const QRectF &hole = {});
    static constexpr int PlacementLaunchTimeoutMs = 10000;
    std::optional<PlacementAim> m_placementAim;
    std::optional<KWin::RectF> m_placementPreview;
    QPointer<KWin::LogicalOutput> m_placementPreviewOutput;
    std::optional<PendingLaunch> m_placementLaunch;
    PlacementAim m_placementLaunchAim;
    QPointF m_placementLaunchPoint;
    quint64 m_placementGeneration = 0;
    bool m_launcherGuestLaunchPending = false;
    QStringList m_launcherGuestLaunchApps;
    QString m_launcherGuestLaunchToken;
    quint64 m_guestGeneration = 0;
    QHash<QString, quint64> m_activationOrder;
    quint64 m_activationSequence = 0;
};

} // namespace Kadunce

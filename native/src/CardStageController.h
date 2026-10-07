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
#include "RowTravel.h"
#include "CarryRow.h"

#include <effect/effectwindow.h>

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <functional>
#include <optional>

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
    // CARD-LIFECYCLE.md §2: the ordinary desktop is shown, with the window the
    // person returned to it. Card Stage still owns its cards and holds them
    // aside, out of sight and out of reach, until Spread opens or one is chosen.
    Desktop,
};

// Marks a card held aside while the desktop is shown. KWin keeps the window
// hidden, and this effect still knows it as a card. Effect data roles below
// 1000 are KWin's.
inline constexpr int CardAsideRole = 1000 + 0x4b41;

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
    // CARD-LIFECYCLE.md §7 leaves a sleeping window owned as an individual
    // card, so eligibility for one cannot require being awake. The predicate
    // above answers what this stage can present; this one answers what it can
    // hold, awake or asleep. A host that draws no distinction may answer the
    // same question.
    [[nodiscard]] virtual bool mayHoldWindowForCardStage(
        const KWin::EffectWindow *window) const {
        return isManagedWindowForCardStage(window);
    }
    // Where the visible virtual keyboard's top edge sits on this display, if
    // one is up. A keyboard is a compositor fact and reaches every effect, so
    // reading it here costs no dependency on a downstream surface. A host with
    // nothing to report answers that nothing is up.
    [[nodiscard]] virtual std::optional<double> inputPanelTopForCardStage(
        KWin::LogicalOutput *) const {
        return std::nullopt;
    }
    // The area cards are laid out in on this display. A panel that gives its
    // room up while the keys are up lends it to them, not to the cards, so the
    // host answers the area as it stood before they came until the panel takes
    // the room back. A host with no keyboard answers the compositor's.
    [[nodiscard]] virtual KWin::RectF workAreaForCardStage(
        const KWin::LogicalOutput *output) const;
    // Whether the keyboard is typing into this window or a dialog of its own.
    // A host with no keyboard answers that it is not.
    [[nodiscard]] virtual bool keyboardTypesIntoForCardStage(
        const KWin::EffectWindow *) const {
        return false;
    }
    // The stage is about to give this card focus by its own gesture, not by
    // a touch inside the card.
    virtual void setPagingShortcutsForCardStage(bool active) = 0;
    virtual void cancelInputForCardStage() = 0;
    // A card whose app asked a question when it was flicked closed is
    // selected; open it so the question can be answered. A host that cannot
    // open a card leaves it selected in Spread.
    virtual void presentSelectedForCardStage() {}
    // A card let go on a pane of the Bento group took that pane; open the
    // group as its layout, the card growing from `from`, where it was drawn.
    virtual void openGroupAfterDropForCardStage(const QRectF &) {}
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
    // CARD-LIFECYCLE.md §10: a card carried to its display's bottom edge
    // returns to the ordinary desktop at `geometry`. The desktop is then shown
    // with it, and any other cards are held aside, one swipe away in Spread;
    // with none left the display's session ends.
    bool releaseNativeCarryToDesktop(const PreparedCarrySource &source,
        KWin::LogicalOutput *output, const KWin::RectF &geometry);

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
    // The stack holding window, front to back from its face; empty for a card
    // alone.
    [[nodiscard]] QList<KWin::EffectWindow *> stackFrontToBack(const KWin::EffectWindow *window) const;
    // Stacks cards that stand alone behind face, as a stack carried here from
    // another desktop left it. Not an ownership transition.
    bool stackBehind(KWin::EffectWindow *face, const QList<KWin::EffectWindow *> &behind);
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
    // The keyboard, its size or text focus changed: while the keys type into
    // the Active card, its bottom edge follows their top, and the card has its
    // height back when they go.
    void refreshKeyboardRoom();
    // The keys said where they are going: their top edge comes to rest at
    // `top`, in `durationMs`.
    void keyboardHeading(double top, int durationMs);
    // Where the Active card is drawn to end while the keys type into it: a
    // gutter above them, whatever size its client has yet.
    [[nodiscard]] std::optional<double> keyboardRoomEdge(
        const KWin::EffectWindow *window) const;
    // The keys were shown or hidden. KWin puts the window it typed into back
    // where it stood when the keys opened, even with its own lift declined,
    // and keys that closed and opened again while the card was short leave
    // it that short place. The card is put where its room has it in the same
    // turn, before KWin's size reaches the client.
    void keepKeyboardRoomPlacement();
    // The area cards are laid out in may have changed: a panel took its room,
    // gave it up or took it back. The Active card is placed in it again. KWin
    // moves only a window touching the old edge, and a card stands a gutter
    // inside it.
    void followWorkArea();
    // The keys moved from `from` to `to`: the band the card's edge crossed is
    // drawn again in the same frame.
    void repaintKeyboardEdge(const KWin::RectF &from, const KWin::RectF &to) const;
    // A card is held in its own Stack, which it stays over.
    [[nodiscard]] bool heldInStack() const;
    // Where a card held in its own Stack would go, drawn as an outline at the
    // Stack's seam, and how far it has been lifted out, from nothing to all
    // the way; empty when no card is held in its Stack.
    [[nodiscard]] KWin::Rect heldStackSeam() const;
    // The place a Stack's card shows: where a held one would go, else its own.
    [[nodiscard]] int shownStackPosition(int cardId) const;
    // What a held card would do if let go, for the probe: "gap", "over",
    // "join", or "place" in its own Stack, or nothing before it is carried;
    // the gap's place or the entry under it, counted along the row without
    // the held card, or its place in its Stack; and how far the row has
    // stepped back.
    // The outline of a held Stack card's place, and how strongly it shows:
    // while held, and fading for a moment after it is let go.
    struct StackOutline {
        KWin::Rect rect;
        double opacity = 0.0;
    };
    [[nodiscard]] StackOutline stackOutline() const;
    [[nodiscard]] QString carryAimName() const;
    [[nodiscard]] int carryAimIndex() const;
    // The pane of the Bento group a held card would take, by window id.
    [[nodiscard]] QString carryAimPane() const;
    // How far the pane of the Bento group a held card rests on has given way
    // to a cutout, from 0 to 1; every other window, 0.
    [[nodiscard]] double carryPaneRecess(const KWin::EffectWindow *window) const;
    // Where a held card resting on a pane is drawn as it slides under the
    // group (HeldTuck.h): how far, its tucked rect and tilt, and the cutout
    // and group it slides under.
    struct HeldTuck {
        double progress = 0.0;
        KWin::Rect tucked;
        double rotation = 0.0;
        KWin::Rect pane;
        KWin::Rect group;
    };
    [[nodiscard]] std::optional<HeldTuck> heldTuck() const;
    [[nodiscard]] double carryScale() const;
    [[nodiscard]] bool animationsRunning() const;
    [[nodiscard]] bool launcherGuestActive() const;
    [[nodiscard]] double launcherGuestOffset() const;
    [[nodiscard]] double launcherGuestTransitionProgress() const;

    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot) const;
    // The same, as drawn with the card `selectedId` selected.
    [[nodiscard]] KWin::Rect cardTargetForSlot(
        KWin::LogicalOutput *output, int slot, int selectedId) const;
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

    // A card chosen by touch or key grows into Active; other callers enter at once.
    void toggle(bool growToActive = false);
    // Ends a chosen card's growth in Active at once; true when there was one.
    bool finishGrowToActive();
    void release();
    void pageHorizontal(int delta);
    void pageStack(int delta);

    // The row as something a hand moves. Travel is the finger's distance since
    // its stroke locked, positive rightward or downward; a velocity is the
    // finger's, in pixels per millisecond. The row has two ends.
    [[nodiscard]] bool rowMoving() const { return m_row.moving(); }
    [[nodiscard]] bool rowStill() const { return m_row.still(); }
    // A touch landed on a row moving fast: it stops where it is. Returns
    // whether it did, so the touch is not also a tap.
    bool catchRow();
    // A stroke that caught the row and did not move it lets it settle.
    void settleRowFromStroke();
    bool beginRowDrag();
    void updateRowDrag(double travel);
    void finishRowDrag(double velocity);
    // A slow sideways stroke on the centred Stack brings its cards forward in
    // turn, keeping the fan where it is.
    bool beginScrub();
    void updateScrub(double travel);
    void finishScrub();
    // Up closes the app under the finger; down takes its card out of a Stack.
    bool beginLift(const QPointF &position);
    void updateLift(double travel);
    void finishLift(double velocity);
    // Input was taken away mid-stroke: the row and any lifted card go home.
    void cancelStroke();
    // Advance a released row, a returning card or a thrown one by a frame.
    void advanceMotion();
    [[nodiscard]] double liftOpacity(const KWin::EffectWindow *window) const;
    // A card flicked away stays out of sight while its app closes, and while
    // it closes, so nothing is drawn at the window's own size.
    [[nodiscard]] bool thrownAway(const KWin::EffectWindow *window) const;
    [[nodiscard]] bool askedInside(const KWin::EffectWindow *window) const;
    [[nodiscard]] bool beginLauncherGuest();
    void updateLauncherGuest(double horizontalDelta);
    [[nodiscard]] bool finishLauncherGuest(double horizontalDelta);
    void endLauncherGuest();
    // A tap beside Search ends it. On a card standing beside it, that card's
    // entry is selected to open, as a tap on it in Spread would select it.
    [[nodiscard]] bool endLauncherGuestOnCard(const QPointF &position);

    // A hold picks up the card under the finger, wherever it stands in the
    // row; let go still, it goes back as it was. Carried, the row parts where
    // it would land; leaned toward a side, the row slides under it closed up
    // and, when long, stepped back to show whole.
    void beginCardGrab(const QPointF &position);
    void updateCardGrab(const QPointF &position);
    void updateCardGrabDestination(const QPointF &position);
    void finishCardGrab(bool commit);
    [[nodiscard]] bool finishCardGrabOnOutput(const QPointF &position);
    // Whether a card in Spread is under `position`.
    [[nodiscard]] bool cardAtPoint(const QPointF &position) const;
    // A tap in Spread. A card fanned behind the centred Stack's face comes
    // forward; any other card is selected to open; empty space selects where
    // the person was, to go back to it.
    enum class SpreadTap { None, Forward, Open };
    SpreadTap tapSpread(const QPointF &position);
    // Escape in Spread: back to where the person was, as a tap on empty space.
    bool backFromSpread();
    // Three fingers open Spread from the Active card under the fingers: the
    // card shrinks into the row and its neighbours slide in as `progress` goes
    // from 0 to 1. Let go past halfway it finishes; short of it, it goes back.
    bool beginOpenSpread();
    void followOpenSpread(double progress);
    void finishOpenSpread(bool open);
    // How far Spread stands open under the fingers, 1 when it is not opening.
    [[nodiscard]] double spreadOpenProgress() const;
    // Spread is forming under the fingers, or going back short of halfway.
    [[nodiscard]] bool spreadOpening() const { return m_openProgress.has_value(); }

    void syncSelectedElevation();
    // §2 Desktop: the desktop is shown and every card is held aside.
    void showDesktop();
    // §10: `window`, not a card, was returned to the desktop of this stage's
    // display while the stage holds cards, so the desktop is shown with it.
    void returnWindowToDesktop(KWin::EffectWindow *window);
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
    // §7: the user minimized a pane, so it leaves Bento at once and becomes a
    // sleeping individual card. Neither door above can take it. The transfer
    // door presents it, and the displaced-pane door is for an awake card
    // hidden behind live panes and asks this stage to be presenting them; a
    // sleeping card is behind everything by being asleep, so it arrives in
    // whatever presentation the display already has. It is never woken,
    // selected, given geometry, or offered a pane again until the user wakes
    // it, which §7 makes an ordinary card waking to Active.
    bool admitSleepingPaneAsCard(KWin::EffectWindow *window,
        const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore);
    // Whether this stage is presenting the display's Bento layout rather than
    // its own cards, so a caller can route an activation to the layout.
    [[nodiscard]] bool presentsBento() const {
        return m_active && m_presentation == CardPresentation::Bento;
    }
    // §10: a carried window with nothing to pair with becomes the Active card.
    bool promoteToActive(KWin::EffectWindow *window);
    // A display was plugged in or out, and KWin moves windows between
    // displays when that happens, cards included: it evacuates a removed
    // display and puts windows back where they last stood on a layout it has
    // seen before. §14 hands a card back to the desktop only by release or
    // disable, so a card KWin moved off this display comes back to it, and the
    // card and its window never name different displays. Returns how many.
    int returnCardsToDisplay();
    // The Active card gutter changed in the settings: every card on this
    // display stands where the Active card does, so each takes the new one now.
    int applyGutter();
    // §3: a window KWin moved onto this display because the display it stood
    // on went away arrives as one card. It is admitted unselected, so the
    // caller decides which arrival, if any, is presented.
    bool admitArrivalAsCard(KWin::EffectWindow *window);
    // A stage owning nothing on a display presenting its layout starts behind
    // it (§2), so what arrives becomes cards there; one that took nothing
    // stops again.
    bool startBehindLayout();
    void stopBehindLayoutIfEmpty();
    // The display's Bento layout ended, so this stage cannot still be
    // presenting one. §12: what this stage owns returns to Spread.
    void leaveBentoPresentation();
    [[nodiscard]] bool handleWindowAdded(KWin::EffectWindow *window);
    void stageWindowArrival(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    // The window left this stage's virtual desktop by other means, such as
    // KWin's window menu. It stops being a card here and goes back to the
    // place its record holds, as a release does; its new desktop takes it
    // from there. The rest of the stage is handled as for a closed card.
    bool releaseCard(KWin::EffectWindow *window);
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
    // Every awake card stands in the Active card's place; each keeps its own
    // in its restore record.
    void settleCardsInActivePlace(const QList<QPointer<KWin::EffectWindow>> &cards);
    // A card behind the Active one that its application maximized, or that
    // otherwise reaches past the Active place, goes back to that place.
    void holdCardInActivePlace(KWin::EffectWindow *window);
    void retainManagedOwnership(KWin::EffectWindow *window);
    void captureCardTransition(bool includeGuest = false, bool includeGrab = false);
    void clearCardTransition();
    void startArrivalTimer(KWin::EffectWindow *window);
    bool growSelectedToActive();
    bool m_growingChosen = false;
    void finishNewArrival(KWin::EffectWindow *window, bool animateArrival, int previousSelection);
    bool enterActive();
    void restoreActiveSnapshot();
    void parkActiveSnapshot();
    [[nodiscard]] KWin::Rect activePlacement(KWin::LogicalOutput *output) const;
    [[nodiscard]] KWin::RectF workArea(const KWin::LogicalOutput *output) const
    {
        return m_host->workAreaForCardStage(output);
    }
    [[nodiscard]] KWin::Rect restingPreviewTarget(
        KWin::LogicalOutput *output, const KWin::EffectWindow *window) const;
    // The two-ended row: which side a two-entry row draws its neighbour on,
    // where every entry stops, and which entries a moving row draws.
    [[nodiscard]] int rowNeighborSide() const;
    [[nodiscard]] bool rowMotionApplies() const;
    [[nodiscard]] RowStops rowStops(KWin::LogicalOutput *output) const;
    [[nodiscard]] int rowSlotInMotion(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::Rect pairTargetInMotion(KWin::LogicalOutput *output,
                                                const KWin::EffectWindow *window,
                                                const KWin::Rect &resting) const;
    [[nodiscard]] int entryOffset(const KWin::EffectWindow *window) const;
    [[nodiscard]] KWin::EffectWindow *cardAt(KWin::LogicalOutput *output,
                                             const QPointF &position) const;
    // The row under a held card, in the tablet's logical pixels.
    struct CarryFrame {
        double centreX = 0.0;
        double centreY = 0.0;
        double pitch = 0.0;
        double width = 0.0;
        double height = 0.0;
        double screenLeft = 0.0;
        double screenWidth = 0.0;
    };
    [[nodiscard]] std::optional<CarryFrame> carryFrame(KWin::LogicalOutput *output) const;
    // A window's item in the row without the held card, or -1.
    [[nodiscard]] int carryItemOf(const KWin::EffectWindow *window) const;
    // Where item k stands along the row, gap and all.
    [[nodiscard]] double carryItemPosition(int k, double pitch) const;
    // The row coordinate drawn at screen x `x`.
    [[nodiscard]] double carryRowAt(double x, const CarryFrame &frame) const;
    // Sideways travel pages a card held in its own Stack through its places.
    void pageHeldStackCard(double fingerX);
    [[nodiscard]] KWin::Rect carryTarget(KWin::LogicalOutput *output,
                                         const KWin::EffectWindow *window) const;
    void advanceCarry(double seconds);
    // How long the card transition under way runs, in milliseconds.
    [[nodiscard]] int transitionDuration() const;
    // A motion's duration at Plasma's animation speed (MotionTime.h).
    [[nodiscard]] static int motion(int base);
    [[nodiscard]] static bool motionIsInstant();
    void aimCarry();
    // Put a held card back as it was picked up: in its Stack, with the Stack's
    // face and the row's selection as they were.
    void restoreCarryOrigin();
    bool selectReturnEntry();
    void stopOpeningSpread();
    // Where the Bento group card draws its pane `pane`.
    [[nodiscard]] std::optional<CardRect> groupPaneDrawn(KWin::LogicalOutput *output,
                                                         KWin::EffectWindow *pane) const;
    // The Bento group's pane under `position`, or the nearest to it.
    [[nodiscard]] KWin::EffectWindow *groupPaneAt(KWin::LogicalOutput *output,
                                                  const QPointF &position) const;
    // §5: a pane pulled down out of the Bento group leaves it, and a group
    // down to one pane ends, so on the tablet every member becomes a card.
    bool pullPaneOutOfGroup(KWin::EffectWindow *pane);
    // The Bento group's pane `part`, in the order the group stores them.
    [[nodiscard]] KWin::EffectWindow *groupPane(int part) const;
    // Whether the held card may take a pane of the Bento group at all.
    [[nodiscard]] bool groupTakesHeldCard() const;
    // Whether the Bento group's pane `part` is large enough for the held card.
    [[nodiscard]] bool groupPaneHolds(int part) const;
    // The pane under `position` the held card would take, or -1 when that
    // pane is too small for it; the pane `kept`, named before, stays while
    // the finger is near it (carryGroupPart).
    [[nodiscard]] int groupPartFor(const QPointF &position, int kept) const;
    // §5: a card let go on a pane of the Bento group in Spread takes that
    // pane's place, and the pane becomes a card just after the group.
    bool replaceGroupPane(int part);
    void browseStack(int delta);
    void commitRowStop();
    void dropRowMotion();
    void throwLifted(double velocity);
    void pullOutLifted();
    void endLift();
    void checkThrownCards();
    void returnThrownCard(KWin::EffectWindow *window, bool present);
    void forgetCloseAsk(const KWin::EffectWindow *window);
    void updateKeyboardRoom(bool resting = false);
    [[nodiscard]] KWin::Window *keyboardRoomClient() const;
    [[nodiscard]] double keyboardRoomFor(const KWin::Window *client, double keyboardTop) const;
    void askKeyboardRoom(KWin::Window *client, double height);
    void putBackKeyboardRoom();
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
    // While a transition runs, a card that left the drawn row slides out to
    // where it now stands rather than vanishing.
    bool m_slideLeavers = false;
    RowMotion m_row;
    double m_rowDragOrigin = 0.0;
    QElapsedTimer m_motionClock;
    bool m_scrubbing = false;
    int m_scrubApplied = 0;
    struct Lift {
        enum class Phase { None, Drag, Return, Throw };
        QPointer<KWin::EffectWindow> window;
        double y = 0.0;
        double velocity = 0.0; // pixels per second
        bool inStack = false;
        // A pane of the Bento group: it comes out by a pull down, and a flick
        // up closes nothing.
        bool inGroup = false;
        Phase phase = Phase::None;
    };
    Lift m_lift;
    struct Thrown {
        QPointer<KWin::EffectWindow> window;
        QElapsedTimer since;
    };
    QList<Thrown> m_thrown;
    QTimer m_thrownTimer;
    // A flicked card's app, since it was asked to close and since it first drew
    // after that. One asking inside its own window draws its question; one
    // closing is gone before it has stayed long enough for a drawing to count.
    struct CloseAsk {
        QPointer<KWin::EffectWindow> window;
        QElapsedTimer asked;
        QElapsedTimer drew;
        QMetaObject::Connection damage;
    };
    QList<CloseAsk> m_closeAsks;
    // A card just closed in Spread; KWin's next activation is its hand-off.
    bool m_closeHandoff = false;
    QTimer m_arrivalTimer;
    QElapsedTimer m_arrivalWait;
    QPointer<KWin::EffectWindow> m_arrivalWindow;
    bool m_arrivalExpanding = false;
    QList<QPointer<KWin::EffectWindow>> m_originalCardStackingOrder;
    QList<QPointer<KWin::EffectWindow>> m_bentoProjectionWindows;
    QHash<const KWin::EffectWindow *, QElapsedTimer> m_heldInPlace;
    QList<QPointer<KWin::EffectWindow>> m_bentoProjectionPaneWindows;
    std::optional<BentoProjectionSession> m_bentoProjectionSession;
    ActiveRestoreSnapshot m_activeRestore;
    QPointer<KWin::EffectWindow> m_presentedActive;
    QList<ActiveRestoreSnapshot> m_parkedRestores;
    std::vector<std::unique_ptr<RestoredMinimization>> m_restoredMinimizations;
    bool m_applyingWindowState = false;
    QTimer m_activeSettleTimer;
    int m_activeSettleRemaining = 0;
    // The Active card's own placement while a keyboard is up, and the height it
    // stands at above the keys. The placement is where the card was when the
    // keyboard arrived, not one read from the work area, because the bottom
    // panels yield to the keyboard and the area grows while the room does not.
    struct KeyboardRoom {
        QPointer<KWin::EffectWindow> window;
        KWin::RectF base;
        double height = 0.0;
    };
    std::optional<KeyboardRoom> m_keyboardRoom;
    QTimer m_keyboardRoomTimer;
    QTimer m_keyboardRoomRelease;
    // The keys are taken to rest once they have not moved for a moment, and
    // where they said they were going is kept until they do.
    QTimer m_keyboardRestTimer;
    QTimer m_keyboardHeadingTimer;
    std::optional<double> m_keyboardHeadingTop;
    QElapsedTimer m_keyboardHeadingSince;
    int m_keyboardHeadingDuration = 0;
    bool m_keyboardHeadingPending = false;
    CardPresentation m_presentation = CardPresentation::Spread;
    QPointF m_cardGrabOffset;
    QPointF m_cardGrabStart;
    KWin::Rect m_cardGrabTarget;
    // Where a card that took a pane of the Bento group was drawn as it was
    // let go, kept until the grab has ended and the group can open from it.
    std::optional<QRectF> m_openGroupFrom;
    double m_cardGrabRotation = 0.0;
    QElapsedTimer m_cardGrabScaleTimer;
    QPointF m_cardGrabPointer;
    QString m_cardGrabDestinationOutput;
    bool m_cardGrabActive = false;
    // The row under a held card; CarryRow.h says how it moves.
    struct Carry {
        // What stands along the row without the held card: an entry, or, in
        // the Stack a card is being reordered in, one card of it.
        struct Item {
            int entry = -1;
            int cardId = 0;
        };
        std::vector<Item> items;
        // Each item's move aside for the gap, easing toward its place.
        std::vector<double> shifts;
        // The gap stays between these items: the whole row.
        int low = 0;
        int high = 0;
        // Held in its own Stack, the card stays over the Stack: `stack` is
        // its cards from front to back, and `paging` where the held one would
        // go, counted from the front.
        bool inStack = false;
        std::vector<int> stack;
        CarryStackPaging paging;
        double lift = 0.0;
        double position = 0.0;
        double scale = 1.0;
        // How wide the held card's place stands open, in pitches.
        double gapWidth = 1.0;
        double heldWidth = 0.0;
        CarryLean lean;
        // How far the finger leans the row, nothing in the middle.
        double depth = 0.0;
        // Pulled down, the row stands at one set zoom; it glides there from
        // `zoomFrom`, and back.
        bool zoomed = false;
        double zoomFrom = 1.0;
        QElapsedTimer zoomSince;
        // Come to rest over item `settle`, the row settles it under where the
        // held card's centre stood then, `settleX`.
        int settle = -1;
        double settleX = 0.0;
        // The card a held one rests on rises; it settles as it is left. Over
        // a Bento group only the pane `risePart` rises.
        double rise = 0.0;
        int riseIndex = -1;
        int risePart = -1;
        // Where on the card the finger holds it, as fractions of its size.
        QPointF grip;
        int heldIndex = 0;
        // The gap where the card was picked up, and where it is now.
        int home = 0;
        int gap = 0;
        QPointer<KWin::EffectWindow> selectedBefore;
        QPointer<KWin::EffectWindow> faceBefore;
        CarryAim aim;
        CarryAim candidate;
        QElapsedTimer candidateSince;
        // Sliding, the row stands closed up.
        bool sliding = false;
        bool moved = false;
        bool animating = false;
    };
    Carry m_carry;
    // Spread opening under three fingers, and let go short of halfway, its
    // way back to the Active card from `m_openReturnFrom`.
    std::optional<double> m_openProgress;
    double m_openReturnFrom = 0.0;
    QElapsedTimer m_openReturn;
    QTimer m_openReturnTimer;
    // A carried card let go lands in one move from the scale the row stood at.
    bool m_landTransition = false;
    double m_landScale = 1.0;
    QPointer<KWin::EffectWindow> m_landWindow;
    // A Stack parting for, paging under and closing after a card held in it
    // glides a little slower than other card motion.
    bool m_stackStepTransition = false;
    // The outline of a held Stack card's place fades out once it is let go.
    KWin::Rect m_stackOutlineRect;
    double m_stackOutlineFrom = 0.0;
    QElapsedTimer m_stackOutlineFade;
    // Spread was opened from the Bento layout, so going back resumes it.
    bool m_returnToGroup = false;
    // Spread was opened from the desktop, so going back returns there.
    bool m_returnToDesktop = false;
    // Cards held aside while the desktop is shown, and the windows returned
    // to it; activating one of those brings the desktop back.
    QList<QPointer<KWin::EffectWindow>> m_aside;
    QList<QPointer<KWin::EffectWindow>> m_returnedToDesktop;
    void holdCardsAside();
    void bringCardsBack();
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

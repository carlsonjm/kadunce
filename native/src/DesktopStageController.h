/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "BentoLayout.h"
#include "BentoSidePlacement.h"
#include "BentoProjectionSession.h"
#include "CardOwnership.h"
#include "DeferredCommandGuard.h"
#include "PreparedCarrySource.h"
#include "RestoredMinimization.h"

#include <effect/effectwindow.h>

#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <vector>
#include <functional>

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
    // Whether a display without cards takes the zones drawn for it with
    // Meta+T, or fills itself as though none were drawn.
    [[nodiscard]] virtual bool usesDrawnZonesForDesktopStage() const { return false; }
    [[nodiscard]] virtual bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const = 0;
    [[nodiscard]] virtual KWin::LogicalOutput *tabletOutputForDesktopStage()
        const = 0;
    // Whether this display can hold individual cards at all. CARD-LIFECYCLE.md
    // §5 ends Bento into one, so the rule needs the capability rather than the
    // display's hardware identity. CARD-LIFECYCLE.md §11 gives every display
    // its own cards; only with `[Spread] OtherDisplays=windows` does a display
    // other than Spread's keep ordinary windows or per-output Bento instead.
    [[nodiscard]] virtual bool outputCanOwnCards(const KWin::LogicalOutput *) const {
        return false;
    }
    // Whether this display is sized Connected
    // (DisplaySizing.h), so a full layout there gives up the window used
    // longest ago. A host that draws no distinction answers that it is not.
    [[nodiscard]] virtual bool connectedOutputForDesktopStage(const KWin::LogicalOutput *) const {
        return false;
    }
    // CARD-LIFECYCLE.md §11, Every display: a display other than Spread's
    // that holds its own cards. Every window there is a card: those it shows
    // are its layout's panes, and the rest wait in Spread. Its layout keeps its
    // last pane, and a card carried there joins it rather than standing loose.
    // A host that draws no distinction answers that none does.
    [[nodiscard]] virtual bool holdsOwnCardsForDesktopStage(const KWin::LogicalOutput *) const {
        return false;
    }
    // §11, Every display: a layout let go there gives its windows back on
    // that display, whatever display their records name.
    [[nodiscard]] virtual bool releasesOnOwnDisplayForDesktopStage(const KWin::LogicalOutput *) const {
        return false;
    }
    // Whether such a display is sized Solo, so it shows one card at a time.
    [[nodiscard]] virtual bool showsOneCardForDesktopStage(const KWin::LogicalOutput *) const {
        return false;
    }
    // When the person last used this window, later uses larger; 0 for never.
    [[nodiscard]] virtual long long lastUsedForDesktopStage(const KWin::EffectWindow *) const {
        return 0;
    }
    [[nodiscard]] virtual KWin::Rect activeTargetForDesktopStage(
        KWin::LogicalOutput *output) const = 0;
    virtual void prepareOutputForDesktopStage(
        KWin::LogicalOutput *output) = 0;
    // The display no longer has a Bento layout. Whatever else owns windows on
    // it can no longer be presenting one behind these panes.
    virtual void retireOutputFromDesktopStage(KWin::LogicalOutput *) {}
    virtual QRectF bentoPresentationRect(KWin::EffectWindow *window) const {
        return window && !window->isMinimized() ? QRectF(window->frameGeometry()) : QRectF{};
    }
    virtual void animateBentoLayout(KWin::LogicalOutput *,
        const QList<QPointer<KWin::EffectWindow>> &, const QList<QRectF> &, const QList<QRectF> &) {}
    // A pane gave way to an arrival and became a card hidden behind the
    // layout; `drawn` is where it was drawn on `output` until then.
    virtual void paneYieldedForDesktopStage(KWin::EffectWindow *, KWin::LogicalOutput *, const QRectF &) {}
    // The layout fell to one pane and ended into card ownership; `drawn` is
    // where that pane was drawn on `output` until then.
    virtual void lastPaneLeftLayoutForDesktopStage(KWin::EffectWindow *, KWin::LogicalOutput *,
                                                   const QRectF &) {}
    // The layout's last pane left `output` for another display.
    virtual void layoutLeftForOtherDisplayForDesktopStage(KWin::LogicalOutput *) {}
    // §5: an arrival that made a pane yield has left the layout on `output`,
    // and the card that pane became may take its place again.
    virtual void displacedCardMayReturnForDesktopStage(KWin::EffectWindow *, KWin::LogicalOutput *) {}
    [[nodiscard]] virtual std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(
        KWin::EffectWindow *) const { return std::nullopt; }
    // CARD-LIFECYCLE.md §5: a displaced pane becomes a nonselected individual
    // card. While the display presents its layout that card is owned and
    // hidden, so it must not arrive through the path that makes a transferred
    // window the Active card. A host that cannot tell the difference may
    // forward this to `admitTransferredWindowToTablet`. Each of these doors
    // names the display whose cards take the window.
    virtual bool admitDisplacedPaneToTablet(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) {
        return admitTransferredWindowToTablet(window, output, commitSource, restore);
    }
    // CARD-LIFECYCLE.md §7: the user minimized a pane, so it leaves Bento at
    // once and becomes a sleeping individual card. It is neither presented nor
    // selected, and it is never offered a pane again until the user wakes it.
    // A host whose card ownership cannot hold a sleeping window answers false,
    // and §5 then keeps the window where it is rather than shedding it to
    // nobody.
    virtual bool admitSleepingPaneToTablet(
        KWin::EffectWindow *, KWin::LogicalOutput *, const std::function<bool()> &,
        const NativeMoveSnapshot * = nullptr) {
        return false;
    }
    // `restore` is the record the window had while a Bento session still held
    // it. CARD-LIFECYCLE.md §5 keeps that record so release still returns the
    // window where it began, and after the session has published its shortened
    // plan the host can no longer find it for itself.
    virtual bool admitTransferredWindowToTablet(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const std::function<bool()> &commitSource, const NativeMoveSnapshot *restore = nullptr) = 0;
};

class DesktopStageController final
{
public:
    explicit DesktopStageController(DesktopStageHost *host);
    [[nodiscard]] std::optional<PreparedCarrySource> prepareNativeCarrySource(KWin::EffectWindow *window) const;
    [[nodiscard]] bool nativeCarrySourceValid(const PreparedCarrySource &source) const;
    [[nodiscard]] QString nativeCarryRefusal(KWin::EffectWindow *window) const;

    [[nodiscard]] bool hasActiveSession() const;
    [[nodiscard]] bool ownsWindow(KWin::EffectWindow *window) const;
    // A session's visible pane combination, which CARD-LIFECYCLE.md §5 makes
    // everything it owns awake. The two differ only where §5 could not shed a
    // §7 sleeping window, which is a display with no card owner to shed it to.
    [[nodiscard]] bool managesWindow(KWin::EffectWindow *window) const;
    // The display whose layout holds `window` as a pane; empty when none does.
    // It is the layout's record, so it is right while KWin still moves the
    // window there.
    [[nodiscard]] QString outputHoldingPane(const KWin::EffectWindow *window) const;
    // A pane KWin sent to another display some other way than a drag, such as
    // its window-to-screen shortcut, leaves its layout as a dragged one does.
    // False when `window` is no pane of another display's layout.
    bool handoffPaneToOutput(KWin::EffectWindow *window, KWin::LogicalOutput *destination);
    // A pane drawn smaller than its place, as a window still kept within the
    // bounds of the display it came from is, is asked for its place again.
    bool replacePaneShortOfPlace(KWin::EffectWindow *window);
    [[nodiscard]] bool hasSessionOnOutput(const QString &outputName) const;
    // A layout on the display laid out in the zones drawn in KDE's tile editor.
    [[nodiscard]] bool laysOutInZones(const QString &outputName) const;
    // A layout on the display whose panes have taken their places, as
    // Spread needs before it can hold the layout.
    [[nodiscard]] bool hasSettledSessionOnOutput(const QString &outputName) const;
    // Between a layout's panes, inside the box they fill together: the gutter a
    // press there must not reach whatever window lies behind the layout.
    [[nodiscard]] bool inPaneGutter(QPointF position) const;
    // CARD-LIFECYCLE.md §11, Connected: the display's layout was placed by a
    // side snap, so it keeps its places and can leave room open. Columns a
    // pair began (BentoColumns.h) leave none.
    [[nodiscard]] bool presentsPlacedLayoutOn(const QString &outputName) const;
    // The room a card arriving would fill in that layout on `output`, in
    // pixels, where the desktop shows; none when it leaves none.
    [[nodiscard]] std::optional<QRectF> openRoomOn(KWin::LogicalOutput *output) const;
    void restoreAllSessions();
    // Every layout placed again where it stands, as after the space between
    // panes changes. A layout being placed, or a pane in a person's hands,
    // waits for its own next placement.
    void relayAllSessions();
    // §11, Every display: a display holding its own cards begins its layout
    // with every window there, `preferred` shown first. False when it already
    // has one or has nothing to show.
    bool holdOutput(KWin::LogicalOutput *output, KWin::EffectWindow *preferred = nullptr);
    // The panes a display shows, in layout order; empty with no layout there.
    [[nodiscard]] QList<QPointer<KWin::EffectWindow>> panesOn(const QString &outputName) const;
    // §5: a layout down to one pane ends into card ownership where its display
    // holds Spread, as after Spread moves onto a display that showed one card.
    void endLonePanes();
    // Synchronous ownership transfer; no restoration or placement on success.
    bool transferTabletSessionToSpread(KWin::LogicalOutput *output,
        const std::function<bool(const BentoProjectionSession &,
                                 const std::function<bool()> &)> &accept);
    bool resumeProjectedSession(const BentoProjectionSession &projection,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource);
    // CARD-LIFECYCLE.md §5 and §10: the carried pane leaves Bento for card
    // ownership in one published step, and §5's one-remaining-pane rule then
    // ends a layout with nothing left to compose. `sourceValid` is the carry's
    // own last refusal; a false answer publishes nothing. `waitBehind` keeps
    // the pane a hidden card behind a layout that outlasts it (§8: nothing is
    // shown on top of live panes); otherwise it is presented.
    bool extractPaneToCards(KWin::EffectWindow *window,
        const std::function<bool()> &sourceValid, bool waitBehind = false);
    // §11, Every display: a pane of a display holding its own cards taken out
    // of what it shows waits in Spread, still that display's card.
    bool yieldPaneToSpread(KWin::EffectWindow *window);
    void stopPendingSettle();
    void cancelRestoredMinimizations();

    // Read-only §14 observation. Reduces each live session to the identities
    // that decide ownership; it never exposes or mutates session state.
    [[nodiscard]] std::vector<BentoOwnershipView> ownershipView() const;

    [[nodiscard]] QStringList outputStageState() const;
    bool toggleOnOutput(const QString &outputName);
    struct GrabRail { QString output; int index; bool vertical; QRectF pill; QRectF hitArea; };
    QList<GrabRail> grabRails() const;
    // Why the layout on `output` draws no dividers, for the workspace
    // context; empty when nothing withholds them.
    [[nodiscard]] QString dividersWithheld(KWin::LogicalOutput *output) const;
    bool beginRail(QPointF position);
    void updateRail(QPointF position);
    void finishRail(bool commit);
    // The host's choice between drawn zones and filling changed: every layout
    // on a display without cards is laid out again under it.
    void zoneModeChanged();
    QList<QRectF> railPreview(const QString &output) const;
    bool handoffLeadToOutput(const QString &sourceName,
                             const QString &destinationName);

    // Returns true when an existing session grew to show the new window as a
    // pane. False leaves it for card ownership; nothing is ever parked.
    bool handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
    // The window left this stage's virtual desktop by other means. Its layout
    // lets it go back to where it stood before the layout, as §13 would, and
    // reflows or ends as for a closed pane.
    void releaseWindow(KWin::EffectWindow *window);
    void handleWindowMinimizedChanged(KWin::EffectWindow *window);
    void handleScreenRemoved(KWin::LogicalOutput *output);
    void handleScreenAdded(KWin::LogicalOutput *output) { m_retiredOutputs.removeAll(output); }
    void handleWindowMoveResizeStarted(KWin::EffectWindow *window);
    void handleWindowMoveResizeStepped(KWin::EffectWindow *window,
                                       const KWin::RectF &geometry);
    void handleWindowMoveResizeFinished(KWin::EffectWindow *window);

    // Card Stage has already committed removal before asking Desktop Stage to
    // accept this physical client. False means leave it as an ordinary window.
    [[nodiscard]] bool admitCardWindow(KWin::EffectWindow *window,
                                       KWin::LogicalOutput *output,
                                       const KWin::RectF &geometry);
    // CARD-LIFECYCLE.md §8: a card the user calls forward on a display that is
    // presenting its layout joins that layout. `commitSource` is card
    // ownership giving the card up, and it is asked only after this session has
    // proved on a value copy that it can show the card. With `growOnly` a full
    // layout refuses the card rather than give it another pane's slot; without
    // `takesFocus` the window in use stays in use.
    [[nodiscard]] bool admitCardToLiveBento(KWin::EffectWindow *window,
        const std::function<bool()> &commitSource, bool growOnly = false, bool takesFocus = true);
    // CARD-LIFECYCLE.md §11, Connected: a Stack fills one pane, and stepping
    // it puts the next card in that pane's slot. Nothing else moves; the pane
    // leaving becomes a card through the same path as any yielded pane; a
    // closing pane only gives up its slot, so its Stack stays in the layout.
    [[nodiscard]] bool replacePaneWithCard(KWin::EffectWindow *pane, KWin::EffectWindow *card,
                                           const std::function<bool()> &commitSource,
                                           bool closing = false);
    // §11, Connected: a pane let go on another pane joins that pane's Stack in
    // front. It takes the other's slot, and the other becomes a card behind
    // it. The slot it left goes to `next`, the next card of its own Stack,
    // whom `commitSource` gives up; with none, its room is given up as a
    // closing pane's is.
    [[nodiscard]] bool stackPaneOnto(KWin::EffectWindow *pane, KWin::EffectWindow *onto,
                                     KWin::EffectWindow *next,
                                     const std::function<bool()> &commitSource);
    // The pane of `output`'s layout under `position`, other than `except`.
    [[nodiscard]] KWin::EffectWindow *paneAt(const KWin::LogicalOutput *output, QPointF position,
                                             const KWin::EffectWindow *except = nullptr) const;

    enum class CardDropIntent { OpenSpace, ActivateBento, NativeDesktop };
    // Opaque receiver reservation. Copies share consumption: a preview cannot
    // be replayed, including when source commitment is rejected.
    class PreparedDrop {
    public:
        std::optional<BentoSidePlacement> sidePlacement() const { return side; }
        KWin::EffectWindow *namedPartner() const { return pairPartner.data(); }
        KWin::EffectWindow *card() const { return window.data(); }
        KWin::LogicalOutput *destinationOutput() const { return output.data(); }
        bool detachesToDesktop() const { return intent == CardDropIntent::NativeDesktop && leavingBento; }
        bool showsPlacementOutline() const { return intent != CardDropIntent::NativeDesktop || leavingBento; }
        bool placesAlone() const { return alone; }
    private:
        friend class DesktopStageController;
        QPointer<KWin::EffectWindow> window;
        QPointer<KWin::LogicalOutput> output;
        KWin::RectF geometry;
        KWin::Rect outputGeometry;
        KWin::Rect area;
        quint64 generation = 0;
        std::weak_ptr<const int> owner;
        std::shared_ptr<bool> consumed = std::make_shared<bool>(false);
        CardDropIntent intent = CardDropIntent::OpenSpace;
        std::optional<BentoSidePlacement> side;
        bool hadSession = false;
        bool leavingBento = false;
        // §11, Connected: the card is placed at its side alone, with nothing
        // else on the display an input to the solve.
        bool alone = false;
        QPointer<KWin::EffectWindow> localTarget;
        // CARD-LIFECYCLE.md §3: a side snap that pairs names the one card it
        // pairs with, so solving, preview and revalidation all see exactly two
        // windows instead of everything the display happens to own.
        QPointer<KWin::EffectWindow> pairPartner;
        QList<QPointer<KWin::EffectWindow>> residents;
        QList<QSizeF> minimumSizes;
        QList<KWin::RectF> sourceGeometries;
    };
    [[nodiscard]] std::optional<PreparedDrop> prepareCardDrop(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, CardDropIntent intent = CardDropIntent::OpenSpace,
        std::optional<BentoSidePlacement> side = {},
        KWin::EffectWindow *pairPartner = nullptr, bool alone = false) const;
    [[nodiscard]] bool cardDropValid(const PreparedDrop &drop) const;
    [[nodiscard]] std::optional<PreparedDrop> prepareLocalCardDrop(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, QPointF contact) const;
    // Read-only layout solve: no placement, source removal or application token.
    [[nodiscard]] std::optional<KWin::RectF> cardDropPreview(const PreparedDrop &drop);
    // §3: Bento begins by pairing the carried window with the Active card. The
    // source gives both up in `commitSource`, after the pair layout is proven
    // and before it is published.
    bool activatePreparedTabletDrop(const PreparedDrop &drop,
        const NativeMoveSnapshot *restore,
        const std::function<bool()> &commitSource);
    bool transferNativeCarryToDesktop(const PreparedCarrySource &source,
                                     const PreparedDrop &drop);
    bool transferPreparedCard(const PreparedDrop &drop,
        const std::function<bool()> &commitSource,
        const std::function<void()> &releaseSource);
    // Two panes of one display's layout trade places, as one let go on the
    // other at once does.
    bool swapPanes(KWin::EffectWindow *window, KWin::EffectWindow *other);
    // CARD-LIFECYCLE.md §11, Connected: a pane already in its display's
    // layout is placed by `side` as a side snap would place it.
    bool placePaneBySide(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                         BentoSidePlacement side);
    // ActivateBento is an explicit, already-validated placement request, not
    // inferred from coordinates. Input/preview adoption is a separate boundary.
    // Synchronous callbacks only: commitSource mutates only source model state;
    // releaseSource runs native/visual cleanup after both owners are published.
    bool transferCardWindow(KWin::EffectWindow *window, KWin::LogicalOutput *output,
                           const KWin::RectF &geometry, const std::function<bool()> &commitSource,
                           const std::function<void()> &releaseSource,
                           CardDropIntent intent = CardDropIntent::OpenSpace,
                           const NativeMoveSnapshot *restore = nullptr,
                           std::optional<BentoSidePlacement> side = {});

private:
    // A layout's restore record: the place, and how the window sleeps.
    struct RestoreSnapshot : WindowPlace {
        QPointer<KWin::EffectWindow> window;
        bool valid = false;
        bool userMinimized = false;
        // Minimized by Kadunce because a layout on a display that cannot own
        // cards had no room for it. It waits in the dock, owned, and comes back
        // as an arrival; release gives it back unminimized.
        bool parked = false;
    };

    struct Session {
        QPointer<KWin::EffectWindow> sideWindow;
        std::optional<BentoSidePlacement> side;
        QString outputName;
        QList<QPointer<KWin::EffectWindow>> windows;
        QList<RestoreSnapshot> snapshots;
        std::vector<BentoRect> rects;
        // On a display with zones drawn in KDE's tile editor, KWin's zone for
        // each pane, in the order of windows; empty under the curated library.
        // KWin then owns each pane's geometry and the edges between them.
        QList<QPointer<QObject>> zones;
        bool applying = false;
        bool participationDirty = false;
        // One window snapped to a side of a display without cards takes half
        // of it, until a second window joins (DECISIONS.md § A display without
        // cards organizes everything it shows).
        bool lone = false;
        // CARD-LIFECYCLE.md §11, Connected: the columns side snaps placed. A
        // pane leaving opens its room, an arrival fills open room, and one
        // with no room to fill lays the whole layout out again.
        std::optional<BentoPlacedColumns> placed;
        // §11, Connected: the card a side snap placed last, and the side it
        // was snapped to. It outlasts the snap: a full layout never gives it
        // up to an arrival, and laid out again past minimum sizes it keeps
        // that side. A Stack stepping in its pane keeps both; it is forgotten
        // once nothing stands in that place.
        QPointer<KWin::EffectWindow> snappedWindow;
        std::optional<BentoSidePlacement> snappedSide;
        // This plan places its one window at `side` alone.
        bool placeAlone = false;
        quint64 applicationToken = 0;
    };
    // Each pane's rectangle as it stands now: KWin's zones where the session
    // has them, the curated layout otherwise.
    [[nodiscard]] std::vector<BentoPixelRect> sessionPixels(const Session &session,
                                                            const KWin::Rect &area) const;
    // Zones KDE moved a pane into by hand become the pane's zone, and a pane
    // that took another's zone swaps with it.
    void syncZonesFromKde(Session &session) const;
    // Kadunce's placements in KWin's zones, so a pane leaving a layout leaves
    // its zone too.
    QList<QPointer<KWin::Window>> m_zoned;
    void releaseStrayZones();
    struct RailDrag {
        GrabRail rail;
        Session original;
        Session preview;
        quint64 generation;
        KWin::Rect area;
        QPointF contact;
    };
    std::optional<RailDrag> m_railDrag;
    // Displays whose layout a departure asked to end while a pane was moved
    // or a divider held; settleAfterInteraction asks again.
    QSet<QString> m_heldDepartures;
    bool m_railRevealed = false;
    bool railValid() const;

    // `unadopted` reports the display's own windows a first layout cannot show.
    // They are still Native, so nothing is taken from them; CARD-LIFECYCLE.md §5
    // has the caller give them to card ownership once the layout is published.
    // A preview passes nullptr and ignores them.
    [[nodiscard]] std::optional<Session> prepareCardAdmission(
        KWin::EffectWindow *window, KWin::LogicalOutput *output,
        const KWin::RectF &geometry, const NativeMoveSnapshot *restore = nullptr,
        std::optional<BentoSidePlacement> side = {},
        KWin::EffectWindow *pairPartner = nullptr,
        QList<QPointer<KWin::EffectWindow>> *unadopted = nullptr, bool alone = false);
    [[nodiscard]] std::optional<Session> prepareLocalPlacement(const PreparedDrop &drop) const;
    // CARD-LIFECYCLE.md §5: the windows a published plan no longer names, with
    // the records this session still holds for them. Captured before the plan
    // is published and handed over after it, so a gesture that fails changes
    // nothing and a gesture that commits never loses a record.
    struct PendingEviction {
        QPointer<KWin::EffectWindow> window;
        NativeMoveSnapshot record;
        QString sourceKey;
        RestoreSnapshot snapshot;
        // The window the plan admitted in its place, when one did.
        QPointer<KWin::EffectWindow> arrival;
    };
    [[nodiscard]] QList<PendingEviction> captureEvictions(
        const Session &live, const Session &published,
        KWin::EffectWindow *arrival = nullptr) const;
    void publishEvictions(const QList<PendingEviction> &pending);
    // §5: a pane an arrival made yield waits behind the layout as a card, and
    // comes back to the layout when that arrival leaves it, if it still waits
    // there and the layout has room for it.
    struct Displacement {
        QString outputName;
        QPointer<KWin::EffectWindow> arrival;
        QPointer<KWin::EffectWindow> yielded;
    };
    QList<Displacement> m_displaced;
    void returnDisplacedCards(const QString &key);
    // §5: a window leaves for the display that can hold it as a card. Where
    // none can, nothing leaves and the layout keeps the combination it has.
    [[nodiscard]] bool canPlaceEvictedCard() const;
    // DECISIONS.md § A display without cards organizes everything it shows: a
    // layout there sends what it has no room for to the dock, never to another
    // display and never loose beside it.
    [[nodiscard]] bool parksOverflow(const QString &key) const;
    // CARD-LIFECYCLE.md §11: the displays the zones drawn in KDE's tile editor
    // can lay out, switched to zones or not: one without cards, and one sized
    // Connected, which plays its cards into them.
    [[nodiscard]] bool canTakeDrawnZones(const QString &key) const;
    void parkWindow(const QString &key, KWin::EffectWindow *window,
                    const RestoreSnapshot *record = nullptr);
    void minimizeParked(const QString &key);
    // Asks the layout again for every window the dock holds and shows those it
    // now has room for. The caller publishes the result.
    bool wakeParked(const QString &key);
    bool admitArrival(Session *session, KWin::EffectWindow *window,
                      const RestoreSnapshot &snapshot);
    // CARD-LIFECYCLE.md §8: a layout that cannot grow gives the arrival one
    // slot. The slot is the smallest whose pixel size satisfies the arrival's
    // minimum, so a window that only fits the wide pane takes the wide pane;
    // `nullopt` when no slot does, which is the arrival §8 answers with a card
    // instead. §11, Connected: the fitting slot used longest ago, never the
    // snapped card's.
    [[nodiscard]] std::optional<int> slotForArrival(const Session &session,
                                                    KWin::EffectWindow *arrival) const;
    // §11, Connected: an arrival no pane fits still joins where the whole
    // layout, laid out again, can hold it. The panes used longest ago leave
    // for Spread one at a time, never the snapped card, until that layout
    // shows the arrival and every pane left; only those leave. Solved on
    // `candidate`, a value copy of the live session, which gains the arrival.
    // False changes nothing: no such layout exists, and §8 makes the arrival
    // a card.
    [[nodiscard]] bool autoFillYielding(Session &candidate, const RestoreSnapshot &arrival) const;
    // The pane cap every solve on `output` reads (BentoLayout.h): its
    // sizing's, Connected or Solo, where it plays cards; none for a display
    // without cards, whose work area decides.
    [[nodiscard]] std::optional<int> paneCap(const KWin::LogicalOutput *output) const;
    // Put the arrival in that slot on a value copy. The layout keeps its shape
    // and every other pane keeps its place, so exactly one window changes and
    // the panes the user did not touch do not move.
    [[nodiscard]] bool takeSlotAtCap(Session &candidate, int slot,
                                     const RestoreSnapshot &arrival) const;
    // §5: shorten a value copy by what one solve cannot show, so the layout
    // asked for is one it can show in full. Nothing is published and no owner
    // moves. False is §5's no-card-display case: the caller keeps what it had.
    [[nodiscard]] bool shortenToShowable(Session &session, const RestoreSnapshot &arrival,
        std::optional<BentoSidePlacement> side, KWin::EffectWindow *sideWindow) const;
    // §7: a minimize or wake changes what a layout owns awake, so it re-solves
    // before publishing. Settled by whoever observed it, because it can owe an
    // eviction and applySession is already committed to placing panes.
    void settleParticipation(const QString &key);
    // §5 and §7 wait while a pane is moved or a divider held. Once that ends,
    // what happened meanwhile is settled: a pane minimized then leaves as a
    // sleeping card, and a layout left with one pane ends. True when the
    // layout on that display ended, so nothing is left to publish.
    [[nodiscard]] bool settleAfterInteraction(const QString &key);

    [[nodiscard]] QString outputKey(const KWin::LogicalOutput *output) const;
    [[nodiscard]] KWin::LogicalOutput *outputForKey(const QString &key) const;
    [[nodiscard]] KWin::Rect stageArea(KWin::LogicalOutput *output) const;
    [[nodiscard]] KWin::Rect workspaceArea(KWin::LogicalOutput *output) const;
    [[nodiscard]] Session *sessionForOutput(KWin::LogicalOutput *output);
    [[nodiscard]] const Session *sessionForOutput(
        const KWin::LogicalOutput *output) const;
    [[nodiscard]] QList<QPointer<KWin::EffectWindow>> collectWindows(
        KWin::LogicalOutput *output, KWin::EffectWindow *preferred) const;
    [[nodiscard]] RestoreSnapshot makeSnapshot(
        KWin::EffectWindow *window) const;
    bool activate(KWin::LogicalOutput *output,
                  KWin::EffectWindow *preferred = nullptr,
                  std::optional<BentoSidePlacement> side = {});
    void restoreSession(const QString &key, bool outputRemoving = false);
    // A place moved and shrunk into the room panels leave on output.
    [[nodiscard]] static KWin::RectF fittedIntoWorkArea(const KWin::RectF &geometry,
        KWin::LogicalOutput *output);
    // Where a window is given back on `target`: on the display its place was
    // taken on, that place, fitted clear of panels when it is its own; on any
    // other, carried there as a card's is (ReleasePlace.h), never pinned to
    // an edge.
    [[nodiscard]] static KWin::RectF givenBackPlace(const RestoreSnapshot &snapshot,
        KWin::LogicalOutput *target, bool ownDisplay);
public:
    // Where the last display to go away had its layout's window given back,
    // which KWin's own evacuation can overwrite before a card is made of it.
    [[nodiscard]] std::optional<KWin::RectF> placeGivenBack(const KWin::EffectWindow *window) const
    {
        for (const auto &[given, place] : m_givenBack)
            if (given && given == window) return place;
        return std::nullopt;
    }
private:
    QList<std::pair<QPointer<KWin::EffectWindow>, KWin::RectF>> m_givenBack;
    // A window given back that was minimized before its layout is minimized
    // again: now, when KWin already holds its restored state, which returns
    // true, else once KWin does.
    bool reminimizeRestored(KWin::Window *client, const RestoreSnapshot &snapshot);
    bool applySession(Session &session, bool activateLead);
    void scheduleSettle();
    void settleSessions();
    // CARD-LIFECYCLE.md §5: a pane that will not fit the rect it was given is
    // a window the layout cannot show, so it leaves for card ownership and the
    // layout keeps the panes that did settle. §13 keeps the native desktop for
    // release and disable, so no settle returns a session to Plasma.
    void shedUnsettledPanes(const QString &key);
    // Every pane stands on its rect, or with `shortOfPlace` within it.
    bool sessionGeometryMatches(const Session &session, bool shortOfPlace = false) const;
    void removeWindow(KWin::EffectWindow *window, bool restoreSnapshot);
    bool handoffWindowToOutput(KWin::EffectWindow *window,
                               KWin::LogicalOutput *destination,
                               const KWin::RectF &destinationGeometry,
                               CardDropIntent intent = CardDropIntent::OpenSpace,
                               std::optional<BentoSidePlacement> side = {});
    // A solve reports what it cannot show; it never moves an owner. CARD-LIFECYCLE.md
    // §5 gives such a window to card ownership, which is a cross-stage transaction
    // and therefore the publisher's work, not the solve's. A preview passes nullptr.
    bool reflowSession(Session &session,
                       KWin::EffectWindow *preferred = nullptr, bool requirePreferred = false,
                       bool invalidateApplication = true,
                       QList<QPointer<KWin::EffectWindow>> *evicted = nullptr);
    bool planSession(Session &session, KWin::EffectWindow *preferred,
                     bool requirePreferred,
                     QList<QPointer<KWin::EffectWindow>> *evicted = nullptr) const;
    // Which door card ownership opens for an evicted window.
    enum class EvictedAs { AwakeCard, SleepingCard, HomedCard };
    // CARD-LIFECYCLE.md §5: a window the layout cannot show becomes an awake
    // individual card on the display that can hold one. That is the same
    // destination-first transfer a deliberate carry makes, so it is the
    // publisher's work and never the solve's. A refusal changes nothing: §5
    // keeps the combination rather than shedding a window with no owner to
    // become, which is the case with no card-owning display attached.
    // `destination` is the only difference between §5's awake remainder and
    // §7's minimized pane, so the transfer itself is written once.
    bool evictToTablet(const QString &sourceKey, KWin::EffectWindow *window,
                       const std::function<bool()> &sourceValid = {},
                       EvictedAs destination = EvictedAs::AwakeCard);
    // CARD-LIFECYCLE.md §5: Bento ends at one visible pane, and that pane
    // becomes an individual card rather than an ordinary desktop window. Call
    // it after a shortening has published, never inside one: ending is itself
    // a cross-stage transfer. A display that cannot own cards has nowhere for
    // the pane to go, so its session keeps the combination it has.
    void endLayoutIntoCardOwnership(const QString &key);
    void retireEmptiedLayout(const QString &key);
    // Whether a layout holding `session` ends into card ownership, as above,
    // or goes on presenting what it holds: a display holding its own cards and
    // a placed card's room keep a layout of one, and a sleeping pane keeps it.
    [[nodiscard]] bool layoutEndsIntoCards(const Session &session) const;
    // CARD-LIFECYCLE.md §5: a layout shows its whole combination or it is not
    // that layout. `probe` is the value the caller is about to ask for,
    // including any arrival it is adding; one solve names what that value
    // cannot show, and each of those windows leaves for card ownership while
    // this session still holds its record. Nothing is published here.
    bool shedUnshowable(const QString &key, Session probe,
                        KWin::EffectWindow *preferred, bool requirePreferred);
    void adjustRail(Session &session, KWin::EffectWindow *window,
                    const KWin::RectF &start, const KWin::RectF &finish);

    DesktopStageHost *m_host;
    std::shared_ptr<const int> m_carrySourceIdentity = std::make_shared<const int>(0);
    // The last answer a carry preview onto a display without cards logged, so
    // a drag reports each change of answer once rather than every frame.
    QString m_lastOverflowPreview;
    DeferredCommandGuard m_applicationGuard;
    bool m_restoring = false;
    bool m_parking = false;
    std::vector<std::unique_ptr<RestoredMinimization>> m_restoredMinimizations;
    QList<QPointer<KWin::LogicalOutput>> m_retiredOutputs;
    QTimer m_settleTimer;
    // The placement each output has already been given a second grace for. A
    // client that moves itself inside the grace reads as unsettled, so every
    // placement is asked for once more before the layout answers for it.
    QHash<QString, quint64> m_settleRetries;
    QHash<QString, Session> m_sessions;
    QPointer<KWin::EffectWindow> m_interactionWindow;
    KWin::RectF m_interactionStart;
    QString m_interactionOutput;
    QString m_pendingDropOutput;
    bool m_interactionResize = false;
};

} // namespace Kadunce

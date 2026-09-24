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
    [[nodiscard]] virtual bool allowsDesktopStageOnOutput(
        const KWin::LogicalOutput *output) const = 0;
    [[nodiscard]] virtual bool isManagedWindowForDesktopStage(
        const KWin::EffectWindow *window) const = 0;
    [[nodiscard]] virtual KWin::LogicalOutput *tabletOutputForDesktopStage()
        const = 0;
    // Whether this display can hold individual cards at all. CARD-LIFECYCLE.md
    // §5 ends Bento into one, so the rule needs the capability rather than the
    // display's hardware identity; PRODUCT-CONTRACT.md gives an external output
    // ordinary windows or per-output Bento, never cards.
    [[nodiscard]] virtual bool outputCanOwnCards(const KWin::LogicalOutput *) const {
        return false;
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
    [[nodiscard]] virtual std::optional<NativeMoveSnapshot> activeRestoreForDesktopStage(
        KWin::EffectWindow *) const { return std::nullopt; }
    // CARD-LIFECYCLE.md §5: a displaced pane becomes a nonselected individual
    // card. While the display presents its layout that card is owned and
    // hidden, so it must not arrive through the path that makes a transferred
    // window the Active card. A host that cannot tell the difference may
    // forward this to `admitTransferredWindowToTablet`.
    virtual bool admitDisplacedPaneToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) {
        return admitTransferredWindowToTablet(window, commitSource, restore);
    }
    // CARD-LIFECYCLE.md §7: the user minimized a pane, so it leaves Bento at
    // once and becomes a sleeping individual card. It is neither presented nor
    // selected, and it is never offered a pane again until the user wakes it.
    // A host whose card ownership cannot hold a sleeping window answers false,
    // and §5 then keeps the window where it is rather than shedding it to
    // nobody.
    virtual bool admitSleepingPaneToTablet(
        KWin::EffectWindow *, const std::function<bool()> &,
        const NativeMoveSnapshot * = nullptr) {
        return false;
    }
    // `restore` is the record the window had while a Bento session still held
    // it. CARD-LIFECYCLE.md §5 keeps that record so release still returns the
    // window where it began, and after the session has published its shortened
    // plan the host can no longer find it for itself.
    virtual bool admitTransferredWindowToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource,
        const NativeMoveSnapshot *restore = nullptr) = 0;
};

class DesktopStageController final
{
public:
    explicit DesktopStageController(DesktopStageHost *host);
    [[nodiscard]] std::optional<PreparedCarrySource> prepareNativeCarrySource(KWin::EffectWindow *window) const;
    [[nodiscard]] bool nativeCarrySourceValid(const PreparedCarrySource &source) const;

    [[nodiscard]] bool hasActiveSession() const;
    [[nodiscard]] bool ownsWindow(KWin::EffectWindow *window) const;
    // A session's visible pane combination, which CARD-LIFECYCLE.md §5 makes
    // everything it owns awake. The two differ only where §5 could not shed a
    // §7 sleeping window, which is a display with no card owner to shed it to.
    [[nodiscard]] bool managesWindow(KWin::EffectWindow *window) const;
    [[nodiscard]] bool hasSessionOnOutput(const QString &outputName) const;
    void restoreAllSessions();
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
    // own last refusal; a false answer publishes nothing.
    bool extractPaneToCards(KWin::EffectWindow *window,
        const std::function<bool()> &sourceValid);
    void stopPendingSettle();
    void cancelRestoredMinimizations();

    // Read-only §14 observation. Reduces each live session to the identities
    // that decide ownership; it never exposes or mutates session state.
    [[nodiscard]] std::vector<BentoOwnershipView> ownershipView() const;

    [[nodiscard]] QStringList outputStageState() const;
    bool toggleOnOutput(const QString &outputName);
    struct GrabRail { QString output; int index; bool vertical; QRectF pill; QRectF hitArea; };
    QList<GrabRail> grabRails() const;
    bool beginRail(QPointF position);
    void updateRail(QPointF position);
    void finishRail(bool commit);
    QList<QRectF> railPreview(const QString &output) const;
    bool handoffLeadToOutput(const QString &sourceName,
                             const QString &destinationName);

    // Returns true when an existing session grew to show the new window as a
    // pane. False leaves it for card ownership; nothing is ever parked.
    bool handleWindowAdded(KWin::EffectWindow *window);
    void handleWindowClosed(KWin::EffectWindow *window);
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
    // proved on a value copy that it can show the card.
    [[nodiscard]] bool admitCardToLiveBento(KWin::EffectWindow *window,
                                            const std::function<bool()> &commitSource);

    enum class CardDropIntent { OpenSpace, ActivateBento, NativeDesktop };
    // Opaque receiver reservation. Copies share consumption: a preview cannot
    // be replayed, including when source commitment is rejected.
    class PreparedDrop {
    public:
        std::optional<BentoSidePlacement> sidePlacement() const { return side; }
        KWin::EffectWindow *namedPartner() const { return pairPartner.data(); }
        KWin::LogicalOutput *destinationOutput() const { return output.data(); }
        bool detachesToDesktop() const { return intent == CardDropIntent::NativeDesktop && leavingBento; }
        bool showsPlacementOutline() const { return intent != CardDropIntent::NativeDesktop || leavingBento; }
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
        KWin::EffectWindow *pairPartner = nullptr) const;
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
        bool applying = false;
        bool participationDirty = false;
        // One window snapped to a side of a display without cards takes half
        // of it, until a second window joins (DECISIONS.md § A display without
        // cards organizes everything it shows).
        bool lone = false;
        quint64 applicationToken = 0;
    };
    struct RailDrag {
        GrabRail rail;
        Session original;
        Session preview;
        quint64 generation;
        KWin::Rect area;
        QPointF contact;
    };
    std::optional<RailDrag> m_railDrag;
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
        QList<QPointer<KWin::EffectWindow>> *unadopted = nullptr);
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
    };
    [[nodiscard]] QList<PendingEviction> captureEvictions(
        const Session &live, const Session &published) const;
    void publishEvictions(const QList<PendingEviction> &pending);
    // §5: a window leaves for the display that can hold it as a card. Where
    // none can, nothing leaves and the layout keeps the combination it has.
    [[nodiscard]] bool canPlaceEvictedCard() const;
    // DECISIONS.md § A display without cards organizes everything it shows: a
    // layout there sends what it has no room for to the dock, never to another
    // display and never loose beside it.
    [[nodiscard]] bool parksOverflow(const QString &key) const;
    void parkWindow(const QString &key, KWin::EffectWindow *window,
                    const RestoreSnapshot *record = nullptr);
    void minimizeParked(const QString &key);
    bool admitArrival(Session *session, KWin::EffectWindow *window,
                      const RestoreSnapshot &snapshot);
    // CARD-LIFECYCLE.md §8: a layout that cannot grow gives the arrival one
    // slot. The slot is the smallest whose pixel size satisfies the arrival's
    // minimum, so a window that only fits the wide pane takes the wide pane;
    // `nullopt` when no slot does, which is the arrival §8 answers with a card
    // instead.
    [[nodiscard]] std::optional<int> slotForArrival(const Session &session,
                                                    KWin::EffectWindow *arrival) const;
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
    bool applySession(Session &session, bool activateLead);
    void scheduleSettle();
    void settleSessions();
    // CARD-LIFECYCLE.md §5: a pane that will not take the rect it was given is
    // a window the layout cannot show, so it leaves for card ownership and the
    // layout keeps the panes that did settle. §13 keeps the native desktop for
    // release and disable, so no settle returns a session to Plasma.
    void shedUnsettledPanes(const QString &key);
    bool sessionGeometryMatches(const Session &session) const;
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
    enum class EvictedAs { AwakeCard, SleepingCard };
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

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
    virtual bool admitTransferredWindowToTablet(
        KWin::EffectWindow *window, const std::function<bool()> &commitSource) = 0;
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
    // everything it owns awake. The two differ only by §7's sleeping windows.
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
    // CARD-LIFECYCLE.md §5: a window the layout cannot show becomes an awake
    // individual card on the display that can hold one. That is the same
    // destination-first transfer a deliberate carry makes, so it is the
    // publisher's work and never the solve's. A refusal changes nothing: §5
    // keeps the combination rather than shedding a window with no owner to
    // become, which is the case with no card-owning display attached.
    bool evictToTablet(const QString &sourceKey, KWin::EffectWindow *window);
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
    std::vector<std::unique_ptr<RestoredMinimization>> m_restoredMinimizations;
    QList<QPointer<KWin::LogicalOutput>> m_retiredOutputs;
    QTimer m_settleTimer;
    QHash<QString, Session> m_sessions;
    QPointer<KWin::EffectWindow> m_interactionWindow;
    KWin::RectF m_interactionStart;
    QString m_interactionOutput;
    QString m_pendingDropOutput;
    bool m_interactionResize = false;
};

} // namespace Kadunce

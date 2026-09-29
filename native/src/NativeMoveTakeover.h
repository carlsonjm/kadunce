/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "CarrySession.h"
#include <effect/effecthandler.h>
#include <window.h>
#include <workspace.h>
#include <QPointer>
#include <QScopedValueRollback>
#include <functional>

namespace Kadunce {

// Observed native state immediately before cancellation. This does NOT replace
// the source controller's authoritative pre-Active/Bento restore record: origin's
// restoreToken/revision refer to that record, which must remain owned by source.
struct NativeMoveSnapshot {
    QPointer<KWin::Window> window;
    QPointer<KWin::LogicalOutput> output;
    KWin::RectF geometry;
    KWin::RectF floatingGeometry;
    KWin::RectF fullscreenRestoreGeometry;
    KWin::MaximizeMode maximizeMode = KWin::MaximizeRestore;
    KWin::QuickTileMode quickTileMode;
    bool fullScreen = false;
    bool minimized = false;
};

// A native-call boundary, not an input filter or destination policy. Callers
// supply a verified physical owner and a source reservation. The filter must
// retain/drain that owner after cancellation and render the resulting pose.
// Do not enable adoption until both adapters exist for the selected source.
class NativeMoveTakeover final : public QObject {
public:
    enum class Result { Rejected, Carrying, Interrupted };

    Result adopt(KWin::Window *window, CarryOwner owner, CarryOrigin origin,
                 QPointF contact, QPointF topLeft,
                 std::function<bool()> sourceValid, bool deferredDesktop = false,
                 QPointF pendingMotion = {})
    {
        m_rejection = nullptr;
        const auto reject = [this](const char *check) { m_rejection = check; return Result::Rejected; };
        if (m_preparing || m_cancellingWindow || busy() || m_pendingOutcome)
            return reject("rejected-carry-busy");
        if (!window || window->isDeleted() || !window->output())
            return reject("rejected-window-gone");
        if (KWin::workspace()->moveResizeWindow() != window
            || !window->isInteractiveMove() || window->isInteractiveResize())
            return reject("rejected-not-moving");
        if (origin.card != window->internalId().toString()) return reject("rejected-other-window");
        if (!deferredDesktop && origin.output != window->output()->name())
            return reject("rejected-output");
        if (!sourceValid) return reject("rejected-source");
        QScopedValueRollback<bool> preparing(m_preparing, true);
        const QPointer<KWin::Window> candidate(window);
        const QPointer<KWin::LogicalOutput> output(window->output());
        if (!sourceValid()) return reject("rejected-source");
        if (!candidate || candidate->isDeleted() || !output) return reject("rejected-window-gone");
        if (candidate->output() != output || !KWin::effects->screens().contains(output.data()))
            return reject("rejected-output");
        if (KWin::workspace()->moveResizeWindow() != candidate
            || !candidate->isInteractiveMove() || candidate->isInteractiveResize())
            return reject("rejected-not-moving");

        disconnectSource();
        if (!m_carry.begin(owner, std::move(origin), contact, topLeft))
            return reject("rejected-carry-begin");
        m_owner = owner;
        m_snapshot = {window, window->output(), window->frameGeometry(),
            window->geometryRestore(), window->fullscreenGeometryRestore(),
            window->maximizeMode(), window->quickTileMode(),
            window->isFullScreen(), window->isMinimized()};
        m_sourceValid = std::move(sourceValid);
        m_snapshot.geometry.translate(pendingMotion);
        m_connections.append(connect(window, &KWin::Window::closed, this, [this] {
            m_carry.cardClosed(m_carry.origin().card);
            collectOutcome();
        }));
        m_connections.append(connect(window, &KWin::Window::outputChanged, this, [this] {
            if (!m_cancellingWindow) invalidateSource();
        }));
        m_connections.append(connect(KWin::effects, &KWin::EffectsHandler::screenRemoved,
            this, [this](KWin::LogicalOutput *output) {
                if (output == m_snapshot.output) invalidateSource();
            }));

        // Published BEFORE KWin synchronously emits interactiveMoveResizeFinished.
        // Even if that callback cancels/closes/removes source, its finish remains
        // takeover (never a drop). Recursive adoption is prohibited until return.
        m_cancellingWindow = window;
        window->cancelInteractiveMoveResize();
        m_cancellingWindow.clear();
        // A deferred ordinary move may already have crossed outputs. Native
        // cancel returns to its original display; keep that authoritative home
        // while the compositor carry retains the observed pre-cancel pose.
        if (deferredDesktop && candidate && candidate->output()
            && candidate->output()->name() == m_carry.origin().output)
            m_snapshot.output = candidate->output();
        if (!valid()) return Result::Interrupted;
        return Result::Carrying;
    }

    // Which check declined the last adoption, for the move trace.
    const char *rejection() const { return m_rejection; }

    bool ownsNativeFinish(const KWin::Window *window) const
    { return window && m_cancellingWindow == window; }
    bool move(CarryOwner owner, QPointF contact)
    { return owner == m_owner && valid() && m_carry.move(owner, contact); }
    std::optional<CarryTicket> preview(CarryDestination destination)
    { return valid() ? m_carry.preview(std::move(destination)) : std::nullopt; }
    std::optional<CarryDropRequest> release(CarryOwner owner)
    {
        if (owner != m_owner || !valid()) return std::nullopt;
        auto request = m_carry.release(owner);
        collectOutcome();
        return request;
    }
    bool resolve(CarryTicket ticket, bool accepted, quint64 sourceRevision,
                 quint64 destinationRevision)
    {
        if (!valid()) return false;
        const bool result = m_carry.resolve(ticket, accepted, sourceRevision, destinationRevision);
        collectOutcome();
        return result;
    }
    void cancel() { m_carry.cancel(); collectOutcome(); }
    void invalidateSource() { m_carry.sourceChanged(); collectOutcome(); }
    std::optional<CarryOutcome> takeOutcome()
    {
        auto result = std::move(m_pendingOutcome);
        m_pendingOutcome.reset();
        return result;
    }
    const NativeMoveSnapshot &snapshot() const { return m_snapshot; }
    QPointF position() const { return m_carry.position(); }
    bool busy() const {
        return m_carry.phase() == CarrySession::Phase::Carrying
            || m_carry.phase() == CarrySession::Phase::AwaitingAcceptance;
    }

private:
    bool valid()
    {
        if (!busy()) return false;
        if (!m_snapshot.window || m_snapshot.window->isDeleted()) {
            m_carry.cardClosed(m_carry.origin().card);
        } else if (!m_snapshot.output
                   || !KWin::effects->screens().contains(m_snapshot.output.data())
                   || m_snapshot.window->output() != m_snapshot.output
                   || m_snapshot.window->isInteractiveMove()
                   || m_snapshot.window->isInteractiveResize()
                   || !m_sourceValid || !m_sourceValid()) {
            m_carry.sourceChanged();
        }
        collectOutcome();
        return busy();
    }
    void collectOutcome()
    {
        if (auto outcome = m_carry.takeOutcome()) {
            m_pendingOutcome = std::move(outcome);
            disconnectSource();
        }
    }
    void disconnectSource()
    {
        for (const auto &connection : std::as_const(m_connections)) disconnect(connection);
        m_connections.clear();
    }
    CarrySession m_carry;
    CarryOwner m_owner{CarryDevice::Pointer, 0, 0};
    NativeMoveSnapshot m_snapshot;
    QPointer<KWin::Window> m_cancellingWindow;
    std::function<bool()> m_sourceValid;
    QList<QMetaObject::Connection> m_connections;
    std::optional<CarryOutcome> m_pendingOutcome;
    const char *m_rejection = nullptr;
    bool m_preparing = false;
};
} // namespace Kadunce

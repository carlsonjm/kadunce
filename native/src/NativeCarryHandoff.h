/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "PreparedCarrySource.h"
#include <QTimer>

namespace Kadunce {
// One-event-turn coordination between native-start and correlated input proof.
// Caller routes finish through ownsNativeFinish BEFORE fallback drop handling.
class NativeCarryHandoff final : public QObject {
public:
    bool stage(KWin::Window *window, PreparedCarrySource source,
               std::function<bool()> sourceValid, std::function<void()> nativeFallback)
    {
        if (m_pending || m_resolving || m_takeover.busy() || m_ownedSource
            || !window || window->isDeleted() || !window->isInteractiveMove()
            || source.origin().card != window->internalId().toString()
            || !sourceValid || !nativeFallback) return false;
        m_pending.emplace(Pending{window, std::move(source), std::move(sourceValid),
                                  std::move(nativeFallback)});
        const auto generation = ++m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (generation == m_generation) flushPending();
        });
        return true;
    }

    NativeMoveTakeover::Result identify(KWin::Window *window, CarryOwner owner,
                                       QPointF contact, const std::function<bool()> &ownerValid,
                                       QPointF pendingMotion = {})
    {
        using Result = NativeMoveTakeover::Result;
        if (!m_pending || m_resolving || m_pending->window != window || !ownerValid)
            return Result::Rejected;
        const auto generation = m_generation;
        const auto pending = *m_pending;
        QScopedValueRollback<bool> resolving(m_resolving, true);
        if (!ownerValid() || generation != m_generation || !pending.window
            || !pending.sourceValid() || generation != m_generation || !pending.window)
            return Result::Rejected;
        // Validate contact before cancellation. Do not retain it as source validity:
        // the physical release ends input ownership but not destination acceptance.
        auto adoptionOwnerCheck = std::make_shared<std::function<bool()>>(ownerValid);
        const auto result = m_takeover.adopt(pending.window, owner, pending.source.origin(),
            contact, pending.window->frameGeometry().topLeft() + pendingMotion,
            [sourceValid = pending.sourceValid, adoptionOwnerCheck] {
                return sourceValid() && (!*adoptionOwnerCheck || (*adoptionOwnerCheck)());
            }, pending.source.isDesktopWindow(), pendingMotion);
        *adoptionOwnerCheck = {}; // release validity belongs to the input adapter afterward
        if (result == Result::Rejected) return result; // queued fallback still owns native start
        m_pending.reset(); ++m_generation; // canceled native finish is never a drop
        if (generation + 1 != m_generation || result == Result::Interrupted) {
            m_takeover.cancel();
            return Result::Interrupted;
        }
        m_ownedSource = pending.source;
        return result;
    }

    void flushPending()
    {
        if (!m_pending || m_resolving) return;
        auto pending = std::move(*m_pending);
        m_pending.reset(); ++m_generation;
        if (pending.window && !pending.window->isDeleted()) pending.fallback();
    }
    bool deferOrdinary(KWin::Window *window) {
        if (!m_pending || m_pending->window != window || !m_pending->source.isDesktopWindow()) return false;
        ++m_generation; // Retire only the one-turn fallback, not the reservation.
        return true;
    }
    const PreparedCarrySource *pendingSource() const { return m_pending ? &m_pending->source : nullptr; }
    void cancel()
    {
        ++m_generation; m_pending.reset(); m_ownedSource.reset(); m_drop.reset(); m_takeover.cancel();
    }
    struct DropResult {
        CarryOutcome outcome;
        bool committed = false;
        // Which step declined a drop that did not commit, for the move trace.
        const char *refusal = nullptr;
    };
    // The adapter binds the semantic preview and receiver reservation together.
    // Callbacks are synchronous; commit must revalidate source/receiver and return
    // true only after logical publication (even if native placement is interrupted).
    bool previewDrop(CarryDestination destination, std::function<bool()> valid,
                     std::function<bool(const PreparedCarrySource &)> commit)
    {
        if (m_resolving || !m_ownedSource || !valid || !commit) return false;
        m_drop.reset();
        const auto ticket = m_takeover.preview(destination);
        if (!ticket) return false;
        m_drop.emplace(Drop{*ticket, std::move(destination), std::move(valid), std::move(commit)});
        return true;
    }
    std::optional<DropResult> releaseDrop(CarryOwner owner)
    {
        if (m_resolving) return std::nullopt;
        QScopedValueRollback<bool> resolving(m_resolving, true);
        const auto request = m_takeover.release(owner);
        if (!request) {
            // A foreign release leaves a live carry and its reservation alone.
            if (auto outcome = takeOutcome()) return DropResult{*outcome, false, "refused-foreign-release"};
            return std::nullopt;
        }
        const auto source = m_ownedSource;
        const auto drop = std::exchange(m_drop, std::nullopt);
        const auto generation = m_generation;
        const bool matched = source && drop && request->origin == source->origin()
            && request->ticket == drop->ticket && request->destination == drop->destination;
        const bool accepted = matched && drop->valid();
        // Validation may synchronously disable Kadunce. Never resurrect that carry.
        if (generation == m_generation)
            m_takeover.resolve(request->ticket, accepted, request->origin.revision,
                               request->destination.revision);
        auto outcome = takeOutcome();
        if (!outcome) return std::nullopt;
        bool committed = false;
        const char *refusal = !matched ? "refused-unmatched"
            : !accepted ? "refused-invalid"
            : generation != m_generation ? "refused-superseded"
            : outcome->resolution != CarryResolution::ReadyToCommit ? "refused-not-ready"
            : nullptr;
        if (generation == m_generation && accepted
            && outcome->resolution == CarryResolution::ReadyToCommit) {
            // Native source observers are retired by resolution before placement
            // can change outputs. The transaction still owns source validation.
            committed = drop->commit(*source);
            if (!committed) {
                refusal = "refused-commit";
                outcome->resolution = CarryResolution::ReturnToOrigin;
                outcome->destination.reset();
            }
        }
        return DropResult{*outcome, committed, refusal};
    }
    bool ownsNativeFinish(const KWin::Window *window) const { return m_takeover.ownsNativeFinish(window); }
    void withdrawDrop() { m_drop.reset(); }
    NativeMoveTakeover &carry() { return m_takeover; }
    const std::optional<PreparedCarrySource> &source() const { return m_ownedSource; }
    std::optional<CarryOutcome> takeOutcome()
    {
        auto result = m_takeover.takeOutcome();
        if (result) { m_ownedSource.reset(); m_drop.reset(); }
        return result;
    }
private:
    struct Drop {
        CarryTicket ticket;
        CarryDestination destination;
        std::function<bool()> valid;
        std::function<bool(const PreparedCarrySource &)> commit;
    };
    std::optional<Drop> m_drop;
    struct Pending {
        QPointer<KWin::Window> window;
        PreparedCarrySource source;
        std::function<bool()> sourceValid;
        std::function<void()> fallback;
    };
    NativeMoveTakeover m_takeover;
    std::optional<Pending> m_pending;
    std::optional<PreparedCarrySource> m_ownedSource;
    quint64 m_generation = 0;
    bool m_resolving = false;
};
} // namespace Kadunce

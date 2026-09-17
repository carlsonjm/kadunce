/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QPointF>
#include <QString>
#include <QtGlobal>
#include <optional>
#include <utility>

namespace Kadunce {

enum class CarryDevice { Pointer, Touch };
struct CarryOwner {
    CarryDevice kind;
    quint64 device;
    qint64 contact; // Touch ID or initiating pointer button, scoped to device.
    bool operator==(const CarryOwner &) const = default;
};
struct CarryOrigin {
    QString card;
    QString output;
    quint64 restoreToken = 0; // Opaque token owned by the source adapter.
    quint64 revision = 0;     // Source membership revision at pickup.
    bool operator==(const CarryOrigin &) const = default;
};
enum class CarryDestinationKind { LineGap, StackGap, LayoutSlot, NewLayoutEdge, NativeDesktop, ActiveCard };
enum class CarryEdge { Left, Right, Top, Bottom };
struct CarryDestination {
    CarryDestinationKind kind;
    QString output;
    QString target; // Stable group/layout identity, not a transient row index.
    quint64 revision = 0;
    int position = -1; // Gap or layout slot; adapter validates range/geometry.
    std::optional<CarryEdge> edge = std::nullopt; // Only NewLayoutEdge; never inferred from pose.
    bool operator==(const CarryDestination &) const = default;
};
struct CarryTicket {
    quint64 session = 0;
    quint64 preview = 0;
    bool operator==(const CarryTicket &) const = default;
};
struct CarryDropRequest {
    CarryTicket ticket;
    CarryOrigin origin;
    CarryDestination destination;
};
enum class CarryResolution { ReadyToCommit, ReturnToOrigin, NeedsRecovery, SourceGone };
struct CarryOutcome {
    CarryResolution resolution;
    CarryOrigin origin;
    std::optional<CarryDestination> destination;
};

// Headless intent state only. No native handles, membership edits or callbacks.
// Source/destination adapters own validation, reservations, commit and rollback.
// ReadyToCommit is permission to attempt that transaction, NOT proof it happened.
class CarrySession {
public:
    enum class Phase { Idle, Carrying, AwaitingAcceptance, Finished };
    CarrySession() = default;
    CarrySession(const CarrySession &) = delete;
    CarrySession &operator=(const CarrySession &) = delete;

    bool begin(CarryOwner owner, CarryOrigin origin, QPointF contact, QPointF topLeft)
    {
        if (busy() || m_outcome || origin.card.isEmpty() || origin.output.isEmpty()
            || !origin.restoreToken || !finite(contact) || !finite(topLeft)
            || !finite(contact - topLeft)) return false;
        ++m_ticket.session;
        m_ticket.preview = 0;
        m_owner = owner;
        m_origin = std::move(origin);
        m_anchor = contact - topLeft;
        m_position = topLeft;
        m_destination.reset();
        m_phase = Phase::Carrying;
        return true;
    }

    bool move(CarryOwner owner, QPointF contact)
    {
        if (m_phase != Phase::Carrying || owner != m_owner || !finite(contact)) return false;
        const QPointF position = contact - m_anchor;
        if (!finite(position)) return false;
        m_position = position; // No axis clamp or magnetic displacement.
        clearPreview(); // Resolver must evaluate the new pose, not a stale slot.
        return true;
    }

    std::optional<CarryTicket> preview(CarryDestination destination)
    {
        if (m_phase != Phase::Carrying) return std::nullopt;
        clearPreview();
        if (destination.output.isEmpty() || destination.target.isEmpty()
            || destination.position < 0) return std::nullopt;
        if (destination.kind == CarryDestinationKind::NewLayoutEdge) {
            if (!destination.edge || destination.position != 0) return std::nullopt;
            switch (*destination.edge) {
            case CarryEdge::Left: case CarryEdge::Right:
            case CarryEdge::Top: case CarryEdge::Bottom: break;
            default: return std::nullopt;
            }
        } else if (destination.edge
            || ((destination.kind == CarryDestinationKind::NativeDesktop
                 || destination.kind == CarryDestinationKind::ActiveCard)
                && destination.position != 0)) {
            return std::nullopt;
        }
        m_destination = std::move(destination);
        return m_ticket;
    }

    std::optional<CarryDropRequest> release(CarryOwner owner)
    {
        if (m_phase != Phase::Carrying || owner != m_owner) return std::nullopt;
        if (!m_destination) {
            finish(CarryResolution::ReturnToOrigin);
            return std::nullopt;
        }
        m_phase = Phase::AwaitingAcceptance;
        return CarryDropRequest{m_ticket, m_origin, *m_destination};
    }

    // Adapter must reserve/validate the exact destination and compare current
    // revisions immediately before replying. All calls are on one owning thread.
    bool resolve(CarryTicket ticket, bool accepted,
                 quint64 sourceRevision, quint64 destinationRevision)
    {
        if (m_phase != Phase::AwaitingAcceptance || ticket != m_ticket) return false;
        if (sourceRevision != m_origin.revision) finish(CarryResolution::NeedsRecovery);
        else if (!accepted || destinationRevision != m_destination->revision)
            finish(CarryResolution::ReturnToOrigin);
        else finish(CarryResolution::ReadyToCommit);
        return true;
    }

    void cancel() { if (busy()) finish(CarryResolution::ReturnToOrigin); }
    void sourceChanged() { if (busy()) finish(CarryResolution::NeedsRecovery); }
    void cardClosed(const QString &card)
    {
        if (busy() && card == m_origin.card) finish(CarryResolution::SourceGone);
    }
    void targetChanged(const QString &target)
    {
        if (!busy() || !m_destination || m_destination->target != target) return;
        invalidateDestination();
    }
    void outputRemoved(const QString &output)
    {
        if (!busy()) return;
        if (output == m_origin.output) finish(CarryResolution::NeedsRecovery);
        else if (m_destination && output == m_destination->output) invalidateDestination();
    }

    Phase phase() const { return m_phase; }
    QPointF position() const { return m_position; }
    const CarryOrigin &origin() const { return m_origin; }
    const std::optional<CarryDestination> &destination() const { return m_destination; }
    std::optional<CarryOutcome> takeOutcome()
    {
        auto result = std::move(m_outcome);
        m_outcome.reset();
        return result; // Delivery is one-shot; phase remains terminal.
    }

private:
    static bool finite(QPointF p) { return qIsFinite(p.x()) && qIsFinite(p.y()); }
    bool busy() const { return m_phase == Phase::Carrying || m_phase == Phase::AwaitingAcceptance; }
    void clearPreview() { ++m_ticket.preview; m_destination.reset(); }
    void invalidateDestination()
    {
        if (m_phase == Phase::AwaitingAcceptance) finish(CarryResolution::ReturnToOrigin);
        else clearPreview();
    }
    void finish(CarryResolution resolution)
    {
        m_outcome = CarryOutcome{resolution, m_origin,
            resolution == CarryResolution::ReadyToCommit ? m_destination : std::nullopt};
        m_phase = Phase::Finished;
        clearPreview();
    }
    Phase m_phase = Phase::Idle;
    CarryOwner m_owner{CarryDevice::Pointer, 0, 0};
    CarryOrigin m_origin;
    CarryTicket m_ticket;
    QPointF m_anchor, m_position;
    std::optional<CarryDestination> m_destination;
    std::optional<CarryOutcome> m_outcome;
};
} // namespace Kadunce

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CarrySession.h"
#include <QRectF>

namespace Kadunce {
// Recognize placement during motion, never infer a new intent at release.
// The bottom band belongs to the dock, including its side corners.
inline std::optional<CarryEdge> monitorCarryEdge(QRectF output, QPointF contact)
{
    if (!output.isValid() || !output.contains(contact)
        || contact.y() >= output.bottom() - 48.0) return std::nullopt;
    constexpr double band = 12.0;
    if (contact.x() <= output.left() + band) return CarryEdge::Left;
    if (contact.x() >= output.right() - band) return CarryEdge::Right;
    if (contact.y() <= output.top() + band) return CarryEdge::Top;
    return std::nullopt;
}

struct MonitorLayoutTarget {
    QString identity;
    quint64 revision = 0;
    int insertion = 0;
};

// Called by a destination adapter AFTER target/preview evaluation, not by a
// release-coordinate heuristic. No geometry, timers, ownership or native calls.
// outputRevision must change when output geometry/topology or layout presence
// changes. The commit adapter revalidates it (or the existing layout revision).
inline std::optional<CarryDestination> monitorDropIntent(
    const QString &output, quint64 outputRevision,
    const std::optional<MonitorLayoutTarget> &existingLayout,
    std::optional<CarryEdge> confirmedEdge = std::nullopt)
{
    if (output.isEmpty()) return std::nullopt;
    if (existingLayout) {
        // A malformed or infeasible existing layout must never fall through
        // into a second layout or an ordinary desktop window.
        if (existingLayout->identity.isEmpty() || existingLayout->insertion < 0)
            return std::nullopt;
        return CarryDestination{CarryDestinationKind::LayoutSlot, output,
            existingLayout->identity, existingLayout->revision, existingLayout->insertion, std::nullopt};
    }
    if (confirmedEdge) {
        switch (*confirmedEdge) {
        case CarryEdge::Left: case CarryEdge::Right:
        case CarryEdge::Top: case CarryEdge::Bottom: break;
        default: return std::nullopt;
        }
        return CarryDestination{CarryDestinationKind::NewLayoutEdge, output,
            output, outputRevision, 0, confirmedEdge};
    }
    return CarryDestination{CarryDestinationKind::NativeDesktop, output,
        output, outputRevision, 0, std::nullopt};
}
} // namespace Kadunce

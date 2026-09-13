// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QRectF>
#include <algorithm>

namespace Kadunce {
inline constexpr int RowPageDuration = 220;
struct RowPageFrame { QRectF rect; double rotation; double opacity; };

inline RowPageFrame rowNeighborOrigin(RowPageFrame destination, const QRectF &work, int side)
{
    destination.rect.moveLeft(side < 0 ? work.left() - destination.rect.width() - 32
                                       : work.right() + 32);
    return destination;
}

// Value-only presentation. A wrapped shoulder leaves and re-enters at the
// edges, never flies across the new center card. Relocation is invisible.
inline RowPageFrame rowPageFrame(RowPageFrame from, RowPageFrame to,
                                const QRectF &work, bool wraps, double progress)
{
    const double p = std::clamp(progress, 0.0, 1.0);
    if (p <= 0) return from;
    if (p >= 1) return to;
    double local = p;
    if (wraps) {
        constexpr double ExitShare = 0.5;
        if (p < ExitShare) {
            to = from;
            to.rect.moveLeft(from.rect.center().x() < work.center().x()
                ? work.left() - from.rect.width() - 32 : work.right() + 32);
            local = p / ExitShare;
        } else {
            from = rowNeighborOrigin(to, work,
                to.rect.center().x() < work.center().x() ? -1 : 1);
            local = (p - ExitShare) / (1 - ExitShare);
        }
    }
    const double t = 1 - (1-local)*(1-local)*(1-local);
    const auto blend = [t](double a, double b) { return a + (b-a)*t; };
    return {{blend(from.rect.x(), to.rect.x()), blend(from.rect.y(), to.rect.y()),
             blend(from.rect.width(), to.rect.width()), blend(from.rect.height(), to.rect.height())},
            blend(from.rotation, to.rotation), blend(from.opacity, to.opacity)};
}

inline RowPageFrame sharedRowPageFrame(RowPageFrame from, RowPageFrame to,
                                      const QRectF &work, bool wraps,
                                      double displacement, double progress)
{
    const double p = std::clamp(progress, 0.0, 1.0);
    const double t = 1 - (1-p)*(1-p)*(1-p);
    if (!wraps) return rowPageFrame(from, to, work, false, p);
    // Both ends of a circular row share the same clock and travel. Keep the
    // departing copy until clipped, then use its incoming representation.
    // Neither representation traverses the middle or starts a second easing.
    RowPageFrame outgoing = from;
    outgoing.rect.translate(-displacement*t, 0);
    RowPageFrame incoming = to;
    incoming.rect.translate(displacement*(1-t), 0);
    if (p <= 0) return from;
    if (p >= 1) return to;
    return outgoing.rect.right() > work.left() && outgoing.rect.left() < work.right()
        ? outgoing : incoming;
}
}

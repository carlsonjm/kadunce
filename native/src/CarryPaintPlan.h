/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QRectF>
#include <optional>

namespace Kadunce {
struct CarryPaintPlan {
    QRectF target;
    QRectF clip;
};
// Logical coordinates throughout; the render viewport alone applies output scale.
// Position is CarrySession's anchored top-left, not the pointer itself.
inline std::optional<CarryPaintPlan> carryPaintPlan(
    const QRectF &pickup, QPointF position, const QRectF &output)
{
    const auto valid = [](const QRectF &r) {
        return qIsFinite(r.x()) && qIsFinite(r.y()) && qIsFinite(r.width())
            && qIsFinite(r.height()) && qIsFinite(r.right()) && qIsFinite(r.bottom())
            && r.width() > 0 && r.height() > 0;
    };
    const QRectF target(position, pickup.size());
    if (!valid(pickup) || !valid(target) || !valid(output)) return std::nullopt;
    return CarryPaintPlan{target, target.intersected(output)};
}
} // namespace Kadunce

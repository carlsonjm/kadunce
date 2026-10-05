/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "MonitorDropIntent.h"
#include <QList>
#include <QRectF>
#include <QString>

namespace Kadunce {
// Where a placement request would put an application, read from one global
// logical point. The answer is the destination outline Kadunce shows while the
// request is aimed, and a request is placed only where it was last aimed.
enum class PlacementAimKind { None, Card, Left, Right, Top, Display };

struct PlacementOutput {
    QString name;
    QRectF geometry;
    bool ownsCards = false;
};

struct PlacementAim {
    PlacementAimKind kind = PlacementAimKind::None;
    QString output;
    friend bool operator==(const PlacementAim &, const PlacementAim &) = default;
};

constexpr double PlacementDockBand = 48.0;

// The dock's band answers nothing, so carrying back down into it cancels. On
// the display that owns cards the side edges pair into Bento and anywhere else
// makes the application the Active card, the top edge included. On any other
// display the edges begin or join its layout as a carried window's would, and
// anywhere else opens the application there.
inline PlacementAim placementAim(const QList<PlacementOutput> &outputs, QPointF point)
{
    for (const auto &output : outputs) {
        if (!output.geometry.isValid() || !output.geometry.contains(point)) continue;
        if (point.y() >= output.geometry.bottom() - PlacementDockBand) return {};
        const auto edge = monitorCarryEdge(output.geometry, point);
        PlacementAimKind kind = output.ownsCards ? PlacementAimKind::Card : PlacementAimKind::Display;
        if (edge == CarryEdge::Left) kind = PlacementAimKind::Left;
        else if (edge == CarryEdge::Right) kind = PlacementAimKind::Right;
        else if (edge == CarryEdge::Top && !output.ownsCards) kind = PlacementAimKind::Top;
        return {kind, output.name};
    }
    return {};
}

inline QString placementAimName(PlacementAimKind kind)
{
    switch (kind) {
    case PlacementAimKind::Card: return QStringLiteral("card");
    case PlacementAimKind::Left: return QStringLiteral("left");
    case PlacementAimKind::Right: return QStringLiteral("right");
    case PlacementAimKind::Top: return QStringLiteral("top");
    case PlacementAimKind::Display: return QStringLiteral("display");
    case PlacementAimKind::None: break;
    }
    return QStringLiteral("none");
}
} // namespace Kadunce

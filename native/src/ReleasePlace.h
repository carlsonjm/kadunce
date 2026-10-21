/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <optional>

namespace Kadunce
{
// Where a card's window is given back when Cards let it go (CARD-LIFECYCLE.md
// §13). A card's restore record keeps the place the window had and the
// display it had it on. That display can be gone by the time the card is let
// go, as a monitor unplugged is, and a place given back there would stand
// where no display shows it.
struct ReleaseDisplay {
    QString name;
    QRectF geometry;
    // The room panels leave, which a carried place is fitted into.
    QRectF work;
};

struct ReleaseRecord {
    QRectF place;
    // The display the place was taken on, and its geometry then; empty when
    // the record does not know it.
    QString origin;
    QRectF originGeometry;
};

// A place to carry the window to: onto `display`, keeping the share of the way
// across and down it had on `from`, or centred where `from` is unknown.
struct ReleaseCarry {
    ReleaseDisplay display;
    QRectF from;
};

inline const ReleaseDisplay *releaseDisplayNamed(const QList<ReleaseDisplay> &displays, const QString &name)
{
    if (name.isEmpty()) return nullptr;
    for (const auto &display : displays)
        if (display.name == name) return &display;
    return nullptr;
}

inline const ReleaseDisplay *releaseDisplayAt(const QList<ReleaseDisplay> &displays, const QPointF &point)
{
    for (const auto &display : displays)
        if (display.geometry.contains(point)) return &display;
    return nullptr;
}

// The frame a record's place is measured in, to carry it onto another display:
// the display holding it, else the one it was taken on as it stands now, else
// as it stood.
inline QRectF releaseFrom(const QList<ReleaseDisplay> &displays, const ReleaseRecord &record)
{
    if (const ReleaseDisplay *holding = releaseDisplayAt(displays, record.place.center()))
        return holding->geometry;
    const ReleaseDisplay *origin = releaseDisplayNamed(displays, record.origin);
    return origin ? origin->geometry : record.originGeometry;
}

// The frame a place leaving its display is carried from: a display that is
// gone is measured as it stood, even where another now stands in its place.
inline QRectF releaseCarriedFrom(const QList<ReleaseDisplay> &displays, const ReleaseRecord &record)
{
    if (!record.origin.isEmpty() && !releaseDisplayNamed(displays, record.origin))
        return record.originGeometry;
    return releaseFrom(displays, record);
}

// Whether, and where, a released window has to move. `own` is the display
// the card belongs to where every display holds its own cards, empty
// otherwise; `standing` is the display the window stands on now.
//
// A card that belongs to a display goes back on it (§11, Every display).
// Otherwise its place is kept while its display is still there and the place
// is on a display, and is carried onto the display the window stands on, else
// the first one, when its display is gone or no display holds the place.
inline std::optional<ReleaseCarry> releaseCarry(const QList<ReleaseDisplay> &displays,
    const ReleaseRecord &record, const QString &own, const QString &standing)
{
    const QPointF centre = record.place.center();
    const ReleaseDisplay *holding = releaseDisplayAt(displays, centre);
    const QRectF from = releaseFrom(displays, record);
    if (const ReleaseDisplay *owner = releaseDisplayNamed(displays, own)) {
        if (owner->geometry.contains(centre)) return std::nullopt;
        return ReleaseCarry{*owner, from};
    }
    const bool originGone = !record.origin.isEmpty() && !releaseDisplayNamed(displays, record.origin);
    if (!originGone && holding) return std::nullopt;
    const ReleaseDisplay *target = releaseDisplayNamed(displays, standing);
    if (!target && !displays.isEmpty()) target = &displays.constFirst();
    if (!target) return std::nullopt;
    return ReleaseCarry{*target, releaseCarriedFrom(displays, record)};
}

// A place carried onto `work`: no larger than it, and the same share of the
// way across and down as on `from`, or centred where `from` is unknown.
inline QRectF carriedPlace(const QRectF &place, const QRectF &from, const QRectF &work)
{
    if (place.width() <= 0 || place.height() <= 0 || work.isEmpty()) return place;
    const double width = std::min(place.width(), work.width());
    const double height = std::min(place.height(), work.height());
    const double across = from.width() > place.width()
        ? (place.x() - from.x()) / (from.width() - place.width()) : 0.5;
    const double down = from.height() > place.height()
        ? (place.y() - from.y()) / (from.height() - place.height()) : 0.5;
    return QRectF(work.x() + std::clamp(across, 0.0, 1.0) * (work.width() - width),
        work.y() + std::clamp(down, 0.0, 1.0) * (work.height() - height), width, height);
}
} // namespace Kadunce

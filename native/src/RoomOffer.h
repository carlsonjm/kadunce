/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QRectF>
#include <QSizeF>

#include <algorithm>
#include <cmath>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: after a side snap leaves room open, that
// room offers the display's other cards as small pictures in Spread's order,
// each named below as Spread names it. One click fills the room with it.
inline constexpr double RoomOfferTileWidth = 240.0;
inline constexpr double RoomOfferTileTallest = 150.0;
// The Visual sheet's Surface step between pictures and around them.
inline constexpr double RoomOfferGap = 24.0;
// What a name and the space above it take under each picture.
inline constexpr double RoomOfferNameRow = 32.0;

// Each picture's place in `room`, in order: rows centred in the room, each as
// tall as its window is for its width, at most RoomOfferTileTallest. Cards
// that do not fit are not offered, so fewer places than sizes can come back.
inline QList<QRectF> roomOfferTiles(const QRectF &room, const QList<QSizeF> &sizes)
{
    QList<QRectF> tiles;
    const double width = std::min(RoomOfferTileWidth, room.width() - 2.0 * RoomOfferGap);
    if (width < 80.0 || sizes.isEmpty()) return tiles;
    const int across = std::max(1, int((room.width() - RoomOfferGap) / (width + RoomOfferGap)));
    const double cell = std::min(RoomOfferTileTallest, width * 10.0 / 16.0) + RoomOfferNameRow;
    const int down = int((room.height() - RoomOfferGap) / (cell + RoomOfferGap));
    const int count = std::min<int>(sizes.size(), across * down);
    if (count <= 0) return tiles;
    const int rows = (count + across - 1) / across;
    const double top = room.y() + (room.height() - rows * cell - (rows - 1) * RoomOfferGap) / 2.0;
    for (int i = 0; i < count; ++i) {
        const int row = i / across;
        const int inRow = std::min(across, count - row * across);
        const double left = room.x()
            + (room.width() - inRow * width - (inRow - 1) * RoomOfferGap) / 2.0;
        const QSizeF size = sizes.at(i);
        const double height = size.width() > 0.0
            ? std::clamp(width * size.height() / size.width(), 40.0, RoomOfferTileTallest)
            : cell - RoomOfferNameRow;
        const double cellTop = top + row * (cell + RoomOfferGap);
        // Pictures in a row share a bottom edge, so the names line up.
        tiles.append(QRectF(left + (i % across) * (width + RoomOfferGap),
                            cellTop + (cell - RoomOfferNameRow - height), width, height));
    }
    return tiles;
}
} // namespace Kadunce

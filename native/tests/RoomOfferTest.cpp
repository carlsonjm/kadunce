/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RoomOffer.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using namespace Kadunce;
    // The left half of a 2560 by 1440 monitor, three cards to offer.
    const QRectF room(0, 0, 1273, 1380);
    const auto tiles = roomOfferTiles(room, {QSizeF(1600, 1000), QSizeF(1200, 900), QSizeF(800, 1200)});
    require(tiles.size() == 3, "Three cards that fit were not all offered");
    for (const auto &tile : tiles) {
        require(room.contains(tile.adjusted(0, 0, 0, RoomOfferNameRow)), "A picture or its name left the room");
        require(tile.width() == RoomOfferTileWidth && tile.height() <= RoomOfferTileTallest,
                "A picture was not the offer's size");
    }
    require(tiles[0].bottom() == tiles[1].bottom() && tiles[1].bottom() == tiles[2].bottom(),
            "Pictures in a row did not share a bottom edge");
    require(tiles[1].left() - tiles[0].right() == RoomOfferGap, "Pictures were not a Surface step apart");
    const double middle = (tiles.first().left() + tiles.last().right()) / 2.0;
    require(std::abs(middle - room.center().x()) < 1.0, "The row was not centred in the room");
    // A room too small for every card offers the ones that fit, first first.
    const auto few = roomOfferTiles(QRectF(0, 0, 600, 260), QList<QSizeF>(9, QSizeF(1600, 1000)));
    require(few.size() == 2, "A small room did not offer as many cards as fit");
    require(roomOfferTiles(QRectF(0, 0, 100, 100), {QSizeF(1600, 1000)}).isEmpty(),
            "A room with no space for a picture offered one");
    std::cout << "Room offer checks passed\n";
    return 0;
}

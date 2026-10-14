/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "KeyFocus.h"

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
    using Kadunce::KeyDirection;
    using Kadunce::keyNeighbour;
    // A small display at the left, sized Active and showing one card, and a
    // large one at its right whose layout is a full-height column beside two
    // panes stacked under each other.
    const QRectF tablet(8, 8, 1264, 784);
    const QRectF left(2568, 8, 1264, 1424);
    const QRectF upper(3848, 8, 1264, 704);
    const QRectF lower(3848, 728, 1264, 704);
    const QList<QRectF> all{tablet, left, upper, lower};

    require(keyNeighbour(left, all, KeyDirection::Right) == 2, "Right of the column, the upper pane beside it");
    require(keyNeighbour(lower, all, KeyDirection::Left) == 1, "Left of the lower pane, the column");
    require(keyNeighbour(upper, all, KeyDirection::Down) == 3, "Under the upper pane, the lower one");
    require(keyNeighbour(lower, all, KeyDirection::Up) == 2, "Over the lower pane, the upper one");
    require(keyNeighbour(upper, all, KeyDirection::Up) == -1, "Nothing above the top of the column");
    require(keyNeighbour(upper, all, KeyDirection::Right) == -1, "Nothing right of the last display");
    require(keyNeighbour(left, all, KeyDirection::Left) == 0, "Past the layout's edge, the display beside");
    require(keyNeighbour(tablet, all, KeyDirection::Right) == 1, "From the small display, the nearest pane");

    // Two halves: each reaches the other, and a pane only diagonal to the
    // focused one waits behind one beside it.
    const QRectF a(0, 0, 500, 1000), b(510, 0, 500, 1000), corner(510, 1010, 500, 300);
    require(keyNeighbour(a, {a, b}, KeyDirection::Right) == 1 && keyNeighbour(b, {a, b}, KeyDirection::Left) == 0,
            "Two halves did not reach each other");
    require(keyNeighbour(a, {corner, b}, KeyDirection::Right) == 1, "A diagonal pane came before one beside");
    // Frames a little past their slots still count as beside each other.
    require(keyNeighbour(QRectF(0, 0, 520, 900), {QRectF(505, 0, 500, 900)}, KeyDirection::Right) == 0,
            "An overlapping neighbour within the gutter was missed");
    require(keyNeighbour(a, {QRectF(200, 0, 500, 900)}, KeyDirection::Right) == -1,
            "A window under this one counted as beside it");

    // Room toward an edge: a lone half on the right has the left half open.
    const QRectF display(0, 0, 2560, 1440);
    require(Kadunce::keyRoomToward(QRectF(1288, 8, 1264, 1424), display, KeyDirection::Left),
            "A right half did not see the open left half");
    require(!Kadunce::keyRoomToward(QRectF(8, 8, 1264, 1424), display, KeyDirection::Left),
            "A left half saw room past the edge");
    std::cout << "Meta's arrows reach the window beside, on this display or the next\n";
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
// Where a Gooseberry note placed on its window stands on that window's card
// in Spread: the same place and size, scaled with the card, inside it.
#include "NotePlacement.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Kadunce;

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

bool near(double a, double b)
{
    return std::abs(a - b) < 0.01;
}

// A 1600 by 1000 window shown as a 640 by 400 card: two fifths.
const QSizeF Window(1600, 1000);
const QRectF Card(100, 200, 640, 400);
} // namespace

int main()
{
    // Never placed, or a card or window of no size: no place, so the stack.
    require(placedNoteRect(Card, Window, QPointF(-1, -1), QSizeF(300, 200)).isEmpty(),
        "A note never placed was given a place");
    require(placedNoteRect(Card, Window, QPointF(0.5, -1), QSizeF()).isEmpty(),
        "A note placed on one axis only was given a place");
    require(placedNoteRect(QRectF(), Window, QPointF(0.5, 0.5), QSizeF()).isEmpty(),
        "A card of no size was given a note");
    require(placedNoteRect(Card, QSizeF(), QPointF(0.5, 0.5), QSizeF()).isEmpty(),
        "A window of unknown size placed a note");

    // The place and the size are the window's, scaled with the card.
    const QRectF resized = placedNoteRect(Card, Window, QPointF(0.25, 0.5), QSizeF(400, 250));
    require(near(resized.x(), 100 + 160) && near(resized.y(), 200 + 200), "A note left its place on the card");
    require(near(resized.width(), 160) && near(resized.height(), 100), "A note was not scaled with its card");
    // The same note on a nearer, bigger card of the same window.
    const QRectF nearCard(0, 0, 1280, 800);
    const QRectF nearer = placedNoteRect(nearCard, Window, QPointF(0.25, 0.5), QSizeF(400, 250));
    require(near(nearer.width(), 2 * resized.width()) && near(nearer.x(), 320),
        "A note did not grow with its card");

    // Never resized, it is drawn at the size every note starts at: 220 px
    // wide on this window, about three lines tall.
    const QRectF started = placedNoteRect(nearCard, Window, QPointF(0.1, 0.1), QSizeF());
    require(near(started.width(), 220 * 0.8) && near(started.height(), 88 * 0.8),
        "A note never resized was not at the starting size");
    // On a narrow window, the starting width keeps to 140 px.
    const QRectF narrow = placedNoteRect(QRectF(0, 0, 400, 400), QSizeF(400, 400), QPointF(0, 0), QSizeF());
    require(near(narrow.width(), 140), "A note on a narrow window was not 140 px wide");

    // At the right or bottom edge, a note is moved back inside its card, as
    // the notes over the window are.
    const QRectF corner = placedNoteRect(Card, Window, QPointF(1, 1), QSizeF(400, 250));
    require(Card.contains(corner), "A note at the corner left its card");
    require(near(corner.right(), Card.right()) && near(corner.bottom(), Card.bottom()),
        "A note at the corner did not keep to the corner");
    // A note bigger than its window is as big as the card, no bigger.
    const QRectF whole = placedNoteRect(Card, Window, QPointF(0.5, 0.5), QSizeF(4000, 4000));
    require(near(whole.width(), Card.width()) && near(whole.height(), Card.height())
                && Card.contains(whole), "A note bigger than its window left its card");
    // A place past the edge reads as the edge.
    require(Card.contains(placedNoteRect(Card, Window, QPointF(3, 2), QSizeF())), "A place past the edge left the card");

    // However far the card, the note is as big as the stack's mini note.
    const QRectF far = placedNoteRect(QRectF(0, 0, 160, 100), Window, QPointF(0.5, 0.5), QSizeF(200, 100));
    require(far.width() >= NotePlacement::SmallestWidth && far.height() >= NotePlacement::SmallestHeight,
        "A note on a far card was too small to read or touch");
    require(QRectF(0, 0, 160, 100).contains(far), "A note on a far card left it");
    // And a card smaller than that holds the note inside it.
    const QRectF tiny(0, 0, 40, 30);
    require(tiny.contains(placedNoteRect(tiny, Window, QPointF(0.5, 0.5), QSizeF())),
        "A note left a card smaller than itself");

    // The note is worked out from whatever card is passed in, and nothing
    // else: a card drawn part of the way between two places, as on every
    // frame of Spread opening, closing or the row moving, carries the note
    // the same part of the way, in place and in size, never from the place
    // the card is heading for.
    {
        const QRectF active(0, 0, 1600, 1000);
        const QRectF slot(420, 260, 480, 300);
        const QPointF place(0.05, 0.08);
        const QSizeF size(360, 240);
        const QRectF from = placedNoteRect(active, Window, place, size);
        const QRectF to = placedNoteRect(slot, Window, place, size);
        for (const double t : {0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0}) {
            const auto lerp = [t](double a, double b) { return a + (b - a) * t; };
            const QRectF card(lerp(active.x(), slot.x()), lerp(active.y(), slot.y()),
                              lerp(active.width(), slot.width()), lerp(active.height(), slot.height()));
            const QRectF note = placedNoteRect(card, Window, place, size);
            require(near(note.x(), lerp(from.x(), to.x())) && near(note.y(), lerp(from.y(), to.y()))
                        && near(note.width(), lerp(from.width(), to.width()))
                        && near(note.height(), lerp(from.height(), to.height())),
                "An interpolated card did not give an interpolated note");
            // Its place is the card's own, at the note's share of it.
            require(near(note.x() - card.x(), place.x() * card.width())
                        && near(note.y() - card.y(), place.y() * card.height()),
                "A note left its share of the card drawn this frame");
        }
        // Back and forth, each card gives the same note: nothing is kept.
        require(placedNoteRect(slot, Window, place, size) == to
                    && placedNoteRect(active, Window, place, size) == from,
            "A note's rectangle depended on the card before");
    }

    // Over its window, a note is where Gooseberry draws it: placed, at its
    // share of the window and moved back from an edge; never placed, at the
    // top-right, each older one further left and down.
    require(noteRectOnWindow(Window, QPointF(0.95, 0.95), QSizeF(400, 300), 0) == QRectF(1200, 700, 400, 300),
        "A note at the window's edge was not moved back inside it");
    require(noteRectOnWindow(Window, QPointF(-1, -1), QSizeF(), 0) == QRectF(1600 - 220 - 24, 44, 220, 88),
        "A note never placed did not start at the window's top-right");
    require(noteRectOnWindow(Window, QPointF(-1, -1), QSizeF(), 2) == QRectF(1600 - 220 - 24 - 28, 104, 220, 88),
        "An older note never placed did not start further left and down");
    require(noteRectOnWindow(QSizeF(), QPointF(0.5, 0.5), QSizeF(), 0).isEmpty(),
        "A window of no size gave a note a place");
    // Folded, the dot's 44 px target hangs from the top note's top-right
    // corner, and keeps inside the window.
    require(foldedDotCentre(Window, QRectF(100, 200, 220, 88)) == QPointF(100 + 220 - 22, 222),
        "The dot did not hang from the top note's corner");
    require(foldedDotCentre(Window, QRectF(-30, 990, 40, 80)) == QPointF(22, 1000 - 22),
        "The dot left its window");
    return 0;
}

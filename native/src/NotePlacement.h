/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <algorithm>

namespace Kadunce
{
// Where a Gooseberry note placed on its window stands on that window's card in
// Spread (TETTEGOUCHE-CONTEXT.md § Stuck notes from Gooseberry): the same
// place and size as over the window, both scaled with the card.
struct NotePlacement {
    // A note never resized is drawn over its window as wide as every note,
    // 30% of the window between 140 and 220 px, and about three lines tall,
    // never taller than it is wide.
    static constexpr double StartShare = 0.3;
    static constexpr double StartWidthMin = 140.0;
    static constexpr double StartWidthMax = 220.0;
    static constexpr double StartHeight = 88.0;
    // However small its card, a note stays as big as the corner stack's mini
    // note, so it can still be read as a note and touched.
    static constexpr double SmallestWidth = 64.0;
    static constexpr double SmallestHeight = 44.0;
    // A note never placed starts at the window's top-right, 24 px in from the
    // right and clear of the title bar, each older one 14 px left and 30 px
    // down.
    static constexpr double StartRight = 24.0;
    static constexpr double StartTop = 44.0;
    static constexpr double StartStepX = 14.0;
    static constexpr double StartStepY = 30.0;
    // Folded, a window's notes are one dot, 9 px in radius as the title bar
    // draws it, centred in a 44 px target whose top-right corner is the top
    // note's.
    static constexpr double DotTarget = 44.0;
    static constexpr double DotRadius = 9.0;
};

[[nodiscard]] inline bool notePlaced(const QPointF &place)
{
    return place.x() >= 0.0 && place.y() >= 0.0;
}

[[nodiscard]] inline bool noteSized(const QSizeF &size)
{
    return size.width() > 0.0 && size.height() > 0.0;
}

// How big a note is over its window, in the window's pixels: as resized
// there, or the size every note starts at, never bigger than the window.
[[nodiscard]] inline QSizeF noteSizeOnWindow(const QSizeF &window, const QSizeF &size)
{
    using P = NotePlacement;
    QSizeF onWindow = size;
    if (!noteSized(onWindow)) {
        const double width = std::clamp(window.width() * P::StartShare, P::StartWidthMin, P::StartWidthMax);
        onWindow = QSizeF(width, std::min(width, P::StartHeight));
    }
    return onWindow.boundedTo(window);
}

// The note's rectangle on its card. `place` is its top-left corner as shares
// of the window's width and height; `size` is how big it is over the window,
// in the window's pixels, or empty for the size every note starts at; `window`
// is the window's own size, which the card shows scaled. The note keeps inside
// the card, moved back from an edge as the window's notes are. Empty when the
// note was never placed or the card or window has no size: it then belongs to
// the corner stack.
[[nodiscard]] inline QRectF placedNoteRect(const QRectF &card, const QSizeF &window,
                                           const QPointF &place, const QSizeF &size)
{
    using P = NotePlacement;
    if (!notePlaced(place) || card.isEmpty() || window.isEmpty()) return {};
    const QSizeF onWindow = noteSizeOnWindow(window, size);
    const double scaleX = card.width() / window.width();
    const double scaleY = card.height() / window.height();
    const double width = std::min(card.width(), std::max(P::SmallestWidth, onWindow.width() * scaleX));
    const double height = std::min(card.height(), std::max(P::SmallestHeight, onWindow.height() * scaleY));
    const double x = std::clamp(place.x(), 0.0, 1.0) * card.width();
    const double y = std::clamp(place.y(), 0.0, 1.0) * card.height();
    return QRectF(card.x() + std::min(x, card.width() - width), card.y() + std::min(y, card.height() - height),
                  width, height);
}

// Where a note stands over its window, in the window's own pixels from its
// top-left: where it was placed, or, never placed, where Gooseberry starts
// it, `index` being its place among the window's notes, the top one first.
// Empty when the window has no size.
[[nodiscard]] inline QRectF noteRectOnWindow(const QSizeF &window, const QPointF &place, const QSizeF &size,
                                             int index)
{
    using P = NotePlacement;
    if (window.isEmpty()) return {};
    const QSizeF note = noteSizeOnWindow(window, size);
    const double freeX = std::max(0.0, window.width() - note.width());
    const double freeY = std::max(0.0, window.height() - note.height());
    if (notePlaced(place)) {
        return QRectF(std::min(freeX, std::clamp(place.x(), 0.0, 1.0) * window.width()),
                      std::min(freeY, std::clamp(place.y(), 0.0, 1.0) * window.height()), note.width(),
                      note.height());
    }
    return QRectF(std::max(0.0, window.width() - note.width() - P::StartRight - index * P::StartStepX),
                  std::min(freeY, P::StartTop + index * P::StartStepY), note.width(), note.height());
}

// The centre of the dot a window's notes fold into, in the window's own
// pixels: its target's top-right corner is the top note's, kept inside the
// window. `top` is that note's rectangle over the window.
[[nodiscard]] inline QPointF foldedDotCentre(const QSizeF &window, const QRectF &top)
{
    using P = NotePlacement;
    const double x = std::clamp(top.right() - P::DotTarget, 0.0, std::max(0.0, window.width() - P::DotTarget));
    const double y = std::clamp(top.top(), 0.0, std::max(0.0, window.height() - P::DotTarget));
    return QPointF(x + P::DotTarget / 2, y + P::DotTarget / 2);
}
} // namespace Kadunce

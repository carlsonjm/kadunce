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
};

[[nodiscard]] inline bool notePlaced(const QPointF &place)
{
    return place.x() >= 0.0 && place.y() >= 0.0;
}

[[nodiscard]] inline bool noteSized(const QSizeF &size)
{
    return size.width() > 0.0 && size.height() > 0.0;
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
    QSizeF onWindow = size;
    if (!noteSized(onWindow)) {
        const double width = std::clamp(window.width() * P::StartShare, P::StartWidthMin, P::StartWidthMax);
        onWindow = QSizeF(width, std::min(width, P::StartHeight));
    }
    onWindow = onWindow.boundedTo(window);
    const double scaleX = card.width() / window.width();
    const double scaleY = card.height() / window.height();
    const double width = std::min(card.width(), std::max(P::SmallestWidth, onWindow.width() * scaleX));
    const double height = std::min(card.height(), std::max(P::SmallestHeight, onWindow.height() * scaleY));
    const double x = std::clamp(place.x(), 0.0, 1.0) * card.width();
    const double y = std::clamp(place.y(), 0.0, 1.0) * card.height();
    return QRectF(card.x() + std::min(x, card.width() - width), card.y() + std::min(y, card.height() - height),
                  width, height);
}
} // namespace Kadunce

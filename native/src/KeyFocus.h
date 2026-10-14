/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QList>
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace Kadunce
{
// INPUT.md § Connected: Meta's arrows move the focus to the window beside, on
// the same display or the next one, and with Shift move the window there.
enum class KeyDirection { Left, Right, Up, Down };

// Panes stand a gutter apart, and a frame may reach a little past its slot.
inline constexpr double KeyNeighbourSlack = 24.0;

// The window the arrow reaches from `from`, as an index into `candidates`, or
// -1 with none on that side. A candidate counts only when it lies wholly past
// `from`'s edge on that side. One beside `from`, sharing some of its height
// for Left and Right or its width for Up and Down, comes before one only
// diagonal to it; among them the nearest edge wins, then the nearer middle.
inline int keyNeighbour(const QRectF &from, const QList<QRectF> &candidates, KeyDirection direction)
{
    const bool across = direction == KeyDirection::Left || direction == KeyDirection::Right;
    int best = -1;
    bool bestBeside = false;
    double bestGap = 0.0;
    double bestOff = 0.0;
    for (int i = 0; i < candidates.size(); ++i) {
        const QRectF &to = candidates.at(i);
        if (!to.isValid() || to == from) continue;
        double gap = 0.0;
        switch (direction) {
        case KeyDirection::Left: gap = from.left() - to.right(); break;
        case KeyDirection::Right: gap = to.left() - from.right(); break;
        case KeyDirection::Up: gap = from.top() - to.bottom(); break;
        case KeyDirection::Down: gap = to.top() - from.bottom(); break;
        }
        if (gap < -KeyNeighbourSlack) continue;
        gap = std::max(gap, 0.0);
        const double overlap = across
            ? std::min(from.bottom(), to.bottom()) - std::max(from.top(), to.top())
            : std::min(from.right(), to.right()) - std::max(from.left(), to.left());
        const bool beside = overlap > 0.0;
        const double off = across ? std::abs(to.center().y() - from.center().y())
                                  : std::abs(to.center().x() - from.center().x());
        const bool better = best < 0 || (beside && !bestBeside)
            || (beside == bestBeside && (gap < bestGap - 0.5
                || (std::abs(gap - bestGap) <= 0.5 && off < bestOff)));
        if (!better) continue;
        best = i;
        bestBeside = beside;
        bestGap = gap;
        bestOff = off;
    }
    return best;
}

// Whether `pane` leaves open room between itself and its display's edge on
// that side, a quarter of the display or more: room a pane carried that way
// moves into before it goes on to the next display.
inline bool keyRoomToward(const QRectF &pane, const QRectF &display, KeyDirection direction)
{
    switch (direction) {
    case KeyDirection::Left: return pane.left() - display.left() >= display.width() / 4.0;
    case KeyDirection::Right: return display.right() - pane.right() >= display.width() / 4.0;
    case KeyDirection::Up: return pane.top() - display.top() >= display.height() / 4.0;
    case KeyDirection::Down: return display.bottom() - pane.bottom() >= display.height() / 4.0;
    }
    return false;
}
} // namespace Kadunce

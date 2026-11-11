/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "BentoColumns.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace Kadunce {

// CARD-LIFECYCLE.md §11, Connected: a card carried in from another display
// over a pane names one of that pane's five places: its middle joins its
// Stack, and each of its four sides places the card beside it there.

// The middle is this share of the pane each way, centred: large enough to be
// found by touch, small enough to leave each side a band to aim at.
inline constexpr double PanePlaceMiddleShare = 0.4;
// A contact within this many pixels of a boundary keeps the place it named
// before, so a hand resting there does not flicker between two.
inline constexpr double PanePlaceSettle = 12.0;
// A card let go on the middle shrinks into the pane and fades for this long
// before it joins the Stack, and the pane then fans for this long, so the
// join is seen.
inline constexpr int StackJoinShrinkMs = 240;
inline constexpr int StackJoinFanMs = 360;

// The place a contact at (x, y) names over a pane at `pane`, or none outside
// it. Outside the middle, the nearest side wins: the pane is read as four
// triangles meeting at its centre, so every side is reachable across its
// whole length, a short side as well as a long one.
inline std::optional<PanePlace> panePlaceAt(const BentoRect &pane, double x, double y,
                                            std::optional<PanePlace> previous = std::nullopt)
{
    if (pane.width <= 0.0 || pane.height <= 0.0 || x < pane.x || y < pane.y
        || x >= pane.x + pane.width || y >= pane.y + pane.height) return std::nullopt;
    const double cx = pane.x + pane.width / 2.0, cy = pane.y + pane.height / 2.0;
    const double halfMiddleW = pane.width * PanePlaceMiddleShare / 2.0;
    const double halfMiddleH = pane.height * PanePlaceMiddleShare / 2.0;
    const double dx = x - cx, dy = y - cy;
    // Normalised to the pane's own shape, so its diagonals divide it.
    const double nx = dx / (pane.width / 2.0), ny = dy / (pane.height / 2.0);
    const auto side = [&] {
        if (std::abs(nx) >= std::abs(ny)) return nx < 0.0 ? PanePlace::Left : PanePlace::Right;
        return ny < 0.0 ? PanePlace::Top : PanePlace::Bottom;
    };
    const bool inMiddle = std::abs(dx) < halfMiddleW && std::abs(dy) < halfMiddleH;
    const PanePlace place = inMiddle ? PanePlace::Middle : side();
    if (!previous || *previous == place) return place;
    // Near the middle's edge, or near a diagonal, the place named before holds.
    const double toMiddle = std::max(std::abs(dx) - halfMiddleW, std::abs(dy) - halfMiddleH);
    const bool nearMiddle = std::abs(toMiddle) <= PanePlaceSettle;
    const double toDiagonal = std::abs(std::abs(nx) - std::abs(ny))
        * std::min(pane.width, pane.height) / 2.0;
    const bool nearDiagonal = !inMiddle && toDiagonal <= PanePlaceSettle;
    if ((*previous == PanePlace::Middle || place == PanePlace::Middle) ? nearMiddle : nearDiagonal)
        return *previous;
    return place;
}

// A Connected layout is a row of columns (BentoColumns.h). A card placed at
// a pane's side joins that pane's column, over or under it, the column's
// panes sharing its height; at the pane's left or right it stands in a new
// full-height column beside the pane's column, the columns sharing the
// width as they always do. False when the layout does not hold `pane`.
template<class Handle>
bool bentoColumnsPlaceBy(BentoColumns<Handle> &layout, const Handle &pane, PanePlace place,
                         const Handle &card)
{
    if (place == PanePlace::Middle || pane == card) return false;
    bentoColumnsRemove(layout, card);
    for (std::size_t c = 0; c < layout.columns.size(); ++c) {
        auto &column = layout.columns[c];
        for (std::size_t s = 0; s < column.size(); ++s) {
            if (column[s].open || column[s].window != pane) continue;
            const BentoColumnSlot<Handle> slot{card, false};
            switch (place) {
            case PanePlace::Top:
                // Open room just above the pane is taken rather than kept.
                if (s > 0 && column[s - 1].open) column[s - 1] = slot;
                else column.insert(column.begin() + std::ptrdiff_t(s), slot);
                return true;
            case PanePlace::Bottom:
                if (s + 1 < column.size() && column[s + 1].open) column[s + 1] = slot;
                else column.insert(column.begin() + std::ptrdiff_t(s + 1), slot);
                return true;
            case PanePlace::Left:
            case PanePlace::Right: {
                // A lone column has the other half of the display open; a
                // card beside it on that side fills it.
                const bool right = place == PanePlace::Right;
                if (layout.columns.size() == 1 && layout.alignRight != right) layout.alignRight = right;
                layout.columns.insert(layout.columns.begin() + std::ptrdiff_t(right ? c + 1 : c),
                                      std::vector<BentoColumnSlot<Handle>>{slot});
                return true;
            }
            case PanePlace::Middle:
                return false;
            }
        }
    }
    return false;
}

// Where the card would stand, as a share of the display, were it placed at
// `place` by `pane`; none where the layout does not hold the pane.
template<class Handle>
[[nodiscard]] std::optional<BentoRect> bentoColumnsPlaceByRect(BentoColumns<Handle> layout,
    const Handle &pane, PanePlace place, const Handle &card)
{
    if (!bentoColumnsPlaceBy(layout, pane, place, card)) return std::nullopt;
    std::vector<Handle> handles;
    std::vector<BentoRect> rects;
    bentoColumnsLayout(layout, handles, rects);
    for (std::size_t i = 0; i < handles.size(); ++i)
        if (handles[i] == card) return rects[i];
    return std::nullopt;
}

} // namespace Kadunce

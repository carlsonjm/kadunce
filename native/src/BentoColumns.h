/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BentoLayout.h"
#include "BentoSidePlacement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a layout a side snap placed, or a pair
// began, is a row of columns, left to right, each holding its cards top to
// bottom. A column's top slot may be open room, where the desktop shows; a
// lone column stands in the half of the display it was placed on, and the
// other half is open too.
template<class Handle>
struct BentoColumnSlot {
    Handle window{};
    bool open = false;
    bool operator==(const BentoColumnSlot &) const = default;
};

template<class Handle>
struct BentoColumns {
    std::vector<std::vector<BentoColumnSlot<Handle>>> columns;
    bool alignRight = false;
    // Begun by pairing a pick or an opened window with the card in use, and
    // placed by no side snap since. Nothing placed these columns, so they
    // leave no room open: down to one column the layout is laid out whole
    // again, and down to one pane it ends (§5).
    bool paired = false;
    bool operator==(const BentoColumns &) const = default;
};

template<class Handle>
[[nodiscard]] bool bentoColumnsHold(const BentoColumns<Handle> &layout, const Handle &window)
{
    for (const auto &column : layout.columns)
        for (const auto &slot : column)
            if (!slot.open && slot.window == window) return true;
    return false;
}

// Takes every card `keep` refuses out of the layout. A column left empty, or
// holding only open room, goes; when one column is left it stands on the side
// it already stood.
template<class Handle, class Keep>
void bentoColumnsKeep(BentoColumns<Handle> &layout, Keep keep)
{
    const std::size_t before = layout.columns.size();
    int firstGone = -1;
    for (std::size_t index = 0; index < layout.columns.size();) {
        auto &column = layout.columns[index];
        std::erase_if(column, [&keep](const auto &slot) { return !slot.open && !keep(slot.window); });
        const bool empty = std::none_of(column.begin(), column.end(),
            [](const auto &slot) { return !slot.open; });
        if (empty) {
            if (firstGone < 0) firstGone = int(index);
            layout.columns.erase(layout.columns.begin() + index);
        } else {
            ++index;
        }
    }
    if (before > 1 && layout.columns.size() == 1) layout.alignRight = firstGone == 0;
}

template<class Handle>
void bentoColumnsRemove(BentoColumns<Handle> &layout, const Handle &window)
{
    bentoColumnsKeep(layout, [&window](const Handle &held) { return held != window; });
}

// The edge column on a side: the first or last column, or none when a lone
// column stands on the other half.
template<class Handle>
[[nodiscard]] int bentoEdgeColumn(const BentoColumns<Handle> &layout, bool right)
{
    if (layout.columns.empty()) return -1;
    if (layout.columns.size() == 1) return layout.alignRight == right ? 0 : -1;
    return right ? int(layout.columns.size()) - 1 : 0;
}

// A side snap puts `window` at a place: beside, a new full-height column at
// that edge, the others moving inward; under, the bottom of that edge's
// column, what it holds moving up into any room open above. With no column on
// that side, under stands in its lower half with the room above it open.
template<class Handle>
void bentoColumnsPlace(BentoColumns<Handle> &layout, const Handle &window,
                       BentoSidePlacement choice)
{
    bentoColumnsRemove(layout, window);
    const BentoColumnSlot<Handle> card{window, false};
    const int edge = bentoEdgeColumn(layout, choice.right);
    if (!choice.large && edge >= 0) {
        auto &column = layout.columns[edge];
        if (!column.empty() && column.front().open) column.erase(column.begin());
        column.push_back(card);
        return;
    }
    std::vector<BentoColumnSlot<Handle>> fresh;
    if (!choice.large) fresh.push_back({Handle{}, true});
    fresh.push_back(card);
    if (layout.columns.empty()) layout.alignRight = choice.right;
    if (choice.right) layout.columns.push_back(std::move(fresh));
    else layout.columns.insert(layout.columns.begin(), std::move(fresh));
}

// A card arriving with no side named fills open room: the open top of a
// column first, then the half beside a lone column. False when no room is
// open.
template<class Handle>
bool bentoColumnsFill(BentoColumns<Handle> &layout, const Handle &window)
{
    for (auto &column : layout.columns)
        for (auto &slot : column)
            if (slot.open) { slot = {window, false}; return true; }
    if (layout.columns.size() != 1) return false;
    std::vector<BentoColumnSlot<Handle>> fresh{{window, false}};
    if (layout.alignRight) layout.columns.insert(layout.columns.begin(), std::move(fresh));
    else layout.columns.push_back(std::move(fresh));
    return true;
}

// A card arriving with no room open joins the bottom of the column holding
// fewest cards, the rightmost of those, so the layout keeps its columns.
template<class Handle>
void bentoColumnsJoin(BentoColumns<Handle> &layout, const Handle &window)
{
    if (layout.columns.empty()) {
        layout.columns.push_back({{window, false}});
        return;
    }
    std::size_t fewest = layout.columns.size() - 1;
    for (std::size_t c = layout.columns.size(); c-- > 0;)
        if (layout.columns[c].size() < layout.columns[fewest].size()) fewest = c;
    layout.columns[fewest].push_back({window, false});
}

// Cards in no row of columns yet, dealt across as many columns as rows,
// left to right, so a side snap has columns to place among.
template<class Handle>
[[nodiscard]] BentoColumns<Handle> bentoColumnsDealt(const std::vector<Handle> &handles)
{
    BentoColumns<Handle> layout;
    if (handles.empty()) return layout;
    const std::size_t count = std::size_t(std::ceil(std::sqrt(double(handles.size()))));
    layout.columns.resize(count);
    for (std::size_t i = 0; i < handles.size(); ++i)
        layout.columns[i % count].push_back({handles[i], false});
    return layout;
}

// Each card's place, in the order `handles` lists them. Columns share the width
// evenly and a column's slots its height; a lone column takes half.
template<class Handle>
void bentoColumnsLayout(const BentoColumns<Handle> &layout, std::vector<Handle> &handles,
                        std::vector<BentoRect> &rects)
{
    handles.clear();
    rects.clear();
    const std::size_t count = layout.columns.size();
    if (count == 0) return;
    const double width = count == 1 ? 0.5 : 1.0 / double(count);
    for (std::size_t c = 0; c < count; ++c) {
        const auto &column = layout.columns[c];
        const double x = count == 1 ? (layout.alignRight ? 0.5 : 0.0) : double(c) * width;
        const double height = 1.0 / double(std::max<std::size_t>(1, column.size()));
        for (std::size_t s = 0; s < column.size(); ++s) {
            if (column[s].open) continue;
            handles.push_back(column[s].window);
            rects.push_back({x, double(s) * height, width, height});
        }
    }
}

// Reads a layout already on screen as columns, when it is one: panes sharing a
// column's left and right edges, stacked without gaps from top to bottom and
// filling the display between them. Nothing else is a row of columns.
template<class Handle>
[[nodiscard]] std::optional<BentoColumns<Handle>> bentoColumnsFromRects(
    const std::vector<Handle> &handles, const std::vector<BentoRect> &rects)
{
    if (handles.empty() || handles.size() != rects.size()) return std::nullopt;
    constexpr double tolerance = .02;
    std::vector<std::size_t> order(handles.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&rects](std::size_t a, std::size_t b) {
        return std::abs(rects[a].x - rects[b].x) > tolerance ? rects[a].x < rects[b].x
                                                             : rects[a].y < rects[b].y;
    });
    BentoColumns<Handle> layout;
    double expectX = 0.0, columnX = -1.0, columnWidth = 0.0, expectY = 0.0;
    for (const std::size_t i : order) {
        const auto &r = rects[i];
        if (columnX < 0 || std::abs(r.x - columnX) > tolerance) {
            if (columnX >= 0 && std::abs(expectY - 1.0) > tolerance) return std::nullopt;
            if (std::abs(r.x - expectX) > tolerance) return std::nullopt;
            layout.columns.push_back({});
            columnX = r.x;
            columnWidth = r.width;
            expectX = r.x + r.width;
            expectY = 0.0;
        }
        if (std::abs(r.width - columnWidth) > tolerance || std::abs(r.y - expectY) > tolerance)
            return std::nullopt;
        layout.columns.back().push_back({handles[i], false});
        expectY = r.y + r.height;
    }
    if (std::abs(expectY - 1.0) > tolerance || std::abs(expectX - 1.0) > tolerance)
        return std::nullopt;
    return layout;
}
} // namespace Kadunce

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BentoLayout.h"
#include "BentoSidePlacement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace Kadunce
{
// CARD-LIFECYCLE.md §11, Connected: a layout a side snap placed, or a pair
// began, is a row of columns, left to right, each holding its cards top to
// bottom. A column's top or bottom slot may be open room, where the desktop
// shows; a lone column stands in the half of the display it was placed on,
// and the other half is open too.
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
    // A share of the width a wheel gave the column a card holds, oldest
    // first. The columns with none share what is left evenly.
    std::vector<std::pair<Handle, double>> shares;
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
    std::erase_if(layout.shares, [&layout](const auto &share) {
        return !bentoColumnsHold(layout, share.first);
    });
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
// column, what it holds moving up into any room open above; over, the top of
// that column, taking the room open there or moving what it holds down. With
// no column on that side, under stands in its lower half and over in its
// upper half, the rest of that half open.
template<class Handle>
void bentoColumnsPlace(BentoColumns<Handle> &layout, const Handle &window,
                       BentoSidePlacement choice)
{
    bentoColumnsRemove(layout, window);
    if (choice.connected && choice.share > 0.0) layout.shares.push_back({window, choice.share});
    const BentoColumnSlot<Handle> card{window, false};
    const int edge = bentoEdgeColumn(layout, choice.right);
    if (choice.over && edge >= 0) {
        auto &column = layout.columns[edge];
        if (!column.empty() && column.front().open) column.front() = card;
        else column.insert(column.begin(), card);
        return;
    }
    if (!choice.large && !choice.over && edge >= 0) {
        auto &column = layout.columns[edge];
        if (!column.empty() && column.front().open) column.erase(column.begin());
        column.push_back(card);
        return;
    }
    std::vector<BentoColumnSlot<Handle>> fresh;
    if (!choice.large && !choice.over) fresh.push_back({Handle{}, true});
    fresh.push_back(card);
    if (choice.over) fresh.push_back({Handle{}, true});
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

// Each column's share of the width, left to right. A column a wheel gave a
// share keeps it, the newest given first; the rest share what is left evenly,
// and while that leaves a free column under a tenth, or none at all, the
// oldest share gives way. A lone column takes its share, or half.
template<class Handle>
[[nodiscard]] std::vector<double> bentoColumnsWidths(const BentoColumns<Handle> &layout)
{
    const std::size_t count = layout.columns.size();
    std::vector<double> given(count, 0.0);
    std::vector<std::size_t> order; // Columns holding a share, oldest first.
    for (const auto &[window, share] : layout.shares) {
        for (std::size_t c = 0; c < count; ++c) {
            const auto &column = layout.columns[c];
            if (std::none_of(column.begin(), column.end(), [&window](const auto &slot) {
                    return !slot.open && slot.window == window;
                })) continue;
            std::erase(order, c);
            order.push_back(c);
            given[c] = std::clamp(share, .05, .95);
        }
    }
    if (count == 1) return {given[0] > 0.0 ? given[0] : 0.5};
    for (std::size_t dropped = 0;; ++dropped) {
        double fixed = 0.0;
        std::size_t free = 0;
        for (std::size_t c = 0; c < count; ++c) given[c] > 0.0 ? void(fixed += given[c]) : void(++free);
        if (free > 0 && (1.0 - fixed) / double(free) >= .1) {
            std::vector<double> widths(count);
            for (std::size_t c = 0; c < count; ++c)
                widths[c] = given[c] > 0.0 ? given[c] : (1.0 - fixed) / double(free);
            return widths;
        }
        if (dropped >= order.size()) return std::vector<double>(count, 1.0 / double(count));
        given[order[dropped]] = 0.0;
    }
}

// Each card's place, in the order `handles` lists them. Columns share the
// width as bentoColumnsWidths gives it, and a column's slots its height.
template<class Handle>
void bentoColumnsLayout(const BentoColumns<Handle> &layout, std::vector<Handle> &handles,
                        std::vector<BentoRect> &rects)
{
    handles.clear();
    rects.clear();
    const std::size_t count = layout.columns.size();
    if (count == 0) return;
    const std::vector<double> widths = bentoColumnsWidths(layout);
    double x = count == 1 && layout.alignRight ? 1.0 - widths[0] : 0.0;
    for (std::size_t c = 0; c < count; ++c) {
        const auto &column = layout.columns[c];
        const double height = 1.0 / double(std::max<std::size_t>(1, column.size()));
        for (std::size_t s = 0; s < column.size(); ++s) {
            if (column[s].open) continue;
            handles.push_back(column[s].window);
            rects.push_back({x, double(s) * height, widths[c], height});
        }
        x += widths[c];
    }
}

// The room a layout leaves open, where the desktop shows, in the order
// bentoColumnsFill fills it: each open slot, then beside a lone column the
// rest of the display.
template<class Handle>
[[nodiscard]] std::vector<BentoRect> bentoColumnsOpenRooms(const BentoColumns<Handle> &layout)
{
    std::vector<BentoRect> rooms;
    const std::size_t count = layout.columns.size();
    if (count == 0 || layout.paired) return rooms;
    const std::vector<double> widths = bentoColumnsWidths(layout);
    double x = count == 1 && layout.alignRight ? 1.0 - widths[0] : 0.0;
    for (std::size_t c = 0; c < count; ++c) {
        const auto &column = layout.columns[c];
        const double height = 1.0 / double(std::max<std::size_t>(1, column.size()));
        for (std::size_t s = 0; s < column.size(); ++s)
            if (column[s].open) rooms.push_back({x, double(s) * height, widths[c], height});
        x += widths[c];
    }
    if (count == 1)
        rooms.push_back({layout.alignRight ? 0.0 : widths[0], 0.0, 1.0 - widths[0], 1.0});
    return rooms;
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

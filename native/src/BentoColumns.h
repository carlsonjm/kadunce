/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BentoLayout.h"
#include "BentoSidePlacement.h"
#include "NaturalSize.h"

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
    // Placed at the widths their cards want (NaturalSize.h): while every
    // column holds a share and the shares leave some of the width, the rest
    // stays open beside them, where the desktop shows.
    bool natural = false;
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

// Gives the column `column` the share `share` of the width, as the wheel
// gives the column a card holds one.
template<class Handle>
void bentoColumnsSetShare(BentoColumns<Handle> &layout, std::size_t column, double share)
{
    if (column >= layout.columns.size()) return;
    const auto &held = layout.columns[column];
    std::erase_if(layout.shares, [&held](const auto &given) {
        return std::any_of(held.begin(), held.end(), [&given](const auto &slot) {
            return !slot.open && slot.window == given.first;
        });
    });
    for (const auto &slot : held)
        if (!slot.open) { layout.shares.push_back({slot.window, share}); return; }
}

// Takes every card `keep` refuses out of the layout. A column left empty, or
// holding only open room, goes; when one column is left it stands on the side
// it already stood.
template<class Handle, class Keep>
void bentoColumnsKeep(BentoColumns<Handle> &layout, Keep keep)
{
    const std::size_t before = layout.columns.size();
    // Natural columns keep their widths through a card leaving, the share
    // passing to a card left in the column; a column left with only open
    // room goes, and leaves its width open.
    std::vector<double> widths = layout.natural ? bentoColumnsWidths(layout) : std::vector<double>{};
    int firstGone = -1;
    for (std::size_t index = 0; index < layout.columns.size();) {
        auto &column = layout.columns[index];
        // §11, Connected: in natural columns a card's room opens as it leaves.
        if (layout.natural) {
            for (auto &slot : column)
                if (!slot.open && !keep(slot.window)) slot = {Handle{}, true};
        } else {
            std::erase_if(column, [&keep](const auto &slot) { return !slot.open && !keep(slot.window); });
        }
        const bool empty = std::none_of(column.begin(), column.end(),
            [](const auto &slot) { return !slot.open; });
        if (empty) {
            if (firstGone < 0) firstGone = int(index);
            layout.columns.erase(layout.columns.begin() + index);
            if (layout.natural) widths.erase(widths.begin() + index);
        } else {
            ++index;
        }
    }
    if (before > 1 && layout.columns.size() == 1) layout.alignRight = firstGone == 0;
    std::erase_if(layout.shares, [&layout](const auto &share) {
        return !bentoColumnsHold(layout, share.first);
    });
    if (layout.natural)
        for (std::size_t c = 0; c < layout.columns.size(); ++c) bentoColumnsSetShare(layout, c, widths[c]);
}

// Four cards, with no room open and no column widened by the wheel, as two
// columns of two, read left to right and top to bottom; none otherwise.
template<class Handle>
[[nodiscard]] std::optional<BentoColumns<Handle>> bentoColumnsQuartered(const BentoColumns<Handle> &layout)
{
    if (!layout.shares.empty()) return std::nullopt;
    std::vector<BentoColumnSlot<Handle>> cards;
    for (const auto &column : layout.columns)
        for (const auto &slot : column) {
            if (slot.open) return std::nullopt;
            cards.push_back(slot);
        }
    if (cards.size() != 4) return std::nullopt;
    BentoColumns<Handle> quarters = layout;
    quarters.columns = {{cards[0], cards[1]}, {cards[2], cards[3]}};
    if (quarters == layout) return std::nullopt;
    return quarters;
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
    if (layout.natural && std::all_of(given.cbegin(), given.cend(), [](double g) { return g > 0.0; })) {
        double total = 0.0;
        for (const double g : given) total += g;
        if (total <= 1.0 + 1e-6) return given;
    }
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

// Where the first column starts: a lone column, or natural columns leaving
// room, stand on the side they were placed on, the room on the other.
[[nodiscard]] inline double bentoColumnsStart(const std::vector<double> &widths, bool alignRight)
{
    double total = 0.0;
    for (const double width : widths) total += width;
    return alignRight && total < 1.0 - 1e-6 ? 1.0 - total : 0.0;
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
    double x = bentoColumnsStart(widths, layout.alignRight);
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
    const double start = bentoColumnsStart(widths, layout.alignRight);
    double x = start;
    for (std::size_t c = 0; c < count; ++c) {
        const auto &column = layout.columns[c];
        const double height = 1.0 / double(std::max<std::size_t>(1, column.size()));
        for (std::size_t s = 0; s < column.size(); ++s)
            if (column[s].open) rooms.push_back({x, double(s) * height, widths[c], height});
        x += widths[c];
    }
    const double left = 1.0 - (x - start);
    if (left > 1e-6) rooms.push_back({start > 0.0 ? 0.0 : x, 0.0, left, 1.0});
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
// Room left beside natural columns narrower than this goes to them rather
// than standing open.
inline constexpr double NaturalSliver = .1;

// Turns standing columns natural without moving any: each keeps the width it
// stands at as its share.
template<class Handle>
void bentoColumnsMakeNatural(BentoColumns<Handle> &layout)
{
    if (layout.natural) return;
    const std::vector<double> widths = bentoColumnsWidths(layout);
    for (std::size_t c = 0; c < layout.columns.size(); ++c) bentoColumnsSetShare(layout, c, widths[c]);
    layout.natural = true;
    layout.paired = false;
}

// Turns standing columns natural at the widths their cards want
// (NaturalSize.h), not the even shares they stood at: each column takes the
// most a card in it wants, a column of no known want keeping its width and one
// in `keep` the share it was given. Too wide together, the columns wanting
// more than their least give way in proportion; room left open narrower than
// NaturalSliver goes to the widest column. Where no card's want is known
// nothing changes.
template<class Handle, class NeedOf>
void bentoColumnsSettleNatural(BentoColumns<Handle> &layout, NeedOf needOf, const Handle &keep = Handle{})
{
    if (layout.natural || layout.columns.empty()) return;
    const std::vector<double> widths = bentoColumnsWidths(layout);
    const std::size_t count = layout.columns.size();
    std::vector<double> want(count, 0.0), least(count, 0.0);
    std::vector<bool> fixed(count, false);
    bool known = false;
    for (std::size_t c = 0; c < count; ++c) {
        for (const auto &slot : layout.columns[c]) {
            if (slot.open) continue;
            const NaturalNeed held = needOf(slot.window);
            known = known || held.width > 0.0;
            want[c] = std::max(want[c], held.width);
            least[c] = std::max(least[c], held.leastWidth);
            if (keep && slot.window == keep) fixed[c] = true;
        }
        if (fixed[c]) {
            for (const auto &[window, share] : layout.shares)
                for (const auto &slot : layout.columns[c])
                    if (!slot.open && slot.window == window) want[c] = share;
        }
        if (want[c] <= 0.0) { want[c] = widths[c]; fixed[c] = true; }
        want[c] = std::max(want[c], least[c]);
    }
    // Columns of no known want stand as they stood, not natural.
    if (!known) return;
    double total = 0.0;
    for (const double w : want) total += w;
    if (total > 1.0 + 1e-6) {
        double spare = 0.0;
        for (std::size_t c = 0; c < count; ++c)
            if (!fixed[c]) spare += std::max(0.0, want[c] - least[c]);
        const double over = total - 1.0;
        for (std::size_t c = 0; c < count && spare > 0.0; ++c)
            if (!fixed[c])
                want[c] -= std::max(0.0, want[c] - least[c]) * std::min(1.0, over / spare);
        total = 0.0;
        for (const double w : want) total += w;
        // Still too wide: the columns stand as they stood.
        if (total > 1.0 + 1e-6) return;
    }
    if (total < 1.0 - 1e-6 && 1.0 - total < NaturalSliver)
        want[std::size_t(std::distance(want.begin(), std::max_element(want.begin(), want.end())))] += 1.0 - total;
    for (std::size_t c = 0; c < count; ++c) bentoColumnsSetShare(layout, c, want[c]);
    layout.natural = true;
    layout.paired = false;
}

// §11, Connected: the wheel turned while a pane is held away from the edges
// gives its column `share` of the width. It takes room open beside the
// columns first, then width from the others, none narrower than its cards
// allow; `leastOf` answers the least share of the width a card allows. False
// when the layout does not hold `window`.
template<class Handle, class LeastOf>
bool bentoColumnsResize(BentoColumns<Handle> &layout, const Handle &window, double share, LeastOf leastOf)
{
    int at = -1;
    for (std::size_t c = 0; c < layout.columns.size() && at < 0; ++c)
        for (const auto &slot : layout.columns[c])
            if (!slot.open && slot.window == window) { at = int(c); break; }
    if (at < 0) return false;
    bentoColumnsMakeNatural(layout);
    const std::vector<double> widths = bentoColumnsWidths(layout);
    const auto leastOfColumn = [&](std::size_t c) {
        double least = 0.0;
        for (const auto &slot : layout.columns[c])
            if (!slot.open) least = std::max(least, leastOf(slot.window));
        return least;
    };
    double others = 0.0, othersLeast = 0.0;
    for (std::size_t c = 0; c < widths.size(); ++c) {
        if (int(c) == at) continue;
        others += widths[c];
        othersLeast += leastOfColumn(c);
    }
    share = std::clamp(share, leastOfColumn(at), std::max(leastOfColumn(at), 1.0 - othersLeast));
    const double over = share + others - 1.0;
    if (over > 1e-6) {
        double spare = 0.0;
        for (std::size_t c = 0; c < widths.size(); ++c)
            if (int(c) != at) spare += std::max(0.0, widths[c] - leastOfColumn(c));
        for (std::size_t c = 0; c < widths.size(); ++c) {
            if (int(c) == at || spare <= 0.0) continue;
            const double give = std::max(0.0, widths[c] - leastOfColumn(c)) * std::min(1.0, over / spare);
            bentoColumnsSetShare(layout, c, widths[c] - give);
        }
    }
    bentoColumnsSetShare(layout, std::size_t(at), share);
    return true;
}

enum class NaturalFit { None, Filled, Opened, Squeezed, Joined };

// CARD-LIFECYCLE.md §11, Connected: a card arriving with no place named fits
// in at its natural size (NaturalSize.h). In order: room open in a column it
// fits; room open beside the columns, where it takes its own width, a short
// window the top of a column of its own with the room under it left open;
// width the columns stand at beyond what their cards want; the foot of a
// column of short windows that holds one more. `needOf` answers what a card
// already standing wants; `near` is the column of the card in use, which an
// arrival stands as close to as it can. Nothing changes when none fits.
template<class Handle, class NeedOf>
NaturalFit bentoColumnsFitNatural(BentoColumns<Handle> &layout, const Handle &window,
                                  const NaturalNeed &need, NeedOf needOf, int near = -1)
{
    bentoColumnsSettleNatural(layout, needOf);
    const std::size_t count = layout.columns.size();
    const std::vector<double> widths = bentoColumnsWidths(layout);
    std::vector<std::size_t> order(count);
    for (std::size_t c = 0; c < count; ++c) order[c] = c;
    if (near >= 0)
        std::stable_sort(order.begin(), order.end(), [near](std::size_t a, std::size_t b) {
            return std::abs(int(a) - near) < std::abs(int(b) - near);
        });
    double used = 0.0;
    for (const double width : widths) used += width;
    const double room = std::max(0.0, 1.0 - used);
    for (const std::size_t c : order) {
        auto &column = layout.columns[c];
        const double height = 1.0 / double(std::max<std::size_t>(1, column.size()));
        if (widths[c] + 1e-6 < need.leastWidth || height + 1e-6 < need.leastHeight) continue;
        for (auto &slot : column) {
            if (!slot.open) continue;
            slot = {window, false};
            // A column narrower than its new card wants widens into room
            // open beside it, leaving none too thin to use.
            if (need.width > widths[c] && room > 0.0) {
                double grown = std::min(room, need.width - widths[c]);
                if (room - grown < NaturalSliver) grown = room;
                bentoColumnsSetShare(layout, c, widths[c] + grown);
            }
            return NaturalFit::Filled;
        }
    }
    // A window of no known kind asks an even share, as the columns stand.
    const double wanted = need.width > 0.0 ? need.width
        : std::max(need.leastWidth, 1.0 / double(count + 1));
    // What each column stands at beyond the most its cards want.
    std::vector<double> spare(count, 0.0);
    double spareTotal = 0.0;
    for (std::size_t c = 0; c < count; ++c) {
        double most = 0.0;
        for (const auto &slot : layout.columns[c]) {
            if (slot.open) continue;
            const NaturalNeed held = needOf(slot.window);
            most = std::max({most, held.width, held.leastWidth});
        }
        spare[c] = most > 0.0 ? std::max(0.0, widths[c] - most) : 0.0;
        spareTotal += spare[c];
    }
    if (room + 1e-6 >= need.leastWidth || room + spareTotal + 1e-6 >= need.leastWidth) {
        double share = std::min(wanted, room + spareTotal);
        if (room + spareTotal - share < NaturalSliver) share = room + spareTotal;
        const double taken = std::max(0.0, share - room);
        const NaturalFit fit = taken > 1e-6 ? NaturalFit::Squeezed : NaturalFit::Opened;
        if (taken > 1e-6)
            for (std::size_t c = 0; c < count; ++c)
                if (spare[c] > 0.0) bentoColumnsSetShare(layout, c, widths[c] - taken * spare[c] / spareTotal);
        std::vector<BentoColumnSlot<Handle>> fresh{{window, false}};
        if (need.shortWindow()) fresh.push_back({Handle{}, true});
        if (layout.columns.empty()) layout.alignRight = false;
        if (layout.alignRight) layout.columns.insert(layout.columns.begin(), std::move(fresh));
        else layout.columns.push_back(std::move(fresh));
        layout.shares.push_back({window, share});
        return fit;
    }
    if (!need.shortWindow()) return NaturalFit::None;
    int best = -1;
    for (const std::size_t c : order) {
        const auto &column = layout.columns[c];
        if (widths[c] + 1e-6 < need.leastWidth) continue;
        const double height = 1.0 / double(column.size() + 1);
        bool fits = height + 1e-6 >= need.leastHeight;
        for (const auto &slot : column) {
            if (slot.open) continue;
            const NaturalNeed held = needOf(slot.window);
            fits = fits && held.shortWindow() && height + 1e-6 >= held.leastHeight;
        }
        if (fits && (best < 0 || column.size() < layout.columns[best].size())) best = int(c);
    }
    if (best < 0) return NaturalFit::None;
    layout.columns[best].push_back({window, false});
    return NaturalFit::Joined;
}
} // namespace Kadunce

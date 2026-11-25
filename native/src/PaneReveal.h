/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>

namespace Kadunce {

// A window that opens into a layout (CARD-LIFECYCLE.md §8) is asked for its
// pane's size the moment it exists, and its client draws that size a little
// later: a terminal in about 20 ms, a mail client in about 100. Until then
// its only picture is the one it opened with, at its own size in the middle
// of the display. That picture is never shown. The window stays unseen, and
// the panes that make room for it stand still, until its client has drawn
// the pane or `OpenedUnseenCap` milliseconds have passed. The panes then
// draw back from over it, each a box cropping the picture it kept, and the
// new window stands behind them in its place, seen only where they have
// left.
inline constexpr int OpenedUnseenCap = 150;

struct RevealBox {
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    [[nodiscard]] double right() const { return x + width; }
    [[nodiscard]] double bottom() const { return y + height; }
    [[nodiscard]] bool empty() const { return width <= 0.0 || height <= 0.0; }
};

// Whether an opened window's client has drawn the place it was asked for:
// its frame stands where it was asked to, at the size asked for or at
// another size than it opened with (a terminal keeping whole rows stops
// short of its pane, and that is its answer).
inline bool openedStandsInPlace(double openedWidth, double openedHeight,
                                const RevealBox &frame, const RevealBox &asked)
{
    const auto near = [](double a, double b, double within) { return std::abs(a - b) < within; };
    if (!near(frame.x, asked.x, 2.0) || !near(frame.y, asked.y, 2.0)) return false;
    if (near(frame.width, asked.width, 2.0) && near(frame.height, asked.height, 2.0)) return true;
    return !near(frame.width, openedWidth, 1.0) || !near(frame.height, openedHeight, 1.0);
}

// Whether the opened window is still unseen, `waited` milliseconds after it
// opened.
inline bool openedStaysUnseen(bool inPlace, long long waited)
{
    return !inPlace && waited < OpenedUnseenCap;
}

// A window that opens with no pane over its place has nothing to be shown
// from behind, so it comes up in place over `OpenedAloneFade` milliseconds
// from when it was first seen, quickly at first.
inline constexpr int OpenedAloneFade = 120;

inline double openedAloneOpacity(long long sinceSeen)
{
    const double left = 1.0 - std::clamp(double(sinceSeen) / OpenedAloneFade, 0.0, 1.0);
    return 1.0 - left * left;
}

// A window that opens as the one card a display shows is unseen in the same
// way, and the card shown before it stays until then. It then comes up over
// that card in `OpenedCardRise` milliseconds, from 90% of its size about its
// middle, its opacity rising evenly; the card under it leaves once it is whole.
// Opened from the dock, it grows out of its icon instead, in `OpenedCardGrow`
// milliseconds, whole within the first `OpenedCardGrowFade`.
inline constexpr int OpenedCardRise = 150;
inline constexpr int OpenedCardGrow = 300;
inline constexpr int OpenedCardGrowFade = 60;

inline double openedCardRise(long long sinceSeen, int duration = OpenedCardRise)
{
    return std::clamp(double(sinceSeen) / std::max(1, duration), 0.0, 1.0);
}

inline double openedCardScale(double rise)
{
    const double left = 1.0 - std::clamp(rise, 0.0, 1.0);
    return 0.9 + 0.1 * (1.0 - left * left * left);
}

// Where a card growing out of `icon` into `card` stands, `t` of the way (eased).
inline RevealBox openedCardFromIcon(const RevealBox &icon, const RevealBox &card, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return RevealBox{mix(icon.x, card.x), mix(icon.y, card.y), mix(icon.width, card.width), mix(icon.height, card.height)};
}

// An opened window is given the keys as it opens. The keys leaving it while
// it is unseen, or within `OpenedKeysWatched` milliseconds of its showing,
// as long as its coming into view takes, are named in the journal.
inline constexpr int OpenedKeysWatched = 250;

// What shows of a window standing in `pane` behind a box drawing back from
// over it: the pane, less the side the box still covers and a gutter beside
// it, so the two stay a gutter apart all the way. A box that covers the pane
// across its whole height takes the side it reaches; one across its whole
// width, the top or bottom; one over a corner, whichever takes less. A box
// lying wholly inside the pane, touching no side, hides nothing: no pane a
// layout moves stands there.
inline RevealBox revealedBeside(RevealBox pane, const RevealBox &box, double gutter)
{
    if (pane.empty() || box.empty()) return pane;
    const RevealBox cover{box.x - gutter, box.y - gutter, box.width + 2.0 * gutter, box.height + 2.0 * gutter};
    const double left = std::max(pane.x, cover.x), top = std::max(pane.y, cover.y);
    const double right = std::min(pane.right(), cover.right()), bottom = std::min(pane.bottom(), cover.bottom());
    if (right <= left || bottom <= top) return pane;
    const bool fromLeft = left <= pane.x, fromRight = right >= pane.right();
    const bool fromTop = top <= pane.y, fromBottom = bottom >= pane.bottom();
    // What each cut would leave of the pane, or nothing where the box does
    // not reach that side.
    const double keepAcross = fromLeft && fromRight ? 0.0
        : fromLeft ? pane.right() - right : fromRight ? left - pane.x : -1.0;
    const double keepDown = fromTop && fromBottom ? 0.0
        : fromTop ? pane.bottom() - bottom : fromBottom ? top - pane.y : -1.0;
    const bool wholeHeight = fromTop && fromBottom, wholeWidth = fromLeft && fromRight;
    const bool across = keepAcross >= 0.0
        && (wholeHeight || keepDown < 0.0 || (!wholeWidth && keepAcross * pane.height >= keepDown * pane.width));
    if (across) {
        if (fromLeft) { pane.width = pane.right() - right; pane.x = right; }
        else pane.width = left - pane.x;
        if (wholeWidth) pane.width = 0.0;
    } else if (keepDown >= 0.0) {
        if (fromTop) { pane.height = pane.bottom() - bottom; pane.y = bottom; }
        else pane.height = top - pane.y;
        if (wholeHeight) pane.height = 0.0;
    }
    pane.width = std::max(0.0, pane.width);
    pane.height = std::max(0.0, pane.height);
    return pane;
}

// How a window opening into a layout arrives, given what of its `pane` the
// panes drawing back have left `shown`. Left on one side only, it is drawn
// whole with its leading edge where they have left, so it pushes in from the
// far side, a gutter from them all the way: the offset from its pane is
// returned. Left from two sides, it opens from the seam between them, drawn
// where it will stand and seen only where they have left: no offset.
struct OpenedPush {
    double dx = 0.0, dy = 0.0;
    bool pushes = false;
};

inline OpenedPush openedPushOffset(const RevealBox &pane, const RevealBox &shown)
{
    const double left = shown.x - pane.x, right = pane.right() - shown.right();
    const double top = shown.y - pane.y, bottom = pane.bottom() - shown.bottom();
    const int sides = (left > 0.5) + (right > 0.5) + (top > 0.5) + (bottom > 0.5);
    if (sides != 1) return {};
    return OpenedPush{left > 0.5 ? left : right > 0.5 ? -right : 0.0,
                      top > 0.5 ? top : bottom > 0.5 ? -bottom : 0.0, true};
}

// Where a window taking the place of a pane that gives way stands, `t` of the
// way in (eased): pushed in from the screen edge nearest its place, starting
// just past that edge.
inline OpenedPush openedFromNearestEdge(const RevealBox &pane, const RevealBox &screen, double t)
{
    if (pane.empty() || screen.empty()) return {};
    const double left = pane.x - screen.x, right = screen.right() - pane.right();
    const double top = pane.y - screen.y, bottom = screen.bottom() - pane.bottom();
    const double nearest = std::min({left, right, top, bottom});
    const double away = 1.0 - std::clamp(t, 0.0, 1.0);
    if (nearest == right) return OpenedPush{(screen.right() - pane.x) * away, 0.0, true};
    if (nearest == left) return OpenedPush{(screen.x - pane.right()) * away, 0.0, true};
    if (nearest == bottom) return OpenedPush{0.0, (screen.bottom() - pane.y) * away, true};
    return OpenedPush{0.0, (screen.y - pane.bottom()) * away, true};
}

} // namespace Kadunce
